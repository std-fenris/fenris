#include "client/request_manager.hpp"
#include "common/request.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace fenris {
namespace client {

std::optional<fenris::Request>
RequestManager::generate_request(const std::vector<std::string> &args)
{

    if (args.empty()) {
        std::cerr << "Error: No command provided" << std::endl;
        return std::nullopt;
    }

    // Get the command from the first argument
    const std::string &cmd = args[0];

    // Find the command in our map
    auto cmd_iter = m_command_map.find(cmd);
    if (cmd_iter == m_command_map.end()) {
        std::cerr << "Error: Unknown command '" << cmd << "'" << std::endl;
        return std::nullopt;
    }

    fenris::Request request;
    request.set_command(cmd_iter->second);

    // Handle different command types
    switch (cmd_iter->second) {
    case fenris::RequestType::PING:
        // No additional arguments needed for ping
        break;

    case fenris::RequestType::CREATE_FILE:
        if (args.size() < 2) {
            std::cerr << "Error: create command requires a filename"
                      << std::endl;
            return std::nullopt;
        }
        return create_file_request(args, 1);

    case fenris::RequestType::READ_FILE:
        if (args.size() < 2) {
            std::cerr << "Error: read command requires a filename" << std::endl;
            return std::nullopt;
        }
        return read_file_request(args, 1);

    case fenris::RequestType::WRITE_FILE:
        if (args.size() < 3) {
            std::cerr << "Error: write command requires a filename and content"
                      << std::endl;
            return std::nullopt;
        }
        return write_file_request(args, 1);

    case fenris::RequestType::APPEND_FILE:
        if (args.size() < 3) {
            std::cerr << "Error: append command requires a filename and content"
                      << std::endl;
            return std::nullopt;
        }
        return append_file_request(args, 1);

    case fenris::RequestType::DELETE_FILE:
        if (args.size() < 2) {
            std::cerr << "Error: delete_file command requires a filename"
                      << std::endl;
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::INFO_FILE:
        if (args.size() < 2) {
            std::cerr << "Error: info command requires a filename" << std::endl;
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::CREATE_DIR:
        if (args.size() < 2) {
            std::cerr << "Error: mkdir command requires a directory name"
                      << std::endl;
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::LIST_DIR:
        if (args.size() < 2) {
            // Default to current directory if none specified
            request.set_filename(".");
        } else {
            request.set_filename(args[1]);
        }
        break;
    case fenris::RequestType::CHANGE_DIR:
        if (args.size() < 2) {
            std::cerr << "Error: cd command requires a directory name"
                      << std::endl;
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::DELETE_DIR:
        if (args.size() < 2) {
            std::cerr << "Error: rmdir command requires a directory name"
                      << std::endl;
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::TERMINATE:
        // No additional arguments needed for terminate
        break;

    default:
        std::cerr << "Error: Unhandled command type" << std::endl;
        return std::nullopt;
    }

    return request;
}

fenris::Request
RequestManager::create_file_request(const std::vector<std::string> &args,
                                    size_t start_idx)
{

    fenris::Request request;
    request.set_command(fenris::RequestType::CREATE_FILE);
    request.set_filename(args[start_idx]);

    // If content is provided (optional)
    if (args.size() > start_idx + 1) {
        // Check if it's a file path
        if (args[start_idx + 1] == "-f" && args.size() > start_idx + 2) {
            // Read content from file
            std::ifstream file(args[start_idx + 2], std::ios::binary);
            if (!file) {
                std::cerr << "Warning: Could not open file "
                          << args[start_idx + 2] << std::endl;
            } else {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                request.set_data(content);
            }
        } else {
            // Use argument as content
            request.set_data(args[start_idx + 1]);
        }
    }

    return request;
}

fenris::Request
RequestManager::read_file_request(const std::vector<std::string> &args,
                                  size_t start_idx)
{

    fenris::Request request;
    request.set_command(fenris::RequestType::READ_FILE);
    request.set_filename(args[start_idx]);

    return request;
}

fenris::Request
RequestManager::write_file_request(const std::vector<std::string> &args,
                                   size_t start_idx)
{

    fenris::Request request;
    request.set_command(fenris::RequestType::WRITE_FILE);
    request.set_filename(args[start_idx]);

    // Handle content
    if (args.size() > start_idx + 1) {
        if (args[start_idx + 1] == "-f" && args.size() > start_idx + 2) {
            // Read content from file
            std::ifstream file(args[start_idx + 2], std::ios::binary);
            if (!file) {
                std::cerr << "Warning: Could not open file "
                          << args[start_idx + 2] << std::endl;
            } else {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                request.set_data(content);
            }
        } else {
            // Use argument as content
            std::stringstream content;
            for (size_t i = start_idx + 1; i < args.size(); i++) {
                if (i > start_idx + 1)
                    content << " ";
                content << args[i];
            }
            request.set_data(content.str());
        }
    }

    return request;
}

fenris::Request
RequestManager::append_file_request(const std::vector<std::string> &args,
                                    size_t start_idx)
{

    fenris::Request request;
    request.set_command(fenris::RequestType::APPEND_FILE);
    request.set_filename(args[start_idx]);

    // Handle content (similar to write)
    if (args.size() > start_idx + 1) {
        if (args[start_idx + 1] == "-f" && args.size() > start_idx + 2) {
            // Read content from file
            std::ifstream file(args[start_idx + 2], std::ios::binary);
            if (!file) {
                std::cerr << "Warning: Could not open file "
                          << args[start_idx + 2] << std::endl;
            } else {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                request.set_data(content);
            }
        } else {
            // Use argument as content
            std::stringstream content;
            for (size_t i = start_idx + 1; i < args.size(); i++) {
                if (i > start_idx + 1)
                    content << " ";
                content << args[i];
            }
            request.set_data(content.str());
        }
    }

    return request;
}

} // namespace client
} // namespace fenris
