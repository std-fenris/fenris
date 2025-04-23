#include "common/crypto_manager.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

namespace fenris {
namespace common {
namespace crypto {
namespace tests {

constexpr size_t PRIVATE_KEY_SIZE = 32; // 256 bits
constexpr size_t PUBLIC_KEY_SIZE = 65;  // Uncompressed point format

// Test ECDH key pair generation
TEST(ECDHTest, KeyPairGeneration)
{
    auto crypto_manager = CryptoManager();

    // Generate a key pair
    auto [private_key, public_key, error] =
        crypto_manager.generate_ecdh_keypair();
    EXPECT_EQ(error, ECDHError::SUCCESS);
    EXPECT_EQ(private_key.size(), PRIVATE_KEY_SIZE);
    EXPECT_EQ(public_key.size(), PUBLIC_KEY_SIZE);

    // Public key should start with 0x04 (uncompressed point format)
    EXPECT_EQ(public_key[0], 0x04);
}

// Test shared secret computation
TEST(ECDHTest, SharedSecretComputation)
{
    auto crypto_manager = CryptoManager();

    auto [alice_private, alice_public, alice_error] =
        crypto_manager.generate_ecdh_keypair();
    EXPECT_EQ(alice_error, ECDHError::SUCCESS);

    auto [bob_private, bob_public, bob_error] =
        crypto_manager.generate_ecdh_keypair();
    EXPECT_EQ(bob_error, ECDHError::SUCCESS);

    auto [alice_shared, alice_shared_error] =
        crypto_manager.compute_ecdh_shared_secret(alice_private, bob_public);

    auto [bob_shared, bob_shared_error] =
        crypto_manager.compute_ecdh_shared_secret(bob_private, alice_public);

    EXPECT_EQ(alice_shared_error, ECDHError::SUCCESS);
    EXPECT_EQ(bob_shared_error, ECDHError::SUCCESS);

    ASSERT_EQ(alice_shared.size(), bob_shared.size());
    EXPECT_EQ(
        0,
        memcmp(alice_shared.data(), bob_shared.data(), alice_shared.size()));
}

// Test key derivation
TEST(ECDHTest, KeyDerivation)
{
    auto crypto_manager = CryptoManager();

    auto [private_key, public_key, gen_error] =
        crypto_manager.generate_ecdh_keypair();
    auto [shared_secret, shared_error] =
        crypto_manager.compute_ecdh_shared_secret(private_key, public_key);

    EXPECT_EQ(gen_error, ECDHError::SUCCESS);
    EXPECT_EQ(shared_error, ECDHError::SUCCESS);

    // Derive AES-256 key
    auto [key, derive_error] =
        crypto_manager.derive_key_from_shared_secret(shared_secret, 32);
    EXPECT_EQ(derive_error, ECDHError::SUCCESS);
    EXPECT_EQ(key.size(), 32); // 256-bit key

    // Derive AES-128 key
    auto [key128, derive_error128] =
        crypto_manager.derive_key_from_shared_secret(shared_secret, 16);
    EXPECT_EQ(derive_error128, ECDHError::SUCCESS);
    EXPECT_EQ(key128.size(), 16); // 128-bit key

    // Context should affect the derived keys
    std::vector<uint8_t> context = {'t', 'e', 's', 't'};
    auto [key_with_context, derive_error_ctx] =
        crypto_manager.derive_key_from_shared_secret(shared_secret,
                                                     32,
                                                     context);
    EXPECT_EQ(derive_error_ctx, ECDHError::SUCCESS);

    // Keys derived with different contexts should be different
    EXPECT_NE(0, memcmp(key.data(), key_with_context.data(), key.size()));
}

// Test complete flow: key exchange, derivation, and encryption/decryption
TEST(ECDHTest, CompleteFlow)
{
    auto crypto_manager = CryptoManager();

    std::string message = "This is a secret message for ECDH testing";
    std::vector<uint8_t> plaintext(message.begin(), message.end());

    auto [alice_private, alice_public, alice_gen_error] =
        crypto_manager.generate_ecdh_keypair();
    EXPECT_EQ(alice_gen_error, ECDHError::SUCCESS);

    auto [bob_private, bob_public, bob_gen_error] =
        crypto_manager.generate_ecdh_keypair();
    EXPECT_EQ(bob_gen_error, ECDHError::SUCCESS);

    auto [alice_shared, alice_shared_error] =
        crypto_manager.compute_ecdh_shared_secret(alice_private, bob_public);
    EXPECT_EQ(alice_shared_error, ECDHError::SUCCESS);

    auto [alice_key, alice_derive_error] =
        crypto_manager.derive_key_from_shared_secret(alice_shared, 32);
    EXPECT_EQ(alice_derive_error, ECDHError::SUCCESS);

    // Generate a random IV for AES-GCM
    std::vector<uint8_t> iv(AES_GCM_IV_SIZE);
    std::random_device rd;
    std::generate(iv.begin(), iv.end(), std::ref(rd));

    auto [ciphertext, encrypt_error] =
        crypto_manager.encrypt_data(plaintext, alice_key, iv);

    EXPECT_EQ(encrypt_error, EncryptionError::SUCCESS);
    EXPECT_FALSE(ciphertext.empty());
    EXPECT_NE(0,
              memcmp(ciphertext.data(),
                     plaintext.data(),
                     std::min(ciphertext.size(), plaintext.size())));

    auto [bob_shared, bob_shared_error] =
        crypto_manager.compute_ecdh_shared_secret(bob_private, alice_public);
    EXPECT_EQ(bob_shared_error, ECDHError::SUCCESS);

    auto [bob_key, bob_derive_error] =
        crypto_manager.derive_key_from_shared_secret(bob_shared, 32);
    EXPECT_EQ(bob_derive_error, ECDHError::SUCCESS);

    // Verify that both sides derived the same key and IV
    EXPECT_EQ(0, memcmp(alice_key.data(), bob_key.data(), alice_key.size()));

    auto [decrypted, decrypt_error] =
        crypto_manager.decrypt_data(ciphertext, bob_key, iv);
    EXPECT_EQ(decrypt_error, EncryptionError::SUCCESS);
    EXPECT_EQ(decrypted.size(), plaintext.size());
    EXPECT_EQ(0, memcmp(decrypted.data(), plaintext.data(), plaintext.size()));

    std::string decrypted_message(decrypted.begin(), decrypted.end());
    EXPECT_EQ(decrypted_message, message);
}

} // namespace tests
} // namespace crypto
} // namespace common
} // namespace fenris
