#include "client/client.hpp"
#include "client/response_manager.hpp"
#include "common/logging.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace fenris {
namespace client {

using namespace common;

Client::Client(const std::string &logger_name)
    : m_connection_manager(nullptr), m_tui(std::make_unique<TUI>()),
      m_request_manager(), m_response_manager(),
      m_logger(get_logger(logger_name)), m_exit_requested(false)
{
    m_logger->info("fenris client initialized");
}

Client::~Client()
{
    if (m_connection_manager && m_connection_manager->is_connected()) {
        m_connection_manager->disconnect();
        m_logger->info("disconnected from server");
    }
    m_logger->info("fenris client shutting down");
}

bool Client::connect_to_server()
{
    // If already connected, return true
    if (m_connection_manager && m_connection_manager->is_connected()) {
        return true;
    }

    // Create a new connection manager if needed
    if (!m_connection_manager) {
        m_connection_manager =
            std::make_unique<ConnectionManager>("ClientConnectionManager");
    }

    // Check if we need to get server details from user
    if (!m_connection_manager->has_connection_info()) {
        // Get server IP from user
        std::string server_ip = m_tui->get_server_IP();
        // Default port for Fenris server
        std::string server_port = m_tui->get_port_number();

        // Set the connection info
        m_connection_manager->set_connection_info(server_ip, server_port);

        m_logger->info("using server at {}:{}", server_ip, server_port);
    }

    // Get the current connection info for logging
    std::string server_ip = m_connection_manager->get_server_info().address;
    std::string server_port = m_connection_manager->get_server_info().port;

    // Attempt to connect
    m_logger->info("attempting to connect to server at {}:{}",
                   server_ip,
                   server_port);
    bool success = m_connection_manager->connect();

    if (success) {
        m_logger->info("successfully connected to server at {}:{}",
                       server_ip,
                       server_port);
        m_tui->display_result(true, "connected to server");
    } else {
        m_logger->error("failed to connect to server at {}:{}",
                        server_ip,
                        server_port);
        m_tui->display_result(false,
                              "Failed to connect to server. Please try a "
                              "different address or port.");

        // Reset connection info to force user to re-enter server details
        m_connection_manager->reset_connection_info();
    }

    return success;
}

bool Client::process_command(const std::vector<std::string> &command_parts)
{
    if (command_parts.empty()) {
        return true;
    }

    if (command_parts[0] == "exit") {
        m_logger->info("exit command received");
        m_exit_requested = true;
        return false;
    }

    if (command_parts[0] == "help") {
        m_tui->display_help();

        return true;
    }

    auto request_opt = m_request_manager.generate_request(command_parts);
    if (!request_opt.has_value()) {
        m_tui->display_result(false, "Invalid command or arguments");

        return true;
    }

    if (!m_connection_manager->send_request(request_opt.value())) {
        m_logger->error("failed to send request to server");
        m_tui->display_result(false, "Failed to send request to server");

        return true;
    }

    auto response_opt = m_connection_manager->receive_response();
    if (!response_opt.has_value()) {
        m_logger->error("failed to receive response from server");
        m_tui->display_result(false, "Failed to receive response from server");

        return true;
    }

    const auto &response = response_opt.value();
    std::vector<std::string> formatted_response =
        m_response_manager.handle_response(response);

    // Extract success status from the first element (following ResponseManager
    // convention)
    bool success =
        (formatted_response.size() > 0 && formatted_response[0] == "Success");

    // Display formatted results to user
    for (size_t i = 1; i < formatted_response.size(); ++i) {
        m_tui->display_result(success, formatted_response[i]);
    }

    // If no results were returned beyond the status, show a generic message
    if (formatted_response.size() <= 1) {
        m_tui->display_result(success,
                              success ? "Operation completed successfully"
                                      : "Operation failed");
    }

    // Update current directory if it was a cd command that succeeded
    if (command_parts[0] == "cd" && success && command_parts.size() > 1) {
        m_tui->update_current_directory(command_parts[1]);
    }

    return true;
}

void Client::run()
{
    m_logger->info("fenris client starting");

    // Check if TUI is properly initialized
    if (!m_tui) {
        m_logger->error("TUI not initialized, cannot run client");
        m_exit_requested = true;
        return;
    }

    // Main application loop
    while (!m_exit_requested) {
        // Ensure connection to server
        if (!m_connection_manager || !m_connection_manager->is_connected()) {
            if (!connect_to_server()) {
                // Allow retry after a short delay
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }

        try {
            // Get command from user
            auto command_parts = m_tui->get_command();

            // Process command and check if we should continue
            if (!process_command(command_parts)) {
                break;
            }
        } catch (const std::exception &e) {
            m_logger->error("exception during command processing: {}",
                            e.what());
            m_tui->display_result(false,
                                  std::string("internal error: ") + e.what());
        } catch (...) {
            m_logger->error("unknown exception during command processing");
            m_tui->display_result(false, "Unknown internal error occurred");
        }
    }

    // Clean up connection
    if (m_connection_manager && m_connection_manager->is_connected()) {
        m_connection_manager->disconnect();
        m_logger->info("disconnected from server");
        m_tui->display_result(true, "Disconnected from server");
    }

    m_logger->info("fenris client exiting");
}

void Client::set_connection_manager(
    std::unique_ptr<ConnectionManager> connection_manager)
{
    m_connection_manager = std::move(connection_manager);
}

void Client::set_tui(std::unique_ptr<ITUI> tui)
{
    m_tui = std::move(tui);
}

bool Client::is_exit_requested() const
{
    return m_exit_requested;
}

} // namespace client
} // namespace fenris
