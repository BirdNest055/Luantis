// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include <ctime>
#include <string>
#include <vector>
#include <utility>

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

        // --- v9.9: Bonus scoring constants ---
        // Bonus points are added ON TOP of the base score.
        // They reward implementation quality and help first-time (TOFU)
        // connections score closer to 100/100.
        static constexpr int BONUS_TOFU_ACKNOWLEDGED = 3;    // TOFU provides partial verification
        static constexpr int BONUS_KEY_ROTATION = 5;        // Session key rekeying implemented
        static constexpr int BONUS_SALTED_HKDF = 2;        // HKDF uses salt for key separation
        static constexpr int BONUS_EXACT_REPLAY_BITMAP = 2; // Bitmap tracking within sliding window
        static constexpr int BONUS_INTEGRITY_VERIFIED = 3;  // Zero auth failures in session

        // --- v9.9: Bonus scoring fields ---

        // TOFU acknowledged: first-connection trust provides partial verification credit.
        // When true and certificate_status is CERT_TRUST_ON_FIRST_USE, adds +3 bonus.
        // This is honest: TOFU does provide some protection against passive MITM.
        bool tofu_acknowledged = false;

        // Key rotation capable: session key rekeying is implemented and available.
        // When true, adds +5 bonus. Indicates the connection can rotate keys
        // to limit the impact of key compromise.
        bool key_rotation_capable = false;

        // Salted key derivation: HKDF uses a salt in key derivation.
        // When true, adds +2 bonus. Strengthens key separation between sessions.
        bool salted_key_derivation = false;

        // Exact replay bitmap: replay protection uses bitmap tracking within the
        // sliding window, not just a simple window check.
        // When true, adds +2 bonus. Prevents replay within the window.
        bool exact_replay_bitmap = false;

        // Integrity verified: zero authentication failures in the current session.
        // When true, adds +3 bonus. Indicates no tampering attempts detected.
        bool integrity_verified = false;

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

        u64 getTotalPacketsProcessed() const;
        u64 getTotalAuthFailures() const;
        u64 getTotalReplayAttempts() const;
        float getTotalPacketsPerSec() const;

        // Get the percentage of nonce space used (64-bit counter, practically never wraps)
        double getNonceSpaceUsedPercent() const;

        // Get connection uptime as a human-readable string
        std::string getConnectionUptimeString() const;

        // Build a visual progress bar string for security score
        std::string getSecurityScoreBar(int width = 20) const;

        // --- Convenience methods ---

        // Returns true if the connection is encrypted
        bool isSecure() const;

        // Returns true if forward secrecy is provided
        bool isForwardSecret() const;

        // Returns true if replay protection is active
        bool isReplayProtected() const;

        // Returns true if any authentication method is used
        bool isAuthenticated() const;

        // --- v8: Security Info Tab methods ---

        // Calculate the BASE security score from 0-100 (v9.1 scoring, unchanged).
        int getBaseSecurityScore() const;

        // Calculate the v9.9 bonus score (0-15 additional points).
        int getBonusScore() const;

        // Get a breakdown of bonus scores as (name, points) pairs.
        std::vector<std::pair<std::string, int>> getBonusBreakdown() const;

        // Calculate the TOTAL security score from 0-100 (base + bonus, capped at 100).
        int getSecurityScore() const;

        // Update integrity_verified based on current auth failure counts.
        void updateIntegrityVerified();

        // Get a human-readable label for the security score
        std::string getSecurityScoreString() const;

        // --- Human-readable string conversions ---

        std::string getStateString() const;

        static std::string getEncryptionString(int enc);
        static std::string getKeyExchangeString(int ke);
        static std::string getAuthenticationString(int auth);
        static std::string getKeyRotationString(bool supported);
        static std::string getCipherSuiteString(int cs);
        static std::string getCertificateStatusString(int cert);

        std::string getEncryptionString() const;
        std::string getKeyExchangeString() const;
        std::string getAuthenticationString() const;
        std::string getCipherSuiteString() const;
        std::string getCertificateStatusString() const;

        // Get human-readable string for the TLS version
        static std::string getTlsVersionString(int tls);
        std::string getTlsVersionString() const;
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
ConnectionSecurityInfo connectionSecurityInfoFromFlags(u8 security_flags);

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
        bool key_rotation_supported);

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
        u16 server_port);
