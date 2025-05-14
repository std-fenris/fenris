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
    m_logger->debug("Stepping directory from '{}' to '{}'",
                    current_directory,
                    new_directory);
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
            m_logger->debug("Moved up one directory to '{}'",
                            current_directory);
        } else {
            m_logger->debug("Already at root directory");
        }
    } else if (new_directory == ".") {
        // Stay in the current directory
        m_logger->debug("Staying in current directory");
    } else {
        // Change to the new directory
        auto it = FST.find_directory(current_node, new_directory);
        if (it == nullptr) {
            m_logger->error("Directory '{}' not found", new_directory);
            // Directory not found
        } else {
            current_node = it;
            current_node->access_count++;
            current_directory += new_directory + "/";
            depth++;
            m_logger->debug("Changed to directory '{}'", current_directory);
        }
    }
    current_directory = new_directory;
    return true;
}

void ClientHandler::traverse_back(std::string &current_directory,
                                  uint32_t &depth,
                                  std::shared_ptr<Node> &current_node)
{
    m_logger->debug("Traversing back from directory '{}'", current_directory);
    while (current_directory != "/") {
        step_directory_with_mutex(current_directory, "..", depth, current_node);
    }
    m_logger->debug("Traversed back to root directory");
}

std::pair<std::string, uint32_t>
ClientHandler::change_directory(std::string current_directory,
                                std::string path,
                                uint32_t &depth,
                                std::shared_ptr<Node> &current_node)
{
    m_logger->debug("Changing directory from '{}' to path '{}'",
                    current_directory,
                    path);
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
        m_logger->debug("Step through path component: '{}'", sub_path);
        step_directory_with_mutex(current_directory,
                                  sub_path,
                                  depth,
                                  current_node);
        ind = x + 1;
    }
    m_logger->debug("Directory changed to '{}', index: {}",
                    current_directory,
                    ind);
    return {current_directory, ind};
}

void ClientHandler::destroy_node(std::string &current_directory,
                                 uint32_t &depth,
                                 std::shared_ptr<Node> &current_node)
{
    m_logger->debug("Destroying node at '{}'", current_directory);
    traverse_back(current_directory, depth, current_node);
    current_node->access_count--;
    m_logger->debug("Node destroyed, access_count decreased");
}

fenris::Response ClientHandler::handle_request(const fenris::Request &request,
                                               ClientInfo &client_info)
{
    m_logger->debug("Handling request of type: {}",
                    static_cast<int>(request.command()));
    fenris::Response response;
    response.set_type(fenris::ResponseType::ERROR);
    response.set_success(false);

    switch (request.command()) {
    case fenris::RequestType::PING: {
        m_logger->debug("Processing PING request");
        response.set_type(fenris::ResponseType::PONG);
        response.set_success(true);
        response.set_data("PONG");
        return response;
    }

    case fenris::RequestType::TERMINATE: {
        m_logger->debug("Processing TERMINATE request");
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
        m_logger->debug("Navigating to client's current directory: '{}'",
                        client_info.current_directory);
        change_directory("/",
                         client_info.current_directory,
                         new_depth,
                         new_node);

        m_logger->debug("Changing directory for request filename: '{}'",
                        request.filename());
        std::tie(new_directory, ind) =
            change_directory(client_info.current_directory,
                             request.filename(),
                             new_depth,
                             new_node);

    } catch (const std::exception &e) {
        m_logger->error("Exception during path navigation: {}", e.what());
        response.set_error_message("Invalid Path!");
        destroy_node(new_directory, new_depth, new_node);
        return response;
    }

    std::string _file = request.filename().substr(ind);
    std::string filename = new_directory + _file;
    std::string absolute_filepath = DEFAULT_SERVER_DIR + filename;
    m_logger->debug("Absolute path: '{}'", absolute_filepath);

    if (filename[filename.size() - 1] == '/') {
        filename = filename.substr(0, filename.size() - 1);
        _file = _file.substr(0, _file.size() - 1);
    }

    m_logger->debug("Target filename: '{}'", filename);

    switch (request.command()) {
    case fenris::RequestType::CREATE_FILE: {
        m_logger->debug("Processing CREATE_FILE request for '{}'", filename);
        std::lock_guard<std::mutex> lock(new_node->node_mutex);

        auto result = common::create_file(absolute_filepath);

        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("File created successfully");
            if (!FST.add_node(filename, false)) {
                m_logger->error("FST not synchronized with file system");
                response.set_error_message(
                    "FST not synchronized with file system.");
                break;
            }

            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
        } else if (result == common::FileOperationResult::FILE_ALREADY_EXISTS) {
            m_logger->warn("File already exists: '{}'", filename);
            response.set_error_message("File already exists!");
        } else {
            m_logger->error("Failed to create file: '{}'", filename);
            response.set_error_message("Failed to create file!");
        }

        break;
    }
    case fenris::RequestType::READ_FILE: {
        m_logger->debug("Processing READ_FILE request for '{}'", filename);
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            m_logger->error("File not found: '{}'", filename);
            response.set_error_message("File not found");
            break;
        }

        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            (it)->access_count++;
            m_logger->debug("Incremented access count for file");
        }

        auto [content, result] = common::read_file(absolute_filepath);

        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            (it)->access_count--;
            m_logger->debug("Decremented access count for file");
        }

        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("File read successfully, content size: {}",
                            content.size());
            response.set_type(fenris::ResponseType::FILE_CONTENT);
            response.set_success(true);
            response.set_data(content.data(), content.size());
        } else if (result == common::FileOperationResult::FILE_NOT_FOUND) {
            m_logger->error("File not found: '{}'", filename);
            response.set_error_message("File not found");
        } else {
            m_logger->error("Failed to read file: '{}'", filename);
            response.set_error_message("Failed to read file");
        }

        break;
    }
    case fenris::RequestType::WRITE_FILE: {
        m_logger->debug("Processing WRITE_FILE request for '{}'", filename);
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            std::lock_guard<std::mutex> lock(new_node->node_mutex);
            auto result = common::create_file(absolute_filepath);

            if (result == common::FileOperationResult::SUCCESS) {
                m_logger->debug("File created successfully");
                if (!FST.add_node(filename, false)) {
                    m_logger->error("FST not synchronized with file system");
                    response.set_error_message(
                        "FST not synchronized with file system.");
                    break;
                }

                it = FST.find_file(new_node, _file);

            } else if (result == fenris::common::FileOperationResult::
                                     FILE_ALREADY_EXISTS) {
                m_logger->error("This should not happen: '{}'", filename);
                response.set_error_message("This should not happen");
                break;
            } else {
                m_logger->error("Failed to create file: '{}'", filename);
                response.set_error_message("Failed to create file");
                break;
            }
        }

        std::lock_guard<std::mutex> lock((it)->node_mutex);
        while ((it)->access_count > 0) {
            // Wait for access count to be zero
        }

        auto result =
            common::write_file(absolute_filepath,
                               {request.data().begin(), request.data().end()});
        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("File written successfully");
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("The file has been written successfully");

        } else if (result == common::FileOperationResult::PERMISSION_DENIED) {
            m_logger->error("Permission denied to write to the file: '{}'",
                            filename);
            response.set_error_message(
                "Permission denied to write to the file");
        } else {
            m_logger->error("Failed to write file: '{}'", filename);
            response.set_error_message("Failed to write file");
        }
        break;
    }
    case fenris::RequestType::DELETE_FILE: {
        m_logger->debug("Processing DELETE_FILE request for '{}'", filename);
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        // Check if the file exists in the current node
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            m_logger->error("File not found: '{}'", filename);
            response.set_error_message("File not found");
            break;
        }
        fenris::common::FileOperationResult result;
        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            while ((it)->access_count > 0) {
                // Wait for access count to be zero
            }
            result = fenris::common::delete_file(absolute_filepath);
        }
        // `result` stores the outcome of the file deletion operation.
        if (result == fenris::common::FileOperationResult::SUCCESS) {
            m_logger->debug("File deleted successfully");
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
        } else if (result == common::FileOperationResult::FILE_NOT_FOUND) {
            m_logger->error("File not found: '{}'", filename);
            response.set_error_message("File not found");
        } else {
            m_logger->error("Failed to delete file: '{}'", filename);
            response.set_error_message("Failed to delete file");
        }
        break;
    }
    case fenris::RequestType::INFO_FILE: {
        m_logger->debug("Processing INFO_FILE request for '{}'", filename);
        auto it = FST.find_file(new_node, _file);

        if (it == nullptr) {
            m_logger->error("File not found: '{}'", filename);
            response.set_error_message("File not found");
            break;
        }
        {
            std::lock_guard<std::mutex> lock((it)->node_mutex);
            (it)->access_count++;
            m_logger->debug("Incremented access count for file info");
        }

        auto [content, result] = common::get_file_info(absolute_filepath);

        (it)->access_count--;
        m_logger->debug("Decremented access count for file info");

        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("File info retrieved successfully");
            response.set_type(fenris::ResponseType::FILE_INFO);
            response.set_success(true);
            fenris::FileInfo *file_info = response.mutable_file_info();
            file_info->set_name(content.name());
            file_info->set_size(content.size());
            file_info->set_is_directory(content.is_directory());
            file_info->set_modified_time(content.modified_time());
        } else if (result ==
                   fenris::common::FileOperationResult::FILE_NOT_FOUND) {
            m_logger->error("File not found: '{}'", filename);
            response.set_error_message("File not found");
        } else {
            m_logger->error("Failed to fetch file info: '{}'", filename);
            response.set_error_message("Failed to fetch file info");
        }
        break;
    }
    case fenris::RequestType::CREATE_DIR: {
        m_logger->debug("Processing CREATE_DIR request for '{}'", filename);
        std::lock_guard<std::mutex> lock(new_node->node_mutex);

        auto result = common::create_directory(absolute_filepath);
        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("Directory created successfully");
            FST.add_node(filename, true);
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
        } else if (result ==
                   common::FileOperationResult::DIRECTORY_ALREADY_EXISTS) {
            m_logger->warn("Directory already exists: '{}'", filename);
            response.set_error_message("Directory already exists");
        } else {
            m_logger->error("Failed to create directory: '{}'", filename);
            response.set_error_message("Failed to create directory");
        }
        break;
    }
    case fenris::RequestType::LIST_DIR: {
        m_logger->debug("Processing LIST_DIR request for '{}'", filename);
        std::lock_guard<std::mutex> lock(new_node->node_mutex);
        auto [entries, result] = common::list_directory(absolute_filepath);
        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("Directory listed successfully, found {} entries",
                            entries.size());
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
            m_logger->error("Directory not found: '{}'", filename);
            response.set_error_message("Directory not found");
        } else if (result == common::FileOperationResult::INVALID_PATH) {
            m_logger->error("Path is not a directory: '{}'", filename);
            response.set_error_message("Path is not a directory");
        } else {
            m_logger->error("Failed to list directory: '{}'", filename);
            response.set_error_message("Failed to list directory");
        }
        break;
    }
    case fenris::RequestType::CHANGE_DIR: {
        m_logger->debug("Processing CHANGE_DIR request for '{}'", filename);
        try {
            step_directory_with_mutex(new_directory,
                                      _file,
                                      new_depth,
                                      new_node);
        } catch (const std::exception &e) {
            m_logger->error("Invalid Path: '{}'", e.what());
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
        m_logger->debug("Processing DELETE_DIR request for '{}'", filename);
        std::lock_guard<std::mutex> lock(new_node->node_mutex);

        auto it = FST.find_directory(new_node, _file);
        if (it == nullptr) {
            m_logger->error("Directory does not exist: '{}'", filename);
            response.set_error_message("Directory does not exist");
            break;
        } else {
            if ((it)->access_count > 0) {
                m_logger->warn("Directory is in use: '{}'", filename);
                response.set_error_message("Directory is in use");
                break;
            }
        }
        auto result = common::delete_directory(absolute_filepath, true);
        if (result == common::FileOperationResult::SUCCESS) {
            m_logger->debug("Directory deleted successfully");
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_success(true);
            response.set_data("DELETE_DIRECTORY");
            FST.remove_node(filename);
        } else if (result == common::FileOperationResult::DIRECTORY_NOT_EMPTY) {
            m_logger->warn("Directory is not empty: '{}'", filename);
            response.set_error_message("Directory is not empty");
        } else {
            m_logger->error("Failed to delete directory: '{}'", filename);
            response.set_error_message("Failed to delete directory");
        }
        break;
    }
    default:
        m_logger->warn("Unknown command: {}",
                       static_cast<int>(request.command()));
        response.set_error_message("Unknown command");
    }

    return response;
}

} // namespace server
} // namespace fenris
