#ifndef FENRIS_SERVER_HPP
#define FENRIS_SERVER_HPP

#include "common/logging.hpp"
#include "server/client_info.hpp"
#include "server/connection_manager.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace fenris {
namespace server {

/**
 * @class Server
 * @brief Main server application class
 *
 * Acts as a wrapper around the ConnectionManager, handling initialization,
 * configuration, and lifecycle management.
 */
class Server {
  public:
    /**
     * @brief Constructor
     * @param hostname Hostname or IP address to bind to
     * @param port Port to listen on
     * @param logger_name Optional name for this server's logger
     */
    Server(const std::string &hostname,
           const std::string &port,
           const std::string &logger_name = "");

    /**
     * @brief Destructor
     *
     * Ensures the server is stopped before destruction
     */
    ~Server();

    /**
     * @brief Set handler for client connections
     * @param handler Function that processes client requests
     */
    void set_client_handler(std::unique_ptr<IClientHandler> handler);

    /**
     * @brief Set socket to non-blocking mode (for testing)
     * @param enabled Whether to enable non-blocking mode
     */
    void set_non_blocking_mode(bool enabled);

    /**
     * @brief Start the server
     * @return true if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if the server is currently running
     * @return true if running, false otherwise
     */
    bool is_running() const;

    /**
     * @brief Get number of active clients
     * @return Number of active client connections
     */
    size_t get_active_client_count() const;

  private:
    std::string m_hostname;
    std::string m_port;
    common::Logger m_logger;
    std::atomic<bool> m_running{false};
    std::unique_ptr<ConnectionManager> m_connection_manager;
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_HPP
