#include "common/compression_manager.hpp"
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace fenris {
namespace common {
namespace compress {

/**
 * Helper function to convert zlib error codes to CompressionError values
 */
CompressionError zlib_error_to_compression_error(int zlib_error)
{
    switch (zlib_error) {
    case Z_OK:
        return CompressionError::SUCCESS;

    case Z_BUF_ERROR:
        return CompressionError::BUFFER_TOO_SMALL;

    case Z_DATA_ERROR:
        return CompressionError::INVALID_DATA;

    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
    default:
        return CompressionError::COMPRESSION_FAILED;
    }
}

// CompressionManager implementation

std::pair<std::vector<uint8_t>, CompressionError>
CompressionManager::compress(const std::vector<uint8_t> &input, int level)
{
    if (input.empty()) {
        return {std::vector<uint8_t>(), CompressionError::SUCCESS};
    }

    if (level < 0 || level > 9) {
        return {std::vector<uint8_t>(), CompressionError::INVALID_LEVEL};
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
        CompressionError error = zlib_error_to_compression_error(zlib_result);
        return {std::vector<uint8_t>(), error};
    }

    // Resize the result to the actual compressed size
    compressed_data.resize(compressed_max_size);
    return {compressed_data, CompressionError::SUCCESS};
}

std::pair<std::vector<uint8_t>, CompressionError>
CompressionManager::decompress(const std::vector<uint8_t> &input,
                               size_t original_size)
{
    if (input.empty()) {
        return {std::vector<uint8_t>(), CompressionError::SUCCESS};
    }

    // Prepare output buffer based on expected original size
    uLongf decompressed_size = original_size;
    std::vector<uint8_t> decompressed_data(original_size);

    int zlib_result = uncompress(decompressed_data.data(),
                                 &decompressed_size,
                                 input.data(),
                                 input.size());
    if (zlib_result != Z_OK) {
        CompressionError error = zlib_error_to_compression_error(zlib_result);
        return {std::vector<uint8_t>(), error};
    }

    // Resize the result to the actual decompressed size
    decompressed_data.resize(decompressed_size);
    return {decompressed_data, CompressionError::SUCCESS};
}

} // namespace compress
} // namespace common
} // namespace fenris
