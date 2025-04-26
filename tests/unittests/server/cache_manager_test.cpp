#include "common/file_operations.hpp"
#include "common/logging.hpp"
#include "server/cache_manager.hpp"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

namespace fenris {
namespace server {
namespace test {

namespace fs = std::filesystem;

class CacheManagerTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        fs::create_directory(test_dir);
        common::LoggingConfig log_config;
        log_config.level = common::LogLevel::DEBUG;
        log_config.console_logging = true;
        log_config.file_logging = false;

        common::initialize_logging(log_config, "TestCacheManager");
        cache_manager = std::make_unique<CacheManager>(3, "TestCacheManager");
    }

    void TearDown() override
    {
        fs::remove_all(test_dir);
    }

    std::string create_test_file(const std::string &filename,
                                 const std::string &content)
    {
        std::string filepath = test_dir + "/" + filename;
        common::write_file(filepath, content);
        return filepath;
    }

    const std::string test_dir = "/tmp/fenris_cache_manager_test";

    std::unique_ptr<CacheManager> cache_manager;
};

TEST_F(CacheManagerTest, ReadFile)
{
    std::string test_content = "This is a test file content";
    std::string filepath = create_test_file("test1.txt", test_content);

    std::string content = cache_manager->read_file(filepath);
    EXPECT_EQ(content, test_content);

    content = cache_manager->read_file(filepath);
    EXPECT_EQ(content, test_content);

    EXPECT_EQ(cache_manager->get_cache_size(), 1);
}

// Test basic write functionality
TEST_F(CacheManagerTest, WriteFile)
{
    std::string filepath = test_dir + "/test2.txt";
    std::string content = "This is new content to write";

    // Write content to file
    bool result = cache_manager->write_file(filepath, content);
    EXPECT_TRUE(result);

    // Verify cache size
    EXPECT_EQ(cache_manager->get_cache_size(), 1);

    // Verify content was written to disk
    auto [file_data, read_result] = common::read_file(filepath);
    std::string disk_content(file_data.begin(), file_data.end());
    EXPECT_EQ(disk_content, content);

    // Verify content is in cache by reading it again
    std::string cached_content = cache_manager->read_file(filepath);
    EXPECT_EQ(cached_content, content);
}

// Test invalidation functionality
TEST_F(CacheManagerTest, InvalidateCache)
{
    // Create test file
    std::string test_content = "This is another test file";
    std::string filepath = create_test_file("test4.txt", test_content);

    // Read to cache the file
    std::string content = cache_manager->read_file(filepath);
    EXPECT_EQ(content, test_content);
    EXPECT_EQ(cache_manager->get_cache_size(), 1);

    // Invalidate cache entry
    cache_manager->invalidate(filepath);

    // Verify cache is empty
    EXPECT_EQ(cache_manager->get_cache_size(), 0);

    // Reading again should be a cache miss
    content = cache_manager->read_file(filepath);
    EXPECT_EQ(content, test_content);
    EXPECT_EQ(cache_manager->get_cache_size(), 1);
}

// Test clear cache functionality
TEST_F(CacheManagerTest, ClearCache)
{
    // Create multiple test files
    std::vector<std::string> filepaths;
    for (int i = 0; i < 3; i++) {
        std::string filename = "clear_test" + std::to_string(i) + ".txt";
        std::string content = "Content for file " + std::to_string(i);
        filepaths.push_back(create_test_file(filename, content));
    }

    // Read all files to cache them
    for (const auto &filepath : filepaths) {
        cache_manager->read_file(filepath);
    }

    // Verify all files are cached
    EXPECT_EQ(cache_manager->get_cache_size(), 3);

    // Clear cache
    cache_manager->clear_cache();

    // Verify cache is empty
    EXPECT_EQ(cache_manager->get_cache_size(), 0);
}

// Test LRU eviction
TEST_F(CacheManagerTest, LruEviction)
{
    // Create more files than cache capacity (which is 3)
    std::vector<std::string> filepaths;
    for (int i = 0; i < 4; i++) {
        std::string filename = "lru_test" + std::to_string(i) + ".txt";
        std::string content = "Content for file " + std::to_string(i);
        filepaths.push_back(create_test_file(filename, content));
    }

    for (int i = 0; i < 3; i++) {
        cache_manager->read_file(filepaths[i]);
    }
    EXPECT_EQ(cache_manager->get_cache_size(), 3);

    // Access file 0 again to make it most recently used
    cache_manager->read_file(filepaths[0]);

    // Add file 3, which should evict file 1 (least recently used)
    cache_manager->read_file(filepaths[3]);
    EXPECT_EQ(cache_manager->get_cache_size(), 3);

    // Files 0, 2, and 3 should be in cache, file 1 should be evicted
    // Read files and check if they're cache hits or misses

    // This should modify files 0, 2, 3 directly to verify they're in cache
    // We'll update them via direct file system access
    for (int i = 0; i < 4; i++) {
        std::string updated_content = "Updated content " + std::to_string(i);
        common::write_file(filepaths[i], updated_content);
    }

    // File 0 should be cached (old content)
    std::string content0 = cache_manager->read_file(filepaths[0]);
    EXPECT_EQ(content0, "Content for file 0"); // Should have old cached content

    // File 2 should be cached
    std::string content2 = cache_manager->read_file(filepaths[2]);
    EXPECT_EQ(content2, "Content for file 2"); // Should have old cached content

    // File 3 should be cached
    std::string content3 = cache_manager->read_file(filepaths[3]);
    EXPECT_EQ(content3, "Content for file 3"); // Should have old cached content

    // File 1 should not be cached
    std::string content1 = cache_manager->read_file(filepaths[1]);
    EXPECT_EQ(content1,
              "Updated content 1"); // Should read updated content from disk
}

// Test thread safety with concurrent reads
TEST_F(CacheManagerTest, ConcurrentReads)
{
    // Create a test file
    std::string filepath =
        create_test_file("concurrent.txt", "Concurrent test content");

    // Create multiple threads that read the same file
    std::vector<std::thread> threads;
    std::vector<std::string> results(10);

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, filepath, i, &results]() {
            results[i] = cache_manager->read_file(filepath);
        });
    }

    // Join all threads
    for (auto &thread : threads) {
        thread.join();
    }

    // Verify all threads read the correct content
    for (const auto &result : results) {
        EXPECT_EQ(result, "Concurrent test content");
    }

    // Verify file is cached
    EXPECT_EQ(cache_manager->get_cache_size(), 1);
}

} // namespace test
} // namespace server
} // namespace fenris
