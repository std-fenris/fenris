#include "common/response.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace fenris {
namespace common {
namespace tests {

// Test basic response serialization and deserialization
TEST(ResponseTest, BasicSerializeDeserialize)
{
    // Create a test response
    fenris::Response response;
    response.set_type(fenris::ResponseType::SUCCESS);
    response.set_success(true);
    response.set_error_message("");
    std::string data = "Success response data";
    response.set_data(data);

    // Serialize the response
    std::vector<uint8_t> serialized = serialize_response(response);
    EXPECT_FALSE(serialized.empty());

    // Deserialize the response
    fenris::Response deserialized = deserialize_response(serialized);

    // Verify the deserialized response matches the original
    EXPECT_EQ(deserialized.type(), response.type());
    EXPECT_EQ(deserialized.success(), response.success());
    EXPECT_EQ(deserialized.error_message(), response.error_message());
    EXPECT_EQ(deserialized.data(), response.data());
}

// Test error response
TEST(ResponseTest, ErrorResponse)
{
    fenris::Response response;
    response.set_type(fenris::ResponseType::ERROR);
    response.set_success(false);
    response.set_error_message("File not found");

    std::vector<uint8_t> serialized = serialize_response(response);
    EXPECT_FALSE(serialized.empty());

    fenris::Response deserialized = deserialize_response(serialized);
    EXPECT_EQ(deserialized.type(), response.type());
    EXPECT_FALSE(deserialized.success());
    EXPECT_EQ(deserialized.error_message(), "File not found");
}

// Test file info response
TEST(ResponseTest, FileInfoResponse)
{
    fenris::Response response;
    response.set_type(fenris::ResponseType::FILE_INFO);
    response.set_success(true);

    fenris::FileInfo *file_info = response.mutable_file_info();
    file_info->set_name("example.txt");
    file_info->set_size(1024);
    file_info->set_is_directory(false);
    file_info->set_modified_time(1621345678);

    std::vector<uint8_t> serialized = serialize_response(response);
    EXPECT_FALSE(serialized.empty());

    fenris::Response deserialized = deserialize_response(serialized);
    EXPECT_EQ(deserialized.type(), fenris::ResponseType::FILE_INFO);
    EXPECT_TRUE(deserialized.success());
    EXPECT_TRUE(deserialized.has_file_info());

    const fenris::FileInfo &deserialized_info = deserialized.file_info();
    EXPECT_EQ(deserialized_info.name(), "example.txt");
    EXPECT_EQ(deserialized_info.size(), 1024);
    EXPECT_FALSE(deserialized_info.is_directory());
    EXPECT_EQ(deserialized_info.modified_time(), 1621345678);
}

// Test directory listing response
TEST(ResponseTest, DirectoryListingResponse)
{
    fenris::Response response;
    response.set_type(fenris::ResponseType::DIR_LISTING);
    response.set_success(true);

    fenris::DirectoryListing *dir_listing =
        response.mutable_directory_listing();

    // Add file entries
    fenris::FileInfo *file1 = dir_listing->add_entries();
    file1->set_name("file1.txt");
    file1->set_size(100);
    file1->set_is_directory(false);
    file1->set_modified_time(1621345600);

    fenris::FileInfo *file2 = dir_listing->add_entries();
    file2->set_name("file2.txt");
    file2->set_size(200);
    file2->set_is_directory(false);
    file2->set_modified_time(1621345700);

    fenris::FileInfo *dir1 = dir_listing->add_entries();
    dir1->set_name("subdir");
    dir1->set_size(0);
    dir1->set_is_directory(true);
    dir1->set_modified_time(1621345800);

    std::vector<uint8_t> serialized = serialize_response(response);
    EXPECT_FALSE(serialized.empty());

    fenris::Response deserialized = deserialize_response(serialized);
    EXPECT_EQ(deserialized.type(), fenris::ResponseType::DIR_LISTING);
    EXPECT_TRUE(deserialized.success());
    EXPECT_TRUE(deserialized.has_directory_listing());

    const fenris::DirectoryListing &deserialized_listing =
        deserialized.directory_listing();
    EXPECT_EQ(deserialized_listing.entries_size(), 3);

    // Check first entry
    EXPECT_EQ(deserialized_listing.entries(0).name(), "file1.txt");
    EXPECT_EQ(deserialized_listing.entries(0).size(), 100);
    EXPECT_FALSE(deserialized_listing.entries(0).is_directory());

    // Check last entry
    EXPECT_EQ(deserialized_listing.entries(2).name(), "subdir");
    EXPECT_TRUE(deserialized_listing.entries(2).is_directory());
}

// Test large data in response
TEST(ResponseTest, LargeData)
{
    fenris::Response response;
    response.set_type(fenris::ResponseType::FILE_CONTENT);
    response.set_success(true);

    // Create large data (1MB)
    std::vector<uint8_t> large_data(1024 * 1024);
    for (size_t i = 0; i < large_data.size(); i++) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }
    response.set_data(std::string(large_data.begin(), large_data.end()));

    std::vector<uint8_t> serialized = serialize_response(response);
    EXPECT_FALSE(serialized.empty());

    fenris::Response deserialized = deserialize_response(serialized);
    EXPECT_EQ(deserialized.type(), response.type());
    EXPECT_TRUE(deserialized.success());
    EXPECT_EQ(deserialized.data().size(), large_data.size());
    EXPECT_EQ(0,
              memcmp(deserialized.data().data(),
                     large_data.data(),
                     large_data.size()));
}

// Test response_to_json functionality
TEST(ResponseTest, ResponseToJson)
{
    fenris::Response response;
    response.set_type(fenris::ResponseType::FILE_INFO);
    response.set_success(true);

    fenris::FileInfo *file_info = response.mutable_file_info();
    file_info->set_name("document.txt");
    file_info->set_size(2048);
    file_info->set_is_directory(false);
    file_info->set_modified_time(1621345678);

    std::string json_str = response_to_json(response);
    EXPECT_FALSE(json_str.empty());

    // Check for expected JSON fragments
    EXPECT_TRUE(json_str.find("\"type\":") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"success\":") != std::string::npos);
    EXPECT_TRUE(json_str.find("true") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"fileInfo\":") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"name\":") != std::string::npos);
    EXPECT_TRUE(json_str.find("document.txt") != std::string::npos);
}

// Test invalid serialized data
TEST(ResponseTest, InvalidData)
{
    std::vector<uint8_t> invalid_data = {0x00,
                                         0x01,
                                         0x02,
                                         0x03}; // Invalid protobuf data

    // This should not crash, but return a default Response object
    fenris::Response deserialized = deserialize_response(invalid_data);

    // Default values should be present
    EXPECT_EQ(deserialized.type(),
              fenris::ResponseType::FILE_INFO); // First enum value
    EXPECT_FALSE(deserialized.success());
    EXPECT_TRUE(deserialized.error_message().empty());
    EXPECT_TRUE(deserialized.data().empty());
}

} // namespace tests
} // namespace common
} // namespace fenris
