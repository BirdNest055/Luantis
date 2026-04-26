// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include "network/networkprotocol.h" // session_t
#include "network/packet_router.h"
#include "util/pointer.h"
#include <optional>
#include <array>
#include <vector>
#include <string>

struct PeerEncryptionState;
struct DirectionalEncryptionState;

/// Result of attempting to decrypt an encrypted packet.
/// Uses a typed result instead of ad-hoc success/failure + error logging.
struct DecryptResult {
	enum Status {
		Success,           ///< Decryption succeeded
		KeyNotInitialized, ///< Decryption key is all zeros (key derivation not complete)
		ReplayDetected,    ///< Nonce counter is a replay
		AuthFailed,        ///< GCM authentication tag verification failed
		InternalError      ///< OpenSSL or other internal error
	};

	Status status;
	SharedBuffer<u8> plaintext;  ///< Valid only when status == Success
	std::string error_detail;    ///< Diagnostic info for error cases
	u64 nonce_counter = 0;       ///< Counter from the packet (for diagnostics)
};

/// Result of attempting to encrypt a plaintext payload.
struct EncryptResult {
	enum Status {
		Success,           ///< Encryption succeeded
		NotActive,         ///< Encryption not active for this peer
		InternalError      ///< OpenSSL or other internal error
	};

	Status status;
	std::vector<u8> ciphertext;
	std::array<u8, 16> tag{};
	std::array<u8, 12> nonce{};
	std::string error_detail;
};

/// Clean handler for packet encryption/decryption operations.
///
/// This class encapsulates ALL the crypto logic that was previously
/// inline in ConnectionReceiveThread::receive() and 
/// ConnectionSendThread::rawSend(). It uses the parsed packet
/// structures from packet_router.h and returns typed results
/// instead of mixing crypto code with transport code.
///
/// Usage in receive path:
///   auto route = routePacket(data, size);
///   switch (route) {
///     case PacketRoute::Plaintext:
///       auto pkt = parsePlaintext(data, size);
///       // process pkt.data through normal MTP
///       break;
///     case PacketRoute::Encrypted:
///       auto enc = parseEncrypted(data, size);
///       auto result = CryptoHandler::decrypt(enc, state, we_are_server);
///       if (result.status == DecryptResult::Success) {
///         // process result.plaintext through normal MTP
///       }
///       break;
///     case PacketRoute::Invalid:
///       // drop silently
///       break;
///   }
class CryptoHandler
{
public:
	/// Decrypt an encrypted packet using the peer's encryption state.
	///
	/// This is the ONLY way to decrypt a received packet. All the logic
	/// for key lookup, replay protection, and GCM decryption is here.
	/// No crypto code should exist in the receive thread.
	///
	/// @param packet      Parsed encrypted packet (from parseEncrypted)
	/// @param enc_state   Peer's encryption state
	/// @param we_are_server  True if we're the server (determines C2S vs S2C key)
	/// @return DecryptResult with status and plaintext (on success)
	static DecryptResult decrypt(
		const EncryptedPacket& packet,
		PeerEncryptionState& enc_state,
		bool we_are_server);

	/// Encrypt a plaintext payload for sending to a peer.
	///
	/// This is the ONLY way to encrypt an outgoing packet. All the logic
	/// for key lookup, nonce generation, and GCM encryption is here.
	/// No crypto code should exist in the send thread.
	///
	/// @param plaintext      Data after base header to encrypt
	/// @param plaintext_len  Length of plaintext
	/// @param enc_state      Peer's encryption state
	/// @param we_are_server  True if we're the server
	/// @return EncryptResult with status and ciphertext/nonce/tag (on success)
	static EncryptResult encrypt(
		const u8* plaintext, size_t plaintext_len,
		PeerEncryptionState& enc_state,
		bool we_are_server);

	/// Check if encryption is active for a peer.
	/// This is the ONLY check the send thread should make before
	/// deciding whether to call encrypt() or send plaintext.
	static bool isEncryptionActive(const PeerEncryptionState& enc_state);

	/// Log a decryption failure with appropriate throttling.
	/// Centralizes the error logging that was scattered in receive().
	static void logDecryptFailure(const DecryptResult& result,
		session_t peer_id, const PeerEncryptionState& enc_state,
		bool we_are_server);
};
