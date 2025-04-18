#include "client/connection_manager.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
#include "fenris.pb.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector> // Include vector

namespace fenris {
namespace client {

using ServerInfo = fenris::client::ServerInfo;
using namespace common;
using namespace common::network; // Added using directive

ConnectionManager::ConnectionManager(const std::string &hostname,
                                     const std::string &port)
    : m_server_hostname(hostname), m_server_port(port),
      m_server_handler(nullptr), // Initialize handler
      m_non_blocking_mode(false), m_server_socket(-1), m_connected(false)
{
}

ConnectionManager::~ConnectionManager()
{
    disconnect();
}

void ConnectionManager::set_non_blocking_mode(bool enabled)
{
    m_non_blocking_mode = enabled;
}

bool ConnectionManager::connect()
{
    // Don't reconnect if already connected
    if (m_connected) {
        std::cerr << "Already connected to server" << std::endl;
        return true;
    }

    // Setup socket
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    if ((rv = getaddrinfo(m_server_hostname.c_str(),
                          m_server_port.c_str(),
                          &hints,
                          &servinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return false;
    }
    ServerInfo server_info;
    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((m_server_socket =
                 socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::cerr << "client: socket creation failed: " << strerror(errno)
                      << std::endl;
            continue;
        }

        if (::connect(m_server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(m_server_socket);
            std::cerr << "client: connect failed: " << strerror(errno)
                      << std::endl;
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr) {
        std::cerr << "client: failed to connect to server" << std::endl;
        return false;
    }

    // Set socket to non-blocking mode if requested (for testing)
    if (m_non_blocking_mode) {
        int flags = fcntl(m_server_socket, F_GETFL);
        fcntl(m_server_socket, F_SETFL, flags | O_NONBLOCK);
    }

    m_connected = true;

    std::cout << "Connected to server " << m_server_hostname << ":"
              << m_server_port << std::endl;

    // TODO: Key exchange step - implement secure key exchange protocol here
    // This should establish shared encryption keys between client and server

    // No longer need the comment block about request steps here

    return true;
}

void ConnectionManager::disconnect()
{
    // Close socket to break out of any blocking read in the response thread
    {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_server_socket != -1) {
            close(m_server_socket);
            m_server_socket = -1;
        }
    }

    if (m_connected.exchange(false)) {
        std::cout << "Disconnecting from server" << std::endl;
    }
}

bool ConnectionManager::is_connected() const
{
    return m_connected;
}

// Add implementation for set_server_handler
void ConnectionManager::set_server_handler(
    std::shared_ptr<ServerHandler> handler)
{
    m_server_handler = handler;
}

// Updated send_data to handle protobuf Request
bool ConnectionManager::send_request(const fenris::Request &request)
{
    if (!m_connected || m_server_socket == -1) {
        std::cerr << "Cannot send request: not connected to server"
                  << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_socket_mutex);

    // Serialize the request
    std::vector<uint8_t> serialized_request = serialize_request(request);
    uint32_t request_size = static_cast<uint32_t>(serialized_request.size());

    // Send the size prefix (network byte order)
    if (!send_size(m_server_socket, request_size, m_non_blocking_mode)) {
        std::cerr << "Error sending request size"
                  << std::endl; // Error already printed by send_size/send_all
        m_connected = false;    // Mark as disconnected due to error
        return false;
    }

    // Send the serialized request data
    if (!send_data(m_server_socket,
                   serialized_request,
                   static_cast<uint32_t>(serialized_request.size()),
                   m_non_blocking_mode)) {
        std::cerr << "Error sending request data"
                  << std::endl; // Error already printed by send_all
        m_connected = false;    // Mark as disconnected due to error
        return false;
    }

    return true;
}

std::optional<fenris::Response> ConnectionManager::receive_response()
{
    if (!m_connected || m_server_socket == -1) {
        std::cerr << "Cannot receive response: not connected to server"
                  << std::endl;
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(m_socket_mutex);

    // Receive the size prefix
    uint32_t response_size = 0;
    if (!receive_size(m_server_socket, response_size, m_non_blocking_mode)) {
        std::cerr << "Error receiving response size"
                  << std::endl; // Error already printed by receive_size
        m_connected = false;    // Mark as disconnected due to error
        return std::nullopt;
    }

    // Receive the serialized response data
    std::vector<uint8_t> serialized_response(response_size);
    if (!receive_data(m_server_socket,
                      serialized_response,
                      response_size,
                      m_non_blocking_mode)) {
        std::cerr << "Error receiving response data"
                  << std::endl; // Error already printed by receive_all
        m_connected = false;    // Mark as disconnected due to error
        return std::nullopt;
    }

    // Deserialize the response
    fenris::Response response = deserialize_response(serialized_response);
    return response;
}

} // namespace client
} // namespace fenris
