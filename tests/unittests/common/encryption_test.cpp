#include "common/crypto.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

namespace fenris {
namespace common {
namespace tests {

// Test basic encryption and decryption
TEST(EncryptionTest, BasicEncryptDecrypt)
{
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
    auto [ciphertext, encrypt_error] = encrypt_data_aes_gcm(plaintext, key, iv);
    EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);
    EXPECT_FALSE(ciphertext.empty());
    EXPECT_NE(ciphertext.size(),
              plaintext.size()); // Should be larger due to authentication tag

    // Ensure the ciphertext is different from plaintext
    EXPECT_NE(0,
              memcmp(ciphertext.data(),
                     plaintext.data(),
                     std::min(ciphertext.size(), plaintext.size())));

    // Decrypt the data
    auto [decrypted, decrypt_error] = decrypt_data_aes_gcm(ciphertext, key, iv);
    EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);

    // Verify the decryption result matches the original
    ASSERT_EQ(decrypted.size(), plaintext.size());
    EXPECT_EQ(0, memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
}

// Test empty input
TEST(EncryptionTest, EmptyInput)
{
    std::vector<uint8_t> empty;
    std::vector<uint8_t> key(32, 0); // 256-bit key
    std::vector<uint8_t> iv(12, 0);  // 96-bit IV

    // Encrypt empty data
    auto [ciphertext, encrypt_error] = encrypt_data_aes_gcm(empty, key, iv);
    EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);
    EXPECT_TRUE(ciphertext.empty());

    // Decrypt empty data
    auto [decrypted, decrypt_error] = decrypt_data_aes_gcm(empty, key, iv);
    EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);
    EXPECT_TRUE(decrypted.empty());
}

// Test invalid key size
TEST(EncryptionTest, InvalidKeySize)
{
    std::string message = "Test message";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    // Create an invalid key (20 bytes - not a valid AES key size)
    std::vector<uint8_t> invalid_key(20, 0);
    std::vector<uint8_t> iv(12, 0);

    // Encrypt with invalid key
    auto [ciphertext, encrypt_error] =
        encrypt_data_aes_gcm(plaintext, invalid_key, iv);
    EXPECT_EQ(encrypt_error, EncryptionError::INVALID_KEY_SIZE);
    EXPECT_TRUE(ciphertext.empty());

    // Create a valid key and encrypt for testing decryption
    std::vector<uint8_t> valid_key(32, 0);
    auto [valid_ciphertext, _] = encrypt_data_aes_gcm(plaintext, valid_key, iv);

    // Decrypt with invalid key
    auto [decrypted, decrypt_error] =
        decrypt_data_aes_gcm(valid_ciphertext, invalid_key, iv);
    EXPECT_EQ(decrypt_error, EncryptionError::INVALID_KEY_SIZE);
    EXPECT_TRUE(decrypted.empty());
}

// Test invalid IV size
TEST(EncryptionTest, InvalidIVSize)
{
    std::string message = "Test message";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    std::vector<uint8_t> key(32, 0); // Valid 256-bit key

    // Create an invalid IV (16 bytes - not the recommended 12 bytes for GCM)
    std::vector<uint8_t> invalid_iv(16, 0);

    // Encrypt with invalid IV
    auto [ciphertext, encrypt_error] =
        encrypt_data_aes_gcm(plaintext, key, invalid_iv);
    EXPECT_EQ(encrypt_error, EncryptionError::INVALID_IV_SIZE);
    EXPECT_TRUE(ciphertext.empty());

    // Create a valid IV and encrypt for testing decryption
    std::vector<uint8_t> valid_iv(12, 0);
    auto [valid_ciphertext, _] = encrypt_data_aes_gcm(plaintext, key, valid_iv);

    // Decrypt with invalid IV
    auto [decrypted, decrypt_error] =
        decrypt_data_aes_gcm(valid_ciphertext, key, invalid_iv);
    EXPECT_EQ(decrypt_error, EncryptionError::INVALID_IV_SIZE);
    EXPECT_TRUE(decrypted.empty());
}

// Test tampered ciphertext (integrity check should fail)
TEST(EncryptionTest, TamperedCiphertext)
{
    std::string message = "This is a test message for integrity check";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    std::vector<uint8_t> key(32, 0); // 256-bit key
    std::vector<uint8_t> iv(12, 0);  // 96-bit IV

    // Encrypt the data
    auto [ciphertext, encrypt_error] = encrypt_data_aes_gcm(plaintext, key, iv);
    EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);

    // Tamper with the ciphertext
    if (!ciphertext.empty()) {
        ciphertext[ciphertext.size() / 2] ^= 0x01; // Flip a bit
    }

    // Decrypt the tampered data
    auto [decrypted, decrypt_error] = decrypt_data_aes_gcm(ciphertext, key, iv);
    EXPECT_EQ(decrypt_error, EncryptionError::DECRYPTION_FAILED);
    EXPECT_TRUE(decrypted.empty());
}

// Test large data encryption and decryption
TEST(EncryptionTest, LargeData)
{
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
    auto [ciphertext, encrypt_error] =
        encrypt_data_aes_gcm(large_data, key, iv);
    EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);

    // Decrypt the data
    auto [decrypted, decrypt_error] = decrypt_data_aes_gcm(ciphertext, key, iv);
    EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);

    // Verify the result matches the original
    ASSERT_EQ(decrypted.size(), large_data.size());
    EXPECT_EQ(0,
              memcmp(decrypted.data(), large_data.data(), large_data.size()));
}

// Test different key sizes (AES-128, AES-192, AES-256)
TEST(EncryptionTest, DifferentKeySizes)
{
    std::string message = "Testing different key sizes";
    std::vector<uint8_t> plaintext(message.begin(), message.end());
    std::vector<uint8_t> iv(12, 0);

    // Test AES-128 (16-byte key)
    {
        std::vector<uint8_t> key_128(16, 1);
        auto [ciphertext, encrypt_error] =
            encrypt_data_aes_gcm(plaintext, key_128, iv);
        EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);

        auto [decrypted, decrypt_error] =
            decrypt_data_aes_gcm(ciphertext, key_128, iv);
        EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);

        ASSERT_EQ(decrypted.size(), plaintext.size());
        EXPECT_EQ(0,
                  memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
    }

    // Test AES-192 (24-byte key)
    {
        std::vector<uint8_t> key_192(24, 2);
        auto [ciphertext, encrypt_error] =
            encrypt_data_aes_gcm(plaintext, key_192, iv);
        EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);

        auto [decrypted, decrypt_error] =
            decrypt_data_aes_gcm(ciphertext, key_192, iv);
        EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);

        ASSERT_EQ(decrypted.size(), plaintext.size());
        EXPECT_EQ(0,
                  memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
    }

    // Test AES-256 (32-byte key)
    {
        std::vector<uint8_t> key_256(32, 3);
        auto [ciphertext, encrypt_error] =
            encrypt_data_aes_gcm(plaintext, key_256, iv);
        EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);

        auto [decrypted, decrypt_error] =
            decrypt_data_aes_gcm(ciphertext, key_256, iv);
        EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);

        ASSERT_EQ(decrypted.size(), plaintext.size());
        EXPECT_EQ(0,
                  memcmp(decrypted.data(), plaintext.data(), plaintext.size()));
    }
}

} // namespace tests
} // namespace common
} // namespace fenris
