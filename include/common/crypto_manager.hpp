#ifndef FENRIS_COMMON_ENCRYPTION_HPP
#define FENRIS_COMMON_ENCRYPTION_HPP

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fenris {
namespace common {
namespace crypto {

/**
 * Result of encryption/decryption operations
 */
enum class EncryptionResult {
    SUCCESS = 0,
    INVALID_KEY_SIZE,
    INVALID_IV_SIZE,
    INVALID_DATA,
    ENCRYPTION_FAILED,
    DECRYPTION_FAILED,
    IV_GENERATION_FAILED,
};

enum class ECDHResult {
    SUCCESS = 0,
    KEY_GENERATION_FAILED,
    SHARED_SECRET_FAILED,
    KEY_DERIVATION_FAILED,
    INVALID_KEY_SIZE,
};

/**
 * Convert EncryptionResult to string representation
 *
 * @param result EncryptionResult to convert
 * @return String representation of the result
 */
std::string encryption_result_to_string(EncryptionResult result);

/**
 * Convert ECDHResult to string representation
 *
 * @param result ECDHResult to convert
 * @return String representation of the result
 */
std::string ecdh_result_to_string(ECDHResult result);

// Constants
constexpr size_t AES_GCM_TAG_SIZE =
    16; // 16 bytes (128 bits) authentication tag
constexpr size_t AES_GCM_IV_SIZE =
    12; // 12 bytes (96 bits) IV as recommended for GCM

/**
 * @interface ICryptoManager
 * @brief Interface defining standard cryptographic operations.
 *
 * This interface provides a contract for classes that implement
 * common cryptographic functionalities like encryption, decryption,
 * key generation, and key derivation.
 */
class ICryptoManager {
  public:
    /**
     * @brief Virtual destructor for the interface.
     */
    virtual ~ICryptoManager() = default;

    /**
     * @brief Encrypts data using a symmetric encryption algorithm.
     *
     * @param plaintext The data to encrypt.
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @return A pair containing the ciphertext (including any authentication
     * tag) and an EncryptionResult.
     */
    virtual std::pair<std::vector<uint8_t>, EncryptionResult>
    encrypt_data(const std::vector<uint8_t> &plaintext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) = 0;

    /**
     * @brief Decrypts data using a symmetric encryption algorithm.
     *
     * @param ciphertext The data to decrypt (including any authentication tag).
     * @param key The decryption key.
     * @param iv The initialization vector.
     * @return A pair containing the plaintext and an EncryptionResult.
     */
    virtual std::pair<std::vector<uint8_t>, EncryptionResult>
    decrypt_data(const std::vector<uint8_t> &ciphertext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) = 0;

    /**
     * @brief Generates an Elliptic Curve Diffie-Hellman (ECDH) key pair.
     *
     * @return A tuple containing the private key, public key, and an
     * ECDHResult.
     */
    virtual std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, ECDHResult>
    generate_ecdh_keypair() = 0;

    /**
     * @brief Computes an ECDH shared secret.
     *
     * @param private_key Our private key.
     * @param peer_public_key The peer's public key.
     * @return A pair containing the shared secret and a ECDHResult.
     */
    virtual std::pair<std::vector<uint8_t>, ECDHResult>
    compute_ecdh_shared_secret(const std::vector<uint8_t> &private_key,
                               const std::vector<uint8_t> &peer_public_key) = 0;

    /**
     * @brief Derives a cryptographic key from a shared secret using a Key
     * Derivation Function (KDF).
     *
     * @param shared_secret The shared secret (e.g., computed via ECDH).
     * @param key_size The desired size of the derived key in bytes.
     * @param context Optional context information for the derivation process.
     * @return A pair containing the derived key and a ECDHResult.
     */
    virtual std::pair<std::vector<uint8_t>, ECDHResult>
    derive_key_from_shared_secret(const std::vector<uint8_t> &shared_secret,
                                  size_t key_size,
                                  const std::vector<uint8_t> &context = {}) = 0;

    /**
     * @brief Generates a cryptographically secure random IV.
     *
     * @return A pair containing the random IV and an EncryptionResult.
     */
    virtual std::pair<std::vector<uint8_t>, EncryptionResult>
    generate_random_iv() = 0;
};

/**
 * @class CryptoManager
 * @brief Concrete implementation of ICryptoManager using Crypto++.
 *
 * This class provides implementations for the cryptographic operations
 * defined in ICryptoManager, specifically using AES-GCM for
 * encryption/decryption and ECDH with the NIST P-256 curve for key exchange.
 */
class CryptoManager : public ICryptoManager {
  public:
    /**
     * @brief Encrypts data using AES-GCM.
     * @param plaintext The data to encrypt.
     * @param key The encryption key (16, 24, or 32 bytes).
     * @param iv The initialization vector (must be AES_GCM_IV_SIZE bytes).
     * @return A pair containing the ciphertext (including tag) and an
     * EncryptionResult.
     * @see ICryptoManager::encrypt_data
     */
    std::pair<std::vector<uint8_t>, EncryptionResult>
    encrypt_data(const std::vector<uint8_t> &plaintext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) override;

    /**
     * @brief Decrypts data using AES-GCM.
     * @param ciphertext The data to decrypt (including tag).
     * @param key The decryption key (16, 24, or 32 bytes).
     * @param iv The initialization vector (must be AES_GCM_IV_SIZE bytes).
     * @return A pair containing the plaintext and a EncryptionResult.
     * @see ICryptoManager::decrypt_data
     */
    std::pair<std::vector<uint8_t>, EncryptionResult>
    decrypt_data(const std::vector<uint8_t> &ciphertext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) override;

    /**
     * @brief Generates an ECDH key pair using the NIST P-256 (secp256r1) curve.
     * @return A tuple containing the private key, public key, and an error
     * code.
     * @see ICryptoManager::generate_ecdh_keypair
     */
    std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, ECDHResult>
    generate_ecdh_keypair() override;

    /**
     * @brief Computes an ECDH shared secret using the NIST P-256 curve.
     * @param private_key Our private key.
     * @param peer_public_key The peer's public key.
     * @return A pair containing the shared secret and a ECDHResult.
     * @see ICryptoManager::compute_ecdh_shared_secret
     */
    std::pair<std::vector<uint8_t>, ECDHResult> compute_ecdh_shared_secret(
        const std::vector<uint8_t> &private_key,
        const std::vector<uint8_t> &peer_public_key) override;

    /**
     * @brief Derives an AES key from a shared secret using HKDF with SHA256.
     * @param shared_secret The shared secret computed via ECDH.
     * @param key_size The desired key size (16, 24, or 32 bytes).
     * @param context Optional context information for the derivation.
     * @return A pair containing the derived key and a ECDHResult.
     * @see ICryptoManager::derive_key_from_shared_secret
     */
    std::pair<std::vector<uint8_t>, ECDHResult> derive_key_from_shared_secret(
        const std::vector<uint8_t> &shared_secret,
        size_t key_size,
        const std::vector<uint8_t> &context = {}) override;

    /**
     * @brief Generates a cryptographically secure random IV for AES-GCM.
     * @return A pair containing the random IV of AES_GCM_IV_SIZE bytes and an
     * EncryptionResult.
     * @see ICryptoManager::generate_random_iv
     */
    std::pair<std::vector<uint8_t>, EncryptionResult>
    generate_random_iv() override;
};

} // namespace crypto
} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_ENCRYPTION_HPP
