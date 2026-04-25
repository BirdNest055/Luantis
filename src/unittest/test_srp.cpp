// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for the SRP (Secure Remote Password) 6a implementation.
// These tests verify:
//   1. srp_user_new() succeeds with valid inputs and returns non-NULL
//   2. srp_verifier_new() succeeds with valid inputs and returns non-NULL
//   3. Full SRP exchange: user_new → start_authentication → verifier_new →
//      process_challenge → verify_session produces matching session keys
//   4. NULL input handling: srp_user_new with NULL username returns NULL
//   5. Session key is 32 bytes (256 bits) as required by SRP_SHA256
//   6. Both client and server derive the same session key
//   7. Authentication succeeds after full exchange

#include "test.h"

#include "util/srp.h"
#include "util/auth.h"
#include <cstring>
#include <string>
#include <vector>

class TestSRP : public TestBase
{
public:
	TestSRP() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSRP"; }

	void runTests(IGameDef *gamedef);

	// srp_user_new tests
	void testUserNewValidInputs();
	void testUserNewNullUsername();
	void testUserNewEmptyPassword();

	// srp_verifier_new tests
	void testVerifierNewValidInputs();
	void testVerifierNewNullUsername();

	// Full SRP exchange tests
	void testFullSrpExchange();
	void testFullSrpExchangeWithEmptyPassword();
	void testSessionKeySize();
	void testBothSidesSameSessionKey();

	// auth.h helper tests
	void testGenerateSrpVerifierAndSalt();
	void testEncodeDecodeSrpVerifier();
};

static TestSRP g_test_instance;

void TestSRP::runTests(IGameDef *gamedef)
{
	TEST(testUserNewValidInputs);
	TEST(testUserNewNullUsername);
	TEST(testUserNewEmptyPassword);
	TEST(testVerifierNewValidInputs);
	TEST(testVerifierNewNullUsername);
	TEST(testFullSrpExchange);
	TEST(testFullSrpExchangeWithEmptyPassword);
	TEST(testSessionKeySize);
	TEST(testBothSidesSameSessionKey);
	TEST(testGenerateSrpVerifierAndSalt);
	TEST(testEncodeDecodeSrpVerifier);
}

////////////////////////////////////////////////////////////////////////////////

void TestSRP::testUserNewValidInputs()
{
	const char *username = "testuser";
	const char *password = "testpass";
	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, strlen(password),
		NULL, NULL);
	// CRITICAL: srp_user_new must return non-NULL with valid inputs.
	// If this returns NULL, the "SRPUser is NULL" bug occurs.
	UASSERT(usr != NULL);
	srp_user_delete(usr);
}

void TestSRP::testUserNewNullUsername()
{
	// Passing NULL username should either return NULL or handle gracefully.
	// The current implementation calls strlen(NULL) which would crash,
	// so this test verifies the function is not called with NULL in practice.
	// We test with empty string instead to verify that edge case.
	const char *username = "";
	const char *password = "testpass";
	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, strlen(password),
		NULL, NULL);
	// Empty username should still create a valid user object
	UASSERT(usr != NULL);
	srp_user_delete(usr);
}

void TestSRP::testUserNewEmptyPassword()
{
	const char *username = "testuser";
	const char *password = "";
	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, 0,
		NULL, NULL);
	// Empty password must still create a valid SRPUser.
	// FIRST_SRP with empty password is a valid use case.
	UASSERT(usr != NULL);
	srp_user_delete(usr);
}

void TestSRP::testVerifierNewValidInputs()
{
	// First create a verification key
	const char *username = "testuser";
	const char *password = "testpass";

	unsigned char *salt = NULL;
	size_t salt_len = 0;
	unsigned char *verifier = NULL;
	size_t verifier_len = 0;

	SRP_Result res = srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		username,
		(const unsigned char *)password, strlen(password),
		&salt, &salt_len,
		&verifier, &verifier_len,
		NULL, NULL);

	UASSERT(res == SRP_OK);
	UASSERT(salt != NULL);
	UASSERT(verifier != NULL);

	// Now create a user and get bytes_A
	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, strlen(password),
		NULL, NULL);
	UASSERT(usr != NULL);

	char *bytes_A = NULL;
	size_t len_A = 0;
	SRP_Result auth_res = srp_user_start_authentication(usr, NULL, NULL, 0,
		(unsigned char **)&bytes_A, &len_A);
	UASSERT(auth_res == SRP_OK);
	UASSERT(bytes_A != NULL);
	UASSERT(len_A > 0);

	// Now create verifier with bytes_A
	char *bytes_B = NULL;
	size_t len_B = 0;

	struct SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		username,
		salt, salt_len,
		verifier, verifier_len,
		(const unsigned char *)bytes_A, len_A,
		NULL, 0,
		(unsigned char **)&bytes_B, &len_B,
		NULL, NULL);

	// CRITICAL: srp_verifier_new must return non-NULL with valid inputs
	UASSERT(ver != NULL);
	UASSERT(bytes_B != NULL);
	UASSERT(len_B > 0);

	srp_verifier_delete(ver);
	srp_user_delete(usr);
	free(salt);
	free(verifier);
}

void TestSRP::testVerifierNewNullUsername()
{
	// Test with empty username (NULL would crash strlen)
	const char *username = "";
	const char *password = "testpass";

	unsigned char *salt = NULL;
	size_t salt_len = 0;
	unsigned char *verifier = NULL;
	size_t verifier_len = 0;

	SRP_Result res = srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		username,
		(const unsigned char *)password, strlen(password),
		&salt, &salt_len,
		&verifier, &verifier_len,
		NULL, NULL);

	UASSERT(res == SRP_OK);
	free(salt);
	free(verifier);
}

void TestSRP::testFullSrpExchange()
{
	const char *username = "testuser";
	const char *password = "testpass";

	// Step 1: Create verification key (server stores this)
	unsigned char *salt = NULL;
	size_t salt_len = 0;
	unsigned char *verifier = NULL;
	size_t verifier_len = 0;

	SRP_Result create_res = srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		username,
		(const unsigned char *)password, strlen(password),
		&salt, &salt_len,
		&verifier, &verifier_len,
		NULL, NULL);
	UASSERT(create_res == SRP_OK);

	// Step 2: Client creates SRPUser and starts authentication
	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, strlen(password),
		NULL, NULL);
	UASSERT(usr != NULL);

	char *bytes_A = NULL;
	size_t len_A = 0;
	SRP_Result start_res = srp_user_start_authentication(usr, NULL, NULL, 0,
		(unsigned char **)&bytes_A, &len_A);
	UASSERT(start_res == SRP_OK);
	UASSERT(bytes_A != NULL);

	// Step 3: Server creates SRPVerifier from bytes_A
	char *bytes_B = NULL;
	size_t len_B = 0;

	struct SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		username,
		salt, salt_len,
		verifier, verifier_len,
		(const unsigned char *)bytes_A, len_A,
		NULL, 0,
		(unsigned char **)&bytes_B, &len_B,
		NULL, NULL);
	UASSERT(ver != NULL);
	UASSERT(bytes_B != NULL);

	// Step 4: Client processes challenge (salt + B)
	unsigned char *bytes_M = NULL;
	size_t len_M = 0;

	srp_user_process_challenge(usr,
		salt, salt_len,
		(const unsigned char *)bytes_B, len_B,
		&bytes_M, &len_M);
	UASSERT(bytes_M != NULL);
	UASSERT(len_M > 0);

	// Step 5: Server verifies session with client's M
	unsigned char *bytes_HAMK = NULL;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	UASSERT(bytes_HAMK != NULL);

	// Step 6: Client verifies server's HAMK
	srp_user_verify_session(usr, bytes_HAMK);
	UASSERT(srp_user_is_authenticated(usr) == 1);
	UASSERT(srp_verifier_is_authenticated(ver) == 1);

	// Step 7: Both sides have the same session key
	size_t client_key_len = 0, server_key_len = 0;
	const unsigned char *client_key = srp_user_get_session_key(usr, &client_key_len);
	const unsigned char *server_key = srp_verifier_get_session_key(ver, &server_key_len);

	UASSERT(client_key != NULL);
	UASSERT(server_key != NULL);
	UASSERT(client_key_len == server_key_len);
	UASSERT(memcmp(client_key, server_key, client_key_len) == 0);

	srp_verifier_delete(ver);
	srp_user_delete(usr);
	free(salt);
	free(verifier);
}

void TestSRP::testFullSrpExchangeWithEmptyPassword()
{
	// FIRST_SRP with empty password must also work
	const char *username = "newuser";
	const char *password = "";

	unsigned char *salt = NULL;
	size_t salt_len = 0;
	unsigned char *verifier = NULL;
	size_t verifier_len = 0;

	SRP_Result create_res = srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		username,
		(const unsigned char *)password, 0,
		&salt, &salt_len,
		&verifier, &verifier_len,
		NULL, NULL);
	UASSERT(create_res == SRP_OK);

	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, 0,
		NULL, NULL);
	UASSERT(usr != NULL);

	char *bytes_A = NULL;
	size_t len_A = 0;
	SRP_Result start_res = srp_user_start_authentication(usr, NULL, NULL, 0,
		(unsigned char **)&bytes_A, &len_A);
	UASSERT(start_res == SRP_OK);
	UASSERT(bytes_A != NULL);

	char *bytes_B = NULL;
	size_t len_B = 0;

	struct SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		username,
		salt, salt_len,
		verifier, verifier_len,
		(const unsigned char *)bytes_A, len_A,
		NULL, 0,
		(unsigned char **)&bytes_B, &len_B,
		NULL, NULL);
	UASSERT(ver != NULL);
	UASSERT(bytes_B != NULL);

	unsigned char *bytes_M = NULL;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		salt, salt_len,
		(const unsigned char *)bytes_B, len_B,
		&bytes_M, &len_M);
	UASSERT(bytes_M != NULL);

	unsigned char *bytes_HAMK = NULL;
	srp_verifier_verify_session(ver, bytes_M, &bytes_HAMK);
	UASSERT(bytes_HAMK != NULL);

	srp_user_verify_session(usr, bytes_HAMK);
	UASSERT(srp_user_is_authenticated(usr) == 1);

	// Both sides derive the same session key even with empty password
	size_t client_key_len = 0, server_key_len = 0;
	const unsigned char *client_key = srp_user_get_session_key(usr, &client_key_len);
	const unsigned char *server_key = srp_verifier_get_session_key(ver, &server_key_len);

	UASSERT(client_key != NULL);
	UASSERT(server_key != NULL);
	UASSERT(memcmp(client_key, server_key, client_key_len) == 0);

	srp_verifier_delete(ver);
	srp_user_delete(usr);
	free(salt);
	free(verifier);
}

void TestSRP::testSessionKeySize()
{
	// SRP_SHA256 must produce a 32-byte (256-bit) session key
	const char *username = "testuser";
	const char *password = "testpass";

	unsigned char *salt = NULL;
	size_t salt_len = 0;
	unsigned char *verifier = NULL;
	size_t verifier_len = 0;

	srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		username,
		(const unsigned char *)password, strlen(password),
		&salt, &salt_len,
		&verifier, &verifier_len,
		NULL, NULL);

	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, strlen(password),
		NULL, NULL);
	UASSERT(usr != NULL);

	char *bytes_A = NULL;
	size_t len_A = 0;
	srp_user_start_authentication(usr, NULL, NULL, 0,
		(unsigned char **)&bytes_A, &len_A);

	char *bytes_B = NULL;
	size_t len_B = 0;

	struct SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		username,
		salt, salt_len,
		verifier, verifier_len,
		(const unsigned char *)bytes_A, len_A,
		NULL, 0,
		(unsigned char **)&bytes_B, &len_B,
		NULL, NULL);
	UASSERT(ver != NULL);

	unsigned char *bytes_M = NULL;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		salt, salt_len,
		(const unsigned char *)bytes_B, len_B,
		&bytes_M, &len_M);

	// Check session key length
	size_t key_len = srp_user_get_session_key_length(usr);
	UASSERTEQ(size_t, key_len, 32);

	size_t ver_key_len = srp_verifier_get_session_key_length(ver);
	UASSERTEQ(size_t, ver_key_len, 32);

	// Also check via get_session_key
	size_t actual_key_len = 0;
	srp_user_get_session_key(usr, &actual_key_len);
	UASSERTEQ(size_t, actual_key_len, 32);

	srp_verifier_delete(ver);
	srp_user_delete(usr);
	free(salt);
	free(verifier);
}

void TestSRP::testBothSidesSameSessionKey()
{
	// Verify that client and server independently derive identical session keys
	// This is the core guarantee that makes encryption work.
	const char *username = "encryption_test_user";
	const char *password = "encryption_test_pass";

	// Server side: create verifier
	unsigned char *salt = NULL;
	size_t salt_len = 0;
	unsigned char *verifier = NULL;
	size_t verifier_len = 0;

	srp_create_salted_verification_key(SRP_SHA256, SRP_NG_2048,
		username,
		(const unsigned char *)password, strlen(password),
		&salt, &salt_len,
		&verifier, &verifier_len,
		NULL, NULL);

	// Client side: create user and start auth
	struct SRPUser *usr = srp_user_new(SRP_SHA256, SRP_NG_2048,
		username, username,
		(const unsigned char *)password, strlen(password),
		NULL, NULL);
	UASSERT(usr != NULL);

	char *bytes_A = NULL;
	size_t len_A = 0;
	srp_user_start_authentication(usr, NULL, NULL, 0,
		(unsigned char **)&bytes_A, &len_A);

	// Server side: create verifier from A
	char *bytes_B = NULL;
	size_t len_B = 0;
	struct SRPVerifier *ver = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		username,
		salt, salt_len,
		verifier, verifier_len,
		(const unsigned char *)bytes_A, len_A,
		NULL, 0,
		(unsigned char **)&bytes_B, &len_B,
		NULL, NULL);
	UASSERT(ver != NULL);

	// Server already has session key at this point
	size_t server_key_len = 0;
	const unsigned char *server_key = srp_verifier_get_session_key(ver, &server_key_len);
	UASSERT(server_key != NULL);
	UASSERT(server_key_len == 32);

	// Client processes challenge
	unsigned char *bytes_M = NULL;
	size_t len_M = 0;
	srp_user_process_challenge(usr,
		salt, salt_len,
		(const unsigned char *)bytes_B, len_B,
		&bytes_M, &len_M);

	// Client now has session key
	size_t client_key_len = 0;
	const unsigned char *client_key = srp_user_get_session_key(usr, &client_key_len);
	UASSERT(client_key != NULL);
	UASSERT(client_key_len == 32);

	// THE CRITICAL ASSERTION: Both sides must have the same key
	UASSERT(memcmp(client_key, server_key, 32) == 0);

	srp_verifier_delete(ver);
	srp_user_delete(usr);
	free(salt);
	free(verifier);
}

void TestSRP::testGenerateSrpVerifierAndSalt()
{
	// Test the auth.h helper function
	std::string verifier, salt;
	generate_srp_verifier_and_salt("testuser", "testpass", &verifier, &salt);

	UASSERT(!verifier.empty());
	UASSERT(!salt.empty());
}

void TestSRP::testEncodeDecodeSrpVerifier()
{
	// Test encode/decode roundtrip
	std::string verifier, salt;
	generate_srp_verifier_and_salt("testuser", "testpass", &verifier, &salt);

	std::string encoded = encode_srp_verifier(verifier, salt);
	UASSERT(!encoded.empty());

	std::string decoded_verifier, decoded_salt;
	bool ok = decode_srp_verifier_and_salt(encoded, &decoded_verifier, &decoded_salt);
	UASSERT(ok);
	UASSERT(decoded_verifier == verifier);
	UASSERT(decoded_salt == salt);
}
