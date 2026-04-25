// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for the updated v9.1 security scoring system.
//
// Tests the complete security score progression:
//   0/100  — No encryption (Insecure)
//  70/100  — SRP + AES-256-GCM (no ECDH, no pinning, custom TLS)
//  85/100  — SRP + AES-256-GCM + ECDH X25519 (PFS)
//  95/100  — SRP + AES-256-GCM + ECDH + Fingerprint Pinned
// 100/100  — SRP + AES-256-GCM + ECDH + Pinned + TLS 1.3 Equivalent
//
// Also tests:
// - CERT_PINNED scores +10 same as CERT_VERIFIED
// - TLS_1_3_EQUIVALENT scores +5 same as TLS_1_3
// - populateRealSecurityInfo() with ECDH and pinning parameters

#include "test.h"

#include "network/connection_security.h"

class TestSecurityScoreV91 : public TestBase
{
public:
        TestSecurityScoreV91() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestSecurityScoreV91"; }

        void runTests(IGameDef *gamedef);

        // Score progression tests
        void testScoreInsecure0();
        void testScoreSRPOnly70();
        void testScoreWithECDH85();
        void testScoreWithECDHAndPinned95();
        void testScoreFull100();

        // New constant tests
        void testCertPinnedConstant();
        void testTls13EquivalentConstant();

        // Scoring detail tests
        void testCertPinnedScoresSameAsVerified();
        void testTls13EquivalentScoresSameAsTls13();
        void testCertTrustedOnFirstUseScores0();
        void testTlsCustomScores0();
        void testEcdhForwardSecrecyField();
        void testFingerprintPinnedField();

        // String conversion tests
        void testCertPinnedString();
        void testTls13EquivalentString();

        // populateRealSecurityInfo tests
        void testPopulateRealSecurityInfoWithECDH();
        void testPopulateRealSecurityInfoWithoutECDH();
        void testPopulateRealSecurityInfoWithPinning();
        void testPopulateRealSecurityInfoFull();

        // Score bar and label tests
        void testScore100LabelExcellent();
        void testScore85LabelGood();
        void testScore95Label();

        // Backwards compatibility
        void testOldCertVerifiedStillScores();
        void testOldTls13StillScores();
};

static TestSecurityScoreV91 g_test_instance;

void TestSecurityScoreV91::runTests(IGameDef *gamedef)
{
        // Score progression
        TEST(testScoreInsecure0);
        TEST(testScoreSRPOnly70);
        TEST(testScoreWithECDH85);
        TEST(testScoreWithECDHAndPinned95);
        TEST(testScoreFull100);

        // Constants
        TEST(testCertPinnedConstant);
        TEST(testTls13EquivalentConstant);

        // Scoring details
        TEST(testCertPinnedScoresSameAsVerified);
        TEST(testTls13EquivalentScoresSameAsTls13);
        TEST(testCertTrustedOnFirstUseScores0);
        TEST(testTlsCustomScores0);
        TEST(testEcdhForwardSecrecyField);
        TEST(testFingerprintPinnedField);

        // Strings
        TEST(testCertPinnedString);
        TEST(testTls13EquivalentString);

        // populateRealSecurityInfo
        TEST(testPopulateRealSecurityInfoWithECDH);
        TEST(testPopulateRealSecurityInfoWithoutECDH);
        TEST(testPopulateRealSecurityInfoWithPinning);
        TEST(testPopulateRealSecurityInfoFull);

        // Labels
        TEST(testScore100LabelExcellent);
        TEST(testScore85LabelGood);
        TEST(testScore95Label);

        // Backwards compatibility
        TEST(testOldCertVerifiedStillScores);
        TEST(testOldTls13StillScores);
}

void TestSecurityScoreV91::testScoreInsecure0()
{
        ConnectionSecurityInfo info;
        UASSERTEQ(int, info.getSecurityScore(), 0);
}

void TestSecurityScoreV91::testScoreSRPOnly70()
{
        // SRP + AES-256-GCM, but no ECDH, no pinning, custom TLS
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = false;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        // 30(enc) + 15(cipher) + 15(auth) + 10(replay) = 70
        UASSERTEQ(int, info.getSecurityScore(), 70);
}

void TestSecurityScoreV91::testScoreWithECDH85()
{
        // Add ECDH forward secrecy
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;  // ECDH provides this
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        // 70 + 15(PFS) = 85
        UASSERTEQ(int, info.getSecurityScore(), 85);
}

void TestSecurityScoreV91::testScoreWithECDHAndPinned95()
{
        // Add fingerprint pinning
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        // 85 + 10(pinned) = 95
        UASSERTEQ(int, info.getSecurityScore(), 95);
}

void TestSecurityScoreV91::testScoreFull100()
{
        // Full security: ECDH + pinned + TLS 1.3 equivalent
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        // 95 + 5(TLS equiv) = 100
        UASSERTEQ(int, info.getSecurityScore(), 100);
}

void TestSecurityScoreV91::testCertPinnedConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::CERT_PINNED, 5);
}

void TestSecurityScoreV91::testTls13EquivalentConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::TLS_1_3_EQUIVALENT, 4);
}

void TestSecurityScoreV91::testCertPinnedScoresSameAsVerified()
{
        // CERT_PINNED should score the same as CERT_VERIFIED (+10)
        ConnectionSecurityInfo info_pinned;
        info_pinned.state = ConnectionSecurity::Encrypted;
        info_pinned.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info_pinned.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info_pinned.replay_protection = true;
        info_pinned.forward_secrecy = true;
        info_pinned.certificate_status = ConnectionSecurityInfo::CERT_PINNED;

        ConnectionSecurityInfo info_verified;
        info_verified.state = ConnectionSecurity::Encrypted;
        info_verified.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info_verified.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info_verified.replay_protection = true;
        info_verified.forward_secrecy = true;
        info_verified.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;

        UASSERTEQ(int, info_pinned.getSecurityScore(), info_verified.getSecurityScore());
}

void TestSecurityScoreV91::testTls13EquivalentScoresSameAsTls13()
{
        // TLS_1_3_EQUIVALENT should score the same as TLS_1_3 (+5)
        ConnectionSecurityInfo info_equiv;
        info_equiv.state = ConnectionSecurity::Encrypted;
        info_equiv.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info_equiv.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info_equiv.replay_protection = true;
        info_equiv.forward_secrecy = true;
        info_equiv.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info_equiv.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        ConnectionSecurityInfo info_tls13;
        info_tls13.state = ConnectionSecurity::Encrypted;
        info_tls13.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info_tls13.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info_tls13.replay_protection = true;
        info_tls13.forward_secrecy = true;
        info_tls13.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info_tls13.tls_version = ConnectionSecurityInfo::TLS_1_3;

        UASSERTEQ(int, info_equiv.getSecurityScore(), info_tls13.getSecurityScore());
}

void TestSecurityScoreV91::testCertTrustedOnFirstUseScores0()
{
        // CERT_TRUST_ON_FIRST_USE does NOT score for certificate verification
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;

        // Without pinning, cert doesn't contribute to score
        int score_with_tofu = info.getSecurityScore();
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        int score_with_pinned = info.getSecurityScore();

        // Pinned should be 10 points more than TOFU
        UASSERTEQ(int, score_with_pinned - score_with_tofu, 10);
}

void TestSecurityScoreV91::testTlsCustomScores0()
{
        // TLS_CUSTOM does NOT score for TLS version
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        int score_custom = info.getSecurityScore();
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;
        int score_equiv = info.getSecurityScore();

        // Equivalent should be 5 points more than custom
        UASSERTEQ(int, score_equiv - score_custom, 5);
}

void TestSecurityScoreV91::testEcdhForwardSecrecyField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.ecdh_forward_secrecy);

        info.ecdh_forward_secrecy = true;
        UASSERT(info.ecdh_forward_secrecy);
}

void TestSecurityScoreV91::testFingerprintPinnedField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.fingerprint_pinned);

        info.fingerprint_pinned = true;
        UASSERT(info.fingerprint_pinned);
}

void TestSecurityScoreV91::testCertPinnedString()
{
        UASSERT(ConnectionSecurityInfo::getCertificateStatusString(
                ConnectionSecurityInfo::CERT_PINNED) == "Pinned (Verified)");
}

void TestSecurityScoreV91::testTls13EquivalentString()
{
        UASSERT(ConnectionSecurityInfo::getTlsVersionString(
                ConnectionSecurityInfo::TLS_1_3_EQUIVALENT) == "TLS 1.3 Equivalent (SRP+ECDH)");
}

void TestSecurityScoreV91::testPopulateRealSecurityInfoWithECDH()
{
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                true,   // ecdh_completed
                false,  // fingerprint_pinned
                0,      // fingerprint_verify_result
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000);

        UASSERT(info.isSecure());
        UASSERT(info.forward_secrecy);
        UASSERT(info.ecdh_forward_secrecy);
        UASSERT(info.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519);
        UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_1_3_EQUIVALENT);
}

void TestSecurityScoreV91::testPopulateRealSecurityInfoWithoutECDH()
{
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                false,  // ecdh_completed
                false,  // fingerprint_pinned
                0,      // fingerprint_verify_result
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000);

        UASSERT(info.isSecure());
        UASSERT(!info.forward_secrecy);
        UASSERT(!info.ecdh_forward_secrecy);
        UASSERT(info.key_exchange == ConnectionSecurityInfo::KEY_EXCHANGE_SRP);
        UASSERT(info.tls_version == ConnectionSecurityInfo::TLS_CUSTOM);
}

void TestSecurityScoreV91::testPopulateRealSecurityInfoWithPinning()
{
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                true,   // ecdh_completed
                true,   // fingerprint_pinned
                1,      // fingerprint_verify_result (match)
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000);

        UASSERT(info.isSecure());
        UASSERT(info.fingerprint_pinned);
        UASSERT(info.fingerprint_verify_result == 1);
        UASSERT(info.certificate_status == ConnectionSecurityInfo::CERT_PINNED);
}

void TestSecurityScoreV91::testPopulateRealSecurityInfoFull()
{
        // Full 100/100 configuration
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                true,   // ecdh_completed
                true,   // fingerprint_pinned
                1,      // fingerprint_verify_result (match)
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000);

        UASSERTEQ(int, info.getSecurityScore(), 100);
}

void TestSecurityScoreV91::testScore100LabelExcellent()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        UASSERT(info.getSecurityScoreString() == "100/100 (Excellent)");
}

void TestSecurityScoreV91::testScore85LabelGood()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        UASSERT(info.getSecurityScoreString() == "85/100 (Good)");
}

void TestSecurityScoreV91::testScore95Label()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        UASSERT(info.getSecurityScoreString() == "95/100 (Good)");
}

void TestSecurityScoreV91::testOldCertVerifiedStillScores()
{
        // Backwards compatibility: CERT_VERIFIED still scores +10
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3;

        UASSERTEQ(int, info.getSecurityScore(), 100);
}

void TestSecurityScoreV91::testOldTls13StillScores()
{
        // Backwards compatibility: TLS_1_3 still scores +5
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_VERIFIED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3;

        // Should score 100
        UASSERTEQ(int, info.getSecurityScore(), 100);
}
