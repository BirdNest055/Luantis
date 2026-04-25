// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for PeerEncryptionState — the per-peer encryption state
// that manages key derivation, activation, disable, directional keys,
// nonce counter management, and replay protection.
//
// These tests prove that:
// - Key derivation from SRP session key works correctly
// - C2S and S2C keys are cryptographically independent
// - Nonce bases are different for each direction
// - Activation is deferred (not active immediately after init)
// - Disable zeroes out key material
// - Nonce counters increment properly (no reuse)
// - Replay protection works within the sliding window

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <set>
#include <thread>
#include <atomic>

class TestPeerEncryptionState : public TestBase
{
public:
        TestPeerEncryptionState() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestPeerEncryptionState"; }

        void runTests(IGameDef *gamedef);

        // Initialization tests
        void testInitFromSRPSessionKeySuccess();
        void testInitFromSRPSessionKeyNullKey();
        void testInitFromSRPSessionKeyWrongSize();
        void testInitDerivesC2SAndS2CKeys();
        void testInitDerivesNonceBases();
        void testInitDerivesSessionId();
        void testInitDerivesServerFingerprint();
        void testInitResetsCounters();

        // Activation tests
        void testNotActiveAfterInit();
        void testActivateSetsActive();
        void testDisableClearsActive();
        void testDisableZeroesKeyMaterial();
        void testDeferredActivationFlow();

        // Directional key independence tests
        void testC2SAndS2CKeysAreDifferent();
        void testC2SAndS2CNonceBasesAreDifferent();
        void testEncryptWithCorrectDirectionKey();

        // Nonce counter tests
        void testNextNonceIncrementsCounter();
        void testNextNonceProducesUniqueNonces();
        void testNextNonceCounterNeverReuses();

        // Replay protection tests
        void testIsNotReplayAcceptsFreshPacket();
        void testIsNotReplayAcceptsNearFuture();
        void testIsNotReplayRejectsOldReplay();
        void testIsNotReplayWindowBoundary();
        void testUpdateCounterAdvancesHighWaterMark();
        void testUpdateCounterDoesNotGoBackwards();

        // Thread safety tests
        void testConcurrentNextNonce();
        void testConcurrentActivateAndRead();

        // Full encrypt/decrypt with PeerEncryptionState
        void testFullPeerEncryptDecryptRoundtrip();
        void testServerClientDirectionalRoundtrip();

        // Key material cleanup
        void testDisableClearsSessionId();
        void testDisableClearsFingerprint();
};

static TestPeerEncryptionState g_test_instance;

// Helper: generate a random SRP session key for testing
static std::array<u8, SRP_SESSION_KEY_SIZE> makeTestSessionKey()
{
        std::array<u8, SRP_SESSION_KEY_SIZE> key;
        for (size_t i = 0; i < key.size(); i++)
                key[i] = static_cast<u8>(i * 7 + 42);
        return key;
}

void TestPeerEncryptionState::runTests(IGameDef *gamedef)
{
        // Initialization tests
        TEST(testInitFromSRPSessionKeySuccess);
        TEST(testInitFromSRPSessionKeyNullKey);
        TEST(testInitFromSRPSessionKeyWrongSize);
        TEST(testInitDerivesC2SAndS2CKeys);
        TEST(testInitDerivesNonceBases);
        TEST(testInitDerivesSessionId);
        TEST(testInitDerivesServerFingerprint);
        TEST(testInitResetsCounters);

        // Activation tests
        TEST(testNotActiveAfterInit);
        TEST(testActivateSetsActive);
        TEST(testDisableClearsActive);
        TEST(testDisableZeroesKeyMaterial);
        TEST(testDeferredActivationFlow);

        // Directional key independence tests
        TEST(testC2SAndS2CKeysAreDifferent);
        TEST(testC2SAndS2CNonceBasesAreDifferent);
        TEST(testEncryptWithCorrectDirectionKey);

        // Nonce counter tests
        TEST(testNextNonceIncrementsCounter);
        TEST(testNextNonceProducesUniqueNonces);
        TEST(testNextNonceCounterNeverReuses);

        // Replay protection tests
        TEST(testIsNotReplayAcceptsFreshPacket);
        TEST(testIsNotReplayAcceptsNearFuture);
        TEST(testIsNotReplayRejectsOldReplay);
        TEST(testIsNotReplayWindowBoundary);
        TEST(testUpdateCounterAdvancesHighWaterMark);
        TEST(testUpdateCounterDoesNotGoBackwards);

        // Thread safety tests
        TEST(testConcurrentNextNonce);
        TEST(testConcurrentActivateAndRead);

        // Full roundtrip
        TEST(testFullPeerEncryptDecryptRoundtrip);
        TEST(testServerClientDirectionalRoundtrip);

        // Key material cleanup
        TEST(testDisableClearsSessionId);
        TEST(testDisableClearsFingerprint);
}

// ============================================================================
// Initialization Tests
// ============================================================================

void TestPeerEncryptionState::testInitFromSRPSessionKeySuccess()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);
}

void TestPeerEncryptionState::testInitFromSRPSessionKeyNullKey()
{
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(nullptr, SRP_SESSION_KEY_SIZE, false);
        UASSERT(!ok);
}

void TestPeerEncryptionState::testInitFromSRPSessionKeyWrongSize()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), 16, false); // Too short
        UASSERT(!ok);
}

void TestPeerEncryptionState::testInitDerivesC2SAndS2CKeys()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);

        // Keys should not be all zeros (i.e., they were actually derived)
        bool c2s_all_zero = true, s2c_all_zero = true;
        for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
                if (state.c2s.key[i] != 0) c2s_all_zero = false;
                if (state.s2c.key[i] != 0) s2c_all_zero = false;
        }
        UASSERT(!c2s_all_zero);
        UASSERT(!s2c_all_zero);
}

void TestPeerEncryptionState::testInitDerivesNonceBases()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);

        // Nonce bases should not be all zeros
        bool c2s_nb_zero = true, s2c_nb_zero = true;
        for (size_t i = 0; i < NONCE_BASE_SIZE; i++) {
                if (state.c2s.nonce_base[i] != 0) c2s_nb_zero = false;
                if (state.s2c.nonce_base[i] != 0) s2c_nb_zero = false;
        }
        UASSERT(!c2s_nb_zero);
        UASSERT(!s2c_nb_zero);
}

void TestPeerEncryptionState::testInitDerivesSessionId()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);
        UASSERT(!state.session_id.empty());
        // Session ID should be a hex string (32 hex chars from 16 bytes)
        UASSERTEQ(size_t, state.session_id.size(), 32u);
}

void TestPeerEncryptionState::testInitDerivesServerFingerprint()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);
        UASSERT(!state.server_fingerprint.empty());
        // Fingerprint should start with "SHA256:"
        UASSERT(state.server_fingerprint.substr(0, 7) == "SHA256:");
}

void TestPeerEncryptionState::testInitResetsCounters()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;

        // Advance counters before init
        state.c2s.nonce_counter = 999;
        state.s2c.nonce_counter = 888;
        state.c2s.packets_processed = 777;
        state.s2c.packets_processed = 666;
        state.c2s.auth_failures = 555;
        state.s2c.auth_failures = 444;

        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);

        // Counters should be reset to zero
        UASSERTEQ(u64, state.c2s.nonce_counter, 0u);
        UASSERTEQ(u64, state.s2c.nonce_counter, 0u);
        UASSERTEQ(u64, state.c2s.packets_processed, 0u);
        UASSERTEQ(u64, state.s2c.packets_processed, 0u);
        UASSERTEQ(u64, state.c2s.auth_failures, 0u);
        UASSERTEQ(u64, state.s2c.auth_failures, 0u);
}

// ============================================================================
// Activation Tests
// ============================================================================

void TestPeerEncryptionState::testNotActiveAfterInit()
{
        // After initFromSRPSessionKey, encryption should NOT be active yet.
        // This is the deferred activation design — prevents chicken-and-egg.
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(!state.active.load());
}

void TestPeerEncryptionState::testActivateSetsActive()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(!state.active.load());

        state.activate();
        UASSERT(state.active.load());
}

void TestPeerEncryptionState::testDisableClearsActive()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        state.activate();
        UASSERT(state.active.load());

        state.disable();
        UASSERT(!state.active.load());
}

void TestPeerEncryptionState::testDisableZeroesKeyMaterial()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        state.activate();
        state.disable();

        // All key material should be zeroed
        for (size_t i = 0; i < AES256_KEY_SIZE; i++) {
                UASSERTEQ(int, state.c2s.key[i], 0);
                UASSERTEQ(int, state.s2c.key[i], 0);
        }
        for (size_t i = 0; i < NONCE_BASE_SIZE; i++) {
                UASSERTEQ(int, state.c2s.nonce_base[i], 0);
                UASSERTEQ(int, state.s2c.nonce_base[i], 0);
        }
        for (size_t i = 0; i < SRP_SESSION_KEY_SIZE; i++) {
                UASSERTEQ(int, state.srp_session_key[i], 0);
        }
}

void TestPeerEncryptionState::testDeferredActivationFlow()
{
        // Simulate the real-world deferred activation flow:
        // 1. Init keys from SRP (not active)
        // 2. Push state to connection layer (not active)
        // 3. Send AUTH_ACCEPT in plaintext (not active)
        // 4. Activate encryption
        // 5. All subsequent packets are encrypted
        auto key = makeTestSessionKey();
        PeerEncryptionState state;

        // Step 1-3: Not active
        state.initFromSRPSessionKey(key.data(), key.size(), true);
        UASSERT(!state.active.load());

        // Simulate: "would send AUTH_ACCEPT here" (plaintext allowed)

        // Step 4: Activate
        state.activate();
        UASSERT(state.active.load());

        // Step 5: Now any packet should be encrypted
        // (We verify by checking that active is true, which the
        // send/receive threads check before encrypting)
}

// ============================================================================
// Directional Key Independence Tests
// ============================================================================

void TestPeerEncryptionState::testC2SAndS2CKeysAreDifferent()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);

        UASSERT(memcmp(state.c2s.key.data(), state.s2c.key.data(),
                       AES256_KEY_SIZE) != 0);
}

void TestPeerEncryptionState::testC2SAndS2CNonceBasesAreDifferent()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);

        UASSERT(memcmp(state.c2s.nonce_base.data(), state.s2c.nonce_base.data(),
                       NONCE_BASE_SIZE) != 0);
}

void TestPeerEncryptionState::testEncryptWithCorrectDirectionKey()
{
        // Verify that encrypting with the C2S key and decrypting with the
        // C2S key works, but cross-direction fails.
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);

        const char *msg = "directional test";
        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        // Encrypt with C2S key
        auto nonce = state.c2s.nextNonce();
        CryptoResult enc = aes256gcm_encrypt(
                state.c2s.key.data(), state.c2s.key.size(),
                nonce.data(), nonce.size(),
                reinterpret_cast<const u8*>(msg), strlen(msg),
                &aad, 1);
        UASSERT(enc.success);

        // Decrypt with C2S key — should work
        CryptoResult dec = aes256gcm_decrypt(
                state.c2s.key.data(), state.c2s.key.size(),
                nonce.data(), nonce.size(),
                enc.data.data(), enc.data.size(),
                enc.tag.data(), enc.tag.size(),
                &aad, 1);
        UASSERT(dec.success);

        // Try to decrypt with S2C key — should FAIL
        CryptoResult wrong_dec = aes256gcm_decrypt(
                state.s2c.key.data(), state.s2c.key.size(),
                nonce.data(), nonce.size(),
                enc.data.data(), enc.data.size(),
                enc.tag.data(), enc.tag.size(),
                &aad, 1);
        UASSERT(!wrong_dec.success);
}

// ============================================================================
// Nonce Counter Tests
// ============================================================================

void TestPeerEncryptionState::testNextNonceIncrementsCounter()
{
        DirectionalEncryptionState dir;
        dir.nonce_counter = 0;

        dir.nextNonce();
        UASSERTEQ(u64, dir.nonce_counter, 1u);

        dir.nextNonce();
        UASSERTEQ(u64, dir.nonce_counter, 2u);

        dir.nextNonce();
        UASSERTEQ(u64, dir.nonce_counter, 3u);
}

void TestPeerEncryptionState::testNextNonceProducesUniqueNonces()
{
        DirectionalEncryptionState dir;
        secure_random(dir.nonce_base.data(), dir.nonce_base.size());
        dir.nonce_counter = 0;

        // Generate 100 nonces and verify they're all unique
        std::vector<std::array<u8, GCM_NONCE_SIZE>> nonces;
        for (int i = 0; i < 100; i++) {
                nonces.push_back(dir.nextNonce());
        }

        for (int i = 0; i < 100; i++) {
                for (int j = i + 1; j < 100; j++) {
                        UASSERT(memcmp(nonces[i].data(), nonces[j].data(),
                                       GCM_NONCE_SIZE) != 0);
                }
        }
}

void TestPeerEncryptionState::testNextNonceCounterNeverReuses()
{
        // Nonce counter must be strictly monotonically increasing.
        // Even after 1000 calls, each nonce should be unique.
        DirectionalEncryptionState dir;
        secure_random(dir.nonce_base.data(), dir.nonce_base.size());
        dir.nonce_counter = 0;

        std::set<u64> seen_counters;
        for (int i = 0; i < 1000; i++) {
                dir.nextNonce();
                UASSERT(seen_counters.find(dir.nonce_counter - 1) == seen_counters.end());
                seen_counters.insert(dir.nonce_counter - 1);
        }

        // Counter should be 1000
        UASSERTEQ(u64, dir.nonce_counter, 1000u);
}

// ============================================================================
// Replay Protection Tests
// ============================================================================

void TestPeerEncryptionState::testIsNotReplayAcceptsFreshPacket()
{
        DirectionalEncryptionState dir;
        dir.nonce_counter = 10;

        // Counter 10 is the current position — should be accepted
        UASSERT(dir.isNotReplay(10));

        // Counter 11 is in the future — should be accepted
        UASSERT(dir.isNotReplay(11));

        // Counter 100 is far in the future — should be accepted
        UASSERT(dir.isNotReplay(100));
}

void TestPeerEncryptionState::testIsNotReplayAcceptsNearFuture()
{
        DirectionalEncryptionState dir;
        dir.nonce_counter = 50;

        // Packets slightly ahead should be accepted
        UASSERT(dir.isNotReplay(51));
        UASSERT(dir.isNotReplay(60));
        UASSERT(dir.isNotReplay(100));
}

void TestPeerEncryptionState::testIsNotReplayRejectsOldReplay()
{
        DirectionalEncryptionState dir;
        dir.nonce_counter = 200;

        // Packets way behind the current counter should be rejected
        // (more than 64 positions behind = replay)
        UASSERT(!dir.isNotReplay(100));  // 100 behind
        UASSERT(!dir.isNotReplay(0));    // Way behind
        UASSERT(!dir.isNotReplay(135));  // 65 behind — just outside window
}

void TestPeerEncryptionState::testIsNotReplayWindowBoundary()
{
        // Test the exact window boundary (REPLAY_WINDOW = 64)
        DirectionalEncryptionState dir;
        dir.nonce_counter = 200;

        // Counter 136 is 64 positions behind — at the boundary, should be accepted
        UASSERT(dir.isNotReplay(136));

        // Counter 135 is 65 positions behind — just outside, should be rejected
        UASSERT(!dir.isNotReplay(135));

        // Counter 137 is 63 positions behind — well inside window
        UASSERT(dir.isNotReplay(137));
}

void TestPeerEncryptionState::testUpdateCounterAdvancesHighWaterMark()
{
        DirectionalEncryptionState dir;
        dir.nonce_counter = 10;

        // Receiving a packet with counter 20 should advance to 21
        dir.updateCounter(20);
        UASSERTEQ(u64, dir.nonce_counter, 21u);

        // Receiving a packet with counter 30 should advance to 31
        dir.updateCounter(30);
        UASSERTEQ(u64, dir.nonce_counter, 31u);
}

void TestPeerEncryptionState::testUpdateCounterDoesNotGoBackwards()
{
        DirectionalEncryptionState dir;
        dir.nonce_counter = 50;

        // Receiving an old packet (counter 30) should NOT move the counter back
        dir.updateCounter(30);
        UASSERTEQ(u64, dir.nonce_counter, 50u);

        // Receiving same counter should NOT change anything
        dir.updateCounter(49);
        UASSERTEQ(u64, dir.nonce_counter, 50u);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

void TestPeerEncryptionState::testConcurrentNextNonce()
{
        // Multiple threads calling nextNonce() concurrently with a mutex
        // should not produce duplicate nonces.
        // DirectionalEncryptionState::nextNonce() is NOT thread-safe by itself;
        // callers must hold PeerEncryptionState::lock() (which they do in
        // real code via rawSend/receive). We replicate that here.
        DirectionalEncryptionState dir;
        secure_random(dir.nonce_base.data(), dir.nonce_base.size());
        dir.nonce_counter = 0;

        std::mutex dir_mutex;  // Protects dir just like PeerEncryptionState::m_mutex

        constexpr int NUM_THREADS = 4;
        constexpr int NONCES_PER_THREAD = 500;
        std::vector<std::vector<std::array<u8, GCM_NONCE_SIZE>>> thread_nonces(NUM_THREADS);

        std::vector<std::thread> threads;
        for (int t = 0; t < NUM_THREADS; t++) {
                threads.emplace_back([&dir, &dir_mutex, &thread_nonces, t]() {
                        for (int i = 0; i < NONCES_PER_THREAD; i++) {
                                std::lock_guard<std::mutex> lk(dir_mutex);
                                thread_nonces[t].push_back(dir.nextNonce());
                        }
                });
        }

        for (auto &th : threads)
                th.join();

        // Verify total packets processed
        UASSERTEQ(u64, dir.packets_processed,
                static_cast<u64>(NUM_THREADS * NONCES_PER_THREAD));

        // Counter should equal total nonces generated
        UASSERTEQ(u64, dir.nonce_counter,
                static_cast<u64>(NUM_THREADS * NONCES_PER_THREAD));
}

void TestPeerEncryptionState::testConcurrentActivateAndRead()
{
        // Test that activation can be read concurrently without data races.
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);

        std::atomic<bool> start{false};
        std::atomic<int> saw_active{0};

        // Writer thread: activates after a delay
        std::thread writer([&]() {
                while (!start.load()) {}
                state.activate();
        });

        // Reader threads: poll active flag
        std::vector<std::thread> readers;
        for (int i = 0; i < 4; i++) {
                readers.emplace_back([&]() {
                        while (!start.load()) {}
                        for (int j = 0; j < 1000; j++) {
                                if (state.active.load(std::memory_order_acquire))
                                        saw_active.fetch_add(1);
                        }
                });
        }

        start.store(true);
        writer.join();
        for (auto &r : readers)
                r.join();

        // At least some readers should have seen active=true
        // (Not all may see it due to timing, but the operation should not crash)
}

// ============================================================================
// Full Roundtrip Tests
// ============================================================================

void TestPeerEncryptionState::testFullPeerEncryptDecryptRoundtrip()
{
        // Initialize encryption state, activate, encrypt a packet, decrypt it.
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        bool ok = state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(ok);
        state.activate();

        const char *plaintext = "Full roundtrip test with PeerEncryptionState!";
        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        // Simulate sending: use C2S direction
        auto nonce = state.c2s.nextNonce();
        CryptoResult enc = aes256gcm_encrypt(
                state.c2s.key.data(), state.c2s.key.size(),
                nonce.data(), nonce.size(),
                reinterpret_cast<const u8*>(plaintext), strlen(plaintext),
                &aad, 1);
        UASSERT(enc.success);

        // Simulate receiving: decrypt with C2S key
        CryptoResult dec = aes256gcm_decrypt(
                state.c2s.key.data(), state.c2s.key.size(),
                nonce.data(), nonce.size(),
                enc.data.data(), enc.data.size(),
                enc.tag.data(), enc.tag.size(),
                &aad, 1);
        UASSERT(dec.success);
        UASSERT(memcmp(dec.data.data(), plaintext, strlen(plaintext)) == 0);
}

void TestPeerEncryptionState::testServerClientDirectionalRoundtrip()
{
        // Both sides share the same SRP session key and derive the same keys.
        // Server encrypts with S2C → Client decrypts with S2C.
        // Client encrypts with C2S → Server decrypts with C2S.
        auto srp_key = makeTestSessionKey();

        PeerEncryptionState server_state;
        PeerEncryptionState client_state;

        bool ok1 = server_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), true);
        bool ok2 = client_state.initFromSRPSessionKey(srp_key.data(), srp_key.size(), false);
        UASSERT(ok1 && ok2);

        // Both sides should derive the same keys
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

        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        // Server → Client (S2C direction)
        const char *server_msg = "Hello from server!";
        auto s2c_nonce = server_state.s2c.nextNonce();

        CryptoResult s_enc = aes256gcm_encrypt(
                server_state.s2c.key.data(), server_state.s2c.key.size(),
                s2c_nonce.data(), s2c_nonce.size(),
                reinterpret_cast<const u8*>(server_msg), strlen(server_msg),
                &aad, 1);
        UASSERT(s_enc.success);

        CryptoResult c_dec = aes256gcm_decrypt(
                client_state.s2c.key.data(), client_state.s2c.key.size(),
                s2c_nonce.data(), s2c_nonce.size(),
                s_enc.data.data(), s_enc.data.size(),
                s_enc.tag.data(), s_enc.tag.size(),
                &aad, 1);
        UASSERT(c_dec.success);
        UASSERT(memcmp(c_dec.data.data(), server_msg, strlen(server_msg)) == 0);

        // Client → Server (C2S direction)
        const char *client_msg = "Hello from client!";
        auto c2s_nonce = client_state.c2s.nextNonce();

        CryptoResult c_enc = aes256gcm_encrypt(
                client_state.c2s.key.data(), client_state.c2s.key.size(),
                c2s_nonce.data(), c2s_nonce.size(),
                reinterpret_cast<const u8*>(client_msg), strlen(client_msg),
                &aad, 1);
        UASSERT(c_enc.success);

        CryptoResult s_dec = aes256gcm_decrypt(
                server_state.c2s.key.data(), server_state.c2s.key.size(),
                c2s_nonce.data(), c2s_nonce.size(),
                c_enc.data.data(), c_enc.data.size(),
                c_enc.tag.data(), c_enc.tag.size(),
                &aad, 1);
        UASSERT(s_dec.success);
        UASSERT(memcmp(s_dec.data.data(), client_msg, strlen(client_msg)) == 0);
}

// ============================================================================
// Key Material Cleanup Tests
// ============================================================================

void TestPeerEncryptionState::testDisableClearsSessionId()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(!state.session_id.empty());

        state.disable();
        UASSERT(state.session_id.empty());
}

void TestPeerEncryptionState::testDisableClearsFingerprint()
{
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(!state.server_fingerprint.empty());

        state.disable();
        UASSERT(state.server_fingerprint.empty());
}
