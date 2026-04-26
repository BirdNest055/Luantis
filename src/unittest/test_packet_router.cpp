// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for the flag-routed packet system (v9.17).
//
// The 0x80 encrypted flag byte is a ROUTING DECISION, not a condition
// to check mid-function. This test suite validates:
//
//   1. PacketRouter: routePacket(), parsePlaintext(), parseEncrypted(),
//      buildEncryptedPacket()
//   2. CryptoHandler: decrypt(), encrypt(), isEncryptionActive()
//   3. Full round-trip: encrypt → build → route → parse → decrypt
//   4. Regression tests for the v9.15 bug class (GCM auth spam,
//      grace period bandaids, transition errors)

#include "test.h"

#include "network/packet_router.h"
#include "network/crypto_handler.h"
#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>

class TestPacketRouter : public TestBase
{
public:
	TestPacketRouter() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPacketRouter"; }

	void runTests(IGameDef *gamedef);

	// === PacketRouter tests ===

	// 1-5: Plaintext packet types route correctly
	void testPlaintextControlRoutesToPlaintext();
	void testPlaintextOriginalRoutesToPlaintext();
	void testPlaintextSplitRoutesToPlaintext();
	void testPlaintextReliableRoutesToPlaintext();

	// 5: Encrypted packet routes correctly
	void testEncryptedFlagRoutesToEncrypted();

	// 6-8: Invalid packets
	void testPacketTooShortForFlagRoutesToInvalid();
	void testEncryptedFlagTooShortRoutesToInvalid();
	void testUnknownFlagByteRoutesToInvalid();

	// 9-10: Boundary cases
	void testMinimumValidEncryptedRoutesToEncrypted();
	void testEmptyPacketRoutesToInvalid();

	// === Plaintext parsing tests ===

	// 11-12
	void testParsePlaintextPreservesDataAfterBaseHeader();
	void testParsePlaintextMinimumSize();

	// === Encrypted parsing tests ===

	// 13-19
	void testParseEncryptedExtractsNonce();
	void testParseEncryptedExtractsNonceCounter();
	void testParseEncryptedExtractsCiphertext();
	void testParseEncryptedExtractsTag();
	void testParseEncryptedReturnsNulloptForTruncated();
	void testParseEncryptedEmptyCiphertext();
	void testParseEncryptedOneByteCiphertext();

	// === buildEncryptedPacket tests ===

	// 20-22
	void testBuildEncryptedPacketCorrectWireFormat();
	void testBuildEncryptedPacketRoundtrip();
	void testBuildEncryptedPacketSize();

	// === Full round-trip tests ===

	// 23-26
	void testFullEncryptBuildRouteParseDecryptServerKey();
	void testFullEncryptBuildRouteParseDecryptClientKey();
	void testPlaintextPacketRoutesToPlaintextNoDecrypt();
	void testDecryptWithWrongKeyReturnsAuthFailed();

	// === Edge case tests ===

	// 27-30
	void testMultipleEncryptedPacketsDifferentCounters();
	void testByte0x80InBodyNotTriggerRouting();
	void testIsEncryptionActiveFalseBeforeActivation();
	void testIsEncryptionActiveTrueAfterActivation();

	// === Regression tests (v9.15 bug class) ===

	// 31-33
	void testPlaintextAfterActivationRoutesToPlaintext();
	void testMixedPlaintextEncryptedSequence();
	void test1000PlaintextAfterActivation();
};

static TestPacketRouter g_test_instance;

void TestPacketRouter::runTests(IGameDef *gamedef)
{
	TEST(testPlaintextControlRoutesToPlaintext);
	TEST(testPlaintextOriginalRoutesToPlaintext);
	TEST(testPlaintextSplitRoutesToPlaintext);
	TEST(testPlaintextReliableRoutesToPlaintext);
	TEST(testEncryptedFlagRoutesToEncrypted);
	TEST(testPacketTooShortForFlagRoutesToInvalid);
	TEST(testEncryptedFlagTooShortRoutesToInvalid);
	TEST(testUnknownFlagByteRoutesToInvalid);
	TEST(testMinimumValidEncryptedRoutesToEncrypted);
	TEST(testEmptyPacketRoutesToInvalid);

	TEST(testParsePlaintextPreservesDataAfterBaseHeader);
	TEST(testParsePlaintextMinimumSize);

	TEST(testParseEncryptedExtractsNonce);
	TEST(testParseEncryptedExtractsNonceCounter);
	TEST(testParseEncryptedExtractsCiphertext);
	TEST(testParseEncryptedExtractsTag);
	TEST(testParseEncryptedReturnsNulloptForTruncated);
	TEST(testParseEncryptedEmptyCiphertext);
	TEST(testParseEncryptedOneByteCiphertext);

	TEST(testBuildEncryptedPacketCorrectWireFormat);
	TEST(testBuildEncryptedPacketRoundtrip);
	TEST(testBuildEncryptedPacketSize);

	TEST(testFullEncryptBuildRouteParseDecryptServerKey);
	TEST(testFullEncryptBuildRouteParseDecryptClientKey);
	TEST(testPlaintextPacketRoutesToPlaintextNoDecrypt);
	TEST(testDecryptWithWrongKeyReturnsAuthFailed);

	TEST(testMultipleEncryptedPacketsDifferentCounters);
	TEST(testByte0x80InBodyNotTriggerRouting);
	TEST(testIsEncryptionActiveFalseBeforeActivation);
	TEST(testIsEncryptionActiveTrueAfterActivation);

	TEST(testPlaintextAfterActivationRoutesToPlaintext);
	TEST(testMixedPlaintextEncryptedSequence);
	TEST(test1000PlaintextAfterActivation);
}

// ============================================================================
// Helpers
// ============================================================================

/// Build a raw plaintext packet for testing.
/// Format: [base_header(7B)][packet_type][payload...]
static std::vector<u8> buildPlaintextPacket(u8 packet_type, size_t payload_size)
{
	size_t total_size = PACKET_BASE_HEADER_SIZE + 1 + payload_size;
	std::vector<u8> packet(total_size, 0xBB);
	// Base header (zeros)
	for (size_t i = 0; i < PACKET_BASE_HEADER_SIZE; i++)
		packet[i] = 0x00;
	// Packet type byte at position BASE_HEADER_SIZE
	packet[PACKET_BASE_HEADER_SIZE] = packet_type;
	return packet;
}

/// Build a raw encrypted packet for testing (with known values).
/// Format: [base_header(7B)][0x80][nonce(12B)][ciphertext(NB)][tag(16B)]
static std::vector<u8> buildEncryptedPacket(size_t ciphertext_size)
{
	size_t total_size = PACKET_BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + ciphertext_size;
	std::vector<u8> packet(total_size, 0xAA);

	// Base header: zeros
	for (size_t i = 0; i < PACKET_BASE_HEADER_SIZE; i++)
		packet[i] = 0x00;

	// Encrypted flag
	packet[PACKET_BASE_HEADER_SIZE] = 0x80;

	// Nonce (12 bytes)
	for (size_t i = 0; i < GCM_NONCE_SIZE; i++)
		packet[PACKET_BASE_HEADER_SIZE + 1 + i] = static_cast<u8>(i);

	// Ciphertext
	for (size_t i = 0; i < ciphertext_size; i++)
		packet[PACKET_BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE + i] = static_cast<u8>(i * 3);

	// GCM tag (last 16 bytes)
	for (size_t i = 0; i < GCM_TAG_SIZE; i++)
		packet[total_size - GCM_TAG_SIZE + i] = static_cast<u8>(i * 5);

	return packet;
}

/// Generate a test SRP session key
static std::array<u8, SRP_SESSION_KEY_SIZE> makeTestSessionKey()
{
	std::array<u8, SRP_SESSION_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = static_cast<u8>(i * 7 + 42);
	return key;
}

// ============================================================================
// Tests 1-5: Plaintext packet types route correctly
// ============================================================================

void TestPacketRouter::testPlaintextControlRoutesToPlaintext()
{
	auto packet = buildPlaintextPacket(0x00, 50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);
}

void TestPacketRouter::testPlaintextOriginalRoutesToPlaintext()
{
	auto packet = buildPlaintextPacket(0x01, 50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);
}

void TestPacketRouter::testPlaintextSplitRoutesToPlaintext()
{
	auto packet = buildPlaintextPacket(0x02, 50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);
}

void TestPacketRouter::testPlaintextReliableRoutesToPlaintext()
{
	auto packet = buildPlaintextPacket(0x03, 50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);
}

void TestPacketRouter::testEncryptedFlagRoutesToEncrypted()
{
	auto packet = buildEncryptedPacket(50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Encrypted);
}

// ============================================================================
// Tests 6-8: Invalid packets
// ============================================================================

void TestPacketRouter::testPacketTooShortForFlagRoutesToInvalid()
{
	// Packet of exactly BASE_HEADER_SIZE bytes — no room for flag byte
	std::vector<u8> packet(PACKET_BASE_HEADER_SIZE, 0x00);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Invalid);
}

void TestPacketRouter::testEncryptedFlagTooShortRoutesToInvalid()
{
	// Packet with 0x80 flag but too short for encrypted format
	// (less than 29 bytes after the header)
	std::vector<u8> packet(PACKET_BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD - 1, 0x00);
	packet[PACKET_BASE_HEADER_SIZE] = 0x80;
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Invalid);
}

void TestPacketRouter::testUnknownFlagByteRoutesToInvalid()
{
	// Flag byte 0xFF is not a valid packet type (0x00-0x03) nor 0x80
	std::vector<u8> packet(100, 0x00);
	packet[PACKET_BASE_HEADER_SIZE] = 0xFF;
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Invalid);
}

// ============================================================================
// Tests 9-10: Boundary cases
// ============================================================================

void TestPacketRouter::testMinimumValidEncryptedRoutesToEncrypted()
{
	// Minimum valid encrypted packet: header(7) + flag(1) + nonce(12) + tag(16) = 36
	// (empty ciphertext — GCM allows this)
	auto packet = buildEncryptedPacket(0);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Encrypted);
	UASSERTEQ(size_t, packet.size(), PACKET_BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD);
}

void TestPacketRouter::testEmptyPacketRoutesToInvalid()
{
	std::vector<u8> packet;
	UASSERTEQ(int, (int)routePacket(packet.data(), 0),
		(int)PacketRoute::Invalid);
}

// ============================================================================
// Tests 11-12: Plaintext parsing
// ============================================================================

void TestPacketRouter::testParsePlaintextPreservesDataAfterBaseHeader()
{
	// Build packet with known payload after base header
	std::vector<u8> payload = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	size_t total_size = PACKET_BASE_HEADER_SIZE + payload.size();
	std::vector<u8> packet(total_size);
	for (size_t i = 0; i < PACKET_BASE_HEADER_SIZE; i++)
		packet[i] = 0x00;
	memcpy(packet.data() + PACKET_BASE_HEADER_SIZE, payload.data(), payload.size());

	auto result = parsePlaintext(packet.data(), packet.size());
	UASSERTEQ(size_t, result.data.getSize(), payload.size());
	UASSERT(memcmp(*result.data, payload.data(), payload.size()) == 0);
}

void TestPacketRouter::testParsePlaintextMinimumSize()
{
	// Minimum plaintext: base_header(7) + packet_type(1) = 8 bytes
	auto packet = buildPlaintextPacket(0x01, 0);
	// packet_type is at BASE_HEADER_SIZE, and nothing follows
	auto result = parsePlaintext(packet.data(), packet.size());
	UASSERTEQ(size_t, result.data.getSize(), 1u);  // just the packet_type byte
	UASSERTEQ(int, result.data[0], 0x01);
}

// ============================================================================
// Tests 13-19: Encrypted parsing
// ============================================================================

void TestPacketRouter::testParseEncryptedExtractsNonce()
{
	auto packet = buildEncryptedPacket(50);
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());

	// Verify nonce matches what we wrote
	for (size_t i = 0; i < GCM_NONCE_SIZE; i++) {
		UASSERTEQ(int, result->nonce[i], (int)i);
	}
}

void TestPacketRouter::testParseEncryptedExtractsNonceCounter()
{
	// Build packet with specific nonce that has a known counter
	size_t total_size = PACKET_BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + 10;
	std::vector<u8> packet(total_size, 0x00);
	packet[PACKET_BASE_HEADER_SIZE] = 0x80;

	// Set nonce bytes 4-11 to a known counter value: 0x0102030405060708
	// Nonce starts at BASE_HEADER_SIZE + 1
	u8* nonce_start = packet.data() + PACKET_BASE_HEADER_SIZE + 1;
	for (int i = 0; i < 4; i++)
		nonce_start[i] = 0x00;  // base (first 4 bytes)
	nonce_start[4] = 0x01;
	nonce_start[5] = 0x02;
	nonce_start[6] = 0x03;
	nonce_start[7] = 0x04;
	nonce_start[8] = 0x05;
	nonce_start[9] = 0x06;
	nonce_start[10] = 0x07;
	nonce_start[11] = 0x08;

	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());
	UASSERTEQ(u64, result->nonce_counter, 0x0102030405060708ULL);
}

void TestPacketRouter::testParseEncryptedExtractsCiphertext()
{
	auto packet = buildEncryptedPacket(10);
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());

	// Verify ciphertext matches what we wrote
	UASSERTEQ(size_t, result->ciphertext.size(), 10u);
	for (size_t i = 0; i < 10; i++) {
		UASSERTEQ(int, result->ciphertext[i], (int)(i * 3));
	}
}

void TestPacketRouter::testParseEncryptedExtractsTag()
{
	auto packet = buildEncryptedPacket(10);
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());

	// Verify tag matches what we wrote (last 16 bytes)
	for (size_t i = 0; i < GCM_TAG_SIZE; i++) {
		UASSERTEQ(int, result->tag[i], (int)(i * 5));
	}
}

void TestPacketRouter::testParseEncryptedReturnsNulloptForTruncated()
{
	// Packet too short for encrypted format
	std::vector<u8> packet(PACKET_BASE_HEADER_SIZE + 10, 0x00);
	packet[PACKET_BASE_HEADER_SIZE] = 0x80;
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(!result.has_value());
}

void TestPacketRouter::testParseEncryptedEmptyCiphertext()
{
	// Minimum encrypted packet (no ciphertext)
	auto packet = buildEncryptedPacket(0);
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());
	UASSERTEQ(size_t, result->ciphertext.size(), 0u);
}

void TestPacketRouter::testParseEncryptedOneByteCiphertext()
{
	auto packet = buildEncryptedPacket(1);
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());
	UASSERTEQ(size_t, result->ciphertext.size(), 1u);
}

// ============================================================================
// Tests 20-22: buildEncryptedPacket
// ============================================================================

void TestPacketRouter::testBuildEncryptedPacketCorrectWireFormat()
{
	// Build with known values and verify the wire format
	u8 base_header[PACKET_BASE_HEADER_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
	std::array<u8, 12> nonce;
	for (size_t i = 0; i < 12; i++) nonce[i] = static_cast<u8>(i + 10);
	std::vector<u8> ciphertext = {0xAA, 0xBB, 0xCC, 0xDD};
	std::array<u8, 16> tag;
	for (size_t i = 0; i < 16; i++) tag[i] = static_cast<u8>(i + 20);

	auto packet = buildEncryptedPacket(base_header, nonce, ciphertext, tag);

	// Verify base header
	UASSERT(memcmp(packet.data(), base_header, PACKET_BASE_HEADER_SIZE) == 0);
	// Verify encrypted flag
	UASSERTEQ(int, packet[PACKET_BASE_HEADER_SIZE], 0x80);
	// Verify nonce
	UASSERT(memcmp(packet.data() + PACKET_BASE_HEADER_SIZE + 1,
		nonce.data(), 12) == 0);
	// Verify ciphertext
	UASSERT(memcmp(packet.data() + PACKET_BASE_HEADER_SIZE + 1 + 12,
		ciphertext.data(), 4) == 0);
	// Verify tag
	UASSERT(memcmp(packet.data() + packet.size() - 16,
		tag.data(), 16) == 0);
}

void TestPacketRouter::testBuildEncryptedPacketRoundtrip()
{
	// Build → parse should produce the same data
	u8 base_header[PACKET_BASE_HEADER_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	std::array<u8, 12> nonce;
	for (size_t i = 0; i < 12; i++) nonce[i] = static_cast<u8>(i);
	std::vector<u8> ciphertext = {0x11, 0x22, 0x33};
	std::array<u8, 16> tag;
	for (size_t i = 0; i < 16; i++) tag[i] = static_cast<u8>(i * 7);

	auto packet = buildEncryptedPacket(base_header, nonce, ciphertext, tag);

	// Route should say Encrypted
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Encrypted);

	// Parse should recover all fields
	auto result = parseEncrypted(packet.data(), packet.size());
	UASSERT(result.has_value());
	UASSERT(memcmp(result->nonce.data(), nonce.data(), 12) == 0);
	UASSERTEQ(size_t, result->ciphertext.size(), ciphertext.size());
	UASSERT(memcmp(result->ciphertext.data(), ciphertext.data(), ciphertext.size()) == 0);
	UASSERT(memcmp(result->tag.data(), tag.data(), 16) == 0);
}

void TestPacketRouter::testBuildEncryptedPacketSize()
{
	u8 base_header[PACKET_BASE_HEADER_SIZE] = {};
	std::array<u8, 12> nonce = {};
	std::vector<u8> ciphertext(100, 0x42);
	std::array<u8, 16> tag = {};

	auto packet = buildEncryptedPacket(base_header, nonce, ciphertext, tag);
	// Size = BASE_HEADER(7) + flag(1) + nonce(12) + ciphertext(100) + tag(16) = 136
	UASSERTEQ(size_t, packet.size(),
		PACKET_BASE_HEADER_SIZE + 1 + 12 + 100 + 16);
}

// ============================================================================
// Tests 23-26: Full round-trip with real crypto
// ============================================================================

void TestPacketRouter::testFullEncryptBuildRouteParseDecryptServerKey()
{
	// Server encrypts with S2C key → client decrypts with S2C key
	auto key = makeTestSessionKey();
	PeerEncryptionState server_state;
	server_state.initFromSRPSessionKey(key.data(), key.size(), true); // is_server=true
	server_state.activate();

	PeerEncryptionState client_state;
	client_state.initFromSRPSessionKey(key.data(), key.size(), false); // is_server=false

	// Plaintext to encrypt (everything after base header)
	std::vector<u8> original_plaintext = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	// Server encrypts (S2C direction)
	auto enc_result = CryptoHandler::encrypt(
		original_plaintext.data(), original_plaintext.size(),
		server_state, true); // we_are_server=true → uses S2C key

	UASSERTEQ(int, (int)enc_result.status, (int)EncryptResult::Success);

	// Build the encrypted packet
	u8 base_header[PACKET_BASE_HEADER_SIZE] = {};
	auto packet = buildEncryptedPacket(
		base_header, enc_result.nonce, enc_result.ciphertext, enc_result.tag);

	// Route it
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Encrypted);

	// Parse it
	auto parsed = parseEncrypted(packet.data(), packet.size());
	UASSERT(parsed.has_value());

	// Client decrypts (we_are_server=false → uses S2C key)
	auto dec_result = CryptoHandler::decrypt(
		*parsed, client_state, false); // we_are_server=false

	UASSERTEQ(int, (int)dec_result.status, (int)DecryptResult::Success);
	UASSERTEQ(size_t, dec_result.plaintext.getSize(), original_plaintext.size());
	UASSERT(memcmp(*dec_result.plaintext, original_plaintext.data(),
		original_plaintext.size()) == 0);
}

void TestPacketRouter::testFullEncryptBuildRouteParseDecryptClientKey()
{
	// Client encrypts with C2S key → server decrypts with C2S key
	auto key = makeTestSessionKey();
	PeerEncryptionState client_state;
	client_state.initFromSRPSessionKey(key.data(), key.size(), false); // is_server=false
	client_state.activate();

	PeerEncryptionState server_state;
	server_state.initFromSRPSessionKey(key.data(), key.size(), true); // is_server=true

	// Plaintext to encrypt
	std::vector<u8> original_plaintext = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

	// Client encrypts (C2S direction)
	auto enc_result = CryptoHandler::encrypt(
		original_plaintext.data(), original_plaintext.size(),
		client_state, false); // we_are_server=false → uses C2S key

	UASSERTEQ(int, (int)enc_result.status, (int)EncryptResult::Success);

	// Build the encrypted packet
	u8 base_header[PACKET_BASE_HEADER_SIZE] = {};
	auto packet = buildEncryptedPacket(
		base_header, enc_result.nonce, enc_result.ciphertext, enc_result.tag);

	// Route it
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Encrypted);

	// Parse it
	auto parsed = parseEncrypted(packet.data(), packet.size());
	UASSERT(parsed.has_value());

	// Server decrypts (we_are_server=true → uses C2S key)
	auto dec_result = CryptoHandler::decrypt(
		*parsed, server_state, true); // we_are_server=true

	UASSERTEQ(int, (int)dec_result.status, (int)DecryptResult::Success);
	UASSERTEQ(size_t, dec_result.plaintext.getSize(), original_plaintext.size());
	UASSERT(memcmp(*dec_result.plaintext, original_plaintext.data(),
		original_plaintext.size()) == 0);
}

void TestPacketRouter::testPlaintextPacketRoutesToPlaintextNoDecrypt()
{
	// A plaintext packet should route to Plaintext, and no decrypt should be attempted
	auto packet = buildPlaintextPacket(0x01, 50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);

	// Parse as plaintext — just get the data
	auto result = parsePlaintext(packet.data(), packet.size());
	UASSERTEQ(size_t, result.data.getSize(), 51u);  // 1 type byte + 50 payload
}

void TestPacketRouter::testDecryptWithWrongKeyReturnsAuthFailed()
{
	// Encrypt with one key, decrypt with a different key → AuthFailed
	auto key1 = makeTestSessionKey();
	std::array<u8, SRP_SESSION_KEY_SIZE> key2;
	for (size_t i = 0; i < key2.size(); i++)
		key2[i] = static_cast<u8>(i * 11 + 99);

	PeerEncryptionState server_state;
	server_state.initFromSRPSessionKey(key1.data(), key1.size(), true);
	server_state.activate();

	PeerEncryptionState wrong_client_state;
	wrong_client_state.initFromSRPSessionKey(key2.data(), key2.size(), false);

	// Server encrypts with key1
	std::vector<u8> plaintext = {0x01, 0x02, 0x03, 0x04};
	auto enc_result = CryptoHandler::encrypt(
		plaintext.data(), plaintext.size(), server_state, true);
	UASSERTEQ(int, (int)enc_result.status, (int)EncryptResult::Success);

	// Build and parse the packet
	u8 base_header[PACKET_BASE_HEADER_SIZE] = {};
	auto packet = buildEncryptedPacket(
		base_header, enc_result.nonce, enc_result.ciphertext, enc_result.tag);
	auto parsed = parseEncrypted(packet.data(), packet.size());
	UASSERT(parsed.has_value());

	// Try to decrypt with wrong key — should get AuthFailed, NOT a crash
	auto dec_result = CryptoHandler::decrypt(
		*parsed, wrong_client_state, false);
	UASSERTEQ(int, (int)dec_result.status, (int)DecryptResult::AuthFailed);
	// NOT Success, NOT a crash — this is the key regression test for GCM auth spam
}

// ============================================================================
// Tests 27-30: Edge cases
// ============================================================================

void TestPacketRouter::testMultipleEncryptedPacketsDifferentCounters()
{
	// Build multiple encrypted packets with different nonce counters
	// and verify they all route to Encrypted
	for (int i = 0; i < 5; i++) {
		auto packet = buildEncryptedPacket(50);
		// Set different nonce counter values
		u8* nonce_start = packet.data() + PACKET_BASE_HEADER_SIZE + 1;
		// Set bytes 4-11 to represent counter = i+1
		u64 counter = static_cast<u64>(i + 1);
		for (int j = 7; j >= 0; j--) {
			nonce_start[NONCE_BASE_SIZE + j] = static_cast<u8>(counter & 0xFF);
			counter >>= 8;
		}

		UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
			(int)PacketRoute::Encrypted);

		auto parsed = parseEncrypted(packet.data(), packet.size());
		UASSERT(parsed.has_value());
		UASSERTEQ(u64, parsed->nonce_counter, static_cast<u64>(i + 1));
	}
}

void TestPacketRouter::testByte0x80InBodyNotTriggerRouting()
{
	// A 0x80 byte in the packet body should NOT trigger encrypted routing.
	// Only the byte at position BASE_HEADER_SIZE matters.
	auto packet = buildPlaintextPacket(0x01, 100);
	packet[50] = 0x80;  // 0x80 in the body — should not matter
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);
}

void TestPacketRouter::testIsEncryptionActiveFalseBeforeActivation()
{
	PeerEncryptionState state;
	auto key = makeTestSessionKey();
	state.initFromSRPSessionKey(key.data(), key.size(), false);
	UASSERT(!CryptoHandler::isEncryptionActive(state));
}

void TestPacketRouter::testIsEncryptionActiveTrueAfterActivation()
{
	PeerEncryptionState state;
	auto key = makeTestSessionKey();
	state.initFromSRPSessionKey(key.data(), key.size(), false);
	state.activate();
	UASSERT(CryptoHandler::isEncryptionActive(state));
}

// ============================================================================
// Tests 31-33: Regression tests (v9.15 bug class)
//
// These tests verify that the flag-routed packet system correctly handles
// the scenarios that caused bugs in v9.15:
//
//   - Plaintext packets received AFTER encryption activates should be
//     routed to Plaintext (NOT trigger GCM decryption attempt)
//   - Mixed plaintext/encrypted sequences should route independently
//   - 1000 plaintext packets after activation → zero decrypt attempts
// ============================================================================

void TestPacketRouter::testPlaintextAfterActivationRoutesToPlaintext()
{
	// After encryption is active, a plaintext packet (no 0x80 flag)
	// should STILL route to Plaintext. This is the fundamental fix
	// that eliminates grace periods — the flag is authoritative.
	PeerEncryptionState state;
	auto key = makeTestSessionKey();
	state.initFromSRPSessionKey(key.data(), key.size(), false);
	state.activate();
	UASSERT(state.active.load());

	auto packet = buildPlaintextPacket(0x01, 50);
	UASSERTEQ(int, (int)routePacket(packet.data(), packet.size()),
		(int)PacketRoute::Plaintext);
	// NOT Encrypted — no GCM decrypt attempt will be made
}

void TestPacketRouter::testMixedPlaintextEncryptedSequence()
{
	// A mix of plaintext and encrypted packets should each route
	// independently based on their own flag byte.
	PeerEncryptionState state;
	auto key = makeTestSessionKey();
	state.initFromSRPSessionKey(key.data(), key.size(), false);
	state.activate();

	// Plaintext packet
	auto plain_pkt = buildPlaintextPacket(0x01, 50);
	UASSERTEQ(int, (int)routePacket(plain_pkt.data(), plain_pkt.size()),
		(int)PacketRoute::Plaintext);

	// Encrypted packet
	auto enc_pkt = buildEncryptedPacket(50);
	UASSERTEQ(int, (int)routePacket(enc_pkt.data(), enc_pkt.size()),
		(int)PacketRoute::Encrypted);

	// Another plaintext
	auto plain_pkt2 = buildPlaintextPacket(0x03, 30);
	UASSERTEQ(int, (int)routePacket(plain_pkt2.data(), plain_pkt2.size()),
		(int)PacketRoute::Plaintext);

	// Another encrypted
	auto enc_pkt2 = buildEncryptedPacket(10);
	UASSERTEQ(int, (int)routePacket(enc_pkt2.data(), enc_pkt2.size()),
		(int)PacketRoute::Encrypted);
}

void TestPacketRouter::test1000PlaintextAfterActivation()
{
	// Simulate receiving 1000 plaintext packets after encryption activation.
	// ALL should route to Plaintext — zero decrypt attempts.
	PeerEncryptionState state;
	auto key = makeTestSessionKey();
	state.initFromSRPSessionKey(key.data(), key.size(), false);
	state.activate();
	state.activated_at = 1;

	int plaintext_count = 0;
	int encrypted_count = 0;

	for (int i = 0; i < 1000; i++) {
		auto packet = buildPlaintextPacket(0x01, 50);
		auto route = routePacket(packet.data(), packet.size());
		if (route == PacketRoute::Plaintext)
			plaintext_count++;
		else if (route == PacketRoute::Encrypted)
			encrypted_count++;
	}

	UASSERTEQ(int, plaintext_count, 1000);
	UASSERTEQ(int, encrypted_count, 0);
}
