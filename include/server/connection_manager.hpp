#ifndef FENRIS_SERVER_CONNECTION_MANAGER_HPP
#define FENRIS_SERVER_CONNECTION_MANAGER_HPP

#include "fenris.pb.h" // Use Protocol Buffers directly

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

// Forward declarations
class ClientHandler;

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
     */
    ConnectionManager(const std::string &hostname, const std::string &port);

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

    std::string m_hostname;
    std::string m_port;
    std::unique_ptr<ClientHandler> m_client_handler;
    int32_t m_server_socket{-1};
    std::atomic<bool> m_running{false};
    std::thread m_listen_thread;
    bool m_non_blocking_mode;

    // Client management
    std::unordered_map<uint32_t, uint32_t>
        m_client_sockets; // clientId -> socket
    std::vector<std::thread> m_client_threads;
    mutable std::mutex m_client_mutex;
    std::atomic<uint32_t> m_next_client_id{1};

    // Client request handler
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
