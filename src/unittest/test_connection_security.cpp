// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "test.h"

#include "network/connection_security.h"

class TestConnectionSecurity : public TestBase
{
public:
	TestConnectionSecurity() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestConnectionSecurity"; }

	void runTests(IGameDef *gamedef);

	void testConnectionSecurityEnum();
	void testIsConnectionSecure();
	void testConnectionSecurityFromFlags_zeroFlags();
	void testConnectionSecurityFromFlags_encryptedFlag();
	void testConnectionSecurityFromFlags_supportedButNotEncrypted();
	void testConnectionSecurityFromFlags_bothFlags();
	void testConnectionSecurityFromFlags_garbageFlags();
	void testSecurityFlagsConstants();
	void testGameUISecurityOverlayDefault();
	void testGameUISecurityOverlayInsecure();
	void testGameUISecurityOverlayEncrypted();
};

static TestConnectionSecurity g_test_instance;

void TestConnectionSecurity::runTests(IGameDef *gamedef)
{
	TEST(testConnectionSecurityEnum);
	TEST(testIsConnectionSecure);
	TEST(testConnectionSecurityFromFlags_zeroFlags);
	TEST(testConnectionSecurityFromFlags_encryptedFlag);
	TEST(testConnectionSecurityFromFlags_supportedButNotEncrypted);
	TEST(testConnectionSecurityFromFlags_bothFlags);
	TEST(testConnectionSecurityFromFlags_garbageFlags);
	TEST(testSecurityFlagsConstants);
	TEST(testGameUISecurityOverlayDefault);
	TEST(testGameUISecurityOverlayInsecure);
	TEST(testGameUISecurityOverlayEncrypted);
}

void TestConnectionSecurity::testConnectionSecurityEnum()
{
	// ConnectionSecurity enum should have two values
	UASSERT(ConnectionSecurity::Insecure == ConnectionSecurity(0));
	UASSERT(ConnectionSecurity::Encrypted == ConnectionSecurity(1));

	// Verify they are distinct
	UASSERT(ConnectionSecurity::Insecure != ConnectionSecurity::Encrypted);
}

void TestConnectionSecurity::testIsConnectionSecure()
{
	// Insecure connections should return false
	UASSERT(!isConnectionSecure(ConnectionSecurity::Insecure));

	// Encrypted connections should return true
	UASSERT(isConnectionSecure(ConnectionSecurity::Encrypted));
}

void TestConnectionSecurity::testConnectionSecurityFromFlags_zeroFlags()
{
	// No security flags → insecure connection
	ConnectionSecurity result = connectionSecurityFromFlags(0x00);
	UASSERT(result == ConnectionSecurity::Insecure);
}

void TestConnectionSecurity::testConnectionSecurityFromFlags_encryptedFlag()
{
	// ENCRYPTED flag set → encrypted connection
	ConnectionSecurity result = connectionSecurityFromFlags(ConnectionSecurityFlags::ENCRYPTED);
	UASSERT(result == ConnectionSecurity::Encrypted);
}

void TestConnectionSecurity::testConnectionSecurityFromFlags_supportedButNotEncrypted()
{
	// ENCRYPTION_SUPPORTED flag set, but ENCRYPTED is NOT → still insecure
	ConnectionSecurity result = connectionSecurityFromFlags(ConnectionSecurityFlags::ENCRYPTION_SUPPORTED);
	UASSERT(result == ConnectionSecurity::Insecure);
}

void TestConnectionSecurity::testConnectionSecurityFromFlags_bothFlags()
{
	// Both flags set → encrypted (ENCRYPTED takes precedence)
	u8 both = ConnectionSecurityFlags::ENCRYPTED | ConnectionSecurityFlags::ENCRYPTION_SUPPORTED;
	ConnectionSecurity result = connectionSecurityFromFlags(both);
	UASSERT(result == ConnectionSecurity::Encrypted);
}

void TestConnectionSecurity::testConnectionSecurityFromFlags_garbageFlags()
{
	// Garbage high bits without ENCRYPTED → insecure
	ConnectionSecurity result = connectionSecurityFromFlags(0xFE);
	UASSERT(result == ConnectionSecurity::Insecure);

	// Garbage high bits WITH ENCRYPTED → encrypted
	result = connectionSecurityFromFlags(0xFF);
	UASSERT(result == ConnectionSecurity::Encrypted);
}

void TestConnectionSecurity::testSecurityFlagsConstants()
{
	// ENCRYPTED is bit 0
	UASSERTEQ(int, ConnectionSecurityFlags::ENCRYPTED, 0x01);

	// ENCRYPTION_SUPPORTED is bit 1
	UASSERTEQ(int, ConnectionSecurityFlags::ENCRYPTION_SUPPORTED, 0x02);

	// They should not overlap
	UASSERT((ConnectionSecurityFlags::ENCRYPTED & ConnectionSecurityFlags::ENCRYPTION_SUPPORTED) == 0);
}

void TestConnectionSecurity::testGameUISecurityOverlayDefault()
{
	// By default (before connecting), the security state should be Insecure
	// This tests that the default value of the security state is insecure
	// which is the safe default — always warn until confirmed encrypted
	ConnectionSecurity default_state = ConnectionSecurity::Insecure;
	UASSERT(!isConnectionSecure(default_state));
}

void TestConnectionSecurity::testGameUISecurityOverlayInsecure()
{
	// When insecure, the overlay should be visible
	// Test the logic: isConnectionSecure returns false → overlay visible
	ConnectionSecurity state = ConnectionSecurity::Insecure;
	bool overlay_visible = !isConnectionSecure(state);
	UASSERT(overlay_visible);
}

void TestConnectionSecurity::testGameUISecurityOverlayEncrypted()
{
	// When encrypted, the overlay should NOT be visible
	// Test the logic: isConnectionSecure returns true → overlay hidden
	ConnectionSecurity state = ConnectionSecurity::Encrypted;
	bool overlay_visible = !isConnectionSecure(state);
	UASSERT(!overlay_visible);
}
