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
    m_crypto_manager = std::make_unique<common::crypto::CryptoManager>();
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
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(m_server_hostname.c_str(),
                         m_server_port.c_str(),
                         &hints,
                         &servinfo);
    if (rv != 0) {
        m_logger->error("getaddrinfo: {}", gai_strerror(rv));
        return false;
    }

    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        m_server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_server_socket == -1) {
            m_logger->error("socket creation failed: {}", strerror(errno));
            continue;
        }

        int rc = ::connect(m_server_socket, p->ai_addr, p->ai_addrlen);
        if (rc == -1) {
            close(m_server_socket);
            m_logger->error("connect failed: {}", strerror(errno));
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

    if (!perform_key_exchange()) {
        m_logger->error("Key exchange with server failed");
        disconnect();
        return false;
    }

    return true;
}

bool ConnectionManager::perform_key_exchange()
{
    auto [private_key, public_key, keygen_error] =
        m_crypto_manager->generate_ecdh_keypair();
    if (keygen_error != crypto::ECDHError::SUCCESS) {
        m_logger->error("Failed to generate ECDH key pair");
        return false;
    }

    // Send public key to server
    NetworkError send_result = send_prefixed_data(m_server_socket, public_key);
    if (send_result != NetworkError::SUCCESS) {
        m_logger->error("Failed to send public key: {}",
                        static_cast<int>(send_result));
        return false;
    }

    // Receive server's public key
    std::vector<uint8_t> server_public_key;

    NetworkError recv_result =
        receive_prefixed_data(m_server_socket, server_public_key);
    if (recv_result != NetworkError::SUCCESS) {
        m_logger->error("Failed to receive server public key: {}",
                        static_cast<int>(recv_result));
        return false;
    }

    // Compute shared secret
    auto [shared_secret, ss_error] =
        m_crypto_manager->compute_ecdh_shared_secret(private_key,
                                                     server_public_key);
    if (ss_error != crypto::ECDHError::SUCCESS) {
        m_logger->error("Failed to compute ECDH shared secret");
        return false;
    }

    // Derive encryption key from shared secret
    auto [derived_key, key_derive_error] =
        m_crypto_manager->derive_key_from_shared_secret(shared_secret, 16);
    if (key_derive_error != crypto::ECDHError::SUCCESS) {
        m_logger->error("Failed to derive encryption key");
        return false;
    }

    // Set the derived key in the crypto manager
    m_server_info.encryption_key = derived_key;
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

    NetworkError send_result =
        send_prefixed_data(m_server_socket, serialized_request);
    if (send_result != NetworkError::SUCCESS) {
        m_logger->error("Error sending request data: {}",
                        static_cast<int>(send_result));
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

    std::vector<uint8_t> serialized_response;

    NetworkError recv_result =
        receive_prefixed_data(m_server_socket, serialized_response);
    if (recv_result != NetworkError::SUCCESS) {
        m_logger->error("Error receiving response data: {}",
                        static_cast<int>(recv_result));

        // If we received a disconnection, update our connection status
        if (recv_result == NetworkError::DISCONNECTED) {
            m_logger->warn("Server disconnected while receiving response data");
        }

        m_connected = false;
        return std::nullopt;
    }

    fenris::Response response = deserialize_response(serialized_response);
    return response;
}

} // namespace client
} // namespace fenris
