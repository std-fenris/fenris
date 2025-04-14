#include "common/compression.hpp"
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace fenris {
namespace common {

std::pair<std::vector<uint8_t>, int>
compress_data(const std::vector<uint8_t> &input, int level)
{
    // If input is empty, return empty result with success
    if (input.empty()) {
        return {std::vector<uint8_t>(), Z_OK};
    }

    // Validate and adjust compression level if needed
    if (level < 0 || level > 9) {
        level = Z_DEFAULT_COMPRESSION; // Default compression level (usually 6)
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
        return {std::vector<uint8_t>(), zlib_result};
    }

    // Resize the result to the actual compressed size
    compressed_data.resize(compressed_max_size);

    return {std::move(compressed_data), Z_OK};
}

std::pair<std::vector<uint8_t>, int>
decompress_data(const std::vector<uint8_t> &input, size_t original_size)
{
    // If input is empty, return empty result with success
    if (input.empty()) {
        return {std::vector<uint8_t>(), Z_OK};
    }

    uLongf decompressed_size = original_size;
    std::vector<uint8_t> decompressed_data(original_size);

    int zlib_result = uncompress(decompressed_data.data(),
                                 &decompressed_size,
                                 input.data(),
                                 input.size());
    if (zlib_result != Z_OK) {
        return {std::vector<uint8_t>(), zlib_result};
    }

    return {std::move(decompressed_data), Z_OK};
}

} // namespace common
} // namespace fenris
