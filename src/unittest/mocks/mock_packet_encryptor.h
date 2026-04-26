// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Mock implementation of IPacketEncryptor for testing.
//
// This mock does NOT perform real encryption. Instead, it prepends
// a 0x80 flag byte on encrypt and strips it on decrypt. This allows
// tests to verify that the encrypt/decrypt interface is called correctly
// without depending on the real crypto implementation.

#pragma once
#include "network/encryption/ipacket_encryptor.h"
#include <vector>
#include <utility>

class MockPacketEncryptor : public IPacketEncryptor
{
public:
	struct EncryptCall {
		session_t peer_id;
		SharedBuffer<u8> plaintext;
	};
	struct DecryptCall {
		session_t peer_id;
		SharedBuffer<u8> ciphertext;
	};

	std::vector<EncryptCall> encrypt_calls;
	std::vector<DecryptCall> decrypt_calls;

	// Configuration
	bool always_encrypt_success = true;
	bool always_decrypt_success = true;
	std::set<session_t> encrypted_peers;

	std::optional<SharedBuffer<u8>> encrypt(
		session_t peer_id, const SharedBuffer<u8>& plaintext) override
	{
		encrypt_calls.push_back({peer_id, plaintext});
		if (!always_encrypt_success) return std::nullopt;
		// Just prepend 0x80 flag and return same data (no real encryption in mock)
		SharedBuffer<u8> result(plaintext.getSize() + 1);
		result[0] = 0x80;
		memcpy(&result[1], *plaintext, plaintext.getSize());
		return result;
	}

	std::optional<SharedBuffer<u8>> decrypt(
		session_t peer_id, const SharedBuffer<u8>& ciphertext) override
	{
		decrypt_calls.push_back({peer_id, ciphertext});
		if (!always_decrypt_success) return std::nullopt;
		// Strip 0x80 flag
		if (ciphertext.getSize() < 1 || ciphertext[0] != 0x80) return std::nullopt;
		SharedBuffer<u8> result(ciphertext.getSize() - 1);
		memcpy(*result, &ciphertext[1], result.getSize());
		return result;
	}

	bool isEncrypted(session_t peer_id) const override {
		return encrypted_peers.count(peer_id) > 0;
	}

	bool isEncryptedFlag(const u8* data, size_t size) const override {
		return size >= 1 && data[0] == 0x80;
	}

	void reset() {
		encrypt_calls.clear();
		decrypt_calls.clear();
	}
};
