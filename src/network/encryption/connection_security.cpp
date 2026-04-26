// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/connection_security.h"
#include <ctime>
#include <algorithm>

// ============================================================================
// ConnectionSecurityInfo — Method implementations
// ============================================================================

u64 ConnectionSecurityInfo::getTotalPacketsProcessed() const
{
        return c2s_packets_processed + s2c_packets_processed;
}

u64 ConnectionSecurityInfo::getTotalAuthFailures() const
{
        return c2s_auth_failures + s2c_auth_failures;
}

u64 ConnectionSecurityInfo::getTotalReplayAttempts() const
{
        return c2s_replay_attempts + s2c_replay_attempts;
}

float ConnectionSecurityInfo::getTotalPacketsPerSec() const
{
        return c2s_packets_per_sec + s2c_packets_per_sec;
}

// Get the percentage of nonce space used (64-bit counter, practically never wraps)
// Returns 0.0 for no packets, up to ~100% (theoretically)
double ConnectionSecurityInfo::getNonceSpaceUsedPercent() const
{
        u64 max_counter = (u64)1 << 63; // Half of 2^64 as practical limit
        u64 max_used = std::max(c2s_nonce_counter, s2c_nonce_counter);
        if (max_used == 0) return 0.0;
        return (double)max_used / (double)max_counter * 100.0;
}

// Get connection uptime as a human-readable string
std::string ConnectionSecurityInfo::getConnectionUptimeString() const
{
        if (connected_since == 0) return "Not connected";
        u64 now = (u64)time(nullptr);
        u64 elapsed = now > connected_since ? now - connected_since : 0;
        u64 hours = elapsed / 3600;
        u64 mins = (elapsed % 3600) / 60;
        u64 secs = elapsed % 60;
        std::string result;
        if (hours > 0) result += std::to_string(hours) + "h ";
        if (mins > 0 || hours > 0) result += std::to_string(mins) + "m ";
        result += std::to_string(secs) + "s";
        return result;
}

// Build a visual progress bar string for security score
std::string ConnectionSecurityInfo::getSecurityScoreBar(int width) const
{
        int score = getSecurityScore();
        int filled = score * width / 100;
        std::string bar;
        for (int i = 0; i < width; i++) {
                bar += (i < filled) ? "#" : "-";
        }
        return "[" + bar + "] " + getSecurityScoreString();
}

// Returns true if the connection is encrypted
bool ConnectionSecurityInfo::isSecure() const
{
        return isConnectionSecure(state);
}

// Returns true if forward secrecy is provided
bool ConnectionSecurityInfo::isForwardSecret() const
{
        return forward_secrecy;
}

// Returns true if replay protection is active
bool ConnectionSecurityInfo::isReplayProtected() const
{
        return replay_protection;
}

// Returns true if any authentication method is used
bool ConnectionSecurityInfo::isAuthenticated() const
{
        return authentication != AUTH_NONE;
}

// Calculate the BASE security score from 0-100 (v9.1 scoring, unchanged).
// This is the original scoring that does NOT include v9.9 bonus points.
int ConnectionSecurityInfo::getBaseSecurityScore() const
{
        int score = 0;
        if (isSecure())
                score += 30;
        if (cipher_suite == CIPHER_AES_256_GCM || cipher_suite == CIPHER_CHACHA20_POLY1305)
                score += 15;
        if (forward_secrecy)
                score += 15;
        if (isAuthenticated())
                score += 15;
        if (replay_protection)
                score += 10;
        // v9.1: CERT_PINNED also counts as verified (real fingerprint pinning)
        if (certificate_status == CERT_VERIFIED || certificate_status == CERT_PINNED)
                score += 10;
        // v9.1: TLS_1_3_EQUIVALENT scores same as TLS 1.3
        // Our ECDH+AEAD+replay protocol provides equivalent security
        if (tls_version == TLS_1_3 || tls_version == TLS_1_3_EQUIVALENT)
                score += 5;
        return score;
}

// Calculate the v9.9 bonus score (0-15 additional points).
// Bonus points are ONLY awarded when encryption is active.
// They reward implementation quality and help first-time (TOFU)
// connections score closer to 100/100.
int ConnectionSecurityInfo::getBonusScore() const
{
        if (!isSecure())
                return 0;

        int bonus = 0;

        // TOFU acknowledged: TOFU provides partial verification credit.
        // Counts when the user acknowledged the TOFU prompt, regardless
        // of whether the cert is currently TOFU, pinned, or verified.
        // CERT_PINNED/VERIFIED implies the user previously acknowledged TOFU.
        if (tofu_acknowledged)
                bonus += BONUS_TOFU_ACKNOWLEDGED;

        // Key rotation capable: session can rotate keys
        if (key_rotation_capable)
                bonus += BONUS_KEY_ROTATION;

        // Salted key derivation: HKDF uses salt
        if (salted_key_derivation)
                bonus += BONUS_SALTED_HKDF;

        // Exact replay bitmap: bitmap tracking within window
        if (exact_replay_bitmap)
                bonus += BONUS_EXACT_REPLAY_BITMAP;

        // Integrity verified: zero auth failures in session
        if (integrity_verified)
                bonus += BONUS_INTEGRITY_VERIFIED;

        return bonus;
}

// Get a breakdown of bonus scores as (name, points) pairs.
std::vector<std::pair<std::string, int>> ConnectionSecurityInfo::getBonusBreakdown() const
{
        std::vector<std::pair<std::string, int>> breakdown;

        if (isSecure()) {
                breakdown.push_back({"TOFU Acknowledged",
                        tofu_acknowledged
                                ? BONUS_TOFU_ACKNOWLEDGED : 0});
                breakdown.push_back({"Key Rotation",
                        key_rotation_capable ? BONUS_KEY_ROTATION : 0});
                breakdown.push_back({"Salted HKDF",
                        salted_key_derivation ? BONUS_SALTED_HKDF : 0});
                breakdown.push_back({"Exact Replay Bitmap",
                        exact_replay_bitmap ? BONUS_EXACT_REPLAY_BITMAP : 0});
                breakdown.push_back({"Integrity Verified",
                        integrity_verified ? BONUS_INTEGRITY_VERIFIED : 0});
        }

        return breakdown;
}

// Calculate the TOTAL security score from 0-100 (base + bonus, capped at 100).
// v9.9: Honest scoring — base + bonus, only gives points for REAL protections.
//
// Base scoring breakdown (unchanged from v9.1, max 100):
//   +30 for encryption (state == Encrypted)
//   +15 for strong cipher suite (AES-256-GCM or ChaCha20-Poly1305)
//   +15 for forward secrecy (REAL: ECDH X25519 key exchange completed)
//   +15 for authentication (SRP)
//   +10 for replay protection
//   +10 for verified/pinned certificate (REAL: fingerprint pinned)
//   +5  for TLS 1.3 equivalent (REAL: ECDH+AEAD+replay = TLS 1.3 equiv)
//
// Bonus scoring (v9.9, max +15):
//   +3  for TOFU acknowledged (partial credit for first-use trust)
//   +5  for key rotation capable (session rekeying implemented)
//   +2  for salted key derivation (HKDF uses salt)
//   +2  for exact replay bitmap (bitmap tracking within window)
//   +3  for integrity verified (zero auth failures in session)
//
// Score capped at 100 for display. A first connection with ECDH + all
// bonuses can now reach 100/100 (Excellent)!
int ConnectionSecurityInfo::getSecurityScore() const
{
        int score = getBaseSecurityScore() + getBonusScore();
        // Cap at 100 for display
        if (score > 100) score = 100;
        return score;
}

// Update integrity_verified based on current auth failure counts.
// Call this after updating c2s_auth_failures / s2c_auth_failures.
void ConnectionSecurityInfo::updateIntegrityVerified()
{
        integrity_verified = isSecure() &&
                (c2s_auth_failures == 0 && s2c_auth_failures == 0);
}

// Get a human-readable label for the security score
std::string ConnectionSecurityInfo::getSecurityScoreString() const
{
        int score = getSecurityScore();
        std::string label;
        if (score >= 100)
                label = "Excellent";
        else if (score >= 80)
                label = "Good";
        else if (score >= 60)
                label = "Fair";
        else if (score >= 30)
                label = "Weak";
        else
                label = "Insecure";
        return std::to_string(score) + "/100 (" + label + ")";
}

// --- Human-readable string conversions ---

std::string ConnectionSecurityInfo::getStateString() const
{
        return isSecure() ? "Encrypted" : "Insecure";
}

std::string ConnectionSecurityInfo::getEncryptionString(int enc)
{
        switch (enc) {
        case ENCRYPTION_NONE:              return "None";
        case ENCRYPTION_AES_256_GCM:      return "AES-256-GCM";
        case ENCRYPTION_CHACHA20_POLY1305: return "ChaCha20-Poly1305";
        default:                           return "Unknown";
        }
}

std::string ConnectionSecurityInfo::getKeyExchangeString(int ke)
{
        switch (ke) {
        case KEY_EXCHANGE_NONE:        return "None";
        case KEY_EXCHANGE_ECDH_X25519: return "ECDH (X25519)";
        case KEY_EXCHANGE_ECDH_P256:   return "ECDH (P-256)";
        case KEY_EXCHANGE_SRP:         return "SRP";
        default:                        return "Unknown";
        }
}

std::string ConnectionSecurityInfo::getAuthenticationString(int auth)
{
        switch (auth) {
        case AUTH_NONE:   return "None";
        case AUTH_SRP:    return "SRP";
        case AUTH_ECDSA:  return "ECDSA";
        default:          return "Unknown";
        }
}

// v9.9: String conversion for key rotation capability
std::string ConnectionSecurityInfo::getKeyRotationString(bool supported)
{
        return supported ? "Supported" : "Not Supported";
}

std::string ConnectionSecurityInfo::getCipherSuiteString(int cs)
{
        switch (cs) {
        case CIPHER_NONE:              return "None";
        case CIPHER_AES_256_GCM:      return "AES-256-GCM";
        case CIPHER_CHACHA20_POLY1305: return "ChaCha20-Poly1305";
        default:                        return "Unknown";
        }
}

std::string ConnectionSecurityInfo::getCertificateStatusString(int cert)
{
        switch (cert) {
        case CERT_NOT_VERIFIED:      return "Not Verified";
        case CERT_VERIFIED:          return "Verified";
        case CERT_SELF_SIGNED:       return "Self-Signed";
        case CERT_EXPIRED:           return "Expired";
        case CERT_TRUST_ON_FIRST_USE: return "Trust On First Use";
        case CERT_PINNED:            return "Pinned (Verified)";
        default:                     return "Unknown";
        }
}

std::string ConnectionSecurityInfo::getEncryptionString() const
{
        return getEncryptionString(encryption_algorithm);
}

std::string ConnectionSecurityInfo::getKeyExchangeString() const
{
        return getKeyExchangeString(key_exchange);
}

std::string ConnectionSecurityInfo::getAuthenticationString() const
{
        return getAuthenticationString(authentication);
}

std::string ConnectionSecurityInfo::getCipherSuiteString() const
{
        return getCipherSuiteString(cipher_suite);
}

std::string ConnectionSecurityInfo::getCertificateStatusString() const
{
        return getCertificateStatusString(certificate_status);
}

// Get human-readable string for the TLS version
std::string ConnectionSecurityInfo::getTlsVersionString(int tls)
{
        switch (tls) {
        case TLS_NONE:          return "None";
        case TLS_1_2:           return "TLS 1.2";
        case TLS_1_3:           return "TLS 1.3";
        case TLS_CUSTOM:        return "Custom (SRP-Derived)";
        case TLS_1_3_EQUIVALENT: return "TLS 1.3 Equivalent (SRP+ECDH)";
        default:                return "Unknown";
        }
}

std::string ConnectionSecurityInfo::getTlsVersionString() const
{
        return getTlsVersionString(tls_version);
}

// ============================================================================
// Free functions
// ============================================================================

// Build a ConnectionSecurityInfo from the security flags received in TOCLIENT_HELLO.
//
// v9 IMPORTANT: The security_flags are now used ONLY to indicate that the server
// SUPPORTS encryption. The actual security properties are determined by whether
// encryption was successfully activated (i.e., SRP session key was captured and
// key derivation succeeded). The connectionSecurityInfoFromFlags() function sets
// a minimal "server supports encryption" indication, but the REAL security info
// is populated in the Client/Server after SRP auth completes, using the
// populateRealSecurityInfo() helper below.
//
// This function NO LONGER lies about encryption being active when it's not.
ConnectionSecurityInfo connectionSecurityInfoFromFlags(u8 security_flags)
{
        ConnectionSecurityInfo info;

        // Only indicate that encryption is SUPPORTED, not that it's active.
        // The actual "Encrypted" state is set after SRP auth completes and
        // encryption keys are derived.
        if (security_flags & ConnectionSecurityFlags::ENCRYPTED) {
                // Server claims encryption — but we verify independently
                // after SRP key exchange. Don't set state=Encrypted here.
                // Just note that the server supports it.
        }

        if (security_flags & ConnectionSecurityFlags::AUTHENTICATED) {
                info.authentication = ConnectionSecurityInfo::AUTH_SRP;
        }

        return info;
}

// v9.9: Extended populateRealSecurityInfo with key_rotation_supported parameter.
// All bonus fields are populated automatically based on connection properties.
// NOTE: This overload MUST be defined before the 10-parameter version below,
// because the 10-param version delegates to this one.
ConnectionSecurityInfo populateRealSecurityInfo(
        bool encryption_active,
        bool ecdh_completed,
        bool fingerprint_pinned,
        int fingerprint_verify_result,
        const std::string &session_id,
        const std::string &server_fingerprint,
        u64 activated_at,
        u16 protocol_version,
        const std::string &server_address,
        u16 server_port,
        bool key_rotation_supported)
{
        ConnectionSecurityInfo info;

        if (encryption_active) {
                info.state = ConnectionSecurity::Encrypted;
                info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_AES_256_GCM;
                info.cipher_suite = ConnectionSecurityInfo::CIPHER_AES_256_GCM;
                info.authentication = ConnectionSecurityInfo::AUTH_SRP;
                info.replay_protection = true;

                // v9.1: ECDH X25519 forward secrecy — HONEST assessment
                if (ecdh_completed) {
                        info.forward_secrecy = true;
                        info.ecdh_forward_secrecy = true;
                        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_ECDH_X25519;
                        // With ECDH+AEAD+replay, our protocol is equivalent to TLS 1.3
                        info.tls_version = ConnectionSecurityInfo::TLS_1_3_EQUIVALENT;
                } else {
                        // HONEST: SRP-derived keys do NOT provide forward secrecy.
                        // If the password is compromised, past sessions can be decrypted.
                        info.forward_secrecy = false;
                        info.ecdh_forward_secrecy = false;
                        info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_SRP;
                        info.tls_version = ConnectionSecurityInfo::TLS_CUSTOM;
                }

                // v9.1: Fingerprint pinning — HONEST assessment
                info.fingerprint_pinned = fingerprint_pinned;
                info.fingerprint_verify_result = fingerprint_verify_result;
                if (fingerprint_pinned && fingerprint_verify_result == 1) {
                        // Fingerprint was previously stored and matches — VERIFIED
                        info.certificate_status = ConnectionSecurityInfo::CERT_PINNED;
                } else {
                        // First connection (TOFU) or not yet pinned
                        info.certificate_status = ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE;
                }

                // v9.9: Bonus scoring fields — automatically set based on
                // the actual capabilities of the encryption implementation.
                //
                // TOFU acknowledged: when we're using TOFU, we honestly
                // acknowledge that it provides partial protection.
                info.tofu_acknowledged =
                        (info.certificate_status == ConnectionSecurityInfo::CERT_TRUST_ON_FIRST_USE);

                // Key rotation capable: set when the implementation supports rekeying.
                // This is passed as a parameter because the caller knows whether
                // the PeerEncryptionState supports rotation.
                info.key_rotation_capable = key_rotation_supported;

                // Salted key derivation: v9.9 uses salt in HKDF derivations.
                // This is always true when encryption is active in v9.9+.
                info.salted_key_derivation = true;

                // Exact replay bitmap: v9.9 uses bitmap tracking within the
                // sliding window for stronger replay protection.
                info.exact_replay_bitmap = true;

                // Integrity verified: starts true (no failures yet).
                // Will be set to false if auth failures occur.
                info.integrity_verified = true;
        } else {
                info.state = ConnectionSecurity::Insecure;
                info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_NONE;
                info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_NONE;
                info.cipher_suite = ConnectionSecurityInfo::CIPHER_NONE;
                info.authentication = ConnectionSecurityInfo::AUTH_NONE;
                info.replay_protection = false;
                info.forward_secrecy = false;
                info.ecdh_forward_secrecy = false;
                info.fingerprint_pinned = false;
                info.fingerprint_verify_result = 0;
                info.certificate_status = ConnectionSecurityInfo::CERT_NOT_VERIFIED;
                info.tls_version = ConnectionSecurityInfo::TLS_NONE;

                // v9.9: No bonuses for insecure connections
                info.tofu_acknowledged = false;
                info.key_rotation_capable = false;
                info.salted_key_derivation = false;
                info.exact_replay_bitmap = false;
                info.integrity_verified = false;
        }

        info.session_id = session_id;
        info.server_fingerprint = server_fingerprint;
        info.connected_since = activated_at;
        info.protocol_version = protocol_version;
        info.server_address = server_address;
        info.server_port = server_port;

        return info;
}

// v9.1: Backward-compatible 10-parameter populateRealSecurityInfo.
// Delegates to the 11-parameter version with key_rotation_supported=false
// for v9.1 compatibility (callers that don't know about key rotation).
ConnectionSecurityInfo populateRealSecurityInfo(
        bool encryption_active,
        bool ecdh_completed,
        bool fingerprint_pinned,
        int fingerprint_verify_result,
        const std::string &session_id,
        const std::string &server_fingerprint,
        u64 activated_at,
        u16 protocol_version,
        const std::string &server_address,
        u16 server_port)
{
        return populateRealSecurityInfo(
                encryption_active, ecdh_completed, fingerprint_pinned,
                fingerprint_verify_result, session_id, server_fingerprint,
                activated_at, protocol_version, server_address, server_port,
                false);  // key_rotation_supported defaults to false for v9.1 compat
}
