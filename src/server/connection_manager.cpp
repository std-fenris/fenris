#include "server/connection_manager.hpp"
#include "common/logging.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
#include "fenris.pb.h"
#include "server/request_manager.hpp"
#include "server/server.hpp"

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
using namespace common::crypto;

ConnectionManager::ConnectionManager(const std::string &hostname,
                                     const std::string &port,
                                     const std::string &logger_name)
    : m_hostname(hostname), m_port(port), m_client_handler(nullptr),
      m_non_blocking_mode(false)
{
    m_logger = get_logger(logger_name);
    m_crypto_manager = std::make_unique<CryptoManager>();
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
        m_logger->warn("connection manager already running");
        return;
    }

    if (!m_client_handler) {
        m_logger->error(
            "no client handler set, cannot start connection manager");
        return;
    }

    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    rv = getaddrinfo(m_hostname.c_str(), m_port.c_str(), &hints, &servinfo);
    if (rv != 0) {
        m_logger->error("getaddrinfo: {}", gai_strerror(rv));
        return;
    }

    for (p = servinfo; p != nullptr; p = p->ai_next) {
        m_server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_server_socket == -1) {
            m_logger->error("server: socket creation failed: {}",
                            strerror(errno));
            continue;
        }

        int yes = 1;
        int rc;
        rc = setsockopt(m_server_socket,
                        SOL_SOCKET,
                        SO_REUSEADDR,
                        &yes,
                        sizeof(int));
        if (rc == -1) {
            m_logger->error("setsockopt failed: {}", strerror(errno));
            close(m_server_socket);
            freeaddrinfo(servinfo);
            return;
        }

        rc = bind(m_server_socket, p->ai_addr, p->ai_addrlen);
        if (rc == -1) {
            close(m_server_socket);
            m_logger->error("server: bind failed: {}", strerror(errno));
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr) {
        m_logger->error("server: failed to bind");
        return;
    }

    // Listen for incoming connections
    if (listen(m_server_socket, 10) == -1) {
        m_logger->error("listen failed: {}", strerror(errno));
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

    m_logger->info("connection manager started on {}:{}", m_hostname, m_port);
}

void ConnectionManager::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_server_socket != -1) {

        int wake_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (wake_socket >= 0) {
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;

            try {
                server_addr.sin_port = htons(std::stoi(m_port));
            } catch (const std::invalid_argument &e) {
                m_logger->error("invalid port number: {}. error: {}",
                                m_port,
                                e.what());
                return;
            } catch (const std::out_of_range &e) {
                m_logger->error("port number out of range: {}. error: {}",
                                m_port,
                                e.what());
                return;
            }

            inet_pton(AF_INET, m_hostname.c_str(), &server_addr.sin_addr);

            // Connect to ourselves to unblock accept()
            connect(wake_socket,
                    (struct sockaddr *)&server_addr,
                    sizeof(server_addr));
            close(wake_socket);
            m_logger->debug("wakeup socket closed");
        }
    }

    if (m_server_socket != -1) {
        close(m_server_socket);
        m_server_socket = -1;
    }

    if (m_listen_thread.joinable()) {
        m_listen_thread.join();
    }

    std::lock_guard<std::mutex> lock(m_client_mutex);

    for (auto &pair : m_client_sockets) {
        close(pair.second);
    }
    m_client_sockets.clear();

    for (auto &thread : m_client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_client_threads.clear();

    m_logger->info("connection manager stopped");
}

void ConnectionManager::set_client_handler(
    std::unique_ptr<ClientHandler> handler)
{
    m_client_handler = std::move(handler);
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

        int client_fd =
            accept(m_server_socket, (struct sockaddr *)&client_addr, &sin_size);

        if (!m_running) {
            break;
        }

        if (client_fd == -1) {
            if (m_non_blocking_mode &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            m_logger->error("accept failed: {}", strerror(errno));
            continue;
        }

        // Get client information for logging
        char client_ip[INET6_ADDRSTRLEN];
        inet_ntop(client_addr.ss_family,
                  &(((struct sockaddr_in *)&client_addr)->sin_addr),
                  client_ip,
                  sizeof client_ip);
        m_logger->info("server: got connection from {}", client_ip);

        uint32_t client_id = generate_client_id();

        {
            std::lock_guard<std::mutex> lock(m_client_mutex);
            m_client_sockets[client_id] = client_fd;
        }

        m_client_threads.emplace_back(&ConnectionManager::handle_client,
                                      this,
                                      client_fd,
                                      client_id);
    }
}

bool ConnectionManager::perform_key_exchange(
    uint32_t client_socket,
    std::vector<uint8_t> &encryption_key)
{
    auto [private_key, public_key, keygen_result] =
        m_crypto_manager->generate_ecdh_keypair();
    if (keygen_result != ECDHResult::SUCCESS) {
        m_logger->error("failed to generate ECDH key pair: {}",
                        ecdh_result_to_string(keygen_result));
        return false;
    }

    // Receive client's public key
    std::vector<uint8_t> server_public_key;
    NetworkResult recv_result = receive_prefixed_data(client_socket,
                                                      server_public_key,
                                                      m_non_blocking_mode);
    if (recv_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to receive client public key: {}",
                        network_result_to_string(recv_result));
        return false;
    }

    // Send public key to client
    NetworkResult send_result =
        send_prefixed_data(client_socket, public_key, m_non_blocking_mode);
    if (send_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to send public key: {}",
                        network_result_to_string(send_result));
        return false;
    }

    // Compute shared secret
    auto [shared_secret, ss_result] =
        m_crypto_manager->compute_ecdh_shared_secret(private_key,
                                                     server_public_key);
    if (ss_result != ECDHResult::SUCCESS) {
        m_logger->error("failed to compute ECDH shared secret: {}",
                        ecdh_result_to_string(ss_result));
        return false;
    }

    // Derive encryption key from shared secret
    auto [derived_key, key_derive_result] =
        m_crypto_manager->derive_key_from_shared_secret(shared_secret, 16);
    if (key_derive_result != ECDHResult::SUCCESS) {
        m_logger->error("failed to derive encryption key: {}",
                        ecdh_result_to_string(key_derive_result));
        return false;
    }

    encryption_key = derived_key;
    return true;
}

void ConnectionManager::handle_client(uint32_t client_socket,
                                      uint32_t client_id)
{

    ClientInfo client_info;
    client_info.client_id = client_id;
    client_info.socket = client_socket;

    // Set client socket to non-blocking if server is in non-blocking mode
    if (m_non_blocking_mode) {
        int flags = fcntl(client_socket, F_GETFL);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    }

    bool keep_connection = true;

    if (!perform_key_exchange(client_info.socket, client_info.encryption_key)) {
        m_logger->error("key exchange failed");
        close(client_socket);
        remove_client(client_id);
        return;
    }

    // Process client requests
    while (m_running && keep_connection) {

        std::vector<uint8_t> request_data;
        NetworkResult recv_result = receive_prefixed_data(client_socket,
                                                          request_data,
                                                          m_non_blocking_mode);
        if (recv_result != NetworkResult::SUCCESS) {
            m_logger->error("failed to receive request data from client: {}",
                            network_result_to_string(recv_result));
            break;
        }

        Request request;
        try {
            request = deserialize_request(request_data);
        } catch (const std::exception &e) {
            m_logger->error("exception while parsing request: {}", e.what());
            break;
        }

        auto response =
            m_client_handler->handle_request(client_socket, request);
        keep_connection = response.second;

        std::vector<uint8_t> response_data = serialize_response(response.first);
        if (response_data.empty()) {
            m_logger->error("failed to serialize response");
            break;
        }

        NetworkResult send_result = send_prefixed_data(client_socket,
                                                       response_data,
                                                       m_non_blocking_mode);
        if (send_result != NetworkResult::SUCCESS) {
            m_logger->error("failed to send response data to client: {}",
                            network_result_to_string(send_result));
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
