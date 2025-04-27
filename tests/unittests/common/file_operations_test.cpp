#include "common/file_operations.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
namespace fenris {
namespace common {
namespace tests {

// Fixture class for file operations tests
class FileOperationsTest : public ::testing::Test {
  protected:
    // Set up temporary directory for tests
    void SetUp() override
    {
        test_dir = fs::temp_directory_path() / "fenris_test_dir";

        // Remove directory if it exists (cleanup from previous failed tests)
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }

        fs::create_directory(test_dir);
        original_dir = fs::current_path();
    }

    // Clean up temporary files after tests
    void TearDown() override
    {
        // Return to original directory
        fs::current_path(original_dir);

        // Remove test directory and contents
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    // Helper method to create a file with content
    void create_test_file(const std::string &filename,
                          const std::string &content)
    {
        std::string filepath = (test_dir / filename).string();
        write_file(filepath, content);
    }

    fs::path test_dir;     // Test directory path
    fs::path original_dir; // Original working directory
};

// Test creating a file
TEST_F(FileOperationsTest, CreateFile)
{
    std::string filename = "test_create.txt";
    std::string filepath = (test_dir / filename).string();

    FileOperationResult result = create_file(filepath);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_TRUE(fs::exists(filepath));

    // Try to create the same file again (should fail)
    result = create_file(filepath);
    EXPECT_EQ(result, FileOperationResult::FILE_ALREADY_EXISTS);
}

// Test reading a file
TEST_F(FileOperationsTest, ReadFile)
{
    std::string filename = "test_read.txt";
    std::string test_content = "Hello, World! This is a test file.";

    // Create test file
    create_test_file(filename, test_content);

    std::string filepath = (test_dir / filename).string();

    auto [content, error] = read_file(filepath);
    EXPECT_EQ(error, FileOperationResult::SUCCESS);
    std::string content_str(content.begin(), content.end());
    EXPECT_EQ(content_str, test_content);

    // Try to read a non-existent file
    auto [invalid_content, invalid_error] =
        read_file(filepath + ".nonexistent");
    EXPECT_EQ(invalid_error, FileOperationResult::FILE_NOT_FOUND);
    EXPECT_TRUE(invalid_content.empty());
}

// Test writing to a file
TEST_F(FileOperationsTest, WriteFile)
{
    std::string filename = "test_write.txt";
    std::string filepath = (test_dir / filename).string();
    std::string test_content = "This is test content for writing to a file.";

    FileOperationResult write_result = write_file(filepath, test_content);
    EXPECT_EQ(write_result, FileOperationResult::SUCCESS);

    auto [read_content, read_error] = read_file(filepath);
    EXPECT_EQ(read_error, FileOperationResult::SUCCESS);
    std::string read_content_str(read_content.begin(), read_content.end());
    EXPECT_EQ(read_content_str, test_content);

    // Test overwriting existing file
    std::string new_content =
        "This is new content that overwrites the old content.";

    write_result = write_file(filepath, new_content);
    EXPECT_EQ(write_result, FileOperationResult::SUCCESS);

    // Read back and verify new content
    auto [updated_content, updated_error] = read_file(filepath);
    EXPECT_EQ(updated_error, FileOperationResult::SUCCESS);
    std::string updated_content_str(updated_content.begin(),
                                    updated_content.end());
    EXPECT_EQ(updated_content_str, new_content);
}

// Test appending to a file
TEST_F(FileOperationsTest, AppendFile)
{
    std::string filename = "test_append.txt";
    std::string filepath = (test_dir / filename).string();

    std::string initial_content = "Initial content. ";
    std::string append_content = "Appended content.";

    // Write initial content
    write_file(filepath, initial_content);

    // Append content
    FileOperationResult append_result = append_file(filepath, append_content);
    EXPECT_EQ(append_result, FileOperationResult::SUCCESS);

    auto [content, error] = read_file(filepath);
    EXPECT_EQ(error, FileOperationResult::SUCCESS);
    std::string expected = initial_content + append_content;
    std::string actual(content.begin(), content.end());
    EXPECT_EQ(actual, expected);

    // Test appending to non-existent file (should fail with FILE_NOT_FOUND)
    std::string new_file = (test_dir / "nonexistent_append.txt").string();
    append_result = append_file(new_file, append_content);
    EXPECT_EQ(append_result, FileOperationResult::FILE_NOT_FOUND);
}

// Test deleting a file
TEST_F(FileOperationsTest, DeleteFile)
{
    std::string filename = "test_delete.txt";
    std::string filepath = (test_dir / filename).string();

    create_test_file(filename, "Test file for deletion");
    EXPECT_TRUE(fs::exists(filepath));

    FileOperationResult delete_result = delete_file(filepath);
    EXPECT_EQ(delete_result, FileOperationResult::SUCCESS);
    EXPECT_FALSE(fs::exists(filepath));

    // Try to delete non-existent file
    delete_result = delete_file(filepath);
    EXPECT_EQ(delete_result, FileOperationResult::FILE_NOT_FOUND);

    // Try to delete a directory (should fail)
    delete_result = delete_file(test_dir.string());
    EXPECT_EQ(delete_result, FileOperationResult::INVALID_PATH);
}

// Test file_exists function
TEST_F(FileOperationsTest, FileExists)
{
    std::string filename = "test_exists.txt";
    std::string filepath = (test_dir / filename).string();

    // File should not exist initially
    EXPECT_FALSE(file_exists(filepath));

    create_test_file(filename, "Test file for existence check");
    EXPECT_TRUE(file_exists(filepath));
    EXPECT_TRUE(file_exists(test_dir.string()));
}

// Test getting file info
TEST_F(FileOperationsTest, GetFileInfo)
{
    std::string filename = "test_info.txt";
    std::string filepath = (test_dir / filename).string();

    create_test_file(filename, "Test file for info check");

    auto [file_info, error] = get_file_info(filepath);
    EXPECT_EQ(error, FileOperationResult::SUCCESS);
    EXPECT_EQ(file_info.name(), filepath);
    EXPECT_FALSE(file_info.is_directory());
    EXPECT_EQ(file_info.size(), 24); // "Test file for info check" length

    // Get info for directory
    auto [dir_info, dir_error] = get_file_info(test_dir.string());
    EXPECT_EQ(dir_error, FileOperationResult::SUCCESS);
    EXPECT_EQ(dir_info.name(), test_dir.string());
    EXPECT_TRUE(dir_info.is_directory());

    // Get info for non-existent file
    auto [invalid_info, invalid_error] =
        get_file_info(filepath + ".nonexistent");
    EXPECT_EQ(invalid_error, FileOperationResult::FILE_NOT_FOUND);
}

// Test creating directories
TEST_F(FileOperationsTest, CreateDirectory)
{
    std::string dirname = "test_dir";
    std::string dirpath = (test_dir / dirname).string();

    FileOperationResult result = create_directory(dirpath);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_TRUE(fs::exists(dirpath));
    EXPECT_TRUE(fs::is_directory(dirpath));

    // Try to create the same directory again
    result = create_directory(dirpath);
    EXPECT_EQ(result, FileOperationResult::DIRECTORY_ALREADY_EXISTS);

    // Try to create a directory where a file exists
    std::string filename = "test_file_not_dir";
    std::string filepath = (test_dir / filename).string();
    create_test_file(filename, "This is a file, not a directory");

    result = create_directory(filepath);
    EXPECT_EQ(result, FileOperationResult::INVALID_PATH);
}

// Test creating nested directories
TEST_F(FileOperationsTest, CreateDirectories)
{
    std::string nested_path = "nested/path/to/create";
    std::string dirpath = (test_dir / nested_path).string();

    FileOperationResult result = create_directories(dirpath);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_TRUE(fs::exists(dirpath));
    EXPECT_TRUE(fs::is_directory(dirpath));

    // Creating them again should still succeed
    result = create_directories(dirpath);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
}

// Test deleting directories
TEST_F(FileOperationsTest, DeleteDirectory)
{
    // Create test directory
    std::string dirname = "dir_to_delete";
    std::string dirpath = (test_dir / dirname).string();
    fs::create_directory(dirpath);
    EXPECT_TRUE(fs::exists(dirpath));

    // Delete directory
    FileOperationResult result = delete_directory(dirpath);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_FALSE(fs::exists(dirpath));

    // Create directory with contents
    fs::create_directory(dirpath);
    create_test_file((fs::path(dirpath) / "file.txt").string(),
                     "Test file in directory");

    // Try to delete non-empty directory (should fail)
    result = delete_directory(dirpath, false);
    EXPECT_EQ(result, FileOperationResult::DIRECTORY_NOT_EMPTY);

    // Delete with recursive flag
    result = delete_directory(dirpath, true);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_FALSE(fs::exists(dirpath));

    // Try to delete non-existent directory
    result = delete_directory(dirpath);
    EXPECT_EQ(result, FileOperationResult::FILE_NOT_FOUND);

    // Try to delete a file as a directory
    std::string filepath = (test_dir / "not_a_dir.txt").string();
    create_test_file("not_a_dir.txt", "This is a file, not a directory");
    result = delete_directory(filepath);
    EXPECT_EQ(result, FileOperationResult::INVALID_PATH);
}

// Test listing directory contents
TEST_F(FileOperationsTest, ListDirectory)
{
    // Create test files and subdirectories
    create_test_file("file1.txt", "File 1 content");
    create_test_file("file2.txt", "File 2 content");
    fs::create_directory(test_dir / "subdir1");
    fs::create_directory(test_dir / "subdir2");

    // List directory contents
    auto [file_infos, error] = list_directory(test_dir.string());
    EXPECT_EQ(error, FileOperationResult::SUCCESS);
    EXPECT_EQ(file_infos.size(), 4);

    // Verify entries by checking names in the FileInfo objects
    std::vector<std::string> file_names;
    for (const auto &info : file_infos) {
        file_names.push_back(fs::path(info.name()).filename().string());
    }

    EXPECT_TRUE(std::find(file_names.begin(), file_names.end(), "file1.txt") !=
                file_names.end());
    EXPECT_TRUE(std::find(file_names.begin(), file_names.end(), "file2.txt") !=
                file_names.end());
    EXPECT_TRUE(std::find(file_names.begin(), file_names.end(), "subdir1") !=
                file_names.end());
    EXPECT_TRUE(std::find(file_names.begin(), file_names.end(), "subdir2") !=
                file_names.end());

    // Check that directory flag is correctly set
    for (const auto &info : file_infos) {
        std::string name = fs::path(info.name()).filename().string();
        if (name == "subdir1" || name == "subdir2") {
            EXPECT_TRUE(info.is_directory());
        } else {
            EXPECT_FALSE(info.is_directory());
        }
    }

    // List contents of non-existent directory
    auto [invalid_infos, invalid_error] =
        list_directory((test_dir / "nonexistent").string());
    EXPECT_EQ(invalid_error, FileOperationResult::FILE_NOT_FOUND);
    EXPECT_TRUE(invalid_infos.empty());

    // List a file as a directory
    auto [file_infos_error, file_error] =
        list_directory((test_dir / "file1.txt").string());
    EXPECT_EQ(file_error, FileOperationResult::INVALID_PATH);
    EXPECT_TRUE(file_infos_error.empty());
}

// Test changing directory
TEST_F(FileOperationsTest, ChangeDirectory)
{
    // Create a subdirectory
    std::string dirname = "change_dir_test";
    std::string dirpath = (test_dir / dirname).string();
    fs::create_directory(dirpath);

    auto [initial_dir, initial_error] = get_current_directory();
    EXPECT_EQ(initial_error, FileOperationResult::SUCCESS);

    FileOperationResult change_result = change_directory(dirpath);
    EXPECT_EQ(change_result, FileOperationResult::SUCCESS);

    auto [new_dir, new_error] = get_current_directory();
    EXPECT_EQ(new_error, FileOperationResult::SUCCESS);

    EXPECT_NE(initial_dir, new_dir);
    EXPECT_EQ(fs::path(new_dir), fs::path(dirpath));

    change_result = change_directory(test_dir.string());
    EXPECT_EQ(change_result, FileOperationResult::SUCCESS);

    change_result = change_directory((test_dir / "nonexistent").string());
    EXPECT_EQ(change_result, FileOperationResult::FILE_NOT_FOUND);

    std::string filepath = (test_dir / "not_a_dir.txt").string();
    create_test_file("not_a_dir.txt", "This is a file, not a directory");
    change_result = change_directory(filepath);
    EXPECT_EQ(change_result, FileOperationResult::INVALID_PATH);
}

// Test renaming files
TEST_F(FileOperationsTest, RenamePath)
{
    // Create a test file
    std::string old_name = "old_name.txt";
    std::string new_name = "new_name.txt";
    std::string old_path = (test_dir / old_name).string();
    std::string new_path = (test_dir / new_name).string();

    create_test_file(old_name, "File for renaming test");

    FileOperationResult result = rename_path(old_path, new_path);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_FALSE(fs::exists(old_path));
    EXPECT_TRUE(fs::exists(new_path));

    // Try to rename non-existent file
    result = rename_path((test_dir / "nonexistent.txt").string(), new_path);
    EXPECT_EQ(result, FileOperationResult::FILE_NOT_FOUND);

    // Try to rename to existing file
    std::string another_file = (test_dir / "another_file.txt").string();
    create_test_file("another_file.txt", "Another test file");

    result = rename_path(another_file, new_path);
    EXPECT_EQ(result, FileOperationResult::FILE_ALREADY_EXISTS);
}

// Test copying files
TEST_F(FileOperationsTest, CopyFile)
{
    // Create source file
    std::string source_name = "source.txt";
    std::string dest_name = "destination.txt";
    std::string source_path = (test_dir / source_name).string();
    std::string dest_path = (test_dir / dest_name).string();

    create_test_file(source_name, "File content for copy test");

    FileOperationResult result = copy_file(source_path, dest_path);
    EXPECT_EQ(result, FileOperationResult::SUCCESS);
    EXPECT_TRUE(fs::exists(dest_path));
    EXPECT_TRUE(fs::exists(source_path));

    // Try to copy non-existent file
    result = copy_file((test_dir / "nonexistent.txt").string(), dest_path);
    EXPECT_EQ(result, FileOperationResult::FILE_NOT_FOUND);

    // Try to copy a directory (should fail)
    std::string dir_path = (test_dir / "test_dir").string();
    fs::create_directory(dir_path);
    result = copy_file(dir_path, (test_dir / "dir_copy").string());
    EXPECT_EQ(result, FileOperationResult::FILE_NOT_FOUND);
}

// Test getting file size
TEST_F(FileOperationsTest, GetFileSize)
{
    std::string filename = "test_size.txt";
    std::string filepath = (test_dir / filename).string();
    std::string content = "This file has a specific size for testing.";

    create_test_file(filename, content);

    auto [size, error] = get_file_size(filepath);

    EXPECT_EQ(error, FileOperationResult::SUCCESS);
    EXPECT_EQ(size, content.size());

    // Try to get size of non-existent file
    auto [invalid_size, invalid_error] =
        get_file_size((test_dir / "nonexistent.txt").string());
    EXPECT_EQ(invalid_error, FileOperationResult::FILE_NOT_FOUND);
    EXPECT_EQ(invalid_size, 0);

    // Try to get size of a directory
    auto [dir_size, dir_error] = get_file_size(test_dir.string());
    EXPECT_EQ(dir_error, FileOperationResult::INVALID_PATH);
    EXPECT_EQ(dir_size, 0);
}

// Test permission errors
TEST_F(FileOperationsTest, PermissionErrors)
{
    // Skip test if not running as regular user (permissions won't work the same
    // if running as root)
    if (geteuid() == 0) {
        GTEST_SKIP() << "Skipping permission tests when running as root";
    }

    // Create test files and directories
    std::string filename = "test_perm_file.txt";
    std::string filepath = (test_dir / filename).string();
    std::string dirname = "test_perm_dir";
    std::string dirpath = (test_dir / dirname).string();

    create_test_file(filename, "File for permission test");
    fs::create_directory(dirpath);

    // Test write permissions by first ensuring we can write to the file
    std::string test_content = "Testing permissions";
    FileOperationResult initial_write = write_file(filepath, test_content);
    EXPECT_EQ(initial_write, FileOperationResult::SUCCESS);

    // Set restrictive permissions (read-only)
    fs::permissions(filepath,
                    fs::perms::owner_read | fs::perms::group_read,
                    fs::perm_options::replace);

    // Try to write to read-only file
    std::string new_content = "Try to write to read-only file";
    FileOperationResult write_result = write_file(filepath, new_content);
    EXPECT_EQ(write_result, FileOperationResult::PERMISSION_DENIED);

    // Try to append to read-only file
    FileOperationResult append_result = append_file(filepath, new_content);
    EXPECT_EQ(append_result, FileOperationResult::PERMISSION_DENIED);

    // Set directory to read-only
    fs::permissions(dirpath,
                    fs::perms::owner_read | fs::perms::group_read,
                    fs::perm_options::replace);

    // Try to create file in read-only directory
    std::string nested_file = (fs::path(dirpath) / "new_file.txt").string();
    FileOperationResult create_result = create_file(nested_file);
    EXPECT_EQ(create_result, FileOperationResult::PERMISSION_DENIED);

    // Try to create directory in read-only directory
    std::string nested_dir = (fs::path(dirpath) / "new_dir").string();
    FileOperationResult mkdir_result = create_directory(nested_dir);
    EXPECT_EQ(mkdir_result, FileOperationResult::PERMISSION_DENIED);

    fs::permissions(filepath, fs::perms::owner_all, fs::perm_options::add);
    fs::permissions(dirpath, fs::perms::owner_all, fs::perm_options::add);
}

// Test getting current directory
TEST_F(FileOperationsTest, GetCurrentDirectory)
{
    auto [current_dir, error] = get_current_directory();
    EXPECT_EQ(error, FileOperationResult::SUCCESS);
    EXPECT_EQ(current_dir, original_dir.string());

    change_directory(test_dir.string());
    auto [new_dir, new_error] = get_current_directory();
    EXPECT_EQ(new_error, FileOperationResult::SUCCESS);
    EXPECT_EQ(new_dir, test_dir.string());
}

} // namespace tests
} // namespace common
} // namespace fenris
