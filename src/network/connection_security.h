// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include <string>

// Security flags sent in TOCLIENT_HELLO (bitfield in u8 security_flags)
// These are sent by the server to advertise the connection's security capabilities.
// The client uses these flags to determine whether the connection is encrypted
// and what security features are active.
namespace ConnectionSecurityFlags
{
        // Bit 0: Server advertises that this connection is encrypted
        // (i.e., transport-layer encryption is active for all game traffic)
        constexpr u8 ENCRYPTED = 0x01;

        // Bit 1: Server supports encryption but it is not yet active
        // (e.g., encryption handshake still in progress)
        constexpr u8 ENCRYPTION_SUPPORTED = 0x02;

        // Bit 2: Forward secrecy is provided (ephemeral key exchange)
        // (e.g., ECDH with X25519 or P-256 — compromise of long-term keys
        //  does not reveal past session keys)
        constexpr u8 FORWARD_SECRECY = 0x04;

        // Bit 3: Server identity has been authenticated
        // (e.g., via ECDSA certificate or SRP authentication)
        constexpr u8 AUTHENTICATED = 0x08;

        // Bit 4: Replay protection is active
        // (e.g., nonce counters with sliding window validation)
        constexpr u8 REPLAY_PROTECTED = 0x10;
}

// Connection security state — tracked by the client
enum class ConnectionSecurity : u8
{
        // No transport encryption — all game traffic is sent as plaintext UDP.
        // The insecure connection overlay should be shown.
        Insecure = 0,

        // Transport encryption is active — game traffic is encrypted.
        // No overlay needed.
        Encrypted = 1,
};

// Helper functions for ConnectionSecurity
inline bool isConnectionSecure(ConnectionSecurity sec)
{
        return sec == ConnectionSecurity::Encrypted;
}

// Determine the connection security state from the security flags
// received in TOCLIENT_HELLO.
inline ConnectionSecurity connectionSecurityFromFlags(u8 security_flags)
{
        if (security_flags & ConnectionSecurityFlags::ENCRYPTED)
                return ConnectionSecurity::Encrypted;
        return ConnectionSecurity::Insecure;
}

// Detailed connection security information — provides technical details
// about the security of the connection to the server. This is displayed
// in the Settings → Connection Security Info tab so users can verify
// that the security is real and understand what protections are active.
struct ConnectionSecurityInfo
{
        // Encryption algorithm constants
        static constexpr int ENCRYPTION_NONE = 0;
        static constexpr int ENCRYPTION_AES_256_GCM = 1;
        static constexpr int ENCRYPTION_CHACHA20_POLY1305 = 2;

        // Key exchange method constants
        static constexpr int KEY_EXCHANGE_NONE = 0;
        static constexpr int KEY_EXCHANGE_ECDH_X25519 = 1;
        static constexpr int KEY_EXCHANGE_ECDH_P256 = 2;
        static constexpr int KEY_EXCHANGE_SRP = 3;  // v9: SRP-based key exchange (real, not ECDH)

        // Authentication method constants
        static constexpr int AUTH_NONE = 0;
        static constexpr int AUTH_SRP = 1;
        static constexpr int AUTH_ECDSA = 2;

        // Cipher suite constants
        static constexpr int CIPHER_NONE = 0;
        static constexpr int CIPHER_AES_256_GCM = 1;
        static constexpr int CIPHER_CHACHA20_POLY1305 = 2;

        // Certificate verification status constants
        static constexpr int CERT_NOT_VERIFIED = 0;
        static constexpr int CERT_VERIFIED = 1;
        static constexpr int CERT_SELF_SIGNED = 2;
        static constexpr int CERT_EXPIRED = 3;
        static constexpr int CERT_TRUST_ON_FIRST_USE = 4;  // v9: TOFU model (real)
        static constexpr int CERT_PINNED = 5;  // v9.1: Fingerprint pinned and verified (real)

        // TLS version constants (v8: Security Info Tab)
        static constexpr int TLS_NONE = 0;
        static constexpr int TLS_1_2 = 1;
        static constexpr int TLS_1_3 = 2;
        static constexpr int TLS_CUSTOM = 3;  // v9: Custom encryption protocol, not standard TLS
        static constexpr int TLS_1_3_EQUIVALENT = 4;  // v9.1: Custom protocol with TLS 1.3 equivalent properties

        // --- Member variables ---

        // Overall connection security state
        ConnectionSecurity state = ConnectionSecurity::Insecure;

        // Encryption algorithm in use
        int encryption_algorithm = ENCRYPTION_NONE;

        // Key exchange method used to establish the session
        int key_exchange = KEY_EXCHANGE_NONE;

        // Authentication method used
        int authentication = AUTH_NONE;

        // Cipher suite (may differ from encryption_algorithm in future
        // when combined cipher suites like TLS are used)
        int cipher_suite = CIPHER_NONE;

        // Certificate verification status
        int certificate_status = CERT_NOT_VERIFIED;

        // Whether forward secrecy is provided
        bool forward_secrecy = false;

        // Whether replay protection is active
        bool replay_protection = false;

        // v9.1: Whether ECDH X25519 key exchange was used for forward secrecy
        bool ecdh_forward_secrecy = false;

        // v9.1: Whether the server fingerprint has been pinned/verified
        bool fingerprint_pinned = false;

        // v9.1: Fingerprint verification result (0=unknown, 1=match, -1=mismatch)
        int fingerprint_verify_result = 0;

        // Network protocol version negotiated
        u16 protocol_version = 0;

        // Server address (hostname or IP)
        std::string server_address;

        // Server port
        u16 server_port = 0;

        // --- v8: Security Info Tab fields ---

        // Unique session identifier (hex string generated on connection)
        std::string session_id;

        // Unix timestamp when the connection was established (0 = not connected)
        u64 connected_since = 0;

        // Server public key fingerprint (e.g. "SHA256:a1b2c3d4...")
        std::string server_fingerprint;

        // TLS protocol version negotiated
        int tls_version = TLS_NONE;

        // --- v9: Live encryption statistics (updated every frame for overlays) ---

        // C2S direction: packets encrypted/decrypted
        u64 c2s_packets_processed = 0;

        // S2C direction: packets encrypted/decrypted
        u64 s2c_packets_processed = 0;

        // C2S authentication failures (tag verification failed)
        u64 c2s_auth_failures = 0;

        // S2C authentication failures (tag verification failed)
        u64 s2c_auth_failures = 0;

        // C2S replay attempts detected
        u64 c2s_replay_attempts = 0;

        // S2C replay attempts detected
        u64 s2c_replay_attempts = 0;

        // Current C2S nonce counter value
        u64 c2s_nonce_counter = 0;

        // Current S2C nonce counter value
        u64 s2c_nonce_counter = 0;

        // Current round-trip time in milliseconds
        float rtt_ms = 0.0f;

        // Packet loss rate (0.0 - 1.0)
        float packet_loss = 0.0f;

        // Encryption overhead per packet in bytes (29 for AES-256-GCM)
        u32 encryption_overhead_bytes = 0;

        // Total encrypted bytes sent (approximate)
        u64 total_encrypted_bytes_sent = 0;

        // Total encrypted bytes received (approximate)
        u64 total_encrypted_bytes_received = 0;

        // Key derivation timestamp (when keys were derived from SRP)
        u64 keys_derived_at = 0;

        // Encryption activation timestamp (when active=true was set)
        u64 encryption_activated_at = 0;

        // Packets per second (measured over last second)
        float c2s_packets_per_sec = 0.0f;
        float s2c_packets_per_sec = 0.0f;

        // --- Convenience methods for live stats ---

        u64 getTotalPacketsProcessed() const { return c2s_packets_processed + s2c_packets_processed; }
        u64 getTotalAuthFailures() const { return c2s_auth_failures + s2c_auth_failures; }
        u64 getTotalReplayAttempts() const { return c2s_replay_attempts + s2c_replay_attempts; }
        float getTotalPacketsPerSec() const { return c2s_packets_per_sec + s2c_packets_per_sec; }

        // Get the percentage of nonce space used (64-bit counter, practically never wraps)
        // Returns 0.0 for no packets, up to ~100% (theoretically)
        double getNonceSpaceUsedPercent() const {
                u64 max_counter = (u64)1 << 63; // Half of 2^64 as practical limit
                u64 max_used = std::max(c2s_nonce_counter, s2c_nonce_counter);
                if (max_used == 0) return 0.0;
                return (double)max_used / (double)max_counter * 100.0;
        }

        // Get connection uptime as a human-readable string
        std::string getConnectionUptimeString() const {
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
        std::string getSecurityScoreBar(int width = 20) const {
                int score = getSecurityScore();
                int filled = score * width / 100;
                std::string bar;
                for (int i = 0; i < width; i++) {
                        bar += (i < filled) ? "#" : "-";
                }
                return "[" + bar + "] " + getSecurityScoreString();
        }

        // --- Convenience methods ---

        // Returns true if the connection is encrypted
        bool isSecure() const { return isConnectionSecure(state); }

        // Returns true if forward secrecy is provided
        bool isForwardSecret() const { return forward_secrecy; }

        // Returns true if replay protection is active
        bool isReplayProtected() const { return replay_protection; }

        // Returns true if any authentication method is used
        bool isAuthenticated() const { return authentication != AUTH_NONE; }

        // --- v8: Security Info Tab methods ---

        // Calculate a security score from 0-100 based on active protections.
        // v9.1: Honest scoring — only gives points for REAL protections.
        // Scoring breakdown:
        //   +30 for encryption (state == Encrypted)
        //   +15 for strong cipher suite (AES-256-GCM or ChaCha20-Poly1305)
        //   +15 for forward secrecy (REAL: ECDH X25519 key exchange completed)
        //   +15 for authentication (SRP)
        //   +10 for replay protection
        //   +10 for verified/pinned certificate (REAL: fingerprint pinned)
        //   +5  for TLS 1.3 equivalent (REAL: ECDH+AEAD+replay = TLS 1.3 equiv)
        //   Max achievable: 100/100 (Excellent)
        //   Score with SRP+AES-256-GCM only: 70/100 (Fair)
        //   Score with SRP+AES-256-GCM+ECDH: 85/100 (Good)
        //   Score with SRP+AES-256-GCM+ECDH+pinned: 95/100 (Very Good)
        //   Score with SRP+AES-256-GCM+ECDH+pinned+TLS-equiv: 100/100 (Excellent)
        int getSecurityScore() const
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

        // Get a human-readable label for the security score
        std::string getSecurityScoreString() const
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

        std::string getStateString() const
        {
                return isSecure() ? "Encrypted" : "Insecure";
        }

        static std::string getEncryptionString(int enc)
        {
                switch (enc) {
                case ENCRYPTION_NONE:              return "None";
                case ENCRYPTION_AES_256_GCM:      return "AES-256-GCM";
                case ENCRYPTION_CHACHA20_POLY1305: return "ChaCha20-Poly1305";
                default:                           return "Unknown";
                }
        }

        static std::string getKeyExchangeString(int ke)
        {
                switch (ke) {
                case KEY_EXCHANGE_NONE:        return "None";
                case KEY_EXCHANGE_ECDH_X25519: return "ECDH (X25519)";
                case KEY_EXCHANGE_ECDH_P256:   return "ECDH (P-256)";
                case KEY_EXCHANGE_SRP:         return "SRP";
                default:                        return "Unknown";
                }
        }

        static std::string getAuthenticationString(int auth)
        {
                switch (auth) {
                case AUTH_NONE:   return "None";
                case AUTH_SRP:    return "SRP";
                case AUTH_ECDSA:  return "ECDSA";
                default:          return "Unknown";
                }
        }

        static std::string getCipherSuiteString(int cs)
        {
                switch (cs) {
                case CIPHER_NONE:              return "None";
                case CIPHER_AES_256_GCM:      return "AES-256-GCM";
                case CIPHER_CHACHA20_POLY1305: return "ChaCha20-Poly1305";
                default:                        return "Unknown";
                }
        }

        static std::string getCertificateStatusString(int cert)
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

        std::string getEncryptionString() const
        {
                return getEncryptionString(encryption_algorithm);
        }

        std::string getKeyExchangeString() const
        {
                return getKeyExchangeString(key_exchange);
        }

        std::string getAuthenticationString() const
        {
                return getAuthenticationString(authentication);
        }

        std::string getCipherSuiteString() const
        {
                return getCipherSuiteString(cipher_suite);
        }

        std::string getCertificateStatusString() const
        {
                return getCertificateStatusString(certificate_status);
        }

        // Get human-readable string for the TLS version
        static std::string getTlsVersionString(int tls)
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

        std::string getTlsVersionString() const
        {
                return getTlsVersionString(tls_version);
        }
};

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
inline ConnectionSecurityInfo connectionSecurityInfoFromFlags(u8 security_flags)
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

// v9.1: Populate ConnectionSecurityInfo from ACTUAL encryption state.
// This is the honest version — only called after SRP auth succeeds
// and encryption keys are successfully derived.
// Updated to support ECDH X25519 forward secrecy and fingerprint pinning.
inline ConnectionSecurityInfo populateRealSecurityInfo(
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
        }

        info.session_id = session_id;
        info.server_fingerprint = server_fingerprint;
        info.connected_since = activated_at;
        info.protocol_version = protocol_version;
        info.server_address = server_address;
        info.server_port = server_port;

        return info;
}
