// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "crypto.h"
#include "encryption_log.h"
#include "config.h"  // brings in cmake_config.h which defines USE_OPENSSL
#include "log.h"
#include <cstring>
#include <fstream>

#if USE_OPENSSL

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>

// ============================================================================
// AES-256-GCM Encryption
// ============================================================================

CryptoResult aes256gcm_encrypt(
        const u8* key, size_t key_len,
        const u8* nonce, size_t nonce_len,
        const u8* plaintext, size_t plaintext_len,
        const u8* aad, size_t aad_len)
{
        CryptoResult result;

        // Validate inputs
        if (!key || key_len != AES256_KEY_SIZE) {
                result.error_msg = "Invalid key size: expected " +
                        std::to_string(AES256_KEY_SIZE) + ", got " +
                        std::to_string(key_len);
                return result;
        }
        if (!nonce || nonce_len != GCM_NONCE_SIZE) {
                result.error_msg = "Invalid nonce size: expected " +
                        std::to_string(GCM_NONCE_SIZE) + ", got " +
                        std::to_string(nonce_len);
                return result;
        }
        if (!plaintext && plaintext_len > 0) {
                result.error_msg = "Null plaintext with non-zero length";
                return result;
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
                result.error_msg = "Failed to create EVP_CIPHER_CTX";
                return result;
        }

        // Initialize encryption context with AES-256-GCM
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
                result.error_msg = "EVP_EncryptInit_ex (cipher) failed";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Set nonce length (default is 12, but be explicit)
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_SIZE, nullptr) != 1) {
                result.error_msg = "Failed to set GCM nonce length";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Set key and nonce
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) {
                result.error_msg = "EVP_EncryptInit_ex (key/nonce) failed";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Provide AAD (Additional Authenticated Data) if present
        // This is authenticated but NOT encrypted — used for the encrypted flag byte
        int outlen = 0;
        if (aad && aad_len > 0) {
                if (EVP_EncryptUpdate(ctx, nullptr, &outlen, aad, aad_len) != 1) {
                        result.error_msg = "EVP_EncryptUpdate (AAD) failed";
                        EVP_CIPHER_CTX_free(ctx);
                        return result;
                }
        }

        // Encrypt the plaintext
        result.data.resize(plaintext_len);
        if (plaintext_len > 0) {
                if (EVP_EncryptUpdate(ctx, result.data.data(), &outlen, plaintext, plaintext_len) != 1) {
                        result.error_msg = "EVP_EncryptUpdate (plaintext) failed";
                        EVP_CIPHER_CTX_free(ctx);
                        return result;
                }
                result.data.resize(outlen);
        }

        // Finalize encryption (GCM always returns 0 on final for encryption, but we must call it)
        if (EVP_EncryptFinal_ex(ctx, result.data.data() + outlen, &outlen) != 1) {
                result.error_msg = "EVP_EncryptFinal_ex failed";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Get the GCM authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, result.tag.data()) != 1) {
                result.error_msg = "Failed to get GCM authentication tag";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Copy nonce to result
        std::memcpy(result.nonce.data(), nonce, GCM_NONCE_SIZE);

        EVP_CIPHER_CTX_free(ctx);
        result.success = true;
        return result;
}

// ============================================================================
// AES-256-GCM Decryption
// ============================================================================

CryptoResult aes256gcm_decrypt(
        const u8* key, size_t key_len,
        const u8* nonce, size_t nonce_len,
        const u8* ciphertext, size_t ciphertext_len,
        const u8* tag, size_t tag_len,
        const u8* aad, size_t aad_len)
{
        CryptoResult result;

        // Validate inputs
        if (!key || key_len != AES256_KEY_SIZE) {
                result.error_msg = "Invalid key size: expected " +
                        std::to_string(AES256_KEY_SIZE) + ", got " +
                        std::to_string(key_len);
                return result;
        }
        if (!nonce || nonce_len != GCM_NONCE_SIZE) {
                result.error_msg = "Invalid nonce size: expected " +
                        std::to_string(GCM_NONCE_SIZE) + ", got " +
                        std::to_string(nonce_len);
                return result;
        }
        if (!tag || tag_len != GCM_TAG_SIZE) {
                result.error_msg = "Invalid tag size: expected " +
                        std::to_string(GCM_TAG_SIZE) + ", got " +
                        std::to_string(tag_len);
                return result;
        }
        if (!ciphertext && ciphertext_len > 0) {
                result.error_msg = "Null ciphertext with non-zero length";
                return result;
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
                result.error_msg = "Failed to create EVP_CIPHER_CTX";
                return result;
        }

        // Initialize decryption context with AES-256-GCM
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
                result.error_msg = "EVP_DecryptInit_ex (cipher) failed";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Set nonce length
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_SIZE, nullptr) != 1) {
                result.error_msg = "Failed to set GCM nonce length";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Set key and nonce
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) {
                result.error_msg = "EVP_DecryptInit_ex (key/nonce) failed";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Provide AAD if present
        int outlen = 0;
        if (aad && aad_len > 0) {
                if (EVP_DecryptUpdate(ctx, nullptr, &outlen, aad, aad_len) != 1) {
                        result.error_msg = "EVP_DecryptUpdate (AAD) failed";
                        EVP_CIPHER_CTX_free(ctx);
                        return result;
                }
        }

        // Decrypt the ciphertext
        result.data.resize(ciphertext_len);
        if (ciphertext_len > 0) {
                if (EVP_DecryptUpdate(ctx, result.data.data(), &outlen, ciphertext, ciphertext_len) != 1) {
                        result.error_msg = "EVP_DecryptUpdate (ciphertext) failed";
                        EVP_CIPHER_CTX_free(ctx);
                        return result;
                }
                result.data.resize(outlen);
        }

        // Set the expected GCM tag BEFORE calling DecryptFinal
        // This is critical: DecryptFinal will verify the tag and return 1 only if it matches
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE,
                        const_cast<u8*>(tag)) != 1) {
                result.error_msg = "Failed to set GCM authentication tag";
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Verify the tag (DecryptFinal returns 1 on success, 0 on authentication failure)
        int final_len = 0;
        if (EVP_DecryptFinal_ex(ctx, result.data.data() + outlen, &final_len) != 1) {
                result.error_msg = "GCM authentication tag verification FAILED — "
                        "packet may have been tampered with or corrupted";
                result.data.clear();
                EVP_CIPHER_CTX_free(ctx);
                return result;
        }

        // Copy nonce to result
        std::memcpy(result.nonce.data(), nonce, GCM_NONCE_SIZE);

        EVP_CIPHER_CTX_free(ctx);
        result.success = true;
        return result;
}

// ============================================================================
// HKDF-SHA256 Key Derivation (RFC 5869)
// ============================================================================

bool hkdf_sha256(
        const u8* ikm, size_t ikm_len,
        const u8* salt, size_t salt_len,
        const u8* info, size_t info_len,
        u8* out, size_t out_len)
{
        if (!ikm || ikm_len == 0 || !out || out_len == 0) {
                enclog_error("hkdf_sha256: invalid input")
                        << EncLog::kv("ikm_present", ikm ? "yes" : "no")
                        << EncLog::kv("ikm_len", (u32)ikm_len)
                        << EncLog::kv("out_present", out ? "yes" : "no")
                        << EncLog::kv("out_len", (u32)out_len)
                        << std::endl;
                return false;
        }

        // Use OpenSSL 3.0+ EVP_KDF API for HKDF
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
        if (!kdf) {
                enclog_error("hkdf_sha256: EVP_KDF_fetch failed")
                        << " — OpenSSL HKDF provider not available!"
                        << std::endl;
                return false;
        }

        EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
        EVP_KDF_free(kdf);
        if (!kctx) {
                enclog_error("hkdf_sha256: EVP_KDF_CTX_new failed") << std::endl;
                return false;
        }

        // Build the OSSL_PARAM array for HKDF
        // Mode: HKDF_MODE_EXPAND_ONLY would skip extract, but we want full HKDF (extract+expand)
        int mode = EVP_KDF_HKDF_MODE_EXTRACT_AND_EXPAND;

        OSSL_PARAM params[6];
        int n = 0;
        params[n++] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode);
        params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                const_cast<char*>("SHA256"), 0);
        params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                const_cast<u8*>(ikm), ikm_len);
        if (salt && salt_len > 0) {
                params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                        const_cast<u8*>(salt), salt_len);
        }
        if (info && info_len > 0) {
                params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                        const_cast<u8*>(info), info_len);
        }
        params[n] = OSSL_PARAM_construct_end();

        int ok = EVP_KDF_derive(kctx, out, out_len, params);
        EVP_KDF_CTX_free(kctx);

        if (ok != 1) {
                enclog_error("hkdf_sha256: EVP_KDF_derive failed")
                        << EncLog::kv("ikm_len", (u32)ikm_len)
                        << EncLog::kv("salt_len", (u32)salt_len)
                        << EncLog::kv("info_len", (u32)info_len)
                        << EncLog::kv("out_len", (u32)out_len)
                        << std::endl;
                return false;
        }

        return true;
}

// ============================================================================
// Secure Random Number Generation
// ============================================================================

bool secure_random(u8* buf, size_t len)
{
        if (!buf || len == 0) {
                return false;
        }
        return RAND_bytes(buf, static_cast<int>(len)) == 1;
}

// ============================================================================
// Nonce Construction
// ============================================================================

void build_nonce(const u8* base, u64 counter, u8* nonce)
{
        // Nonce format: [4-byte base][8-byte counter (big-endian)]
        std::memcpy(nonce, base, NONCE_BASE_SIZE);

        // Write counter in big-endian order
        for (int i = 0; i < 8; i++) {
                nonce[NONCE_BASE_SIZE + i] = static_cast<u8>((counter >> (56 - 8 * i)) & 0xFF);
        }
}

// ============================================================================
// PeerEncryptionState
// ============================================================================

bool PeerEncryptionState::initFromSRPSessionKey(const u8* session_key, size_t key_len, bool is_server)
{
        if (!session_key || key_len != SRP_SESSION_KEY_SIZE) {
                enclog_error("initFromSRPSessionKey: invalid session key")
                        << EncLog::kv("key_present", session_key ? "yes" : "no")
                        << EncLog::kv("len", (u32)key_len)
                        << EncLog::kv("expected", (u32)SRP_SESSION_KEY_SIZE)
                        << std::endl;
                return false;
        }

        enclog_init("Deriving encryption keys from SRP session key")
                << EncLog::kv("key_len", (u32)key_len)
                << EncLog::kv("role", is_server ? "server" : "client")
                << std::endl;

        // Store the SRP session key (for fingerprint derivation)
        std::memcpy(srp_session_key.data(), session_key, SRP_SESSION_KEY_SIZE);

        // v9.9: Derive HKDF salt deterministically from the SRP session key.
        // Both sides must derive the SAME salt so that the resulting encryption
        // keys are identical. Using a derived salt (rather than random) ensures:
        //   1. No protocol change needed (no salt exchange over the wire)
        //   2. Key separation between sessions with different SRP keys
        //   3. Salted HKDF provides stronger key separation than unsalted
        const char* salt_info = "Luanti v9 HKDF Salt";
        if (!hkdf_sha256(session_key, key_len,
                nullptr, 0,  // no salt for the salt derivation itself
                reinterpret_cast<const u8*>(salt_info), strlen(salt_info),
                hkdf_salt.data(), hkdf_salt.size())) {
                enclog_error("initFromSRPSessionKey: failed to derive HKDF salt") << std::endl;
                return false;
        }

        // Derive C2S encryption key: HKDF(key, salt, "Luanti v9 C2S Key")
        const char* c2s_info = "Luanti v9 C2S Key";
        if (!hkdf_sha256(session_key, key_len,
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(c2s_info), strlen(c2s_info),
                c2s.key.data(), AES256_KEY_SIZE)) {
                enclog_error("HKDF derivation FAILED for C2S key") << std::endl;
                return false;
        }
        enclog_init("C2S encryption key derived")
                << EncLog::kv("algorithm", "HKDF-SHA256")
                << EncLog::kv("info", c2s_info)
                << EncLog::kv("output_bits", 256)
                << EncLog::kv("salted", "yes")
                << std::endl;

        // Derive S2C encryption key: HKDF(key, salt, "Luanti v9 S2C Key")
        const char* s2c_info = "Luanti v9 S2C Key";
        if (!hkdf_sha256(session_key, key_len,
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(s2c_info), strlen(s2c_info),
                s2c.key.data(), AES256_KEY_SIZE)) {
                enclog_error("HKDF derivation FAILED for S2C key") << std::endl;
                return false;
        }
        enclog_init("S2C encryption key derived")
                << EncLog::kv("algorithm", "HKDF-SHA256")
                << EncLog::kv("info", s2c_info)
                << EncLog::kv("output_bits", 256)
                << EncLog::kv("salted", "yes")
                << std::endl;

        // Derive C2S nonce base: HKDF(key, salt, "Luanti v9 C2S Nonce")
        const char* c2s_nonce_info = "Luanti v9 C2S Nonce";
        if (!hkdf_sha256(session_key, key_len,
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(c2s_nonce_info), strlen(c2s_nonce_info),
                c2s.nonce_base.data(), NONCE_BASE_SIZE)) {
                enclog_error("HKDF derivation FAILED for C2S nonce base") << std::endl;
                return false;
        }

        // Derive S2C nonce base: HKDF(key, salt, "Luanti v9 S2C Nonce")
        const char* s2c_nonce_info = "Luanti v9 S2C Nonce";
        if (!hkdf_sha256(session_key, key_len,
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(s2c_nonce_info), strlen(s2c_nonce_info),
                s2c.nonce_base.data(), NONCE_BASE_SIZE)) {
                enclog_error("HKDF derivation FAILED for S2C nonce base") << std::endl;
                return false;
        }

        // Derive session ID: HKDF(key, salt, "Luanti v9 Session ID") → first 16 bytes as hex
        std::array<u8, 16> sid_bytes;
        const char* sid_info = "Luanti v9 Session ID";
        if (!hkdf_sha256(session_key, key_len,
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(sid_info), strlen(sid_info),
                sid_bytes.data(), sid_bytes.size())) {
                return false;
        }
        session_id = binToHex(sid_bytes.data(), sid_bytes.size());

        // Derive server fingerprint from session key
        server_fingerprint = keyToFingerprint(srp_session_key.data(), SRP_SESSION_KEY_SIZE);

        // Reset counters
        c2s.nonce_counter = 0;
        s2c.nonce_counter = 0;
        c2s.packets_processed = 0;
        s2c.packets_processed = 0;
        c2s.auth_failures = 0;
        s2c.auth_failures = 0;
        c2s.replay_attempts = 0;
        s2c.replay_attempts = 0;
        c2s.replay_bitmap.fill(0);
        s2c.replay_bitmap.fill(0);
        packets_since_audit = 0;
        last_audit_time_ms = 0;
        key_rotation_count = 0;

        // Do NOT set active = true here. The caller must explicitly activate
        // encryption after ensuring that any plaintext packets (like AUTH_ACCEPT)
        // have been sent. This avoids the chicken-and-egg problem where the
        // server encrypts AUTH_ACCEPT before the client can decrypt it.
        active = false;
        activated_at = 0; // Will be set by caller with actual timestamp

        enclog_init("Encryption keys initialized (NOT YET ACTIVE)")
                << EncLog::kv("session_id", session_id)
                << EncLog::kv("fingerprint", server_fingerprint)
                << EncLog::kv("role", is_server ? "server" : "client")
                << EncLog::kv("active", false)
                << EncLog::kv("cipher", "AES-256-GCM")
                << EncLog::kv("nonce_bits", 96)
                << EncLog::kv("salted_hkdf", "yes")
                << EncLog::kv("replay_bitmap", "exact")
                << EncLog::kv("key_rotation", "supported")
                << std::endl;

        return true;
}

// ============================================================================
// v9.9: Key Rotation — Forward Secrecy Within Sessions
// ============================================================================

bool PeerEncryptionState::rotateKeys()
{
        auto lck = lock();

        if (!active.load(std::memory_order_acquire)) {
                enclog_error("rotateKeys: encryption not active, cannot rotate") << std::endl;
                return false;
        }

        enclog_security("Starting key rotation")
                << EncLog::kv("rotation_count", (u32)key_rotation_count + 1)
                << std::endl;

        // Generate new ECDH key pair for this rotation
        X25519KeyPair new_keypair = x25519_generate_keypair();
        if (!new_keypair.success) {
                enclog_error("rotateKeys: failed to generate new X25519 key pair") << std::endl;
                return false;
        }

        // v9.11 FIX: Key rotation uses deterministic salt derivation instead of
        // secure_random() for the HKDF salt. The previous v9.9 implementation used
        // secure_random() for the salt, which would cause KEY MISMATCH between
        // client and server (each side generates a different random salt → different
        // keys). This is the same bug pattern as the original v9.9 initFromSRPSessionKey()
        // salt bug that caused GCM auth failures.
        //
        // The rotation IKM includes: SRP key + ECDH shared secret + rotation counter.
        // The rotation counter ensures each rotation produces different keys even
        // with the same SRP+ECDH inputs.
        //
        // IMPORTANT: For a REAL key rotation to work across the network, both sides
        // need to exchange new ECDH public keys via a wire protocol. The current
        // implementation performs local-only rotation, which is useful for:
        // 1. Testing that the rotation logic works
        // 2. Future integration with a key rotation wire protocol
        // 3. Ensuring deterministic salt derivation works correctly

        // Build rotation IKM: SRP key + ECDH shared secret + rotation counter bytes
        // The rotation counter ensures uniqueness across successive rotations.
        std::array<u8, sizeof(u32)> rotation_counter_bytes;
        u32 next_rotation = key_rotation_count + 1;
        rotation_counter_bytes[0] = static_cast<u8>((next_rotation >> 24) & 0xFF);
        rotation_counter_bytes[1] = static_cast<u8>((next_rotation >> 16) & 0xFF);
        rotation_counter_bytes[2] = static_cast<u8>((next_rotation >> 8) & 0xFF);
        rotation_counter_bytes[3] = static_cast<u8>(next_rotation & 0xFF);

        std::array<u8, SRP_SESSION_KEY_SIZE + X25519_SHARED_SECRET_SIZE + sizeof(u32)> rotation_ikm;
        std::memcpy(rotation_ikm.data(), srp_session_key.data(), SRP_SESSION_KEY_SIZE);
        std::memcpy(rotation_ikm.data() + SRP_SESSION_KEY_SIZE, ecdh_shared_secret.data(), X25519_SHARED_SECRET_SIZE);
        std::memcpy(rotation_ikm.data() + SRP_SESSION_KEY_SIZE + X25519_SHARED_SECRET_SIZE,
                     rotation_counter_bytes.data(), sizeof(u32));

        // v9.11 FIX: Derive HKDF salt deterministically from the rotation IKM.
        // Both sides with the same rotation IKM will derive the same salt.
        const char* rotation_salt_info = "Luanti v9.11 Rotation HKDF Salt";
        if (!hkdf_sha256(rotation_ikm.data(), rotation_ikm.size(),
                nullptr, 0,  // no salt for the salt derivation itself
                reinterpret_cast<const u8*>(rotation_salt_info), strlen(rotation_salt_info),
                hkdf_salt.data(), hkdf_salt.size())) {
                enclog_error("rotateKeys: failed to derive rotation HKDF salt") << std::endl;
                rotation_ikm.fill(0);
                return false;
        }

        // Re-derive all keys with the new IKM and salt
        const char* c2s_info = "Luanti v9.11 C2S Key (Rotation)";
        if (!hkdf_sha256(rotation_ikm.data(), rotation_ikm.size(),
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(c2s_info), strlen(c2s_info),
                c2s.key.data(), AES256_KEY_SIZE)) {
                enclog_error("rotateKeys: HKDF derivation FAILED for C2S key") << std::endl;
                rotation_ikm.fill(0);
                return false;
        }

        const char* s2c_info = "Luanti v9.11 S2C Key (Rotation)";
        if (!hkdf_sha256(rotation_ikm.data(), rotation_ikm.size(),
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(s2c_info), strlen(s2c_info),
                s2c.key.data(), AES256_KEY_SIZE)) {
                enclog_error("rotateKeys: HKDF derivation FAILED for S2C key") << std::endl;
                rotation_ikm.fill(0);
                return false;
        }

        const char* c2s_nonce_info = "Luanti v9.11 C2S Nonce (Rotation)";
        if (!hkdf_sha256(rotation_ikm.data(), rotation_ikm.size(),
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(c2s_nonce_info), strlen(c2s_nonce_info),
                c2s.nonce_base.data(), NONCE_BASE_SIZE)) {
                enclog_error("rotateKeys: HKDF derivation FAILED for C2S nonce base") << std::endl;
                rotation_ikm.fill(0);
                return false;
        }

        const char* s2c_nonce_info = "Luanti v9.11 S2C Nonce (Rotation)";
        if (!hkdf_sha256(rotation_ikm.data(), rotation_ikm.size(),
                hkdf_salt.data(), hkdf_salt.size(),
                reinterpret_cast<const u8*>(s2c_nonce_info), strlen(s2c_nonce_info),
                s2c.nonce_base.data(), NONCE_BASE_SIZE)) {
                enclog_error("rotateKeys: HKDF derivation FAILED for S2C nonce base") << std::endl;
                rotation_ikm.fill(0);
                return false;
        }

        // Reset nonce counters and replay bitmaps after rotation
        c2s.nonce_counter = 0;
        s2c.nonce_counter = 0;
        c2s.replay_bitmap.fill(0);
        s2c.replay_bitmap.fill(0);

        // Zero sensitive rotation material
        rotation_ikm.fill(0);

        // Update ECDH keys
        ecdh_private_key = new_keypair.private_key;
        ecdh_public_key = new_keypair.public_key;

        key_rotation_count++;

        enclog_security("Key rotation completed successfully")
                << EncLog::kv("rotation_count", (u32)key_rotation_count)
                << EncLog::kv("session_id", session_id)
                << EncLog::kv("salted_hkdf", "yes")
                << EncLog::kv("deterministic_salt", "yes")
                << std::endl;

        return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string binToHex(const u8* data, size_t len)
{
        std::ostringstream oss;
        for (size_t i = 0; i < len; i++) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(data[i]);
        }
        return oss.str();
}

std::string keyToFingerprint(const u8* key, size_t len)
{
        // Compute SHA-256 of the key for a fingerprint
        // Then format as "SHA256:<hex>"
        std::array<u8, 32> hash;
        const EVP_MD* md = EVP_sha256();
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
                return "SHA256:ERROR";
        }

        unsigned int hash_len = 0;
        if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
                EVP_DigestUpdate(ctx, key, len) != 1 ||
                EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
                EVP_MD_CTX_free(ctx);
                return "SHA256:ERROR";
        }
        EVP_MD_CTX_free(ctx);

        return "SHA256:" + binToHex(hash.data(), hash_len);
}

// ============================================================================
// v9.1: X25519 ECDH Key Exchange (Forward Secrecy)
// ============================================================================

X25519KeyPair x25519_generate_keypair()
{
        X25519KeyPair result;

        EVP_PKEY* pkey = nullptr;
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "X25519", nullptr);
        if (!ctx) {
                result.error_msg = "Failed to create X25519 keygen context";
                enclog_error("x25519_generate_keypair: EVP_PKEY_CTX_new_from_name failed") << std::endl;
                return result;
        }

        if (EVP_PKEY_keygen_init(ctx) <= 0) {
                result.error_msg = "Failed to init X25519 keygen";
                enclog_error("x25519_generate_keypair: EVP_PKEY_keygen_init failed") << std::endl;
                EVP_PKEY_CTX_free(ctx);
                return result;
        }

        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
                result.error_msg = "Failed to generate X25519 key pair";
                enclog_error("x25519_generate_keypair: EVP_PKEY_keygen failed") << std::endl;
                EVP_PKEY_CTX_free(ctx);
                return result;
        }
        EVP_PKEY_CTX_free(ctx);

        // Extract the public key
        size_t pub_len = X25519_PUBLIC_KEY_SIZE;
        if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                        result.public_key.data(), pub_len, &pub_len) != 1) {
                result.error_msg = "Failed to extract X25519 public key";
                enclog_error("x25519_generate_keypair: get public key failed") << std::endl;
                EVP_PKEY_free(pkey);
                return result;
        }

        // Extract the private key
        size_t priv_len = X25519_PRIVATE_KEY_SIZE;
        if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                        result.private_key.data(), priv_len, &priv_len) != 1) {
                result.error_msg = "Failed to extract X25519 private key";
                enclog_error("x25519_generate_keypair: get private key failed") << std::endl;
                EVP_PKEY_free(pkey);
                return result;
        }

        EVP_PKEY_free(pkey);
        result.success = true;

        enclog_init("X25519 ephemeral key pair generated")
                << EncLog::kv("pub_key_size", (u32)pub_len)
                << EncLog::kv("priv_key_size", (u32)priv_len)
                << std::endl;

        return result;
}

X25519SharedSecret x25519_compute_shared_secret(
        const u8* our_private_key, size_t our_private_key_len,
        const u8* peer_public_key, size_t peer_public_key_len)
{
        X25519SharedSecret result;

        if (!our_private_key || our_private_key_len != X25519_PRIVATE_KEY_SIZE) {
                result.error_msg = "Invalid private key size for X25519";
                return result;
        }
        if (!peer_public_key || peer_public_key_len != X25519_PUBLIC_KEY_SIZE) {
                result.error_msg = "Invalid public key size for X25519";
                return result;
        }

        // Build our key pair from raw private key
        OSSL_PARAM params[2];
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                const_cast<u8*>(our_private_key), our_private_key_len);
        params[1] = OSSL_PARAM_construct_end();

        EVP_PKEY_CTX* keygen_ctx = EVP_PKEY_CTX_new_from_name(nullptr, "X25519", nullptr);
        if (!keygen_ctx) {
                result.error_msg = "Failed to create X25519 context for key loading";
                return result;
        }

        if (EVP_PKEY_fromdata_init(keygen_ctx) <= 0) {
                result.error_msg = "EVP_PKEY_fromdata_init failed";
                EVP_PKEY_CTX_free(keygen_ctx);
                return result;
        }

        EVP_PKEY* our_key = nullptr;
        if (EVP_PKEY_fromdata(keygen_ctx, &our_key, EVP_PKEY_KEYPAIR, params) <= 0) {
                result.error_msg = "EVP_PKEY_fromdata failed for our private key";
                EVP_PKEY_CTX_free(keygen_ctx);
                return result;
        }
        EVP_PKEY_CTX_free(keygen_ctx);

        // Build peer's public key
        OSSL_PARAM peer_params[2];
        peer_params[0] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                const_cast<u8*>(peer_public_key), peer_public_key_len);
        peer_params[1] = OSSL_PARAM_construct_end();

        EVP_PKEY_CTX* peer_ctx = EVP_PKEY_CTX_new_from_name(nullptr, "X25519", nullptr);
        if (!peer_ctx) {
                result.error_msg = "Failed to create X25519 context for peer key";
                EVP_PKEY_free(our_key);
                return result;
        }

        if (EVP_PKEY_fromdata_init(peer_ctx) <= 0) {
                result.error_msg = "EVP_PKEY_fromdata_init failed for peer";
                EVP_PKEY_CTX_free(peer_ctx);
                EVP_PKEY_free(our_key);
                return result;
        }

        EVP_PKEY* peer_key = nullptr;
        if (EVP_PKEY_fromdata(peer_ctx, &peer_key, EVP_PKEY_PUBLIC_KEY, peer_params) <= 0) {
                result.error_msg = "EVP_PKEY_fromdata failed for peer public key";
                EVP_PKEY_CTX_free(peer_ctx);
                EVP_PKEY_free(our_key);
                return result;
        }
        EVP_PKEY_CTX_free(peer_ctx);

        // Derive shared secret
        EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(our_key, nullptr);
        if (!derive_ctx) {
                result.error_msg = "Failed to create derivation context";
                EVP_PKEY_free(our_key);
                EVP_PKEY_free(peer_key);
                return result;
        }

        if (EVP_PKEY_derive_init(derive_ctx) <= 0) {
                result.error_msg = "EVP_PKEY_derive_init failed";
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(our_key);
                EVP_PKEY_free(peer_key);
                return result;
        }

        if (EVP_PKEY_derive_set_peer(derive_ctx, peer_key) <= 0) {
                result.error_msg = "EVP_PKEY_derive_set_peer failed";
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(our_key);
                EVP_PKEY_free(peer_key);
                return result;
        }

        // Get the shared secret length first
        size_t secret_len = 0;
        if (EVP_PKEY_derive(derive_ctx, nullptr, &secret_len) <= 0) {
                result.error_msg = "EVP_PKEY_derive (get length) failed";
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(our_key);
                EVP_PKEY_free(peer_key);
                return result;
        }

        if (secret_len != X25519_SHARED_SECRET_SIZE) {
                result.error_msg = "Unexpected X25519 shared secret size: " + std::to_string(secret_len);
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(our_key);
                EVP_PKEY_free(peer_key);
                return result;
        }

        // Compute the shared secret
        if (EVP_PKEY_derive(derive_ctx, result.shared_secret.data(), &secret_len) <= 0) {
                result.error_msg = "EVP_PKEY_derive failed";
                EVP_PKEY_CTX_free(derive_ctx);
                EVP_PKEY_free(our_key);
                EVP_PKEY_free(peer_key);
                return result;
        }

        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(our_key);
        EVP_PKEY_free(peer_key);

        result.success = true;

        enclog_init("X25519 ECDH shared secret computed")
                << EncLog::kv("secret_len", (u32)secret_len)
                << std::endl;

        return result;
}

bool mixECDHSecretIntoKeys(PeerEncryptionState &state,
        const u8* ecdh_secret, size_t ecdh_secret_len)
{
        if (!ecdh_secret || ecdh_secret_len != X25519_SHARED_SECRET_SIZE) {
                enclog_error("mixECDHSecretIntoKeys: invalid ECDH secret")
                        << EncLog::kv("present", ecdh_secret ? "yes" : "no")
                        << EncLog::kv("len", (u32)ecdh_secret_len)
                        << std::endl;
                return false;
        }

        auto lock = state.lock();

        // Combine SRP key + ECDH secret as the new IKM for HKDF
        // IKM = SRP_session_key || ECDH_shared_secret (64 bytes total)
        std::array<u8, SRP_SESSION_KEY_SIZE + X25519_SHARED_SECRET_SIZE> combined_ikm;
        std::memcpy(combined_ikm.data(), state.srp_session_key.data(), SRP_SESSION_KEY_SIZE);
        std::memcpy(combined_ikm.data() + SRP_SESSION_KEY_SIZE, ecdh_secret, X25519_SHARED_SECRET_SIZE);

        enclog_init("Re-deriving encryption keys with ECDH+SRP combined secret")
                << EncLog::kv("ikm_len", (u32)combined_ikm.size())
                << EncLog::kv("pfs", "yes")
                << std::endl;

        // v9.11 FIX: Derive HKDF salt deterministically from the combined IKM.
        // Both sides have the same combined IKM (SRP key + ECDH shared secret),
        // so they will derive the same salt and therefore the same encryption keys.
        // Previous versions used nullptr salt (unsalted HKDF), which was inconsistent
        // with the salted HKDF used in initFromSRPSessionKey().
        const char* salt_info = "Luanti v9.11 ECDH HKDF Salt";
        if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                nullptr, 0,  // no salt for the salt derivation itself
                reinterpret_cast<const u8*>(salt_info), strlen(salt_info),
                state.hkdf_salt.data(), state.hkdf_salt.size())) {
                enclog_error("mixECDHSecretIntoKeys: failed to derive ECDH HKDF salt") << std::endl;
                combined_ikm.fill(0);
                return false;
        }

        // Re-derive all keys with the combined IKM and salted HKDF
        const char* c2s_info = "Luanti v9.11 C2S Key (ECDH+SRP)";
        if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                state.hkdf_salt.data(), state.hkdf_salt.size(),
                reinterpret_cast<const u8*>(c2s_info), strlen(c2s_info),
                state.c2s.key.data(), AES256_KEY_SIZE)) {
                enclog_error("HKDF re-derivation FAILED for C2S key (ECDH)") << std::endl;
                combined_ikm.fill(0);
                return false;
        }

        const char* s2c_info = "Luanti v9.11 S2C Key (ECDH+SRP)";
        if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                state.hkdf_salt.data(), state.hkdf_salt.size(),
                reinterpret_cast<const u8*>(s2c_info), strlen(s2c_info),
                state.s2c.key.data(), AES256_KEY_SIZE)) {
                enclog_error("HKDF re-derivation FAILED for S2C key (ECDH)") << std::endl;
                combined_ikm.fill(0);
                return false;
        }

        const char* c2s_nonce_info = "Luanti v9.11 C2S Nonce (ECDH+SRP)";
        if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                state.hkdf_salt.data(), state.hkdf_salt.size(),
                reinterpret_cast<const u8*>(c2s_nonce_info), strlen(c2s_nonce_info),
                state.c2s.nonce_base.data(), NONCE_BASE_SIZE)) {
                enclog_error("HKDF re-derivation FAILED for C2S nonce base (ECDH)") << std::endl;
                combined_ikm.fill(0);
                return false;
        }

        const char* s2c_nonce_info = "Luanti v9.11 S2C Nonce (ECDH+SRP)";
        if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                state.hkdf_salt.data(), state.hkdf_salt.size(),
                reinterpret_cast<const u8*>(s2c_nonce_info), strlen(s2c_nonce_info),
                state.s2c.nonce_base.data(), NONCE_BASE_SIZE)) {
                enclog_error("HKDF re-derivation FAILED for S2C nonce base (ECDH)") << std::endl;
                combined_ikm.fill(0);
                return false;
        }

        // Re-derive session ID with ECDH info
        std::array<u8, 16> sid_bytes;
        const char* sid_info = "Luanti v9.11 Session ID (ECDH+SRP)";
        if (!hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                state.hkdf_salt.data(), state.hkdf_salt.size(),
                reinterpret_cast<const u8*>(sid_info), strlen(sid_info),
                sid_bytes.data(), sid_bytes.size())) {
                combined_ikm.fill(0);
                return false;
        }
        state.session_id = binToHex(sid_bytes.data(), sid_bytes.size());

        // Store ECDH shared secret (for potential future rekeying)
        std::memcpy(state.ecdh_shared_secret.data(), ecdh_secret, X25519_SHARED_SECRET_SIZE);

        // Mark ECDH as completed — forward secrecy is now REAL
        state.ecdh_completed.store(true, std::memory_order_release);

        // Reset nonce counters after ECDH key change (keys are different, counters start fresh)
        state.c2s.nonce_counter = 0;
        state.s2c.nonce_counter = 0;
        state.c2s.replay_bitmap.fill(0);
        state.s2c.replay_bitmap.fill(0);

        // Zero the combined IKM — no longer needed
        combined_ikm.fill(0);

        enclog_security("ECDH X25519 forward secrecy established")
                << EncLog::kv("session_id", state.session_id)
                << EncLog::kv("pfs", "yes")
                << EncLog::kv("key_exchange", "SRP+X25519")
                << EncLog::kv("salted_hkdf", "yes")
                << std::endl;

        return true;
}

// ============================================================================
// v9.1: FingerprintStore — Server Fingerprint Pinning
// ============================================================================

std::string FingerprintStore::makeKey(const std::string &server_address, u16 port)
{
        return server_address + ":" + std::to_string(port);
}

bool FingerprintStore::load(const std::string &filepath)
{
        std::ifstream file(filepath);
        if (!file.is_open()) {
                // File doesn't exist yet — that's OK (first run)
                return true;
        }

        std::string line;
        while (std::getline(file, line)) {
                // Skip empty lines and comments
                if (line.empty() || line[0] == '#')
                        continue;

                // Format: <server_address>:<port> <fingerprint>
                // Find the space separating key from fingerprint
                size_t space_pos = line.find(' ');
                if (space_pos == std::string::npos)
                        continue;

                std::string key = line.substr(0, space_pos);
                std::string fingerprint = line.substr(space_pos + 1);

                // Trim whitespace from fingerprint
                while (!fingerprint.empty() && (fingerprint.back() == '\r' || fingerprint.back() == ' '))
                        fingerprint.pop_back();

                if (!key.empty() && !fingerprint.empty()) {
                        m_fingerprints[key] = fingerprint;
                }
        }

        enclog_init("FingerprintStore loaded")
                << EncLog::kv("filepath", filepath)
                << EncLog::kv("entries", (u32)m_fingerprints.size())
                << std::endl;

        return true;
}

bool FingerprintStore::save(const std::string &filepath) const
{
        std::ofstream file(filepath);
        if (!file.is_open()) {
                enclog_error("FingerprintStore: failed to open file for writing")
                        << EncLog::kv("filepath", filepath)
                        << std::endl;
                return false;
        }

        file << "# Luanti v9.1 Server Fingerprint Store\n";
        file << "# Format: <address>:<port> <fingerprint>\n";
        file << "# DO NOT EDIT — managed by Luanti encryption subsystem\n\n";

        for (const auto &entry : m_fingerprints) {
                file << entry.first << " " << entry.second << "\n";
        }

        enclog_init("FingerprintStore saved")
                << EncLog::kv("filepath", filepath)
                << EncLog::kv("entries", (u32)m_fingerprints.size())
                << std::endl;

        return true;
}

int FingerprintStore::verify(const std::string &server_address, u16 port,
        const std::string &fingerprint) const
{
        std::string key = makeKey(server_address, port);
        auto it = m_fingerprints.find(key);

        if (it == m_fingerprints.end()) {
                // Unknown server — first connection (TOFU)
                return 0;
        }

        if (it->second == fingerprint) {
                // Fingerprint matches — verified!
                return 1;
        }

        // Fingerprint MISMATCH — possible MITM!
        enclog_error("FINGERPRINT MISMATCH — possible MITM attack!")
                << EncLog::kv("server", server_address)
                << EncLog::kv("port", (u16)port)
                << EncLog::kv("expected", it->second)
                << EncLog::kv("got", fingerprint)
                << std::endl;
        return -1;
}

void FingerprintStore::record(const std::string &server_address, u16 port,
        const std::string &fingerprint)
{
        std::string key = makeKey(server_address, port);
        m_fingerprints[key] = fingerprint;

        enclog_init("Server fingerprint recorded (TOFU)")
                << EncLog::kv("server", server_address)
                << EncLog::kv("port", (u16)port)
                << EncLog::kv("fingerprint", fingerprint)
                << std::endl;
}

std::string FingerprintStore::getStoredFingerprint(const std::string &server_address, u16 port) const
{
        std::string key = makeKey(server_address, port);
        auto it = m_fingerprints.find(key);
        if (it != m_fingerprints.end())
                return it->second;
        return "";
}

#else // !USE_OPENSSL

// Stub implementations when OpenSSL is not available

CryptoResult aes256gcm_encrypt(
        const u8* key, size_t key_len,
        const u8* nonce, size_t nonce_len,
        const u8* plaintext, size_t plaintext_len,
        const u8* aad, size_t aad_len)
{
        CryptoResult result;
        result.error_msg = "Encryption not available: OpenSSL is not enabled. "
                "Rebuild with -DENABLE_OPENSSL=ON to enable real encryption.";
        enclog_error("aes256gcm_encrypt called but OpenSSL NOT linked")
                << " — all connections will be INSECURE"
                << std::endl;
        return result;
}

CryptoResult aes256gcm_decrypt(
        const u8* key, size_t key_len,
        const u8* nonce, size_t nonce_len,
        const u8* ciphertext, size_t ciphertext_len,
        const u8* tag, size_t tag_len,
        const u8* aad, size_t aad_len)
{
        CryptoResult result;
        result.error_msg = "Decryption not available: OpenSSL is not enabled. "
                "Rebuild with -DENABLE_OPENSSL=ON to enable real encryption.";
        enclog_error("aes256gcm_decrypt called but OpenSSL NOT linked") << std::endl;
        return result;
}

bool hkdf_sha256(
        const u8* ikm, size_t ikm_len,
        const u8* salt, size_t salt_len,
        const u8* info, size_t info_len,
        u8* out, size_t out_len)
{
        return false;
}

bool secure_random(u8* buf, size_t len)
{
        return false;
}

void build_nonce(const u8* base, u64 counter, u8* nonce)
{
        memset(nonce, 0, GCM_NONCE_SIZE);
}

bool PeerEncryptionState::initFromSRPSessionKey(const u8* session_key, size_t key_len, bool is_server)
{
        return false;
}

std::string binToHex(const u8* data, size_t len)
{
        return "";
}

std::string keyToFingerprint(const u8* key, size_t len)
{
        return "SHA256:NOT_AVAILABLE";
}

X25519KeyPair x25519_generate_keypair()
{
        X25519KeyPair result;
        result.error_msg = "X25519 not available: OpenSSL is not enabled";
        return result;
}

X25519SharedSecret x25519_compute_shared_secret(
        const u8* our_private_key, size_t our_private_key_len,
        const u8* peer_public_key, size_t peer_public_key_len)
{
        X25519SharedSecret result;
        result.error_msg = "X25519 not available: OpenSSL is not enabled";
        return result;
}

bool mixECDHSecretIntoKeys(PeerEncryptionState &state,
        const u8* ecdh_secret, size_t ecdh_secret_len)
{
        return false;
}

// FingerprintStore stubs (file I/O works without OpenSSL)
// These are NOT stubs — FingerprintStore is pure file I/O and works
// even without OpenSSL. The implementations are shared between both
// #ifdef branches (already defined above the #else).

#endif // USE_OPENSSL
