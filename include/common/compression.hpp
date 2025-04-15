#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace fenris {
namespace common {

/**
 * Error codes for compression/decompression operations
 */
enum class CompressionError {
    SUCCESS = 0,
    INVALID_LEVEL,
    COMPRESSION_FAILED,
    DECOMPRESSION_FAILED,
    BUFFER_TOO_SMALL,
    INVALID_DATA
};

/**
 * Compresses data using zlib
 *
 * @param input The data to compress
 * @param level Compression level (0-9, where 0 is no compression and 9 is
 * maximum)
 * @return A pair containing the compressed data and an error code
 */
std::pair<std::vector<uint8_t>, CompressionError>
compress_data(const std::vector<uint8_t> &input, int level);

/**
 * Decompresses data using zlib
 *
 * @param input The compressed data
 * @param original_size The original uncompressed size (this must be correct,
 * otherwise the implementation will fail)
 * @return A pair containing the decompressed data and an error code
 */
std::pair<std::vector<uint8_t>, CompressionError>
decompress_data(const std::vector<uint8_t> &input, size_t original_size);

} // namespace common
} // namespace fenris
