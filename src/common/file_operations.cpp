#include "common/file_operations.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

namespace fenris {
namespace common {

FileError system_error_to_file_error(const std::error_code &ec)
{
    if (!ec) {
        return FileError::SUCCESS;
    }

    switch (static_cast<std::errc>(ec.value())) {
    case std::errc::no_such_file_or_directory:
        return FileError::FILE_NOT_FOUND;

    case std::errc::permission_denied:
        return FileError::PERMISSION_DENIED;

    case std::errc::file_exists:
        return FileError::FILE_ALREADY_EXISTS;

    case std::errc::directory_not_empty:
        return FileError::DIRECTORY_NOT_EMPTY;

    case std::errc::invalid_argument:
    case std::errc::filename_too_long:
        return FileError::INVALID_PATH;

    case std::errc::io_error:
        return FileError::IO_ERROR;

    default:
        return FileError::UNKNOWN_ERROR;
    }
}

std::pair<std::vector<uint8_t>, FileError>
read_file(const std::string &filepath)
{
    std::vector<uint8_t> content;
    std::error_code ec;
    if (!fs::exists(filepath, ec)) {
        return {content, FileError::FILE_NOT_FOUND};
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return {content, FileError::IO_ERROR};
    }

    // Get file size
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    content.resize(static_cast<size_t>(file_size));

    if (!file.read(reinterpret_cast<char *>(content.data()), file_size)) {
        return {std::vector<uint8_t>(), FileError::IO_ERROR};
    }

    return {content, FileError::SUCCESS};
}

FileError write_file(const std::string &filepath,
                     const std::vector<uint8_t> &data)
{
    std::error_code ec;

    // Check if file exists and is writable
    if (fs::exists(filepath, ec)) {
        fs::file_status file_status = fs::status(filepath, ec);
        if (ec) {
            return system_error_to_file_error(ec);
        }

        if ((file_status.permissions() & fs::perms::owner_write) ==
            fs::perms::none) {
            return FileError::PERMISSION_DENIED;
        }
    } else {
        // Check if parent directory exists and is writable
        fs::path parent_path = fs::path(filepath).parent_path();
        if (!parent_path.empty() && fs::exists(parent_path, ec)) {
            fs::file_status parent_status = fs::status(parent_path, ec);
            if (ec) {
                return system_error_to_file_error(ec);
            }

            if ((parent_status.permissions() & fs::perms::owner_write) ==
                fs::perms::none) {
                return FileError::PERMISSION_DENIED;
            }
        }
    }

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (errno == EACCES || errno == EPERM) {
            return FileError::PERMISSION_DENIED;
        }
        return FileError::IO_ERROR;
    }

    if (!file.write(reinterpret_cast<const char *>(data.data()), data.size())) {
        return FileError::IO_ERROR;
    }

    return FileError::SUCCESS;
}

FileError append_file(const std::string &filepath,
                      const std::vector<uint8_t> &data)
{
    std::error_code ec;

    // Check if file exists
    if (!fs::exists(filepath, ec)) {
        return FileError::FILE_NOT_FOUND;
    }

    // Check file permissions
    fs::file_status file_status = fs::status(filepath, ec);
    if (ec) {
        return system_error_to_file_error(ec);
    }

    // Check write permissions on the file
    if ((file_status.permissions() & fs::perms::owner_write) ==
        fs::perms::none) {
        return FileError::PERMISSION_DENIED;
    }

    // Open file in binary mode with append flag
    std::ofstream file(filepath, std::ios::binary | std::ios::app);
    if (!file) {
        // Check specifically for permission errors
        if (errno == EACCES || errno == EPERM) {
            return FileError::PERMISSION_DENIED;
        }
        return FileError::IO_ERROR;
    }

    if (!file.write(reinterpret_cast<const char *>(data.data()), data.size())) {
        return FileError::IO_ERROR;
    }

    return FileError::SUCCESS;
}

FileError create_file(const std::string &filepath)
{
    std::error_code ec;

    if (fs::exists(filepath, ec)) {
        return FileError::FILE_ALREADY_EXISTS;
    }

    // Check if parent directory exists and is writable
    fs::path parent_path = fs::path(filepath).parent_path();
    if (!parent_path.empty()) {
        if (!fs::exists(parent_path, ec)) {
            return FileError::FILE_NOT_FOUND;
        }

        // Check write permissions on parent directory
        fs::file_status parent_status = fs::status(parent_path, ec);
        if (ec) {
            return system_error_to_file_error(ec);
        }

        // If we can't write to the directory, return permission denied
        if ((parent_status.permissions() & fs::perms::owner_write) ==
            fs::perms::none) {
            return FileError::PERMISSION_DENIED;
        }
    }

    std::ofstream file(filepath);
    if (!file) {
        // Check specifically for permission errors
        if (errno == EACCES || errno == EPERM) {
            return FileError::PERMISSION_DENIED;
        }
        return FileError::IO_ERROR;
    }
    file.close();

    if (!fs::exists(filepath, ec)) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

FileError delete_file(const std::string &filepath)
{
    std::error_code ec;

    if (!fs::exists(filepath, ec)) {
        return FileError::FILE_NOT_FOUND;
    }

    if (!fs::is_regular_file(filepath, ec)) {
        return FileError::INVALID_PATH;
    }

    if (!fs::remove(filepath, ec)) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

std::pair<fs::file_status, FileError> get_file_info(const std::string &filepath)
{
    std::error_code ec;
    fs::file_status status = fs::status(filepath, ec);

    if (ec) {
        return {status, system_error_to_file_error(ec)};
    }

    return {status, FileError::SUCCESS};
}

bool file_exists(const std::string &filepath)
{
    std::error_code ec;
    return fs::exists(filepath, ec) && !ec;
}

FileError create_directory(const std::string &dirpath)
{
    std::error_code ec;

    if (fs::exists(dirpath, ec)) {
        if (fs::is_directory(dirpath, ec)) {
            return FileError::DIRECTORY_ALREADY_EXISTS;
        } else {
            // Path exists but is not a directory
            return FileError::INVALID_PATH;
        }
    }

    if (!fs::create_directory(dirpath, ec)) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

FileError create_directories(const std::string &dirpath)
{
    std::error_code ec;

    // Create the directory and all parent directories
    if (!fs::create_directories(dirpath, ec)) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

FileError delete_directory(const std::string &dirpath, bool recursive)
{
    std::error_code ec;

    if (!fs::exists(dirpath, ec)) {
        return system_error_to_file_error(
            ec.value()
                ? ec
                : std::make_error_code(std::errc::no_such_file_or_directory));
    }

    if (!fs::is_directory(dirpath, ec)) {
        return FileError::INVALID_PATH;
    }

    if (recursive) {
        fs::remove_all(dirpath, ec);
        if (ec) {
            return system_error_to_file_error(ec);
        }
    } else {
        // Try to remove the directory (will fail if not empty)
        if (!fs::remove(dirpath, ec)) {
            if (ec.value() ==
                static_cast<int>(std::errc::directory_not_empty)) {
                return FileError::DIRECTORY_NOT_EMPTY;
            }
            return system_error_to_file_error(ec);
        }
    }

    return FileError::SUCCESS;
}

std::pair<std::vector<std::string>, FileError>
list_directory(const std::string &dirpath)
{
    std::vector<std::string> entries;
    std::error_code ec;

    if (!fs::exists(dirpath, ec)) {
        return {entries, FileError::FILE_NOT_FOUND};
    }

    if (!fs::is_directory(dirpath, ec)) {
        return {entries, FileError::INVALID_PATH};
    }

    for (const auto &entry : fs::directory_iterator(dirpath, ec)) {
        entries.push_back(entry.path().filename().string());
    }

    if (ec) {
        return {entries, system_error_to_file_error(ec)};
    }

    return {entries, FileError::SUCCESS};
}

FileError change_directory(const std::string &dirpath)
{
    std::error_code ec;

    if (!fs::exists(dirpath, ec)) {
        return FileError::FILE_NOT_FOUND;
    }

    if (!fs::is_directory(dirpath, ec)) {
        return FileError::INVALID_PATH;
    }

    fs::current_path(dirpath, ec);
    if (ec) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

std::pair<std::string, FileError> get_current_directory()
{
    std::error_code ec;
    fs::path current_path = fs::current_path(ec);

    if (ec) {
        return {"", system_error_to_file_error(ec)};
    }

    return {current_path.string(), FileError::SUCCESS};
}

FileError rename_path(const std::string &oldpath, const std::string &newpath)
{
    std::error_code ec;

    if (!fs::exists(oldpath, ec)) {
        return FileError::FILE_NOT_FOUND;
    }

    if (fs::exists(newpath, ec)) {
        return FileError::FILE_ALREADY_EXISTS;
    }

    fs::rename(oldpath, newpath, ec);
    if (ec) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

FileError copy_file(const std::string &source, const std::string &destination)
{
    std::error_code ec;

    if (!fs::exists(source, ec) || !fs::is_regular_file(source, ec)) {
        return FileError::FILE_NOT_FOUND;
    }

    fs::copy_file(source,
                  destination,
                  fs::copy_options::overwrite_existing,
                  ec);
    if (ec) {
        return system_error_to_file_error(ec);
    }

    return FileError::SUCCESS;
}

std::pair<uintmax_t, FileError> get_file_size(const std::string &filepath)
{
    std::error_code ec;

    if (!fs::exists(filepath, ec)) {
        return {0, FileError::FILE_NOT_FOUND};
    }

    if (!fs::is_regular_file(filepath, ec)) {
        return {0, FileError::INVALID_PATH};
    }

    uintmax_t size = fs::file_size(filepath, ec);
    if (ec) {
        return {0, system_error_to_file_error(ec)};
    }

    return {size, FileError::SUCCESS};
}

} // namespace common
} // namespace fenris
