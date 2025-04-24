#include "client/connection_manager.hpp"
#include "common/logging.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
#include "fenris.pb.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace fenris {
namespace client {

using ServerInfo = fenris::client::ServerInfo;
using namespace common;
using namespace common::network;

ConnectionManager::ConnectionManager(const std::string &hostname,
                                     const std::string &port,
                                     const std::string &logger_name)
    : m_server_hostname(hostname), m_server_port(port),
      m_server_handler(nullptr), m_non_blocking_mode(false),
      m_server_socket(-1), m_connected(false)
{
    m_logger = get_logger(logger_name);
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
        m_logger->warn("Already connected to server");
        return true;
    }

    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(m_server_hostname.c_str(),
                          m_server_port.c_str(),
                          &hints,
                          &servinfo)) != 0) {
        m_logger->error("getaddrinfo: {}", gai_strerror(rv));
        return false;
    }
    ServerInfo server_info;

    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((m_server_socket =
                 socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            m_logger->error("client: socket creation failed: {}",
                            strerror(errno));
            continue;
        }

        if (::connect(m_server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(m_server_socket);
            m_logger->error("client: connect failed: {}", strerror(errno));
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr) {
        m_logger->error("client: failed to connect to server");
        return false;
    }

    // Set socket to non-blocking mode if requested (for testing)
    if (m_non_blocking_mode) {
        int flags = fcntl(m_server_socket, F_GETFL);
        fcntl(m_server_socket, F_SETFL, flags | O_NONBLOCK);
    }

    m_connected = true;

    m_logger->info("Connected to server {}:{}",
                   m_server_hostname,
                   m_server_port);

    // TODO: Key exchange step - implement secure key exchange protocol here
    // This should establish shared encryption keys between client and server

    return true;
}

void ConnectionManager::disconnect()
{

    {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_server_socket != -1) {
            close(m_server_socket);
            m_server_socket = -1;
        }
    }

    if (m_connected.exchange(false)) {
        m_logger->info("Disconnecting from server");
    }
}

bool ConnectionManager::is_connected() const
{
    return m_connected;
}

void ConnectionManager::set_server_handler(
    std::unique_ptr<ServerHandler> handler)
{
    m_server_handler = std::move(handler);
}

bool ConnectionManager::send_request(const fenris::Request &request)
{
    if (!m_connected || m_server_socket == -1) {
        m_logger->error("Cannot send request: not connected to server");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_socket_mutex);

    std::vector<uint8_t> serialized_request = serialize_request(request);
    uint32_t request_size = static_cast<uint32_t>(serialized_request.size());

    if (!send_size(m_server_socket, request_size, m_non_blocking_mode)) {
        m_logger->error("Error sending request size");
        m_connected = false;
        return false;
    }

    if (!send_data(m_server_socket,
                   serialized_request,
                   static_cast<uint32_t>(serialized_request.size()),
                   m_non_blocking_mode)) {
        m_logger->error("Error sending request data");
        m_connected = false;
        return false;
    }

    return true;
}

std::optional<fenris::Response> ConnectionManager::receive_response()
{
    if (!m_connected || m_server_socket == -1) {
        m_logger->error("Cannot receive response: not connected to server");
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(m_socket_mutex);

    uint32_t response_size = 0;
    if (!receive_size(m_server_socket, response_size, m_non_blocking_mode)) {
        m_logger->error("Error receiving response size");
        m_connected = false;
        return std::nullopt;
    }

    std::vector<uint8_t> serialized_response(response_size);
    if (!receive_data(m_server_socket,
                      serialized_response,
                      response_size,
                      m_non_blocking_mode)) {
        m_logger->error("Error receiving response data");
        m_connected = false;
        return std::nullopt;
    }

    fenris::Response response = deserialize_response(serialized_response);
    return response;
}

} // namespace client
} // namespace fenris
