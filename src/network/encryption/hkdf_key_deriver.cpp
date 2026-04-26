// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/encryption/hkdf_key_deriver.h"
#include "network/crypto.h"
#include "encryption_log.h"
#include <cstring>

bool HKDFKeyDeriver::deriveKeys(
    const u8* ikm, size_t ikm_len,
    bool is_server,
    const std::string& version_info,
    DerivedDirectionalKeys& out)
{
    if (!ikm || ikm_len == 0) {
        enclog_error("HKDFKeyDeriver::deriveKeys: invalid IKM")
            << EncLog::kv("ikm_present", ikm ? "yes" : "no")
            << EncLog::kv("ikm_len", (u32)ikm_len)
            << std::endl;
        return false;
    }

    enclog_init("Deriving directional encryption keys via HKDFKeyDeriver")
        << EncLog::kv("ikm_len", (u32)ikm_len)
        << EncLog::kv("role", is_server ? "server" : "client")
        << EncLog::kv("version_info", version_info)
        << std::endl;

    // Step 1: Derive salt deterministically from IKM.
    // Both sides must derive the SAME salt so that the resulting encryption
    // keys are identical. Using a derived salt (rather than random) ensures:
    //   1. No protocol change needed (no salt exchange over the wire)
    //   2. Key separation between sessions with different IKM
    //   3. Salted HKDF provides stronger key separation than unsalted
    std::string salt_info_str = version_info + " Salt";
    std::array<u8, 16> hkdf_salt;
    if (!hkdf_sha256(ikm, ikm_len,
            nullptr, 0,  // no salt for the salt derivation itself
            reinterpret_cast<const u8*>(salt_info_str.c_str()), salt_info_str.size(),
            hkdf_salt.data(), hkdf_salt.size())) {
        enclog_error("HKDFKeyDeriver: failed to derive HKDF salt") << std::endl;
        return false;
    }

    // Step 2: Derive C2S encryption key
    std::string c2s_key_info = version_info + " C2S Key";
    if (!hkdf_sha256(ikm, ikm_len,
            hkdf_salt.data(), hkdf_salt.size(),
            reinterpret_cast<const u8*>(c2s_key_info.c_str()), c2s_key_info.size(),
            out.c2s_key.data(), AES256_KEY_SIZE_D)) {
        enclog_error("HKDFKeyDeriver: HKDF derivation FAILED for C2S key") << std::endl;
        return false;
    }
    enclog_init("C2S encryption key derived")
        << EncLog::kv("algorithm", "HKDF-SHA256")
        << EncLog::kv("info", c2s_key_info)
        << EncLog::kv("output_bits", 256)
        << EncLog::kv("salted", "yes")
        << std::endl;

    // Step 3: Derive S2C encryption key
    std::string s2c_key_info = version_info + " S2C Key";
    if (!hkdf_sha256(ikm, ikm_len,
            hkdf_salt.data(), hkdf_salt.size(),
            reinterpret_cast<const u8*>(s2c_key_info.c_str()), s2c_key_info.size(),
            out.s2c_key.data(), AES256_KEY_SIZE_D)) {
        enclog_error("HKDFKeyDeriver: HKDF derivation FAILED for S2C key") << std::endl;
        return false;
    }
    enclog_init("S2C encryption key derived")
        << EncLog::kv("algorithm", "HKDF-SHA256")
        << EncLog::kv("info", s2c_key_info)
        << EncLog::kv("output_bits", 256)
        << EncLog::kv("salted", "yes")
        << std::endl;

    // Step 4: Derive C2S nonce base
    std::string c2s_nonce_info = version_info + " C2S Nonce";
    if (!hkdf_sha256(ikm, ikm_len,
            hkdf_salt.data(), hkdf_salt.size(),
            reinterpret_cast<const u8*>(c2s_nonce_info.c_str()), c2s_nonce_info.size(),
            out.c2s_nonce_base.data(), NONCE_BASE_SIZE_D)) {
        enclog_error("HKDFKeyDeriver: HKDF derivation FAILED for C2S nonce base") << std::endl;
        return false;
    }

    // Step 5: Derive S2C nonce base
    std::string s2c_nonce_info = version_info + " S2C Nonce";
    if (!hkdf_sha256(ikm, ikm_len,
            hkdf_salt.data(), hkdf_salt.size(),
            reinterpret_cast<const u8*>(s2c_nonce_info.c_str()), s2c_nonce_info.size(),
            out.s2c_nonce_base.data(), NONCE_BASE_SIZE_D)) {
        enclog_error("HKDFKeyDeriver: HKDF derivation FAILED for S2C nonce base") << std::endl;
        return false;
    }

    // Step 6: Derive session ID (first 16 bytes as hex)
    std::array<u8, 16> sid_bytes;
    std::string sid_info = version_info + " Session ID";
    if (!hkdf_sha256(ikm, ikm_len,
            hkdf_salt.data(), hkdf_salt.size(),
            reinterpret_cast<const u8*>(sid_info.c_str()), sid_info.size(),
            sid_bytes.data(), sid_bytes.size())) {
        return false;
    }
    out.session_id = binToHex(sid_bytes.data(), sid_bytes.size());

    // Step 7: Derive server fingerprint from C2S key
    out.server_fingerprint = keyToFingerprint(out.c2s_key.data(), AES256_KEY_SIZE_D);

    enclog_init("Directional keys derived successfully")
        << EncLog::kv("session_id", out.session_id)
        << EncLog::kv("fingerprint", out.server_fingerprint)
        << EncLog::kv("role", is_server ? "server" : "client")
        << EncLog::kv("salted_hkdf", "yes")
        << std::endl;

    return true;
}
