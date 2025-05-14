#include "client/response_manager.hpp"
#include "client/colors.hpp"
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

ResponseManager::ResponseManager()
    : m_logger(common::get_logger("ResponseManager"))
{
    m_logger->debug("ResponseManager initialized");
}

std::vector<std::string>
ResponseManager::handle_response(const fenris::Response &response)
{
    m_logger->debug("Handling response of type: {}",
                    static_cast<int>(response.type()));
    std::vector<std::string> result;

    // Add colorized status as first line
    if (response.success()) {
        result.push_back(colors::success("Success"));
    } else {
        result.push_back(colors::error("Error"));
    }

    switch (response.type()) {
    case ResponseType::PONG:
        m_logger->debug("Processing PONG response");
        handle_pong_response(response, result);
        break;

    case ResponseType::FILE_INFO:
        m_logger->debug("Processing FILE_INFO response");
        handle_file_info_response(response, result);
        break;

    case ResponseType::FILE_CONTENT:
        m_logger->debug("Processing FILE_CONTENT response");
        handle_file_content_response(response, result);
        break;

    case ResponseType::DIR_LISTING:
        m_logger->debug("Processing DIR_LISTING response");
        handle_directory_listing_response(response, result);
        break;

    case ResponseType::SUCCESS:
        m_logger->debug("Processing SUCCESS response");
        handle_success_response(response, result);
        break;

    case ResponseType::ERROR:
        m_logger->debug("Processing ERROR response");
        handle_error_response(response, result);
        break;

    case ResponseType::TERMINATED:
        m_logger->debug("Processing TERMINATED response");
        handle_terminated_response(response, result);
        break;

    default:
        // Unknown response type
        result.push_back("Unknown response type");
        m_logger->warn("Received unknown response type: {}",
                       static_cast<int>(response.type()));
        break;
    }

    m_logger->debug("Response handling complete, generated {} result lines",
                    result.size());
    return result;
}

void ResponseManager::handle_pong_response(const fenris::Response &response,
                                           std::vector<std::string> &result)
{
    result.push_back(colors::success("Server is alive"));

    if (!response.data().empty()) {
        m_logger->debug("PONG response includes message: {}", response.data());
        result.push_back(colors::info("Message: " + response.data()));
    }
}

void ResponseManager::handle_file_info_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    if (!response.has_file_info()) {
        m_logger->warn("Received FILE_INFO response without file_info field");
        result.push_back("Error: File info missing in response");
        return;
    }

    const auto &file_info = response.file_info();
    m_logger->debug("Processing file info for: {}", file_info.name());

    if (colors::use_colors) {
        result.push_back(colors::BOLD + "File: " + colors::RESET +
                         colors::CYAN + file_info.name() + colors::RESET);

        // Format file size with appropriate units
        std::string size_str = format_file_size(file_info.size());
        result.push_back(colors::BOLD + "Size: " + colors::RESET + size_str);

        // Format timestamp to human-readable date
        std::string time_str = format_timestamp(file_info.modified_time());
        result.push_back(colors::BOLD + "Modified: " + colors::RESET +
                         time_str);

        // Add file/directory type indicator
        result.push_back(colors::BOLD + "Type: " + colors::RESET +
                         (file_info.is_directory()
                              ? colors::BLUE + "Directory" + colors::RESET
                              : colors::GREEN + "File" + colors::RESET));

        if (file_info.permissions()) {
            result.push_back(colors::BOLD + "Permissions: " + colors::RESET +
                             format_permissions(file_info.permissions()));
        }
    } else {
        // Plain text version for tests
        result.push_back("File: " + file_info.name());
        result.push_back("Size: " + format_file_size(file_info.size()));
        result.push_back("Modified: " +
                         format_timestamp(file_info.modified_time()));
        result.push_back("Type: " + std::string(file_info.is_directory()
                                                    ? "Directory"
                                                    : "File"));

        if (file_info.permissions()) {
            result.push_back("Permissions: " +
                             format_permissions(file_info.permissions()));
        }
    }

    m_logger->debug("File info formatted successfully");
}

void ResponseManager::handle_file_content_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    if (response.data().empty()) {
        m_logger->debug("File content is empty");
        if (colors::use_colors) {
            result.push_back(colors::YELLOW + "(Empty file)" + colors::RESET);
        } else {
            result.push_back("(Empty file)");
        }
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
        m_logger->debug(
            "File content appears to be binary data, size: {} bytes",
            response.data().size());
        if (colors::use_colors) {
            result.push_back(colors::MAGENTA + "(Binary data, " +
                             format_file_size(response.data().size()) + ")" +
                             colors::RESET);
        } else {
            result.push_back("(Binary data, " +
                             format_file_size(response.data().size()) + ")");
        }
        return;
    }

    m_logger->debug("Processing text file content, size: {} bytes",
                    response.data().size());

    // Split the content into lines for text data
    std::istringstream stream(response.data());
    std::string line;
    int line_count = 0;
    while (std::getline(stream, line)) {
        result.push_back(line);
        line_count++;
    }

    m_logger->debug("Processed {} lines of text content", line_count);
}

void ResponseManager::handle_directory_listing_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    if (!response.has_directory_listing()) {
        if (!response.data().empty()) {
            // Fallback to legacy string representation if available
            m_logger->warn(
                "Directory listing field missing, using legacy data field");
            result.push_back(response.data());
        } else {
            m_logger->error("Directory listing response missing both "
                            "directory_listing and data fields");
            result.push_back("Error: Directory listing missing in response");
        }
        return;
    }

    const auto &listing = response.directory_listing();
    m_logger->debug("Processing directory listing with {} entries",
                    listing.entries_size());

    if (listing.entries_size() == 0) {
        m_logger->debug("Directory is empty");
        if (colors::use_colors) {
            result.push_back(colors::YELLOW + "(Empty directory)" +
                             colors::RESET);
        } else {
            result.push_back("(Empty directory)");
        }
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
    if (colors::use_colors) {
        header << colors::BOLD << std::left << std::setw(name_width + 2)
               << "Name" << std::setw(size_width + 2) << "Size" << std::setw(20)
               << "Modified"
               << "Type" << colors::RESET;
        result.push_back(header.str());

        result.push_back(colors::CYAN +
                         std::string(header.str().length() -
                                         colors::BOLD.length() -
                                         colors::RESET.length(),
                                     '-') +
                         colors::RESET);
    } else {
        header << std::left << std::setw(name_width + 2) << "Name"
               << std::setw(size_width + 2) << "Size" << std::setw(20)
               << "Modified"
               << "Type";
        result.push_back(header.str());
        result.push_back(std::string(header.str().length(), '-'));
    }

    for (const auto &entry : listing.entries()) {
        std::ostringstream line;

        if (colors::use_colors) {
            line << std::left
                 << (entry.is_directory() ? colors::BLUE : colors::GREEN)
                 << std::setw(name_width + 2) << entry.name() << colors::RESET
                 << std::setw(size_width + 2) << format_file_size(entry.size())
                 << std::setw(20) << format_timestamp(entry.modified_time())
                 << (entry.is_directory()
                         ? colors::BLUE + "Directory" + colors::RESET
                         : colors::GREEN + "File" + colors::RESET);
        } else {
            line << std::left << std::setw(name_width + 2) << entry.name()
                 << std::setw(size_width + 2) << format_file_size(entry.size())
                 << std::setw(20) << format_timestamp(entry.modified_time())
                 << (entry.is_directory() ? "Directory" : "File");
        }

        result.push_back(line.str());
    }

    m_logger->debug("Directory listing formatted into {} rows",
                    listing.entries_size() + 2); // +2 for header and separator
}

void ResponseManager::handle_success_response(const fenris::Response &response,
                                              std::vector<std::string> &result)
{
    if (!response.data().empty()) {
        m_logger->debug("Success response includes message: {}",
                        response.data());

        if (colors::use_colors) {
            result.push_back(colors::success(response.data()));
        } else {
            result.push_back(response.data());
        }
    } else {
        m_logger->debug("Success response with no message");

        if (colors::use_colors) {
            result.push_back(
                colors::success("Operation completed successfully"));
        } else {
            result.push_back("Operation completed successfully");
        }
    }
}

void ResponseManager::handle_error_response(const fenris::Response &response,
                                            std::vector<std::string> &result)
{
    if (!response.error_message().empty()) {
        m_logger->warn("Error response: {}", response.error_message());
        if (colors::use_colors) {
            result.push_back(
                colors::error("Error: " + response.error_message()));
        } else {
            result.push_back("Error: " + response.error_message());
        }
    } else if (!response.data().empty()) {
        m_logger->warn("Error response (in data field): {}", response.data());
        if (colors::use_colors) {
            result.push_back(colors::error("Error: " + response.data()));
        } else {
            result.push_back("Error: " + response.data());
        }
    } else {
        m_logger->warn("Error response with no error message");
        if (colors::use_colors) {
            result.push_back(colors::error("Unknown error occurred"));
        } else {
            result.push_back("Unknown error occurred");
        }
    }
}

void ResponseManager::handle_terminated_response(
    const fenris::Response &response,
    std::vector<std::string> &result)
{
    m_logger->info("Connection termination acknowledged by server");

    if (colors::use_colors) {
        result.push_back(colors::warning("Server connection terminated"));
    } else {
        result.push_back("Server connection terminated");
    }

    if (!response.data().empty()) {
        m_logger->debug("Termination reason: {}", response.data());
        if (colors::use_colors) {
            result.push_back(colors::info("Reason: " + response.data()));
        } else {
            result.push_back("Reason: " + response.data());
        }
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

    std::string size_str;

    // Format size based on magnitude
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

    size_str = size_stream.str();

    // Apply appropriate color based on file size if colors are enabled
    if (colors::use_colors) {
        if (size_bytes < MB) {
            return colors::GREEN + size_str + colors::RESET;
        } else if (size_bytes < GB) {
            return colors::YELLOW + size_str + colors::RESET;
        } else if (size_bytes < TB) {
            return colors::MAGENTA + size_str + colors::RESET;
        } else {
            return colors::RED + size_str + colors::RESET;
        }
    }

    return size_str;
}

std::string ResponseManager::format_timestamp(uint64_t timestamp)
{
    try {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm *tm_info = std::localtime(&time);

        if (tm_info == nullptr) {
            m_logger->warn("Failed to convert timestamp: {}", timestamp);
            return colors::use_colors
                       ? colors::RED + "Invalid timestamp" + colors::RESET
                       : "Invalid timestamp";
        }

        std::ostringstream time_stream;
        time_stream << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S");

        return colors::use_colors
                   ? colors::CYAN + time_stream.str() + colors::RESET
                   : time_stream.str();
    } catch (const std::exception &e) {
        m_logger->error("Error formatting timestamp {}: {}",
                        timestamp,
                        e.what());
        return colors::use_colors
                   ? colors::RED + "Invalid timestamp" + colors::RESET
                   : "Invalid timestamp";
    }
}

std::string ResponseManager::format_permissions(uint32_t permissions)
{
    std::ostringstream perm_stream;

    if (colors::use_colors) {
        // Format as rwxrwxrwx with colors
        perm_stream
            << ((permissions & 0400) ? colors::GREEN + "r" : colors::RED + "-")
            << ((permissions & 0200) ? colors::GREEN + "w" : colors::RED + "-")
            << ((permissions & 0100) ? colors::GREEN + "x" : colors::RED + "-")
            << colors::RESET
            << ((permissions & 0040) ? colors::YELLOW + "r" : colors::RED + "-")
            << ((permissions & 0020) ? colors::YELLOW + "w" : colors::RED + "-")
            << ((permissions & 0010) ? colors::YELLOW + "x" : colors::RED + "-")
            << colors::RESET
            << ((permissions & 0004) ? colors::CYAN + "r" : colors::RED + "-")
            << ((permissions & 0002) ? colors::CYAN + "w" : colors::RED + "-")
            << ((permissions & 0001) ? colors::CYAN + "x" : colors::RED + "-")
            << colors::RESET;

        perm_stream << " (" << colors::YELLOW << std::oct << permissions
                    << std::dec << colors::RESET << ")";
    } else {
        // Format without colors for tests
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
    }

    return perm_stream.str();
}

} // namespace client
} // namespace fenris
