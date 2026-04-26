// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/packet_router.h"
#include "network/crypto.h"
#include <cstring>

/// Encrypted packet overhead after the base header:
/// flag(1) + nonce(12) + tag(16) = 29 bytes
/// A valid encrypted packet must have at least this many bytes after the header.
static constexpr size_t ENCRYPTED_OVERHEAD_AFTER_HEADER =
	1 + GCM_NONCE_SIZE + GCM_TAG_SIZE;  // 29

PacketRoute routePacket(const u8* data, size_t size)
{
	// Too short to even have a flag byte after the base header
	if (size <= PACKET_BASE_HEADER_SIZE)
		return PacketRoute::Invalid;

	u8 flag = data[PACKET_BASE_HEADER_SIZE];

	// 0x80 = encrypted packet
	if (isEncryptedFlag(flag)) {
		// Must have enough bytes after the header for the encrypted format:
		// flag(1) + nonce(12) + tag(16) = 29 bytes minimum (empty ciphertext OK)
		size_t after_header = size - PACKET_BASE_HEADER_SIZE;
		if (after_header < ENCRYPTED_OVERHEAD_AFTER_HEADER)
			return PacketRoute::Invalid;
		return PacketRoute::Encrypted;
	}

	// 0x00-0x03 = valid MTP packet types (plaintext)
	if (isValidPacketType(flag))
		return PacketRoute::Plaintext;

	// Unknown flag byte — neither a valid packet type nor the encrypted flag
	return PacketRoute::Invalid;
}

PlaintextPacket parsePlaintext(const u8* data, size_t size)
{
	PlaintextPacket result;
	size_t data_size = size - PACKET_BASE_HEADER_SIZE;
	result.data = SharedBuffer<u8>(data_size);
	memcpy(*result.data, data + PACKET_BASE_HEADER_SIZE, data_size);
	return result;
}

std::optional<EncryptedPacket> parseEncrypted(const u8* data, size_t size)
{
	// Validate size
	if (size <= PACKET_BASE_HEADER_SIZE)
		return std::nullopt;

	size_t after_header = size - PACKET_BASE_HEADER_SIZE;
	if (after_header < ENCRYPTED_OVERHEAD_AFTER_HEADER)
		return std::nullopt;

	// Verify the flag byte
	u8 flag = data[PACKET_BASE_HEADER_SIZE];
	if (!isEncryptedFlag(flag))
		return std::nullopt;

	EncryptedPacket result;

	// Extract nonce (12 bytes after the flag byte)
	const u8* nonce_ptr = data + PACKET_BASE_HEADER_SIZE + 1;
	memcpy(result.nonce.data(), nonce_ptr, GCM_NONCE_SIZE);

	// Extract nonce counter from nonce bytes [4..11] (big-endian)
	result.nonce_counter = 0;
	for (int i = 0; i < 8; i++) {
		result.nonce_counter = (result.nonce_counter << 8) |
			result.nonce[NONCE_BASE_SIZE + i];
	}

	// Extract ciphertext (between nonce and tag)
	// after_header layout: flag(1) + nonce(12) + ciphertext(N) + tag(16)
	size_t ciphertext_len = after_header - 1 - GCM_NONCE_SIZE - GCM_TAG_SIZE;
	if (ciphertext_len > 0) {
		result.ciphertext.resize(ciphertext_len);
		const u8* ciphertext_ptr = nonce_ptr + GCM_NONCE_SIZE;
		memcpy(result.ciphertext.data(), ciphertext_ptr, ciphertext_len);
	}

	// Extract tag (last 16 bytes of the packet)
	const u8* tag_ptr = data + size - GCM_TAG_SIZE;
	memcpy(result.tag.data(), tag_ptr, GCM_TAG_SIZE);

	// Store raw data for diagnostics
	result.raw_data = SharedBuffer<u8>(size);
	memcpy(*result.raw_data, data, size);

	return result;
}

std::vector<u8> buildEncryptedPacket(
	const u8* base_header,
	const std::array<u8, 12>& nonce,
	const std::vector<u8>& ciphertext,
	const std::array<u8, 16>& tag)
{
	// Wire format: [base_header(7B)][0x80][nonce(12B)][ciphertext(NB)][tag(16B)]
	size_t total_size = PACKET_BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE +
		ciphertext.size() + GCM_TAG_SIZE;

	std::vector<u8> packet(total_size);

	// Copy base header (unencrypted)
	memcpy(packet.data(), base_header, PACKET_BASE_HEADER_SIZE);

	// Write encrypted flag
	packet[PACKET_BASE_HEADER_SIZE] = 0x80;

	// Write nonce
	memcpy(packet.data() + PACKET_BASE_HEADER_SIZE + 1,
		nonce.data(), GCM_NONCE_SIZE);

	// Write ciphertext
	if (!ciphertext.empty()) {
		memcpy(packet.data() + PACKET_BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE,
			ciphertext.data(), ciphertext.size());
	}

	// Write GCM tag
	memcpy(packet.data() + total_size - GCM_TAG_SIZE,
		tag.data(), GCM_TAG_SIZE);

	return packet;
}
