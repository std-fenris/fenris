#include "common/compression_manager.hpp"
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace fenris {
namespace common {
namespace compress {

std::string compression_error_to_string(CompressionResult result)
{
    switch (result) {
    case CompressionResult::SUCCESS:
        return "success";
    case CompressionResult::INVALID_LEVEL:
        return "invalid compression level";
    case CompressionResult::COMPRESSION_FAILED:
        return "compression operation failed";
    case CompressionResult::DECOMPRESSION_FAILED:
        return "decompression operation failed";
    case CompressionResult::BUFFER_TOO_SMALL:
        return "buffer too small for operation";
    case CompressionResult::INVALID_DATA:
        return "invalid compressed data";
    default:
        return "unrecognized compression result";
    }
}

/**
 * Helper function to convert zlib error codes to CompressionResult values
 */
CompressionResult zlib_error_to_compression_result(int zlib_error)
{
    switch (zlib_error) {
    case Z_OK:
        return CompressionResult::SUCCESS;

    case Z_BUF_ERROR:
        return CompressionResult::BUFFER_TOO_SMALL;

    case Z_DATA_ERROR:
        return CompressionResult::INVALID_DATA;

    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
    default:
        return CompressionResult::COMPRESSION_FAILED;
    }
}

// CompressionManager implementation

std::pair<std::vector<uint8_t>, CompressionResult>
CompressionManager::compress(const std::vector<uint8_t> &input, int level)
{
    if (input.empty()) {
        return {std::vector<uint8_t>(), CompressionResult::SUCCESS};
    }

    if (level < 0 || level > 9) {
        return {std::vector<uint8_t>(), CompressionResult::INVALID_LEVEL};
    }

    // Estimate the maximum compressed size
    uLongf compressed_max_size = compressBound(input.size());
    std::vector<uint8_t> compressed_data(compressed_max_size);

    int zlib_result = compress2(compressed_data.data(),
                                &compressed_max_size,
                                input.data(),
                                input.size(),
                                level);
    if (zlib_result != Z_OK) {
        CompressionResult result =
            zlib_error_to_compression_result(zlib_result);
        return {std::vector<uint8_t>(), result};
    }

    // Resize the result to the actual compressed size
    compressed_data.resize(compressed_max_size);
    return {compressed_data, CompressionResult::SUCCESS};
}

std::pair<std::vector<uint8_t>, CompressionResult>
CompressionManager::decompress(const std::vector<uint8_t> &input,
                               size_t original_size)
{
    if (input.empty()) {
        return {std::vector<uint8_t>(), CompressionResult::SUCCESS};
    }

    // Prepare output buffer based on expected original size
    uLongf decompressed_size = original_size;
    std::vector<uint8_t> decompressed_data(original_size);

    int zlib_result = uncompress(decompressed_data.data(),
                                 &decompressed_size,
                                 input.data(),
                                 input.size());
    if (zlib_result != Z_OK) {
        CompressionResult result =
            zlib_error_to_compression_result(zlib_result);
        return {std::vector<uint8_t>(), result};
    }

    // Resize the result to the actual decompressed size
    decompressed_data.resize(decompressed_size);
    return {decompressed_data, CompressionResult::SUCCESS};
}

} // namespace compress
} // namespace common
} // namespace fenris
