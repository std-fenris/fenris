#include "server/server.hpp"
#include "common/logging.hpp"
#include "server/connection_manager.hpp"

namespace fenris {
namespace server {

using namespace common;

Server::Server(const std::string &hostname,
               const std::string &port,
               const std::string &logger_name)
    : m_hostname(hostname), m_port(port),
      m_logger(get_logger(logger_name.empty() ? "FenrisServer" : logger_name)),
      m_connection_manager(std::make_unique<ConnectionManager>(
          hostname,
          port,
          logger_name.empty() ? "FenrisServer" : logger_name))
{
    m_logger->info("Server initialized with host: {}, port: {}",
                   hostname,
                   port);
}

Server::~Server()
{
    if (is_running()) {
        stop();
    }
}

void Server::set_client_handler(std::unique_ptr<IClientHandler> handler)
{
    if (is_running()) {
        m_logger->warn("Cannot change client handler while server is running");
        return;
    }

    if (!handler) {
        m_logger->error("Attempted to set null client handler");
        return;
    }

    m_connection_manager->set_client_handler(std::move(handler));
    m_logger->debug("Client handler set successfully");
}

void Server::set_non_blocking_mode(bool enabled)
{
    if (is_running()) {
        m_logger->warn("Cannot change blocking mode while server is running");
        return;
    }

    m_connection_manager->set_non_blocking_mode(enabled);
    m_logger->debug("Non-blocking mode set to: {}", enabled);
}

bool Server::start()
{
    if (is_running()) {
        m_logger->warn("Server already running");
        return true;
    }

    m_logger->info("Starting server on {}:{}", m_hostname, m_port);
    m_connection_manager->start();
    m_running = true;

    // Check if server started successfully by verifying the connection manager
    // is active
    if (!m_connection_manager) {
        m_logger->error("Failed to start server");
        m_running = false;
        return false;
    }

    m_logger->info("Server started successfully");
    return true;
}

void Server::stop()
{
    if (!is_running()) {
        m_logger->warn("Server not running");
        return;
    }

    m_logger->info("Stopping server");
    m_connection_manager->stop();
    m_running = false;
    m_logger->info("Server stopped");
}

bool Server::is_running() const
{
    return m_running;
}

size_t Server::get_active_client_count() const
{
    if (!is_running() || !m_connection_manager) {
        return 0;
    }
    return m_connection_manager->get_active_client_count();
}

} // namespace server
} // namespace fenris
