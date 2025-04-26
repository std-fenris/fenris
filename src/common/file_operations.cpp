#include "common/file_operations.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

namespace fenris {
namespace common {

std::string file_operation_result_to_string(FileOperationResult result)
{
    switch (result) {
    case FileOperationResult::SUCCESS:
        return "success";
    case FileOperationResult::FILE_NOT_FOUND:
        return "file not found";
    case FileOperationResult::PERMISSION_DENIED:
        return "permission denied";
    case FileOperationResult::PATH_NOT_EXIST:
        return "path does not exist";
    case FileOperationResult::FILE_ALREADY_EXISTS:
        return "file already exists";
    case FileOperationResult::DIRECTORY_NOT_EMPTY:
        return "directory not empty";
    case FileOperationResult::IO_ERROR:
        return "i/o error";
    case FileOperationResult::INVALID_PATH:
        return "invalid path";
    case FileOperationResult::DIRECTORY_ALREADY_EXISTS:
        return "directory already exists";
    case FileOperationResult::UNKNOWN_ERROR:
        return "unknown error";
    default:
        return "unrecognized error";
    }
}

// Rename function for consistency
FileOperationResult
system_error_to_file_operation_result(const std::error_code &ec)
{
    if (!ec) {
        return FileOperationResult::SUCCESS;
    }

    switch (static_cast<std::errc>(ec.value())) {
    case std::errc::no_such_file_or_directory:
        return FileOperationResult::FILE_NOT_FOUND;

    case std::errc::permission_denied:
        return FileOperationResult::PERMISSION_DENIED;

    case std::errc::file_exists:
        return FileOperationResult::FILE_ALREADY_EXISTS;

    case std::errc::directory_not_empty:
        return FileOperationResult::DIRECTORY_NOT_EMPTY;

    case std::errc::invalid_argument:
    case std::errc::filename_too_long:
        return FileOperationResult::INVALID_PATH;

    case std::errc::io_error:
        return FileOperationResult::IO_ERROR;

    default:
        return FileOperationResult::UNKNOWN_ERROR;
    }
}

std::pair<std::string, FileOperationResult>
read_file(const std::string &filepath)
{
    std::string content;
    std::error_code ec;
    if (!fs::exists(filepath, ec)) {
        return {content, FileOperationResult::FILE_NOT_FOUND};
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return {content, FileOperationResult::IO_ERROR};
    }

    // Get file size
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    content.resize(static_cast<size_t>(file_size));

    if (!file.read(content.data(), file_size)) {
        return {"", FileOperationResult::IO_ERROR};
    }

    return {content, FileOperationResult::SUCCESS};
}

FileOperationResult write_file(const std::string &filepath,
                               const std::string &data)
{
    std::error_code ec;

    // Check if file exists and is writable
    if (fs::exists(filepath, ec)) {
        fs::file_status file_status = fs::status(filepath, ec);
        if (ec) {
            return system_error_to_file_operation_result(ec);
        }

        if ((file_status.permissions() & fs::perms::owner_write) ==
            fs::perms::none) {
            return FileOperationResult::PERMISSION_DENIED;
        }
    } else {
        // Check if parent directory exists and is writable
        fs::path parent_path = fs::path(filepath).parent_path();
        if (!parent_path.empty() && fs::exists(parent_path, ec)) {
            fs::file_status parent_status = fs::status(parent_path, ec);
            if (ec) {
                return system_error_to_file_operation_result(ec);
            }

            if ((parent_status.permissions() & fs::perms::owner_write) ==
                fs::perms::none) {
                return FileOperationResult::PERMISSION_DENIED;
            }
        }
    }

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (errno == EACCES || errno == EPERM) {
            return FileOperationResult::PERMISSION_DENIED;
        }
        return FileOperationResult::IO_ERROR;
    }

    if (!file.write(data.data(), data.size())) {
        return FileOperationResult::IO_ERROR;
    }

    return FileOperationResult::SUCCESS;
}

FileOperationResult append_file(const std::string &filepath,
                                const std::string &data)
{
    std::error_code ec;

    // Check if file exists
    if (!fs::exists(filepath, ec)) {
        return FileOperationResult::FILE_NOT_FOUND;
    }

    // Check file permissions
    fs::file_status file_status = fs::status(filepath, ec);
    if (ec) {
        return system_error_to_file_operation_result(ec);
    }

    // Check write permissions on the file
    if ((file_status.permissions() & fs::perms::owner_write) ==
        fs::perms::none) {
        return FileOperationResult::PERMISSION_DENIED;
    }

    // Open file in binary mode with append flag
    std::ofstream file(filepath, std::ios::binary | std::ios::app);
    if (!file) {
        // Check specifically for permission errors
        if (errno == EACCES || errno == EPERM) {
            return FileOperationResult::PERMISSION_DENIED;
        }
        return FileOperationResult::IO_ERROR;
    }

    if (!file.write(data.data(), data.size())) {
        return FileOperationResult::IO_ERROR;
    }

    return FileOperationResult::SUCCESS;
}

FileOperationResult create_file(const std::string &filepath)
{
    std::error_code ec;

    if (fs::exists(filepath, ec)) {
        return FileOperationResult::FILE_ALREADY_EXISTS;
    }

    // Check if parent directory exists and is writable
    fs::path parent_path = fs::path(filepath).parent_path();
    if (!parent_path.empty()) {
        if (!fs::exists(parent_path, ec)) {
            return FileOperationResult::FILE_NOT_FOUND;
        }

        // Check write permissions on parent directory
        fs::file_status parent_status = fs::status(parent_path, ec);
        if (ec) {
            return system_error_to_file_operation_result(ec);
        }

        // If we can't write to the directory, return permission denied
        if ((parent_status.permissions() & fs::perms::owner_write) ==
            fs::perms::none) {
            return FileOperationResult::PERMISSION_DENIED;
        }
    }

    std::ofstream file(filepath);
    if (!file) {
        // Check specifically for permission errors
        if (errno == EACCES || errno == EPERM) {
            return FileOperationResult::PERMISSION_DENIED;
        }
        return FileOperationResult::IO_ERROR;
    }
    file.close();

    if (!fs::exists(filepath, ec)) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

FileOperationResult delete_file(const std::string &filepath)
{
    std::error_code ec;

    if (!fs::exists(filepath, ec)) {
        return FileOperationResult::FILE_NOT_FOUND;
    }

    if (!fs::is_regular_file(filepath, ec)) {
        return FileOperationResult::INVALID_PATH;
    }

    if (!fs::remove(filepath, ec)) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

std::pair<fenris::FileInfo, FileOperationResult>
get_file_info(const std::string &filepath)
{
    std::error_code ec;
    FileInfo file_info;

    // Check if file exists
    if (!fs::exists(filepath, ec)) {
        return {file_info, FileOperationResult::FILE_NOT_FOUND};
    }

    // Set file name
    file_info.set_name(filepath);

    // Set file size (for regular files)
    if (fs::is_regular_file(filepath, ec)) {
        if (ec) {
            return {file_info, system_error_to_file_operation_result(ec)};
        }

        uintmax_t size = fs::file_size(filepath, ec);
        if (ec) {
            return {file_info, system_error_to_file_operation_result(ec)};
        }
        file_info.set_size(size);
    } else {
        // For directories or other types, size is 0
        file_info.set_size(0);
    }

    // Check if it's a directory
    file_info.set_is_directory(fs::is_directory(filepath, ec));
    if (ec) {
        return {file_info, system_error_to_file_operation_result(ec)};
    }

    // Get last modified time
    auto last_write = fs::last_write_time(filepath, ec);
    if (ec) {
        return {file_info, system_error_to_file_operation_result(ec)};
    }
    file_info.set_modified_time(last_write.time_since_epoch().count());

    // Get permissions
    fs::file_status status = fs::status(filepath, ec);
    if (ec) {
        return {file_info, system_error_to_file_operation_result(ec)};
    }
    auto perms = status.permissions();
    file_info.set_permissions(static_cast<uint32_t>(
        perms &
        (fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all)));

    return {file_info, FileOperationResult::SUCCESS};
}

bool file_exists(const std::string &filepath)
{
    std::error_code ec;
    return fs::exists(filepath, ec) && !ec;
}

FileOperationResult create_directory(const std::string &dirpath)
{
    std::error_code ec;

    if (fs::exists(dirpath, ec)) {
        if (fs::is_directory(dirpath, ec)) {
            return FileOperationResult::DIRECTORY_ALREADY_EXISTS;
        } else {
            // Path exists but is not a directory
            return FileOperationResult::INVALID_PATH;
        }
    }

    if (!fs::create_directory(dirpath, ec)) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

FileOperationResult create_directories(const std::string &dirpath)
{
    std::error_code ec;

    // Create the directory and all parent directories
    if (!fs::create_directories(dirpath, ec)) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

FileOperationResult delete_directory(const std::string &dirpath, bool recursive)
{
    std::error_code ec;

    if (!fs::exists(dirpath, ec)) {
        return system_error_to_file_operation_result(
            ec.value()
                ? ec
                : std::make_error_code(std::errc::no_such_file_or_directory));
    }

    if (!fs::is_directory(dirpath, ec)) {
        return FileOperationResult::INVALID_PATH;
    }

    if (recursive) {
        fs::remove_all(dirpath, ec);
        if (ec) {
            return system_error_to_file_operation_result(ec);
        }
    } else {
        // Try to remove the directory (will fail if not empty)
        if (!fs::remove(dirpath, ec)) {
            if (ec.value() ==
                static_cast<int>(std::errc::directory_not_empty)) {
                return FileOperationResult::DIRECTORY_NOT_EMPTY;
            }
            return system_error_to_file_operation_result(ec);
        }
    }

    return FileOperationResult::SUCCESS;
}

std::pair<std::vector<fenris::FileInfo>, FileOperationResult>
list_directory(const std::string &dirpath)
{
    std::vector<std::string> entries;
    std::error_code ec;
    std::vector<fenris::FileInfo> file_info_list;

    if (!fs::exists(dirpath, ec)) {
        return {file_info_list, FileOperationResult::FILE_NOT_FOUND};
    }

    if (!fs::is_directory(dirpath, ec)) {
        return {file_info_list, FileOperationResult::INVALID_PATH};
    }

    for (const auto &entry : fs::directory_iterator(dirpath, ec)) {
        entries.push_back(entry.path().filename().string());
    }

    for (const auto &entry : entries) {
        std::string full_path = (fs::path(dirpath) / entry).string();
        auto [file_info, result] = get_file_info(full_path);
        if (result != FileOperationResult::SUCCESS) {
            return {file_info_list, result};
        }
        file_info_list.push_back(file_info);
    }

    return {file_info_list, FileOperationResult::SUCCESS};
}

FileOperationResult change_directory(const std::string &dirpath)
{
    std::error_code ec;

    if (!fs::exists(dirpath, ec)) {
        return FileOperationResult::FILE_NOT_FOUND;
    }

    if (!fs::is_directory(dirpath, ec)) {
        return FileOperationResult::INVALID_PATH;
    }

    fs::current_path(dirpath, ec);
    if (ec) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

std::pair<std::string, FileOperationResult> get_current_directory()
{
    std::error_code ec;
    fs::path current_path = fs::current_path(ec);

    if (ec) {
        return {"", system_error_to_file_operation_result(ec)};
    }

    return {current_path.string(), FileOperationResult::SUCCESS};
}

FileOperationResult rename_path(const std::string &oldpath,
                                const std::string &newpath)
{
    std::error_code ec;

    if (!fs::exists(oldpath, ec)) {
        return FileOperationResult::FILE_NOT_FOUND;
    }

    if (fs::exists(newpath, ec)) {
        return FileOperationResult::FILE_ALREADY_EXISTS;
    }

    fs::rename(oldpath, newpath, ec);
    if (ec) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

FileOperationResult copy_file(const std::string &source,
                              const std::string &destination)
{
    std::error_code ec;

    if (!fs::exists(source, ec) || !fs::is_regular_file(source, ec)) {
        return FileOperationResult::FILE_NOT_FOUND;
    }

    fs::copy_file(source,
                  destination,
                  fs::copy_options::overwrite_existing,
                  ec);
    if (ec) {
        return system_error_to_file_operation_result(ec);
    }

    return FileOperationResult::SUCCESS;
}

std::pair<uintmax_t, FileOperationResult>
get_file_size(const std::string &filepath)
{
    std::error_code ec;

    if (!fs::exists(filepath, ec)) {
        return {0, FileOperationResult::FILE_NOT_FOUND};
    }

    if (!fs::is_regular_file(filepath, ec)) {
        return {0, FileOperationResult::INVALID_PATH};
    }

    uintmax_t size = fs::file_size(filepath, ec);
    if (ec) {
        return {0, system_error_to_file_operation_result(ec)};
    }

    return {size, FileOperationResult::SUCCESS};
}

} // namespace common
} // namespace fenris
