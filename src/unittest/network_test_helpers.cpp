// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Implementation of shared network test utilities.

#include "network_test_helpers.h"

#include "network/mtp/internal.h"
#include "util/serialize.h"

#include <cstring>

// ============================================================================
// Test Key and State Generation
// ============================================================================

std::array<u8, SRP_SESSION_KEY_SIZE> makeTestSessionKey()
{
	// Deterministic test key: same pattern used across all tests for
	// reproducibility. This is NOT a real SRP session key — it's only
	// for testing.
	std::array<u8, SRP_SESSION_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = static_cast<u8>(i * 7 + 42);
	return key;
}

PeerEncryptionState makeTestEncState(bool is_server)
{
	auto key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(key.data(), key.size(), is_server);
	return state;
}

// ============================================================================
// Encrypt / Decrypt Helpers
// ============================================================================

CryptoResult encryptPacket(const u8* key, size_t key_len,
		const u8* nonce, size_t nonce_len,
		const u8* plaintext, size_t plaintext_len,
		const u8* aad, size_t aad_len)
{
	return aes256gcm_encrypt(key, key_len,
			nonce, nonce_len,
			plaintext, plaintext_len,
			aad, aad_len);
}

CryptoResult decryptPacket(const u8* key, size_t key_len,
		const u8* nonce, size_t nonce_len,
		const u8* ciphertext, size_t ciphertext_len,
		const u8* tag, size_t tag_len,
		const u8* aad, size_t aad_len)
{
	return aes256gcm_decrypt(key, key_len,
			nonce, nonce_len,
			ciphertext, ciphertext_len,
			tag, tag_len,
			aad, aad_len);
}

// ============================================================================
// Packet Building Helpers
// ============================================================================

std::vector<u8> buildPlaintextPacket(session_t peer_id, u8 channel,
		const std::string& payload)
{
	// Base header format:
	//   [0..3] u32 protocol_id  (0x4f457403)
	//   [4..5] session_t peer_id (big-endian)
	//   [6]    u8 channel
	//   [7..]  payload

	constexpr size_t BASE_HEADER_SIZE = 7;
	size_t total_size = BASE_HEADER_SIZE + payload.size();
	std::vector<u8> packet(total_size, 0);

	// Protocol ID
	writeU32(&packet[0], PROTOCOL_ID);

	// Peer ID (big-endian u16)
	writeU16(&packet[4], peer_id);

	// Channel
	packet[6] = channel;

	// Payload
	if (!payload.empty())
		memcpy(&packet[BASE_HEADER_SIZE], payload.data(), payload.size());

	return packet;
}

std::vector<u8> buildEncryptedPacket(session_t peer_id, u8 channel,
		const std::string& payload, PeerEncryptionState& state)
{
	// Encrypted packet format:
	//   [0..6]  base header (protocol_id + peer_id + channel)
	//   [7]     0x80 encrypted flag
	//   [8..19] 12-byte GCM nonce
	//   [20..N-16] ciphertext
	//   [N-16..N-1] 16-byte GCM tag

	constexpr size_t BASE_HEADER_SIZE = 7;

	// Encrypt the payload using the S2C direction
	auto lock = state.lock();
	auto nonce = state.s2c.nextNonce();
	u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc_result = aes256gcm_encrypt(
			state.s2c.key.data(), state.s2c.key.size(),
			nonce.data(), nonce.size(),
			reinterpret_cast<const u8*>(payload.data()), payload.size(),
			&encrypted_flag, 1);
	lock.unlock();

	if (!enc_result.success)
		return {}; // Return empty vector on failure

	// Build the complete packet
	size_t enc_size = BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + enc_result.data.size();
	std::vector<u8> packet(enc_size, 0);

	// Base header
	writeU32(&packet[0], PROTOCOL_ID);
	writeU16(&packet[4], peer_id);
	packet[6] = channel;

	// Encrypted flag
	packet[BASE_HEADER_SIZE] = ENCRYPTED_FLAG_AES_256_GCM;

	// Nonce
	memcpy(&packet[BASE_HEADER_SIZE + 1], nonce.data(), GCM_NONCE_SIZE);

	// Ciphertext
	if (!enc_result.data.empty())
		memcpy(&packet[BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE],
				enc_result.data.data(), enc_result.data.size());

	// GCM authentication tag
	memcpy(&packet[enc_size - GCM_TAG_SIZE],
			enc_result.tag.data(), GCM_TAG_SIZE);

	return packet;
}

// ============================================================================
// Buffer Check Helpers
// ============================================================================

bool isAllZeros(const u8* data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (data[i] != 0)
			return false;
	}
	return true;
}

bool isKeyInitialized(const u8* data, size_t len)
{
	return !isAllZeros(data, len);
}
