#ifndef FENRIS_CLIENT_REQUEST_MANAGER_HPP
#define FENRIS_CLIENT_REQUEST_MANAGER_HPP

#include "common/logging.hpp"
#include "fenris.pb.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fenris {
namespace client {

/**
 * @class RequestManager
 * @brief Handles creation of client requests based on command line arguments
 */
class RequestManager {
  public:
    /**
     * @brief Default constructor
     */
    RequestManager() : m_logger(common::get_logger("ClientRequestManager")) {}

    /**
     * @brief Generate a Request object from command line arguments
     * @param args Vector of command line arguments
     * @return Optional Request object (empty if arguments are invalid)
     */
    std::optional<fenris::Request>
    generate_request(const std::vector<std::string> &args);

  private:
    common::Logger m_logger; // Added logger member
    // Maps string command names to RequestType enum values
    const std::unordered_map<std::string, fenris::RequestType> m_command_map = {
        {"ping", fenris::RequestType::PING},
        {"create", fenris::RequestType::CREATE_FILE},
        {"cat", fenris::RequestType::READ_FILE},
        {"write", fenris::RequestType::WRITE_FILE},
        {"append", fenris::RequestType::APPEND_FILE},
        {"rm", fenris::RequestType::DELETE_FILE},
        {"info", fenris::RequestType::INFO_FILE},
        {"mkdir", fenris::RequestType::CREATE_DIR},
        {"ls", fenris::RequestType::LIST_DIR},
        {"cd", fenris::RequestType::CHANGE_DIR},
        {"rmdir", fenris::RequestType::DELETE_DIR},
        {"terminate", fenris::RequestType::TERMINATE}};

    // Helper functions for specific request types
    fenris::Request create_file_request(const std::vector<std::string> &args,
                                        size_t start_idx);
    fenris::Request read_file_request(const std::vector<std::string> &args,
                                      size_t start_idx);
    fenris::Request write_file_request(const std::vector<std::string> &args,
                                       size_t start_idx);
    fenris::Request append_file_request(const std::vector<std::string> &args,
                                        size_t start_idx);
};

} // namespace client
} // namespace fenris

#endif // FENRIS_CLIENT_REQUEST_MANAGER_HPP
