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
namespace common {

/**
 * Result of file operations
 */
enum class FileOperationResult {
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
 * Convert FileOperationResult to string representation
 *
 * @param result FileOperationResult to convert
 * @return String representation of the result
 */
std::string file_operation_result_to_string(FileOperationResult result);

/**
 * Read data from a file
 *
 * @param filepath Path to the file to read
 * @return Pair of (file content as vector of bytes, FileOperationResult)
 */
std::pair<std::vector<uint8_t>, FileOperationResult>
read_file(const std::string &filepath);

/**
 * Write data to a file (creates the file if it doesn't exist, otherwise
 * overwrites)
 *
 * @param filepath Path to the file to write
 * @param data Data to write to the file
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult write_file(const std::string &filepath,
                               const std::vector<uint8_t> &data);

/**
 * Append data to a file (the file must exist)
 *
 * @param filepath Path to the file to append to
 * @param data Data to append to the file
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult append_file(const std::string &filepath,
                                const std::vector<uint8_t> &data);

/**
 * Create a new empty file
 *
 * @param filepath Path of the file to create
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult create_file(const std::string &filepath);

/**
 * Delete a file
 *
 * @param filepath Path of the file to delete
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult delete_file(const std::string &filepath);

/**
 * Get file information (size, modification time, etc.)
 *
 * @param filepath Path to the file
 * @return Pair of (filesystem::file_status, FileOperationResult)
 */
std::pair<std::filesystem::file_status, FileOperationResult>
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
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult create_directory(const std::string &dirpath);

/**
 * Create a directory and all parent directories if they don't exist
 *
 * @param dirpath Path of the directory to create
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult create_directories(const std::string &dirpath);

/**
 * Delete a directory
 *
 * @param dirpath Path of the directory to delete
 * @param recursive If true, delete all contents recursively; if false, fail if
 * directory is not empty
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult delete_directory(const std::string &dirpath,
                                     bool recursive = false);

/**
 * List contents of a directory
 *
 * @param dirpath Path of the directory to list
 * @return Pair of (vector of entry names, FileOperationResult)
 */
std::pair<std::vector<std::string>, FileOperationResult>
list_directory(const std::string &dirpath);

/**
 * Change the current working directory
 *
 * @param dirpath Path to change to
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult change_directory(const std::string &dirpath);

/**
 * Get the current working directory
 *
 * @return Pair of (current working directory path, FileOperationResult)
 */
std::pair<std::string, FileOperationResult> get_current_directory();

/**
 * Rename a file or directory
 *
 * @param oldpath Current path
 * @param newpath New path
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult rename_path(const std::string &oldpath,
                                const std::string &newpath);

/**
 * Copy a file
 *
 * @param source Source file path
 * @param destination Destination file path
 * @return FileOperationResult indicating success or failure
 */
FileOperationResult copy_file(const std::string &source,
                              const std::string &destination);

/**
 * Get file size
 *
 * @param filepath Path to the file
 * @return Pair of (file size in bytes, FileOperationResult)
 */
std::pair<uintmax_t, FileOperationResult>
get_file_size(const std::string &filepath);

/**
 * Convert system_error to FileOperationResult
 *
 * @param ec System error code
 * @return Equivalent FileOperationResult
 */
FileOperationResult
system_error_to_file_operation_result(const std::error_code &ec);

} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_FILE_OPERATIONS_HPP
