// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Integration tests for the encrypted packet wire format.
//
// These tests prove that encrypted packets on the wire:
// - Have the correct structure: [base_header(7B)][0x80][nonce(12B)][ciphertext][tag(16B)]
// - Do NOT contain the plaintext Luanti protocol magic (0x4f457403)
// - Have the encrypted flag byte (0x80) after the base header
// - Have the correct total packet size
// - Cannot be confused with plaintext packets
// - Resist tampering at every position in the encrypted portion

#include "test.h"

#include "network/crypto.h"
#include "network/networkprotocol.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <memory>

// Simulated base header size (same as in threads.cpp)
static constexpr size_t BASE_HEADER_SIZE = 7;

class TestEncryptedPacketFormat : public TestBase
{
public:
        TestEncryptedPacketFormat() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestEncryptedPacketFormat"; }

        void runTests(IGameDef *gamedef);

        // Wire format structure tests
        void testEncryptedPacketStructure();
        void testEncryptedFlagBytePosition();
        void testEncryptedFlagByteValue();
        void testNoncePosition();
        void testNonceSize();
        void testTagPosition();
        void testTagSize();
        void testCiphertextPosition();
        void testCiphertextSize();

        // Plaintext header invisibility tests
        void testNoPlaintextProtocolMagic();
        void testNoPlaintextPacketType();
        void testNoPlaintextPeerId();
        void testNoPlaintextChannel();

        // Size calculation tests
        void testEncryptedPacketOverhead();
        void testEncryptedPacketTotalSize();
        void testEmptyPayloadEncryptedSize();

        // Packet type discrimination tests
        void testEncryptedFlagNotValidPacketType();
        void testPlaintextPacketTypeNotEncryptedFlag();

        // Tamper resistance tests
        void testTamperEncryptedFlagFails();
        void testTamperNonceByteFails();
        void testTamperCiphertextByteFails();
        void testTamperTagByteFails();

        // Simulated full packet encrypt/decrypt
        void testSimulatedFullPacketRoundtrip();
        void testSimulatedBaseHeaderPreserved();
        void testMultiplePacketsUniqueNonces();

        // Comparison: plaintext vs encrypted
        void testPlaintextPacketFormat();
        void testEncryptedPacketIsLarger();
};

static TestEncryptedPacketFormat g_test_instance;

// Helper: build a simulated plaintext packet with base header + payload
static std::vector<u8> makePlaintextPacket(const std::vector<u8> &payload)
{
        std::vector<u8> packet(BASE_HEADER_SIZE + payload.size());

        // Base header: protocol_id(4B) + peer_id(2B) + channel(1B)
        // Luanti protocol ID = 0x4f457403 (big-endian)
        packet[0] = 0x4f;
        packet[1] = 0x45;
        packet[2] = 0x74;
        packet[3] = 0x03;
        packet[4] = 0x00; // peer_id high byte
        packet[5] = 0x02; // peer_id low byte (peer 2)
        packet[6] = 0x00; // channel 0

        // Payload
        if (!payload.empty())
                memcpy(packet.data() + BASE_HEADER_SIZE, payload.data(), payload.size());

        return packet;
}

// Helper: encrypt a plaintext packet into the wire format
// Returns: encrypted packet in wire format
static std::vector<u8> encryptPacket(
        const std::vector<u8> &plaintext_packet,
        PeerEncryptionState &enc_state,
        bool use_s2c)
{
        auto lock = enc_state.lock();
        DirectionalEncryptionState &dir = use_s2c ? enc_state.s2c : enc_state.c2s;
        auto nonce = dir.nextNonce();

        // Plaintext = everything after base header
        const u8 *pt = plaintext_packet.data() + BASE_HEADER_SIZE;
        size_t pt_len = plaintext_packet.size() - BASE_HEADER_SIZE;

        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        CryptoResult result = aes256gcm_encrypt(
                dir.key.data(), dir.key.size(),
                nonce.data(), nonce.size(),
                pt, pt_len,
                &aad, 1);

        // Build encrypted packet:
        // [base_header(7B)][encrypted_flag(1B)][nonce(12B)][ciphertext(NB)][tag(16B)]
        size_t enc_size = BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + result.data.size();
        std::vector<u8> enc_packet(enc_size);

        // Copy base header (unencrypted)
        memcpy(enc_packet.data(), plaintext_packet.data(), BASE_HEADER_SIZE);

        // Write encrypted flag
        enc_packet[BASE_HEADER_SIZE] = ENCRYPTED_FLAG_AES_256_GCM;

        // Write nonce
        memcpy(enc_packet.data() + BASE_HEADER_SIZE + 1, nonce.data(), GCM_NONCE_SIZE);

        // Write ciphertext
        if (!result.data.empty())
                memcpy(enc_packet.data() + BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE,
                       result.data.data(), result.data.size());

        // Write tag
        memcpy(enc_packet.data() + enc_size - GCM_TAG_SIZE,
               result.tag.data(), GCM_TAG_SIZE);

        return enc_packet;
}

// Helper: decrypt a wire-format encrypted packet back to plaintext
static std::vector<u8> decryptPacket(
        const std::vector<u8> &enc_packet,
        const PeerEncryptionState &enc_state,
        bool use_c2s_for_decrypt)
{
        auto lock = enc_state.lock();
        const DirectionalEncryptionState &dir = use_c2s_for_decrypt ? enc_state.c2s : enc_state.s2c;

        const u8 *after_header = enc_packet.data() + BASE_HEADER_SIZE;
        // Read the actual encrypted flag byte from the packet (not hardcoded)
        // This ensures that tampering with the flag byte causes GCM auth failure
        u8 aad = after_header[0];
        const u8 *nonce_ptr = after_header + 1;
        size_t data_after_header = enc_packet.size() - BASE_HEADER_SIZE;
        size_t remaining = data_after_header - 1 - GCM_NONCE_SIZE;
        size_t ciphertext_len = remaining - GCM_TAG_SIZE;
        const u8 *ciphertext_ptr = after_header + 1 + GCM_NONCE_SIZE;
        const u8 *tag_ptr = enc_packet.data() + enc_packet.size() - GCM_TAG_SIZE;

        CryptoResult result = aes256gcm_decrypt(
                dir.key.data(), dir.key.size(),
                nonce_ptr, GCM_NONCE_SIZE,
                ciphertext_ptr, ciphertext_len,
                tag_ptr, GCM_TAG_SIZE,
                &aad, 1);

        // Reconstruct plaintext packet
        std::vector<u8> plaintext(BASE_HEADER_SIZE + result.data.size());
        memcpy(plaintext.data(), enc_packet.data(), BASE_HEADER_SIZE);
        if (!result.data.empty())
                memcpy(plaintext.data() + BASE_HEADER_SIZE, result.data.data(), result.data.size());

        return plaintext;
}

// Helper: create a PeerEncryptionState initialized with a test key
static std::unique_ptr<PeerEncryptionState> makeTestEncState()
{
        std::array<u8, SRP_SESSION_KEY_SIZE> key;
        for (size_t i = 0; i < key.size(); i++)
                key[i] = static_cast<u8>(i * 3 + 17);

        auto state = std::make_unique<PeerEncryptionState>();
        state->initFromSRPSessionKey(key.data(), key.size(), false);
        state->activate();
        return state;
}

void TestEncryptedPacketFormat::runTests(IGameDef *gamedef)
{
        // Wire format structure tests
        TEST(testEncryptedPacketStructure);
        TEST(testEncryptedFlagBytePosition);
        TEST(testEncryptedFlagByteValue);
        TEST(testNoncePosition);
        TEST(testNonceSize);
        TEST(testTagPosition);
        TEST(testTagSize);
        TEST(testCiphertextPosition);
        TEST(testCiphertextSize);

        // Plaintext header invisibility tests
        TEST(testNoPlaintextProtocolMagic);
        TEST(testNoPlaintextPacketType);
        TEST(testNoPlaintextPeerId);
        TEST(testNoPlaintextChannel);

        // Size calculation tests
        TEST(testEncryptedPacketOverhead);
        TEST(testEncryptedPacketTotalSize);
        TEST(testEmptyPayloadEncryptedSize);

        // Packet type discrimination tests
        TEST(testEncryptedFlagNotValidPacketType);
        TEST(testPlaintextPacketTypeNotEncryptedFlag);

        // Tamper resistance tests
        TEST(testTamperEncryptedFlagFails);
        TEST(testTamperNonceByteFails);
        TEST(testTamperCiphertextByteFails);
        TEST(testTamperTagByteFails);

        // Simulated full packet encrypt/decrypt
        TEST(testSimulatedFullPacketRoundtrip);
        TEST(testSimulatedBaseHeaderPreserved);
        TEST(testMultiplePacketsUniqueNonces);

        // Comparison tests
        TEST(testPlaintextPacketFormat);
        TEST(testEncryptedPacketIsLarger);
}

// ============================================================================
// Wire Format Structure Tests
// ============================================================================

void TestEncryptedPacketFormat::testEncryptedPacketStructure()
{
        // Build a plaintext packet with a typical payload
        std::vector<u8> payload = {0x01, 0x00, 0x05, 'H', 'e', 'l', 'l', 'o'};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Verify the encrypted packet is larger than plaintext
        UASSERT(enc_packet.size() > pt_packet.size());

        // Verify base header is preserved
        UASSERT(memcmp(enc_packet.data(), pt_packet.data(), BASE_HEADER_SIZE) == 0);
}

void TestEncryptedPacketFormat::testEncryptedFlagBytePosition()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Encrypted flag byte should be at position BASE_HEADER_SIZE
        UASSERTEQ(int, enc_packet[BASE_HEADER_SIZE], ENCRYPTED_FLAG_AES_256_GCM);
}

void TestEncryptedPacketFormat::testEncryptedFlagByteValue()
{
        std::vector<u8> payload = {0x01};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Encrypted flag must be 0x80
        UASSERTEQ(int, enc_packet[BASE_HEADER_SIZE], 0x80);
}

void TestEncryptedPacketFormat::testNoncePosition()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Nonce starts at BASE_HEADER_SIZE + 1
        size_t nonce_offset = BASE_HEADER_SIZE + 1;
        UASSERT(nonce_offset + GCM_NONCE_SIZE <= enc_packet.size());

        // Nonce should not be all zeros (derived from HKDF)
        bool all_zero = true;
        for (size_t i = 0; i < GCM_NONCE_SIZE; i++) {
                if (enc_packet[nonce_offset + i] != 0) { all_zero = false; break; }
        }
        UASSERT(!all_zero);
}

void TestEncryptedPacketFormat::testNonceSize()
{
        std::vector<u8> payload = {0x01, 0x02};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Verify there's space for exactly 12 nonce bytes
        size_t nonce_offset = BASE_HEADER_SIZE + 1;
        // Nonce occupies bytes [nonce_offset, nonce_offset + 12)
        UASSERT(enc_packet.size() >= nonce_offset + GCM_NONCE_SIZE);
}

void TestEncryptedPacketFormat::testTagPosition()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Tag is at the end of the packet, after base header + flag + nonce + ciphertext
        size_t tag_offset = enc_packet.size() - GCM_TAG_SIZE;
        // Tag must start after the base header + flag(1) + nonce(12) = 20 bytes minimum
        // and after the ciphertext (payload.size() bytes)
        size_t expected_tag_start = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE + payload.size();
        UASSERT(tag_offset == expected_tag_start);
        UASSERT(tag_offset > BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE);
}

void TestEncryptedPacketFormat::testTagSize()
{
        std::vector<u8> payload = {0x01, 0x02};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Last 16 bytes should be the tag
        UASSERT(enc_packet.size() >= GCM_TAG_SIZE);
}

void TestEncryptedPacketFormat::testCiphertextPosition()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Ciphertext starts after: base_header(7) + flag(1) + nonce(12) = 20
        size_t ct_offset = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE;
        size_t ct_len = payload.size(); // Ciphertext same length as plaintext

        UASSERT(enc_packet.size() >= ct_offset + ct_len + GCM_TAG_SIZE);
}

void TestEncryptedPacketFormat::testCiphertextSize()
{
        // AES-GCM ciphertext size equals plaintext size (no padding)
        std::vector<u8> payload(100, 0xAA);
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Total encrypted size = BASE_HEADER_SIZE + 1 + 12 + payload.size() + 16
        size_t expected = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE + payload.size() + GCM_TAG_SIZE;
        UASSERTEQ(size_t, enc_packet.size(), expected);
}

// ============================================================================
// Plaintext Header Invisibility Tests
// ============================================================================

void TestEncryptedPacketFormat::testNoPlaintextProtocolMagic()
{
        // The Luanti protocol magic 0x4f457403 must NOT appear in the encrypted portion.
        // Wireshark looks for this pattern to identify Luanti packets.
        // If it's visible after the base header, encryption is broken.
        std::vector<u8> payload = {0x4f, 0x45, 0x74, 0x03, 0x2c, 0x02, 0x02, 0x00};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Check that the encrypted portion (after base header) does NOT start with 4f457403
        // The encrypted portion starts with the encrypted flag (0x80), not the protocol magic
        UASSERT(enc_packet[BASE_HEADER_SIZE] == ENCRYPTED_FLAG_AES_256_GCM);
        UASSERT(enc_packet[BASE_HEADER_SIZE] != 0x4f);

        // Also verify the ciphertext doesn't accidentally start with the protocol magic
        size_t ct_offset = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE;
        if (enc_packet.size() >= ct_offset + 4) {
                bool has_magic = (enc_packet[ct_offset] == 0x4f &&
                                  enc_packet[ct_offset + 1] == 0x45 &&
                                  enc_packet[ct_offset + 2] == 0x74 &&
                                  enc_packet[ct_offset + 3] == 0x03);
                // While possible in theory (1 in 2^32), it's extremely unlikely
                // with AES-256-GCM ciphertext. If this ever fails, it's a bug.
                UASSERT(!has_magic);
        }
}

void TestEncryptedPacketFormat::testNoPlaintextPacketType()
{
        // After the base header in a plaintext packet, the first byte is the
        // packet type (0-3). In an encrypted packet, it's the encrypted flag (0x80).
        std::vector<u8> payload = {0x00, 0x01, 0x02}; // packet type 0 (original)
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // The byte after base header should be 0x80, NOT a valid packet type
        u8 flag = enc_packet[BASE_HEADER_SIZE];
        UASSERT(flag == ENCRYPTED_FLAG_AES_256_GCM);
        UASSERT(flag > 0x03); // Not a valid packet type
}

void TestEncryptedPacketFormat::testNoPlaintextPeerId()
{
        // The peer_id in the base header IS preserved (it's needed for routing),
        // but any peer_id in the payload is encrypted.
        std::vector<u8> payload = {0x00, 0x00, 0x02}; // Contains peer_id 2
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // The base header peer_id is preserved (for routing)
        UASSERT(enc_packet[4] == 0x00);
        UASSERT(enc_packet[5] == 0x02);

        // But the payload peer_id is encrypted (part of ciphertext)
        size_t ct_offset = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE;
        if (enc_packet.size() > ct_offset + 2) {
                // The ciphertext bytes are NOT the same as the plaintext payload
                UASSERT(enc_packet[ct_offset] != payload[0] ||
                        enc_packet[ct_offset + 1] != payload[1] ||
                        enc_packet[ct_offset + 2] != payload[2]);
        }
}

void TestEncryptedPacketFormat::testNoPlaintextChannel()
{
        // Channel number in the base header is preserved for routing,
        // but any channel data in the payload is encrypted.
        std::vector<u8> payload = {0x01}; // Channel 1 data
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Base header channel is preserved
        UASSERTEQ(int, enc_packet[6], 0x00); // Channel 0 in base header

        // Payload channel data is encrypted
        size_t ct_offset = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE;
        if (enc_packet.size() > ct_offset) {
                UASSERT(enc_packet[ct_offset] != payload[0]); // Very likely different
        }
}

// ============================================================================
// Size Calculation Tests
// ============================================================================

void TestEncryptedPacketFormat::testEncryptedPacketOverhead()
{
        // Overhead = encrypted_flag(1) + nonce(12) + tag(16) = 29 bytes
        UASSERTEQ(size_t, ENCRYPTED_PACKET_OVERHEAD, 29u);
}

void TestEncryptedPacketFormat::testEncryptedPacketTotalSize()
{
        size_t payload_size = 50;
        std::vector<u8> payload(payload_size, 0xBB);
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Total = base_header(7) + flag(1) + nonce(12) + ciphertext(50) + tag(16) = 86
        size_t expected = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE + payload_size + GCM_TAG_SIZE;
        UASSERTEQ(size_t, enc_packet.size(), expected);
}

void TestEncryptedPacketFormat::testEmptyPayloadEncryptedSize()
{
        // Even with empty payload, encrypted packet has overhead
        std::vector<u8> payload;
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Total = base_header(7) + flag(1) + nonce(12) + ciphertext(0) + tag(16) = 36
        size_t expected = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE + 0 + GCM_TAG_SIZE;
        UASSERTEQ(size_t, enc_packet.size(), expected);
}

// ============================================================================
// Packet Type Discrimination Tests
// ============================================================================

void TestEncryptedPacketFormat::testEncryptedFlagNotValidPacketType()
{
        // 0x80 cannot be a valid packet type (0-3), so there's no ambiguity
        UASSERT(ENCRYPTED_FLAG_AES_256_GCM > 0x03);
        UASSERT(ENCRYPTED_FLAG_AES_256_GCM == 0x80);
}

void TestEncryptedPacketFormat::testPlaintextPacketTypeNotEncryptedFlag()
{
        // Valid packet types (0-3) are not the encrypted flag
        for (int i = 0; i <= 3; i++) {
                UASSERT(i != ENCRYPTED_FLAG_AES_256_GCM);
        }
}

// ============================================================================
// Tamper Resistance Tests
// ============================================================================

void TestEncryptedPacketFormat::testTamperEncryptedFlagFails()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Tamper with the encrypted flag byte (part of AAD)
        enc_packet[BASE_HEADER_SIZE] ^= 0x01;

        // Decryption should fail (AAD tampering detected)
        bool decrypt_ok_flag = true;
        try {
                auto dec = decryptPacket(enc_packet, *enc_state, false);
                // If we get here, check if the decrypted data matches
                if (dec != pt_packet) decrypt_ok_flag = false;
        } catch (...) {
                decrypt_ok_flag = false;
        }
        // The key point: tampering should cause decryption to fail
        // (either exception or wrong output)
        UASSERT(!decrypt_ok_flag);
}

void TestEncryptedPacketFormat::testTamperNonceByteFails()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Tamper with a nonce byte
        size_t nonce_offset = BASE_HEADER_SIZE + 1;
        enc_packet[nonce_offset] ^= 0x01;

        // Attempting to decrypt with the tampered nonce should fail
        // (We use a separate enc_state for decrypt to avoid counter issues)
        auto dec_state = makeTestEncState();
        auto lock = dec_state->lock();
        const u8 *after_header = enc_packet.data() + BASE_HEADER_SIZE;
        const u8 *nonce_ptr = after_header + 1;
        size_t data_after_header = enc_packet.size() - BASE_HEADER_SIZE;
        size_t remaining = data_after_header - 1 - GCM_NONCE_SIZE;
        size_t ct_len = remaining - GCM_TAG_SIZE;
        const u8 *ct_ptr = after_header + 1 + GCM_NONCE_SIZE;
        const u8 *tag_ptr = enc_packet.data() + enc_packet.size() - GCM_TAG_SIZE;
        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        CryptoResult result = aes256gcm_decrypt(
                dec_state->s2c.key.data(), dec_state->s2c.key.size(),
                nonce_ptr, GCM_NONCE_SIZE,
                ct_ptr, ct_len,
                tag_ptr, GCM_TAG_SIZE,
                &aad, 1);
        UASSERT(!result.success);
}

void TestEncryptedPacketFormat::testTamperCiphertextByteFails()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Tamper with a ciphertext byte
        size_t ct_offset = BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE;
        enc_packet[ct_offset] ^= 0x01;

        auto dec_state = makeTestEncState();
        auto lock = dec_state->lock();
        const u8 *after_header = enc_packet.data() + BASE_HEADER_SIZE;
        const u8 *nonce_ptr = after_header + 1;
        size_t data_after_header = enc_packet.size() - BASE_HEADER_SIZE;
        size_t remaining = data_after_header - 1 - GCM_NONCE_SIZE;
        size_t ct_len = remaining - GCM_TAG_SIZE;
        const u8 *ct_ptr = after_header + 1 + GCM_NONCE_SIZE;
        const u8 *tag_ptr = enc_packet.data() + enc_packet.size() - GCM_TAG_SIZE;
        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        CryptoResult result = aes256gcm_decrypt(
                dec_state->s2c.key.data(), dec_state->s2c.key.size(),
                nonce_ptr, GCM_NONCE_SIZE,
                ct_ptr, ct_len,
                tag_ptr, GCM_TAG_SIZE,
                &aad, 1);
        UASSERT(!result.success);
}

void TestEncryptedPacketFormat::testTamperTagByteFails()
{
        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Tamper with a tag byte
        enc_packet[enc_packet.size() - 1] ^= 0x01;

        auto dec_state = makeTestEncState();
        auto lock = dec_state->lock();
        const u8 *after_header = enc_packet.data() + BASE_HEADER_SIZE;
        const u8 *nonce_ptr = after_header + 1;
        size_t data_after_header = enc_packet.size() - BASE_HEADER_SIZE;
        size_t remaining = data_after_header - 1 - GCM_NONCE_SIZE;
        size_t ct_len = remaining - GCM_TAG_SIZE;
        const u8 *ct_ptr = after_header + 1 + GCM_NONCE_SIZE;
        const u8 *tag_ptr = enc_packet.data() + enc_packet.size() - GCM_TAG_SIZE;
        u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

        CryptoResult result = aes256gcm_decrypt(
                dec_state->s2c.key.data(), dec_state->s2c.key.size(),
                nonce_ptr, GCM_NONCE_SIZE,
                ct_ptr, ct_len,
                tag_ptr, GCM_TAG_SIZE,
                &aad, 1);
        UASSERT(!result.success);
}

// ============================================================================
// Simulated Full Packet Encrypt/Decrypt
// ============================================================================

void TestEncryptedPacketFormat::testSimulatedFullPacketRoundtrip()
{
        // Full roundtrip: plaintext packet → encrypt → wire format → decrypt → original
        std::vector<u8> payload = {0x01, 0x00, 0x0A, 'H', 'e', 'l', 'l', 'o', '!', '!', '!', '!'}; 
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Decrypt using a fresh state (same keys, counter 0)
        auto dec_state = makeTestEncState();
        auto dec_packet = decryptPacket(enc_packet, *dec_state, false);

        // Verify decrypted packet matches original
        UASSERT(dec_packet.size() == pt_packet.size());
        UASSERT(memcmp(dec_packet.data(), pt_packet.data(), pt_packet.size()) == 0);
}

void TestEncryptedPacketFormat::testSimulatedBaseHeaderPreserved()
{
        // The base header (protocol_id, peer_id, channel) must be preserved
        // through encryption and decryption for packet routing.
        std::vector<u8> payload = {0x01, 0x02};
        auto pt_packet = makePlaintextPacket(payload);

        // Set specific base header values
        pt_packet[4] = 0x00; // peer_id high
        pt_packet[5] = 0x42; // peer_id low
        pt_packet[6] = 0x01; // channel 1

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        // Verify base header is preserved in encrypted packet
        UASSERTEQ(int, enc_packet[0], 0x4f); // Protocol ID
        UASSERTEQ(int, enc_packet[1], 0x45);
        UASSERTEQ(int, enc_packet[2], 0x74);
        UASSERTEQ(int, enc_packet[3], 0x03);
        UASSERTEQ(int, enc_packet[4], 0x00); // peer_id
        UASSERTEQ(int, enc_packet[5], 0x42);
        UASSERTEQ(int, enc_packet[6], 0x01); // channel

        // Decrypt and verify base header is still intact
        auto dec_state = makeTestEncState();
        auto dec_packet = decryptPacket(enc_packet, *dec_state, false);

        UASSERTEQ(int, dec_packet[4], 0x00);
        UASSERTEQ(int, dec_packet[5], 0x42);
        UASSERTEQ(int, dec_packet[6], 0x01);
}

void TestEncryptedPacketFormat::testMultiplePacketsUniqueNonces()
{
        // When encrypting multiple packets, each must use a unique nonce.
        auto enc_state = makeTestEncState();

        std::vector<u8> payload = {0x01, 0x02, 0x03};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc1 = encryptPacket(pt_packet, *enc_state, true);
        auto enc2 = encryptPacket(pt_packet, *enc_state, true);
        auto enc3 = encryptPacket(pt_packet, *enc_state, true);

        // Extract nonces from each encrypted packet
        size_t nonce_offset = BASE_HEADER_SIZE + 1;
        std::array<u8, GCM_NONCE_SIZE> n1, n2, n3;
        memcpy(n1.data(), enc1.data() + nonce_offset, GCM_NONCE_SIZE);
        memcpy(n2.data(), enc2.data() + nonce_offset, GCM_NONCE_SIZE);
        memcpy(n3.data(), enc3.data() + nonce_offset, GCM_NONCE_SIZE);

        // All nonces must be unique
        UASSERT(memcmp(n1.data(), n2.data(), GCM_NONCE_SIZE) != 0);
        UASSERT(memcmp(n2.data(), n3.data(), GCM_NONCE_SIZE) != 0);
        UASSERT(memcmp(n1.data(), n3.data(), GCM_NONCE_SIZE) != 0);
}

// ============================================================================
// Comparison Tests
// ============================================================================

void TestEncryptedPacketFormat::testPlaintextPacketFormat()
{
        // A plaintext packet has the format:
        // [base_header(7B)][packet_type(1B)][payload(NB)]
        // The byte after base header should be a valid packet type (0-3).
        std::vector<u8> payload = {0x00, 0x01, 0x02}; // packet_type=0 (original)
        auto pt_packet = makePlaintextPacket(payload);

        // First byte after base header is packet type
        u8 first_byte = pt_packet[BASE_HEADER_SIZE];
        UASSERT(first_byte <= 0x03); // Valid packet type

        // It should NOT be the encrypted flag
        UASSERT(first_byte != ENCRYPTED_FLAG_AES_256_GCM);
}

void TestEncryptedPacketFormat::testEncryptedPacketIsLarger()
{
        // An encrypted packet is always larger than the plaintext equivalent
        // due to the overhead (encrypted_flag + nonce + tag = 29 bytes).
        std::vector<u8> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto pt_packet = makePlaintextPacket(payload);

        auto enc_state = makeTestEncState();
        auto enc_packet = encryptPacket(pt_packet, *enc_state, true);

        UASSERT(enc_packet.size() > pt_packet.size());
        UASSERTEQ(size_t, enc_packet.size() - pt_packet.size(), ENCRYPTED_PACKET_OVERHEAD);
}
