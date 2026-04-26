// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/crypto_handler.h"
#include "network/crypto.h"
#include "network/encryption_log.h"
#include "network/networkprotocol.h"
#include <cstring>

DecryptResult CryptoHandler::decrypt(
        const EncryptedPacket& packet,
        PeerEncryptionState& enc_state,
        bool we_are_server)
{
        DecryptResult result;
        result.nonce_counter = packet.nonce_counter;

        // Lock the encryption state for thread-safe access
        auto lock = enc_state.lock();

        // Determine which direction key to use for decryption.
        // If we're the server, we receive packets encrypted with C2S key.
        // If we're the client, we receive packets encrypted with S2C key.
        DirectionalEncryptionState &dir_state =
                we_are_server ? enc_state.c2s : enc_state.s2c;

        const char *direction = we_are_server ? "C2S" : "S2C";

        // v9.19-trace: Log full decrypt attempt details
        enclog_trace("decrypt: ATTEMPT")
                << EncLog::kv("direction", direction)
                << EncLog::kv("we_are_server", we_are_server)
                << EncLog::kv("packet_nonce_counter", packet.nonce_counter)
                << EncLog::kv("local_nonce_counter", dir_state.nonce_counter)
                << EncLog::kv("ciphertext_len", (u32)packet.ciphertext.size())
                << EncLog::kv("nonce_hex", EncLog::hexDump(packet.nonce.data(), packet.nonce.size()))
                << EncLog::kv("tag_hex", EncLog::hexDump(packet.tag.data(), packet.tag.size()))
                << EncLog::kv("key_fp", keyToFingerprint(dir_state.key.data(), AES256_KEY_SIZE))
                << EncLog::kv("nonce_base_hex", EncLog::hexDump(dir_state.nonce_base.data(), dir_state.nonce_base.size()))
                << EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
                << EncLog::kv("active", enc_state.active.load())
                << EncLog::kv("session_id", enc_state.session_id)
                << std::endl;

        // Check that decryption key is initialized (not all zeros).
        // This can happen if we receive an encrypted packet before
        // our local key derivation is complete (e.g., during the
        // ECDH handshake when the server activates before the client).
        bool key_initialized = false;
        for (size_t i = 0; i < dir_state.key.size(); i++) {
                if (dir_state.key[i] != 0) {
                        key_initialized = true;
                        break;
                }
        }
        if (!key_initialized) {
                enclog_trace("decrypt: KEY NOT INITIALIZED — all zeros")
                        << EncLog::kv("direction", direction)
                        << EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
                        << std::endl;
                result.status = DecryptResult::KeyNotInitialized;
                result.error_detail = "Decryption key not initialized (key derivation may not have completed)";
                return result;
        }

        // Replay protection check
        if (!dir_state.isNotReplay(packet.nonce_counter)) {
                dir_state.replay_attempts++;
                enclog_trace("decrypt: REPLAY DETECTED")
                        << EncLog::kv("direction", direction)
                        << EncLog::kv("received_counter", packet.nonce_counter)
                        << EncLog::kv("expected_counter", dir_state.nonce_counter)
                        << std::endl;
                result.status = DecryptResult::ReplayDetected;
                result.error_detail = "Replay detected (counter=" +
                        std::to_string(packet.nonce_counter) +
                        ", expected=" + std::to_string(dir_state.nonce_counter) + ")";
                return result;
        }

        // AAD: the encrypted flag byte
        u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

        enclog_trace("decrypt: calling aes256gcm_decrypt")
                << EncLog::kv("direction", direction)
                << EncLog::kv("key_hex_first8", EncLog::hexDump(dir_state.key.data(), 8))
                << EncLog::kv("nonce_hex", EncLog::hexDump(packet.nonce.data(), packet.nonce.size()))
                << EncLog::kv("ciphertext_len", (u32)packet.ciphertext.size())
                << EncLog::kv("ciphertext_hex_first16", EncLog::hexDump(packet.ciphertext.data(), packet.ciphertext.size(), 16))
                << EncLog::kv("tag_hex", EncLog::hexDump(packet.tag.data(), packet.tag.size()))
                << EncLog::kv("aad", EncLog::hexByte(encrypted_flag))
                << std::endl;

        // Decrypt
        CryptoResult crypto_result = aes256gcm_decrypt(
                dir_state.key.data(), dir_state.key.size(),
                packet.nonce.data(), GCM_NONCE_SIZE,
                packet.ciphertext.data(), packet.ciphertext.size(),
                packet.tag.data(), GCM_TAG_SIZE,
                &encrypted_flag, 1);

        if (crypto_result.success) {
                // Decryption succeeded — return plaintext
                result.status = DecryptResult::Success;
                result.plaintext = SharedBuffer<u8>(crypto_result.data.size());
                memcpy(*result.plaintext, crypto_result.data.data(), crypto_result.data.size());

                // Update high-water mark counter
                dir_state.updateCounter(packet.nonce_counter);

                // Mark this counter as received in the replay bitmap
                dir_state.markReceived(packet.nonce_counter);

                dir_state.packets_processed++;

                enclog_trace("decrypt: SUCCESS")
                        << EncLog::kv("direction", direction)
                        << EncLog::kv("plaintext_len", (u32)crypto_result.data.size())
                        << EncLog::kv("counter_after", dir_state.nonce_counter)
                        << std::endl;
        } else {
                // Decryption failed — authentication tag mismatch
                dir_state.auth_failures++;
                result.status = DecryptResult::AuthFailed;
                result.error_detail = crypto_result.error_msg;
                result.auth_failure_count = dir_state.auth_failures;
                result.expected_nonce_counter = dir_state.nonce_counter;

                enclog_trace("decrypt: GCM AUTH FAILED")
                        << EncLog::kv("direction", direction)
                        << EncLog::kv("failure_num", dir_state.auth_failures)
                        << EncLog::kv("error", crypto_result.error_msg)
                        << EncLog::kv("key_fp_full", keyToFingerprint(dir_state.key.data(), AES256_KEY_SIZE))
                        << EncLog::kv("nonce_base_hex", EncLog::hexDump(dir_state.nonce_base.data(), dir_state.nonce_base.size()))
                        << EncLog::kv("s2c_key_fp", keyToFingerprint(enc_state.s2c.key.data(), AES256_KEY_SIZE))
                        << EncLog::kv("c2s_key_fp", keyToFingerprint(enc_state.c2s.key.data(), AES256_KEY_SIZE))
                        << EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
                        << EncLog::kv("active", enc_state.active.load())
                        << EncLog::kv("hkdf_salt_hex", EncLog::hexDump(enc_state.hkdf_salt.data(), enc_state.hkdf_salt.size()))
                        << std::endl;
        }

        return result;
}

EncryptResult CryptoHandler::encrypt(
        const u8* plaintext, size_t plaintext_len,
        PeerEncryptionState& enc_state,
        bool we_are_server)
{
        EncryptResult result;

        // Check if encryption is active
        if (!enc_state.active.load(std::memory_order_acquire)) {
                result.status = EncryptResult::NotActive;
                result.error_detail = "Encryption not active for this peer";
                return result;
        }

        // Lock the encryption state for thread-safe access
        auto lock = enc_state.lock();

        // Determine which direction key to use for encryption.
        // If we're the server, we send packets encrypted with S2C key.
        // If we're the client, we send packets encrypted with C2S key.
        DirectionalEncryptionState &dir_state =
                we_are_server ? enc_state.s2c : enc_state.c2s;

        const char *direction = we_are_server ? "S2C" : "C2S";

        // Build nonce
        result.nonce = dir_state.nextNonce();

        // AAD: the encrypted flag byte
        u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

        // v9.19-trace: Log encryption attempt with full key details
        enclog_trace("encrypt: SENDING ENCRYPTED PACKET")
                << EncLog::kv("direction", direction)
                << EncLog::kv("we_are_server", we_are_server)
                << EncLog::kv("plaintext_len", (u32)plaintext_len)
                << EncLog::kv("nonce_hex", EncLog::hexDump(result.nonce.data(), result.nonce.size()))
                << EncLog::kv("nonce_counter", dir_state.nonce_counter)
                << EncLog::kv("key_fp", keyToFingerprint(dir_state.key.data(), AES256_KEY_SIZE))
                << EncLog::kv("nonce_base_hex", EncLog::hexDump(dir_state.nonce_base.data(), dir_state.nonce_base.size()))
                << EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
                << EncLog::kv("active", enc_state.active.load())
                << EncLog::kv("session_id", enc_state.session_id)
                << EncLog::kv("aad", EncLog::hexByte(encrypted_flag))
                << std::endl;

        // Encrypt
        CryptoResult crypto_result = aes256gcm_encrypt(
                dir_state.key.data(), dir_state.key.size(),
                result.nonce.data(), result.nonce.size(),
                plaintext, plaintext_len,
                &encrypted_flag, 1);

        if (crypto_result.success) {
                result.status = EncryptResult::Success;
                result.ciphertext = std::move(crypto_result.data);
                result.tag = crypto_result.tag;
        } else {
                result.status = EncryptResult::InternalError;
                result.error_detail = crypto_result.error_msg;
                enclog_trace("encrypt: ENCRYPTION FAILED")
                        << EncLog::kv("direction", direction)
                        << EncLog::kv("error", crypto_result.error_msg)
                        << std::endl;
        }

        return result;
}

bool CryptoHandler::isEncryptionActive(const PeerEncryptionState& enc_state)
{
        return enc_state.active.load(std::memory_order_acquire);
}

void CryptoHandler::logDecryptFailure(const DecryptResult& result,
        session_t peer_id, const PeerEncryptionState& enc_state,
        bool we_are_server)
{
        switch (result.status) {
        case DecryptResult::KeyNotInitialized:
                enclog_error("Received encrypted packet but decryption key not initialized")
                        << EncLog::kv("peer", peer_id)
                        << EncLog::kv("action", "PACKET_DROPPED")
                        << EncLog::kv("hint", "key_derivation_may_not_have_completed_yet")
                        << std::endl;
                break;

        case DecryptResult::ReplayDetected:
                enclog_error("Replay detected from peer")
                        << EncLog::kv("peer", peer_id)
                        << EncLog::kv("received_counter", result.nonce_counter)
                        << EncLog::kv("expected_counter", result.expected_nonce_counter)
                        << EncLog::kv("action", "PACKET_DROPPED")
                        << std::endl;
                break;

        case DecryptResult::AuthFailed: {
                const u64 count = result.auth_failure_count;
                const char *direction = we_are_server ? "C2S" : "S2C";

                // v9.19: Throttled GCM auth failure logging.
                // Previous versions logged EVERY failure at ERROR level, causing
                // massive log spam when a key mismatch occurred.
                if (count <= 3) {
                        // Full diagnostic dump for the first few failures
                        enclog_error("GCM auth FAILED from peer")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("direction", direction)
                                << EncLog::kv("received_counter", result.nonce_counter)
                                << EncLog::kv("expected_counter", result.expected_nonce_counter)
                                << EncLog::kv("failure_num", count)
                                << EncLog::kv("s2c_key_fp",
                                        keyToFingerprint(enc_state.s2c.key.data(), AES256_KEY_SIZE).substr(0, 16))
                                << EncLog::kv("c2s_key_fp",
                                        keyToFingerprint(enc_state.c2s.key.data(), AES256_KEY_SIZE).substr(0, 16))
                                << EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
                                << EncLog::kv("active", enc_state.active.load())
                                << EncLog::kv("session_id", enc_state.session_id.substr(0, 16))
                                << std::endl;

                        // One-time key mismatch diagnostic on the FIRST failure only
                        if (count == 1) {
                                enclog_error("KEY MISMATCH DIAGNOSTIC -- first GCM auth failure for this session")
                                        << EncLog::kv("peer", peer_id)
                                        << EncLog::kv("direction", direction)
                                        << EncLog::kv("s2c_key_full_fp",
                                                keyToFingerprint(enc_state.s2c.key.data(), AES256_KEY_SIZE))
                                        << EncLog::kv("c2s_key_full_fp",
                                                keyToFingerprint(enc_state.c2s.key.data(), AES256_KEY_SIZE))
                                        << EncLog::kv("s2c_nonce_base",
                                                binToHex(enc_state.s2c.nonce_base.data(), enc_state.s2c.nonce_base.size()))
                                        << EncLog::kv("c2s_nonce_base",
                                                binToHex(enc_state.c2s.nonce_base.data(), enc_state.c2s.nonce_base.size()))
                                        << EncLog::kv("s2c_counter", enc_state.s2c.nonce_counter)
                                        << EncLog::kv("c2s_counter", enc_state.c2s.nonce_counter)
                                        << EncLog::kv("s2c_packets", enc_state.s2c.packets_processed)
                                        << EncLog::kv("c2s_packets", enc_state.c2s.packets_processed)
                                        << EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
                                        << EncLog::kv("activated_at", enc_state.activated_at)
                                        << EncLog::kv("session_id", enc_state.session_id)
                                        << std::endl;
                        }
                } else if (count <= 9) {
                        // Brief warning for failures 4-9
                        warningstream << ENC_TAG_ERROR
                                << "GCM auth failure #" << count
                                << " from peer " << peer_id
                                << " | direction=" << direction
                                << " | counter=" << result.nonce_counter
                                << " | session=" << enc_state.session_id.substr(0, 16)
                                << std::endl;
                } else if (count % 50 == 0) {
                        // Periodic summary every 50 failures after the first 9
                        warningstream << ENC_TAG_ERROR
                                << "GCM auth failures continue: " << count
                                << " total from peer " << peer_id
                                << " | direction=" << direction
                                << " | session=" << enc_state.session_id.substr(0, 16)
                                << " | likely KEY MISMATCH -- check ECDH exchange"
                                << std::endl;
                }

                // Persistent mismatch banner at 100 failures
                if (count == 100) {
                        enclog_error("PERSISTENT KEY MISMATCH -- 100 consecutive GCM auth failures")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("direction", direction)
                                << EncLog::kv("session_id", enc_state.session_id.substr(0, 16))
                                << EncLog::kv("possible_cause", "ECDH_key_exchange_failed_on_one_side")
                                << EncLog::kv("recommendation", "check_initFromSRPSessionKey_and_mixECDHSecretIntoKeys_on_both_sides")
                                << std::endl;
                }
                break;
        }

        case DecryptResult::InternalError:
                enclog_error("Decryption internal error from peer")
                        << EncLog::kv("peer", peer_id)
                        << EncLog::kv("error", result.error_detail)
                        << EncLog::kv("action", "PACKET_DROPPED")
                        << std::endl;
                break;

        case DecryptResult::Success:
                // No failure to log
                break;
        }
}
