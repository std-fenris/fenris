#include "common/compression.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <zlib.h>

namespace fenris {
namespace common {
namespace tests {

// Test empty input for compression
TEST(CompressionTest, CompressEmptyData)
{
    std::vector<uint8_t> input = {};
    auto [result, success] =
        compress_data(input, 6); // Default compression level

    EXPECT_EQ(success, CompressionError::SUCCESS);
    EXPECT_TRUE(result.empty());
}

// Test compression of normal data
TEST(CompressionTest, CompressNormalData)
{
    std::string test_data =
        "This is a test string that should compress well. Repeated data: ";
    test_data += test_data; // Double it to make it more compressible

    std::vector<uint8_t> input(test_data.begin(), test_data.end());
    auto [compressed, success] = compress_data(input, 6);

    EXPECT_EQ(success, CompressionError::SUCCESS);
    EXPECT_FALSE(compressed.empty());

    // Compressed data should be smaller than original for compressible data
    EXPECT_LT(compressed.size(), input.size());
}

// Test compression with different levels
TEST(CompressionTest, CompressWithDifferentLevels)
{
    std::string test_data =
        "This is a test string for compression level testing. " +
        std::string(1000, 'a');
    std::vector<uint8_t> input(test_data.begin(), test_data.end());

    // Test min level compression
    auto [result_min, success_min] = compress_data(input, 1);
    EXPECT_EQ(success_min, CompressionError::SUCCESS);

    // Test max level compression
    auto [result_max, success_max] = compress_data(input, 9);
    EXPECT_EQ(success_max, CompressionError::SUCCESS);

    // Higher compression level should result in same or smaller data for
    // compressible data (This might not always be true for all data types, but
    // should be for our test data)
    EXPECT_LE(result_max.size(), result_min.size());
}

// Test invalid compression level
TEST(CompressionTest, CompressInvalidLevel)
{
    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    auto [result, success] = compress_data(input, -1); // Invalid level

    EXPECT_EQ(success, CompressionError::INVALID_LEVEL);
    EXPECT_TRUE(result.empty());
}

// Test empty input for decompression
TEST(DecompressionTest, DecompressEmptyData)
{
    std::vector<uint8_t> input = {};
    auto [result, success] = decompress_data(input, 0);

    EXPECT_EQ(success, CompressionError::SUCCESS);
    EXPECT_TRUE(result.empty());
}

// Test round-trip compression and decompression
TEST(CompressionTest, RoundTrip)
{
    // Create test data
    std::string test_data = "This is a test string for round-trip "
                            "compression/decompression testing.";
    std::vector<uint8_t> input(test_data.begin(), test_data.end());

    // Compress
    auto [compressed, compress_success] = compress_data(input, 6);
    EXPECT_EQ(compress_success, CompressionError::SUCCESS);

    // Decompress
    auto [decompressed, decompress_success] =
        decompress_data(compressed, input.size());
    EXPECT_EQ(decompress_success, CompressionError::SUCCESS);

    // Verify the result matches the original
    ASSERT_EQ(decompressed.size(), input.size());
    EXPECT_EQ(memcmp(decompressed.data(), input.data(), input.size()), 0);
}

// Test large data compression and decompression
TEST(CompressionTest, LargeData)
{
    // Create a large block of random data (1MB)
    std::vector<uint8_t> large_data(1024 * 1024);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    for (auto &byte : large_data) {
        byte = static_cast<uint8_t>(distrib(gen));
    }

    // Compress the data
    auto [compressed, compress_success] =
        compress_data(large_data, 1); // Using low compression level for speed
    EXPECT_EQ(compress_success, CompressionError::SUCCESS);

    // Decompress the data
    auto [decompressed, decompress_success] =
        decompress_data(compressed, large_data.size());
    EXPECT_EQ(decompress_success, CompressionError::SUCCESS);

    // Verify the result matches the original
    ASSERT_EQ(decompressed.size(), large_data.size());
    EXPECT_EQ(memcmp(decompressed.data(), large_data.data(), large_data.size()),
              0);
}

// Test decompression with invalid data
TEST(DecompressionTest, DecompressInvalidData)
{
    // Create clearly invalid compressed data
    std::vector<uint8_t> invalid_data = {0x78, 0x9C, 0xFF, 0xFF, 0xFF, 0xFF};

    auto [result, success] = decompress_data(invalid_data, 100);

    // Expect INVALID_DATA error because the data is invalid
    EXPECT_EQ(success, CompressionError::INVALID_DATA);
}

// Test decompression with too small buffer
TEST(DecompressionTest, DecompressTooSmallBuffer)
{
    // Create and compress test data that will decompress to a larger size
    std::vector<uint8_t> input(1000, 'A'); // Highly compressible

    auto [compressed, compress_success] = compress_data(input, 9);
    EXPECT_EQ(compress_success, CompressionError::SUCCESS);

    // Decompress with a buffer size that's too small
    auto [decompressed, decompress_success] =
        decompress_data(compressed, 10); // Actual size is 1000

    // Expect BUFFER_TOO_SMALL error because the buffer is too small
    EXPECT_EQ(decompress_success, CompressionError::BUFFER_TOO_SMALL);
}

} // namespace tests
} // namespace common
} // namespace fenris
