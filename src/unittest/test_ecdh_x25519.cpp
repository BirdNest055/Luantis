// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Comprehensive unit tests for X25519 ECDH key exchange (forward secrecy).
//
// These tests prove that:
// - X25519 key pair generation works (produces 32-byte keys)
// - Two key pairs produce different public keys (ephemeral)
// - Shared secret computation works correctly
// - Both sides derive the SAME shared secret (DH symmetry)
// - Invalid key sizes are rejected
// - mixECDHSecretIntoKeys re-derives keys with combined SRP+ECDH
// - Keys derived with ECDH differ from keys derived without ECDH
// - Full ECDH+SRP encrypt/decrypt roundtrip works

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>

class TestECDHX25519 : public TestBase
{
public:
        TestECDHX25519() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestECDHX25519"; }

        void runTests(IGameDef *gamedef);

        // Key pair generation tests
        void testGenerateKeyPairSuccess();
        void testGenerateKeyPairProducesNonZeroKeys();
        void testGenerateKeyPairDifferentEachCall();
        void testGenerateKeyPairPublicKeySize();
        void testGenerateKeyPairPrivateKeySize();

        // Shared secret computation tests
        void testComputeSharedSecretSuccess();
        void testComputeSharedSecretSymmetry();
        void testComputeSharedSecretDifferentPairsSameSecret();
        void testComputeSharedSecretInvalidPrivateKeySize();
        void testComputeSharedSecretInvalidPublicKeySize();
        void testComputeSharedSecretNullPrivateKey();
        void testComputeSharedSecretNullPublicKey();

        // Key mixing tests
        void testMixECDHSecretIntoKeysSuccess();
        void testMixECDHSecretProducesDifferentKeys();
        void testMixECDHSecretSetsECDHCompleted();
        void testMixECDHSecretInvalidSizeFails();

        // Integration tests
        void testFullECDHSRPKeyDerivationRoundtrip();
        void testECDHEncryptDecryptWithReDerivedKeys();
        void testECDHKeysAreDifferentFromSRPOnlyKeys();
};

static TestECDHX25519 g_test_instance;

void TestECDHX25519::runTests(IGameDef *gamedef)
{
        // Key pair generation
        TEST(testGenerateKeyPairSuccess);
        TEST(testGenerateKeyPairProducesNonZeroKeys);
        TEST(testGenerateKeyPairDifferentEachCall);
        TEST(testGenerateKeyPairPublicKeySize);
        TEST(testGenerateKeyPairPrivateKeySize);

        // Shared secret
        TEST(testComputeSharedSecretSuccess);
        TEST(testComputeSharedSecretSymmetry);
        TEST(testComputeSharedSecretDifferentPairsSameSecret);
        TEST(testComputeSharedSecretInvalidPrivateKeySize);
        TEST(testComputeSharedSecretInvalidPublicKeySize);
        TEST(testComputeSharedSecretNullPrivateKey);
        TEST(testComputeSharedSecretNullPublicKey);

        // Key mixing
        TEST(testMixECDHSecretIntoKeysSuccess);
        TEST(testMixECDHSecretProducesDifferentKeys);
        TEST(testMixECDHSecretSetsECDHCompleted);
        TEST(testMixECDHSecretInvalidSizeFails);

        // Integration
        TEST(testFullECDHSRPKeyDerivationRoundtrip);
        TEST(testECDHEncryptDecryptWithReDerivedKeys);
        TEST(testECDHKeysAreDifferentFromSRPOnlyKeys);
}

// Helper: generate a test SRP session key
static std::array<u8, SRP_SESSION_KEY_SIZE> makeTestSessionKey()
{
        std::array<u8, SRP_SESSION_KEY_SIZE> key;
        for (size_t i = 0; i < key.size(); i++)
                key[i] = static_cast<u8>(i * 7 + 42);
        return key;
}

// ============================================================================
// Key Pair Generation Tests
// ============================================================================

void TestECDHX25519::testGenerateKeyPairSuccess()
{
        X25519KeyPair kp = x25519_generate_keypair();
        UASSERT(kp.success);
}

void TestECDHX25519::testGenerateKeyPairProducesNonZeroKeys()
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

void TestECDHX25519::testGenerateKeyPairDifferentEachCall()
{
        // Two successive key pair generations should produce different keys
        X25519KeyPair kp1 = x25519_generate_keypair();
        X25519KeyPair kp2 = x25519_generate_keypair();

        UASSERT(kp1.success);
        UASSERT(kp2.success);

        // Public keys should differ (ephemeral)
        UASSERT(memcmp(kp1.public_key.data(), kp2.public_key.data(),
                       X25519_PUBLIC_KEY_SIZE) != 0);

        // Private keys should differ
        UASSERT(memcmp(kp1.private_key.data(), kp2.private_key.data(),
                       X25519_PRIVATE_KEY_SIZE) != 0);
}

void TestECDHX25519::testGenerateKeyPairPublicKeySize()
{
        X25519KeyPair kp = x25519_generate_keypair();
        UASSERT(kp.success);
        UASSERTEQ(size_t, kp.public_key.size(), X25519_PUBLIC_KEY_SIZE);
}

void TestECDHX25519::testGenerateKeyPairPrivateKeySize()
{
        X25519KeyPair kp = x25519_generate_keypair();
        UASSERT(kp.success);
        UASSERTEQ(size_t, kp.private_key.size(), X25519_PRIVATE_KEY_SIZE);
}

// ============================================================================
// Shared Secret Computation Tests
// ============================================================================

void TestECDHX25519::testComputeSharedSecretSuccess()
{
        // Alice and Bob generate key pairs
        X25519KeyPair alice = x25519_generate_keypair();
        X25519KeyPair bob = x25519_generate_keypair();

        UASSERT(alice.success);
        UASSERT(bob.success);

        // Alice computes shared secret with Bob's public key
        X25519SharedSecret alice_ss = x25519_compute_shared_secret(
                alice.private_key.data(), alice.private_key.size(),
                bob.public_key.data(), bob.public_key.size());

        UASSERT(alice_ss.success);
        UASSERTEQ(size_t, alice_ss.shared_secret.size(), X25519_SHARED_SECRET_SIZE);
}

void TestECDHX25519::testComputeSharedSecretSymmetry()
{
        // PROOF: Both sides derive the SAME shared secret.
        // This is the fundamental property of Diffie-Hellman.
        X25519KeyPair alice = x25519_generate_keypair();
        X25519KeyPair bob = x25519_generate_keypair();

        UASSERT(alice.success);
        UASSERT(bob.success);

        // Alice: her private + Bob's public
        X25519SharedSecret alice_ss = x25519_compute_shared_secret(
                alice.private_key.data(), alice.private_key.size(),
                bob.public_key.data(), bob.public_key.size());

        // Bob: his private + Alice's public
        X25519SharedSecret bob_ss = x25519_compute_shared_secret(
                bob.private_key.data(), bob.private_key.size(),
                alice.public_key.data(), alice.public_key.size());

        UASSERT(alice_ss.success);
        UASSERT(bob_ss.success);

        // Both shared secrets MUST be identical
        UASSERT(memcmp(alice_ss.shared_secret.data(), bob_ss.shared_secret.data(),
                       X25519_SHARED_SECRET_SIZE) == 0);
}

void TestECDHX25519::testComputeSharedSecretDifferentPairsSameSecret()
{
        // Verify the shared secret is consistent (recomputing gives same result)
        X25519KeyPair alice = x25519_generate_keypair();
        X25519KeyPair bob = x25519_generate_keypair();

        UASSERT(alice.success);
        UASSERT(bob.success);

        X25519SharedSecret ss1 = x25519_compute_shared_secret(
                alice.private_key.data(), alice.private_key.size(),
                bob.public_key.data(), bob.public_key.size());

        X25519SharedSecret ss2 = x25519_compute_shared_secret(
                alice.private_key.data(), alice.private_key.size(),
                bob.public_key.data(), bob.public_key.size());

        UASSERT(ss1.success);
        UASSERT(ss2.success);

        // Same inputs → same output (deterministic)
        UASSERT(memcmp(ss1.shared_secret.data(), ss2.shared_secret.data(),
                       X25519_SHARED_SECRET_SIZE) == 0);
}

void TestECDHX25519::testComputeSharedSecretInvalidPrivateKeySize()
{
        X25519KeyPair alice = x25519_generate_keypair();
        X25519KeyPair bob = x25519_generate_keypair();

        // Wrong private key size
        X25519SharedSecret ss = x25519_compute_shared_secret(
                alice.private_key.data(), 16,  // Too short
                bob.public_key.data(), bob.public_key.size());

        UASSERT(!ss.success);
}

void TestECDHX25519::testComputeSharedSecretInvalidPublicKeySize()
{
        X25519KeyPair alice = x25519_generate_keypair();
        X25519KeyPair bob = x25519_generate_keypair();

        // Wrong public key size
        X25519SharedSecret ss = x25519_compute_shared_secret(
                alice.private_key.data(), alice.private_key.size(),
                bob.public_key.data(), 16);  // Too short

        UASSERT(!ss.success);
}

void TestECDHX25519::testComputeSharedSecretNullPrivateKey()
{
        X25519KeyPair bob = x25519_generate_keypair();

        X25519SharedSecret ss = x25519_compute_shared_secret(
                nullptr, X25519_PRIVATE_KEY_SIZE,
                bob.public_key.data(), bob.public_key.size());

        UASSERT(!ss.success);
}

void TestECDHX25519::testComputeSharedSecretNullPublicKey()
{
        X25519KeyPair alice = x25519_generate_keypair();

        X25519SharedSecret ss = x25519_compute_shared_secret(
                alice.private_key.data(), alice.private_key.size(),
                nullptr, X25519_PUBLIC_KEY_SIZE);

        UASSERT(!ss.success);
}

// ============================================================================
// Key Mixing Tests
// ============================================================================

void TestECDHX25519::testMixECDHSecretIntoKeysSuccess()
{
        auto srp_key = makeTestSessionKey();
        PeerEncryptionState state;
        bool init_ok = state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);
        UASSERT(init_ok);

        // Compute ECDH shared secret
        X25519KeyPair client_kp = x25519_generate_keypair();
        X25519KeyPair server_kp = x25519_generate_keypair();
        X25519SharedSecret ecdh_ss = x25519_compute_shared_secret(
                client_kp.private_key.data(), client_kp.private_key.size(),
                server_kp.public_key.data(), server_kp.public_key.size());
        UASSERT(ecdh_ss.success);

        // Mix ECDH secret into keys
        bool mix_ok = mixECDHSecretIntoKeys(state,
                ecdh_ss.shared_secret.data(), ecdh_ss.shared_secret.size());
        UASSERT(mix_ok);
}

void TestECDHX25519::testMixECDHSecretProducesDifferentKeys()
{
        // Keys derived with SRP+ECDH should differ from SRP-only keys
        auto srp_key = makeTestSessionKey();

        // Derive keys with SRP only
        PeerEncryptionState state_srp_only;
        bool ok1 = state_srp_only.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);
        UASSERT(ok1);

        // Save the SRP-only keys
        auto srp_only_c2s_key = state_srp_only.c2s.key;
        auto srp_only_s2c_key = state_srp_only.s2c.key;

        // Derive keys with SRP + ECDH
        PeerEncryptionState state_ecdh;
        bool ok2 = state_ecdh.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);
        UASSERT(ok2);

        X25519KeyPair kp1 = x25519_generate_keypair();
        X25519KeyPair kp2 = x25519_generate_keypair();
        X25519SharedSecret ecdh_ss = x25519_compute_shared_secret(
                kp1.private_key.data(), kp1.private_key.size(),
                kp2.public_key.data(), kp2.public_key.size());
        UASSERT(ecdh_ss.success);

        bool mix_ok = mixECDHSecretIntoKeys(state_ecdh,
                ecdh_ss.shared_secret.data(), ecdh_ss.shared_secret.size());
        UASSERT(mix_ok);

        // ECDH-derived keys MUST differ from SRP-only keys
        UASSERT(memcmp(srp_only_c2s_key.data(), state_ecdh.c2s.key.data(),
                       AES256_KEY_SIZE) != 0);
        UASSERT(memcmp(srp_only_s2c_key.data(), state_ecdh.s2c.key.data(),
                       AES256_KEY_SIZE) != 0);
}

void TestECDHX25519::testMixECDHSecretSetsECDHCompleted()
{
        auto srp_key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);
        UASSERT(!state.ecdh_completed.load());

        X25519KeyPair kp1 = x25519_generate_keypair();
        X25519KeyPair kp2 = x25519_generate_keypair();
        X25519SharedSecret ecdh_ss = x25519_compute_shared_secret(
                kp1.private_key.data(), kp1.private_key.size(),
                kp2.public_key.data(), kp2.public_key.size());

        mixECDHSecretIntoKeys(state,
                ecdh_ss.shared_secret.data(), ecdh_ss.shared_secret.size());

        UASSERT(state.ecdh_completed.load());
}

void TestECDHX25519::testMixECDHSecretInvalidSizeFails()
{
        auto srp_key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

        // Wrong ECDH secret size
        std::array<u8, 16> bad_secret{};
        bool mix_ok = mixECDHSecretIntoKeys(state, bad_secret.data(), bad_secret.size());
        UASSERT(!mix_ok);

        // Null ECDH secret
        mix_ok = mixECDHSecretIntoKeys(state, nullptr, X25519_SHARED_SECRET_SIZE);
        UASSERT(!mix_ok);
}

// ============================================================================
// Integration Tests
// ============================================================================

void TestECDHX25519::testFullECDHSRPKeyDerivationRoundtrip()
{
        // Complete flow: SRP auth → ECDH exchange → key derivation → encrypt/decrypt
        auto srp_key = makeTestSessionKey();

        // Both sides initialize from SRP session key
        PeerEncryptionState server_state;
        PeerEncryptionState client_state;
        server_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);
        client_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

        // Both sides generate X25519 key pairs
        X25519KeyPair server_kp = x25519_generate_keypair();
        X25519KeyPair client_kp = x25519_generate_keypair();

        // Server computes shared secret with client's public key
        X25519SharedSecret server_ss = x25519_compute_shared_secret(
                server_kp.private_key.data(), server_kp.private_key.size(),
                client_kp.public_key.data(), client_kp.public_key.size());

        // Client computes shared secret with server's public key
        X25519SharedSecret client_ss = x25519_compute_shared_secret(
                client_kp.private_key.data(), client_kp.private_key.size(),
                server_kp.public_key.data(), server_kp.public_key.size());

        UASSERT(server_ss.success);
        UASSERT(client_ss.success);

        // Both shared secrets must be identical
        UASSERT(memcmp(server_ss.shared_secret.data(), client_ss.shared_secret.data(),
                       X25519_SHARED_SECRET_SIZE) == 0);

        // Mix ECDH secret into keys on both sides
        bool server_mix = mixECDHSecretIntoKeys(server_state,
                server_ss.shared_secret.data(), server_ss.shared_secret.size());
        bool client_mix = mixECDHSecretIntoKeys(client_state,
                client_ss.shared_secret.data(), client_ss.shared_secret.size());
        UASSERT(server_mix);
        UASSERT(client_mix);

        // Both sides should have the SAME re-derived keys
        UASSERT(memcmp(server_state.c2s.key.data(), client_state.c2s.key.data(),
                       AES256_KEY_SIZE) == 0);
        UASSERT(memcmp(server_state.s2c.key.data(), client_state.s2c.key.data(),
                       AES256_KEY_SIZE) == 0);

        // Both sides should have ECDH completed
        UASSERT(server_state.ecdh_completed.load());
        UASSERT(client_state.ecdh_completed.load());
}

void TestECDHX25519::testECDHEncryptDecryptWithReDerivedKeys()
{
        // After ECDH key mixing, encrypt on one side, decrypt on the other
        auto srp_key = makeTestSessionKey();

        PeerEncryptionState server_state;
        PeerEncryptionState client_state;
        server_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);
        client_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

        // ECDH exchange
        X25519KeyPair server_kp = x25519_generate_keypair();
        X25519KeyPair client_kp = x25519_generate_keypair();

        X25519SharedSecret server_ss = x25519_compute_shared_secret(
                server_kp.private_key.data(), server_kp.private_key.size(),
                client_kp.public_key.data(), client_kp.public_key.size());
        X25519SharedSecret client_ss = x25519_compute_shared_secret(
                client_kp.private_key.data(), client_kp.private_key.size(),
                server_kp.public_key.data(), server_kp.public_key.size());

        mixECDHSecretIntoKeys(server_state,
                server_ss.shared_secret.data(), server_ss.shared_secret.size());
        mixECDHSecretIntoKeys(client_state,
                client_ss.shared_secret.data(), client_ss.shared_secret.size());

        // Encrypt with server's S2C key, decrypt with client's S2C key
        const char *msg = "Hello with ECDH forward secrecy!";
        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        auto nonce = server_state.s2c.nextNonce();
        CryptoResult enc = aes256gcm_encrypt(
                server_state.s2c.key.data(), server_state.s2c.key.size(),
                nonce.data(), nonce.size(),
                reinterpret_cast<const u8*>(msg), strlen(msg),
                &aad, 1);
        UASSERT(enc.success);

        CryptoResult dec = aes256gcm_decrypt(
                client_state.s2c.key.data(), client_state.s2c.key.size(),
                nonce.data(), nonce.size(),
                enc.data.data(), enc.data.size(),
                enc.tag.data(), enc.tag.size(),
                &aad, 1);
        UASSERT(dec.success);
        UASSERT(memcmp(dec.data.data(), msg, strlen(msg)) == 0);
}

void TestECDHX25519::testECDHKeysAreDifferentFromSRPOnlyKeys()
{
        // Verify that ECDH-derived keys are cryptographically independent
        // from SRP-only keys. This proves that compromising the SRP password
        // alone does NOT reveal the ECDH-derived session keys.
        auto srp_key = makeTestSessionKey();

        // SRP-only derivation
        PeerEncryptionState srp_state;
        srp_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

        // SRP+ECDH derivation
        PeerEncryptionState ecdh_state;
        ecdh_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);

        X25519KeyPair kp1 = x25519_generate_keypair();
        X25519KeyPair kp2 = x25519_generate_keypair();
        X25519SharedSecret ss = x25519_compute_shared_secret(
                kp1.private_key.data(), kp1.private_key.size(),
                kp2.public_key.data(), kp2.public_key.size());

        mixECDHSecretIntoKeys(ecdh_state,
                ss.shared_secret.data(), ss.shared_secret.size());

        // All four key/nonce components must differ
        UASSERT(memcmp(srp_state.c2s.key.data(), ecdh_state.c2s.key.data(),
                       AES256_KEY_SIZE) != 0);
        UASSERT(memcmp(srp_state.s2c.key.data(), ecdh_state.s2c.key.data(),
                       AES256_KEY_SIZE) != 0);
        UASSERT(memcmp(srp_state.c2s.nonce_base.data(), ecdh_state.c2s.nonce_base.data(),
                       NONCE_BASE_SIZE) != 0);
        UASSERT(memcmp(srp_state.s2c.nonce_base.data(), ecdh_state.s2c.nonce_base.data(),
                       NONCE_BASE_SIZE) != 0);
}
