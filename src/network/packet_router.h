// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include "util/pointer.h"
#include <variant>
#include <optional>

/// The 0x80 encrypted flag byte is a FIRST-CLASS ROUTING DECISION.
/// This file encapsulates the flag-based packet routing logic that
/// determines whether a received packet is plaintext or encrypted,
/// and provides clean data structures for each case.
///
/// DESIGN PRINCIPLE: The flag is checked ONCE at the boundary, and
/// the result is a discriminated union (std::variant) that forces
/// the caller to handle each case explicitly. There is NO path where
/// a plaintext packet is accidentally treated as encrypted or vice versa.
///
/// WIRE FORMAT:
///   Plaintext: [base_header(7B)][packet_type(0-3)][payload...]
///   Encrypted: [base_header(7B)][0x80][nonce(12B)][ciphertext][GCM_tag(16B)]

constexpr size_t PACKET_BASE_HEADER_SIZE = 7;

/// Result of routing a received packet by its flag byte.
/// This is the SINGLE source of truth for the flag decision.
enum class PacketRoute {
	Plaintext,   ///< No 0x80 flag — process as plaintext MTP packet
	Encrypted,   ///< 0x80 flag present — must decrypt before MTP processing
	Invalid       ///< Packet too short or malformed — drop silently
};

/// Parsed structure for a plaintext packet (after base header stripped).
struct PlaintextPacket {
	SharedBuffer<u8> data;  ///< Everything after the base header (includes packet_type byte)
};

/// Parsed structure for an encrypted packet (after base header stripped).
/// All pointer arithmetic is encapsulated here — no raw pointer math in thread code.
struct EncryptedPacket {
	/// The 12-byte GCM nonce
	std::array<u8, 12> nonce;

	/// The nonce counter extracted from the nonce (bytes 4-11, big-endian)
	u64 nonce_counter;

	/// The ciphertext (does NOT include tag)
	std::vector<u8> ciphertext;

	/// The 16-byte GCM authentication tag
	std::array<u8, 16> tag;

	/// Original raw data (for debugging/diagnostics)
	SharedBuffer<u8> raw_data;
};

/// Route a packet based on its flag byte.
///
/// This is the SINGLE function that determines packet treatment.
/// It checks:
///   1. Is the packet long enough to have a flag byte?
///   2. Is the flag byte 0x80 (encrypted) or 0x00-0x03 (plaintext)?
///   3. If encrypted, is the packet long enough for the encrypted format?
///
/// @param data      Raw packet data (including base header)
/// @param size      Size of the packet data
/// @return PacketRoute indicating how to process this packet
PacketRoute routePacket(const u8* data, size_t size);

/// Parse a plaintext packet from raw data.
/// @param data      Raw packet data (including base header)
/// @param size      Size of the packet data
/// @return Parsed plaintext packet
PlaintextPacket parsePlaintext(const u8* data, size_t size);

/// Parse an encrypted packet from raw data.
/// Returns std::nullopt if the packet is malformed (too short, bad format).
/// @param data      Raw packet data (including base header)
/// @param size      Size of the packet data
/// @return Parsed encrypted packet, or std::nullopt if invalid
std::optional<EncryptedPacket> parseEncrypted(const u8* data, size_t size);

/// Build an encrypted packet from base header + plaintext payload.
/// This is the inverse of parseEncrypted — used by the send path.
/// @param base_header    The 7-byte base header (protocol_id + peer_id + channel)
/// @param nonce          The 12-byte GCM nonce
/// @param ciphertext     The encrypted payload
/// @param tag            The 16-byte GCM authentication tag
/// @return Complete encrypted packet ready to send
std::vector<u8> buildEncryptedPacket(
	const u8* base_header,
	const std::array<u8, 12>& nonce,
	const std::vector<u8>& ciphertext,
	const std::array<u8, 16>& tag);

/// Check if a byte is a valid MTP packet type (0x00-0x03).
/// Used to validate that a non-0x80 flag byte is actually a packet type.
inline bool isValidPacketType(u8 byte) {
	return byte <= 0x03;
}

/// Check if a byte is the encrypted flag (0x80).
inline bool isEncryptedFlag(u8 byte) {
	return byte == 0x80;
}
