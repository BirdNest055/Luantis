// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "test.h"

#include "network/connection_security.h"

class TestConnectionSecurityInfo : public TestBase
{
public:
        TestConnectionSecurityInfo() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestConnectionSecurityInfo"; }

        void runTests(IGameDef *gamedef);

        // ConnectionSecurityInfo struct tests
        void testSecurityInfoDefaultValues();
        void testSecurityInfoInsecureConnection();
        void testSecurityInfoEncryptedConnection();
        void testSecurityInfoCustomValues();
        void testSecurityInfoGetStateString();
        void testSecurityInfoGetEncryptionString();
        void testSecurityInfoGetKeyExchangeString();
        void testSecurityInfoGetAuthenticationString();
        void testSecurityInfoGetCipherSuiteString();
        void testSecurityInfoGetCertStatusString();
        void testSecurityInfoIsSecure();
        void testSecurityInfoIsForwardSecret();
        void testSecurityInfoIsReplayProtected();
        void testSecurityInfoIsAuthenticated();

        // Extended security flags tests
        void testExtendedSecurityFlagsConstants();
        void testSecurityInfoFromExtendedFlags_zero();
        void testSecurityInfoFromExtendedFlags_allSet();
        void testSecurityInfoFromExtendedFlags_partial();
        void testSecurityInfoFromExtendedFlags_forwardSecrecyOnly();

        // Integration tests
        void testSecurityInfoOverlayConsistency();
        void testSecurityInfoStateTransitions();

        // v8: Security Info Tab — new fields and security score
        void testSecurityInfoV8NewFieldDefaults();
        void testSecurityInfoV8SessionId();
        void testSecurityInfoV8ConnectedSince();
        void testSecurityInfoV8ServerFingerprint();
        void testSecurityInfoV8TlsVersion();
        void testSecurityInfoV8SecurityScore_insecure();
        void testSecurityInfoV8SecurityScore_partial();
        void testSecurityInfoV8SecurityScore_full();
        void testSecurityInfoV8GetTlsVersionString();
        void testSecurityInfoV8GetSecurityScoreString();
        void testSecurityInfoV8SecurityInfoFromFlags_withSessionData();
        void testSecurityInfoV8ResetClearsNewFields();
};

static TestConnectionSecurityInfo g_test_instance;

void TestConnectionSecurityInfo::runTests(IGameDef *gamedef)
{
        TEST(testSecurityInfoDefaultValues);
        TEST(testSecurityInfoInsecureConnection);
        TEST(testSecurityInfoEncryptedConnection);
        TEST(testSecurityInfoCustomValues);
        TEST(testSecurityInfoGetStateString);
        TEST(testSecurityInfoGetEncryptionString);
        TEST(testSecurityInfoGetKeyExchangeString);
        TEST(testSecurityInfoGetAuthenticationString);
        TEST(testSecurityInfoGetCipherSuiteString);
        TEST(testSecurityInfoGetCertStatusString);
        TEST(testSecurityInfoIsSecure);
        TEST(testSecurityInfoIsForwardSecret);
        TEST(testSecurityInfoIsReplayProtected);
        TEST(testSecurityInfoIsAuthenticated);
        TEST(testExtendedSecurityFlagsConstants);
        TEST(testSecurityInfoFromExtendedFlags_zero);
        TEST(testSecurityInfoFromExtendedFlags_allSet);
        TEST(testSecurityInfoFromExtendedFlags_partial);
        TEST(testSecurityInfoFromExtendedFlags_forwardSecrecyOnly);
        TEST(testSecurityInfoOverlayConsistency);
        TEST(testSecurityInfoStateTransitions);

        // v8: Security Info Tab — new field tests
        TEST(testSecurityInfoV8NewFieldDefaults);
        TEST(testSecurityInfoV8SessionId);
        TEST(testSecurityInfoV8ConnectedSince);
        TEST(testSecurityInfoV8ServerFingerprint);
        TEST(testSecurityInfoV8TlsVersion);
        TEST(testSecurityInfoV8SecurityScore_insecure);
        TEST(testSecurityInfoV8SecurityScore_partial);
        TEST(testSecurityInfoV8SecurityScore_full);
        TEST(testSecurityInfoV8GetTlsVersionString);
        TEST(testSecurityInfoV8GetSecurityScoreString);
        TEST(testSecurityInfoV8SecurityInfoFromFlags_withSessionData);
        TEST(testSecurityInfoV8ResetClearsNewFields);
}

// --- ConnectionSecurityInfo struct tests ---

void TestConnectionSecurityInfo::testSecurityInfoDefaultValues()
{
        // Default ConnectionSecurityInfo should represent an insecure, unconnected state
        ConnectionSecurityInfo info;

        UASSERT(info.state == ConnectionSecurity::Insecure);
        UASSERT(info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);
        UASSERT(info.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_NONE);
        UASSERT(info.authentication == ConnectionSecurityInfo::AUTH_NONE);
        UASSERT(info.cipher_suite == ConnectionSecurityInfo::CIPHER_NONE);
        UASSERT(info.certificate_status == ConnectionSecurityInfo::CERT_NOT_VERIFIED);
        UASSERT(!info.forward_secrecy);
        UASSERT(!info.replay_protection);
        UASSERT(info.protocol_version == 0);
        UASSERT(info.server_address.empty());
        UASSERT(info.server_port == 0);
}

void TestConnectionSecurityInfo::testSecurityInfoInsecureConnection()
{
        // Insecure connection info should have all security fields at "none" values
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Insecure;

        UASSERT(info.state == ConnectionSecurity::Insecure);
        UASSERT(!isConnectionSecure(info.state));
        UASSERT(!info.isSecure());
}

void TestConnectionSecurityInfo::testSecurityInfoEncryptedConnection()
{
        // Encrypted connection with full security details
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;
        info.forward_secrecy = true;
        info.replay_protection = true;

        UASSERT(info.state == ConnectionSecurity::Encrypted);
        UASSERT(isConnectionSecure(info.state));
        UASSERT(info.isSecure());
        UASSERT(info.forward_secrecy);
        UASSERT(info.replay_protection);
}

void TestConnectionSecurityInfo::testSecurityInfoCustomValues()
{
        // Test setting and reading back custom values
        ConnectionSecurityInfo info;

        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_CHACHA20_POLY1305;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_P256;
        info.authentication = ConnectionSecurityInfo::AUTH_ECDSA;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_CHACHA20_POLY1305;
        info.certificate_status = ConnectionSecurityInfo::CERT_SELF_SIGNED;
        info.forward_secrecy = true;
        info.replay_protection = false;
        info.protocol_version = 42;
        info.server_address = "example.com";
        info.server_port = 30000;

        UASSERTEQ(int, info.encryption_algorithm, ConnectionSecurityInfo::ENCRYPTION_CHACHA20_POLY1305);
        UASSERTEQ(int, info.key_exchange, ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_P256);
        UASSERTEQ(int, info.authentication, ConnectionSecurityInfo::AUTH_ECDSA);
        UASSERTEQ(int, info.cipher_suite, ConnectionSecurityInfo::CIPHER_CHACHA20_POLY1305);
        UASSERTEQ(int, info.certificate_status, ConnectionSecurityInfo::CERT_SELF_SIGNED);
        UASSERT(info.forward_secrecy);
        UASSERT(!info.replay_protection);
        UASSERTEQ(int, info.protocol_version, 42);
        UASSERT(info.server_address == "example.com");
        UASSERTEQ(int, info.server_port, 30000);
}

void TestConnectionSecurityInfo::testSecurityInfoGetStateString()
{
        // Human-readable state strings
        ConnectionSecurityInfo insecure;
        UASSERT(insecure.getStateString() == "Insecure");

        ConnectionSecurityInfo encrypted;
        encrypted.state = ConnectionSecurity::Encrypted;
        UASSERT(encrypted.getStateString() == "Encrypted");
}

void TestConnectionSecurityInfo::testSecurityInfoGetEncryptionString()
{
        UASSERT(ConnectionSecurityInfo::getEncryptionString(
                ConnectionSecurityInfo::ENCRYPTION_NONE) == "None");
        UASSERT(ConnectionSecurityInfo::getEncryptionString(
                ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM) == "AES-256-GCM");
        UASSERT(ConnectionSecurityInfo::getEncryptionString(
                ConnectionSecurityInfo::ENCRYPTION_CHACHA20_POLY1305) == "ChaCha20-Poly1305");
}

void TestConnectionSecurityInfo::testSecurityInfoGetKeyExchangeString()
{
        UASSERT(ConnectionSecurityInfo::getKeyExchangeString(
                ConnectionSecurityInfo::KEY_EXCHANGE_NONE) == "None");
        UASSERT(ConnectionSecurityInfo::getKeyExchangeString(
                ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519) == "ECDH (X25519)");
        UASSERT(ConnectionSecurityInfo::getKeyExchangeString(
                ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_P256) == "ECDH (P-256)");
}

void TestConnectionSecurityInfo::testSecurityInfoGetAuthenticationString()
{
        UASSERT(ConnectionSecurityInfo::getAuthenticationString(
                ConnectionSecurityInfo::AUTH_NONE) == "None");
        UASSERT(ConnectionSecurityInfo::getAuthenticationString(
                ConnectionSecurityInfo::AUTH_SRP) == "SRP");
        UASSERT(ConnectionSecurityInfo::getAuthenticationString(
                ConnectionSecurityInfo::AUTH_ECDSA) == "ECDSA");
}

void TestConnectionSecurityInfo::testSecurityInfoGetCipherSuiteString()
{
        UASSERT(ConnectionSecurityInfo::getCipherSuiteString(
                ConnectionSecurityInfo::CIPHER_NONE) == "None");
        UASSERT(ConnectionSecurityInfo::getCipherSuiteString(
                ConnectionSecurityInfo::CIPHER_AES_256_GCM) == "AES-256-GCM");
        UASSERT(ConnectionSecurityInfo::getCipherSuiteString(
                ConnectionSecurityInfo::CIPHER_CHACHA20_POLY1305) == "ChaCha20-Poly1305");
}

void TestConnectionSecurityInfo::testSecurityInfoGetCertStatusString()
{
        UASSERT(ConnectionSecurityInfo::getCertificateStatusString(
                ConnectionSecurityInfo::CERT_NOT_VERIFIED) == "Not Verified");
        UASSERT(ConnectionSecurityInfo::getCertificateStatusString(
                ConnectionSecurityInfo::CERT_VERIFIED) == "Verified");
        UASSERT(ConnectionSecurityInfo::getCertificateStatusString(
                ConnectionSecurityInfo::CERT_SELF_SIGNED) == "Self-Signed");
        UASSERT(ConnectionSecurityInfo::getCertificateStatusString(
                ConnectionSecurityInfo::CERT_EXPIRED) == "Expired");
}

void TestConnectionSecurityInfo::testSecurityInfoIsSecure()
{
        // isSecure() should return true only when state is Encrypted
        ConnectionSecurityInfo info;
        UASSERT(!info.isSecure());

        info.state = ConnectionSecurity::Encrypted;
        UASSERT(info.isSecure());
}

void TestConnectionSecurityInfo::testSecurityInfoIsForwardSecret()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.isForwardSecret());

        info.forward_secrecy = true;
        UASSERT(info.isForwardSecret());
}

void TestConnectionSecurityInfo::testSecurityInfoIsReplayProtected()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.isReplayProtected());

        info.replay_protection = true;
        UASSERT(info.isReplayProtected());
}

void TestConnectionSecurityInfo::testSecurityInfoIsAuthenticated()
{
        // isAuthenticated() should return true when authentication is not NONE
        ConnectionSecurityInfo info;
        UASSERT(!info.isAuthenticated());

        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        UASSERT(info.isAuthenticated());

        info.authentication = ConnectionSecurityInfo::AUTH_ECDSA;
        UASSERT(info.isAuthenticated());
}

// --- Extended security flags tests ---

void TestConnectionSecurityInfo::testExtendedSecurityFlagsConstants()
{
        // Verify new flag constants
        UASSERTEQ(int, ConnectionSecurityFlags::ENCRYPTED, 0x01);
        UASSERTEQ(int, ConnectionSecurityFlags::ENCRYPTION_SUPPORTED, 0x02);
        UASSERTEQ(int, ConnectionSecurityFlags::FORWARD_SECRECY, 0x04);
        UASSERTEQ(int, ConnectionSecurityFlags::AUTHENTICATED, 0x08);
        UASSERTEQ(int, ConnectionSecurityFlags::REPLAY_PROTECTED, 0x10);

        // No overlap between any two flags
        UASSERT((ConnectionSecurityFlags::ENCRYPTED & ConnectionSecurityFlags::ENCRYPTION_SUPPORTED) == 0);
        UASSERT((ConnectionSecurityFlags::ENCRYPTED & ConnectionSecurityFlags::FORWARD_SECRECY) == 0);
        UASSERT((ConnectionSecurityFlags::ENCRYPTED & ConnectionSecurityFlags::AUTHENTICATED) == 0);
        UASSERT((ConnectionSecurityFlags::ENCRYPTED & ConnectionSecurityFlags::REPLAY_PROTECTED) == 0);
        UASSERT((ConnectionSecurityFlags::FORWARD_SECRECY & ConnectionSecurityFlags::AUTHENTICATED) == 0);
        UASSERT((ConnectionSecurityFlags::FORWARD_SECRECY & ConnectionSecurityFlags::REPLAY_PROTECTED) == 0);
        UASSERT((ConnectionSecurityFlags::AUTHENTICATED & ConnectionSecurityFlags::REPLAY_PROTECTED) == 0);
}

void TestConnectionSecurityInfo::testSecurityInfoFromExtendedFlags_zero()
{
        // No flags set → fully insecure info
        ConnectionSecurityInfo info = connectionSecurityInfoFromFlags(0x00);

        UASSERT(info.state == ConnectionSecurity::Insecure);
        UASSERT(info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);
        UASSERT(!info.forward_secrecy);
        UASSERT(!info.replay_protection);
        UASSERT(info.authentication == ConnectionSecurityInfo::AUTH_NONE);
}

void TestConnectionSecurityInfo::testSecurityInfoFromExtendedFlags_allSet()
{
        // All flags set → v9.1 honest: connectionSecurityInfoFromFlags() only notes
        // that the server supports encryption. It does NOT set state=Encrypted
        // because encryption isn't confirmed until SRP auth completes.
        // The real security info is populated by populateRealSecurityInfo().
        u8 all_flags = ConnectionSecurityFlags::ENCRYPTED
                | ConnectionSecurityFlags::ENCRYPTION_SUPPORTED
                | ConnectionSecurityFlags::FORWARD_SECRECY
                | ConnectionSecurityFlags::AUTHENTICATED
                | ConnectionSecurityFlags::REPLAY_PROTECTED;

        ConnectionSecurityInfo info = connectionSecurityInfoFromFlags(all_flags);

        // v9.1: flags only indicate server support, not active encryption
        // state remains Insecure until populateRealSecurityInfo() is called
        UASSERT(info.state == ConnectionSecurity::Insecure);
        // authentication is set from AUTHENTICATED flag
        UASSERT(info.authentication == ConnectionSecurityInfo::AUTH_SRP);
        // forward_secrecy, replay_protection, encryption_algorithm are NOT set
        // from flags alone — they require actual encryption activation
        UASSERT(!info.forward_secrecy);
        UASSERT(!info.replay_protection);
        UASSERT(info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);

        // Now test the REAL path: populateRealSecurityInfo with encryption active
        ConnectionSecurityInfo real_info = populateRealSecurityInfo(
                true,   /* encryption_active */
                true,   /* ecdh_completed */
                true,   /* fingerprint_pinned */
                1,      /* fingerprint_verify_result = match */
                "test-session", "SHA256:abc", 0, 44, "server.com", 30000);
        UASSERT(real_info.state == ConnectionSecurity::Encrypted);
        UASSERT(real_info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM);
        UASSERT(real_info.forward_secrecy);
        UASSERT(real_info.replay_protection);
        UASSERT(real_info.authentication == ConnectionSecurityInfo::AUTH_SRP);
}

void TestConnectionSecurityInfo::testSecurityInfoFromExtendedFlags_partial()
{
        // v9.1 honest: ENCRYPTED flag alone doesn't mean encryption is active.
        // Use populateRealSecurityInfo for the real state.
        u8 flags = ConnectionSecurityFlags::ENCRYPTED
                | ConnectionSecurityFlags::REPLAY_PROTECTED;

        ConnectionSecurityInfo info = connectionSecurityInfoFromFlags(flags);

        // Flags don't set Encrypted state or algorithm — only SRP auth does
        UASSERT(info.state == ConnectionSecurity::Insecure);
        UASSERT(info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);
        UASSERT(!info.forward_secrecy);
        UASSERT(!info.replay_protection);

        // The REAL path: encryption active but no ECDH → no forward secrecy
        ConnectionSecurityInfo real_info = populateRealSecurityInfo(
                true,   /* encryption_active */
                false,  /* ecdh_completed = NO forward secrecy */
                false,  /* fingerprint_pinned */
                0,      /* fingerprint_verify_result */
                "test-session", "SHA256:abc", 0, 44, "server.com", 30000);
        UASSERT(real_info.state == ConnectionSecurity::Encrypted);
        UASSERT(real_info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM);
        UASSERT(!real_info.forward_secrecy);
        UASSERT(real_info.replay_protection);  // Always true when encryption is active
}

void TestConnectionSecurityInfo::testSecurityInfoFromExtendedFlags_forwardSecrecyOnly()
{
        // FORWARD_SECRECY without ENCRYPTED → still insecure (forward secrecy is irrelevant without encryption)
        u8 flags = ConnectionSecurityFlags::FORWARD_SECRECY;

        ConnectionSecurityInfo info = connectionSecurityInfoFromFlags(flags);

        UASSERT(info.state == ConnectionSecurity::Insecure);
        UASSERT(info.encryption_algorithm == ConnectionSecurityInfo::ENCRYPTION_NONE);
        UASSERT(!info.forward_secrecy); // Can't have forward secrecy without encryption
}

// --- Integration tests ---

void TestConnectionSecurityInfo::testSecurityInfoOverlayConsistency()
{
        // When ConnectionSecurityInfo says insecure, shouldShowSecurityOverlay must agree
        ConnectionSecurityInfo insecure_info;
        UASSERT(!insecure_info.isSecure());
        // The overlay logic: show overlay when insecure AND setting enabled
        bool should_show = !isConnectionSecure(insecure_info.state);
        UASSERT(should_show);

        // When encrypted, overlay should not show
        ConnectionSecurityInfo encrypted_info;
        encrypted_info.state = ConnectionSecurity::Encrypted;
        encrypted_info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        should_show = !isConnectionSecure(encrypted_info.state);
        UASSERT(!should_show);
}

void TestConnectionSecurityInfo::testSecurityInfoStateTransitions()
{
        // Test transitioning from insecure to encrypted and back
        ConnectionSecurityInfo info;

        // Start insecure
        UASSERT(!info.isSecure());

        // Transition to encrypted
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.forward_secrecy = true;
        info.replay_protection = true;
        UASSERT(info.isSecure());

        // Transition back to insecure (e.g., connection downgrade)
        info.state = ConnectionSecurity::Insecure;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_NONE;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_NONE;
        info.forward_secrecy = false;
        info.replay_protection = false;
        UASSERT(!info.isSecure());
}

// --- v8: Security Info Tab — new field tests ---

void TestConnectionSecurityInfo::testSecurityInfoV8NewFieldDefaults()
{
        // New v8 fields should have sensible defaults for "not connected" state
        ConnectionSecurityInfo info;

        UASSERT(info.session_id.empty());
        UASSERT(info.connected_since == 0);
        UASSERT(info.server_fingerprint.empty());
        UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_NONE);
}

void TestConnectionSecurityInfo::testSecurityInfoV8SessionId()
{
        // Session ID should be a hex string that can be set and read back
        ConnectionSecurityInfo info;
        info.session_id = "a1b2c3d4e5f6";
        UASSERT(info.session_id == "a1b2c3d4e5f6");

        // Session ID should be clearable
        info.session_id.clear();
        UASSERT(info.session_id.empty());
}

void TestConnectionSecurityInfo::testSecurityInfoV8ConnectedSince()
{
        // connected_since stores a Unix timestamp
        ConnectionSecurityInfo info;
        UASSERT(info.connected_since == 0);

        info.connected_since = 1700000000; // a reasonable recent timestamp
        UASSERTEQ(u64, info.connected_since, 1700000000);
}

void TestConnectionSecurityInfo::testSecurityInfoV8ServerFingerprint()
{
        // Server fingerprint should be a hex string
        ConnectionSecurityInfo info;
        info.server_fingerprint = "SHA256:abc123def456";
        UASSERT(info.server_fingerprint == "SHA256:abc123def456");
}

void TestConnectionSecurityInfo::testSecurityInfoV8TlsVersion()
{
        // TLS version constants should be defined
        UASSERTEQ(int, ConnectionSecurityInfo::TLS_NONE, 0);
        UASSERTEQ(int, ConnectionSecurityInfo::TLS_1_2, 1);
        UASSERTEQ(int, ConnectionSecurityInfo::TLS_1_3, 2);
}

void TestConnectionSecurityInfo::testSecurityInfoV8SecurityScore_insecure()
{
        // Insecure connection should have a score of 0
        ConnectionSecurityInfo info;
        UASSERTEQ(int, info.getSecurityScore(), 0);
}

void TestConnectionSecurityInfo::testSecurityInfoV8SecurityScore_partial()
{
        // Partially secure: encrypted but no forward secrecy, no cert verification
        // Score should be moderate
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;

        int score = info.getSecurityScore();
        UASSERT(score > 0);     // Some points for encryption
        UASSERT(score < 100);   // But not full marks
}

void TestConnectionSecurityInfo::testSecurityInfoV8SecurityScore_full()
{
        // Fully secure: all properties set, including v9.1 honest requirements:
        // ECDH forward secrecy, fingerprint pinning, and TLS 1.3 equivalent
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;  // v9.1: needs PINNED for full score
        info.forward_secrecy = true;
        info.replay_protection = true;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;  // v9.1: needs this for +5

        UASSERTEQ(int, info.getSecurityScore(), 100);
}

void TestConnectionSecurityInfo::testSecurityInfoV8GetTlsVersionString()
{
        UASSERT(ConnectionSecurityInfo::getTlsVersionString(
                ConnectionSecurityInfo::TLS_NONE) == "None");
        UASSERT(ConnectionSecurityInfo::getTlsVersionString(
                ConnectionSecurityInfo::TLS_1_2) == "TLS 1.2");
        UASSERT(ConnectionSecurityInfo::getTlsVersionString(
                ConnectionSecurityInfo::TLS_1_3) == "TLS 1.3");
}

void TestConnectionSecurityInfo::testSecurityInfoV8GetSecurityScoreString()
{
        // getSecurityScoreString() should return a human-readable label
        ConnectionSecurityInfo insecure;
        UASSERT(insecure.getSecurityScoreString() == "0/100 (Insecure)");

        // v9.1: Full score requires CERT_PINNED and TLS_1_3_EQUIVALENT
        ConnectionSecurityInfo full;
        full.state = ConnectionSecurity::Encrypted;
        full.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        full.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        full.authentication = ConnectionSecurityInfo::AUTH_SRP;
        full.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        full.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        full.forward_secrecy = true;
        full.replay_protection = true;
        full.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;
        UASSERT(full.getSecurityScoreString() == "100/100 (Excellent)");

        // v9.1: With just CERT_VERIFIED (not PINNED) and no TLS equiv → 90/100 (Good)
        ConnectionSecurityInfo good;
        good.state = ConnectionSecurity::Encrypted;
        good.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        good.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
        good.authentication = ConnectionSecurityInfo::AUTH_SRP;
        good.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        good.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;
        good.forward_secrecy = true;
        good.replay_protection = true;
        UASSERT(good.getSecurityScoreString() == "90/100 (Good)");
}

void TestConnectionSecurityInfo::testSecurityInfoV8SecurityInfoFromFlags_withSessionData()
{
        // v9.1 honest: connectionSecurityInfoFromFlags() does NOT set tls_version
        // because flags alone don't prove encryption is active.
        // Use populateRealSecurityInfo() for real TLS version info.
        u8 all_flags = ConnectionSecurityFlags::ENCRYPTED
                | ConnectionSecurityFlags::ENCRYPTION_SUPPORTED
                | ConnectionSecurityFlags::FORWARD_SECRECY
                | ConnectionSecurityFlags::AUTHENTICATED
                | ConnectionSecurityFlags::REPLAY_PROTECTED;

        ConnectionSecurityInfo info = connectionSecurityInfoFromFlags(all_flags);

        // v9.1: flags don't set tls_version — that requires actual encryption
        UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_NONE);

        // The REAL path with ECDH gives TLS 1.3 equivalent
        ConnectionSecurityInfo real_info = populateRealSecurityInfo(
                true,   /* encryption_active */
                true,   /* ecdh_completed */
                true,   /* fingerprint_pinned */
                1,      /* fingerprint_verify_result */
                "session-123", "SHA256:abc", 0, 44, "server.com", 30000);
        UASSERT(real_info.tls_version == ConnectionSecurityInfo::TLS_1_3_EQUIVALENT);

        // Without ECDH, it's TLS_CUSTOM (SRP-derived)
        ConnectionSecurityInfo srp_info = populateRealSecurityInfo(
                true,   /* encryption_active */
                false,  /* ecdh_completed */
                false,  /* fingerprint_pinned */
                0,      /* fingerprint_verify_result */
                "session-123", "SHA256:abc", 0, 44, "server.com", 30000);
        UASSERT(srp_info.tls_version == ConnectionSecurityInfo::TLS_CUSTOM);
}

void TestConnectionSecurityInfo::testSecurityInfoV8ResetClearsNewFields()
{
        // After setting v8 fields, they should be clearable
        ConnectionSecurityInfo info;
        info.session_id = "test-session";
        info.connected_since = 1700000000;
        info.server_fingerprint = "SHA256:abc";
        info.tls_version = ConnectionSecurityInfo::TLS_1_3;

        // "Reset" by creating a new default object
        info = ConnectionSecurityInfo();
        UASSERT(info.session_id.empty());
        UASSERT(info.connected_since == 0);
        UASSERT(info.server_fingerprint.empty());
        UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_NONE);
}
