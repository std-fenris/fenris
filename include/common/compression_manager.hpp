#ifndef FENRIS_COMMON_COMPRESSION_MANAGER_HPP
#define FENRIS_COMMON_COMPRESSION_MANAGER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fenris {
namespace common {
namespace compress {

/**
 * Result of compression/decompression operations
 */
enum class CompressionResult {
    SUCCESS = 0,
    INVALID_LEVEL,
    COMPRESSION_FAILED,
    DECOMPRESSION_FAILED,
    BUFFER_TOO_SMALL,
    INVALID_DATA
};

/**
 * Convert CompressionResult to string representation
 *
 * @param result CompressionResult to convert
 * @return String representation of the result
 */
std::string compression_result_to_string(CompressionResult result);

/**
 * @class CompressionManager
 *
 * This class implements compression/decompression operations using the
 * zlib library.
 */
class CompressionManager {
  public:
    /**
     * Compresses data using zlib
     *
     * @param input The data to compress
     * @param level Compression level (0-9, where 0 is no compression and 9 is
     * maximum)
     * @return A pair containing the compressed data and a CompressionResult
     */
    std::pair<std::vector<uint8_t>, CompressionResult>
    compress(const std::vector<uint8_t> &input, int level);

    /**
     * Decompresses data using zlib
     *
     * @param input The compressed data
     * @param original_size The original uncompressed size (this must be
     * correct, otherwise the implementation will fail)
     * @return A pair containing the decompressed data and a CompressionResult
     */
    std::pair<std::vector<uint8_t>, CompressionResult>
    decompress(const std::vector<uint8_t> &input, size_t original_size);
};

} // namespace compress
} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_COMPRESSION_MANAGER_HPP
