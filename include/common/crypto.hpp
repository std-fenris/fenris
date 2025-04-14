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

enum class ECDHError {
    SUCCESS = 0,
    KEY_GENERATION_FAILED,
    SHARED_SECRET_FAILED,
    KEY_DERIVATION_FAILED,
    INVALID_KEY_SIZE,
};

// Constants
constexpr size_t AES_GCM_TAG_SIZE =
    16; // 16 bytes (128 bits) authentication tag
constexpr size_t AES_GCM_IV_SIZE =
    12; // 12 bytes (96 bits) IV as recommended for GCM

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

/**
 * Generates an ECDH key pair (private and public key).
 *
 * Uses the NIST P-256 curve (secp256r1).
 *
 * @return Tuple of (private key, public key, error code)
 */
std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, ECDHError>
generate_ecdh_keypair();

/**
 * Derives a shared secret using ECDH.
 *
 * @param private_key Your private key
 * @param peer_public_key The public key of the peer
 * @return Pair of (shared secret, error code)
 */
std::pair<std::vector<uint8_t>, ECDHError>
compute_ecdh_shared_secret(const std::vector<uint8_t> &private_key,
                           const std::vector<uint8_t> &peer_public_key);

/**
 * Derives AES key from a shared secret.
 *
 * Uses HKDF (HMAC-based Key Derivation Function) with SHA-256.
 *
 * @param shared_secret The ECDH shared secret
 * @param key_size Size of the AES key to generate (16, 24, or 32 bytes)
 * @param context Optional context information for the derivation
 * @return Pair of (AES key, error code)
 */
std::pair<std::vector<uint8_t>, ECDHError>
derive_key_from_shared_secret(const std::vector<uint8_t> &shared_secret,
                              size_t key_size = 32,
                              const std::vector<uint8_t> &context = {});

} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_ENCRYPTION_HPP
