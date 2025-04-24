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
     * tag) and an error code.
     */
    virtual std::pair<std::vector<uint8_t>, EncryptionError>
    encrypt_data(const std::vector<uint8_t> &plaintext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) = 0;

    /**
     * @brief Decrypts data using a symmetric encryption algorithm.
     *
     * @param ciphertext The data to decrypt (including any authentication tag).
     * @param key The decryption key.
     * @param iv The initialization vector.
     * @return A pair containing the plaintext and an error code.
     */
    virtual std::pair<std::vector<uint8_t>, EncryptionError>
    decrypt_data(const std::vector<uint8_t> &ciphertext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) = 0;

    /**
     * @brief Generates an Elliptic Curve Diffie-Hellman (ECDH) key pair.
     *
     * @return A tuple containing the private key, public key, and an error
     * code.
     */
    virtual std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, ECDHError>
    generate_ecdh_keypair() = 0;

    /**
     * @brief Computes an ECDH shared secret.
     *
     * @param private_key Our private key.
     * @param peer_public_key The peer's public key.
     * @return A pair containing the shared secret and an error code.
     */
    virtual std::pair<std::vector<uint8_t>, ECDHError>
    compute_ecdh_shared_secret(const std::vector<uint8_t> &private_key,
                               const std::vector<uint8_t> &peer_public_key) = 0;

    /**
     * @brief Derives a cryptographic key from a shared secret using a Key
     * Derivation Function (KDF).
     *
     * @param shared_secret The shared secret (e.g., computed via ECDH).
     * @param key_size The desired size of the derived key in bytes.
     * @param context Optional context information for the derivation process.
     * @return A pair containing the derived key and an error code.
     */
    virtual std::pair<std::vector<uint8_t>, ECDHError>
    derive_key_from_shared_secret(const std::vector<uint8_t> &shared_secret,
                                  size_t key_size,
                                  const std::vector<uint8_t> &context = {}) = 0;
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
     * @return A pair containing the ciphertext (including tag) and an error
     * code.
     * @see ICryptoManager::encrypt_data
     */
    std::pair<std::vector<uint8_t>, EncryptionError>
    encrypt_data(const std::vector<uint8_t> &plaintext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) override;

    /**
     * @brief Decrypts data using AES-GCM.
     * @param ciphertext The data to decrypt (including tag).
     * @param key The decryption key (16, 24, or 32 bytes).
     * @param iv The initialization vector (must be AES_GCM_IV_SIZE bytes).
     * @return A pair containing the plaintext and an error code.
     * @see ICryptoManager::decrypt_data
     */
    std::pair<std::vector<uint8_t>, EncryptionError>
    decrypt_data(const std::vector<uint8_t> &ciphertext,
                 const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &iv) override;

    /**
     * @brief Generates an ECDH key pair using the NIST P-256 (secp256r1) curve.
     * @return A tuple containing the private key, public key, and an error
     * code.
     * @see ICryptoManager::generate_ecdh_keypair
     */
    std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, ECDHError>
    generate_ecdh_keypair() override;

    /**
     * @brief Computes an ECDH shared secret using the NIST P-256 curve.
     * @param private_key Our private key.
     * @param peer_public_key The peer's public key.
     * @return A pair containing the shared secret and an error code.
     * @see ICryptoManager::compute_ecdh_shared_secret
     */
    std::pair<std::vector<uint8_t>, ECDHError> compute_ecdh_shared_secret(
        const std::vector<uint8_t> &private_key,
        const std::vector<uint8_t> &peer_public_key) override;

    /**
     * @brief Derives an AES key from a shared secret using HKDF with SHA256.
     * @param shared_secret The shared secret computed via ECDH.
     * @param key_size The desired key size (16, 24, or 32 bytes).
     * @param context Optional context information for the derivation.
     * @return A pair containing the derived key and an error code.
     * @see ICryptoManager::derive_key_from_shared_secret
     */
    std::pair<std::vector<uint8_t>, ECDHError> derive_key_from_shared_secret(
        const std::vector<uint8_t> &shared_secret,
        size_t key_size,
        const std::vector<uint8_t> &context = {}) override;
};

} // namespace crypto
} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_ENCRYPTION_HPP
