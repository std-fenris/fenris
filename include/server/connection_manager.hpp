#ifndef FENRIS_SERVER_CONNECTION_MANAGER_HPP
#define FENRIS_SERVER_CONNECTION_MANAGER_HPP

#include "common/crypto_manager.hpp"
#include "common/logging.hpp"
#include "fenris.pb.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fenris {
namespace server {

/**
 * @class ConnectionManager
 * @brief Manages server connections and handles client requests
 *
 * This class manages server socket initialization, accepting client
 * connections, and dispatching client requests to the appropriate handler.
 */
class ConnectionManager {
  public:
    /**
     * @brief Constructor
     * @param hostname Hostname or IP address to bind to
     * @param port Port to listen on
     * @param logger_name Name for this connection manager's logger
     */
    ConnectionManager(
        const std::string &hostname,
        const std::string &port,
        const std::string &logger_name = "ServerConnectionManager");

    /**
     * @brief Destructor
     */
    ~ConnectionManager();

    /**
     * @brief Set socket to non-blocking mode (for testing)
     * @param enabled Whether to enable non-blocking mode
     */
    void set_non_blocking_mode(bool enabled);

    /**
     * @brief Start listening for connections
     */
    void start();

    /**
     * @brief Stop listening for connections and clean up resources
     */
    void stop();

    /**
     * @brief Set handler for client connections
     * @param handler Function that processes client requests
     */
    void set_client_handler(std::unique_ptr<ClientHandler> handler);

    /**
     * @brief Get number of active clients
     * @return Number of active client connections
     */
    size_t get_active_client_count() const;

    /**
     * @brief Send a response to a client
     * @param client_info ClientInfo struct containing client connection
     * information
     * @param response The response to send
     * @return true if send successful, false otherwise
     *
     * This method encrypts the response using the client's key
     * and a randomly generated IV, prefixing the IV to the message
     */
    bool send_response(const ClientInfo &client_info,
                       const fenris::Response &response);

    /**
     * @brief Receive a request from a client
     * @param client_info ClientInfo struct containing client connection
     * information
     * @return Optional containing the request if successfully received and
     * decrypted
     *
     * This method extracts the IV from the first part of the message
     * and uses it to decrypt the request data
     */
    std::optional<fenris::Request>
    receive_request(const ClientInfo &client_info);

  private:
    /**
     * @brief Listen for incoming connections
     */
    void listen_for_connection();

    /**
     * @brief Handle client connection in its own thread
     * @param client_socket Socket descriptor for the client connection
     * @param client_id Unique identifier for the client
     */
    void handle_client(uint32_t client_socket, uint32_t client_id);

    /**
     * @brief Generate a unique client ID
     * @return New unique client ID
     */
    uint32_t generate_client_id();

    /**
     * @brief Remove a client from the active clients list
     * @param client_id ID of the client to remove
     */
    void remove_client(uint32_t client_id);

    /**
     * @brief Perform key exchange with client and save the encryption key in
     * the client info struct
     * @param client_info ClientInfo struct containing client connection
     * @return true if key exchange was successful, false otherwise
     */
    bool perform_key_exchange(ClientInfo &client_info);

    std::string m_hostname;
    std::string m_port;
    std::unique_ptr<ClientHandler> m_client_handler;
    int32_t m_server_socket{-1};
    std::atomic<bool> m_running{false};
    std::thread m_listen_thread;
    bool m_non_blocking_mode;
    common::crypto::CryptoManager m_crypto_manager;
    common::Logger m_logger;

    // Client management
    std::unordered_map<uint32_t, uint32_t>
        m_client_sockets; // (client_id -> client socket)
    std::vector<std::thread> m_client_threads;
    mutable std::mutex m_client_mutex;
    std::atomic<uint32_t> m_next_client_id{1};
};

/**
 * @class ClientHandler
 * @brief Interface for handling client requests
 *
 * Implement this interface to process client requests in your file system
 */
class ClientHandler {
  public:
    virtual ~ClientHandler() = default;

    /**
     * @brief Process a client request.
     * @param client_socket Socket descriptor for the client connection.
     * @param request The deserialized client request.
     * @return A Response object to be sent back to the client.
     *         The 'success' field should indicate if the operation succeeded.
     *         Return a default or error response if processing fails
     * internally.
     */
    virtual std::pair<fenris::Response, bool>
    handle_request(uint32_t client_socket, const fenris::Request &request) = 0;
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_CONNECTION_MANAGER_HPP
