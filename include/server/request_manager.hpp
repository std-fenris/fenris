#ifndef FENRIS_SERVER_REQUEST_MANAGER_HPP
#define FENRIS_SERVER_REQUEST_MANAGER_HPP

#include "common/file_operations.hpp"
#include "fenris.pb.h"
#include "server/fenris_server_struct.hpp"

namespace fenris {
namespace server {

class ClientHandler {
  public:
    explicit ClientHandler();

    void step_directory_with_mutex(std::string &current_directory,
                                   std::string &new_directory,
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
                      std::shared_ptr<Node> &current_node);

    fenris::Response handle_request(const fenris::Request &request,
                                    ClientInfo &client_info);

    std::vector<uint32_t> get_handled_client_ids();

    std::vector<fenris::Request> get_received_requests();

    int get_request_count();

  private:
    int m_request_count;
    std::mutex m_mutex;
    std::vector<uint32_t> m_handled_client_sockets;
    std::vector<fenris::Request> m_received_requests;
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_REQUEST_MANAGER_HPP
