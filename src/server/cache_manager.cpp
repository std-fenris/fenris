#include "server/cache_manager.hpp"
#include "common/file_operations.hpp"
#include "common/logging.hpp"

#include <algorithm>

namespace fenris {
namespace server {

using namespace common;

CacheManager::CacheManager(size_t max_cache_size,
                           const std::string &logger_name)
    : m_max_cache_size(max_cache_size), m_logger(get_logger(logger_name))
{
    m_logger->info("cache manager initialized with max size: {}",
                   max_cache_size);
}

std::string CacheManager::read_file(const std::string &filename)
{

    // Check if file is in cache
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(filename);
        if (it != m_cache.end()) {
            // Cache hit: update LRU and return content
            m_logger->debug("cache hit for file: {}", filename);
            update_lru(filename);
            return it->second;
        }
    }

    m_logger->debug("cache miss for file: {}", filename);

    // Cache miss: read from file system using existing file operations
    auto [data, result] = common::read_file(filename);
    if (result != common::FileOperationResult::SUCCESS) {
        m_logger->warn("failed to read file: {}, error: {}",
                       filename,
                       common::file_operation_result_to_string(result));
        return "";
    }

    // Add to cache if not empty
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!data.empty()) {
            if (m_cache.find(filename) == m_cache.end() &&
                m_cache.size() >= m_max_cache_size) {
                // Cache is full, remove least recently used entry
                remove_lru_entry();
            }

            // Insert into cache and update LRU
            m_cache[filename] = data;
            update_lru(filename);
        }
    }

    m_logger->debug("file cached: {} ({} bytes)", filename, data.size());

    return data;
}

bool CacheManager::write_file(const std::string &filename,
                              const std::string &content)
{
    // Write to file system using existing file operations
    auto result = common::write_file(filename, content);
    if (result != common::FileOperationResult::SUCCESS) {
        m_logger->warn("failed to write file: {}, error: {}",
                       filename,
                       common::file_operation_result_to_string(result));
        return false;
    }

    // Update cache with new content
    m_logger->debug("updating cache for file: {}", filename);

    // If adding this would exceed cache size, remove LRU entry
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cache.find(filename) == m_cache.end() &&
            m_cache.size() >= m_max_cache_size) {
            remove_lru_entry();
        }

        // Add/update in cache
        m_cache[filename] = content;
        update_lru(filename);
    }

    return true;
}

void CacheManager::invalidate(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto cache_it = m_cache.find(filename);
    if (cache_it != m_cache.end()) {
        // Remove from cache
        m_cache.erase(filename);

        // Remove from LRU tracking
        auto lru_it = m_lru_map.find(filename);
        if (lru_it != m_lru_map.end()) {
            m_lru_list.erase(lru_it->second);
            m_lru_map.erase(lru_it);
        } else {
            m_logger->warn("file not found in LRU tracking: {}", filename);
        }

        m_logger->debug("invalidated cache entry: {}", filename);
    }
}

void CacheManager::clear_cache()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = m_cache.size();
    m_cache.clear();
    m_lru_list.clear();
    m_lru_map.clear();
    m_logger->info("cache cleared, {} entries removed", count);
}

size_t CacheManager::get_cache_size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_cache.size();
}

void CacheManager::update_lru(const std::string &filename)
{
    // Check if file is in LRU list
    auto it = m_lru_map.find(filename);
    if (it != m_lru_map.end() && it->second != m_lru_list.end()) {
        // Move the file to the front of the list (most recently used)
        m_lru_list.erase(it->second);
        m_lru_list.push_front(filename);
        m_lru_map[filename] = m_lru_list.begin();
    } else {
        // If not in LRU list but in cache, add it to LRU tracking
        if (m_cache.find(filename) != m_cache.end()) {
            m_lru_list.push_front(filename);
            m_lru_map[filename] = m_lru_list.begin();
        }
    }
}

void CacheManager::remove_lru_entry()
{
    if (m_lru_list.empty()) {
        return;
    }

    // Get the least recently used filename (at the back of the list)
    std::string lru_filename = m_lru_list.back();

    m_logger->debug("removing LRU cache entry: {}", lru_filename);

    // Remove from cache
    m_cache.erase(lru_filename);

    // Remove from LRU tracking
    m_lru_map.erase(lru_filename);
    m_lru_list.pop_back();
}

} // namespace server
} // namespace fenris
