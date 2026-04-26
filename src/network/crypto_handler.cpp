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
		result.status = DecryptResult::KeyNotInitialized;
		result.error_detail = "Decryption key not initialized (key derivation may not have completed)";
		return result;
	}

	// Replay protection check
	if (!dir_state.isNotReplay(packet.nonce_counter)) {
		dir_state.replay_attempts++;
		result.status = DecryptResult::ReplayDetected;
		result.error_detail = "Replay detected (counter=" +
			std::to_string(packet.nonce_counter) +
			", expected=" + std::to_string(dir_state.nonce_counter) + ")";
		return result;
	}

	// AAD: the encrypted flag byte
	u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

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
	} else {
		// Decryption failed — authentication tag mismatch
		dir_state.auth_failures++;
		result.status = DecryptResult::AuthFailed;
		result.error_detail = crypto_result.error_msg;
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

	// Build nonce
	result.nonce = dir_state.nextNonce();

	// AAD: the encrypted flag byte
	u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

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
			<< EncLog::kv("action", "PACKET_DROPPED")
			<< std::endl;
		break;

	case DecryptResult::AuthFailed: {
		// Throttled logging: first 3 at ERROR, then every 100th
		// Determine direction for auth_failures lookup
		// (The caller already incremented auth_failures in decrypt(),
		//  so we check the count to decide whether to log.)
		bool should_log = true;  // Default to logging; caller can check
		// The actual throttling is based on the auth_failures counter
		// which was already incremented in decrypt().
		// Since we don't have access to the counter here directly
		// (it's inside the locked state), we log a generic message.
		// The caller should check dir_state.auth_failures for throttling.
		enclog_error("Decryption FAILED from peer")
			<< EncLog::kv("peer", peer_id)
			<< EncLog::kv("error", result.error_detail)
			<< EncLog::kv("action", "PACKET_DROPPED")
			<< EncLog::kv("direction", we_are_server ? "C2S" : "S2C")
			<< EncLog::kv("received_counter", result.nonce_counter)
			<< EncLog::kv("s2c_key_fp",
				keyToFingerprint(enc_state.s2c.key.data(), AES256_KEY_SIZE).substr(0, 8))
			<< EncLog::kv("c2s_key_fp",
				keyToFingerprint(enc_state.c2s.key.data(), AES256_KEY_SIZE).substr(0, 8))
			<< EncLog::kv("ecdh_completed", enc_state.ecdh_completed.load())
			<< EncLog::kv("session_id", enc_state.session_id)
			<< std::endl;
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
