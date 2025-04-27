#include "server/request_manager.hpp"

namespace fenris {
namespace server {

ClientHandler::ClientHandler() : m_request_count(0) {}

void ClientHandler::step_directory_with_mutex(
    std::string &current_directory,
    std::string &new_directory,
    uint32_t &depth,
    std::shared_ptr<Node> &current_node)
{
    if (new_directory == "..") {
        // Go up one directory
        if (current_directory != "/") {
            current_directory.pop_back(); // Remove trailing slash
            size_t pos = current_directory.find_last_of('/');
            if (pos != std::string::npos) {
                pos++;
                current_directory = current_directory.substr(0, pos);
                depth--;
            }
            current_node->access_count--;
            current_node = current_node->parent.lock();
        }
    } else if (new_directory == ".") {
        // Stay in the current directory
    } else {
        // Change to the new directory
        auto it =
            std::find_if(current_node->children.begin(),
                         current_node->children.end(),
                         [&new_directory](const std::shared_ptr<Node> &child) {
                             return ((child->name == new_directory) &&
                                     (child->is_directory == true));
                         });
        if (it == current_node->children.end()) {
            assert(1 == 0);
            // Directory not found
        } else {
            current_node = *it;
            current_node->access_count++;
            current_directory += new_directory + "/";
            depth++;
        }
    }
    current_directory = new_directory;
}

void ClientHandler::traverse_back(std::string &current_directory,
                                  uint32_t &depth,
                                  std::shared_ptr<Node> &current_node)
{
    while (current_directory != "/") {
        step_directory_with_mutex(current_directory, "..", depth, current_node);
    }
}

pair<std::string, uint32_t>
ClientHandler::change_directory(std::string current_directory,
                                std::string path,
                                uint32_t &depth,
                                std::shared_ptr<Node> &current_node)
{
    if (path[path.size() - 1] == '/') {
        path = path.substr(0, path.size() - 1);
    }
    uint32_t ind = 0;
    if (path[0] == "/") {
        traverse_back(current_directory, depth, current_node);
        ind++;
    }
    while (path.find("/", ind) != std::string::npos) {
        uint32_t x = path.find("/", ind);
        step_directory_with_mutex(current_directory,
                                  request.filename().substr(ind, x - ind),
                                  depth,
                                  current_node);
        ind = x + 1;
    }
    return {current_directory, ind};
}

void ClientHandler::destroy_node(std::string &current_directory,
                                 std::shared_ptr<Node> &current_node)
{
    traverse_back(current_directory, 0, current_node);
    current_node->access_count--;
}

fenris::Response ClientHandler::handle_request(const fenris::Request &request,
                                               ClientInfo &client_info)
{

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handled_client_sockets.push_back(client_info.socket);
        m_received_requests.push_back(request);
        m_request_count++;
    }

    fenris::Response response;

    switch (request.command()) {
    case fenris::RequestType::PING: {
        response.set_type(fenris::ResponseType::PONG);
        response.set_success(true);
        response.set_data("PING");
        return response;
    }
    case fenris::RequestType::TERMINATE: {
        response.set_type(fenris::ResponseType::TERMINATED);
        response.set_success(true);
        response.set_data("Terminated successfully");
        traverse_back(client_info.current_directory,
                      client_info.depth,
                      client_info.current_node);
        client_info.keep_connection = false;
        return response;
    }
    default:
    }

    std::shared_ptr<Node> new_node = FST.root;
    new_node->access_count++;
    uint32_t new_depth = 0;
    try {
        change_directory("/",
                         client_info.current_directory,
                         new_depth,
                         new_node);
        auto [new_directory, ind] =
            change_directory(client_info.current_directory,
                             request.filename(),
                             new_depth,
                             new_node);
    } catch (const std::exception &e) {
        response.set_type(fenris::ResponseType::ERROR);
        response.set_success(false);
        response.set_error_message("Invalid Path!");
        destroy_node(new_directory, new_node);
        return response;
    }

    std::string _file = request.filename().substring(ind);
    std::string filename = DEFAULT_SERVER_DIR + new_directory + _file;
    if (filename[filename.size() - 1] == '/') {
        filename = filename.substr(0, filename.size() - 1);
        _file = _file.substr(0, _file.size() - 1);
    }

    switch (request.command()) {
    case fenris::RequestType::CREATE_FILE: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto result = fenris::common::create_file(filename);
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            if (!FST.addNode(filename, false)) {
                response.set_type(fenris::ResponseType::ERROR);
                response.set_success(false);
                response.set_error_message(
                    "FST not synchronized with file system.");
                break;
            }
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("CREATE_FILE");
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_ALREADY_EXISTS) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File already exists");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to create file");
        }
        break;
    }
    case fenris::RequestType::READ_FILE: {
        auto it = std::find_if(new_node->children.begin(),
                               new_node->children.end(),
                               [&_file](const std::shared_ptr<Node> &child) {
                                   return ((child->name == _file) &&
                                           (child->is_directory == false));
                               });

        if (it == new_node->children.end()) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File not found");
            break;
        }
        {
            std::lock_guard<std::mutex> lock((*it)->node_mutex);
            (*it)->access_count++;
        }

        auto [content, result] = fenris::common::read_file(filename);

        (*it)->access_count--;

        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::FILE_CONTENT);
            response.set_success(true);
            response.set_data(content.data(), content.size());
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File not found");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to read file");
        }
        break;
    }
    case fenris::RequestType::WRITE_FILE: {

        auto it = std::find_if(new_node->children.begin(),
                               new_node->children.end(),
                               [&_file](const std::shared_ptr<Node> &child) {
                                   return ((child->name == _file) &&
                                           (child->is_directory == false));
                               });

        if (it == new_node->children.end()) {
            std::lock_guard<std::mutex> lock(new_node->node_mutex);
            auto result = fenris::common::create_file(filename);
            if (result == fenris::common::FileOperationResult::SUCCESS) {
                if (!FST.addNode(filename, false)) {
                    response.set_type(fenris::ResponseType::ERROR);
                    response.set_success(false);
                    response.set_error_message(
                        "FST not synchronized with file system.");
                    break;
                }
                it = std::find_if(new_node->children.begin(),
                                  new_node->children.end(),
                                  [&_file](const std::shared_ptr<Node> &child) {
                                      return ((child->name == _file) &&
                                              (child->is_directory == false));
                                  });
            } else if (result == fenris::common::FileOperationResult::
                                     FILE_ALREADY_EXISTS) {
                response.set_type(fenris::ResponseType::ERROR);
                response.set_success(false);
                response.set_error_message("This should not happen");
                break;
            } else {
                response.set_type(fenris::ResponseType::ERROR);
                response.set_success(false);
                response.set_error_message("Failed to create file");
                break;
            }
        }

        std::lock_guard<std::mutex> lock((*it)->node_mutex);
        while ((*it)->access_count > 0) {
            // Wait for access count to be zero
        }

        auto result = fenris::common::write_file(
            filename,
            {request.data().begin(), request.data().end()});
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("The file has been written successfully");
        } else if (result ==
                   fenris::common::FileOperationResult::PERMISSION_DENIED) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message(
                "Permission denied to write to the file");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to write file");
        }
        break;
    }
    case fenris::RequestType::DELETE_FILE: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto it = std::find_if(new_node->children.begin(),
                               new_node->children.end(),
                               [&_file](const std::shared_ptr<Node> &child) {
                                   return ((child->name == _file) &&
                                           (child->is_directory == false));
                               });

        if (it == new_node->children.end()) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File not found");
            break;
        }
        std::lock_guard<std::mutex> lock((*it)->node_mutex);
        while ((*it)->access_count > 0) {
            // Wait for access count to be zero
        }
        auto result = fenris::common::delete_file(filename);
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("DELETE_FILE");
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File not found");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to delete file");
        }
        break;
    }
    case fenris::RequestType::INFO_FILE: {

        auto it = std::find_if(new_node->children.begin(),
                               new_node->children.end(),
                               [&_file](const std::shared_ptr<Node> &child) {
                                   return ((child->name == _file) &&
                                           (child->is_directory == false));
                               });

        if (it == new_node->children.end()) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File not found");
            break;
        }
        {
            std::lock_guard<std::mutex> lock((*it)->node_mutex);
            (*it)->access_count++;
        }

        auto [content, result] = fenris::common::get_file_info(filename);

        (*it)->access_count--;

        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::FILE_INFO);
            response.set_success(true);
            response.set_file_info(content);
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("File not found");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to fetch file info");
        }
        break;
    }
    case fenris::RequestType::CREATE_DIR: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto result = fenris::common::create_directory(filename);
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            FST.addNode(filename, true);
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("CREATE_DIR");
        } else if (result == fenris::common::FileOperationResult::
                                 DIRECTORY_ALREADY_EXISTS) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Directory already exists");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to create directory");
        }
        break;
    }
    case fenris::RequestType::LIST_DIR: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto [entries, result] = fenris::common::list_directory(filename);
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::DIR_LISTING);
            response.set_success(true);

            // fenris::DirectoryListing dir_listing;
            // for (const auto &entry : entries) {
            //     auto *file_info = dir_listing.add_entries();
            //     file_info->set_name(entry);
            // }
            // response.mutable_details()->set_allocated_directory_listing(
            //     &dir_listing);

        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Directory not found");
        }

        else if (result == fenris::common::FileOperationResult::INVALID_PATH) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Path is not a directory");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to list directory");
        }
        break;
    }
    case fenris::RequestType::CHANGE_DIR: {
        try {
            step_directory_with_mutex(new_directory,
                                      _file,
                                      new_depth,
                                      new_node);
        } catch (const std::exception &e) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Invalid Path!");
            destroy_node(new_directory, new_node);
            return response;
        }
        swap(new_directory, client_info.current_directory);
        swap(new_node, client_info.current_node);
        swap(new_depth, client_info.depth);
        response.set_type(fenris::ResponseType::SUCCESS);
        response.set_success(true);
        response.set_data("Changed directory successfully");
        break;
    }
    case fenris::RequestType::DELETE_DIR: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto it = std::find_if(new_node->children.begin(),
                               new_node->children.end(),
                               [&_file](const std::shared_ptr<Node> &child) {
                                   return child->name == _file;
                               });
        if (it == new_node->children.end()) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Directory does not exist");
            break;
        } else {
            if ((*it)->access_count > 0) {
                response.set_type(fenris::ResponseType::ERROR);
                response.set_success(false);
                response.set_error_message("Directory is in use");
                break;
            }
        }
        auto result = fenris::common::delete_directory(filename, true);
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("DELETE_DIRECTORY");
            FST.removeNode(filename);
        } else if (result ==
                   fenris::common::FileOperationResult::DIRECTORY_NOT_EMPTY) {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Directory is not empty");
        } else {
            response.set_type(fenris::ResponseType::ERROR);
            response.set_success(false);
            response.set_error_message("Failed to delete directory");
        }
        break;
    }
    default:
        response.set_type(fenris::ResponseType::ERROR);
        response.set_success(false);
        response.set_error_message("Unknown command");
    }

    return response;
}

std::vector<uint32_t> ClientHandler::get_handled_client_ids()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_handled_client_sockets;
}

std::vector<fenris::Request> ClientHandler::get_received_requests()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_received_requests;
}

int ClientHandler::get_request_count()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_request_count;
}

} // namespace server
} // namespace fenris
