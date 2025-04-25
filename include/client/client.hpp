#ifndef FENRIS_CLIENT_HPP
#define FENRIS_CLIENT_HPP

#include "client/connection_manager.hpp"
#include "client/interface.hpp"
#include "client/request_manager.hpp"
#include "client/response_manager.hpp"
#include "common/logging.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fenris {
namespace client {

/**
 * @class Client
 * @brief Main client application class
 *
 * Manages the connection to the server, user interface interactions,
 * and request/response processing in a continuous loop.
 */
class Client {
  public:
    /**
     * @brief Constructor
     * @param logger_name Name for this client's logger
     */
    explicit Client(const std::string &logger_name = "FenrisClient");

    /**
     * @brief Destructor
     */
    ~Client();

    /**
     * @brief Sets a custom connection manager for testing
     * @param connection_manager Unique pointer to a connection manager
     */
    void set_connection_manager(
        std::unique_ptr<ConnectionManager> connection_manager);

    /**
     * @brief Sets a custom TUI for testing
     * @param tui Unique pointer to a TUI implementation
     */
    void set_tui(std::unique_ptr<ITUI> tui);

    /**
     * @brief Check if client has requested exit
     * @return True if exit was requested, false otherwise
     */
    bool is_exit_requested() const;

    /**
     * @brief Run the client application
     *
     * Main program loop that handles connection setup, command processing,
     * and server interactions.
     */
    void run();

  private:
    /**
     * @brief Establish connection to the server
     * @return true if connection successful, false otherwise
     */
    bool connect_to_server();

    /**
     * @brief Process a user command
     * @param command_parts Vector of command parts
     * @return true to continue execution, false to exit
     */
    bool process_command(const std::vector<std::string> &command_parts);

    std::unique_ptr<ConnectionManager> m_connection_manager;
    std::unique_ptr<ITUI> m_tui;
    RequestManager m_request_manager;
    ResponseManager m_response_manager;
    common::Logger m_logger;
    bool m_exit_requested{false};
};

} // namespace client
} // namespace fenris

#endif // FENRIS_CLIENT_HPP
