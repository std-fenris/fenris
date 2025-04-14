#ifndef FENRIS_COMMON_FILE_OPERATIONS_HPP
#define FENRIS_COMMON_FILE_OPERATIONS_HPP

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fenris {
namespace server {

/**
 * Error codes for file operations
 */
enum class FileError {
    SUCCESS = 0,
    FILE_NOT_FOUND,
    PERMISSION_DENIED,
    PATH_NOT_EXIST,
    FILE_ALREADY_EXISTS,
    DIRECTORY_NOT_EMPTY,
    IO_ERROR,
    INVALID_PATH,
    DIRECTORY_ALREADY_EXISTS,
    UNKNOWN_ERROR
};

/**
 * Read data from a file
 *
 * @param filepath Path to the file to read
 * @return Pair of (file content as vector of bytes, error code)
 */
std::pair<std::vector<uint8_t>, FileError>
read_file(const std::string &filepath);

/**
 * Write data to a file (creates the file if it doesn't exist, otherwise
 * overwrites)
 *
 * @param filepath Path to the file to write
 * @param data Data to write to the file
 * @return Error code indicating success or failure
 */
FileError write_file(const std::string &filepath,
                     const std::vector<uint8_t> &data);

/**
 * Append data to a file (the file must exist)
 *
 * @param filepath Path to the file to append to
 * @param data Data to append to the file
 * @return Error code indicating success or failure
 */
FileError append_file(const std::string &filepath,
                      const std::vector<uint8_t> &data);

/**
 * Create a new empty file
 *
 * @param filepath Path of the file to create
 * @return Error code indicating success or failure
 */
FileError create_file(const std::string &filepath);

/**
 * Delete a file
 *
 * @param filepath Path of the file to delete
 * @return Error code indicating success or failure
 */
FileError delete_file(const std::string &filepath);

/**
 * Get file information (size, modification time, etc.)
 *
 * @param filepath Path to the file
 * @return Pair of (filesystem::file_status, error code)
 */
std::pair<std::filesystem::file_status, FileError>
get_file_info(const std::string &filepath);

/**
 * Check if a file exists
 *
 * @param filepath Path to check
 * @return True if the file exists, false otherwise
 */
bool file_exists(const std::string &filepath);

/**
 * Create a directory
 *
 * @param dirpath Path of the directory to create
 * @return Error code indicating success or failure
 */
FileError create_directory(const std::string &dirpath);

/**
 * Create a directory and all parent directories if they don't exist
 *
 * @param dirpath Path of the directory to create
 * @return Error code indicating success or failure
 */
FileError create_directories(const std::string &dirpath);

/**
 * Delete a directory
 *
 * @param dirpath Path of the directory to delete
 * @param recursive If true, delete all contents recursively; if false, fail if
 * directory is not empty
 * @return Error code indicating success or failure
 */
FileError delete_directory(const std::string &dirpath, bool recursive = false);

/**
 * List contents of a directory
 *
 * @param dirpath Path of the directory to list
 * @return Pair of (vector of entry names, error code)
 */
std::pair<std::vector<std::string>, FileError>
list_directory(const std::string &dirpath);

/**
 * Change the current working directory
 *
 * @param dirpath Path to change to
 * @return Error code indicating success or failure
 */
FileError change_directory(const std::string &dirpath);

/**
 * Get the current working directory
 *
 * @return Pair of (current working directory path, error code)
 */
std::pair<std::string, FileError> get_current_directory();

/**
 * Rename a file or directory
 *
 * @param oldpath Current path
 * @param newpath New path
 * @return Error code indicating success or failure
 */
FileError rename_path(const std::string &oldpath, const std::string &newpath);

/**
 * Copy a file
 *
 * @param source Source file path
 * @param destination Destination file path
 * @return Error code indicating success or failure
 */
FileError copy_file(const std::string &source, const std::string &destination);

/**
 * Get file size
 *
 * @param filepath Path to the file
 * @return Pair of (file size in bytes, error code)
 */
std::pair<uintmax_t, FileError> get_file_size(const std::string &filepath);

/**
 * Convert system_error to FileError
 *
 * @param ec System error code
 * @return Equivalent FileError
 */
FileError system_error_to_file_error(const std::error_code &ec);

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_FILE_OPERATIONS_HPP
