// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Shared test utilities for all network-related unit tests.
//
// Provides helper functions for creating test keys, encryption state,
// plaintext and encrypted MTP packets, and common buffer checks.
// These helpers eliminate duplication across test files and ensure
// consistent test data generation.

#pragma once

#include "irrlichttypes.h"
#include "network/crypto.h"
#include "util/pointer.h"

#include <array>
#include <string>
#include <vector>

/// Generate a deterministic 32-byte test SRP session key.
/// The same key is produced every time, making tests reproducible.
std::array<u8, SRP_SESSION_KEY_SIZE> makeTestSessionKey();

/// Create a PeerEncryptionState initialized with a test SRP session key.
/// @param is_server  True for server side, false for client side.
/// @return Initialized PeerEncryptionState (not yet activated).
PeerEncryptionState makeTestEncState(bool is_server);

/// Encrypt a buffer using AES-256-GCM with the given key and nonce.
/// @param key          32-byte AES-256 key
/// @param nonce        12-byte GCM nonce
/// @param plaintext    Data to encrypt
/// @param plaintext_len Length of plaintext
/// @param aad          Additional Authenticated Data (typically ENCRYPTED_FLAG_AES_256_GCM)
/// @param aad_len      Length of AAD
/// @return CryptoResult with ciphertext and tag on success
CryptoResult encryptPacket(const u8* key, size_t key_len,
		const u8* nonce, size_t nonce_len,
		const u8* plaintext, size_t plaintext_len,
		const u8* aad, size_t aad_len);

/// Decrypt a buffer using AES-256-GCM with the given key, nonce, and tag.
/// @param key           32-byte AES-256 key
/// @param nonce         12-byte GCM nonce
/// @param ciphertext    Encrypted data
/// @param ciphertext_len Length of ciphertext
/// @param tag           16-byte GCM authentication tag
/// @param tag_len       Length of tag
/// @param aad           Additional Authenticated Data
/// @param aad_len       Length of AAD
/// @return CryptoResult with plaintext on success
CryptoResult decryptPacket(const u8* key, size_t key_len,
		const u8* nonce, size_t nonce_len,
		const u8* ciphertext, size_t ciphertext_len,
		const u8* tag, size_t tag_len,
		const u8* aad, size_t aad_len);

/// Build a plaintext MTP packet with the standard base header.
/// Format: [protocol_id(4)][peer_id(2)][channel(1)][payload...]
/// @param peer_id  Sender peer ID
/// @param channel  Channel number
/// @param payload  Payload data after base header
/// @return Complete packet as a vector
std::vector<u8> buildPlaintextPacket(session_t peer_id, u8 channel,
		const std::string& payload);

/// Build an encrypted MTP packet with the standard format.
/// Format: [base_header(7)][0x80 flag(1)][nonce(12)][ciphertext...][tag(16)]
/// Uses the S2C direction from the given state for encryption.
/// @param peer_id  Sender peer ID
/// @param channel  Channel number
/// @param payload  Payload data to encrypt
/// @param state    PeerEncryptionState (uses s2c direction)
/// @return Complete encrypted packet as a vector, empty on failure
std::vector<u8> buildEncryptedPacket(session_t peer_id, u8 channel,
		const std::string& payload, PeerEncryptionState& state);

/// Check if a buffer is entirely zeros.
/// @param data  Pointer to buffer
/// @param len   Length of buffer
/// @return true if all bytes are zero
bool isAllZeros(const u8* data, size_t len);

/// Check if a key buffer has been initialized (not all zeros).
/// @param data  Pointer to key buffer
/// @param len   Length of key buffer
/// @return true if at least one byte is non-zero
bool isKeyInitialized(const u8* data, size_t len);
