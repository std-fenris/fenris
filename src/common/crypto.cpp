#include "common/crypto.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/filters.h>
#include <cryptopp/gcm.h>
#include <cryptopp/hkdf.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/secblock.h>
#include <cryptopp/sha.h>

#include <stdexcept>
#include <vector>

namespace fenris {
namespace common {

using namespace CryptoPP;

std::pair<std::vector<uint8_t>, EncryptionError>
encrypt_data_aes_gcm(const std::vector<uint8_t> &plaintext,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &iv)
{
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

/**
 * Generates an ECDH key pair using the NIST P-256 (secp256r1) curve.
 */
std::tuple<std::vector<uint8_t>, std::vector<uint8_t>, ECDHError>
generate_ecdh_keypair()
{
    try {
        // Use the NIST P-256 curve
        ECDH<ECP>::Domain domain(ASN1::secp256r1());

        // Generate a random private key
        AutoSeededRandomPool rng;
        SecByteBlock private_key(domain.PrivateKeyLength());
        domain.GeneratePrivateKey(rng, private_key);

        // Calculate the corresponding public key
        SecByteBlock public_key(domain.PublicKeyLength());
        domain.GeneratePublicKey(rng, private_key, public_key);

        std::vector<uint8_t> private_key_vec(private_key.begin(),
                                             private_key.end());
        std::vector<uint8_t> public_key_vec(public_key.begin(),
                                            public_key.end());

        return {private_key_vec, public_key_vec, ECDHError::SUCCESS};
    } catch (...) {
        return {std::vector<uint8_t>(),
                std::vector<uint8_t>(),
                ECDHError::KEY_GENERATION_FAILED};
    }
}

/**
 * Computes a shared secret using our private key and the peer's public key.
 */
std::pair<std::vector<uint8_t>, ECDHError>
compute_ecdh_shared_secret(const std::vector<uint8_t> &private_key,
                           const std::vector<uint8_t> &peer_public_key)
{
    try {
        // Use the NIST P-256 curve
        ECDH<ECP>::Domain domain(ASN1::secp256r1());

        // Convert from vector<uint8_t> to SecByteBlock
        SecByteBlock private_key_block(private_key.data(), private_key.size());
        SecByteBlock public_key_block(peer_public_key.data(),
                                      peer_public_key.size());

        // Compute the shared secret
        SecByteBlock shared_secret(domain.AgreedValueLength());
        AutoSeededRandomPool rng;

        // Make sure the key agreement succeeded
        if (!domain.Agree(shared_secret, private_key_block, public_key_block)) {
            return {std::vector<uint8_t>(), ECDHError::SHARED_SECRET_FAILED};
        }

        std::vector<uint8_t> shared_secret_vec(shared_secret.begin(),
                                               shared_secret.end());

        return {shared_secret_vec, ECDHError::SUCCESS};
    } catch (...) {
        return {std::vector<uint8_t>(), ECDHError::SHARED_SECRET_FAILED};
    }
}

/**
 * Derives an AES key from a shared secret using HKDF.
 */
std::pair<std::vector<uint8_t>, ECDHError>
derive_key_from_shared_secret(const std::vector<uint8_t> &shared_secret,
                              size_t key_size,
                              const std::vector<uint8_t> &context)
{
    try {
        if (key_size != 16 && key_size != 24 && key_size != 32) {
            return {std::vector<uint8_t>(), ECDHError::INVALID_KEY_SIZE};
        }

        // Default salt (can be empty for HKDF)
        std::vector<uint8_t> salt =
            {'f', 'e', 'n', 'r', 'i', 's', '-', 's', 'a', 'l', 't'};

        // Default info (can be customized via context parameter)
        std::vector<uint8_t> info = {'A', 'E', 'S', '-', 'K', 'e', 'y'};
        if (!context.empty()) {
            info.insert(info.end(), context.begin(), context.end());
        }

        // Output size: key_size (for AES key)
        const size_t output_size = key_size;

        // Derive key material using HKDF
        HKDF<SHA256> hkdf;
        SecByteBlock derived_key_material(output_size);

        hkdf.DeriveKey(derived_key_material,
                       derived_key_material.size(),
                       shared_secret.data(),
                       shared_secret.size(),
                       salt.data(),
                       salt.size(),
                       info.data(),
                       info.size());

        std::vector<uint8_t> derived_key(derived_key_material.begin(),
                                         derived_key_material.end());

        return {derived_key, ECDHError::SUCCESS};
    } catch (...) {
        return {std::vector<uint8_t>(), ECDHError::KEY_DERIVATION_FAILED};
    }
}

} // namespace common
} // namespace fenris
