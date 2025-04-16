#include "common/request.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace fenris {
namespace common {
namespace tests {

// Test basic request serialization and deserialization
TEST(RequestTest, BasicSerializeDeserialize)
{
    // Create a test request
    fenris::Request request;
    request.set_command(fenris::RequestType::READ_FILE);
    request.set_filename("/path/to/file.txt");
    request.set_ip_addr(0x7F000001); // 127.0.0.1
    std::string file_content = "This is test file content";
    request.set_data(file_content);

    // Serialize the request
    std::vector<uint8_t> serialized = serialize_request(request);
    EXPECT_FALSE(serialized.empty());

    // Deserialize the request
    fenris::Request deserialized = deserialize_request(serialized);

    // Verify the deserialized request matches the original
    EXPECT_EQ(deserialized.command(), request.command());
    EXPECT_EQ(deserialized.filename(), request.filename());
    EXPECT_EQ(deserialized.ip_addr(), request.ip_addr());
    EXPECT_EQ(deserialized.data(), request.data());
}

// Test request with empty fields
TEST(RequestTest, EmptyFields)
{
    fenris::Request request;
    request.set_command(fenris::RequestType::TERMINATE);

    std::vector<uint8_t> serialized = serialize_request(request);
    EXPECT_FALSE(serialized.empty());

    fenris::Request deserialized = deserialize_request(serialized);
    EXPECT_EQ(deserialized.command(), request.command());
    EXPECT_TRUE(deserialized.filename().empty());
    EXPECT_EQ(deserialized.ip_addr(), 0);
    EXPECT_TRUE(deserialized.data().empty());
}

// Test large data in request
TEST(RequestTest, LargeData)
{
    fenris::Request request;
    request.set_command(fenris::RequestType::WRITE_FILE);
    request.set_filename("largefile.bin");

    // Create large data (1MB)
    std::vector<uint8_t> large_data(1024 * 1024);
    for (size_t i = 0; i < large_data.size(); i++) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }
    request.set_data(std::string(large_data.begin(), large_data.end()));

    std::vector<uint8_t> serialized = serialize_request(request);
    EXPECT_FALSE(serialized.empty());

    fenris::Request deserialized = deserialize_request(serialized);
    EXPECT_EQ(deserialized.command(), request.command());
    EXPECT_EQ(deserialized.filename(), request.filename());
    EXPECT_EQ(deserialized.data(), request.data());
    EXPECT_EQ(deserialized.data().size(), large_data.size());
}

// Test request_to_json functionality
TEST(RequestTest, RequestToJson)
{
    fenris::Request request;
    request.set_command(fenris::RequestType::INFO_FILE);
    request.set_filename("test_file.txt");
    request.set_ip_addr(0x0A000001); // 10.0.0.1

    std::string json_str = request_to_json(request);
    EXPECT_FALSE(json_str.empty());

    // Check for expected JSON fragments (basic validation)
    EXPECT_TRUE(json_str.find("\"command\":") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"filename\":") != std::string::npos);
    EXPECT_TRUE(json_str.find("test_file.txt") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"ipAddr\":") != std::string::npos);
}

// Test invalid serialized data
TEST(RequestTest, InvalidData)
{
    std::vector<uint8_t> invalid_data = {0x00,
                                         0x01,
                                         0x02,
                                         0x03}; // Invalid protobuf data

    // This should not crash, but may return an empty or default Request object
    fenris::Request deserialized = deserialize_request(invalid_data);

    // Default command value should be the first enum value (CREATE_FILE)
    EXPECT_EQ(deserialized.command(), fenris::RequestType::CREATE_FILE);
    EXPECT_TRUE(deserialized.filename().empty());
    EXPECT_EQ(deserialized.ip_addr(), 0);
    EXPECT_TRUE(deserialized.data().empty());
}

} // namespace tests
} // namespace common
} // namespace fenris
