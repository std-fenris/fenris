#include "server/connection_manager.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
#include "fenris.pb.h"
#include "server/request_manager.hpp"
#include "server/server.hpp"

#include <google/protobuf/stubs/common.h>

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace fenris {
namespace server {

using namespace common;
using namespace common::network;

ConnectionManager::ConnectionManager(const std::string &hostname,
                                     const std::string &port)
    : m_hostname(hostname), m_port(port), m_client_handler(nullptr),
      m_non_blocking_mode(false)
{
}

ConnectionManager::~ConnectionManager()
{
    stop();
}

void ConnectionManager::set_non_blocking_mode(bool enabled)
{
    m_non_blocking_mode = enabled;
}

void ConnectionManager::start()
{
    if (m_running) {
        std::cerr << "Connection manager already running" << std::endl;
        return;
    }

    if (!m_client_handler) {
        std::cerr << "No client handler set, cannot start connection manager"
                  << std::endl;
        return;
    }

    // Setup socket
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Use my IP

    if ((rv = getaddrinfo(m_hostname.c_str(),
                          m_port.c_str(),
                          &hints,
                          &servinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((m_server_socket =
                 socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::cerr << "server: socket creation failed: " << strerror(errno)
                      << std::endl;
            continue;
        }

        // Avoid "Address already in use" error
        int yes = 1;
        if (setsockopt(m_server_socket,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       &yes,
                       sizeof(int)) == -1) {
            std::cerr << "setsockopt failed: " << strerror(errno) << std::endl;
            close(m_server_socket);
            freeaddrinfo(servinfo);
            return;
        }

        if (bind(m_server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(m_server_socket);
            std::cerr << "server: bind failed: " << strerror(errno)
                      << std::endl;
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr) {
        std::cerr << "server: failed to bind" << std::endl;
        return;
    }

    // Listen for incoming connections
    if (listen(m_server_socket, 10) == -1) {
        std::cerr << "listen failed: " << strerror(errno) << std::endl;
        close(m_server_socket);
        m_server_socket = -1;
        return;
    }

    // Set socket to non-blocking mode if requested (for testing)
    if (m_non_blocking_mode) {
        int flags = fcntl(m_server_socket, F_GETFL);
        fcntl(m_server_socket, F_SETFL, flags | O_NONBLOCK);
    }

    m_running = true;
    m_listen_thread =
        std::thread(&ConnectionManager::listen_for_connection, this);

    std::cout << "Connection manager started on " << m_hostname << ":" << m_port
              << std::endl;
}

void ConnectionManager::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_server_socket != -1) {
        // Create a client socket to connect to our server
        int wake_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (wake_socket >= 0) {
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(std::stoi(m_port));
            inet_pton(AF_INET, m_hostname.c_str(), &server_addr.sin_addr);

            // Connect to ourselves to unblock accept()
            connect(wake_socket,
                    (struct sockaddr *)&server_addr,
                    sizeof(server_addr));
            close(wake_socket);
            std::cout << "wakeup socket closed" << std::endl;
        }
    }

    // Now close the server socket
    if (m_server_socket != -1) {
        close(m_server_socket);
        m_server_socket = -1;
    }

    // Wait for listen thread to exit
    if (m_listen_thread.joinable()) {
        m_listen_thread.join();
    }

    // Close all client connections
    std::lock_guard<std::mutex> lock(m_client_mutex);

    for (auto &pair : m_client_sockets) {
        close(pair.second);
    }
    m_client_sockets.clear();

    // Wait for all client threads to complete
    for (auto &thread : m_client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_client_threads.clear();

    std::cout << "Connection manager stopped" << std::endl;
}

void ConnectionManager::set_client_handler(
    std::shared_ptr<ClientHandler> handler)
{
    m_client_handler = handler;
}

size_t ConnectionManager::get_active_client_count() const
{
    std::lock_guard<std::mutex> lock(m_client_mutex);
    return m_client_sockets.size();
}

void ConnectionManager::listen_for_connection()
{
    struct sockaddr_storage client_addr;
    socklen_t sin_size = sizeof(client_addr);

    while (m_running) {
        // Accept connection - will be non-blocking if in non-blocking mode
        int client_fd =
            accept(m_server_socket, (struct sockaddr *)&client_addr, &sin_size);

        if (!m_running) {
            break; // Server is shutting down
        }

        if (client_fd == -1) {
            if (m_non_blocking_mode &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // No connection available, sleep briefly and continue
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            std::cerr << "accept failed: " << strerror(errno) << std::endl;
            continue;
        }

        // Get client information for logging
        char client_ip[INET6_ADDRSTRLEN];
        inet_ntop(client_addr.ss_family,
                  &(((struct sockaddr_in *)&client_addr)->sin_addr),
                  client_ip,
                  sizeof client_ip);

        std::cout << "server: got connection from " << client_ip << std::endl;

        // Create a new client ID and add to list
        uint32_t client_id = generate_client_id();

        {
            std::lock_guard<std::mutex> lock(m_client_mutex);
            m_client_sockets[client_id] = client_fd;
        }

        // Create a thread to handle this client
        m_client_threads.emplace_back(&ConnectionManager::handle_client,
                                      this,
                                      client_fd,
                                      client_id);
    }
}

void ConnectionManager::handle_client(uint32_t client_socket,
                                      uint32_t client_id)
{
    // Create client info entry
    ClientInfo client_info;
    client_info.client_id = client_id;
    client_info.socket = client_socket;

    // Set client socket to non-blocking if server is in non-blocking mode
    if (m_non_blocking_mode) {
        int flags = fcntl(client_socket, F_GETFL);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    }

    bool keep_connection = true;

    // Process client requests
    while (m_running && keep_connection) {
        // Read the message size (first 4 bytes)
        uint32_t message_size = 0;
        if (!receive_size(client_socket, message_size, m_non_blocking_mode)) {
            std::cerr << "Failed to read message size from client" << std::endl;
            break;
        }

        // Read the serialized request data
        std::vector<uint8_t> request_data(message_size);
        if (!receive_data(client_socket,
                          request_data,
                          message_size,
                          m_non_blocking_mode)) {
            std::cerr << "Failed to read request data from client" << std::endl;
            break;
        }

        // Deserialize the request using Protocol Buffers
        Request request;
        try {
            request = deserialize_request(request_data);
        } catch (const std::exception &e) {
            std::cerr << "Exception while parsing request: " << e.what()
                      << std::endl;
            break;
        }

        // handle client
        auto response =
            m_client_handler->handle_request(client_socket, request);
        keep_connection = response.second;

        // Serialize the response
        std::vector<uint8_t> response_data = serialize_response(response.first);
        if (response_data.empty()) {
            std::cerr << "Failed to serialize response" << std::endl;
            break;
        }

        // Send the response size (4 bytes)
        uint32_t response_size = static_cast<uint32_t>(response_data.size());

        if (!send_size(client_socket, response_size, m_non_blocking_mode)) {
            std::cerr << "Failed to send response size to client" << std::endl;
            break;
        }

        // Send the serialized response data
        if (!send_data(client_socket,
                       response_data,
                       static_cast<uint32_t>(response_data.size()),
                       m_non_blocking_mode)) {
            std::cerr << "Failed to send response data to client" << std::endl;
            break;
        }
    }

    close(client_socket);
    remove_client(client_id);
}

uint32_t ConnectionManager::generate_client_id()
{
    return m_next_client_id++;
}

void ConnectionManager::remove_client(uint32_t client_id)
{
    std::lock_guard<std::mutex> lock(m_client_mutex);
    m_client_sockets.erase(client_id);
}

} // namespace server
} // namespace fenris
