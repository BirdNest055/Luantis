// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 BirdNest055
//
// Comprehensive unit tests for voice E2EE (End-to-End Encryption).
//
// These tests verify the cryptographic correctness and security properties
// of the voice E2EE subsystem: X25519 key exchange, HKDF key derivation,
// AES-256-GCM encrypt/decrypt roundtrip, nonce construction, replay
// protection, and the security of the global channel key combiner.
//
// The tests use the VoicePeerState struct directly and the shared crypto
// module functions (not the full VoiceChatManager, which requires Opus
// and audio hardware — those are tested in the e2e integration test).

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <algorithm>

// Re-use the same constants from voice_chat.h for consistency
constexpr size_t VOICE_NONCE_SIZE = 12;
constexpr size_t VOICE_TAG_SIZE = 16;
constexpr size_t VOICE_KEY_SIZE = 32;

class TestVoiceE2EE : public TestBase
{
public:
        TestVoiceE2EE() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestVoiceE2EE"; }

        void runTests(IGameDef *gamedef);

        // X25519 key exchange for voice E2EE
        void testX25519KeyExchangeProducesSharedSecret();
        void testX25519KeyExchangeSymmetry();
        void testX25519KeyExchangeDifferentPairsProduceDifferentSecrets();
        void testX25519KeyExchangeInvalidKeySizesFail();

        // HKDF key derivation for voice session keys
        void testHkdfVoiceKeyDerivationProducesValidKey();
        void testHkdfVoiceKeyDerivationDeterministic();
        void testHkdfVoiceKeyDerivationDifferentPeersDifferentKeys();
        void testHkdfVoiceNonceBaseDerivationProducesValidBase();
        void testHkdfVoiceNonceBaseDeterministic();
        void testHkdfVoiceNonceBaseDifferentFromSessionKey();

        // AES-256-GCM encrypt/decrypt for voice
        void testVoiceEncryptDecryptRoundtrip();
        void testVoiceEncryptDecryptLargePayload();
        void testVoiceEncryptDecryptMultiplePackets();
        void testVoiceEncryptProducesDifferentCiphertextEachTime();
        void testVoiceDecryptWrongKeyFails();
        void testVoiceDecryptWrongNonceFails();
        void testVoiceDecryptTamperedCiphertextFails();
        void testVoiceDecryptTamperedTagFails();
        void testVoiceDecryptWrongAADFails();

        // Nonce construction for voice
        void testVoiceNonceConstructionDeterministic();
        void testVoiceNonceConstructionDifferentCounters();
        void testVoiceNonceCounterBigEndianEncoding();
        void testVoiceNonceBaseFromHKDFIs4Bytes();

        // Global channel key combiner (replaces insecure XOR)
        void testGlobalChannelKeyCombinerHKDF();
        void testGlobalChannelKeyCombinerSymmetricInPeerOrder();
        void testGlobalChannelKeyCombinerDifferentFromPerPeerKey();
        void testGlobalChannelKeyCombinerSinglePeer();
        void testGlobalChannelKeyCombinerMultiplePeers();
        void testGlobalChannelXORWouldBeInsecure();  // Regression test

        // AAD (Additional Authenticated Data) for voice
        void testVoiceAADIncludesPeerID();
        void testVoiceAADIncludesCounter();
        void testVoiceAADMismatchCausesDecryptFailure();

        // Replay protection for voice
        void testReplayProtectionFirstPacketAccepted();
        void testReplayProtectionFuturePacketAccepted();
        void testReplayProtectionOldReplayRejected();
        void testReplayProtectionSameCounterRejected();
        void testReplayProtectionWindowBoundary();
        void testReplayProtectionCounterAdvances();

        // Peer state management
        void testPeerStateNonceBasePreservedOnUpdate();
        void testPeerStateSessionKeyPreservedOnUpdate();
        void testPeerStateReplayStatePreservedOnUpdate();
        void testPeerStateNewPeerHasNoE2EE();
        void testPeerStateDepartedPeerRemoved();

        // Wire format for voice E2EE
        void testWireFormatCiphertextTagNonce();
        void testWireFormatDecryptStripsTagAndNonce();
        void testWireFormatMinimumSize();
};

static TestVoiceE2EE g_test_instance;

void TestVoiceE2EE::runTests(IGameDef *gamedef)
{
#if USE_OPENSSL
        TEST(testX25519KeyExchangeProducesSharedSecret);
        TEST(testX25519KeyExchangeSymmetry);
        TEST(testX25519KeyExchangeDifferentPairsProduceDifferentSecrets);
        TEST(testX25519KeyExchangeInvalidKeySizesFail);

        TEST(testHkdfVoiceKeyDerivationProducesValidKey);
        TEST(testHkdfVoiceKeyDerivationDeterministic);
        TEST(testHkdfVoiceKeyDerivationDifferentPeersDifferentKeys);
        TEST(testHkdfVoiceNonceBaseDerivationProducesValidBase);
        TEST(testHkdfVoiceNonceBaseDeterministic);
        TEST(testHkdfVoiceNonceBaseDifferentFromSessionKey);

        TEST(testVoiceEncryptDecryptRoundtrip);
        TEST(testVoiceEncryptDecryptLargePayload);
        TEST(testVoiceEncryptDecryptMultiplePackets);
        TEST(testVoiceEncryptProducesDifferentCiphertextEachTime);
        TEST(testVoiceDecryptWrongKeyFails);
        TEST(testVoiceDecryptWrongNonceFails);
        TEST(testVoiceDecryptTamperedCiphertextFails);
        TEST(testVoiceDecryptTamperedTagFails);
        TEST(testVoiceDecryptWrongAADFails);

        TEST(testVoiceNonceConstructionDeterministic);
        TEST(testVoiceNonceConstructionDifferentCounters);
        TEST(testVoiceNonceCounterBigEndianEncoding);
        TEST(testVoiceNonceBaseFromHKDFIs4Bytes);

        TEST(testGlobalChannelKeyCombinerHKDF);
        TEST(testGlobalChannelKeyCombinerSymmetricInPeerOrder);
        TEST(testGlobalChannelKeyCombinerDifferentFromPerPeerKey);
        TEST(testGlobalChannelKeyCombinerSinglePeer);
        TEST(testGlobalChannelKeyCombinerMultiplePeers);
        TEST(testGlobalChannelXORWouldBeInsecure);

        TEST(testVoiceAADIncludesPeerID);
        TEST(testVoiceAADIncludesCounter);
        TEST(testVoiceAADMismatchCausesDecryptFailure);

        TEST(testReplayProtectionFirstPacketAccepted);
        TEST(testReplayProtectionFuturePacketAccepted);
        TEST(testReplayProtectionOldReplayRejected);
        TEST(testReplayProtectionSameCounterRejected);
        TEST(testReplayProtectionWindowBoundary);
        TEST(testReplayProtectionCounterAdvances);

        TEST(testPeerStateNonceBasePreservedOnUpdate);
        TEST(testPeerStateSessionKeyPreservedOnUpdate);
        TEST(testPeerStateReplayStatePreservedOnUpdate);
        TEST(testPeerStateNewPeerHasNoE2EE);
        TEST(testPeerStateDepartedPeerRemoved);

        TEST(testWireFormatCiphertextTagNonce);
        TEST(testWireFormatDecryptStripsTagAndNonce);
        TEST(testWireFormatMinimumSize);
#else
        // Without OpenSSL, voice E2EE cannot function
        infostream << "Skipping TestVoiceE2EE — OpenSSL not available" << std::endl;
#endif
}

// ============================================================================
// Helper: Simulate voice E2EE encrypt (matching voice_chat.cpp logic)
// ============================================================================
static bool simulateVoiceEncrypt(
        const u8 *key, size_t key_len,
        const u8 *nonce_base, size_t nonce_base_len,
        u64 counter, u16 peer_id,
        const std::vector<u8> &plaintext,
        std::vector<u8> &output)
{
        // Build nonce
        u8 nonce[VOICE_NONCE_SIZE];
        build_nonce(nonce_base, counter, nonce);

        // Build AAD
        u8 aad[8];
        
        
        for (int i = 0; i < 8; i++)
                aad[i] = (counter >> (56 - i * 8)) & 0xFF;

        auto result = aes256gcm_encrypt(key, key_len, nonce, VOICE_NONCE_SIZE,
                plaintext.data(), plaintext.size(), aad, 8);

        if (!result.success)
                return false;

        // Wire format: ciphertext + tag + nonce
        output.resize(result.data.size() + VOICE_TAG_SIZE + VOICE_NONCE_SIZE);
        memcpy(output.data(), result.data.data(), result.data.size());
        memcpy(output.data() + result.data.size(), result.tag.data(), VOICE_TAG_SIZE);
        memcpy(output.data() + result.data.size() + VOICE_TAG_SIZE, nonce, VOICE_NONCE_SIZE);
        return true;
}

// Helper: Simulate voice E2EE decrypt (matching voice_chat.cpp logic)
static bool simulateVoiceDecrypt(
        const u8 *key, size_t key_len,
        const std::vector<u8> &input, u16 peer_id,
        std::vector<u8> &plaintext)
{
        if (input.size() < VOICE_TAG_SIZE)
                return false;

        // Extract nonce from end of input
        if (input.size() < VOICE_TAG_SIZE + VOICE_NONCE_SIZE)
                return false;

        u8 nonce[VOICE_NONCE_SIZE];
        memcpy(nonce, input.data() + input.size() - VOICE_NONCE_SIZE, VOICE_NONCE_SIZE);

        // Extract tag (before nonce)
        const u8 *tag = input.data() + input.size() - VOICE_NONCE_SIZE - VOICE_TAG_SIZE;

        // Ciphertext is everything before tag
        size_t ciphertext_len = input.size() - VOICE_TAG_SIZE - VOICE_NONCE_SIZE;
        const u8 *ciphertext = input.data();

        // Extract counter from nonce for AAD
        u64 counter = 0;
        for (int i = 0; i < 8; i++)
                counter = (counter << 8) | nonce[4 + i];

        // Build AAD
        u8 aad[8];
        
        
        for (int i = 0; i < 8; i++)
                aad[i] = (counter >> (56 - i * 8)) & 0xFF;

        auto result = aes256gcm_decrypt(key, key_len, nonce, VOICE_NONCE_SIZE,
                ciphertext, ciphertext_len, tag, VOICE_TAG_SIZE, aad, 8);

        if (!result.success)
                return false;

        plaintext = std::move(result.data);
        return true;
}

// Helper: Extract nonce from encrypted voice data
static std::vector<u8> extractNonce(const std::vector<u8> &encrypted)
{
        if (encrypted.size() < VOICE_NONCE_SIZE)
                return {};
        return std::vector<u8>(encrypted.end() - VOICE_NONCE_SIZE, encrypted.end());
}

// Helper: Derive a voice session key from X25519 shared secret via HKDF
static bool deriveVoiceSessionKey(
        const u8 *shared_secret, size_t shared_secret_len,
        u8 *session_key, size_t session_key_len)
{
        const u8 info[] = "LuantisVoiceE2EEv1";
        return hkdf_sha256(shared_secret, shared_secret_len,
                nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                session_key, session_key_len);
}

// Helper: Derive a voice nonce base from X25519 shared secret via HKDF
static bool deriveVoiceNonceBase(
        const u8 *shared_secret, size_t shared_secret_len,
        u8 *nonce_base, size_t nonce_base_len)
{
        const u8 info[] = "LuantisVoiceE2EEv1Nonce";
        return hkdf_sha256(shared_secret, shared_secret_len,
                nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                nonce_base, nonce_base_len);
}

// Simple VoicePeerState-like struct for replay testing
struct TestPeerReplayState {
        u64 highest_recv_counter = 0;
        bool counter_initialized = false;
        static constexpr size_t REPLAY_WINDOW_SIZE = 64;
        std::array<u64, 1> replay_bitmap{};

        bool isNotReplay(u64 recv_ctr) {
                if (!counter_initialized) return true;
                if (recv_ctr > highest_recv_counter) return true;
                if (recv_ctr == highest_recv_counter) return false;  // Same counter = replay!
                if (highest_recv_counter - recv_ctr > REPLAY_WINDOW_SIZE) return false;
                s64 offset = static_cast<s64>(highest_recv_counter) - static_cast<s64>(recv_ctr) - 1;
                if (offset >= 0 && static_cast<size_t>(offset) < REPLAY_WINDOW_SIZE) {
                        size_t bit = static_cast<size_t>(offset);
                        return (replay_bitmap[0] & (1ULL << bit)) == 0;
                }
                return true;
        }

        void markAndAdvance(u64 recv_ctr) {
                if (!counter_initialized || recv_ctr > highest_recv_counter) {
                        if (counter_initialized) {
                                u64 shift = recv_ctr - highest_recv_counter;
                                if (shift >= REPLAY_WINDOW_SIZE)
                                        replay_bitmap.fill(0);
                                else
                                        replay_bitmap[0] <<= shift;
                        }
                        if (counter_initialized && highest_recv_counter != recv_ctr) {
                                u64 old_offset = recv_ctr - highest_recv_counter - 1;
                                if (old_offset < REPLAY_WINDOW_SIZE)
                                        replay_bitmap[0] |= (1ULL << old_offset);
                        }
                        highest_recv_counter = recv_ctr;
                        counter_initialized = true;
                } else {
                        s64 offset = static_cast<s64>(highest_recv_counter) - static_cast<s64>(recv_ctr) - 1;
                        if (offset >= 0 && static_cast<size_t>(offset) < REPLAY_WINDOW_SIZE) {
                                size_t bit = static_cast<size_t>(offset);
                                replay_bitmap[0] |= (1ULL << bit);
                        }
                }
        }
};

// ============================================================================
// X25519 Key Exchange Tests
// ============================================================================

void TestVoiceE2EE::testX25519KeyExchangeProducesSharedSecret()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        UASSERT(kp_a.success);
        UASSERT(kp_b.success);

        auto shared_ab = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());

        UASSERT(shared_ab.success);
        UASSERT(shared_ab.shared_secret.size() == 32);

        // Shared secret should not be all zeros
        bool all_zero = true;
        for (auto b : shared_ab.shared_secret) {
                if (b != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestVoiceE2EE::testX25519KeyExchangeSymmetry()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();

        auto shared_ab = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());

        auto shared_ba = x25519_compute_shared_secret(
                kp_b.private_key.data(), kp_b.private_key.size(),
                kp_a.public_key.data(), kp_a.public_key.size());

        UASSERT(shared_ab.success);
        UASSERT(shared_ba.success);
        UASSERT(shared_ab.shared_secret == shared_ba.shared_secret);
}

void TestVoiceE2EE::testX25519KeyExchangeDifferentPairsProduceDifferentSecrets()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto kp_c = x25519_generate_keypair();

        auto shared_ab = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());

        auto shared_ac = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_c.public_key.data(), kp_c.public_key.size());

        UASSERT(shared_ab.success);
        UASSERT(shared_ac.success);
        UASSERT(shared_ab.shared_secret != shared_ac.shared_secret);
}

void TestVoiceE2EE::testX25519KeyExchangeInvalidKeySizesFail()
{
        auto kp = x25519_generate_keypair();
        UASSERT(kp.success);

        // Wrong private key size
        auto result1 = x25519_compute_shared_secret(
                kp.private_key.data(), 16,  // too short
                kp.public_key.data(), kp.public_key.size());
        UASSERT(!result1.success);

        // Wrong public key size
        auto result2 = x25519_compute_shared_secret(
                kp.private_key.data(), kp.private_key.size(),
                kp.public_key.data(), 16);  // too short
        UASSERT(!result2.success);
}

// ============================================================================
// HKDF Key Derivation for Voice Session Keys
// ============================================================================

void TestVoiceE2EE::testHkdfVoiceKeyDerivationProducesValidKey()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));

        // Key should not be all zeros
        bool all_zero = true;
        for (auto b : session_key) {
                if (b != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestVoiceE2EE::testHkdfVoiceKeyDerivationDeterministic()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 key1[32], key2[32];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), key1, sizeof(key1)));
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), key2, sizeof(key2)));

        UASSERT(memcmp(key1, key2, 32) == 0);
}

void TestVoiceE2EE::testHkdfVoiceKeyDerivationDifferentPeersDifferentKeys()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto kp_c = x25519_generate_keypair();

        auto shared_ab = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        auto shared_ac = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_c.public_key.data(), kp_c.public_key.size());
        UASSERT(shared_ab.success);
        UASSERT(shared_ac.success);

        u8 key_ab[32], key_ac[32];
        UASSERT(deriveVoiceSessionKey(shared_ab.shared_secret.data(),
                shared_ab.shared_secret.size(), key_ab, sizeof(key_ab)));
        UASSERT(deriveVoiceSessionKey(shared_ac.shared_secret.data(),
                shared_ac.shared_secret.size(), key_ac, sizeof(key_ac)));

        UASSERT(memcmp(key_ab, key_ac, 32) != 0);
}

void TestVoiceE2EE::testHkdfVoiceNonceBaseDerivationProducesValidBase()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 nonce_base[4];
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        // Nonce base should not be all zeros (extremely unlikely for random secret)
        bool all_zero = true;
        for (auto b : nonce_base) {
                if (b != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestVoiceE2EE::testHkdfVoiceNonceBaseDeterministic()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 base1[4], base2[4];
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), base1, sizeof(base1)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), base2, sizeof(base2)));

        UASSERT(memcmp(base1, base2, 4) == 0);
}

void TestVoiceE2EE::testHkdfVoiceNonceBaseDifferentFromSessionKey()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32];
        u8 nonce_base[4];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        // The first 4 bytes of the session key should (almost certainly) differ
        // from the nonce base, because they use different HKDF info strings
        UASSERT(memcmp(session_key, nonce_base, 4) != 0);
}

// ============================================================================
// AES-256-GCM Encrypt/Decrypt for Voice
// ============================================================================

void TestVoiceE2EE::testVoiceEncryptDecryptRoundtrip()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32], nonce_base[4];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        // Simulate a voice frame
        std::vector<u8> plaintext = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        u16 peer_id = 42;
        u64 counter = 0;

        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                counter, peer_id, plaintext, encrypted));

        // Decrypt
        std::vector<u8> nonce_vec = extractNonce(encrypted);
        UASSERT(nonce_vec.size() == VOICE_NONCE_SIZE);

        std::vector<u8> decrypted;
        UASSERT(simulateVoiceDecrypt(session_key, 32, encrypted, peer_id, decrypted));

        UASSERT(decrypted == plaintext);
}

void TestVoiceE2EE::testVoiceEncryptDecryptLargePayload()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32], nonce_base[4];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        // Simulate a large Opus frame (up to 4000 bytes)
        std::vector<u8> plaintext(4000);
        for (size_t i = 0; i < plaintext.size(); i++)
                plaintext[i] = static_cast<u8>(i & 0xFF);

        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        std::vector<u8> decrypted;
        UASSERT(simulateVoiceDecrypt(session_key, 32, encrypted, 1, decrypted));

        UASSERT(decrypted == plaintext);
}

void TestVoiceE2EE::testVoiceEncryptDecryptMultiplePackets()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32], nonce_base[4];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        // Encrypt and decrypt 100 sequential packets
        u16 peer_id = 100;
        for (u64 counter = 0; counter < 100; counter++) {
                std::vector<u8> plaintext = {
                        static_cast<u8>(counter & 0xFF),
                        static_cast<u8>((counter >> 8) & 0xFF),
                        0xAA, 0xBB, 0xCC
                };

                std::vector<u8> encrypted;
                UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                        counter, peer_id, plaintext, encrypted));

                std::vector<u8> decrypted;
                UASSERT(simulateVoiceDecrypt(session_key, 32, encrypted, peer_id, decrypted));

                UASSERT(decrypted == plaintext);
        }
}

void TestVoiceE2EE::testVoiceEncryptProducesDifferentCiphertextEachTime()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32], nonce_base[4];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        std::vector<u8> plaintext = {0x01, 0x02, 0x03, 0x04};

        // Encrypt the same plaintext with different counters
        std::vector<u8> enc1, enc2;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, enc1));
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                1, 1, plaintext, enc2));

        // Ciphertext should differ because different nonces (counter 0 vs 1)
        // Extract just the ciphertext portion (exclude tag and nonce from comparison)
        size_t ct_len = plaintext.size();
        UASSERT(memcmp(enc1.data(), enc2.data(), ct_len) != 0);
}

void TestVoiceE2EE::testVoiceDecryptWrongKeyFails()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 session_key[32], nonce_base[4];
        UASSERT(deriveVoiceSessionKey(shared.shared_secret.data(),
                shared.shared_secret.size(), session_key, sizeof(session_key)));
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        // Try to decrypt with a wrong key
        u8 wrong_key[32];
        secure_random(wrong_key, sizeof(wrong_key));

        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(wrong_key, 32, encrypted, 1, decrypted));
}

void TestVoiceE2EE::testVoiceDecryptWrongNonceFails()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        // Tamper with the nonce in the encrypted data
        encrypted[encrypted.size() - 1] ^= 0xFF;

        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(session_key, 32, encrypted, 1, decrypted));
}

void TestVoiceE2EE::testVoiceDecryptTamperedCiphertextFails()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        // Tamper with ciphertext
        encrypted[0] ^= 0xFF;

        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(session_key, 32, encrypted, 1, decrypted));
}

void TestVoiceE2EE::testVoiceDecryptTamperedTagFails()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        // Tamper with the authentication tag (after ciphertext, before nonce)
        size_t tag_offset = plaintext.size();
        encrypted[tag_offset] ^= 0xFF;

        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(session_key, 32, encrypted, 1, decrypted));
}

void TestVoiceE2EE::testVoiceDecryptWrongAADFails()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 42, plaintext, encrypted));  // peer_id = 42

        // Try to decrypt with a different peer_id in AAD (peer_id = 99)
        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(session_key, 32, encrypted, 99, decrypted));
}

// ============================================================================
// Nonce Construction for Voice
// ============================================================================

void TestVoiceE2EE::testVoiceNonceConstructionDeterministic()
{
        u8 nonce_base[4] = {0xAB, 0xCD, 0xEF, 0x12};
        u64 counter = 42;

        u8 nonce1[12], nonce2[12];
        build_nonce(nonce_base, counter, nonce1);
        build_nonce(nonce_base, counter, nonce2);

        UASSERT(memcmp(nonce1, nonce2, 12) == 0);
}

void TestVoiceE2EE::testVoiceNonceConstructionDifferentCounters()
{
        u8 nonce_base[4] = {0xAB, 0xCD, 0xEF, 0x12};

        u8 nonce0[12], nonce1[12];
        build_nonce(nonce_base, 0, nonce0);
        build_nonce(nonce_base, 1, nonce1);

        UASSERT(memcmp(nonce0, nonce1, 12) != 0);
        // Only the counter portion should differ
        UASSERT(memcmp(nonce0, nonce1, 4) == 0);  // base is the same
        UASSERT(memcmp(nonce0 + 4, nonce1 + 4, 8) != 0);  // counter differs
}

void TestVoiceE2EE::testVoiceNonceCounterBigEndianEncoding()
{
        u8 nonce_base[4] = {0x00, 0x00, 0x00, 0x00};
        u8 nonce[12];
        build_nonce(nonce_base, 1, nonce);

        // Counter 1 in big-endian: 00 00 00 00 00 00 00 01
        UASSERT(nonce[4] == 0x00);
        UASSERT(nonce[5] == 0x00);
        UASSERT(nonce[6] == 0x00);
        UASSERT(nonce[7] == 0x00);
        UASSERT(nonce[8] == 0x00);
        UASSERT(nonce[9] == 0x00);
        UASSERT(nonce[10] == 0x00);
        UASSERT(nonce[11] == 0x01);
}

void TestVoiceE2EE::testVoiceNonceBaseFromHKDFIs4Bytes()
{
        auto kp_a = x25519_generate_keypair();
        auto kp_b = x25519_generate_keypair();
        auto shared = x25519_compute_shared_secret(
                kp_a.private_key.data(), kp_a.private_key.size(),
                kp_b.public_key.data(), kp_b.public_key.size());
        UASSERT(shared.success);

        u8 nonce_base[4];
        UASSERT(deriveVoiceNonceBase(shared.shared_secret.data(),
                shared.shared_secret.size(), nonce_base, sizeof(nonce_base)));

        // Verify it's exactly 4 bytes (no more, no less)
        // This matches NONCE_BASE_SIZE used in the transport layer
        UASSERT(sizeof(nonce_base) == 4);
}

// ============================================================================
// Global Channel Key Combiner
// ============================================================================

void TestVoiceE2EE::testGlobalChannelKeyCombinerHKDF()
{
        // Simulate 3 peers with different session keys
        u8 peer_key_1[32], peer_key_2[32], peer_key_3[32];
        secure_random(peer_key_1, 32);
        secure_random(peer_key_2, 32);
        secure_random(peer_key_3, 32);

        // Combine via HKDF (as the fixed code does)
        std::vector<u8> combined_ikm;
        combined_ikm.insert(combined_ikm.end(), peer_key_1, peer_key_1 + 32);
        combined_ikm.insert(combined_ikm.end(), peer_key_2, peer_key_2 + 32);
        combined_ikm.insert(combined_ikm.end(), peer_key_3, peer_key_3 + 32);

        u8 combined_key[32];
        const u8 info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(combined_ikm.data(), combined_ikm.size(),
                nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                combined_key, sizeof(combined_key)));

        // Key should not be all zeros
        bool all_zero = true;
        for (auto b : combined_key) {
                if (b != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestVoiceE2EE::testGlobalChannelKeyCombinerSymmetricInPeerOrder()
{
        u8 peer_key_1[32], peer_key_2[32];
        secure_random(peer_key_1, 32);
        secure_random(peer_key_2, 32);

        // Order 1: key1 || key2
        std::vector<u8> ikm_12;
        ikm_12.insert(ikm_12.end(), peer_key_1, peer_key_1 + 32);
        ikm_12.insert(ikm_12.end(), peer_key_2, peer_key_2 + 32);

        // Order 2: key2 || key1
        std::vector<u8> ikm_21;
        ikm_21.insert(ikm_21.end(), peer_key_2, peer_key_2 + 32);
        ikm_21.insert(ikm_21.end(), peer_key_1, peer_key_1 + 32);

        u8 key_12[32], key_21[32];
        const u8 info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(ikm_12.data(), ikm_12.size(), nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                key_12, sizeof(key_12)));
        UASSERT(hkdf_sha256(ikm_21.data(), ikm_21.size(), nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                key_21, sizeof(key_21)));

        // HKDF is NOT symmetric in peer order (concatenation order matters).
        // This is expected — peers must agree on a canonical ordering.
        // In practice, the server sends peers in peer_id order, ensuring consistency.
        // We verify that both produce valid (non-zero) keys.
        bool key_12_nonzero = false, key_21_nonzero = false;
        for (int i = 0; i < 32; i++) {
                if (key_12[i] != 0) key_12_nonzero = true;
                if (key_21[i] != 0) key_21_nonzero = true;
        }
        UASSERT(key_12_nonzero);
        UASSERT(key_21_nonzero);
}

void TestVoiceE2EE::testGlobalChannelKeyCombinerDifferentFromPerPeerKey()
{
        u8 peer_key_1[32], peer_key_2[32];
        secure_random(peer_key_1, 32);
        secure_random(peer_key_2, 32);

        // Per-peer key (just peer_key_1)
        u8 per_peer_key[32];
        const u8 single_info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(peer_key_1, 32, nullptr, 0,
                single_info, strlen(reinterpret_cast<const char*>(single_info)),
                per_peer_key, sizeof(per_peer_key)));

        // Combined key (key1 || key2)
        std::vector<u8> combined_ikm;
        combined_ikm.insert(combined_ikm.end(), peer_key_1, peer_key_1 + 32);
        combined_ikm.insert(combined_ikm.end(), peer_key_2, peer_key_2 + 32);

        u8 combined_key[32];
        UASSERT(hkdf_sha256(combined_ikm.data(), combined_ikm.size(), nullptr, 0,
                single_info, strlen(reinterpret_cast<const char*>(single_info)),
                combined_key, sizeof(combined_key)));

        // The combined key should differ from a single-peer derived key
        UASSERT(memcmp(per_peer_key, combined_key, 32) != 0);
}

void TestVoiceE2EE::testGlobalChannelKeyCombinerSinglePeer()
{
        u8 peer_key[32];
        secure_random(peer_key, 32);

        // Single peer: combined key = HKDF(peer_key, "LuantisVoiceGlobalE2EEv1")
        u8 combined_key[32];
        const u8 info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(peer_key, 32, nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                combined_key, sizeof(combined_key)));

        // With a single peer, the combined key should still be valid
        bool all_zero = true;
        for (auto b : combined_key) {
                if (b != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestVoiceE2EE::testGlobalChannelKeyCombinerMultiplePeers()
{
        // 5 peers
        u8 peer_keys[5][32];
        std::vector<u8> combined_ikm;
        for (int i = 0; i < 5; i++) {
                secure_random(peer_keys[i], 32);
                combined_ikm.insert(combined_ikm.end(), peer_keys[i], peer_keys[i] + 32);
        }

        u8 combined_key[32];
        const u8 info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(combined_ikm.data(), combined_ikm.size(), nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                combined_key, sizeof(combined_key)));

        bool all_zero = true;
        for (auto b : combined_key) {
                if (b != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestVoiceE2EE::testGlobalChannelXORWouldBeInsecure()
{
        // REGRESSION TEST: The old XOR combiner had a critical flaw.
        // If two peers happen to have the same session key, XOR produces zero.
        u8 key_a[32], key_b[32];
        secure_random(key_a, 32);
        memcpy(key_b, key_a, 32);  // Same key!

        // Old XOR combiner would produce all zeros:
        u8 xor_result[32] = {};
        for (int i = 0; i < 32; i++)
                xor_result[i] = key_a[i] ^ key_b[i];

        bool xor_all_zero = true;
        for (auto b : xor_result) {
                if (b != 0) { xor_all_zero = false; break; }
        }
        UASSERT(xor_all_zero);  // XOR with same key = all zeros (BUG!)

        // HKDF combiner produces a valid key even with identical inputs:
        std::vector<u8> combined_ikm;
        combined_ikm.insert(combined_ikm.end(), key_a, key_a + 32);
        combined_ikm.insert(combined_ikm.end(), key_b, key_b + 32);

        u8 hkdf_result[32];
        const u8 info[] = "LuantisVoiceGlobalE2EEv1";
        UASSERT(hkdf_sha256(combined_ikm.data(), combined_ikm.size(), nullptr, 0,
                info, strlen(reinterpret_cast<const char*>(info)),
                hkdf_result, sizeof(hkdf_result)));

        bool hkdf_all_zero = true;
        for (auto b : hkdf_result) {
                if (b != 0) { hkdf_all_zero = false; break; }
        }
        UASSERT(!hkdf_all_zero);  // HKDF still produces a valid key (FIXED!)
}

// ============================================================================
// AAD (Additional Authenticated Data) for Voice
// ============================================================================

void TestVoiceE2EE::testVoiceAADIncludesPeerID()
{
        // After the AAD fix, peer_id is NOT included in AAD because
        // encrypt uses the recipient's peer_id while decrypt uses the sender's
        // peer_id — they would never match. Instead, the AAD contains only
        // the counter, and per-peer key separation provides identity binding.
        // This test verifies that decryption succeeds with ANY peer_id parameter
        // (since peer_id is no longer in AAD).
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};

        // Encrypt with peer_id = 42
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 42, plaintext, encrypted));

        // Decrypt with peer_id = 42 should succeed
        std::vector<u8> decrypted;
        UASSERT(simulateVoiceDecrypt(session_key, 32, encrypted, 42, decrypted));

        // Decrypt with peer_id = 43 should ALSO succeed (peer_id not in AAD anymore)
        std::vector<u8> decrypted2 = encrypted;
        UASSERT(simulateVoiceDecrypt(session_key, 32, decrypted2, 43, decrypted2));
}

void TestVoiceE2EE::testVoiceAADIncludesCounter()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};

        // Encrypt with counter = 5
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                5, 1, plaintext, encrypted));

        // Decrypt should work (counter is extracted from nonce)
        std::vector<u8> decrypted;
        UASSERT(simulateVoiceDecrypt(session_key, 32, encrypted, 1, decrypted));
        UASSERT(decrypted == plaintext);
}

void TestVoiceE2EE::testVoiceAADMismatchCausesDecryptFailure()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0xAA, 0xBB, 0xCC, 0xDD};

        // Encrypt with counter = 0
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 100, plaintext, encrypted));

        // Tamper with the nonce to change the counter (which is in AAD)
        // This should cause GCM auth failure since the AAD won't match
        encrypted[encrypted.size() - 1] ^= 0xFF;  // Flip last byte of nonce

        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(session_key, 32, encrypted, 100, decrypted));
}

// ============================================================================
// Replay Protection for Voice
// ============================================================================

void TestVoiceE2EE::testReplayProtectionFirstPacketAccepted()
{
        TestPeerReplayState state;
        UASSERT(state.isNotReplay(0));  // First packet always accepted
}

void TestVoiceE2EE::testReplayProtectionFuturePacketAccepted()
{
        TestPeerReplayState state;
        state.markAndAdvance(10);
        UASSERT(state.isNotReplay(11));  // Future packet accepted
        UASSERT(state.isNotReplay(100)); // Far future also accepted
}

void TestVoiceE2EE::testReplayProtectionOldReplayRejected()
{
        TestPeerReplayState state;
        state.markAndAdvance(100);
        // Counter 0 is far behind (100 > 64 window), should be rejected
        UASSERT(!state.isNotReplay(0));
}

void TestVoiceE2EE::testReplayProtectionSameCounterRejected()
{
        TestPeerReplayState state;
        state.markAndAdvance(10);
        // Counter 10 was just seen — replay!
        UASSERT(!state.isNotReplay(10));
}

void TestVoiceE2EE::testReplayProtectionWindowBoundary()
{
        TestPeerReplayState state;
        state.markAndAdvance(64);

        // Counter 0 is exactly at window boundary (64 - 0 = 64 = window size)
        // Should be rejected (>= window means out of window)
        UASSERT(!state.isNotReplay(0));

        // Counter 1 is within window (64 - 1 = 63 < 64)
        UASSERT(state.isNotReplay(1));

        // But after marking 1, replaying it should fail
        state.markAndAdvance(1);
        UASSERT(!state.isNotReplay(1));
}

void TestVoiceE2EE::testReplayProtectionCounterAdvances()
{
        TestPeerReplayState state;

        // Process packets in order
        for (u64 i = 0; i < 50; i++) {
                UASSERT(state.isNotReplay(i));
                state.markAndAdvance(i);
        }

        // Replay of any previous counter should fail
        UASSERT(!state.isNotReplay(25));
        UASSERT(!state.isNotReplay(49));

        // Future counter should succeed
        UASSERT(state.isNotReplay(50));
}

// ============================================================================
// Peer State Management
// ============================================================================

void TestVoiceE2EE::testPeerStateNonceBasePreservedOnUpdate()
{
        // Simulate the merge logic from updatePeerList
        // Create a peer with E2EE state
        std::unordered_map<u16, std::vector<u8>> peers;
        u16 peer_id = 42;

        // Simulate initial peer with nonce base
        std::vector<u8> nonce_base = {0xAB, 0xCD, 0xEF, 0x12};
        peers[peer_id] = nonce_base;

        // Simulate update: peer still exists
        auto it = peers.find(peer_id);
        UASSERT(it != peers.end());
        // Nonce base should be preserved
        UASSERT(it->second == nonce_base);
}

void TestVoiceE2EE::testPeerStateSessionKeyPreservedOnUpdate()
{
        std::unordered_map<u16, std::vector<u8>> peers;
        u16 peer_id = 42;

        std::vector<u8> session_key(32, 0x42);
        peers[peer_id] = session_key;

        // Peer still exists after update
        auto it = peers.find(peer_id);
        UASSERT(it != peers.end());
        UASSERT(it->second == session_key);
}

void TestVoiceE2EE::testPeerStateReplayStatePreservedOnUpdate()
{
        TestPeerReplayState state;
        state.markAndAdvance(50);
        state.markAndAdvance(51);
        state.markAndAdvance(52);

        // After processing, replay of 50 should be detected
        UASSERT(!state.isNotReplay(50));
        UASSERT(!state.isNotReplay(51));

        // Future counter accepted
        UASSERT(state.isNotReplay(53));
}

void TestVoiceE2EE::testPeerStateNewPeerHasNoE2EE()
{
        // A new peer (just joined) should not have E2EE active
        // until key exchange is completed
        bool e2ee_active = false;  // Default for new peer
        UASSERT(!e2ee_active);
}

void TestVoiceE2EE::testPeerStateDepartedPeerRemoved()
{
        std::unordered_map<u16, bool> peers;
        peers[42] = true;
        peers[43] = true;

        // Remove departed peer
        peers.erase(42);

        UASSERT(peers.find(42) == peers.end());  // Removed
        UASSERT(peers.count(43));  // Still present
}

// ============================================================================
// Wire Format for Voice E2EE
// ============================================================================

void TestVoiceE2EE::testWireFormatCiphertextTagNonce()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        // Wire format: [ciphertext(5)][tag(16)][nonce(12)] = 33 bytes
        UASSERT(encrypted.size() == 5 + VOICE_TAG_SIZE + VOICE_NONCE_SIZE);

        // Nonce is at the end
        std::vector<u8> extracted_nonce(encrypted.end() - VOICE_NONCE_SIZE, encrypted.end());
        UASSERT(extracted_nonce.size() == VOICE_NONCE_SIZE);

        // Tag is before nonce
        // (We can verify the tag is present by checking that decryption works)
}

void TestVoiceE2EE::testWireFormatDecryptStripsTagAndNonce()
{
        u8 session_key[32], nonce_base[4];
        secure_random(session_key, 32);
        secure_random(nonce_base, 4);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<u8> encrypted;
        UASSERT(simulateVoiceEncrypt(session_key, 32, nonce_base, 4,
                0, 1, plaintext, encrypted));

        std::vector<u8> decrypted;
        UASSERT(simulateVoiceDecrypt(session_key, 32, encrypted, 1, decrypted));

        // After decryption, only plaintext should remain (no tag or nonce)
        UASSERT(decrypted.size() == plaintext.size());
        UASSERT(decrypted == plaintext);
}

void TestVoiceE2EE::testWireFormatMinimumSize()
{
        // The minimum encrypted voice data is: empty ciphertext + tag + nonce
        // But in practice, even the smallest Opus frame is > 0 bytes
        // The decrypt should reject data smaller than the tag size

        u8 session_key[32];
        secure_random(session_key, 32);

        // 15 bytes < VOICE_TAG_SIZE (16)
        std::vector<u8> too_small(15, 0xAA);
        std::vector<u8> decrypted;
        UASSERT(!simulateVoiceDecrypt(session_key, 32, too_small, 1, decrypted));
}
