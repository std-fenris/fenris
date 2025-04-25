#ifndef FENRIS_COMMON_COMPRESSION_MANAGER_HPP
#define FENRIS_COMMON_COMPRESSION_MANAGER_HPP

#include <cstdint>
#include <memory>
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
 * @class ICompressionManager
 * @brief Interface for compression/decompression operations
 *
 * This abstract class defines the interface for compressing and
 * decompressing data using various compression algorithms.
 */
class ICompressionManager {
  public:
    virtual ~ICompressionManager() = default;

    /**
     * Compresses data using the implemented algorithm
     *
     * @param input The data to compress
     * @param level Compression level (algorithm specific)
     * @return A pair containing the compressed data and a CompressionResult
     */
    virtual std::pair<std::vector<uint8_t>, CompressionResult>
    compress(const std::vector<uint8_t> &input, int level) = 0;

    /**
     * Decompresses data using the implemented algorithm
     *
     * @param input The compressed data
     * @param original_size The original uncompressed size
     * @return A pair containing the decompressed data and a CompressionResult
     */
    virtual std::pair<std::vector<uint8_t>, CompressionResult>
    decompress(const std::vector<uint8_t> &input, size_t original_size) = 0;
};

/**
 * @class CompressionManager
 * @brief Implementation of ICompressionManager using zlib
 *
 * This class implements compression/decompression operations using the
 * zlib library.
 */
class CompressionManager : public ICompressionManager {
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
    compress(const std::vector<uint8_t> &input, int level) override;

    /**
     * Decompresses data using zlib
     *
     * @param input The compressed data
     * @param original_size The original uncompressed size (this must be
     * correct, otherwise the implementation will fail)
     * @return A pair containing the decompressed data and a CompressionResult
     */
    std::pair<std::vector<uint8_t>, CompressionResult>
    decompress(const std::vector<uint8_t> &input,
               size_t original_size) override;
};

} // namespace compress
} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_COMPRESSION_MANAGER_HPP
