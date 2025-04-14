#ifndef FENRIS_COMMON_ENCRYPTION_HPP
#define FENRIS_COMMON_ENCRYPTION_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fenris {
namespace common {

/**
 * Error codes for encryption/decryption operations
 */
enum class EncryptionError {
    SUCCESS = 0,
    INVALID_KEY_SIZE,
    INVALID_IV_SIZE,
    INVALID_DATA,
    ENCRYPTION_FAILED,
    DECRYPTION_FAILED,
};

/**
 * Encrypts data using AES-GCM.
 *
 * @param plaintext Data to encrypt
 * @param key Encryption key (must be 16, 24, or 32 bytes for AES-128, AES-192,
 * or AES-256)
 * @param iv Initialization vector (must be 12 bytes for GCM)
 * @return Pair of (encrypted data with auth tag appended, error code)
 */
std::pair<std::vector<uint8_t>, EncryptionError>
encrypt_data_aes_gcm(const std::vector<uint8_t> &plaintext,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &iv);

/**
 * Decrypts data using AES-GCM.
 *
 * @param ciphertext Encrypted data with authentication tag appended
 * @param key Encryption key (must be 16, 24, or 32 bytes for AES-128, AES-192,
 * or AES-256)
 * @param iv Initialization vector (must be 12 bytes for GCM)
 * @return Pair of (decrypted data, error code)
 */
std::pair<std::vector<uint8_t>, EncryptionError>
decrypt_data_aes_gcm(const std::vector<uint8_t> &ciphertext,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &iv);

} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_ENCRYPTION_HPP
