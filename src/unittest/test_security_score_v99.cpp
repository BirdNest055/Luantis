// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for the v9.9 enhanced security scoring system.
//
// TDD tests for new bonus scoring factors that help connections
// get closer to 100/100 on first connection (TOFU) and reward
// implementation quality (key rotation, salted HKDF, exact replay).
//
// v9.9 Scoring Design:
//   Base scoring (unchanged from v9.1, max 100):
//     +30 Encryption active
//     +15 Strong cipher suite
//     +15 Forward secrecy (ECDH)
//     +15 Authentication (SRP)
//     +10 Replay protection
//     +10 Certificate verified/pinned
//     +5  TLS 1.3 Equivalent
//
//   NEW Bonus scoring (v9.9, max +15):
//     +3  TOFU acknowledged (partial credit for first-use trust)
//     +5  Key rotation capable (session rekeying implemented)
//     +2  Salted key derivation (HKDF uses salt)
//     +2  Exact replay bitmap (bitmap within sliding window)
//     +3  Integrity verified (zero auth failures in session)
//
//   A first connection with ECDH + TOFU + all bonuses can now reach 100/100!
//
// Score progression (v9.9):
//   0/100   — No encryption (Insecure)
//  73/100   — SRP + AES-256-GCM + TOFU bonus (Fair→Good range)
//  88/100   — SRP + AES-256-GCM + ECDH + TOFU bonus (Good)
//  95/100   — SRP + AES-256-GCM + ECDH + Pinned (Very Good)
// 100/100   — Full security with all features (Excellent)
// 100/100   — First connection with ECDH + all bonuses (Excellent!)

#include "test.h"

#include "network/connection_security.h"

class TestSecurityScoreV99 : public TestBase
{
public:
        TestSecurityScoreV99() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestSecurityScoreV99"; }

        void runTests(IGameDef *gamedef);

        // --- New bonus field tests ---
        void testTofuAcknowledgedField();
        void testKeyRotationCapableField();
        void testSaltedKeyDerivationField();
        void testExactReplayBitmapField();
        void testIntegrityVerifiedField();

        // --- Bonus scoring tests ---
        void testTofuBonusScores3();
        void testKeyRotationBonusScores5();
        void testSaltedKeyDerivationBonusScores2();
        void testExactReplayBitmapBonusScores2();
        void testIntegrityVerifiedBonusScores3();

        // --- Score progression with bonuses ---
        void testFirstConnectionWithECDHAndAllBonusesReaches100();
        void testFirstConnectionWithECDHAndTOFUBonus();
        void testReturnConnectionWithPinningStill100();
        void testNoEncryptionWithBonusesIsStill0();

        // --- Backwards compatibility ---
        void testExisting100ScoreStillWorks();
        void testBonusDoesNotReduceBaseScore();
        void testBaseScoreMethodUnchanged();

        // --- New constants ---
        void testTofuAcknowledgedConstant();
        void testKeyRotationCapableConstant();
        void testSaltedKeyDerivationConstant();
        void testExactReplayBitmapConstant();
        void testIntegrityVerifiedConstant();

        // --- String conversion tests ---
        void testTofuAcknowledgedString();
        void testKeyRotationCapableString();

        // --- populateRealSecurityInfo with new fields ---
        void testPopulateRealSecurityInfoWithBonuses();
        void testPopulateRealSecurityInfoWithKeyRotation();
        void testPopulateRealSecurityInfoWithIntegrity();

        // --- Score label tests ---
        void testScore100StillExcellent();
        void testScore88WithBonusesLabel();

        // --- getSecurityScoreWithBonuses method ---
        void testGetSecurityScoreWithBonusesMethod();
        void testGetBonusScoreMethod();
        void testGetBonusBreakdownMethod();
};

static TestSecurityScoreV99 g_test_instance;

void TestSecurityScoreV99::runTests(IGameDef *gamedef)
{
        // New bonus field tests
        TEST(testTofuAcknowledgedField);
        TEST(testKeyRotationCapableField);
        TEST(testSaltedKeyDerivationField);
        TEST(testExactReplayBitmapField);
        TEST(testIntegrityVerifiedField);

        // Bonus scoring tests
        TEST(testTofuBonusScores3);
        TEST(testKeyRotationBonusScores5);
        TEST(testSaltedKeyDerivationBonusScores2);
        TEST(testExactReplayBitmapBonusScores2);
        TEST(testIntegrityVerifiedBonusScores3);

        // Score progression with bonuses
        TEST(testFirstConnectionWithECDHAndAllBonusesReaches100);
        TEST(testFirstConnectionWithECDHAndTOFUBonus);
        TEST(testReturnConnectionWithPinningStill100);
        TEST(testNoEncryptionWithBonusesIsStill0);

        // Backwards compatibility
        TEST(testExisting100ScoreStillWorks);
        TEST(testBonusDoesNotReduceBaseScore);
        TEST(testBaseScoreMethodUnchanged);

        // New constants
        TEST(testTofuAcknowledgedConstant);
        TEST(testKeyRotationCapableConstant);
        TEST(testSaltedKeyDerivationConstant);
        TEST(testExactReplayBitmapConstant);
        TEST(testIntegrityVerifiedConstant);

        // String conversion
        TEST(testTofuAcknowledgedString);
        TEST(testKeyRotationCapableString);

        // populateRealSecurityInfo with new fields
        TEST(testPopulateRealSecurityInfoWithBonuses);
        TEST(testPopulateRealSecurityInfoWithKeyRotation);
        TEST(testPopulateRealSecurityInfoWithIntegrity);

        // Score label tests
        TEST(testScore100StillExcellent);
        TEST(testScore88WithBonusesLabel);

        // New methods
        TEST(testGetSecurityScoreWithBonusesMethod);
        TEST(testGetBonusScoreMethod);
        TEST(testGetBonusBreakdownMethod);
}

// ============================================================================
// New Bonus Field Tests
// ============================================================================

void TestSecurityScoreV99::testTofuAcknowledgedField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.tofu_acknowledged);

        info.tofu_acknowledged = true;
        UASSERT(info.tofu_acknowledged);
}

void TestSecurityScoreV99::testKeyRotationCapableField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.key_rotation_capable);

        info.key_rotation_capable = true;
        UASSERT(info.key_rotation_capable);
}

void TestSecurityScoreV99::testSaltedKeyDerivationField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.salted_key_derivation);

        info.salted_key_derivation = true;
        UASSERT(info.salted_key_derivation);
}

void TestSecurityScoreV99::testExactReplayBitmapField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.exact_replay_bitmap);

        info.exact_replay_bitmap = true;
        UASSERT(info.exact_replay_bitmap);
}

void TestSecurityScoreV99::testIntegrityVerifiedField()
{
        ConnectionSecurityInfo info;
        UASSERT(!info.integrity_verified);

        info.integrity_verified = true;
        UASSERT(info.integrity_verified);
}

// ============================================================================
// Bonus Scoring Tests
// ============================================================================

void TestSecurityScoreV99::testTofuBonusScores3()
{
        // Base config: SRP+AES+ECDH+replay (85 base) + TOFU cert status
        // Without TOFU bonus: 85
        // With TOFU bonus: 88
        ConnectionSecurityInfo info_no_tofu;
        info_no_tofu.state = ConnectionSecurity::Encrypted;
        info_no_tofu.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info_no_tofu.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info_no_tofu.replay_protection = true;
        info_no_tofu.forward_secrecy = true;
        info_no_tofu.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info_no_tofu.tofu_acknowledged = false;

        ConnectionSecurityInfo info_with_tofu = info_no_tofu;
        info_with_tofu.tofu_acknowledged = true;

        int base_diff = info_with_tofu.getSecurityScore() - info_no_tofu.getSecurityScore();
        UASSERTEQ(int, base_diff, 3);
}

void TestSecurityScoreV99::testKeyRotationBonusScores5()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.key_rotation_capable = false;

        int score_no_rotation = info.getSecurityScore();

        info.key_rotation_capable = true;
        int score_with_rotation = info.getSecurityScore();

        UASSERTEQ(int, score_with_rotation - score_no_rotation, 5);
}

void TestSecurityScoreV99::testSaltedKeyDerivationBonusScores2()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.salted_key_derivation = false;

        int score_no_salt = info.getSecurityScore();

        info.salted_key_derivation = true;
        int score_with_salt = info.getSecurityScore();

        UASSERTEQ(int, score_with_salt - score_no_salt, 2);
}

void TestSecurityScoreV99::testExactReplayBitmapBonusScores2()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.exact_replay_bitmap = false;

        int score_no_bitmap = info.getSecurityScore();

        info.exact_replay_bitmap = true;
        int score_with_bitmap = info.getSecurityScore();

        UASSERTEQ(int, score_with_bitmap - score_no_bitmap, 2);
}

void TestSecurityScoreV99::testIntegrityVerifiedBonusScores3()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.integrity_verified = false;

        int score_no_integrity = info.getSecurityScore();

        info.integrity_verified = true;
        int score_with_integrity = info.getSecurityScore();

        UASSERTEQ(int, score_with_integrity - score_no_integrity, 3);
}

// ============================================================================
// Score Progression with Bonuses
// ============================================================================

void TestSecurityScoreV99::testFirstConnectionWithECDHAndAllBonusesReaches100()
{
        // First connection: TOFU (not pinned), but with ECDH + all bonuses
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        // v9.9 bonuses
        info.tofu_acknowledged = true;
        info.key_rotation_capable = true;
        info.salted_key_derivation = true;
        info.exact_replay_bitmap = true;
        info.integrity_verified = true;

        // Base: 30+15+15+15+10+0+5 = 90
        // TOFU bonus: +3 (replaces 0 from TOFU cert)
        // Key rotation: +5
        // Salted HKDF: +2
        // Exact replay: +2
        // Integrity: +3
        // Total: 90 + 3 + 5 + 2 + 2 + 3 = 105, capped at 100
        int score = info.getSecurityScore();
        UASSERT(score >= 100);

        // The string should show Excellent
        UASSERT(info.getSecurityScoreString().find("Excellent") != std::string::npos);
}

void TestSecurityScoreV99::testFirstConnectionWithECDHAndTOFUBonus()
{
        // First connection: TOFU with ECDH + TOFU bonus only (no other bonuses)
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;
        info.tofu_acknowledged = true;

        // Base: 30+15+15+15+10+0+5 = 90
        // TOFU: +3
        // Total: 93
        UASSERTEQ(int, info.getSecurityScore(), 93);
}

void TestSecurityScoreV99::testReturnConnectionWithPinningStill100()
{
        // Return connection: pinned cert + ECDH + TLS equiv (original 100/100 path)
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        // Original path: 30+15+15+15+10+10+5 = 100
        UASSERTEQ(int, info.getSecurityScore(), 100);
}

void TestSecurityScoreV99::testNoEncryptionWithBonusesIsStill0()
{
        // Bonuses should NOT apply when encryption is not active
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Insecure;
        info.tofu_acknowledged = true;
        info.key_rotation_capable = true;
        info.salted_key_derivation = true;
        info.exact_replay_bitmap = true;
        info.integrity_verified = true;

        // Score must still be 0 — bonuses only count for encrypted connections
        UASSERTEQ(int, info.getSecurityScore(), 0);
}

// ============================================================================
// Backwards Compatibility Tests
// ============================================================================

void TestSecurityScoreV99::testExisting100ScoreStillWorks()
{
        // The original v9.1 path to 100 should still work unchanged
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        UASSERTEQ(int, info.getSecurityScore(), 100);
}

void TestSecurityScoreV99::testBonusDoesNotReduceBaseScore()
{
        // A v9.1 configuration (no bonus fields set) should score the same as before
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;

        // v9.1 score: 30+15+15+15+10+0+0 = 85
        // v9.9 with no bonuses: should still be 85 (no TOFU bonus since tofu_acknowledged=false)
        UASSERTEQ(int, info.getSecurityScore(), 85);
}

void TestSecurityScoreV99::testBaseScoreMethodUnchanged()
{
        // getBaseSecurityScore() should return the same as v9.1 getSecurityScore()
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;

        // Base score should be 100 (v9.1 scoring)
        UASSERTEQ(int, info.getBaseSecurityScore(), 100);
}

// ============================================================================
// New Constants Tests
// ============================================================================

void TestSecurityScoreV99::testTofuAcknowledgedConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::BONUS_TOFU_ACKNOWLEDGED, 3);
}

void TestSecurityScoreV99::testKeyRotationCapableConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::BONUS_KEY_ROTATION, 5);
}

void TestSecurityScoreV99::testSaltedKeyDerivationConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::BONUS_SALTED_HKDF, 2);
}

void TestSecurityScoreV99::testExactReplayBitmapConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::BONUS_EXACT_REPLAY_BITMAP, 2);
}

void TestSecurityScoreV99::testIntegrityVerifiedConstant()
{
        UASSERTEQ(int, ConnectionSecurityInfo::BONUS_INTEGRITY_VERIFIED, 3);
}

// ============================================================================
// String Conversion Tests
// ============================================================================

void TestSecurityScoreV99::testTofuAcknowledgedString()
{
        UASSERT(ConnectionSecurityInfo::getCertificateStatusString(
                ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE) == "Trust On First Use");
}

void TestSecurityScoreV99::testKeyRotationCapableString()
{
        UASSERT(ConnectionSecurityInfo::getKeyRotationString(true) == "Supported");
        UASSERT(ConnectionSecurityInfo::getKeyRotationString(false) == "Not Supported");
}

// ============================================================================
// populateRealSecurityInfo with New Fields
// ============================================================================

void TestSecurityScoreV99::testPopulateRealSecurityInfoWithBonuses()
{
        // v9.9 populateRealSecurityInfo should set bonus fields
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                true,   // ecdh_completed
                false,  // fingerprint_pinned (TOFU)
                0,      // fingerprint_verify_result
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000);

        // When encryption is active with TOFU, tofu_acknowledged should be true
        UASSERT(info.tofu_acknowledged);

        // When encryption is active, integrity_verified starts true (no failures yet)
        UASSERT(info.integrity_verified);
}

void TestSecurityScoreV99::testPopulateRealSecurityInfoWithKeyRotation()
{
        // v9.9 populateRealSecurityInfo with key_rotation parameter
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                true,   // ecdh_completed
                true,   // fingerprint_pinned
                1,      // fingerprint_verify_result (match)
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000,
                true);  // key_rotation_supported (new v9.9 parameter)

        UASSERT(info.key_rotation_capable);
        UASSERT(info.salted_key_derivation);
}

void TestSecurityScoreV99::testPopulateRealSecurityInfoWithIntegrity()
{
        // When there are auth failures, integrity_verified should be false
        ConnectionSecurityInfo info = populateRealSecurityInfo(
                true,   // encryption_active
                true,   // ecdh_completed
                false,  // fingerprint_pinned
                0,      // fingerprint_verify_result
                "test-session", "SHA256:abc", 1700000000, 42,
                "game.example.com", 30000);

        // Initially no failures, so integrity is verified
        UASSERT(info.integrity_verified);

        // Simulate an auth failure
        info.c2s_auth_failures = 1;
        // Integrity should now be false (call updateIntegrityVerified)
        info.updateIntegrityVerified();
        UASSERT(!info.integrity_verified);
}

// ============================================================================
// Score Label Tests
// ============================================================================

void TestSecurityScoreV99::testScore100StillExcellent()
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

void TestSecurityScoreV99::testScore88WithBonusesLabel()
{
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;
        info.tofu_acknowledged = true;

        // 30+15+15+15+10+0+0+3 = 88
        UASSERT(info.getSecurityScoreString() == "88/100 (Good)");
}

// ============================================================================
// New Method Tests
// ============================================================================

void TestSecurityScoreV99::testGetSecurityScoreWithBonusesMethod()
{
        // getSecurityScore() now includes bonuses (capped at 100)
        ConnectionSecurityInfo info;
        info.state = ConnectionSecurity::Encrypted;
        info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
        info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        info.replay_protection = true;
        info.forward_secrecy = true;
        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;
        info.tofu_acknowledged = true;
        info.key_rotation_capable = true;
        info.salted_key_derivation = true;
        info.exact_replay_bitmap = true;
        info.integrity_verified = true;

        // Base: 100, Bonuses: 3+5+2+2+3=15, Total: 115 → capped at 100
        UASSERTEQ(int, info.getSecurityScore(), 100);

        // But getBaseSecurityScore() should be 100 (without bonuses)
        UASSERTEQ(int, info.getBaseSecurityScore(), 100);

        // getBonusScore() should be 15
        UASSERTEQ(int, info.getBonusScore(), 15);
}

void TestSecurityScoreV99::testGetBonusScoreMethod()
{
        ConnectionSecurityInfo info;
        // No bonuses set (and not encrypted → bonus is 0)
        UASSERTEQ(int, info.getBonusScore(), 0);

        // Enable all bonuses AND set encrypted state
        // (getBonusScore() returns 0 when !isSecure())
        info.state = ConnectionSecurity::Encrypted;
        info.tofu_acknowledged = true;
        info.key_rotation_capable = true;
        info.salted_key_derivation = true;
        info.exact_replay_bitmap = true;
        info.integrity_verified = true;

        // 3+5+2+2+3 = 15
        UASSERTEQ(int, info.getBonusScore(), 15);
}

void TestSecurityScoreV99::testGetBonusBreakdownMethod()
{
        ConnectionSecurityInfo info;
        // Must set encrypted state for breakdown to be populated
        info.state = ConnectionSecurity::Encrypted;
        info.tofu_acknowledged = true;
        info.key_rotation_capable = true;
        info.salted_key_derivation = true;
        info.exact_replay_bitmap = true;
        info.integrity_verified = true;

        auto breakdown = info.getBonusBreakdown();
        UASSERTEQ(int, breakdown.size(), 5);

        // Each entry should be a pair of (name, points)
        // Order: TOFU, KeyRotation, SaltedHKDF, ExactReplay, Integrity
        UASSERTEQ(int, breakdown[0].second, 3);  // TOFU
        UASSERTEQ(int, breakdown[1].second, 5);  // Key Rotation
        UASSERTEQ(int, breakdown[2].second, 2);  // Salted HKDF
        UASSERTEQ(int, breakdown[3].second, 2);  // Exact Replay
        UASSERTEQ(int, breakdown[4].second, 3);  // Integrity
}
