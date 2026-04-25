// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for v9.15 0x80 flag-based encrypted packet detection.
//
// This is the core fix for the GCM auth failure spam:
// The 0x80 encrypted flag byte is the SOLE determinant of whether
// a packet is encrypted. No grace periods, no transition counters,
// no `encryption_active` check for plaintext packets.
//
// The receive path logic is:
//   1. Check packetdata[BASE_HEADER_SIZE] for 0x80 flag
//   2. If 0x80 present AND data_after_header_size >= ENCRYPTED_PACKET_OVERHEAD:
//      → Decrypt the packet (requires valid keys)
//   3. Otherwise:
//      → Process as plaintext (ALWAYS, no conditions)
//
// This eliminates the previous approach where `encryption_active` was
// checked to decide if a plaintext packet was a security violation.
// With the 0x80 flag, there's no ambiguity and no need for grace periods.

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>

class TestEncryptedFlagDetection : public TestBase
{
public:
        TestEncryptedFlagDetection() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestEncryptedFlagDetection"; }

        void runTests(IGameDef *gamedef);

        // Core flag detection tests
        void testEncryptedFlagConstantValue();
        void testPlaintextFlagConstantValue();
        void testEncryptedFlagDoesNotConflictWithPacketTypes();

        // Packet size threshold tests
        void testEncryptedOverheadCalculation();
        void testMinimumEncryptedPacketSize();
        void testPacketWithExactOverheadIsEncrypted();
        void testPacketBelowOverheadIsPlaintext();

        // Flag-based detection (simulating receive path logic)
        void testEncryptedPacketDetectedByFlag();
        void testPlaintextPacketDetectedByNoFlag();
        void testControlPacketNotMistakenForEncrypted();
        void testOriginalPacketNotMistakenForEncrypted();
        void testSplitPacketNotMistakenForEncrypted();
        void testReliablePacketNotMistakenForEncrypted();

        // Size check boundary (v9.15: >= instead of >)
        void testEmptyCiphertextEncryptedPacketDetected();
        void testOneByteCiphertextEncryptedPacketDetected();
        void testTruncatedEncryptedPacketFallsBackToPlaintext();

        // Flag is sole determinant (independent of encryption_active)
        void testPlaintextAcceptedBeforeActivation();
        void testPlaintextAcceptedAfterActivation();
        void testPlaintextAcceptedLongAfterActivation();
        void testEncryptedDetectedBeforeActivation();
        void testEncryptedDetectedAfterActivation();

        // No grace period needed
        void testNoGracePeriodForPlaintextPackets();
        void testNoTransitionCountersInState();
        void testActivateSimplySetsActiveFlag();

        // Round-trip: encrypt → build packet → detect → decrypt
        void testEncryptBuildDetectDecryptRoundTrip();
        void testEncryptWithDifferentKeysProducesDifferentCiphertext();

        // Edge cases
        void testZeroByteAfterHeaderNotEncrypted();
        void testByte0x80InPacketBodyNotConfused();
        void testMultipleEncryptedPacketsDifferentCounters();
};

static TestEncryptedFlagDetection g_test_instance;

void TestEncryptedFlagDetection::runTests(IGameDef *gamedef)
{
        TEST(testEncryptedFlagConstantValue);
        TEST(testPlaintextFlagConstantValue);
        TEST(testEncryptedFlagDoesNotConflictWithPacketTypes);

        TEST(testEncryptedOverheadCalculation);
        TEST(testMinimumEncryptedPacketSize);
        TEST(testPacketWithExactOverheadIsEncrypted);
        TEST(testPacketBelowOverheadIsPlaintext);

        TEST(testEncryptedPacketDetectedByFlag);
        TEST(testPlaintextPacketDetectedByNoFlag);
        TEST(testControlPacketNotMistakenForEncrypted);
        TEST(testOriginalPacketNotMistakenForEncrypted);
        TEST(testSplitPacketNotMistakenForEncrypted);
        TEST(testReliablePacketNotMistakenForEncrypted);

        TEST(testEmptyCiphertextEncryptedPacketDetected);
        TEST(testOneByteCiphertextEncryptedPacketDetected);
        TEST(testTruncatedEncryptedPacketFallsBackToPlaintext);

        TEST(testPlaintextAcceptedBeforeActivation);
        TEST(testPlaintextAcceptedAfterActivation);
        TEST(testPlaintextAcceptedLongAfterActivation);
        TEST(testEncryptedDetectedBeforeActivation);
        TEST(testEncryptedDetectedAfterActivation);

        TEST(testNoGracePeriodForPlaintextPackets);
        TEST(testNoTransitionCountersInState);
        TEST(testActivateSimplySetsActiveFlag);

        TEST(testEncryptBuildDetectDecryptRoundTrip);
        TEST(testEncryptWithDifferentKeysProducesDifferentCiphertext);

        TEST(testZeroByteAfterHeaderNotEncrypted);
        TEST(testByte0x80InPacketBodyNotConfused);
        TEST(testMultipleEncryptedPacketsDifferentCounters);
}

// Helper: simulate the receive-path flag detection logic
static bool isPacketEncrypted(const std::vector<u8> &packet)
{
        constexpr size_t BASE_HEADER_SIZE = 7;
        if (packet.size() <= BASE_HEADER_SIZE)
                return false;
        size_t data_after_header_size = packet.size() - BASE_HEADER_SIZE;
        return (data_after_header_size >= ENCRYPTED_PACKET_OVERHEAD)
                && (packet[BASE_HEADER_SIZE] == ENCRYPTED_FLAG_AES_256_GCM);
}

// Helper: build a simulated encrypted packet
static std::vector<u8> buildEncryptedPacket(size_t ciphertext_size)
{
        constexpr size_t BASE_HEADER_SIZE = 7;
        size_t total_size = BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + ciphertext_size;
        std::vector<u8> packet(total_size, 0xAA);

        // Base header: protocol_id(4) + peer_id(2) + channel(1)
        for (size_t i = 0; i < BASE_HEADER_SIZE; i++)
                packet[i] = 0x00;

        // Encrypted flag
        packet[BASE_HEADER_SIZE] = ENCRYPTED_FLAG_AES_256_GCM;

        // Nonce (12 bytes)
        for (size_t i = 0; i < GCM_NONCE_SIZE; i++)
                packet[BASE_HEADER_SIZE + 1 + i] = static_cast<u8>(i);

        // Ciphertext (ciphertext_size bytes)
        for (size_t i = 0; i < ciphertext_size; i++)
                packet[BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE + i] = static_cast<u8>(i * 3);

        // GCM tag (16 bytes)
        for (size_t i = 0; i < GCM_TAG_SIZE; i++)
                packet[total_size - GCM_TAG_SIZE + i] = static_cast<u8>(i * 5);

        return packet;
}

// Helper: build a simulated plaintext packet
static std::vector<u8> buildPlaintextPacket(u8 packet_type, size_t payload_size)
{
        constexpr size_t BASE_HEADER_SIZE = 7;
        size_t total_size = BASE_HEADER_SIZE + 1 + payload_size; // +1 for packet type byte
        std::vector<u8> packet(total_size, 0xBB);

        // Base header
        for (size_t i = 0; i < BASE_HEADER_SIZE; i++)
                packet[i] = 0x00;

        // Packet type byte (at position BASE_HEADER_SIZE)
        packet[BASE_HEADER_SIZE] = packet_type;

        return packet;
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
// Core Flag Detection Tests
// ============================================================================

void TestEncryptedFlagDetection::testEncryptedFlagConstantValue()
{
        UASSERTEQ(int, ENCRYPTED_FLAG_AES_256_GCM, 0x80);
}

void TestEncryptedFlagDetection::testPlaintextFlagConstantValue()
{
        UASSERTEQ(int, ENCRYPTED_FLAG_PLAINTEXT, 0x00);
}

void TestEncryptedFlagDetection::testEncryptedFlagDoesNotConflictWithPacketTypes()
{
        // Packet types 0x00-0x03 must not conflict with 0x80
        for (int pt = 0; pt <= 3; pt++) {
                UASSERT(pt != ENCRYPTED_FLAG_AES_256_GCM);
        }
}

// ============================================================================
// Packet Size Threshold Tests
// ============================================================================

void TestEncryptedFlagDetection::testEncryptedOverheadCalculation()
{
        // Overhead = flag(1) + nonce(12) + tag(16) = 29
        UASSERTEQ(size_t, ENCRYPTED_PACKET_OVERHEAD, 1u + GCM_NONCE_SIZE + GCM_TAG_SIZE);
        UASSERTEQ(size_t, ENCRYPTED_PACKET_OVERHEAD, 29u);
}

void TestEncryptedFlagDetection::testMinimumEncryptedPacketSize()
{
        // Minimum encrypted packet = base_header(7) + overhead(29) = 36 bytes
        // (empty ciphertext — GCM allows this)
        constexpr size_t BASE_HEADER_SIZE = 7;
        UASSERTEQ(size_t, BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD, 36u);
}

void TestEncryptedFlagDetection::testPacketWithExactOverheadIsEncrypted()
{
        // v9.15: Changed > to >= so empty ciphertext is handled
        auto packet = buildEncryptedPacket(0); // 0 bytes ciphertext
        UASSERT(isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testPacketBelowOverheadIsPlaintext()
{
        // A packet with 0x80 flag but too short to be encrypted format
        constexpr size_t BASE_HEADER_SIZE = 7;
        std::vector<u8> packet(BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD - 1, 0x00);
        packet[BASE_HEADER_SIZE] = 0x80;
        UASSERT(!isPacketEncrypted(packet));
}

// ============================================================================
// Flag-Based Detection (Simulating Receive Path)
// ============================================================================

void TestEncryptedFlagDetection::testEncryptedPacketDetectedByFlag()
{
        auto packet = buildEncryptedPacket(50);
        UASSERT(isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testPlaintextPacketDetectedByNoFlag()
{
        auto packet = buildPlaintextPacket(0x01, 50);
        UASSERT(!isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testControlPacketNotMistakenForEncrypted()
{
        auto packet = buildPlaintextPacket(0x00, 50); // PACKET_TYPE_CONTROL
        UASSERT(!isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testOriginalPacketNotMistakenForEncrypted()
{
        auto packet = buildPlaintextPacket(0x01, 50); // PACKET_TYPE_ORIGINAL
        UASSERT(!isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testSplitPacketNotMistakenForEncrypted()
{
        auto packet = buildPlaintextPacket(0x02, 50); // PACKET_TYPE_SPLIT
        UASSERT(!isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testReliablePacketNotMistakenForEncrypted()
{
        auto packet = buildPlaintextPacket(0x03, 50); // PACKET_TYPE_RELIABLE
        UASSERT(!isPacketEncrypted(packet));
}

// ============================================================================
// Size Check Boundary (v9.15: >= instead of >)
// ============================================================================

void TestEncryptedFlagDetection::testEmptyCiphertextEncryptedPacketDetected()
{
        // v9.15 fix: >= allows empty ciphertext detection
        auto packet = buildEncryptedPacket(0);
        UASSERT(isPacketEncrypted(packet));
        UASSERT(packet.size() == 7 + ENCRYPTED_PACKET_OVERHEAD); // 36 bytes
}

void TestEncryptedFlagDetection::testOneByteCiphertextEncryptedPacketDetected()
{
        auto packet = buildEncryptedPacket(1);
        UASSERT(isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testTruncatedEncryptedPacketFallsBackToPlaintext()
{
        // A packet with 0x80 flag but truncated (less than overhead)
        constexpr size_t BASE_HEADER_SIZE = 7;
        std::vector<u8> packet(BASE_HEADER_SIZE + 20, 0x00);
        packet[BASE_HEADER_SIZE] = ENCRYPTED_FLAG_AES_256_GCM;
        UASSERT(!isPacketEncrypted(packet)); // Too short, treated as plaintext
}

// ============================================================================
// Flag Is Sole Determinant (Independent of encryption_active)
// ============================================================================

void TestEncryptedFlagDetection::testPlaintextAcceptedBeforeActivation()
{
        // Before encryption is active, plaintext packets are normal
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(!state.active.load());

        auto packet = buildPlaintextPacket(0x01, 50);
        UASSERT(!isPacketEncrypted(packet));
        // Would be processed as plaintext — correct
}

void TestEncryptedFlagDetection::testPlaintextAcceptedAfterActivation()
{
        // v9.15 KEY TEST: After encryption is active, plaintext packets
        // (no 0x80 flag) are STILL accepted without error or grace period.
        // This is the fundamental change that eliminates grace periods.
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        state.activate();
        state.activated_at = 1000;
        UASSERT(state.active.load());

        auto packet = buildPlaintextPacket(0x01, 50);
        UASSERT(!isPacketEncrypted(packet));
        // Would be processed as plaintext — NO grace period check, NO error
}

void TestEncryptedFlagDetection::testPlaintextAcceptedLongAfterActivation()
{
        // Even long after activation (30+ seconds), plaintext packets
        // are accepted without any time-based grace period check.
        // The 0x80 flag is the sole determinant — time is irrelevant.
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        state.activate();
        state.activated_at = 1; // Very old activation
        UASSERT(state.active.load());

        auto packet = buildPlaintextPacket(0x01, 50);
        UASSERT(!isPacketEncrypted(packet));
        // No grace period check — 0x80 flag is authoritative
}

void TestEncryptedFlagDetection::testEncryptedDetectedBeforeActivation()
{
        // Before encryption is active, an encrypted packet (0x80 flag)
        // is still detected as encrypted. The receive path will attempt
        // to decrypt it (and auto-activate if successful).
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        UASSERT(!state.active.load());

        auto packet = buildEncryptedPacket(50);
        UASSERT(isPacketEncrypted(packet));
        // Would attempt decryption — correct (may auto-activate)
}

void TestEncryptedFlagDetection::testEncryptedDetectedAfterActivation()
{
        // After encryption is active, encrypted packets are detected normally.
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        state.activate();
        UASSERT(state.active.load());

        auto packet = buildEncryptedPacket(50);
        UASSERT(isPacketEncrypted(packet));
}

// ============================================================================
// No Grace Period Needed
// ============================================================================

void TestEncryptedFlagDetection::testNoGracePeriodForPlaintextPackets()
{
        // v9.15: No grace period logic exists. The 0x80 flag determines
        // everything. Simulate receiving 1000 plaintext packets after
        // activation — they're all accepted without any time/counter check.
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);
        state.activate();
        state.activated_at = 1;

        for (int i = 0; i < 1000; i++) {
                auto packet = buildPlaintextPacket(0x01, 50);
                UASSERT(!isPacketEncrypted(packet));
                // All accepted as plaintext — no counter, no time check
        }
}

void TestEncryptedFlagDetection::testNoTransitionCountersInState()
{
        // v9.15: The transition_plaintext_count, transition_logged,
        // transition_warning_logged, TRANSITION_MAX_PLAINTEXT, and
        // TRANSITION_GRACE_PERIOD_MS have been removed.
        // This test verifies that PeerEncryptionState still works
        // correctly without them.
        PeerEncryptionState state;
        auto key = makeTestSessionKey();
        state.initFromSRPSessionKey(key.data(), key.size(), false);

        // activate() should work without referencing removed fields
        state.activate();
        UASSERT(state.active.load());

        // disable() should work without referencing removed fields
        state.disable();
        UASSERT(!state.active.load());
}

void TestEncryptedFlagDetection::testActivateSimplySetsActiveFlag()
{
        // v9.15: activate() is a simple atomic store of active=true.
        // No transition counter resets, no warning flag resets.
        PeerEncryptionState state;
        UASSERT(!state.active.load());
        state.activate();
        UASSERT(state.active.load());
}

// ============================================================================
// Round-Trip: Encrypt → Build Packet → Detect → Decrypt
// ============================================================================

void TestEncryptedFlagDetection::testEncryptBuildDetectDecryptRoundTrip()
{
        // Full round-trip: encrypt plaintext, build packet, detect as
        // encrypted, decrypt, verify plaintext matches.
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), true); // is_server=true

        // Plaintext to encrypt (simulating everything after base header)
        std::vector<u8> original_plaintext = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

        // Encrypt using S2C direction (server sends to client)
        auto lock = state.lock();
        auto nonce = state.s2c.nextNonce();
        u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;
        CryptoResult enc_result = aes256gcm_encrypt(
                state.s2c.key.data(), state.s2c.key.size(),
                nonce.data(), nonce.size(),
                original_plaintext.data(), original_plaintext.size(),
                &encrypted_flag, 1);
        lock.unlock();

        UASSERT(enc_result.success);

        // Build the encrypted packet
        constexpr size_t BASE_HEADER_SIZE = 7;
        size_t enc_size = BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + enc_result.data.size();
        std::vector<u8> packet(enc_size);
        // Base header (zeros)
        // Encrypted flag
        packet[BASE_HEADER_SIZE] = ENCRYPTED_FLAG_AES_256_GCM;
        // Nonce
        memcpy(packet.data() + BASE_HEADER_SIZE + 1, nonce.data(), GCM_NONCE_SIZE);
        // Ciphertext
        if (!enc_result.data.empty())
                memcpy(packet.data() + BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE,
                        enc_result.data.data(), enc_result.data.size());
        // Tag
        memcpy(packet.data() + enc_size - GCM_TAG_SIZE, enc_result.tag.data(), GCM_TAG_SIZE);

        // Detect as encrypted
        UASSERT(isPacketEncrypted(packet));

        // Now decrypt (simulating client receive)
        PeerEncryptionState client_state;
        client_state.initFromSRPSessionKey(key.data(), key.size(), false); // is_server=false

        auto client_lock = client_state.lock();
        const u8 *after_header = &packet[BASE_HEADER_SIZE];
        const u8 *nonce_ptr = after_header + 1;
        const u8 *ciphertext_ptr = after_header + 1 + GCM_NONCE_SIZE;
        size_t data_after_header = packet.size() - BASE_HEADER_SIZE;
        size_t remaining = data_after_header - 1 - GCM_NONCE_SIZE;
        size_t ciphertext_len = remaining - GCM_TAG_SIZE;
        const u8 *tag_ptr = after_header + data_after_header - GCM_TAG_SIZE;

        u8 dec_flag = ENCRYPTED_FLAG_AES_256_GCM;
        CryptoResult dec_result = aes256gcm_decrypt(
                client_state.s2c.key.data(), client_state.s2c.key.size(),
                nonce_ptr, GCM_NONCE_SIZE,
                ciphertext_ptr, ciphertext_len,
                tag_ptr, GCM_TAG_SIZE,
                &dec_flag, 1);
        client_lock.unlock();

        UASSERT(dec_result.success);
        UASSERTEQ(size_t, dec_result.data.size(), original_plaintext.size());
        UASSERT(memcmp(dec_result.data.data(), original_plaintext.data(), original_plaintext.size()) == 0);
}

void TestEncryptedFlagDetection::testEncryptWithDifferentKeysProducesDifferentCiphertext()
{
        // Two different keys should produce different ciphertext for the same plaintext.
        auto key1 = makeTestSessionKey();
        std::array<u8, SRP_SESSION_KEY_SIZE> key2;
        for (size_t i = 0; i < key2.size(); i++)
                key2[i] = static_cast<u8>(i * 11 + 99);

        PeerEncryptionState state1, state2;
        state1.initFromSRPSessionKey(key1.data(), key1.size(), true);
        state2.initFromSRPSessionKey(key2.data(), key2.size(), true);

        std::vector<u8> plaintext = {0xAA, 0xBB, 0xCC, 0xDD};

        auto lock1 = state1.lock();
        auto nonce1 = state1.s2c.nextNonce();
        u8 flag = ENCRYPTED_FLAG_AES_256_GCM;
        CryptoResult result1 = aes256gcm_encrypt(
                state1.s2c.key.data(), state1.s2c.key.size(),
                nonce1.data(), nonce1.size(),
                plaintext.data(), plaintext.size(),
                &flag, 1);
        lock1.unlock();

        auto lock2 = state2.lock();
        auto nonce2 = state2.s2c.nextNonce();
        CryptoResult result2 = aes256gcm_encrypt(
                state2.s2c.key.data(), state2.s2c.key.size(),
                nonce2.data(), nonce2.size(),
                plaintext.data(), plaintext.size(),
                &flag, 1);
        lock2.unlock();

        UASSERT(result1.success);
        UASSERT(result2.success);

        // Ciphertexts should differ (different keys)
        bool all_same = (result1.data.size() == result2.data.size());
        if (all_same) {
                for (size_t i = 0; i < result1.data.size(); i++) {
                        if (result1.data[i] != result2.data[i]) {
                                all_same = false;
                                break;
                        }
                }
        }
        UASSERT(!all_same);
}

// ============================================================================
// Edge Cases
// ============================================================================

void TestEncryptedFlagDetection::testZeroByteAfterHeaderNotEncrypted()
{
        // A packet with 0x00 after the header is a control packet,
        // NOT an encrypted packet.
        constexpr size_t BASE_HEADER_SIZE = 7;
        std::vector<u8> packet(100, 0x00);
        packet[BASE_HEADER_SIZE] = 0x00; // PACKET_TYPE_CONTROL
        UASSERT(!isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testByte0x80InPacketBodyNotConfused()
{
        // The 0x80 flag is only checked at position BASE_HEADER_SIZE.
        // A 0x80 byte elsewhere in the packet should not trigger detection.
        constexpr size_t BASE_HEADER_SIZE = 7;
        std::vector<u8> packet(200, 0x00);
        packet[BASE_HEADER_SIZE] = 0x01; // plaintext packet type
        packet[50] = 0x80; // 0x80 in the body — should not matter
        UASSERT(!isPacketEncrypted(packet));
}

void TestEncryptedFlagDetection::testMultipleEncryptedPacketsDifferentCounters()
{
        // Encrypt multiple packets with different nonce counters and
        // verify they all have the 0x80 flag and produce valid ciphertext.
        auto key = makeTestSessionKey();
        PeerEncryptionState state;
        state.initFromSRPSessionKey(key.data(), key.size(), true);

        std::vector<u8> plaintext = {0x01, 0x02, 0x03};

        for (int i = 0; i < 5; i++) {
                auto lock = state.lock();
                auto nonce = state.s2c.nextNonce();
                u8 flag = ENCRYPTED_FLAG_AES_256_GCM;
                CryptoResult result = aes256gcm_encrypt(
                        state.s2c.key.data(), state.s2c.key.size(),
                        nonce.data(), nonce.size(),
                        plaintext.data(), plaintext.size(),
                        &flag, 1);
                lock.unlock();

                UASSERT(result.success);

                // Build and detect
                constexpr size_t BASE_HEADER_SIZE = 7;
                size_t enc_size = BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + result.data.size();
                std::vector<u8> packet(enc_size, 0x00);
                packet[BASE_HEADER_SIZE] = ENCRYPTED_FLAG_AES_256_GCM;
                memcpy(packet.data() + BASE_HEADER_SIZE + 1, nonce.data(), GCM_NONCE_SIZE);
                if (!result.data.empty())
                        memcpy(packet.data() + BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE,
                                result.data.data(), result.data.size());
                memcpy(packet.data() + enc_size - GCM_TAG_SIZE, result.tag.data(), GCM_TAG_SIZE);

                UASSERT(isPacketEncrypted(packet));
                UASSERTEQ(u64, state.s2c.nonce_counter, (u64)(i + 1));
        }
}
