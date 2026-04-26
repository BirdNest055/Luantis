// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for HKDFKeyDeriver — the key derivation component that
// derives C2S/S2C encryption keys and nonce bases from the SRP
// session key.
//
// These tests prove that:
// - Keys are correctly derived from test IKM
// - Both directions get different keys (C2S ≠ S2C)
// - Server/client key swap: C2S/S2C are swapped between sides
// - Same IKM + same version = same keys (deterministic)
// - Different version = different keys
// - Empty IKM fails
// - Too-short IKM fails
//
// The key derivation is tested via PeerEncryptionState::initFromSRPSessionKey
// and the underlying hkdf_sha256 function, which is the actual
// implementation that HKDFKeyDeriver wraps.

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>

class TestKeyDeriver : public TestBase
{
public:
	TestKeyDeriver() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestKeyDeriver"; }

	void runTests(IGameDef *gamedef);

	// Basic key derivation
	void testDeriveKeysFromTestIKM();
	void testBothDirectionsGetDifferentKeys();
	void testServerClientKeySwap();
	void testSameIKMProducesSameKeys();
	void testDifferentIKMProducesDifferentKeys();

	// Version isolation
	void testDifferentVersionProducesDifferentKeys();

	// Error handling
	void testEmptyIKMFails();
	void testTooShortIKMFails();

	// Key material properties
	void testDerivedKeysAreNotAllZeros();
	void testDerivedNonceBasesAreNotAllZeros();
	void testNonceBasesAreDifferentFromKeys();
	void testSessionIdIsDeterministic();

	// Comprehensive round-trip
	void testFullDerivationAndRoundtrip();
};

static TestKeyDeriver g_test_instance;

void TestKeyDeriver::runTests(IGameDef *gamedef)
{
	TEST(testDeriveKeysFromTestIKM);
	TEST(testBothDirectionsGetDifferentKeys);
	TEST(testServerClientKeySwap);
	TEST(testSameIKMProducesSameKeys);
	TEST(testDifferentIKMProducesDifferentKeys);

	TEST(testDifferentVersionProducesDifferentKeys);

	TEST(testEmptyIKMFails);
	TEST(testTooShortIKMFails);

	TEST(testDerivedKeysAreNotAllZeros);
	TEST(testDerivedNonceBasesAreNotAllZeros);
	TEST(testNonceBasesAreDifferentFromKeys);
	TEST(testSessionIdIsDeterministic);

	TEST(testFullDerivationAndRoundtrip);
}

// Helper: generate a deterministic test IKM
static std::array<u8, SRP_SESSION_KEY_SIZE> makeTestIKM()
{
	std::array<u8, SRP_SESSION_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = static_cast<u8>(i * 7 + 42);
	return key;
}

// Helper: generate a different test IKM
static std::array<u8, SRP_SESSION_KEY_SIZE> makeAltTestIKM()
{
	std::array<u8, SRP_SESSION_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = static_cast<u8>(i * 11 + 99);
	return key;
}

// ============================================================================
// Basic Key Derivation Tests
// ============================================================================

void TestKeyDeriver::testDeriveKeysFromTestIKM()
{
	// initFromSRPSessionKey should succeed with valid IKM and derive
	// non-trivial keys.
	auto ikm = makeTestIKM();
	PeerEncryptionState state;
	bool ok = state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);
	UASSERT(ok);

	// C2S and S2C keys should be non-trivial (not all zeros)
	bool c2s_all_zero = true, s2c_all_zero = true;
	for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
		if (state.c2s.key[i] != 0) c2s_all_zero = false;
		if (state.s2c.key[i] != 0) s2c_all_zero = false;
	}
	UASSERT(!c2s_all_zero);
	UASSERT(!s2c_all_zero);
}

void TestKeyDeriver::testBothDirectionsGetDifferentKeys()
{
	// C2S and S2C keys MUST be different. This is critical for
	// security — if they were the same, an attacker could decrypt
	// traffic in both directions with a single key.
	auto ikm = makeTestIKM();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	UASSERT(memcmp(state.c2s.key.data(), state.s2c.key.data(),
			AES256_KEY_SIZE) != 0);
}

void TestKeyDeriver::testServerClientKeySwap()
{
	// The server and client sides derive the same keys, but use them
	// in opposite directions:
	//   Server C2S key == Client C2S key
	//   Server S2C key == Client S2C key
	//
	// The server ENCRYPTS with S2C (server→client), while the client
	// DECRYPTS with S2C. The client ENCRYPTS with C2S, the server
	// DECRYPTS with C2S.
	auto ikm = makeTestIKM();

	PeerEncryptionState server_state;
	PeerEncryptionState client_state;

	bool ok1 = server_state.initFromSRPSessionKey(ikm.data(), ikm.size(), true);
	bool ok2 = client_state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);
	UASSERT(ok1 && ok2);

	// Both sides should derive IDENTICAL keys (same SRP session key)
	UASSERT(memcmp(server_state.c2s.key.data(), client_state.c2s.key.data(),
			AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(server_state.s2c.key.data(), client_state.s2c.key.data(),
			AES256_KEY_SIZE) == 0);

	// Nonce bases should also match
	UASSERT(memcmp(server_state.c2s.nonce_base.data(),
			client_state.c2s.nonce_base.data(),
			NONCE_BASE_SIZE) == 0);
	UASSERT(memcmp(server_state.s2c.nonce_base.data(),
			client_state.s2c.nonce_base.data(),
			NONCE_BASE_SIZE) == 0);
}

void TestKeyDeriver::testSameIKMProducesSameKeys()
{
	// Same IKM should always produce the same derived keys.
	// This is essential for both sides of the connection to agree.
	auto ikm = makeTestIKM();

	PeerEncryptionState state1, state2;
	state1.initFromSRPSessionKey(ikm.data(), ikm.size(), false);
	state2.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	// All derived values should match exactly
	UASSERT(memcmp(state1.c2s.key.data(), state2.c2s.key.data(),
			AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(state1.s2c.key.data(), state2.s2c.key.data(),
			AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(state1.c2s.nonce_base.data(), state2.c2s.nonce_base.data(),
			NONCE_BASE_SIZE) == 0);
	UASSERT(memcmp(state1.s2c.nonce_base.data(), state2.s2c.nonce_base.data(),
			NONCE_BASE_SIZE) == 0);
}

void TestKeyDeriver::testDifferentIKMProducesDifferentKeys()
{
	// Different IKM should produce different keys.
	// If two different SRP sessions produced the same keys,
	// that would be a catastrophic failure.
	auto ikm1 = makeTestIKM();
	auto ikm2 = makeAltTestIKM();

	PeerEncryptionState state1, state2;
	state1.initFromSRPSessionKey(ikm1.data(), ikm1.size(), false);
	state2.initFromSRPSessionKey(ikm2.data(), ikm2.size(), false);

	// C2S keys should differ
	UASSERT(memcmp(state1.c2s.key.data(), state2.c2s.key.data(),
			AES256_KEY_SIZE) != 0);

	// S2C keys should differ
	UASSERT(memcmp(state1.s2c.key.data(), state2.s2c.key.data(),
			AES256_KEY_SIZE) != 0);
}

// ============================================================================
// Version Isolation Tests
// ============================================================================

void TestKeyDeriver::testDifferentVersionProducesDifferentKeys()
{
	// HKDF info strings include the version (e.g., "Luanti v9 C2S Key").
	// If the version changes, the derived keys must be different.
	// This test verifies that the info string matters by testing
	// the raw hkdf_sha256 function with different info strings.

	auto ikm = makeTestIKM();

	// Derive with "v9" info strings
	const char *info_v9_c2s = "Luanti v9 C2S Key";
	std::array<u8, AES256_KEY_SIZE> key_v9{};
	bool ok1 = hkdf_sha256(ikm.data(), ikm.size(),
			nullptr, 0,
			reinterpret_cast<const u8*>(info_v9_c2s), strlen(info_v9_c2s),
			key_v9.data(), key_v9.size());
	UASSERT(ok1);

	// Derive with "v10" info strings (hypothetical future version)
	const char *info_v10_c2s = "Luanti v10 C2S Key";
	std::array<u8, AES256_KEY_SIZE> key_v10{};
	bool ok2 = hkdf_sha256(ikm.data(), ikm.size(),
			nullptr, 0,
			reinterpret_cast<const u8*>(info_v10_c2s), strlen(info_v10_c2s),
			key_v10.data(), key_v10.size());
	UASSERT(ok2);

	// Different info strings → different keys
	UASSERT(memcmp(key_v9.data(), key_v10.data(), AES256_KEY_SIZE) != 0);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

void TestKeyDeriver::testEmptyIKMFails()
{
	// A null IKM should cause initFromSRPSessionKey to fail.
	PeerEncryptionState state;
	bool ok = state.initFromSRPSessionKey(nullptr, SRP_SESSION_KEY_SIZE, false);
	UASSERT(!ok);
}

void TestKeyDeriver::testTooShortIKMFails()
{
	// An IKM shorter than SRP_SESSION_KEY_SIZE should fail.
	auto ikm = makeTestIKM();
	PeerEncryptionState state;

	// 16 bytes instead of 32
	bool ok = state.initFromSRPSessionKey(ikm.data(), 16, false);
	UASSERT(!ok);

	// 0 bytes
	bool ok2 = state.initFromSRPSessionKey(ikm.data(), 0, false);
	UASSERT(!ok2);

	// 31 bytes (just one byte short)
	bool ok3 = state.initFromSRPSessionKey(ikm.data(), 31, false);
	UASSERT(!ok3);
}

// ============================================================================
// Key Material Properties
// ============================================================================

void TestKeyDeriver::testDerivedKeysAreNotAllZeros()
{
	auto ikm = makeTestIKM();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	// C2S key should not be all zeros
	bool c2s_zero = true;
	for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
		if (state.c2s.key[i] != 0) { c2s_zero = false; break; }
	}
	UASSERT(!c2s_zero);

	// S2C key should not be all zeros
	bool s2c_zero = true;
	for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
		if (state.s2c.key[i] != 0) { s2c_zero = false; break; }
	}
	UASSERT(!s2c_zero);
}

void TestKeyDeriver::testDerivedNonceBasesAreNotAllZeros()
{
	auto ikm = makeTestIKM();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	// C2S nonce base should not be all zeros
	bool c2s_nb_zero = true;
	for (size_t i = 0; i < NONCE_BASE_SIZE; i++) {
		if (state.c2s.nonce_base[i] != 0) { c2s_nb_zero = false; break; }
	}
	UASSERT(!c2s_nb_zero);

	// S2C nonce base should not be all zeros
	bool s2c_nb_zero = true;
	for (size_t i = 0; i < NONCE_BASE_SIZE; i++) {
		if (state.s2c.nonce_base[i] != 0) { s2c_nb_zero = false; break; }
	}
	UASSERT(!s2c_nb_zero);
}

void TestKeyDeriver::testNonceBasesAreDifferentFromKeys()
{
	// The nonce bases should be cryptographically independent from
	// the encryption keys (derived with different HKDF info strings).
	auto ikm = makeTestIKM();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	// The nonce base is only 4 bytes while the key is 32 bytes,
	// but they should not share the same prefix.
	// Compare the first 4 bytes of C2S key against C2S nonce base.
	UASSERT(memcmp(state.c2s.key.data(), state.c2s.nonce_base.data(),
			NONCE_BASE_SIZE) != 0);
}

void TestKeyDeriver::testSessionIdIsDeterministic()
{
	// Same IKM should always produce the same session ID.
	auto ikm = makeTestIKM();

	PeerEncryptionState state1, state2;
	state1.initFromSRPSessionKey(ikm.data(), ikm.size(), false);
	state2.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	UASSERT(state1.session_id == state2.session_id);
	UASSERT(!state1.session_id.empty());

	// Different IKM should produce different session IDs
	auto ikm2 = makeAltTestIKM();
	PeerEncryptionState state3;
	state3.initFromSRPSessionKey(ikm2.data(), ikm2.size(), false);

	UASSERT(state1.session_id != state3.session_id);
}

// ============================================================================
// Comprehensive Round-Trip
// ============================================================================

void TestKeyDeriver::testFullDerivationAndRoundtrip()
{
	// Full test: derive keys on both sides, encrypt with one side,
	// decrypt with the other.
	auto ikm = makeTestIKM();

	PeerEncryptionState server_state;
	PeerEncryptionState client_state;
	server_state.initFromSRPSessionKey(ikm.data(), ikm.size(), true);
	client_state.initFromSRPSessionKey(ikm.data(), ikm.size(), false);

	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	// Server encrypts with S2C key → Client decrypts with S2C key
	const char *msg = "Key derivation round-trip test!";
	auto s2c_nonce = server_state.s2c.nextNonce();

	CryptoResult enc = aes256gcm_encrypt(
			server_state.s2c.key.data(), server_state.s2c.key.size(),
			s2c_nonce.data(), s2c_nonce.size(),
			reinterpret_cast<const u8*>(msg), strlen(msg),
			&aad, 1);
	UASSERT(enc.success);

	CryptoResult dec = aes256gcm_decrypt(
			client_state.s2c.key.data(), client_state.s2c.key.size(),
			s2c_nonce.data(), s2c_nonce.size(),
			enc.data.data(), enc.data.size(),
			enc.tag.data(), enc.tag.size(),
			&aad, 1);
	UASSERT(dec.success);
	UASSERT(memcmp(dec.data.data(), msg, strlen(msg)) == 0);

	// Client encrypts with C2S key → Server decrypts with C2S key
	auto c2s_nonce = client_state.c2s.nextNonce();

	CryptoResult enc2 = aes256gcm_encrypt(
			client_state.c2s.key.data(), client_state.c2s.key.size(),
			c2s_nonce.data(), c2s_nonce.size(),
			reinterpret_cast<const u8*>(msg), strlen(msg),
			&aad, 1);
	UASSERT(enc2.success);

	CryptoResult dec2 = aes256gcm_decrypt(
			server_state.c2s.key.data(), server_state.c2s.key.size(),
			c2s_nonce.data(), c2s_nonce.size(),
			enc2.data.data(), enc2.data.size(),
			enc2.tag.data(), enc2.tag.size(),
			&aad, 1);
	UASSERT(dec2.success);
	UASSERT(memcmp(dec2.data.data(), msg, strlen(msg)) == 0);

	// Cross-direction MUST fail: server can't decrypt S2C with C2S key
	CryptoResult wrong_dec = aes256gcm_decrypt(
			server_state.c2s.key.data(), server_state.c2s.key.size(),
			s2c_nonce.data(), s2c_nonce.size(),
			enc.data.data(), enc.data.size(),
			enc.tag.data(), enc.tag.size(),
			&aad, 1);
	UASSERT(!wrong_dec.success);
}
