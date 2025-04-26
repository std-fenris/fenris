#ifndef FENRIS_SERVER_CACHE_MANAGER_HPP
#define FENRIS_SERVER_CACHE_MANAGER_HPP

#include "common/file_operations.hpp"
#include "common/logging.hpp"

#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fenris {
namespace server {

/**
 * @class CacheManager
 * @brief Manages file content caching with LRU invalidation strategy
 *
 * This class provides caching for file contents to reduce disk I/O operations.
 * It implements an LRU (Least Recently Used) cache eviction policy when
 * the maximum cache size is reached.
 */
class CacheManager {
  public:
    /**
     * @brief Constructor
     * @param max_cache_size Maximum number of files to cache
     * @param logger_name Name for the logger instance
     */
    explicit CacheManager(size_t max_cache_size = 100,
                          const std::string &logger_name = "CacheManager");

    /**
     * @brief Read file content, using cache if available
     * @param filename Path to the file
     * @return File content as string, empty if file not found
     */
    std::string read_file(const std::string &filename);

    /**
     * @brief Write content to file and update cache
     * @param filename Path to the file
     * @param content Content to write
     * @return true if successful, false otherwise
     */
    bool write_file(const std::string &filename, const std::string &content);

    /**
     * @brief Invalidate a specific file in cache
     * @param filename Path to the file to invalidate
     */
    void invalidate(const std::string &filename);

    /**
     * @brief Clear all cached entries
     */
    void clear_cache();

    /**
     * @brief Get current number of cached files
     * @return Number of files in cache
     */
    size_t get_cache_size() const;

  private:
    // Key: filename, Value: file content
    std::unordered_map<std::string, std::string> m_cache;

    std::unordered_map<std::string, std::list<std::string>::iterator> m_lru_map;

    // For LRU tracking - list of filenames ordered by most recently used
    std::list<std::string> m_lru_list;

    // Maximum number of files to cache
    size_t m_max_cache_size;

    // Logger
    common::Logger m_logger;

    // Mutex for thread safety
    mutable std::mutex m_mutex;

    // Helper method to update LRU information when a file is accessed
    void update_lru(const std::string &filename);

    // Helper method to remove least recently used entry when cache is full
    void remove_lru_entry();
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_CACHE_MANAGER_HPP
