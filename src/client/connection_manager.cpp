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
    : m_server_handler(nullptr), m_non_blocking_mode(false), m_connected(false)
{
    m_logger = get_logger(logger_name);
    m_server_info.address = hostname;
    m_server_info.port = port;
    m_server_info.socket = -1;
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

    int rv = getaddrinfo(m_server_info.address.c_str(),
                         m_server_info.port.c_str(),
                         &hints,
                         &servinfo);
    if (rv != 0) {
        m_logger->error("getaddrinfo: {}", gai_strerror(rv));
        return false;
    }

    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        m_server_info.socket =
            socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_server_info.socket == -1) {
            m_logger->error("socket creation failed: {}", strerror(errno));
            continue;
        }

        // Use socket connect instead of ConnectionManager::connect
        int rc = ::connect(m_server_info.socket, p->ai_addr, p->ai_addrlen);
        if (rc == -1) {
            close(m_server_info.socket);
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
        int flags = fcntl(m_server_info.socket, F_GETFL);
        fcntl(m_server_info.socket, F_SETFL, flags | O_NONBLOCK);
    }

    m_connected = true;

    m_logger->info("connected to server {}:{}",
                   m_server_info.address,
                   m_server_info.port);

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
        m_crypto_manager.generate_ecdh_keypair();
    if (keygen_result != ECDHResult::SUCCESS) {
        m_logger->error("failed to generate ECDH key pair: {}",
                        ecdh_result_to_string(keygen_result));
        return false;
    }

    // Send public key to server
    NetworkResult send_result = send_prefixed_data(m_server_info.socket,
                                                   public_key,
                                                   m_non_blocking_mode);
    if (send_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to send public key: {}",
                        network_result_to_string(send_result));
        return false;
    }

    // Receive server's public key
    std::vector<uint8_t> server_public_key;

    NetworkResult recv_result = receive_prefixed_data(m_server_info.socket,
                                                      server_public_key,
                                                      m_non_blocking_mode);
    if (recv_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to receive server public key: {}",
                        network_result_to_string(recv_result));
        return false;
    }

    // Compute shared secret
    auto [shared_secret, ss_result] =
        m_crypto_manager.compute_ecdh_shared_secret(private_key,
                                                    server_public_key);
    if (ss_result != ECDHResult::SUCCESS) {
        m_logger->error("failed to compute ECDH shared secret: {}",
                        ecdh_result_to_string(ss_result));
        return false;
    }

    // Derive encryption key from shared secret
    auto [derived_key, key_derive_result] =
        m_crypto_manager.derive_key_from_shared_secret(shared_secret,
                                                       AES_GCM_KEY_SIZE);
    if (key_derive_result != crypto::ECDHResult::SUCCESS) {
        m_logger->error("Failed to derive encryption key: {}",
                        ecdh_result_to_string(key_derive_result));
        return false;
    }

    // Set the derived key in the crypto manager
    m_server_info.encryption_key = std::move(derived_key);
    return true;
}

void ConnectionManager::disconnect()
{

    {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_server_info.socket != -1) {
            close(m_server_info.socket);
            m_server_info.socket = -1;
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
    if (!m_connected || m_server_info.socket == -1) {
        m_logger->error("cannot send request: not connected to server");
        return false;
    }

    std::vector<uint8_t> serialized_request = serialize_request(request);

    // Generate a random IV
    auto [iv, iv_gen_result] = m_crypto_manager.generate_random_iv();
    if (iv_gen_result != crypto::EncryptionResult::SUCCESS) {
        m_logger->error("failed to generate IV: {}",
                        crypto::encryption_result_to_string(iv_gen_result));
        return false;
    }

    // Encrypt the serialized request
    auto [encrypted_request, encrypt_result] =
        m_crypto_manager.encrypt_data(serialized_request,
                                      m_server_info.encryption_key,
                                      iv);
    if (encrypt_result != crypto::EncryptionResult::SUCCESS) {
        m_logger->error("failed to encrypt request: {}",
                        crypto::encryption_result_to_string(encrypt_result));
        return false;
    }

    // Create the final message with IV prefixed to encrypted data
    std::vector<uint8_t> message_with_iv;
    message_with_iv.reserve(iv.size() + encrypted_request.size());
    message_with_iv.insert(message_with_iv.end(), iv.begin(), iv.end());
    message_with_iv.insert(message_with_iv.end(),
                           encrypted_request.begin(),
                           encrypted_request.end());

    // Send the IV-prefixed encrypted request
    NetworkResult send_result = send_prefixed_data(m_server_info.socket,
                                                   message_with_iv,
                                                   m_non_blocking_mode);
    if (send_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to send encrypted request: {}",
                        network_result_to_string(send_result));
        return false;
    }

    return true;
}

std::optional<fenris::Response> ConnectionManager::receive_response()
{
    if (!m_connected || m_server_info.socket == -1) {
        m_logger->error("cannot receive response: not connected to server");
        return std::nullopt;
    }

    // Receive encrypted data (includes IV + encrypted response)
    std::vector<uint8_t> encrypted_data;
    NetworkResult recv_result = receive_prefixed_data(m_server_info.socket,
                                                      encrypted_data,
                                                      m_non_blocking_mode);
    if (recv_result != NetworkResult::SUCCESS) {
        m_logger->error("failed to receive response: {}",
                        network_result_to_string(recv_result));
        return std::nullopt;
    }

    if (encrypted_data.size() < AES_GCM_IV_SIZE) {
        m_logger->error("received data too small to contain IV");
        return std::nullopt;
    }

    // Extract IV from the beginning of the message
    std::vector<uint8_t> iv(encrypted_data.begin(),
                            encrypted_data.begin() + AES_GCM_IV_SIZE);

    // Extract the encrypted response data (after the IV)
    std::vector<uint8_t> encrypted_response(encrypted_data.begin() +
                                                AES_GCM_IV_SIZE,
                                            encrypted_data.end());

    // Decrypt the response using the extracted IV
    auto [decrypted_data, decrypt_result] =
        m_crypto_manager.decrypt_data(encrypted_response,
                                      m_server_info.encryption_key,
                                      iv);

    if (decrypt_result != crypto::EncryptionResult::SUCCESS) {
        m_logger->error("failed to decrypt response: {}",
                        crypto::encryption_result_to_string(decrypt_result));
        return std::nullopt;
    }

    // Deserialize the response
    return deserialize_response(decrypted_data);
}

} // namespace client
} // namespace fenris
