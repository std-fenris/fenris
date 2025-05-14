#ifndef FENRIS_SERVER_REQUEST_MANAGER_HPP
#define FENRIS_SERVER_REQUEST_MANAGER_HPP

#include "common/file_operations.hpp"
#include "common/logging.hpp"
#include "fenris.pb.h"
#include "server/client_info.hpp"
#include "server/connection_manager.hpp"

namespace fenris {
namespace server {

class ClientHandler : public IClientHandler {
  public:
    // Default constructor that uses the server's main logger
    ClientHandler() : m_logger(common::get_logger("fenris_server")) {}

    // Constructor that accepts a specific logger name
    explicit ClientHandler(const std::string &logger_name)
        : m_logger(common::get_logger(logger_name))
    {
    }

    bool step_directory_with_mutex(std::string &current_directory,
                                   const std::string &new_directory,
                                   uint32_t &depth,
                                   std::shared_ptr<Node> &current_node);

    void traverse_back(std::string &current_directory,
                       uint32_t &depth,
                       std::shared_ptr<Node> &current_node);

    std::pair<std::string, uint32_t>
    change_directory(std::string current_directory,
                     std::string path,
                     uint32_t &depth,
                     std::shared_ptr<Node> &current_node);

    void destroy_node(std::string &current_directory,
                      uint32_t &depth,
                      std::shared_ptr<Node> &current_node);

    fenris::Response handle_request(const fenris::Request &request,
                                    ClientInfo &client_info);

    // void initialize_file_system_tree();

    FileSystemTree FST;

  private:
    common::Logger m_logger;
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_REQUEST_MANAGER_HPP
