#include "common/crypto_manager.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

namespace fenris {
namespace common {
namespace crypto {
namespace tests {

// Test basic encryption and decryption
TEST(EncryptionTest, BasicEncryptDecrypt)
{
    auto crypto_manager = CryptoManager();

    // Create test data
    std::string message = "This is a secret message to encrypt";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    // Create a 256-bit key (32 bytes)
    std::vector<uint8_t> key(32, 0);
    for (size_t i = 0; i < key.size(); i++) {
        key[i] = static_cast<uint8_t>(i % 256);
    }

    // Create a 96-bit IV (12 bytes)
    std::vector<uint8_t> iv(12, 0);
    for (size_t i = 0; i < iv.size(); i++) {
        iv[i] = static_cast<uint8_t>((i + 100) % 256);
    }

    // Encrypt the data
    auto [ciphertext, encrypt_result] =
        crypto_manager.encrypt_data(plaintext, key, iv);

    EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);
    EXPECT_FALSE(ciphertext.empty());
    EXPECT_NE(ciphertext.size(),
              plaintext.size()); // Should be larger due to authentication tag

    // Ensure the ciphertext is different from plaintext
    EXPECT_NE(0,
              memcmp(ciphertext.data(),
                     plaintext.data(),
                     std::min(ciphertext.size(), plaintext.size())));

    // Decrypt the data
    auto [decrypted, decrypt_result] =
        crypto_manager.decrypt_data(ciphertext, key, iv);
    EXPECT_EQ(decrypt_result, EncryptionResult::SUCCESS);

    // Verify the decryption result matches the original
    ASSERT_EQ(decrypted.size(), plaintext.size());
    EXPECT_EQ(0, memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
}

// Test empty input
TEST(EncryptionTest, EmptyInput)
{
    auto crypto_manager = CryptoManager();

    std::vector<uint8_t> empty;
    std::vector<uint8_t> key(32, 0); // 256-bit key
    std::vector<uint8_t> iv(12, 0);  // 96-bit IV

    // Encrypt empty data
    auto [ciphertext, encrypt_result] =
        crypto_manager.encrypt_data(empty, key, iv);
    EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);
    EXPECT_TRUE(ciphertext.empty());

    // Decrypt empty data
    auto [decrypted, decrypt_result] =
        crypto_manager.decrypt_data(empty, key, iv);
    EXPECT_EQ(decrypt_result, EncryptionResult::SUCCESS);
    EXPECT_TRUE(decrypted.empty());
}

// Test invalid key size
TEST(EncryptionTest, InvalidKeySize)
{
    auto crypto_manager = CryptoManager();

    std::string message = "Test message";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    // Create an invalid key (20 bytes - not a valid AES key size)
    std::vector<uint8_t> invalid_key(20, 0);
    std::vector<uint8_t> iv(12, 0);

    // Encrypt with invalid key
    auto [ciphertext, encrypt_result] =
        crypto_manager.encrypt_data(plaintext, invalid_key, iv);
    EXPECT_EQ(encrypt_result, EncryptionResult::INVALID_KEY_SIZE);
    EXPECT_TRUE(ciphertext.empty());

    // Create a valid key and encrypt for testing decryption
    std::vector<uint8_t> valid_key(32, 0);
    auto [valid_ciphertext, _] =
        crypto_manager.encrypt_data(plaintext, valid_key, iv);

    // Decrypt with invalid key
    auto [decrypted, decrypt_result] =
        crypto_manager.decrypt_data(valid_ciphertext, invalid_key, iv);
    EXPECT_EQ(decrypt_result, EncryptionResult::INVALID_KEY_SIZE);
    EXPECT_TRUE(decrypted.empty());
}

// Test invalid IV size
TEST(EncryptionTest, InvalidIVSize)
{
    auto crypto_manager = CryptoManager();

    std::string message = "Test message";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    std::vector<uint8_t> key(32, 0); // Valid 256-bit key

    // Create an invalid IV (16 bytes - not the recommended 12 bytes for GCM)
    std::vector<uint8_t> invalid_iv(16, 0);

    // Encrypt with invalid IV
    auto [ciphertext, encrypt_result] =
        crypto_manager.encrypt_data(plaintext, key, invalid_iv);
    EXPECT_EQ(encrypt_result, EncryptionResult::INVALID_IV_SIZE);
    EXPECT_TRUE(ciphertext.empty());

    // Create a valid IV and encrypt for testing decryption
    std::vector<uint8_t> valid_iv(12, 0);
    auto [valid_ciphertext, _] =
        crypto_manager.encrypt_data(plaintext, key, valid_iv);

    // Decrypt with invalid IV
    auto [decrypted, decrypt_result] =
        crypto_manager.decrypt_data(valid_ciphertext, key, invalid_iv);
    EXPECT_EQ(decrypt_result, EncryptionResult::INVALID_IV_SIZE);
    EXPECT_TRUE(decrypted.empty());
}

// Test tampered ciphertext (integrity check should fail)
TEST(EncryptionTest, TamperedCiphertext)
{
    auto crypto_manager = CryptoManager();

    std::string message = "This is a test message for integrity check";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    std::vector<uint8_t> key(32, 0); // 256-bit key
    std::vector<uint8_t> iv(12, 0);  // 96-bit IV

    // Encrypt the data
    auto [ciphertext, encrypt_result] =
        crypto_manager.encrypt_data(plaintext, key, iv);
    EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);

    // Tamper with the ciphertext
    if (!ciphertext.empty()) {
        ciphertext[ciphertext.size() / 2] ^= 0x01; // Flip a bit
    }

    // Decrypt the tampered data
    auto [decrypted, decrypt_result] =
        crypto_manager.decrypt_data(ciphertext, key, iv);
    EXPECT_EQ(decrypt_result, EncryptionResult::DECRYPTION_FAILED);
    EXPECT_TRUE(decrypted.empty());
}

// Test large data encryption and decryption
TEST(EncryptionTest, LargeData)
{
    auto crypto_manager = CryptoManager();

    // Create a large block of random data (1MB)
    std::vector<uint8_t> large_data(1024 * 1024);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    for (auto &byte : large_data) {
        byte = static_cast<uint8_t>(distrib(gen));
    }

    // Create key and IV
    std::vector<uint8_t> key(32, 0); // 256-bit key
    std::vector<uint8_t> iv(12, 0);  // 96-bit IV

    // Fill with some random data
    for (size_t i = 0; i < key.size(); i++) {
        key[i] = static_cast<uint8_t>(distrib(gen));
    }

    for (size_t i = 0; i < iv.size(); i++) {
        iv[i] = static_cast<uint8_t>(distrib(gen));
    }

    // Encrypt the data
    auto [ciphertext, encrypt_result] =
        crypto_manager.encrypt_data(large_data, key, iv);
    EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);

    // Decrypt the data
    auto [decrypted, decrypt_result] =
        crypto_manager.decrypt_data(ciphertext, key, iv);
    EXPECT_EQ(decrypt_result, EncryptionResult::SUCCESS);

    // Verify the result matches the original
    ASSERT_EQ(decrypted.size(), large_data.size());
    EXPECT_EQ(0,
              memcmp(decrypted.data(), large_data.data(), large_data.size()));
}

// Test different key sizes (AES-128, AES-192, AES-256)
TEST(EncryptionTest, DifferentKeySizes)
{
    auto crypto_manager = CryptoManager();

    std::string message = "Testing different key sizes";
    std::vector<uint8_t> plaintext(message.begin(), message.end());
    std::vector<uint8_t> iv(12, 0);

    // Test AES-128 (16-byte key)
    {
        std::vector<uint8_t> key_128(16, 1);
        auto [ciphertext, encrypt_result] =
            crypto_manager.encrypt_data(plaintext, key_128, iv);
        EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);

        auto [decrypted, decrypt_result] =
            crypto_manager.decrypt_data(ciphertext, key_128, iv);
        EXPECT_EQ(decrypt_result, EncryptionResult::SUCCESS);

        ASSERT_EQ(decrypted.size(), plaintext.size());
        EXPECT_EQ(0,
                  memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
    }

    // Test AES-192 (24-byte key)
    {
        std::vector<uint8_t> key_192(24, 2);
        auto [ciphertext, encrypt_result] =
            crypto_manager.encrypt_data(plaintext, key_192, iv);
        EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);

        auto [decrypted, decrypt_result] =
            crypto_manager.decrypt_data(ciphertext, key_192, iv);
        EXPECT_EQ(decrypt_result, EncryptionResult::SUCCESS);

        ASSERT_EQ(decrypted.size(), plaintext.size());
        EXPECT_EQ(0,
                  memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
    }

    // Test AES-256 (32-byte key)
    {
        std::vector<uint8_t> key_256(32, 3);
        auto [ciphertext, encrypt_result] =
            crypto_manager.encrypt_data(plaintext, key_256, iv);
        EXPECT_EQ(encrypt_result, EncryptionResult::SUCCESS);

        auto [decrypted, decrypt_result] =
            crypto_manager.decrypt_data(ciphertext, key_256, iv);
        EXPECT_EQ(decrypt_result, EncryptionResult::SUCCESS);

        ASSERT_EQ(decrypted.size(), plaintext.size());
        EXPECT_EQ(0,
                  memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
    }
}

} // namespace tests
} // namespace crypto
} // namespace common
} // namespace fenris
