#include "client/request_manager.hpp"
#include "fenris.pb.h"
#include "gtest/gtest.h"

#include <fstream>
#include <vector>
#include <string>
#include <optional>

namespace fenris {
namespace client {
namespace tests {

class RequestManagerTest : public ::testing::Test {
  protected:
    RequestManager request_manager;

    // Helper to create args vector
    std::vector<std::string> create_args(const std::vector<const char *> &args_cstr)
    {
        std::vector<std::string> args;
        for (const char *cstr : args_cstr) {
            args.push_back(std::string(cstr));
        }
        return args;
    }

    // Helper to create a temporary file with content
    std::string create_temp_file(const std::string &content)
    {
        char filename[] = "/tmp/fenris_test_XXXXXX";
        int fd = mkstemp(filename);
        if (fd == -1) {
            perror("mkstemp failed");
            return "";
        }
        write(fd, content.c_str(), content.size());
        close(fd);
        return std::string(filename);
    }
};

TEST_F(RequestManagerTest, GeneratePingRequest)
{
    auto args = create_args({"ping"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::PING);
}

TEST_F(RequestManagerTest, GenerateLsRequest)
{
    auto args_no_path = create_args({"ls"});
    auto request_opt_no_path = request_manager.generate_request(args_no_path);
    ASSERT_TRUE(request_opt_no_path.has_value());
    EXPECT_EQ(request_opt_no_path.value().command(),
              fenris::RequestType::LIST_DIR);
    EXPECT_EQ(request_opt_no_path.value().filename(), ".");

    auto args_with_path = create_args({"ls", "/some/dir"});
    auto request_opt_with_path =
        request_manager.generate_request(args_with_path);
    ASSERT_TRUE(request_opt_with_path.has_value());
    EXPECT_EQ(request_opt_with_path.value().command(),
              fenris::RequestType::LIST_DIR);
    EXPECT_EQ(request_opt_with_path.value().filename(), "/some/dir");
}

TEST_F(RequestManagerTest, GenerateReadFileRequest)
{
    auto args = create_args({"cat", "myfile.txt"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::READ_FILE);
    EXPECT_EQ(request_opt.value().filename(), "myfile.txt");
}

TEST_F(RequestManagerTest, GenerateWriteFileRequestInline)
{
    auto args = create_args({"write", "newfile.txt", "Hello World"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::WRITE_FILE);
    EXPECT_EQ(request_opt.value().filename(), "newfile.txt");
    EXPECT_EQ(request_opt.value().data(), "Hello World");
}

TEST_F(RequestManagerTest, GenerateWriteFileRequestFromFile)
{
    std::string content = "Content from file";
    std::string temp_filename = create_temp_file(content);
    ASSERT_FALSE(temp_filename.empty());

    auto args = create_args({"write", "target.txt", "-f", temp_filename.c_str()});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::WRITE_FILE);
    EXPECT_EQ(request_opt.value().filename(), "target.txt");
    EXPECT_EQ(request_opt.value().data(), content);

    unlink(temp_filename.c_str()); // Clean up temp file
}

TEST_F(RequestManagerTest, GenerateAppendFileRequestInline)
{
    auto args = create_args({"append", "logfile.log", "More data"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::APPEND_FILE);
    EXPECT_EQ(request_opt.value().filename(), "logfile.log");
    EXPECT_EQ(request_opt.value().data(), "More data");
}

TEST_F(RequestManagerTest, GenerateAppendFileRequestFromFile)
{
    std::string content = "Append this content";
    std::string temp_filename = create_temp_file(content);
    ASSERT_FALSE(temp_filename.empty());

    auto args =
        create_args({"append", "target.txt", "-f", temp_filename.c_str()});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::APPEND_FILE);
    EXPECT_EQ(request_opt.value().filename(), "target.txt");
    EXPECT_EQ(request_opt.value().data(), content);

    unlink(temp_filename.c_str()); // Clean up temp file
}

TEST_F(RequestManagerTest, GenerateDeleteFileRequest)
{
    auto args = create_args({"rm", "oldfile.bak"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::DELETE_FILE);
    EXPECT_EQ(request_opt.value().filename(), "oldfile.bak");
}

TEST_F(RequestManagerTest, GenerateInfoFileRequest)
{
    auto args = create_args({"info", "details.txt"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::INFO_FILE);
    EXPECT_EQ(request_opt.value().filename(), "details.txt");
}

TEST_F(RequestManagerTest, GenerateCreateDirRequest)
{
    auto args = create_args({"mkdir", "new_directory"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::CREATE_DIR);
    EXPECT_EQ(request_opt.value().filename(), "new_directory");
}

TEST_F(RequestManagerTest, GenerateChangeDirRequest)
{
    auto args = create_args({"cd", "../parent"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::CHANGE_DIR);
    EXPECT_EQ(request_opt.value().filename(), "../parent");
}

TEST_F(RequestManagerTest, GenerateDeleteDirRequest)
{
    auto args = create_args({"rmdir", "empty_dir"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::DELETE_DIR);
    EXPECT_EQ(request_opt.value().filename(), "empty_dir");
}

TEST_F(RequestManagerTest, GenerateTerminateRequest)
{
    auto args = create_args({"terminate"}); // Assuming 'terminate' is a valid command
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::TERMINATE);
}

TEST_F(RequestManagerTest, InvalidCommand)
{
    auto args = create_args({"invalid_command", "arg1"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_FALSE(request_opt.has_value());
}

TEST_F(RequestManagerTest, MissingArguments)
{
    auto args_cd = create_args({"cd"}); // Missing directory
    ASSERT_FALSE(request_manager.generate_request(args_cd).has_value());

    auto args_cat = create_args({"cat"}); // Missing filename
    ASSERT_FALSE(request_manager.generate_request(args_cat).has_value());

    auto args_write = create_args({"write", "file"}); // Missing content
    ASSERT_FALSE(request_manager.generate_request(args_write).has_value());

    auto args_append = create_args({"append", "file"}); // Missing content
    ASSERT_FALSE(request_manager.generate_request(args_append).has_value());

    auto args_rm = create_args({"rm"}); // Missing filename
    ASSERT_FALSE(request_manager.generate_request(args_rm).has_value());

    auto args_info = create_args({"info"}); // Missing filename
    ASSERT_FALSE(request_manager.generate_request(args_info).has_value());

    auto args_mkdir = create_args({"mkdir"}); // Missing directory name
    ASSERT_FALSE(request_manager.generate_request(args_mkdir).has_value());

    auto args_rmdir = create_args({"rmdir"}); // Missing directory name
    ASSERT_FALSE(request_manager.generate_request(args_rmdir).has_value());
}

TEST_F(RequestManagerTest, WriteFileWithMultipleContentArgs)
{
    // Although the TUI validation might prevent this, test if RequestManager handles it
    auto args = create_args({"write", "myfile.txt", "part1", "part2"});
    auto request_opt = request_manager.generate_request(args);
    ASSERT_TRUE(request_opt.has_value());
    EXPECT_EQ(request_opt.value().command(), fenris::RequestType::WRITE_FILE);
    EXPECT_EQ(request_opt.value().filename(), "myfile.txt");
    // RequestManager currently takes only the first content arg if not using -f
    EXPECT_EQ(request_opt.value().data(), "part1 part2");
}


} // namespace tests
} // namespace client
} // namespace fenris
