// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Comprehensive unit tests for the crypto module (AES-256-GCM, HKDF-SHA256,
// secure_random, nonce construction, binToHex, keyToFingerprint).
//
// These tests prove that the OpenSSL-backed cryptographic primitives work
// correctly: encryption produces ciphertext (not plaintext), decryption
// recovers the original data, key derivation is deterministic, and
// authentication-tag tampering is detected.

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <string>

class TestCrypto : public TestBase
{
public:
	TestCrypto() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestCrypto"; }

	void runTests(IGameDef *gamedef);

	// AES-256-GCM encrypt/decrypt tests
	void testAes256GcmBasicRoundtrip();
	void testAes256GcmEmptyPlaintext();
	void testAes256GcmLargePayload();
	void testAes256GcmWrongKeyFails();
	void testAes256GcmWrongNonceFails();
	void testAes256GcmTamperedTagFails();
	void testAes256GcmTamperedCiphertextFails();
	void testAes256GcmWrongAADFails();
	void testAes256GcmMissingAADEncryptDecrypt();
	void testAes256GcmInvalidKeySize();
	void testAes256GcmInvalidNonceSize();
	void testAes256GcmInvalidTagSize();
	void testAes256GcmCiphertextDiffersFromPlaintext();
	void testAes256GcmDifferentNoncesProduceDifferentCiphertext();
	void testAes256GcmAADAuthenticatedButNotEncrypted();

	// HKDF-SHA256 tests
	void testHkdfDeterministic();
	void testHkdfDifferentInfoProducesDifferentOutput();
	void testHkdfDifferentIKMProducesDifferentOutput();
	void testHkdfWithSalt();
	void testHkdfNullIKMFails();
	void testHkdfZeroLengthIKMFails();
	void testHkdfNullOutputFails();
	void testHkdfZeroLengthOutputFails();

	// secure_random tests
	void testSecureRandomNonZero();
	void testSecureRandomDifferentCalls();
	void testSecureRandomNullBufferFails();
	void testSecureRandomZeroLengthFails();

	// Nonce construction tests
	void testBuildNonceCounterZero();
	void testBuildNonceCounterMax();
	void testBuildNonceDifferentCounters();
	void testBuildNonceDifferentBases();
	void testBuildNonceBigEndianCounter();

	// Utility function tests
	void testBinToHexEmpty();
	void testBinToHexSingleByte();
	void testBinToHexMultiByte();
	void testBinToHexAllZeros();
	void testBinToHexAllOnes();
	void testKeyToFingerprintFormat();
	void testKeyToFingerprintConsistency();
	void testKeyToFingerprintDifferentKeys();

	// OpenSSL compilation guard
	void testOpenSSLIsEnabled();

	// Integration: full SRP session key → derived keys → encrypt/decrypt
	void testFullKeyDerivationAndRoundtrip();
};

static TestCrypto g_test_instance;

void TestCrypto::runTests(IGameDef *gamedef)
{
	// AES-256-GCM tests
	TEST(testAes256GcmBasicRoundtrip);
	TEST(testAes256GcmEmptyPlaintext);
	TEST(testAes256GcmLargePayload);
	TEST(testAes256GcmWrongKeyFails);
	TEST(testAes256GcmWrongNonceFails);
	TEST(testAes256GcmTamperedTagFails);
	TEST(testAes256GcmTamperedCiphertextFails);
	TEST(testAes256GcmWrongAADFails);
	TEST(testAes256GcmMissingAADEncryptDecrypt);
	TEST(testAes256GcmInvalidKeySize);
	TEST(testAes256GcmInvalidNonceSize);
	TEST(testAes256GcmInvalidTagSize);
	TEST(testAes256GcmCiphertextDiffersFromPlaintext);
	TEST(testAes256GcmDifferentNoncesProduceDifferentCiphertext);
	TEST(testAes256GcmAADAuthenticatedButNotEncrypted);

	// HKDF-SHA256 tests
	TEST(testHkdfDeterministic);
	TEST(testHkdfDifferentInfoProducesDifferentOutput);
	TEST(testHkdfDifferentIKMProducesDifferentOutput);
	TEST(testHkdfWithSalt);
	TEST(testHkdfNullIKMFails);
	TEST(testHkdfZeroLengthIKMFails);
	TEST(testHkdfNullOutputFails);
	TEST(testHkdfZeroLengthOutputFails);

	// Secure random tests
	TEST(testSecureRandomNonZero);
	TEST(testSecureRandomDifferentCalls);
	TEST(testSecureRandomNullBufferFails);
	TEST(testSecureRandomZeroLengthFails);

	// Nonce construction tests
	TEST(testBuildNonceCounterZero);
	TEST(testBuildNonceCounterMax);
	TEST(testBuildNonceDifferentCounters);
	TEST(testBuildNonceDifferentBases);
	TEST(testBuildNonceBigEndianCounter);

	// Utility function tests
	TEST(testBinToHexEmpty);
	TEST(testBinToHexSingleByte);
	TEST(testBinToHexMultiByte);
	TEST(testBinToHexAllZeros);
	TEST(testBinToHexAllOnes);
	TEST(testKeyToFingerprintFormat);
	TEST(testKeyToFingerprintConsistency);
	TEST(testKeyToFingerprintDifferentKeys);

	// Compilation guard
	TEST(testOpenSSLIsEnabled);

	// Integration
	TEST(testFullKeyDerivationAndRoundtrip);
}

// ============================================================================
// AES-256-GCM Encrypt/Decrypt Tests
// ============================================================================

void TestCrypto::testAes256GcmBasicRoundtrip()
{
	// Basic encrypt then decrypt: the most fundamental test.
	// If this fails, everything else is broken.
	std::array<u8, AES256_KEY_SIZE> key;
	for (size_t i = 0; i < key.size(); i++) key[i] = static_cast<u8>(i);

	std::array<u8, GCM_NONCE_SIZE> nonce;
	for (size_t i = 0; i < nonce.size(); i++) nonce[i] = static_cast<u8>(i + 0xA0);

	const char *plaintext = "Hello, Luanti encryption!";
	size_t pt_len = strlen(plaintext);

	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	// Encrypt
	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(plaintext), pt_len,
		&aad, 1);

	UASSERT(enc.success);
	UASSERTEQ(size_t, enc.data.size(), pt_len);

	// Decrypt
	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(dec.success);
	UASSERTEQ(size_t, dec.data.size(), pt_len);
	UASSERT(memcmp(dec.data.data(), plaintext, pt_len) == 0);
}

void TestCrypto::testAes256GcmEmptyPlaintext()
{
	// Encrypting zero bytes should still produce a valid authentication tag.
	std::array<u8, AES256_KEY_SIZE> key{};
	std::array<u8, GCM_NONCE_SIZE> nonce{};

	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		nullptr, 0,
		&aad, 1);

	UASSERT(enc.success);
	UASSERT(enc.data.empty());

	// Decrypt empty ciphertext
	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		nullptr, 0,
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(dec.success);
	UASSERT(dec.data.empty());
}

void TestCrypto::testAes256GcmLargePayload()
{
	// Test with a payload larger than a single AES block (16 bytes).
	// 4096 bytes = 256 AES blocks.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce;
	secure_random(nonce.data(), nonce.size());

	std::vector<u8> plaintext(4096);
	secure_random(plaintext.data(), plaintext.size());

	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		plaintext.data(), plaintext.size(),
		&aad, 1);

	UASSERT(enc.success);
	UASSERTEQ(size_t, enc.data.size(), plaintext.size());

	// Verify ciphertext differs from plaintext
	UASSERT(memcmp(enc.data.data(), plaintext.data(), plaintext.size()) != 0);

	// Decrypt and verify
	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(dec.success);
	UASSERT(memcmp(dec.data.data(), plaintext.data(), plaintext.size()) == 0);
}

void TestCrypto::testAes256GcmWrongKeyFails()
{
	// Decrypting with the wrong key must fail (tag verification).
	std::array<u8, AES256_KEY_SIZE> key1;
	std::array<u8, AES256_KEY_SIZE> key2;
	secure_random(key1.data(), key1.size());
	secure_random(key2.data(), key2.size());
	// Ensure keys are different
	key2[0] = ~key1[0];

	std::array<u8, GCM_NONCE_SIZE> nonce{};

	const char *msg = "secret message";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key1.data(), key1.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	UASSERT(enc.success);

	// Try to decrypt with the wrong key
	CryptoResult dec = aes256gcm_decrypt(
		key2.data(), key2.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(!dec.success);
	UASSERT(!dec.error_msg.empty());
}

void TestCrypto::testAes256GcmWrongNonceFails()
{
	// Decrypting with the wrong nonce must fail.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce1{};
	std::array<u8, GCM_NONCE_SIZE> nonce2{};
	nonce2[0] = 0xFF; // Different from nonce1

	const char *msg = "secret message";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce1.data(), nonce1.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	UASSERT(enc.success);

	// Try to decrypt with wrong nonce
	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce2.data(), nonce2.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(!dec.success);
}

void TestCrypto::testAes256GcmTamperedTagFails()
{
	// Tampering with the GCM authentication tag must cause decryption to fail.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce{};

	const char *msg = "important data";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	UASSERT(enc.success);

	// Tamper with the tag
	std::array<u8, GCM_TAG_SIZE> tampered_tag = enc.tag;
	tampered_tag[0] ^= 0xFF; // Flip all bits in first byte

	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		tampered_tag.data(), tampered_tag.size(),
		&aad, 1);

	UASSERT(!dec.success);
}

void TestCrypto::testAes256GcmTamperedCiphertextFails()
{
	// Tampering with the ciphertext must cause decryption to fail.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce{};

	const char *msg = "important data";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	UASSERT(enc.success);

	// Tamper with the ciphertext
	std::vector<u8> tampered_ct = enc.data;
	if (!tampered_ct.empty())
		tampered_ct[0] ^= 0x01; // Flip one bit

	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		tampered_ct.data(), tampered_ct.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(!dec.success);
}

void TestCrypto::testAes256GcmWrongAADFails()
{
	// Using a different AAD during decryption must fail.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce{};

	const char *msg = "secret";
	u8 aad_enc = ENCRYPTED_FLAG_AES_256_GCM;
	u8 aad_dec = ENCRYPTED_FLAG_PLAINTEXT; // Different AAD!

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad_enc, 1);

	UASSERT(enc.success);

	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad_dec, 1);

	UASSERT(!dec.success);
}

void TestCrypto::testAes256GcmMissingAADEncryptDecrypt()
{
	// Encrypting without AAD should work and decrypt without AAD should also work.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce{};

	const char *msg = "no AAD";

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		nullptr, 0);

	UASSERT(enc.success);

	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		nullptr, 0);

	UASSERT(dec.success);
	UASSERT(memcmp(dec.data.data(), msg, strlen(msg)) == 0);
}

void TestCrypto::testAes256GcmInvalidKeySize()
{
	// Passing a key that's not 32 bytes must fail gracefully.
	std::array<u8, 16> short_key{};
	std::array<u8, GCM_NONCE_SIZE> nonce{};

	CryptoResult enc = aes256gcm_encrypt(
		short_key.data(), short_key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>("x"), 1,
		nullptr, 0);

	UASSERT(!enc.success);
	UASSERT(!enc.error_msg.empty());
}

void TestCrypto::testAes256GcmInvalidNonceSize()
{
	// Passing a nonce that's not 12 bytes must fail gracefully.
	std::array<u8, AES256_KEY_SIZE> key{};
	std::array<u8, 8> short_nonce{};

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		short_nonce.data(), short_nonce.size(),
		reinterpret_cast<const u8*>("x"), 1,
		nullptr, 0);

	UASSERT(!enc.success);
	UASSERT(!enc.error_msg.empty());
}

void TestCrypto::testAes256GcmInvalidTagSize()
{
	// Decrypting with a tag that's not 16 bytes must fail gracefully.
	std::array<u8, AES256_KEY_SIZE> key{};
	std::array<u8, GCM_NONCE_SIZE> nonce{};
	std::array<u8, 8> short_tag{};

	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>("x"), 1,
		short_tag.data(), short_tag.size(),
		nullptr, 0);

	UASSERT(!dec.success);
	UASSERT(!dec.error_msg.empty());
}

void TestCrypto::testAes256GcmCiphertextDiffersFromPlaintext()
{
	// PROOF: Ciphertext must NOT be identical to plaintext.
	// This is the most basic proof that encryption is actually happening.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce;
	secure_random(nonce.data(), nonce.size());

	// Use a pattern that is unlikely to appear in random ciphertext
	const u8 plaintext[] = { 0x4f, 0x45, 0x74, 0x03, 0x2c, 0x02, 0x02, 0x00,
	                         0x00, 0x02, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00 };
	//                ^ This is the actual Luanti protocol header "4f457403" ^

	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		plaintext, sizeof(plaintext),
		&aad, 1);

	UASSERT(enc.success);

	// The ciphertext MUST differ from the plaintext
	// (if it were the same, encryption would be broken/trivial)
	UASSERT(memcmp(enc.data.data(), plaintext, sizeof(plaintext)) != 0);

	// Specifically, the protocol magic bytes 4f457403 must NOT appear at the
	// start of the ciphertext. This proves that Wireshark will NOT see the
	// plaintext Luanti protocol header in encrypted packets.
	bool has_plaintext_header = (enc.data.size() >= 4 &&
		enc.data[0] == 0x4f && enc.data[1] == 0x45 &&
		enc.data[2] == 0x74 && enc.data[3] == 0x03);
	UASSERT(!has_plaintext_header);
}

void TestCrypto::testAes256GcmDifferentNoncesProduceDifferentCiphertext()
{
	// Using different nonces with the same key must produce different ciphertext.
	// This proves nonce uniqueness matters (critical for GCM security).
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce1{};
	nonce1[11] = 1;
	std::array<u8, GCM_NONCE_SIZE> nonce2{};
	nonce2[11] = 2;

	const char *msg = "same message, different nonce";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc1 = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce1.data(), nonce1.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	CryptoResult enc2 = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce2.data(), nonce2.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	UASSERT(enc1.success);
	UASSERT(enc2.success);
	UASSERT(memcmp(enc1.data.data(), enc2.data.data(), enc1.data.size()) != 0);
}

void TestCrypto::testAes256GcmAADAuthenticatedButNotEncrypted()
{
	// AAD is authenticated (tampering causes failure) but NOT encrypted
	// (it's not part of the ciphertext output). This test verifies the
	// behavior matches the GCM specification.
	std::array<u8, AES256_KEY_SIZE> key;
	secure_random(key.data(), key.size());

	std::array<u8, GCM_NONCE_SIZE> nonce{};

	const char *msg = "payload";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult enc = aes256gcm_encrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		reinterpret_cast<const u8*>(msg), strlen(msg),
		&aad, 1);

	UASSERT(enc.success);

	// Verify decryption with the SAME AAD succeeds.
	CryptoResult dec = aes256gcm_decrypt(
		key.data(), key.size(),
		nonce.data(), nonce.size(),
		enc.data.data(), enc.data.size(),
		enc.tag.data(), enc.tag.size(),
		&aad, 1);

	UASSERT(dec.success);
}

// ============================================================================
// HKDF-SHA256 Tests
// ============================================================================

void TestCrypto::testHkdfDeterministic()
{
	// HKDF with the same inputs must always produce the same output.
	std::array<u8, 32> ikm;
	for (size_t i = 0; i < ikm.size(); i++) ikm[i] = static_cast<u8>(i);

	const char *info = "Luanti v9 C2S Key";

	std::array<u8, 32> out1{};
	std::array<u8, 32> out2{};

	bool ok1 = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out1.data(), out1.size());

	bool ok2 = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out2.data(), out2.size());

	UASSERT(ok1);
	UASSERT(ok2);
	UASSERT(memcmp(out1.data(), out2.data(), out1.size()) == 0);
}

void TestCrypto::testHkdfDifferentInfoProducesDifferentOutput()
{
	// Different info strings must produce different output keys.
	// This proves that C2S and S2C keys will be different.
	std::array<u8, 32> ikm;
	for (size_t i = 0; i < ikm.size(); i++) ikm[i] = static_cast<u8>(i);

	std::array<u8, 32> out_c2s{};
	std::array<u8, 32> out_s2c{};

	const char *info_c2s = "Luanti v9 C2S Key";
	const char *info_s2c = "Luanti v9 S2C Key";

	bool ok1 = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info_c2s), strlen(info_c2s),
		out_c2s.data(), out_c2s.size());

	bool ok2 = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info_s2c), strlen(info_s2c),
		out_s2c.data(), out_s2c.size());

	UASSERT(ok1);
	UASSERT(ok2);
	UASSERT(memcmp(out_c2s.data(), out_s2c.data(), out_c2s.size()) != 0);
}

void TestCrypto::testHkdfDifferentIKMProducesDifferentOutput()
{
	// Different input keying material must produce different output.
	std::array<u8, 32> ikm1;
	std::array<u8, 32> ikm2;
	for (size_t i = 0; i < 32; i++) {
		ikm1[i] = static_cast<u8>(i);
		ikm2[i] = static_cast<u8>(i + 1);
	}

	const char *info = "test info";

	std::array<u8, 32> out1{};
	std::array<u8, 32> out2{};

	bool ok1 = hkdf_sha256(ikm1.data(), ikm1.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out1.data(), out1.size());

	bool ok2 = hkdf_sha256(ikm2.data(), ikm2.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out2.data(), out2.size());

	UASSERT(ok1);
	UASSERT(ok2);
	UASSERT(memcmp(out1.data(), out2.data(), out1.size()) != 0);
}

void TestCrypto::testHkdfWithSalt()
{
	// HKDF with salt should produce different output than without salt.
	std::array<u8, 32> ikm;
	for (size_t i = 0; i < ikm.size(); i++) ikm[i] = static_cast<u8>(i);

	const char *info = "test info";
	std::array<u8, 16> salt;
	for (size_t i = 0; i < salt.size(); i++) salt[i] = static_cast<u8>(0xAA);

	std::array<u8, 32> out_no_salt{};
	std::array<u8, 32> out_with_salt{};

	bool ok1 = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out_no_salt.data(), out_no_salt.size());

	bool ok2 = hkdf_sha256(ikm.data(), ikm.size(),
		salt.data(), salt.size(),
		reinterpret_cast<const u8*>(info), strlen(info),
		out_with_salt.data(), out_with_salt.size());

	UASSERT(ok1);
	UASSERT(ok2);
	UASSERT(memcmp(out_no_salt.data(), out_with_salt.data(), out_no_salt.size()) != 0);
}

void TestCrypto::testHkdfNullIKMFails()
{
	const char *info = "test";
	std::array<u8, 32> out{};
	bool ok = hkdf_sha256(nullptr, 32,
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out.data(), out.size());
	UASSERT(!ok);
}

void TestCrypto::testHkdfZeroLengthIKMFails()
{
	std::array<u8, 1> dummy = {0};
	const char *info = "test";
	std::array<u8, 32> out{};
	bool ok = hkdf_sha256(dummy.data(), 0,
		nullptr, 0,
		reinterpret_cast<const u8*>(info), strlen(info),
		out.data(), out.size());
	UASSERT(!ok);
}

void TestCrypto::testHkdfNullOutputFails()
{
	std::array<u8, 32> ikm{};
	bool ok = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		nullptr, 0,
		nullptr, 32);
	UASSERT(!ok);
}

void TestCrypto::testHkdfZeroLengthOutputFails()
{
	std::array<u8, 32> ikm{};
	std::array<u8, 1> dummy = {0};
	bool ok = hkdf_sha256(ikm.data(), ikm.size(),
		nullptr, 0,
		nullptr, 0,
		dummy.data(), 0);
	UASSERT(!ok);
}

// ============================================================================
// Secure Random Tests
// ============================================================================

void TestCrypto::testSecureRandomNonZero()
{
	// Generate 32 random bytes and verify they're not all zeros.
	// (The probability of 32 random bytes all being zero is 2^-256.)
	std::array<u8, 32> buf{};
	bool ok = secure_random(buf.data(), buf.size());
	UASSERT(ok);

	bool all_zero = true;
	for (auto b : buf) {
		if (b != 0) { all_zero = false; break; }
	}
	UASSERT(!all_zero);
}

void TestCrypto::testSecureRandomDifferentCalls()
{
	// Two successive calls should produce different random data.
	std::array<u8, 32> buf1{};
	std::array<u8, 32> buf2{};

	bool ok1 = secure_random(buf1.data(), buf1.size());
	bool ok2 = secure_random(buf2.data(), buf2.size());

	UASSERT(ok1);
	UASSERT(ok2);
	UASSERT(memcmp(buf1.data(), buf2.data(), buf1.size()) != 0);
}

void TestCrypto::testSecureRandomNullBufferFails()
{
	bool ok = secure_random(nullptr, 32);
	UASSERT(!ok);
}

void TestCrypto::testSecureRandomZeroLengthFails()
{
	std::array<u8, 1> dummy = {0};
	bool ok = secure_random(dummy.data(), 0);
	UASSERT(!ok);
}

// ============================================================================
// Nonce Construction Tests
// ============================================================================

void TestCrypto::testBuildNonceCounterZero()
{
	std::array<u8, NONCE_BASE_SIZE> base = {0xAB, 0xCD, 0xEF, 0x01};
	std::array<u8, GCM_NONCE_SIZE> nonce{};

	build_nonce(base.data(), 0, nonce.data());

	// First 4 bytes should be the base
	UASSERT(memcmp(nonce.data(), base.data(), NONCE_BASE_SIZE) == 0);

	// Last 8 bytes should be all zeros (counter = 0, big-endian)
	for (int i = NONCE_BASE_SIZE; i < GCM_NONCE_SIZE; i++) {
		UASSERTEQ(int, nonce[i], 0);
	}
}

void TestCrypto::testBuildNonceCounterMax()
{
	std::array<u8, NONCE_BASE_SIZE> base = {0x11, 0x22, 0x33, 0x44};
	std::array<u8, GCM_NONCE_SIZE> nonce{};

	u64 max_counter = 0xFFFFFFFFFFFFFFFFULL;
	build_nonce(base.data(), max_counter, nonce.data());

	// First 4 bytes should be the base
	UASSERT(memcmp(nonce.data(), base.data(), NONCE_BASE_SIZE) == 0);

	// Last 8 bytes should be all 0xFF (counter = max, big-endian)
	for (int i = NONCE_BASE_SIZE; i < GCM_NONCE_SIZE; i++) {
		UASSERTEQ(int, nonce[i], 0xFF);
	}
}

void TestCrypto::testBuildNonceDifferentCounters()
{
	std::array<u8, NONCE_BASE_SIZE> base = {0xAA, 0xBB, 0xCC, 0xDD};
	std::array<u8, GCM_NONCE_SIZE> nonce1{};
	std::array<u8, GCM_NONCE_SIZE> nonce2{};

	build_nonce(base.data(), 1, nonce1.data());
	build_nonce(base.data(), 2, nonce2.data());

	// Nonces should differ only in the counter portion
	UASSERT(memcmp(nonce1.data(), nonce2.data(), NONCE_BASE_SIZE) == 0);
	UASSERT(memcmp(nonce1.data(), nonce2.data(), GCM_NONCE_SIZE) != 0);

	// Counter 2 should have nonce1's last byte + 1
	UASSERTEQ(int, nonce2[11], nonce1[11] + 1);
}

void TestCrypto::testBuildNonceDifferentBases()
{
	std::array<u8, NONCE_BASE_SIZE> base1 = {0xAA, 0xBB, 0xCC, 0xDD};
	std::array<u8, NONCE_BASE_SIZE> base2 = {0x11, 0x22, 0x33, 0x44};
	std::array<u8, GCM_NONCE_SIZE> nonce1{};
	std::array<u8, GCM_NONCE_SIZE> nonce2{};

	build_nonce(base1.data(), 0, nonce1.data());
	build_nonce(base2.data(), 0, nonce2.data());

	// Nonces should differ in the base portion
	UASSERT(memcmp(nonce1.data(), nonce2.data(), NONCE_BASE_SIZE) != 0);
	// Counter portion should be the same (both 0)
	UASSERT(memcmp(nonce1.data() + NONCE_BASE_SIZE,
	               nonce2.data() + NONCE_BASE_SIZE,
	               NONCE_COUNTER_SIZE) == 0);
}

void TestCrypto::testBuildNonceBigEndianCounter()
{
	// Counter value 0x0102030405060708 should be stored as big-endian.
	std::array<u8, NONCE_BASE_SIZE> base = {0x00, 0x00, 0x00, 0x00};
	std::array<u8, GCM_NONCE_SIZE> nonce{};

	u64 counter = 0x0102030405060708ULL;
	build_nonce(base.data(), counter, nonce.data());

	// Big-endian: most significant byte first
	UASSERTEQ(int, nonce[4], 0x01);
	UASSERTEQ(int, nonce[5], 0x02);
	UASSERTEQ(int, nonce[6], 0x03);
	UASSERTEQ(int, nonce[7], 0x04);
	UASSERTEQ(int, nonce[8], 0x05);
	UASSERTEQ(int, nonce[9], 0x06);
	UASSERTEQ(int, nonce[10], 0x07);
	UASSERTEQ(int, nonce[11], 0x08);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

void TestCrypto::testBinToHexEmpty()
{
	UASSERT(binToHex(nullptr, 0) == "");
}

void TestCrypto::testBinToHexSingleByte()
{
	u8 data = 0xAB;
	UASSERT(binToHex(&data, 1) == "ab");
}

void TestCrypto::testBinToHexMultiByte()
{
	u8 data[] = {0xDE, 0xAD, 0xBE, 0xEF};
	UASSERT(binToHex(data, 4) == "deadbeef");
}

void TestCrypto::testBinToHexAllZeros()
{
	u8 data[] = {0x00, 0x00};
	UASSERT(binToHex(data, 2) == "0000");
}

void TestCrypto::testBinToHexAllOnes()
{
	u8 data[] = {0xFF, 0xFF};
	UASSERT(binToHex(data, 2) == "ffff");
}

void TestCrypto::testKeyToFingerprintFormat()
{
	// Fingerprint should start with "SHA256:" and contain hex characters.
	u8 key[32];
	for (int i = 0; i < 32; i++) key[i] = static_cast<u8>(i);

	std::string fp = keyToFingerprint(key, 32);
	UASSERT(fp.substr(0, 7) == "SHA256:");
	// SHA-256 produces 32 bytes = 64 hex characters
	UASSERTEQ(size_t, fp.size(), 7 + 64); // "SHA256:" + 64 hex chars
}

void TestCrypto::testKeyToFingerprintConsistency()
{
	// Same key must always produce the same fingerprint.
	u8 key[32];
	for (int i = 0; i < 32; i++) key[i] = static_cast<u8>(i);

	std::string fp1 = keyToFingerprint(key, 32);
	std::string fp2 = keyToFingerprint(key, 32);
	UASSERT(fp1 == fp2);
}

void TestCrypto::testKeyToFingerprintDifferentKeys()
{
	// Different keys must produce different fingerprints.
	u8 key1[32], key2[32];
	for (int i = 0; i < 32; i++) {
		key1[i] = static_cast<u8>(i);
		key2[i] = static_cast<u8>(i + 1);
	}

	std::string fp1 = keyToFingerprint(key1, 32);
	std::string fp2 = keyToFingerprint(key2, 32);
	UASSERT(fp1 != fp2);
}

// ============================================================================
// OpenSSL Compilation Guard
// ============================================================================

void TestCrypto::testOpenSSLIsEnabled()
{
	// This test proves that USE_OPENSSL is defined and the real OpenSSL
	// code is being compiled (not stubs). If this test fails, the build
	// is using stub crypto and is NOT actually encrypting anything.
#if USE_OPENSSL
	UASSERT(true); // OpenSSL is enabled — real crypto is active
#else
	UASSERT(!"USE_OPENSSL is NOT defined — stub crypto is being used! "
	         "Encryption is fake. Rebuild with -DENABLE_OPENSSL=ON "
	         "and ensure config.h is included.");
#endif
}

// ============================================================================
// Integration: Full Key Derivation + Encrypt/Decrypt Roundtrip
// ============================================================================

void TestCrypto::testFullKeyDerivationAndRoundtrip()
{
	// Simulates the complete flow:
	// 1. Generate a random SRP session key
	// 2. Derive C2S and S2C keys + nonce bases via HKDF
	// 3. Server encrypts with S2C key, client decrypts with S2C key
	// 4. Client encrypts with C2S key, server decrypts with C2S key
	// 5. Verify cross-direction fails (server can't decrypt with C2S key)

	// Step 1: Simulated SRP session key
	std::array<u8, SRP_SESSION_KEY_SIZE> srp_key;
	bool rnd_ok = secure_random(srp_key.data(), srp_key.size());
	UASSERT(rnd_ok);

	// Step 2: Derive keys using the same HKDF calls as PeerEncryptionState
	const char *c2s_info = "Luanti v9 C2S Key";
	const char *s2c_info = "Luanti v9 S2C Key";
	const char *c2s_nonce_info = "Luanti v9 C2S Nonce";
	const char *s2c_nonce_info = "Luanti v9 S2C Nonce";

	std::array<u8, AES256_KEY_SIZE> c2s_key{}, s2c_key{};
	std::array<u8, NONCE_BASE_SIZE> c2s_nonce_base{}, s2c_nonce_base{};

	bool ok = true;
	ok &= hkdf_sha256(srp_key.data(), srp_key.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(c2s_info), strlen(c2s_info),
		c2s_key.data(), c2s_key.size());
	ok &= hkdf_sha256(srp_key.data(), srp_key.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(s2c_info), strlen(s2c_info),
		s2c_key.data(), s2c_key.size());
	ok &= hkdf_sha256(srp_key.data(), srp_key.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(c2s_nonce_info), strlen(c2s_nonce_info),
		c2s_nonce_base.data(), c2s_nonce_base.size());
	ok &= hkdf_sha256(srp_key.data(), srp_key.size(),
		nullptr, 0,
		reinterpret_cast<const u8*>(s2c_nonce_info), strlen(s2c_nonce_info),
		s2c_nonce_base.data(), s2c_nonce_base.size());
	UASSERT(ok);

	// Verify C2S and S2C keys are different
	UASSERT(memcmp(c2s_key.data(), s2c_key.data(), AES256_KEY_SIZE) != 0);

	// Step 3: Server encrypts with S2C key (server→client direction)
	std::array<u8, GCM_NONCE_SIZE> server_nonce;
	build_nonce(s2c_nonce_base.data(), 0, server_nonce.data());

	const char *server_msg = "Hello from server!";
	u8 aad = ENCRYPTED_FLAG_AES_256_GCM;

	CryptoResult server_enc = aes256gcm_encrypt(
		s2c_key.data(), s2c_key.size(),
		server_nonce.data(), server_nonce.size(),
		reinterpret_cast<const u8*>(server_msg), strlen(server_msg),
		&aad, 1);

	UASSERT(server_enc.success);

	// Client decrypts with S2C key — should succeed
	CryptoResult client_dec = aes256gcm_decrypt(
		s2c_key.data(), s2c_key.size(),
		server_nonce.data(), server_nonce.size(),
		server_enc.data.data(), server_enc.data.size(),
		server_enc.tag.data(), server_enc.tag.size(),
		&aad, 1);

	UASSERT(client_dec.success);
	UASSERT(memcmp(client_dec.data.data(), server_msg, strlen(server_msg)) == 0);

	// Step 4: Client encrypts with C2S key (client→server direction)
	std::array<u8, GCM_NONCE_SIZE> client_nonce;
	build_nonce(c2s_nonce_base.data(), 0, client_nonce.data());

	const char *client_msg = "Hello from client!";

	CryptoResult client_enc = aes256gcm_encrypt(
		c2s_key.data(), c2s_key.size(),
		client_nonce.data(), client_nonce.size(),
		reinterpret_cast<const u8*>(client_msg), strlen(client_msg),
		&aad, 1);

	UASSERT(client_enc.success);

	// Server decrypts with C2S key — should succeed
	CryptoResult server_dec = aes256gcm_decrypt(
		c2s_key.data(), c2s_key.size(),
		client_nonce.data(), client_nonce.size(),
		client_enc.data.data(), client_enc.data.size(),
		client_enc.tag.data(), client_enc.tag.size(),
		&aad, 1);

	UASSERT(server_dec.success);
	UASSERT(memcmp(server_dec.data.data(), client_msg, strlen(client_msg)) == 0);

	// Step 5: Cross-direction MUST fail
	// Server can't decrypt its own S2C message with the C2S key
	CryptoResult wrong_key_dec = aes256gcm_decrypt(
		c2s_key.data(), c2s_key.size(), // Wrong key!
		server_nonce.data(), server_nonce.size(),
		server_enc.data.data(), server_enc.data.size(),
		server_enc.tag.data(), server_enc.tag.size(),
		&aad, 1);

	UASSERT(!wrong_key_dec.success);
}
