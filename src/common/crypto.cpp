#include "common/crypto.hpp"
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/gcm.h>
#include <cryptopp/osrng.h>
#include <cryptopp/secblock.h>
#include <stdexcept>
#include <vector>

namespace fenris {
namespace common {

// Constants
constexpr size_t AES_GCM_TAG_SIZE =
    16; // 16 bytes (128 bits) authentication tag
constexpr size_t AES_GCM_IV_SIZE =
    12; // 12 bytes (96 bits) IV as recommended for GCM

using namespace CryptoPP;

std::pair<std::vector<uint8_t>, EncryptionError>
encrypt_data_aes_gcm(const std::vector<uint8_t> &plaintext,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &iv)
{
    // Check for empty input
    if (plaintext.empty()) {
        return {std::vector<uint8_t>(), EncryptionError::SUCCESS};
    }

    // Validate key size (must be 16, 24, or 32 bytes for AES-128, AES-192, or
    // AES-256)
    if (key.size() != 16 && key.size() != 24 && key.size() != 32) {
        return {std::vector<uint8_t>(), EncryptionError::INVALID_KEY_SIZE};
    }

    // Validate IV size (must be 12 bytes for GCM as recommended)
    if (iv.size() != AES_GCM_IV_SIZE) {
        return {std::vector<uint8_t>(), EncryptionError::INVALID_IV_SIZE};
    }

    // Create encrypted output vector (will be resized later)
    std::vector<uint8_t> cipher;

    // Set up AES-GCM encryption
    GCM<AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

    AuthenticatedEncryptionFilter encrypt_filter(encryptor,
                                                 new VectorSink(cipher));
    try {
        ArraySource(plaintext.data(),
                    plaintext.size(),
                    true,
                    new Redirector(encrypt_filter));

        return {cipher, EncryptionError::SUCCESS};
    } catch (...) {
        return {std::vector<uint8_t>(), EncryptionError::ENCRYPTION_FAILED};
    }
}

std::pair<std::vector<uint8_t>, EncryptionError>
decrypt_data_aes_gcm(const std::vector<uint8_t> &ciphertext,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &iv)
{
    // Check for empty input
    if (ciphertext.empty()) {
        return {std::vector<uint8_t>(), EncryptionError::SUCCESS};
    }

    // Validate key size (must be 16, 24, or 32 bytes for AES-128, AES-192, or
    // AES-256)
    if (key.size() != 16 && key.size() != 24 && key.size() != 32) {
        return {std::vector<uint8_t>(), EncryptionError::INVALID_KEY_SIZE};
    }

    // Validate IV size (must be 12 bytes for GCM as recommended)
    if (iv.size() != AES_GCM_IV_SIZE) {
        return {std::vector<uint8_t>(), EncryptionError::INVALID_IV_SIZE};
    }

    // Make sure the ciphertext is large enough to contain the auth tag
    if (ciphertext.size() < AES_GCM_TAG_SIZE) {
        return {std::vector<uint8_t>(), EncryptionError::INVALID_DATA};
    }

    // Create output vector
    std::vector<uint8_t> plaintext;
    // Set up AES-GCM decryption
    GCM<AES>::Decryption decryptor;
    decryptor.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());
    AuthenticatedDecryptionFilter decrypt_filter(decryptor,
                                                 new VectorSink(plaintext));
    try {
        ArraySource(ciphertext.data(),
                    ciphertext.size(),
                    true,
                    new Redirector(decrypt_filter));

        return {plaintext, EncryptionError::SUCCESS};
    } catch (...) {
        return {std::vector<uint8_t>(), EncryptionError::DECRYPTION_FAILED};
    }
}

} // namespace common
} // namespace fenris
