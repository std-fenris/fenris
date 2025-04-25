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
using namespace common::crypto;

ConnectionManager::ConnectionManager(const std::string &hostname,
                                     const std::string &port,
                                     const std::string &logger_name)
    : m_server_hostname(hostname), m_server_port(port),
      m_server_handler(nullptr), m_non_blocking_mode(false),
      m_server_socket(-1), m_connected(false)
{
    m_logger = get_logger(logger_name);
    m_crypto_manager = std::make_unique<CryptoManager>();
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
        m_logger->warn("already connected to server");
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

    m_logger->info("connected to server {}:{}",
                   m_server_hostname,
                   m_server_port);

    if (!perform_key_exchange()) {
        m_logger->error("key exchange with server failed");
        disconnect();
        return false;
    }

    return true;
}

bool ConnectionManager::perform_key_exchange()
{
    auto [private_key, public_key, keygen_result] =
        m_crypto_manager->generate_ecdh_keypair();
    if (keygen_result != ECDHResult::SUCCESS) {
        m_logger->error("failed to generate ECDH key pair: {}",
                        ecdh_result_to_string(keygen_result));
        return false;
    }

    // Send public key to server
    NetworkResult send_result =
        send_prefixed_data(m_server_socket, public_key, m_non_blocking_mode);
    if (send_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to send public key: {}",
                        network_result_to_string(send_result));
        return false;
    }

    // Receive server's public key
    std::vector<uint8_t> server_public_key;

    NetworkResult recv_result = receive_prefixed_data(m_server_socket,
                                                      server_public_key,
                                                      m_non_blocking_mode);
    if (recv_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to receive server public key: {}",
                        network_result_to_string(recv_result));
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
    if (key_derive_result != crypto::ECDHResult::SUCCESS) {
        m_logger->error("Failed to derive encryption key: {}",
                        ecdh_result_to_string(key_derive_result));
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
        m_logger->info("disconnecting from server");
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

const std::vector<uint8_t> &ConnectionManager::get_encryption_key() const
{
    return m_server_info.encryption_key;
}

bool ConnectionManager::send_request(const fenris::Request &request)
{
    if (!m_connected || m_server_socket == -1) {
        m_logger->error("cannot send request: not connected to server");
        return false;
    }

    std::vector<uint8_t> serialized_request = serialize_request(request);
    NetworkResult send_result;

    {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        send_result = send_prefixed_data(m_server_socket,
                                         serialized_request,
                                         m_non_blocking_mode);
    }
    if (send_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to send request: {}",
                        network_result_to_string(send_result));
        m_connected = false;
        return false;
    }

    return true;
}

std::optional<fenris::Response> ConnectionManager::receive_response()
{
    if (!m_connected || m_server_socket == -1) {
        m_logger->error("cannot receive response: not connected to server");
        return std::nullopt;
    }

    std::vector<uint8_t> serialized_response;
    NetworkResult recv_result;

    {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        recv_result = receive_prefixed_data(m_server_socket,
                                            serialized_response,
                                            m_non_blocking_mode);
    }
    if (recv_result != NetworkResult::SUCCESS) {
        m_logger->error("error receiving response data: {}",
                        network_result_to_string(recv_result));

        // If we received a disconnection, update our connection status
        if (recv_result == NetworkResult::DISCONNECTED) {
            m_logger->warn("server disconnected while receiving response data");
        }

        m_connected = false;
        return std::nullopt;
    }

    fenris::Response response = deserialize_response(serialized_response);
    return response;
}

} // namespace client
} // namespace fenris
