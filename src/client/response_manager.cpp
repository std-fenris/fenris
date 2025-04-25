#include "client/response_manager.hpp"
#include "common/logging.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fenris {
namespace client {

using namespace common;

std::vector<std::string>
ResponseManager::handle_response(const fenris::Response &response)
{
    std::vector<std::string> result;

    result.push_back(response.success() ? "Success" : "Error");

    switch (response.type()) {
    case ResponseType::PONG:
        handle_pong_response(response, result);
        break;

    case ResponseType::FILE_INFO:
        handle_file_info_response(response, result);
        break;

    case ResponseType::FILE_CONTENT:
        handle_file_content_response(response, result);
        break;

    case ResponseType::DIR_LISTING:
        handle_directory_listing_response(response, result);
        break;

    case ResponseType::SUCCESS:
        handle_success_response(response, result);
        break;

    case ResponseType::ERROR:
        handle_error_response(response, result);
        break;

    case ResponseType::TERMINATED:
        handle_terminated_response(response, result);
        break;

    default:
        // Unknown response type
        result.push_back("Unknown response type");
        m_logger->warn("Received unknown response type: {}",
                       static_cast<int>(response.type()));
        break;
    }

    return result;
}

void ResponseManager::handle_pong_response(const fenris::Response &response,
                                           std::vector<std::string> &result)
{
    result.push_back("Server is alive");

    if (!response.data().empty()) {
        result.push_back("Message: " + response.data());
    }
}

void ResponseManager::handle_file_info_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    if (!response.has_file_info()) {
        result.push_back("Error: File info missing in response");
        return;
    }

    const auto &file_info = response.file_info();
    result.push_back("File: " + file_info.name());

    // Format file size with appropriate units
    std::string size_str = format_file_size(file_info.size());
    result.push_back("Size: " + size_str);

    // Format timestamp to human-readable date
    std::string time_str = format_timestamp(file_info.modified_time());
    result.push_back("Modified: " + time_str);

    // Add file/directory type indicator
    result.push_back("Type: " + std::string(file_info.is_directory()
                                                ? "Directory"
                                                : "File"));

    if (file_info.permissions()) {
        result.push_back("Permissions: " +
                         format_permissions(file_info.permissions()));
    }
}

void ResponseManager::handle_file_content_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    if (response.data().empty()) {
        result.push_back("(Empty file)");
        return;
    }

    // First check if data is binary (contains null bytes or non-printable
    // chars)
    bool is_binary = false;
    const size_t sample_size =
        std::min(response.data().size(),
                 static_cast<size_t>(1024)); // Check first 1KB
    for (size_t i = 0; i < sample_size; ++i) {
        unsigned char c = static_cast<unsigned char>(response.data()[i]);
        if (c == 0 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
            is_binary = true;
            break;
        }
    }

    if (is_binary) {
        result.push_back("(Binary data, " +
                         format_file_size(response.data().size()) + ")");
        return;
    }

    // Split the content into lines for text data
    std::istringstream stream(response.data());
    std::string line;
    while (std::getline(stream, line)) {
        result.push_back(line);
    }
}

void ResponseManager::handle_directory_listing_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    if (!response.has_directory_listing()) {
        if (!response.data().empty()) {
            // Fallback to legacy string representation if available
            result.push_back(response.data());
        } else {
            result.push_back("Error: Directory listing missing in response");
        }
        return;
    }

    const auto &listing = response.directory_listing();

    if (listing.entries_size() == 0) {
        result.push_back("(Empty directory)");
        return;
    }

    size_t name_width = 0;
    size_t size_width = 0;

    for (const auto &entry : listing.entries()) {
        name_width = std::max(name_width, entry.name().length());
        std::string size_str = format_file_size(entry.size());
        size_width = std::max(size_width, size_str.length());
    }

    std::ostringstream header;
    header << std::left << std::setw(name_width + 2) << "Name"
           << std::setw(size_width + 2) << "Size" << std::setw(20) << "Modified"
           << "Type";
    result.push_back(header.str());

    result.push_back(std::string(header.str().length(), '-'));

    for (const auto &entry : listing.entries()) {
        std::ostringstream line;
        line << std::left << std::setw(name_width + 2) << entry.name()
             << std::setw(size_width + 2) << format_file_size(entry.size())
             << std::setw(20) << format_timestamp(entry.modified_time())
             << (entry.is_directory() ? "Directory" : "File");
        result.push_back(line.str());
    }
}

void ResponseManager::handle_success_response(const fenris::Response &response,
                                              std::vector<std::string> &result)
{
    if (!response.data().empty()) {
        result.push_back(response.data());
    } else {
        result.push_back("Operation completed successfully");
    }
}

void ResponseManager::handle_error_response(const fenris::Response &response,
                                            std::vector<std::string> &result)
{
    if (!response.error_message().empty()) {
        result.push_back("Error: " + response.error_message());
    } else if (!response.data().empty()) {
        result.push_back("Error: " + response.data());
    } else {
        result.push_back("Unknown error occurred");
    }
}

void ResponseManager::handle_terminated_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    result.push_back("Server connection terminated");
    if (!response.data().empty()) {
        result.push_back("Reason: " + response.data());
    }
}

std::string ResponseManager::format_file_size(uint64_t size_bytes)
{
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;

    std::ostringstream size_stream;
    size_stream << std::fixed << std::setprecision(2);

    if (size_bytes < KB) {
        size_stream << size_bytes << " B";
    } else if (size_bytes < MB) {
        size_stream << (size_bytes / KB) << " KB";
    } else if (size_bytes < GB) {
        size_stream << (size_bytes / MB) << " MB";
    } else if (size_bytes < TB) {
        size_stream << (size_bytes / GB) << " GB";
    } else {
        size_stream << (size_bytes / TB) << " TB";
    }

    return size_stream.str();
}

std::string ResponseManager::format_timestamp(uint64_t timestamp)
{
    try {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm *tm_info = std::localtime(&time);

        if (tm_info == nullptr) {
            return "Invalid timestamp";
        }

        std::ostringstream time_stream;
        time_stream << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S");
        return time_stream.str();
    } catch (const std::exception &e) {
        m_logger->error("Error formatting timestamp: {}", e.what());
        return "Invalid timestamp";
    }
}

std::string ResponseManager::format_permissions(uint32_t permissions)
{
    std::ostringstream perm_stream;

    // Format as rwxrwxrwx
    perm_stream << ((permissions & 0400) ? "r" : "-")
                << ((permissions & 0200) ? "w" : "-")
                << ((permissions & 0100) ? "x" : "-")
                << ((permissions & 0040) ? "r" : "-")
                << ((permissions & 0020) ? "w" : "-")
                << ((permissions & 0010) ? "x" : "-")
                << ((permissions & 0004) ? "r" : "-")
                << ((permissions & 0002) ? "w" : "-")
                << ((permissions & 0001) ? "x" : "-");

    perm_stream << " (" << std::oct << permissions << std::dec << ")";

    return perm_stream.str();
}

ResponseManager::ResponseManager()
    : m_logger(common::get_logger("ResponseManager"))
{
}

} // namespace client
} // namespace fenris
