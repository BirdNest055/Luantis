// Luanti-Secure
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti-Secure contributors

#include "test.h"

#include "util/keypair.h"
#include "util/auth.h"
#include "util/base64.h"
#include <filesystem>
#include <fstream>

class TestKeypair : public TestBase
{
public:
	TestKeypair() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestKeypair"; }

	void runTests(IGameDef *gamedef) override
	{
		TEST(testKeypairGeneration);
		TEST(testSignVerifyCycle);
		TEST(testKeypairPersistence);
		TEST(testPublicKeyEncoding);
		TEST(testChallengeGeneration);
		TEST(testPerServerUsernameMemory);
		TEST(testSignatureFailsWithWrongKey);
		TEST(testSignatureFailsWithTamperedMessage);
	}

private:
	void testKeypairGeneration()
	{
		// Create a temporary directory for the keypair
		std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_" + std::to_string(::getpid());
		fs::CreateDir(tmpdir);

		KeypairManager km(tmpdir);
		UASSERT(km.ensureKeypair());
		UASSERT(km.hasKeypair());

		std::string pubkey = km.getPublicKey();
		UASSERT(pubkey.size() == ED25519_PUBLIC_KEY_SIZE);

		std::string pubkey_b64 = km.getPublicKeyBase64();
		UASSERT(!pubkey_b64.empty());
		UASSERT(base64_is_valid(pubkey_b64));

		// Clean up
		std::filesystem::remove_all(tmpdir);
	}

	void testSignVerifyCycle()
	{
		std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_sign_" + std::to_string(::getpid());
		fs::CreateDir(tmpdir);

		KeypairManager km(tmpdir);
		UASSERT(km.ensureKeypair());

		std::string message = "Hello, world!";
		std::string signature = km.sign(message);
		UASSERT(signature.size() == ED25519_SIGNATURE_SIZE);

		std::string pubkey = km.getPublicKey();
		UASSERT(KeypairManager::verify(pubkey, message, signature));

		std::filesystem::remove_all(tmpdir);
	}

	void testKeypairPersistence()
	{
		std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_persist_" + std::to_string(::getpid());
		fs::CreateDir(tmpdir);

		// Generate and save
		{
			KeypairManager km(tmpdir);
			UASSERT(km.ensureKeypair());
		}

		// Load from disk
		KeypairManager km2(tmpdir);
		UASSERT(km2.ensureKeypair());
		UASSERT(km2.hasKeypair());

		// Sign with loaded keypair
		std::string message = "Persistence test";
		std::string signature = km2.sign(message);
		UASSERT(!signature.empty());
		UASSERT(KeypairManager::verify(km2.getPublicKey(), message, signature));

		std::filesystem::remove_all(tmpdir);
	}

	void testPublicKeyEncoding()
	{
		// Test the #2# encoding format
		std::string raw_pubkey(ED25519_PUBLIC_KEY_SIZE, 'A');
		std::string encoded = encode_keypair_pubkey(raw_pubkey);

		UASSERT(is_keypair_auth(encoded));
		UASSERT(encoded.substr(0, 3) == "#2#");

		std::string decoded;
		UASSERT(decode_keypair_pubkey(encoded, &decoded));
		UASSERT(decoded == raw_pubkey);

		// Test that SRP encoding is not keypair auth
		std::string srp_encoded = "#1#salt#verifier";
		UASSERT(!is_keypair_auth(srp_encoded));
	}

	void testChallengeGeneration()
	{
		std::string nonce1 = KeypairManager::generateChallenge();
		std::string nonce2 = KeypairManager::generateChallenge();

		UASSERT(nonce1.size() == KEYPAIR_CHALLENGE_SIZE);
		UASSERT(nonce2.size() == KEYPAIR_CHALLENGE_SIZE);
		// Two challenges should be different (cryptographically random)
		UASSERT(nonce1 != nonce2);
	}

	void testPerServerUsernameMemory()
	{
		std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_users_" + std::to_string(::getpid());
		fs::CreateDir(tmpdir);

		KeypairManager km(tmpdir);
		km.ensureKeypair();

		// Remember a username for a server
		km.rememberServerUser("example.com:30000", "player1");

		UASSERT(km.hasServerUser("example.com:30000"));
		UASSERT(km.getServerUser("example.com:30000") == "player1");

		// Different server should have no user yet
		UASSERT(!km.hasServerUser("other.com:30000"));
		UASSERT(km.getServerUser("other.com:30000") == "");

		// Remember another server
		km.rememberServerUser("other.com:30000", "player2");
		UASSERT(km.getServerUser("other.com:30000") == "player2");

		// Test persistence across reloads
		{
			KeypairManager km2(tmpdir);
			km2.ensureKeypair();
			UASSERT(km2.getServerUser("example.com:30000") == "player1");
			UASSERT(km2.getServerUser("other.com:30000") == "player2");
		}

		std::filesystem::remove_all(tmpdir);
	}

	void testSignatureFailsWithWrongKey()
	{
		std::string tmpdir1 = std::filesystem::temp_directory_path().string() + "/keypair_test_wrong1_" + std::to_string(::getpid());
		std::string tmpdir2 = std::filesystem::temp_directory_path().string() + "/keypair_test_wrong2_" + std::to_string(::getpid());
		fs::CreateDir(tmpdir1);
		fs::CreateDir(tmpdir2);

		KeypairManager km1(tmpdir1);
		KeypairManager km2(tmpdir2);
		km1.ensureKeypair();
		km2.ensureKeypair();

		std::string message = "Test message";
		std::string signature = km1.sign(message);

		// Verify with the WRONG public key should fail
		std::string wrong_pubkey = km2.getPublicKey();
		UASSERT(!KeypairManager::verify(wrong_pubkey, message, signature));

		// Verify with the CORRECT public key should succeed
		std::string correct_pubkey = km1.getPublicKey();
		UASSERT(KeypairManager::verify(correct_pubkey, message, signature));

		std::filesystem::remove_all(tmpdir1);
		std::filesystem::remove_all(tmpdir2);
	}

	void testSignatureFailsWithTamperedMessage()
	{
		std::string tmpdir = std::filesystem::temp_directory_path().string() + "/keypair_test_tamper_" + std::to_string(::getpid());
		fs::CreateDir(tmpdir);

		KeypairManager km(tmpdir);
		km.ensureKeypair();

		std::string message = "Original message";
		std::string signature = km.sign(message);

		// Tampered message should fail verification
		std::string tampered = "Tampered message";
		UASSERT(!KeypairManager::verify(km.getPublicKey(), tampered, signature));

		// Original message should still pass
		UASSERT(KeypairManager::verify(km.getPublicKey(), message, signature));

		std::filesystem::remove_all(tmpdir);
	}
};

static TestKeypair g_test_instance;
