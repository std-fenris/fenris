#include "client/request_manager.hpp"
#include "common/request.hpp"
#include <fstream>
#include <sstream>

namespace fenris {
namespace client {

std::optional<fenris::Request>
RequestManager::generate_request(const std::vector<std::string> &args)
{
    if (args.empty()) {
        m_logger->error("no command provided");
        return std::nullopt;
    }

    // Get the command from the first argument
    const std::string &cmd = args[0];

    // Handle special client-side commands that map to other request types
    if (cmd == "upload") {
        if (args.size() < 3) {
            m_logger->error("upload command requires a local file path and "
                            "remote filename");
            return std::nullopt;
        }
        return upload_file_request(args, 1);
    }

    // Find the command in our map
    auto cmd_iter = m_command_map.find(cmd);
    if (cmd_iter == m_command_map.end()) {
        m_logger->error("unknown command '{}'", cmd);
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
            m_logger->error("create command requires a filename");
            return std::nullopt;
        }
        return create_file_request(args, 1);

    case fenris::RequestType::READ_FILE:
        if (args.size() < 2) {
            m_logger->error("read command requires a filename");
            return std::nullopt;
        }
        return read_file_request(args, 1);

    case fenris::RequestType::WRITE_FILE:
        if (args.size() < 3 && !(args.size() == 4 && args[1] == "-f")) {
            m_logger->error("write command requires a filename and content (or "
                            "-f <filepath>)");
            return std::nullopt;
        }
        return write_file_request(args, 1);

    case fenris::RequestType::APPEND_FILE:
        if (args.size() < 3 && !(args.size() == 4 && args[1] == "-f")) {
            m_logger->error("append command requires a filename and content "
                            "(or -f <filepath>)");
            return std::nullopt;
        }
        return append_file_request(args, 1);

    case fenris::RequestType::DELETE_FILE:
        if (args.size() < 2 && !(args.size() == 3 && args[1] == "-f")) {
            m_logger->error("delete_file command requires a filename");
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::INFO_FILE:
        if (args.size() < 2 && !(args.size() == 3 && args[1] == "-f")) {
            m_logger->error("info command requires a filename");
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::CREATE_DIR:
        if (args.size() < 2) {
            m_logger->error("mkdir command requires a directory name");
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
            m_logger->error("cd command requires a directory name");
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::DELETE_DIR:
        if (args.size() < 2) {
            m_logger->error("rmdir command requires a directory name");
            return std::nullopt;
        }
        request.set_filename(args[1]);
        break;

    case fenris::RequestType::TERMINATE:
        // No additional arguments needed for terminate
        break;

    default:
        m_logger->error("unhandled command type");
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
                m_logger->warn("could not open file '{}' for create content",
                               args[start_idx + 2]);
            } else {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                request.set_data(content);
            }
        } else {
            // Use argument as content (concatenate remaining args)
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
                m_logger->warn("could not open file '{}' for write content",
                               args[start_idx + 2]);
            } else {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                request.set_data(content);
            }
        } else {
            // Use argument as content (concatenate remaining args)
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
                m_logger->warn("could not open file '{}' for append content",
                               args[start_idx + 2]);
            } else {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                request.set_data(content);
            }
        } else {
            // Use argument as content (concatenate remaining args)
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
RequestManager::upload_file_request(const std::vector<std::string> &args,
                                    size_t start_idx)
{
    fenris::Request request;
    request.set_command(fenris::RequestType::WRITE_FILE);

    // args[start_idx] is the local file path to read from
    // args[start_idx + 1] is the remote filename to create
    std::string local_path = args[start_idx];
    std::string remote_filename = args[start_idx + 1];

    request.set_filename(remote_filename);

    // Read content from local file
    std::ifstream file(local_path, std::ios::binary);
    if (!file) {
        m_logger->error("could not open local file '{}' for upload",
                        local_path);
    } else {
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        request.set_data(content);
        m_logger->info("read {} bytes from '{}' for upload",
                       content.size(),
                       local_path);
    }

    return request;
}

} // namespace client
} // namespace fenris
