#include "server/request_manager.hpp"
#include <utility>
namespace fenris {
namespace server {

bool ClientHandler::step_directory_with_mutex(
    std::string &current_directory,
    const std::string &new_directory,
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
        auto it = FST.find_directory(current_node, new_directory);
        if (it == nullptr) {
            assert(false);
            // Directory not found
        } else {
            current_node = it;
            current_node->access_count++;
            current_directory += new_directory + "/";
            depth++;
        }
    }
    current_directory = new_directory;
    return true;
}

void ClientHandler::traverse_back(std::string &current_directory,
                                  uint32_t &depth,
                                  std::shared_ptr<Node> &current_node)
{
    while (current_directory != "/") {
        step_directory_with_mutex(current_directory, "..", depth, current_node);
    }
}

std::pair<std::string, uint32_t>
ClientHandler::change_directory(std::string current_directory,
                                std::string path,
                                uint32_t &depth,
                                std::shared_ptr<Node> &current_node)
{
    if (path[path.size() - 1] == '/') {
        path = path.substr(0, path.size() - 1);
    }
    uint32_t ind = 0;
    if (path[0] == '/') {
        traverse_back(current_directory, depth, current_node);
        ind++;
    }
    while (path.find("/", ind) != std::string::npos) {
        uint32_t x = path.find("/", ind);
        std::string sub_path = path.substr(ind, x - ind); // Temporary variable
        step_directory_with_mutex(current_directory,
                                  sub_path,
                                  depth,
                                  current_node);
        ind = x + 1;
    }
    return {current_directory, ind};
}

void ClientHandler::destroy_node(std::string &current_directory,
                                 uint32_t &depth,
                                 std::shared_ptr<Node> &current_node)
{
    traverse_back(current_directory, depth, current_node);
    current_node->access_count--;
}

fenris::Response ClientHandler::handle_request(const fenris::Request &request,
                                               ClientInfo &client_info)
{
    fenris::Response response;
    response.set_type(fenris::ResponseType::ERROR);
    response.set_success(false);

    switch (request.command()) {
    case fenris::RequestType::PING: {
        response.set_type(fenris::ResponseType::PONG);
        response.set_success(true);
        response.set_data("PONG");
        return response;
    }

    case fenris::RequestType::TERMINATE: {
        response.set_type(fenris::ResponseType::TERMINATED);
        response.set_success(true);
        response.set_data("Terminated successfully!");

        traverse_back(client_info.current_directory,
                      client_info.depth,
                      client_info.current_node);

        client_info.keep_connection = false;
        return response;
    }
    default:
        break;
    }

    std::shared_ptr<Node> new_node = FST.root;
    new_node->access_count++;

    uint32_t new_depth = 0;
    std::string new_directory = client_info.current_directory;
    uint32_t ind = 0;
    try {
        change_directory("/",
                         client_info.current_directory,
                         new_depth,
                         new_node);

        std::tie(new_directory, ind) =
            change_directory(client_info.current_directory,
                             request.filename(),
                             new_depth,
                             new_node);

    } catch (const std::exception &e) {
        response.set_error_message("Invalid Path!");
        destroy_node(new_directory, new_depth, new_node);
        return response;
    }

    std::string _file = request.filename().substr(ind);

    std::string filename = DEFAULT_SERVER_DIR + new_directory + _file;

    if (filename[filename.size() - 1] == '/') {
        filename = filename.substr(0, filename.size() - 1);
        _file = _file.substr(0, _file.size() - 1);
    }

    switch (request.command()) {
    case fenris::RequestType::CREATE_FILE: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);

        auto result = common::create_file(filename);

        if (result == common::FileOperationResult::SUCCESS) {

            if (!FST.add_node(filename, false)) {
                response.set_error_message(
                    "FST not synchronized with file system.");
                break;
            }

            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
        } else if (result == common::FileOperationResult::FILE_ALREADY_EXISTS) {
            response.set_error_message("File already exists!");
        } else {
            response.set_error_message("Failed to create file!");
        }

        break;
    }
    case fenris::RequestType::READ_FILE: {
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            response.set_error_message("File not found");
            break;
        }

        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            (it)->access_count++;
        }

        auto [content, result] = common::read_file(filename);

        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            (it)->access_count--;
        }

        if (result == common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::FILE_CONTENT);
            response.set_success(true);
            response.set_data(content.data(), content.size());
        } else if (result == common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_error_message("File not found");
        } else {
            response.set_error_message("Failed to read file");
        }

        break;
    }
    case fenris::RequestType::WRITE_FILE: {
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            std::lock_guard<std::mutex> lock(new_node->node_mutex);
            auto result = common::create_file(filename);

            if (result == common::FileOperationResult::SUCCESS) {
                if (!FST.add_node(filename, false)) {
                    response.set_error_message(
                        "FST not synchronized with file system.");
                    break;
                }

                it = FST.find_file(new_node, _file);

            } else if (result == fenris::common::FileOperationResult::
                                     FILE_ALREADY_EXISTS) {
                response.set_error_message("This should not happen");
                break;
            } else {
                response.set_error_message("Failed to create file");
                break;
            }
        }

        std::lock_guard<std::mutex> lock((it)->node_mutex);
        while ((it)->access_count > 0) {
            // Wait for access count to be zero
        }

        auto result =
            common::write_file(filename,
                               {request.data().begin(), request.data().end()});
        if (result == common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("The file has been written successfully");

        } else if (result == common::FileOperationResult::PERMISSION_DENIED) {
            response.set_error_message(
                "Permission denied to write to the file");
        } else {
            response.set_error_message("Failed to write file");
        }
        break;
    }
    case fenris::RequestType::DELETE_FILE: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        // Check if the file exists in the current node
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            response.set_error_message("File not found");
            break;
        }
        fenris::common::FileOperationResult result;
        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            while ((it)->access_count > 0) {
                // Wait for access count to be zero
            }
            result = fenris::common::delete_file(filename);
        }
        // `result` stores the outcome of the file deletion operation.
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
        } else if (result == common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_error_message("File not found");
        } else {
            response.set_error_message("Failed to delete file");
        }
        break;
    }
    case fenris::RequestType::INFO_FILE: {
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            response.set_error_message("File not found");
            break;
        }
        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            (it)->access_count++;
        }

        auto [content, result] = common::get_file_info(filename);

        (it)->access_count--;

        if (result == common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::FILE_INFO);
            response.set_success(true);
            fenris::FileInfo *file_info = response.mutable_file_info();
            file_info->set_name(content.name());
            file_info->set_size(content.size());
            file_info->set_is_directory(content.is_directory());
            file_info->set_modified_time(content.modified_time());
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_error_message("File not found");
        } else {
            response.set_error_message("Failed to fetch file info");
        }
        break;
    }
    case fenris::RequestType::CREATE_DIR: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);

        auto result = common::create_directory(filename);
        if (result == common::FileOperationResult::SUCCESS) {
            FST.add_node(filename, true);
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("CREATE_DIR");
        } else if (result ==
                   common::FileOperationResult::DIRECTORY_ALREADY_EXISTS) {
            response.set_error_message("Directory already exists");
        } else {
            response.set_error_message("Failed to create directory");
        }
        break;
    }
    case fenris::RequestType::LIST_DIR: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto [entries, result] = common::list_directory(filename);
        if (result == common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::DIR_LISTING);
            response.set_success(true);

            fenris::DirectoryListing *dir_listing =
                response.mutable_directory_listing();

            for (const auto &entry : entries) {
                fenris::FileInfo *file_info = dir_listing->add_entries();
                file_info->set_name(entry.name());
                file_info->set_size(entry.size());
                file_info->set_is_directory(entry.is_directory());
                file_info->set_modified_time(entry.modified_time());
            }
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            response.set_error_message("Directory not found");
        }

        else if (result == common::FileOperationResult::INVALID_PATH) {
            response.set_error_message("Path is not a directory");
        } else {
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
            response.set_error_message("Invalid Path!");
            destroy_node(new_directory, new_depth, new_node);
            return response;
        }
        std::swap(new_directory, client_info.current_directory);
        std::swap(new_node, client_info.current_node);
        std::swap(new_depth, client_info.depth);
        response.set_type(fenris::ResponseType::SUCCESS);
        response.set_success(true);
        response.set_data("Changed directory successfully");
        break;
    }
    case fenris::RequestType::DELETE_DIR: {
        std::lock_guard<std::mutex> lock(new_node->node_mutex);

        auto it = FST.find_directory(new_node, _file);
        if (it == nullptr) {
            response.set_error_message("Directory does not exist");
            break;
        } else {
            if ((it)->access_count > 0) {
                response.set_error_message("Directory is in use");
                break;
            }
        }
        auto result = common::delete_directory(filename, true);
        if (result == common::FileOperationResult::SUCCESS) {
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("DELETE_DIRECTORY");
            FST.remove_node(filename);
        } else if (result == common::FileOperationResult::DIRECTORY_NOT_EMPTY) {
            response.set_error_message("Directory is not empty");
        } else {
            response.set_error_message("Failed to delete directory");
        }
        break;
    }
    default:
        response.set_error_message("Unknown command");
    }

    return response;
}

} // namespace server
} // namespace fenris
