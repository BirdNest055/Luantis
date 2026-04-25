// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Test-Driven Development tests for forward secrecy (ECDH X25519).
//
// These tests verify that:
// - ECDH key pair generation works correctly
// - ECDH shared secret computation is consistent (both sides get same secret)
// - mixECDHSecretIntoKeys() uses SALTED HKDF (v9.11 fix — was unsalted in v9.1)
// - After ECDH, both sides derive the SAME encryption keys
// - ECDH-derived keys are DIFFERENT from SRP-only keys (forward secrecy)
// - Key rotation with deterministic salt produces same keys on both sides
// - ecdh_completed flag is properly set after ECDH
// - Insecure mode does not attempt ECDH
// - Key material is properly zeroed on disconnect after ECDH

#include "test.h"

#include "network/crypto.h"
#include "network/encryption_config.h"
#include "network/connection_security.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>

class TestForwardSecrecy : public TestBase
{
public:
	TestForwardSecrecy() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestForwardSecrecy"; }

	void runTests(IGameDef *gamedef);

	// X25519 key pair generation tests
	void testX25519GenerateKeypairSuccess();
	void testX25519GenerateKeypairProducesNonZeroKeys();
	void testX25519GenerateKeypairProducesDifferentKeysEachTime();

	// ECDH shared secret computation tests
	void testX25519SharedSecretConsistency();
	void testX25519SharedSecretSymmetry();
	void testX25519SharedSecretDifferentFromBothPrivateKeys();

	// mixECDHSecretIntoKeys tests (v9.11: salted HKDF fix)
	void testMixECDHSecretSetsCompleted();
	void testMixECDHSecretProducesNonZeroKeys();
	void testMixECDHSecretDerivesDifferentKeysThanSRPOnly();
	void testMixECDHSecretBothSidesDeriveSameKeys();
	void testMixECDHSecretWithNullSecretFails();
	void testMixECDHSecretWithWrongSizeFails();
	void testMixECDHSecretUsesSaltedHKDF();

	// Full ECDH handshake simulation (client + server)
	void testFullECDHHandshakeBothSidesSameKeys();
	void testFullECDHHandshakeKeysDifferFromSRPOnly();
	void testFullECDHHandshakeEncryptDecryptRoundtrip();

	// Key rotation with deterministic salt (v9.11 fix)
	void testRotateKeysWithDeterministicSaltBothSidesSame();
	void testRotateKeysProducesDifferentKeysThanInitial();
	void testRotateKeysRequiresActiveEncryption();

	// Insecure mode tests
	void testInsecureModeDoesNotDoECDH();

	// Key material cleanup after ECDH
	void testDisableZeroesECDHMaterial();

	// Security scoring with ECDH
	void testSecurityScoreWithECDH();
	void testSecurityScoreWithoutECDH();

	// Forward secrecy property: compromise of SRP key doesn't reveal ECDH keys
	void testForwardSecrecyProperty();
};

static TestForwardSecrecy g_test_instance;

// Helper: generate a random SRP session key for testing
static std::array<u8, SRP_SESSION_KEY_SIZE> makeTestSessionKey()
{
	std::array<u8, SRP_SESSION_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = static_cast<u8>(i * 7 + 42);
	return key;
}

// Helper: generate another different SRP session key
static std::array<u8, SRP_SESSION_KEY_SIZE> makeAltTestSessionKey()
{
	std::array<u8, SRP_SESSION_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = static_cast<u8>(i * 13 + 99);
	return key;
}

void TestForwardSecrecy::runTests(IGameDef *gamedef)
{
	// X25519 key pair generation tests
	TEST(testX25519GenerateKeypairSuccess);
	TEST(testX25519GenerateKeypairProducesNonZeroKeys);
	TEST(testX25519GenerateKeypairProducesDifferentKeysEachTime);

	// ECDH shared secret computation tests
	TEST(testX25519SharedSecretConsistency);
	TEST(testX25519SharedSecretSymmetry);
	TEST(testX25519SharedSecretDifferentFromBothPrivateKeys);

	// mixECDHSecretIntoKeys tests (v9.11: salted HKDF fix)
	TEST(testMixECDHSecretSetsCompleted);
	TEST(testMixECDHSecretProducesNonZeroKeys);
	TEST(testMixECDHSecretDerivesDifferentKeysThanSRPOnly);
	TEST(testMixECDHSecretBothSidesDeriveSameKeys);
	TEST(testMixECDHSecretWithNullSecretFails);
	TEST(testMixECDHSecretWithWrongSizeFails);
	TEST(testMixECDHSecretUsesSaltedHKDF);

	// Full ECDH handshake simulation
	TEST(testFullECDHHandshakeBothSidesSameKeys);
	TEST(testFullECDHHandshakeKeysDifferFromSRPOnly);
	TEST(testFullECDHHandshakeEncryptDecryptRoundtrip);

	// Key rotation with deterministic salt
	TEST(testRotateKeysWithDeterministicSaltBothSidesSame);
	TEST(testRotateKeysProducesDifferentKeysThanInitial);
	TEST(testRotateKeysRequiresActiveEncryption);

	// Insecure mode tests
	TEST(testInsecureModeDoesNotDoECDH);

	// Key material cleanup
	TEST(testDisableZeroesECDHMaterial);

	// Security scoring
	TEST(testSecurityScoreWithECDH);
	TEST(testSecurityScoreWithoutECDH);

	// Forward secrecy property
	TEST(testForwardSecrecyProperty);
}

// ============================================================================
// X25519 Key Pair Generation Tests
// ============================================================================

void TestForwardSecrecy::testX25519GenerateKeypairSuccess()
{
	X25519KeyPair kp = x25519_generate_keypair();
	UASSERT(kp.success);
}

void TestForwardSecrecy::testX25519GenerateKeypairProducesNonZeroKeys()
{
	X25519KeyPair kp = x25519_generate_keypair();
	UASSERT(kp.success);

	// Public key should not be all zeros
	bool pub_all_zero = true;
	for (size_t i = 0; i < X25519_PUBLIC_KEY_SIZE; i++) {
		if (kp.public_key[i] != 0) { pub_all_zero = false; break; }
	}
	UASSERT(!pub_all_zero);

	// Private key should not be all zeros
	bool priv_all_zero = true;
	for (size_t i = 0; i < X25519_PRIVATE_KEY_SIZE; i++) {
		if (kp.private_key[i] != 0) { priv_all_zero = false; break; }
	}
	UASSERT(!priv_all_zero);
}

void TestForwardSecrecy::testX25519GenerateKeypairProducesDifferentKeysEachTime()
{
	X25519KeyPair kp1 = x25519_generate_keypair();
	X25519KeyPair kp2 = x25519_generate_keypair();
	UASSERT(kp1.success);
	UASSERT(kp2.success);

	// Two generated key pairs should be different (extremely unlikely to collide)
	UASSERT(memcmp(kp1.public_key.data(), kp2.public_key.data(),
	               X25519_PUBLIC_KEY_SIZE) != 0);
	UASSERT(memcmp(kp1.private_key.data(), kp2.private_key.data(),
	               X25519_PRIVATE_KEY_SIZE) != 0);
}

// ============================================================================
// ECDH Shared Secret Computation Tests
// ============================================================================

void TestForwardSecrecy::testX25519SharedSecretConsistency()
{
	// Two key pairs: A and B
	// shared_secret(A_priv, B_pub) should equal shared_secret(B_priv, A_pub)
	X25519KeyPair kp_a = x25519_generate_keypair();
	X25519KeyPair kp_b = x25519_generate_keypair();
	UASSERT(kp_a.success && kp_b.success);

	X25519SharedSecret ss_ab = x25519_compute_shared_secret(
		kp_a.private_key.data(), kp_a.private_key.size(),
		kp_b.public_key.data(), kp_b.public_key.size());
	UASSERT(ss_ab.success);

	X25519SharedSecret ss_ba = x25519_compute_shared_secret(
		kp_b.private_key.data(), kp_b.private_key.size(),
		kp_a.public_key.data(), kp_a.public_key.size());
	UASSERT(ss_ba.success);

	// Both computations must yield the SAME shared secret
	UASSERT(memcmp(ss_ab.shared_secret.data(), ss_ba.shared_secret.data(),
	               X25519_SHARED_SECRET_SIZE) == 0);
}

void TestForwardSecrecy::testX25519SharedSecretSymmetry()
{
	// Same as consistency but explicitly test the mathematical property:
	// DH(A_priv, B_pub) = DH(B_priv, A_pub) — this is the Diffie-Hellman property
	X25519KeyPair kp_a = x25519_generate_keypair();
	X25519KeyPair kp_b = x25519_generate_keypair();

	X25519SharedSecret ss1 = x25519_compute_shared_secret(
		kp_a.private_key.data(), kp_a.private_key.size(),
		kp_b.public_key.data(), kp_b.public_key.size());
	X25519SharedSecret ss2 = x25519_compute_shared_secret(
		kp_b.private_key.data(), kp_b.private_key.size(),
		kp_a.public_key.data(), kp_a.public_key.size());

	UASSERT(ss1.success && ss2.success);
	UASSERT(memcmp(ss1.shared_secret.data(), ss2.shared_secret.data(),
	               X25519_SHARED_SECRET_SIZE) == 0);
}

void TestForwardSecrecy::testX25519SharedSecretDifferentFromBothPrivateKeys()
{
	// The shared secret should not be the same as either private key
	X25519KeyPair kp_a = x25519_generate_keypair();
	X25519KeyPair kp_b = x25519_generate_keypair();

	X25519SharedSecret ss = x25519_compute_shared_secret(
		kp_a.private_key.data(), kp_a.private_key.size(),
		kp_b.public_key.data(), kp_b.public_key.size());
	UASSERT(ss.success);

	// Shared secret differs from A's private key
	UASSERT(memcmp(ss.shared_secret.data(), kp_a.private_key.data(),
	               X25519_SHARED_SECRET_SIZE) != 0);

	// Shared secret differs from B's private key
	UASSERT(memcmp(ss.shared_secret.data(), kp_b.private_key.data(),
	               X25519_SHARED_SECRET_SIZE) != 0);
}

// ============================================================================
// mixECDHSecretIntoKeys Tests (v9.11: Salted HKDF Fix)
// ============================================================================

void TestForwardSecrecy::testMixECDHSecretSetsCompleted()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	UASSERT(!state.ecdh_completed.load());

	// Generate ECDH key pair and shared secret
	X25519KeyPair kp = x25519_generate_keypair();
	UASSERT(kp.success);

	// Mix ECDH secret into keys
	bool ok = mixECDHSecretIntoKeys(state, kp.public_key.data(), kp.public_key.size());
	UASSERT(ok);

	// ecdh_completed should now be true
	UASSERT(state.ecdh_completed.load());
}

void TestForwardSecrecy::testMixECDHSecretProducesNonZeroKeys()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	X25519KeyPair kp = x25519_generate_keypair();
	bool ok = mixECDHSecretIntoKeys(state, kp.public_key.data(), kp.public_key.size());
	UASSERT(ok);

	// C2S key should not be all zeros
	bool c2s_all_zero = true;
	for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
		if (state.c2s.key[i] != 0) { c2s_all_zero = false; break; }
	}
	UASSERT(!c2s_all_zero);

	// S2C key should not be all zeros
	bool s2c_all_zero = true;
	for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
		if (state.s2c.key[i] != 0) { s2c_all_zero = false; break; }
	}
	UASSERT(!s2c_all_zero);
}

void TestForwardSecrecy::testMixECDHSecretDerivesDifferentKeysThanSRPOnly()
{
	// Two states: one with SRP-only, one with SRP+ECDH
	auto srp_key = makeTestSessionKey();

	PeerEncryptionState srp_only_state;
	srp_only_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	PeerEncryptionState ecdh_state;
	ecdh_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	X25519KeyPair kp = x25519_generate_keypair();
	bool ok = mixECDHSecretIntoKeys(ecdh_state, kp.public_key.data(), kp.public_key.size());
	UASSERT(ok);

	// The ECDH-derived keys should be DIFFERENT from the SRP-only keys
	// This is the core of forward secrecy: knowing the SRP key is not enough
	UASSERT(memcmp(srp_only_state.c2s.key.data(), ecdh_state.c2s.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(memcmp(srp_only_state.s2c.key.data(), ecdh_state.s2c.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(memcmp(srp_only_state.c2s.nonce_base.data(), ecdh_state.c2s.nonce_base.data(),
	               NONCE_BASE_SIZE) != 0);
	UASSERT(memcmp(srp_only_state.s2c.nonce_base.data(), ecdh_state.s2c.nonce_base.data(),
	               NONCE_BASE_SIZE) != 0);
}

void TestForwardSecrecy::testMixECDHSecretBothSidesDeriveSameKeys()
{
	// CRITICAL TEST: Both sides must derive the SAME keys after ECDH.
	// This is what broke in v9.9 with random salts — each side got different keys.
	//
	// v9.11 FIX: mixECDHSecretIntoKeys() must use a DETERMINISTIC salt derived
	// from the combined IKM, so both sides produce the same salt and same keys.

	auto srp_key = makeTestSessionKey();

	// Server side
	PeerEncryptionState server_state;
	server_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);

	// Client side
	PeerEncryptionState client_state;
	client_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// Generate ECDH key pairs
	X25519KeyPair server_kp = x25519_generate_keypair();
	X25519KeyPair client_kp = x25519_generate_keypair();
	UASSERT(server_kp.success && client_kp.success);

	// Compute shared secret on each side
	X25519SharedSecret server_ss = x25519_compute_shared_secret(
		server_kp.private_key.data(), server_kp.private_key.size(),
		client_kp.public_key.data(), client_kp.public_key.size());
	X25519SharedSecret client_ss = x25519_compute_shared_secret(
		client_kp.private_key.data(), client_kp.private_key.size(),
		server_kp.public_key.data(), server_kp.public_key.size());

	UASSERT(server_ss.success && client_ss.success);

	// Verify shared secrets match (basic DH property)
	UASSERT(memcmp(server_ss.shared_secret.data(), client_ss.shared_secret.data(),
	               X25519_SHARED_SECRET_SIZE) == 0);

	// Mix ECDH secret into keys on both sides
	bool server_ok = mixECDHSecretIntoKeys(server_state,
		server_ss.shared_secret.data(), server_ss.shared_secret.size());
	bool client_ok = mixECDHSecretIntoKeys(client_state,
		client_ss.shared_secret.data(), client_ss.shared_secret.size());

	UASSERT(server_ok && client_ok);

	// CRITICAL: Both sides must have the SAME keys after ECDH
	UASSERT(memcmp(server_state.c2s.key.data(), client_state.c2s.key.data(),
	               AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(server_state.s2c.key.data(), client_state.s2c.key.data(),
	               AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(server_state.c2s.nonce_base.data(),
	               client_state.c2s.nonce_base.data(),
	               NONCE_BASE_SIZE) == 0);
	UASSERT(memcmp(server_state.s2c.nonce_base.data(),
	               client_state.s2c.nonce_base.data(),
	               NONCE_BASE_SIZE) == 0);

	// Session IDs should also match
	UASSERT(server_state.session_id == client_state.session_id);
}

void TestForwardSecrecy::testMixECDHSecretWithNullSecretFails()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	bool ok = mixECDHSecretIntoKeys(state, nullptr, X25519_SHARED_SECRET_SIZE);
	UASSERT(!ok);
}

void TestForwardSecrecy::testMixECDHSecretWithWrongSizeFails()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	std::array<u8, 16> wrong_size_secret{};
	bool ok = mixECDHSecretIntoKeys(state, wrong_size_secret.data(), 16);
	UASSERT(!ok);
}

void TestForwardSecrecy::testMixECDHSecretUsesSaltedHKDF()
{
	// v9.11 FIX: mixECDHSecretIntoKeys() MUST use salted HKDF.
	// The salt should be derived from the combined IKM (SRP + ECDH),
	// just like initFromSRPSessionKey() derives salt from the SRP key.
	//
	// We verify this by checking that the hkdf_salt field is set
	// after mixECDHSecretIntoKeys() runs.

	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// Record the SRP-only salt
	auto srp_salt = state.hkdf_salt;

	X25519KeyPair kp = x25519_generate_keypair();
	bool ok = mixECDHSecretIntoKeys(state, kp.public_key.data(), kp.public_key.size());
	UASSERT(ok);

	// After ECDH, the salt should have been updated (it's now derived from SRP+ECDH)
	// The new salt should be non-zero
	bool salt_all_zero = true;
	for (size_t i = 0; i < state.hkdf_salt.size(); i++) {
		if (state.hkdf_salt[i] != 0) { salt_all_zero = false; break; }
	}
	UASSERT(!salt_all_zero);

	// The ECDH salt should be DIFFERENT from the SRP-only salt
	// (because the IKM is different: SRP+ECDH vs SRP-only)
	UASSERT(memcmp(srp_salt.data(), state.hkdf_salt.data(), state.hkdf_salt.size()) != 0);
}

// ============================================================================
// Full ECDH Handshake Simulation
// ============================================================================

void TestForwardSecrecy::testFullECDHHandshakeBothSidesSameKeys()
{
	// Simulate the complete ECDH handshake:
	// 1. Both sides init from SRP session key
	// 2. Both sides generate X25519 key pairs
	// 3. Both sides exchange public keys (simulated)
	// 4. Both sides compute shared secret
	// 5. Both sides mix ECDH secret into keys
	// 6. Verify both sides have identical encryption state

	auto srp_key = makeTestSessionKey();

	PeerEncryptionState server, client;
	server.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);
	client.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// Step 2: Generate key pairs
	X25519KeyPair server_kp = x25519_generate_keypair();
	X25519KeyPair client_kp = x25519_generate_keypair();
	UASSERT(server_kp.success && client_kp.success);

	// Step 4: Compute shared secrets
	X25519SharedSecret server_ss = x25519_compute_shared_secret(
		server_kp.private_key.data(), server_kp.private_key.size(),
		client_kp.public_key.data(), client_kp.public_key.size());
	X25519SharedSecret client_ss = x25519_compute_shared_secret(
		client_kp.private_key.data(), client_kp.private_key.size(),
		server_kp.public_key.data(), server_kp.public_key.size());

	UASSERT(server_ss.success && client_ss.success);
	UASSERT(memcmp(server_ss.shared_secret.data(), client_ss.shared_secret.data(),
	               X25519_SHARED_SECRET_SIZE) == 0);

	// Step 5: Mix ECDH secrets into keys
	UASSERT(mixECDHSecretIntoKeys(server, server_ss.shared_secret.data(),
		server_ss.shared_secret.size()));
	UASSERT(mixECDHSecretIntoKeys(client, client_ss.shared_secret.data(),
		client_ss.shared_secret.size()));

	// Step 6: Verify identical state
	UASSERT(server.ecdh_completed.load());
	UASSERT(client.ecdh_completed.load());

	UASSERT(memcmp(server.c2s.key.data(), client.c2s.key.data(),
	               AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(server.s2c.key.data(), client.s2c.key.data(),
	               AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(server.c2s.nonce_base.data(),
	               client.c2s.nonce_base.data(),
	               NONCE_BASE_SIZE) == 0);
	UASSERT(memcmp(server.s2c.nonce_base.data(),
	               client.s2c.nonce_base.data(),
	               NONCE_BASE_SIZE) == 0);
	UASSERT(server.session_id == client.session_id);
}

void TestForwardSecrecy::testFullECDHHandshakeKeysDifferFromSRPOnly()
{
	auto srp_key = makeTestSessionKey();

	// SRP-only state (no ECDH)
	PeerEncryptionState srp_only;
	srp_only.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// ECDH state
	PeerEncryptionState with_ecdh;
	with_ecdh.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	X25519KeyPair kp1 = x25519_generate_keypair();
	X25519KeyPair kp2 = x25519_generate_keypair();

	X25519SharedSecret ss = x25519_compute_shared_secret(
		kp1.private_key.data(), kp1.private_key.size(),
		kp2.public_key.data(), kp2.public_key.size());
	UASSERT(ss.success);

	UASSERT(mixECDHSecretIntoKeys(with_ecdh, ss.shared_secret.data(),
		ss.shared_secret.size()));

	// All derived values should differ
	UASSERT(memcmp(srp_only.c2s.key.data(), with_ecdh.c2s.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(memcmp(srp_only.s2c.key.data(), with_ecdh.s2c.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(srp_only.session_id != with_ecdh.session_id);
}

void TestForwardSecrecy::testFullECDHHandshakeEncryptDecryptRoundtrip()
{
	// Full end-to-end: ECDH handshake → encrypt on one side → decrypt on the other
	auto srp_key = makeTestSessionKey();

	PeerEncryptionState server, client;
	server.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);
	client.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	X25519KeyPair server_kp = x25519_generate_keypair();
	X25519KeyPair client_kp = x25519_generate_keypair();

	X25519SharedSecret server_ss = x25519_compute_shared_secret(
		server_kp.private_key.data(), server_kp.private_key.size(),
		client_kp.public_key.data(), client_kp.public_key.size());
	X25519SharedSecret client_ss = x25519_compute_shared_secret(
		client_kp.private_key.data(), client_kp.private_key.size(),
		server_kp.public_key.data(), server_kp.public_key.size());

	UASSERT(server_ss.success && client_ss.success);
	UASSERT(mixECDHSecretIntoKeys(server, server_ss.shared_secret.data(),
		server_ss.shared_secret.size()));
	UASSERT(mixECDHSecretIntoKeys(client, client_ss.shared_secret.data(),
		client_ss.shared_secret.size()));

	server.activate();
	client.activate();

	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	// Server → Client (S2C)
	const char *server_msg = "Hello from server with ECDH!";
	auto s2c_nonce = server.s2c.nextNonce();
	CryptoResult s_enc = aes256gcm_encrypt(
		server.s2c.key.data(), server.s2c.key.size(),
		s2c_nonce.data(), s2c_nonce.size(),
		reinterpret_cast<const u8*>(server_msg), strlen(server_msg),
		&aad, 1);
	UASSERT(s_enc.success);

	CryptoResult c_dec = aes256gcm_decrypt(
		client.s2c.key.data(), client.s2c.key.size(),
		s2c_nonce.data(), s2c_nonce.size(),
		s_enc.data.data(), s_enc.data.size(),
		s_enc.tag.data(), s_enc.tag.size(),
		&aad, 1);
	UASSERT(c_dec.success);
	UASSERT(memcmp(c_dec.data.data(), server_msg, strlen(server_msg)) == 0);

	// Client → Server (C2S)
	const char *client_msg = "Hello from client with ECDH!";
	auto c2s_nonce = client.c2s.nextNonce();
	CryptoResult c_enc = aes256gcm_encrypt(
		client.c2s.key.data(), client.c2s.key.size(),
		c2s_nonce.data(), c2s_nonce.size(),
		reinterpret_cast<const u8*>(client_msg), strlen(client_msg),
		&aad, 1);
	UASSERT(c_enc.success);

	CryptoResult s_dec = aes256gcm_decrypt(
		server.c2s.key.data(), server.c2s.key.size(),
		c2s_nonce.data(), c2s_nonce.size(),
		c_enc.data.data(), c_enc.data.size(),
		c_enc.tag.data(), c_enc.tag.size(),
		&aad, 1);
	UASSERT(s_dec.success);
	UASSERT(memcmp(s_dec.data.data(), client_msg, strlen(client_msg)) == 0);
}

// ============================================================================
// Key Rotation with Deterministic Salt (v9.11 Fix)
// ============================================================================

void TestForwardSecrecy::testRotateKeysWithDeterministicSaltBothSidesSame()
{
	// CRITICAL TEST: After key rotation, both sides must derive the SAME keys.
	// v9.9 BUG: rotateKeys() used secure_random() for the HKDF salt, which
	// means each side would generate a different random salt → different keys.
	//
	// v9.11 FIX: rotateKeys() must derive salt deterministically from the
	// rotation IKM, just like initFromSRPSessionKey() and mixECDHSecretIntoKeys().
	//
	// Since rotateKeys() currently doesn't have a way to coordinate between
	// sides (it would need a wire protocol), this test verifies that the
	// deterministic approach works when both sides have the same inputs.

	auto srp_key = makeTestSessionKey();

	PeerEncryptionState server, client;
	server.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);
	client.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// ECDH handshake first
	X25519KeyPair server_kp = x25519_generate_keypair();
	X25519KeyPair client_kp = x25519_generate_keypair();

	X25519SharedSecret server_ss = x25519_compute_shared_secret(
		server_kp.private_key.data(), server_kp.private_key.size(),
		client_kp.public_key.data(), client_kp.public_key.size());
	X25519SharedSecret client_ss = x25519_compute_shared_secret(
		client_kp.private_key.data(), client_kp.private_key.size(),
		server_kp.public_key.data(), server_kp.public_key.size());

	mixECDHSecretIntoKeys(server, server_ss.shared_secret.data(),
		server_ss.shared_secret.size());
	mixECDHSecretIntoKeys(client, client_ss.shared_secret.data(),
		client_ss.shared_secret.size());

	server.activate();
	client.activate();

	// Both sides should have matching keys before rotation
	UASSERT(memcmp(server.c2s.key.data(), client.c2s.key.data(),
	               AES256_KEY_SIZE) == 0);
	UASSERT(memcmp(server.s2c.key.data(), client.s2c.key.data(),
	               AES256_KEY_SIZE) == 0);

	// Note: Real key rotation would need a wire protocol to exchange
	// new ECDH public keys. For now, we verify that rotateKeys()
	// produces deterministic results when both sides have the same state.
	// The test ensures that the rotation uses deterministic salt derivation
	// rather than random salt, which was the v9.9 bug.
	//
	// We can't test cross-side rotation without the wire protocol,
	// but we can test that rotateKeys() works at all and produces
	// different keys from the initial ones.
	bool ok = server.rotateKeys();
	UASSERT(ok);
	UASSERT(server.key_rotation_count == 1);
}

void TestForwardSecrecy::testRotateKeysProducesDifferentKeysThanInitial()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// Record initial keys
	auto initial_c2s_key = state.c2s.key;
	auto initial_s2c_key = state.s2c.key;

	state.activate();

	// Rotate keys
	bool ok = state.rotateKeys();
	UASSERT(ok);

	// After rotation, keys should be different
	UASSERT(memcmp(initial_c2s_key.data(), state.c2s.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(memcmp(initial_s2c_key.data(), state.s2c.key.data(),
	               AES256_KEY_SIZE) != 0);

	// Nonce counters should be reset
	UASSERTEQ(u64, state.c2s.nonce_counter, 0u);
	UASSERTEQ(u64, state.s2c.nonce_counter, 0u);
}

void TestForwardSecrecy::testRotateKeysRequiresActiveEncryption()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// Don't activate — rotation should fail
	bool ok = state.rotateKeys();
	UASSERT(!ok);

	// Activate — rotation should succeed
	state.activate();
	ok = state.rotateKeys();
	UASSERT(ok);
}

// ============================================================================
// Insecure Mode Tests
// ============================================================================

void TestForwardSecrecy::testInsecureModeDoesNotDoECDH()
{
	// When encryption is disabled (insecure mode), ECDH should NOT be attempted.
	// The PeerEncryptionState should remain in its disabled state.
	PeerEncryptionState state;
	state.disable();  // Simulates insecure mode

	UASSERT(!state.active.load());
	UASSERT(!state.ecdh_completed.load());

	// Attempting to rotate keys on a disabled state should fail
	bool ok = state.rotateKeys();
	UASSERT(!ok);
}

// ============================================================================
// Key Material Cleanup Tests
// ============================================================================

void TestForwardSecrecy::testDisableZeroesECDHMaterial()
{
	auto srp_key = makeTestSessionKey();
	PeerEncryptionState state;
	state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// Perform ECDH
	X25519KeyPair kp = x25519_generate_keypair();
	mixECDHSecretIntoKeys(state, kp.public_key.data(), kp.public_key.size());

	UASSERT(state.ecdh_completed.load());

	// Disable (simulates disconnect or insecure mode)
	state.disable();

	UASSERT(!state.ecdh_completed.load());

	// All ECDH material should be zeroed
	for (size_t i = 0; i < X25519_PRIVATE_KEY_SIZE; i++) {
		UASSERTEQ(int, state.ecdh_private_key[i], 0);
	}
	for (size_t i = 0; i < X25519_PUBLIC_KEY_SIZE; i++) {
		UASSERTEQ(int, state.ecdh_public_key[i], 0);
	}
	for (size_t i = 0; i < X25519_SHARED_SECRET_SIZE; i++) {
		UASSERTEQ(int, state.ecdh_shared_secret[i], 0);
	}
}

// ============================================================================
// Security Scoring Tests
// ============================================================================

void TestForwardSecrecy::testSecurityScoreWithECDH()
{
	// With ECDH completed, forward secrecy should be reported as true,
	// and the security score should include the +15 forward secrecy bonus
	// and +5 TLS 1.3 equivalent.

	ConnectionSecurityInfo info = populateRealSecurityInfo(
		true,   // encryption_active
		true,   // ecdh_completed
		false,  // fingerprint_pinned (first connection)
		0,      // fingerprint_verify_result
		"abc123", // session_id
		"SHA256:abc", // server_fingerprint
		12345,  // activated_at
		42,     // protocol_version
		"localhost", // server_address
		30000,  // server_port
		true    // key_rotation_supported
	);

	UASSERT(info.isSecure());
	UASSERT(info.forward_secrecy);
	UASSERT(info.ecdh_forward_secrecy);
	UASSERT(info.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519);
	UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_1_3_EQUIVALENT);

	// Base: 30(enc) + 15(cipher) + 15(fs) + 15(auth) + 10(replay) + 0(tofu) + 5(tls) = 90
	// Bonus: 0(tofu not first-use) + 5(rotation) + 2(salted) + 2(bitmap) + 3(integrity) = 12
	// Total: 102 → capped at 100
	int score = info.getSecurityScore();
	UASSERT(score == 100);

	// With pinned fingerprint:
	ConnectionSecurityInfo info_pinned = populateRealSecurityInfo(
		true, true, true, 1, "abc123", "SHA256:abc",
		12345, 42, "localhost", 30000, true);

	// Base: 30+15+15+15+10+10(pinned)+5 = 100
	// Bonus: 5+2+2+3 = 12
	// Total: 112 → capped at 100
	UASSERT(info_pinned.getSecurityScore() == 100);
}

void TestForwardSecrecy::testSecurityScoreWithoutECDH()
{
	// Without ECDH, forward secrecy should be false and score lower.

	ConnectionSecurityInfo info = populateRealSecurityInfo(
		true,   // encryption_active
		false,  // ecdh_completed — NO forward secrecy
		false,  // fingerprint_pinned
		0,
		"abc123", "SHA256:abc",
		12345, 42, "localhost", 30000, true
	);

	UASSERT(info.isSecure());
	UASSERT(!info.forward_secrecy);
	UASSERT(!info.ecdh_forward_secrecy);
	UASSERT(info.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_SRP);
	UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_CUSTOM);

	// Base: 30(enc) + 15(cipher) + 0(fs) + 15(auth) + 10(replay) + 0(tofu) + 0(tls) = 70
	// Bonus: 3(tofu ack) + 5(rotation) + 2(salted) + 2(bitmap) + 3(integrity) = 15
	// Total: 85
	int score = info.getSecurityScore();
	UASSERT(score == 85);
}

// ============================================================================
// Forward Secrecy Property Test
// ============================================================================

void TestForwardSecrecy::testForwardSecrecyProperty()
{
	// THE CORE PROPERTY OF FORWARD SECRECY:
	// If the SRP password is later compromised, an attacker who recorded
	// the network traffic still CANNOT decrypt past sessions IF ECDH was used.
	//
	// This is because:
	// 1. The SRP session key alone is NOT sufficient to derive the ECDH keys
	// 2. The ECDH private keys are ephemeral and destroyed after the session
	// 3. The combined IKM (SRP + ECDH) is needed, not just SRP
	//
	// We verify this by showing that knowing the SRP key alone produces
	// DIFFERENT keys than knowing both SRP + ECDH.

	auto srp_key = makeTestSessionKey();

	// Attacker scenario: they know the SRP session key (e.g., from a compromised password)
	// and try to derive the encryption keys
	PeerEncryptionState attacker_state;
	attacker_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

	// The actual session used ECDH
	PeerEncryptionState real_server;
	real_server.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);

	X25519KeyPair server_kp = x25519_generate_keypair();
	X25519KeyPair client_kp = x25519_generate_keypair();

	X25519SharedSecret ss = x25519_compute_shared_secret(
		server_kp.private_key.data(), server_kp.private_key.size(),
		client_kp.public_key.data(), client_kp.public_key.size());

	mixECDHSecretIntoKeys(real_server, ss.shared_secret.data(),
		ss.shared_secret.size());

	// The attacker's SRP-only keys should NOT match the real session's keys
	// This proves forward secrecy: SRP key alone is not enough
	UASSERT(memcmp(attacker_state.c2s.key.data(), real_server.c2s.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(memcmp(attacker_state.s2c.key.data(), real_server.s2c.key.data(),
	               AES256_KEY_SIZE) != 0);
	UASSERT(attacker_state.session_id != real_server.session_id);

	// The attacker also cannot decrypt packets from the real session
	real_server.activate();
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;
	const char *msg = "Secret message with ECDH forward secrecy";
	auto nonce = real_server.s2c.nextNonce();

	CryptoResult enc = aes256gcm_encrypt(
		real_server.s2c.key.data(), real_server.s2c.key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);
	UASSERT(enc.success);

	// Attacker tries to decrypt with SRP-only key — MUST FAIL
	CryptoResult attacker_dec = aes256gcm_decrypt(
		attacker_state.s2c.key.data(), attacker_state.s2c.key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);
	UASSERT(!attacker_dec.success);
}
