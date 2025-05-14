#include "client/colors.hpp"
#include "client/response_manager.hpp"
#include "fenris.pb.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

namespace fenris {
namespace client {
namespace tests {

class ResponseManagerTest : public ::testing::Test {
  protected:
    ResponseManager response_manager;

    void SetUp() override
    {
        // Disable colors for testing to ensure assertions work with plain text
        colors::disable_colors();
    }

    void TearDown() override
    {
        // Re-enable colors for normal usage
        colors::enable_colors();
    }
};

TEST_F(ResponseManagerTest, HandlePongResponse)
{
    fenris::Response response;
    response.set_success(true);
    response.set_type(fenris::ResponseType::PONG);
    response.set_data("Server OK");

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "Success");
    EXPECT_EQ(result[1], "Server is alive");
}

TEST_F(ResponseManagerTest, HandleSuccessResponse)
{
    fenris::Response response;
    response.set_success(true);
    response.set_type(fenris::ResponseType::SUCCESS);
    response.set_data("Directory created");

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "Success");
    EXPECT_EQ(result[1], "Directory created");
}

TEST_F(ResponseManagerTest, HandleErrorResponse)
{
    fenris::Response response;
    response.set_success(false);
    response.set_type(fenris::ResponseType::ERROR);
    response.set_error_message("File not found");

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "Error"); // Internal error indicator
    EXPECT_EQ(result[1], "Error: File not found");
}

TEST_F(ResponseManagerTest, HandleFileInfoResponse)
{
    fenris::Response response;
    response.set_success(true);
    response.set_type(fenris::ResponseType::FILE_INFO);

    auto *file_info = response.mutable_file_info();
    file_info->set_name("test.txt");
    file_info->set_size(1024);
    file_info->set_is_directory(false);
    file_info->set_permissions(0644);         // rw-r--r--
    file_info->set_modified_time(1678886400); // Example timestamp

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(),
              6); // Success + 5 lines (Name, Size, Type, Perms, Modified)
    EXPECT_EQ(result[0], "Success");
    EXPECT_NE(result[1].find("test.txt"), std::string::npos);
    EXPECT_NE(result[2].find("1.00 KB"),
              std::string::npos); // Check for formatted size
    EXPECT_NE(result[3].find("Modified:"),
              std::string::npos); // Check timestamp line start
    EXPECT_NE(result[4].find("File"), std::string::npos); // Check for type
    EXPECT_NE(result[5].find("rw-r--r--"),
              std::string::npos); // Check for permissions string
    EXPECT_NE(result[5].find("644"),
              std::string::npos); // Check for octal permissions
}

TEST_F(ResponseManagerTest, HandleFileContentResponseText)
{
    fenris::Response response;
    response.set_success(true);
    response.set_type(fenris::ResponseType::FILE_CONTENT);
    response.set_data("Line 1\nLine 2\nAnother line");

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 4); // Success + 3 lines
    EXPECT_EQ(result[0], "Success");
    EXPECT_EQ(result[1], "Line 1");
    EXPECT_EQ(result[2], "Line 2");
    EXPECT_EQ(result[3], "Another line");
}

TEST_F(ResponseManagerTest, HandleFileContentResponseBinary)
{
    fenris::Response response;
    response.set_success(true);
    response.set_type(fenris::ResponseType::FILE_CONTENT);
    response.set_data(std::string("Some\0Binary\tData", 16));

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 2); // Success + binary data message
    EXPECT_EQ(result[0], "Success");
    // Check for the specific binary data message format
    EXPECT_EQ(result[1], "(Binary data, 16 B)");
}

TEST_F(ResponseManagerTest, HandleDirectoryListingResponse)
{
    fenris::Response response;
    response.set_success(true);
    response.set_type(fenris::ResponseType::DIR_LISTING);

    auto *listing = response.mutable_directory_listing();

    auto *entry1 = listing->add_entries();
    entry1->set_name("file.txt");
    entry1->set_size(500);
    entry1->set_is_directory(false);
    entry1->set_modified_time(1678886400);
    entry1->set_permissions(0644); // Example permissions

    auto *entry2 = listing->add_entries();
    entry2->set_name(
        "subdir"); // Name without trailing slash expected from proto
    entry2->set_size(4096);
    entry2->set_is_directory(true);
    entry2->set_modified_time(1678886400);
    entry2->set_permissions(0755); // Example permissions

    auto *entry3 = listing->add_entries();
    entry3->set_name(".hidden");
    entry3->set_size(10);
    entry3->set_is_directory(false);
    entry3->set_modified_time(1678886400);
    entry3->set_permissions(0600); // Example permissions

    auto result = response_manager.handle_response(response);

    // Output format is aligned: permissions type size date time name
    ASSERT_GE(result.size(), 4); // Success + at least 3 entries
    EXPECT_EQ(result[0], "Success");

    EXPECT_NE(result[3].find("file.txt"), std::string::npos);
    EXPECT_NE(result[3].find("500 B"), std::string::npos);
    EXPECT_NE(result[3].find("File"), std::string::npos);

    EXPECT_NE(result[4].find("subdir"), std::string::npos);
    EXPECT_NE(result[4].find("4.00 KB"), std::string::npos);
    EXPECT_NE(result[4].find("Directory"), std::string::npos);

    EXPECT_NE(result[5].find(".hidden"), std::string::npos);
    EXPECT_NE(result[5].find("10 B"), std::string::npos);
    EXPECT_NE(result[5].find("File"), std::string::npos);
}

TEST_F(ResponseManagerTest, HandleTerminatedResponse)
{
    fenris::Response response;
    response.set_success(true); // Termination is usually a successful operation
    response.set_type(fenris::ResponseType::TERMINATED);
    response.set_data("Server initiated shutdown");

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "Success"); // Or maybe "Terminated"? Let's stick to
                                     // Success/Error convention
    EXPECT_EQ(result[1], "Server connection terminated");
}

TEST_F(ResponseManagerTest, HandleUnknownResponseType)
{
    fenris::Response response;
    response.set_success(false);
    // Set an invalid or unknown type explicitly if possible,
    // otherwise use a type without a specific handler.
    auto unknown_type_val = static_cast<fenris::ResponseType>(999);
    response.set_type(unknown_type_val); // Invalid type

    auto result = response_manager.handle_response(response);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "Error"); // Should be treated as an error
    // Check for the specific error message format
    std::string expected_error = "Unknown response type";
    EXPECT_EQ(result[1], expected_error);
}

} // namespace tests
} // namespace client
} // namespace fenris
