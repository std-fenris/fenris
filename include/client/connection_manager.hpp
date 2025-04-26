#ifndef FENRIS_CLIENT_CONNECTION_MANAGER_HPP
#define FENRIS_CLIENT_CONNECTION_MANAGER_HPP

#include "client/client.hpp"
#include "common/crypto_manager.hpp"
#include "common/logging.hpp"
#include "fenris.pb.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace fenris {
namespace client {

using ServerInfo = fenris::client::ServerInfo;

class ServerHandler;

/**
 * @class ConnectionManager
 * @brief Manages connection to a file server for client applications
 *
 * Handles establishing and maintaining a connection to the server,
 * and provides an interface for sending requests and receiving responses.
 */
class ConnectionManager {
  public:
    /**
     * @brief Constructor
     * @param server_hostname The hostname or IP address of the server
     * @param server_port The port the server is listening on
     * @param logger_name Name for this connection manager's logger
     */
    ConnectionManager(
        const std::string &hostname,
        const std::string &port,
        const std::string &logger_name = "ClientConnectionManager");

    /**
     * @brief Destructor, ensures proper cleanup of resources
     */
    ~ConnectionManager();

    /**
     * @brief Set socket to non-blocking mode (for testing)
     * @param enabled Whether to enable non-blocking mode
     */
    void set_non_blocking_mode(bool enabled);

    /**
     * @brief Connect to the server
     * @return true if connection successful, false otherwise
     */
    bool connect();

    /**
     * @brief Disconnect from the server and clean up resources
     */
    void disconnect();

    /**
     * @brief Set handler for server responses
     * @param handler Function that processes server responses
     */
    void set_server_handler(std::unique_ptr<ServerHandler> handler);

    /**
     * @brief Send a request to the server
     * @param request The request to send
     * @return true if send successful, false otherwise
     *
     * This method encrypts the request using the server's key
     * and a randomly generated IV, prefixing the IV to the message
     */
    bool send_request(const fenris::Request &request);

    /**
     * @brief Receive a response from the server
     * @return Optional containing the response if successfully received and
     * decrypted
     *
     * This method extracts the IV from the first part of the message
     * and uses it to decrypt the response data
     */
    std::optional<fenris::Response> receive_response();

    /**
     * @brief Check if currently connected to the server
     * @return true if connected, false otherwise
     */
    bool is_connected() const;

    /**
     * @brief Get the current encryption key
     * @return Reference to the current encryption key
     */
    const std::vector<uint8_t> &get_encryption_key() const;

  private:
    /**
     * @brief Perform key exchange with server and save the encryption key
     * @returns true if key exchange was successful, false otherwise
     */
    bool perform_key_exchange();

    std::unique_ptr<ServerHandler> m_server_handler;
    bool m_non_blocking_mode;
    std::atomic<bool> m_connected{false};
    std::mutex m_socket_mutex;
    ServerInfo m_server_info;
    common::crypto::CryptoManager m_crypto_manager;
    common::Logger m_logger;
};

/**
 * @class ServerHandler
 * @brief Interface for handling server responses
 *
 * Implement this interface to process responses from the server
 */
class ServerHandler {
  public:
    virtual ~ServerHandler() = default;

    /**
     * @brief Process server responses
     * @param response The deserialized Protocol Buffer response
     * @return true if the connection should remain open, false to close
     */
    virtual bool handle_response(const fenris::Response &response) = 0;
};

} // namespace client
} // namespace fenris

#endif // FENRIS_CLIENT_CONNECTION_MANAGER_HPP
