// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include "threading/mutex_auto_lock.h"
#include <atomic>
#include <mutex>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <map>
#include <functional>

// Cryptographic primitives for real transport encryption.
// Uses OpenSSL 3.0+ EVP APIs for AES-256-GCM encryption,
// HKDF-SHA256 key derivation, and secure random number generation.

/// Size of an AES-256 key in bytes
constexpr size_t AES256_KEY_SIZE = 32;

/// Size of a GCM nonce (IV) in bytes — standard for AES-GCM
constexpr size_t GCM_NONCE_SIZE = 12;

/// Size of a GCM authentication tag in bytes
constexpr size_t GCM_TAG_SIZE = 16;

/// Size of the nonce base derived from HKDF (per-direction uniqueness)
constexpr size_t NONCE_BASE_SIZE = 4;

/// Size of the nonce counter portion (8 bytes = 64-bit counter)
constexpr size_t NONCE_COUNTER_SIZE = 8;

/// Size of the SRP session key in bytes (SHA-256 output)
constexpr size_t SRP_SESSION_KEY_SIZE = 32;

/// Encrypted packet overhead: encrypted_flag(1) + nonce(12) + tag(16) = 29 bytes
constexpr size_t ENCRYPTED_PACKET_OVERHEAD = 1 + GCM_NONCE_SIZE + GCM_TAG_SIZE;

/// Encrypted flag byte values (prepended after base header)
/// Value 0x80 is chosen because it cannot be a valid PacketType (0-3),
/// so there is no ambiguity between encrypted and plaintext packets.
constexpr u8 ENCRYPTED_FLAG_PLAINTEXT = 0x00;
constexpr u8 ENCRYPTED_FLAG_AES_256_GCM = 0x80;

/// Result of an encryption/decryption operation
struct CryptoResult
{
        bool success = false;
        std::string error_msg;

        // For encryption: the ciphertext + tag
        // For decryption: the plaintext
        std::vector<u8> data;

        // The nonce used (for encryption, this is generated; for decryption, extracted)
        std::array<u8, GCM_NONCE_SIZE> nonce{};

        // The GCM authentication tag
        std::array<u8, GCM_TAG_SIZE> tag{};
};

/// AES-256-GCM authenticated encryption.
///
/// Provides both confidentiality and integrity:
/// - Confidentiality: AES-256 in GCM mode encrypts the plaintext
/// - Integrity: GCM authentication tag detects any tampering
/// - Nonce: 12-byte nonce, must NEVER be reused with the same key
///
/// Usage:
///   Encrypt: aes256gcm_encrypt(key, nonce, plaintext, aad) -> ciphertext + tag
///   Decrypt: aes256gcm_decrypt(key, nonce, ciphertext, tag, aad) -> plaintext
///
/// The nonce is typically constructed from a per-direction base (4 bytes from HKDF)
/// plus a monotonically increasing 8-byte counter. This ensures nonce uniqueness
/// as long as the counter never wraps around (2^64 packets per direction).

/// Encrypt data using AES-256-GCM.
///
/// @param key       32-byte AES-256 key
/// @param nonce     12-byte nonce (MUST be unique per key — never reuse!)
/// @param plaintext Data to encrypt
/// @param aad       Additional Authenticated Data (encrypted flag byte, not encrypted but authenticated)
/// @return CryptoResult with ciphertext and authentication tag on success
CryptoResult aes256gcm_encrypt(
        const u8* key, size_t key_len,
        const u8* nonce, size_t nonce_len,
        const u8* plaintext, size_t plaintext_len,
        const u8* aad, size_t aad_len);

/// Decrypt data using AES-256-GCM.
///
/// @param key        32-byte AES-256 key
/// @param nonce      12-byte nonce used during encryption
/// @param ciphertext Encrypted data
/// @param tag        16-byte GCM authentication tag
/// @param aad        Additional Authenticated Data (must match what was used during encryption)
/// @return CryptoResult with plaintext on success (success=false if tag verification fails)
CryptoResult aes256gcm_decrypt(
        const u8* key, size_t key_len,
        const u8* nonce, size_t nonce_len,
        const u8* ciphertext, size_t ciphertext_len,
        const u8* tag, size_t tag_len,
        const u8* aad, size_t aad_len);

/// HKDF-SHA256 key derivation function (RFC 5869).
///
/// Derives cryptographically independent key material from a shared secret.
/// Used to derive separate C2S and S2C encryption keys from the SRP session key,
/// as well as per-direction nonce bases.
///
/// @param ikm       Input Keying Material (e.g., SRP session key)
/// @param ikm_len   Length of IKM
/// @param salt      Optional salt (can be empty for first derivation)
/// @param salt_len  Length of salt
/// @param info      Context-specific info string (e.g., "Luanti v9 C2S Key")
/// @param info_len  Length of info
/// @param out       Output buffer for derived key material
/// @param out_len   Desired output length (typically 32 or 4 bytes)
/// @return true on success, false on failure
bool hkdf_sha256(
        const u8* ikm, size_t ikm_len,
        const u8* salt, size_t salt_len,
        const u8* info, size_t info_len,
        u8* out, size_t out_len);

/// Generate cryptographically secure random bytes.
///
/// Uses OpenSSL's RAND_bytes for cryptographic-quality randomness.
/// Used for generating nonce bases, session IDs, and other security values.
///
/// @param buf  Output buffer
/// @param len  Number of random bytes to generate
/// @return true on success, false on failure
bool secure_random(u8* buf, size_t len);

/// Build a 12-byte GCM nonce from a 4-byte base and 8-byte counter.
///
/// Nonce format: [base(4 bytes)][counter(8 bytes, big-endian)]
/// The base is derived from HKDF and is unique per direction per session.
/// The counter is monotonically increasing and never reused.
///
/// @param base     4-byte nonce base
/// @param counter  8-byte monotonic counter
/// @param nonce    Output 12-byte nonce
void build_nonce(const u8* base, u64 counter, u8* nonce);

/// Per-direction encryption state for a single peer connection.
///
/// Each direction (C2S and S2C) has its own key, nonce base, and counter.
/// This ensures that the same nonce is NEVER used twice for the same key,
/// which is a critical requirement for AES-GCM security.
struct DirectionalEncryptionState
{
        /// AES-256 encryption key for this direction
        std::array<u8, AES256_KEY_SIZE> key{};

        /// Nonce base (4 bytes, derived from HKDF, unique per direction)
        std::array<u8, NONCE_BASE_SIZE> nonce_base{};

        /// Monotonically increasing nonce counter (never wraps, never reuses)
        u64 nonce_counter = 0;

        /// Number of packets encrypted/decrypted in this direction
        u64 packets_processed = 0;

        /// Number of authentication failures in this direction
        u64 auth_failures = 0;

        /// Number of replay attempts detected in this direction
        u64 replay_attempts = 0;

        // --- v9.9: Exact replay bitmap ---
        // Tracks which counters within the sliding window have been seen.
        // Each bit represents one counter position within the window.
        // This prevents replay attacks WITHIN the sliding window,
        // not just outside it.
        static constexpr size_t REPLAY_WINDOW_SIZE = 64;
        static constexpr size_t REPLAY_BITMAP_WORDS = REPLAY_WINDOW_SIZE / (sizeof(u64) * 8);
        std::array<u64, REPLAY_BITMAP_WORDS> replay_bitmap{};

        /// Build the next nonce for this direction.
        /// Increments the counter after building.
        /// @return The 12-byte nonce to use for the next packet
        std::array<u8, GCM_NONCE_SIZE> nextNonce()
        {
                std::array<u8, GCM_NONCE_SIZE> nonce{};
                build_nonce(nonce_base.data(), nonce_counter, nonce.data());
                nonce_counter++;
                packets_processed++;
                return nonce;
        }

        /// Mark a received counter as seen in the replay bitmap.
        /// Call this when a packet passes the initial isNotReplay check.
        ///
        /// @param received_counter  The counter from the received packet
        void markReceived(u64 received_counter)
        {
                // Calculate the bit position within the window
                if (nonce_counter == 0) return;
                s64 offset = static_cast<s64>(nonce_counter) - static_cast<s64>(received_counter) - 1;
                if (offset >= 0 && offset < static_cast<s64>(REPLAY_WINDOW_SIZE)) {
                        size_t word = static_cast<size_t>(offset) / 64;
                        size_t bit = static_cast<size_t>(offset) % 64;
                        if (word < REPLAY_BITMAP_WORDS) {
                                replay_bitmap[word] |= (1ULL << bit);
                        }
                }
        }

        /// Check if a specific counter has already been seen (within the window).
        ///
        /// @param received_counter  The counter to check
        /// @return true if this counter was already seen (replay detected)
        bool isAlreadySeen(u64 received_counter) const
        {
                if (nonce_counter == 0) return false;
                s64 offset = static_cast<s64>(nonce_counter) - static_cast<s64>(received_counter) - 1;
                if (offset >= 0 && offset < static_cast<s64>(REPLAY_WINDOW_SIZE)) {
                        size_t word = static_cast<size_t>(offset) / 64;
                        size_t bit = static_cast<size_t>(offset) % 64;
                        if (word < REPLAY_BITMAP_WORDS) {
                                return (replay_bitmap[word] & (1ULL << bit)) != 0;
                        }
                }
                return false;
        }

        /// Shift the replay bitmap when the high-water mark advances.
        /// Call this after updateCounter() moves the counter forward.
        ///
        /// @param old_counter  The counter value before the update
        /// @param new_counter  The counter value after the update
        void shiftBitmap(u64 old_counter, u64 new_counter)
        {
                u64 shift = new_counter - old_counter;
                if (shift == 0) return;

                // If shift is larger than the window, clear the bitmap
                if (shift >= REPLAY_WINDOW_SIZE) {
                        replay_bitmap.fill(0);
                        return;
                }

                // Shift the bitmap by 'shift' positions to the right
                // (older entries move further from the high-water mark)
                for (size_t i = REPLAY_BITMAP_WORDS; i > 0; i--) {
                        size_t idx = i - 1;
                        size_t word_shift = static_cast<size_t>(shift);
                        size_t big_shift = word_shift / 64;
                        size_t small_shift = word_shift % 64;

                        u64 val = 0;
                        if (idx >= big_shift) {
                                val = replay_bitmap[idx - big_shift] >> small_shift;
                                if (small_shift > 0 && idx > big_shift) {
                                        val |= replay_bitmap[idx - big_shift - 1] << (64 - small_shift);
                                }
                        }
                        replay_bitmap[idx] = val;
                }
        }

        /// Check if a received nonce counter is valid (not a replay).
        /// v9.9: Uses exact bitmap tracking within the sliding window.
        ///
        /// @param received_counter  The counter from the received packet
        /// @return true if the packet is not a replay
        bool isNotReplay(u64 received_counter) const
        {
                // Allow packets up to REPLAY_WINDOW_SIZE positions behind the latest seen counter
                if (received_counter > nonce_counter) {
                        // Future packet — always accept (counter will be updated by caller)
                        return true;
                }

                if (nonce_counter - received_counter > REPLAY_WINDOW_SIZE) {
                        // Too far behind — likely a replay attack
                        return false;
                }

                // Within the acceptable window — check if already seen (v9.9)
                if (isAlreadySeen(received_counter)) {
                        // This counter was already received — replay!
                        return false;
                }

                return true;
        }

        /// Update the high-water mark counter after accepting a packet.
        /// Also shifts the replay bitmap.
        ///
        /// @param received_counter  The counter from the received packet
        void updateCounter(u64 received_counter)
        {
                if (received_counter >= nonce_counter) {
                        u64 old_counter = nonce_counter;
                        nonce_counter = received_counter + 1;
                        shiftBitmap(old_counter, nonce_counter);
                }
        }
};

/// Size of an X25519 public key in bytes (standard Curve25519)
constexpr size_t X25519_PUBLIC_KEY_SIZE = 32;

/// Size of an X25519 private key in bytes (standard Curve25519)
constexpr size_t X25519_PRIVATE_KEY_SIZE = 32;

/// Size of an X25519 shared secret in bytes
constexpr size_t X25519_SHARED_SECRET_SIZE = 32;

/// Full encryption state for a peer connection (both directions).
///
/// Contains separate state for client-to-server and server-to-client
/// encryption, derived from the SRP session key via HKDF.
struct PeerEncryptionState
{
private:
        /// Mutex to protect concurrent access to encryption state.
        /// The main thread writes (SetPeerEncryptionState, activate),
        /// while the send/receive threads read (rawSend, receive).
        mutable std::mutex m_mutex;

public:
        /// Whether encryption is currently active for this peer
        /// Atomic so the receive thread can check it without locking
        std::atomic<bool> active{false};

        /// Encryption state for client→server direction
        DirectionalEncryptionState c2s;

        /// Encryption state for server→client direction
        DirectionalEncryptionState s2c;

        /// The SRP session key (kept for fingerprint derivation)
        std::array<u8, SRP_SESSION_KEY_SIZE> srp_session_key{};

        /// Session ID (derived from key material, hex string)
        std::string session_id;

        /// Server fingerprint (derived from SRP verifier hash, hex string)
        std::string server_fingerprint;

        /// v9.1: Whether ECDH X25519 forward secrecy key exchange was completed
        /// When true, forward_secrecy can be honestly reported as true
        std::atomic<bool> ecdh_completed{false};

        /// v9.1: X25519 ephemeral private key (zeroed after session)
        std::array<u8, X25519_PRIVATE_KEY_SIZE> ecdh_private_key{};

        /// v9.1: X25519 public key for this side (sent to peer)
        std::array<u8, X25519_PUBLIC_KEY_SIZE> ecdh_public_key{};

        /// v9.1: The ECDH shared secret (kept for rekeying, zeroed on disconnect)
        std::array<u8, X25519_SHARED_SECRET_SIZE> ecdh_shared_secret{};

        /// v9.9: HKDF salt used in key derivation (generated once per session)
        std::array<u8, 16> hkdf_salt{};

        /// v9.9: Number of key rotations performed in this session
        u32 key_rotation_count = 0;

        /// v9.9: Whether key rotation is supported (always true in v9.9+)
        static constexpr bool KEY_ROTATION_SUPPORTED = true;

        /// Time when encryption was activated (unix timestamp)
        u64 activated_at = 0;

        /// v9.13: Transition grace period fields.
        /// After encryption activates, we may still receive a few plaintext
        /// packets from the peer that were already in the network pipeline.
        /// These are expected during the transition and should NOT be treated
        /// as security violations or spam the error log.

        /// Number of plaintext packets received after encryption activated
        std::atomic<u32> transition_plaintext_count{0};

        /// Whether we have already logged the transition-period summary
        std::atomic<bool> transition_logged{false};

        /// Maximum number of plaintext packets to accept during transition.
        /// v9.14: Increased from 50 to 500 — world loading generates hundreds
        /// of map block packets that arrive over several seconds.
        static constexpr u32 TRANSITION_MAX_PLAINTEXT = 500;

        /// Duration of the transition grace period in milliseconds.
        /// v9.14: Increased from 2000 to 10000 — world loading on a localhost
        /// or LAN connection can take several seconds, and pipeline packets
        /// from before the server activated encryption can arrive for much
        /// longer than 2 seconds.
        static constexpr u64 TRANSITION_GRACE_PERIOD_MS = 10000;

        /// Whether we have already logged the one-time post-grace-period warning.
        /// v9.14: Prevents repeated ERROR spam after the grace period expires.
        /// Instead of logging every 100th violation as ERROR, we log a single
        /// WARNING once and then accept plaintext packets silently.
        std::atomic<bool> transition_warning_logged{false};

        /// Time of last encryption audit log (to avoid log spam)
        u64 last_audit_time_ms = 0;

        /// Number of packets since last audit (for periodic logging)
        u64 packets_since_audit = 0;

        /// Audit interval in milliseconds (default: 30 seconds)
        static constexpr u64 AUDIT_INTERVAL_MS = 30000;

        /// Minimum packets between audits (avoid spam on idle connections)
        static constexpr u64 AUDIT_MIN_PACKETS = 100;

        /// Lock the encryption state for reading/writing.
        /// Call this before accessing c2s/s2c key/nonce data.
        std::unique_lock<std::mutex> lock() const { return std::unique_lock<std::mutex>(m_mutex); }

        /// Initialize encryption from the SRP session key.
        ///
        /// Derives C2S key, S2C key, C2S nonce base, and S2C nonce base
        /// using HKDF-SHA256 with direction-specific info strings.
        ///
        /// @param session_key  The 32-byte SRP session key
        /// @param key_len      Length of the session key (must be 32)
        /// @param is_server    True if this is the server side
        /// @return true on success, false on failure
        bool initFromSRPSessionKey(const u8* session_key, size_t key_len, bool is_server);

        /// Activate encryption after keys have been set.
        /// This should be called only after ensuring that any necessary
        /// plaintext packets (like AUTH_ACCEPT) have been queued/sent.
        /// v9.13: Also resets transition grace period counters.
        /// v9.14: Also resets transition_warning_logged.
        void activate()
        {
                active.store(true, std::memory_order_release);
                transition_plaintext_count.store(0, std::memory_order_release);
                transition_logged.store(false, std::memory_order_release);
                transition_warning_logged.store(false, std::memory_order_release);
        }

        /// Disable encryption (e.g., on disconnect or auth failure)
        /// v9.13: Also resets transition grace period counters.
        /// v9.14: Also resets transition_warning_logged.
        void disable()
        {
                active.store(false, std::memory_order_release);
                ecdh_completed.store(false, std::memory_order_release);
                transition_plaintext_count.store(0, std::memory_order_release);
                transition_logged.store(false, std::memory_order_release);
                transition_warning_logged.store(false, std::memory_order_release);
                std::lock_guard<std::mutex> lock(m_mutex);
                // Zero out sensitive key material
                c2s.key.fill(0);
                s2c.key.fill(0);
                c2s.nonce_base.fill(0);
                s2c.nonce_base.fill(0);
                srp_session_key.fill(0);
                ecdh_private_key.fill(0);
                ecdh_public_key.fill(0);
                ecdh_shared_secret.fill(0);
                hkdf_salt.fill(0);
                session_id.clear();
                server_fingerprint.clear();
        }

        /// v9.9: Rotate session keys using fresh ECDH key exchange.
        ///
        /// Performs a new X25519 key exchange and re-derives all encryption
        /// keys. The old keys are securely zeroed. This provides forward
        /// secrecy within a long-lived session — if a key is compromised,
        /// only the packets encrypted with that key are affected.
        ///
        /// This should be called periodically (e.g., every 10 minutes or
        /// every 1 million packets) to limit the impact of key compromise.
        ///
        /// @return true on success, false on failure
        bool rotateKeys();
};

/// Convert binary data to hexadecimal string
std::string binToHex(const u8* data, size_t len);

/// Convert a 32-byte key to a fingerprint string like "SHA256:a1b2c3..."
std::string keyToFingerprint(const u8* key, size_t len);

// ============================================================================
// v9.1: X25519 ECDH Key Exchange for Forward Secrecy
// ============================================================================
//
// After SRP authentication establishes identity, both sides perform an
// ephemeral X25519 key exchange. The resulting shared secret is mixed
// into the HKDF key derivation alongside the SRP session key. This
// provides REAL forward secrecy: if the SRP password is later compromised,
// past session keys cannot be recovered because the X25519 ephemeral
// private keys are destroyed after the session.
//
// Performance: One extra round-trip during connection setup (X25519 is
// extremely fast — ~50 microseconds per key exchange). Zero impact on
// real-time gameplay after the initial handshake.
//
// Wire format: After AUTH_ACCEPT, client and server exchange 32-byte
// X25519 public keys in new packet types:
//   TOSERVER_ECDH_PUBKEY  — client sends its X25519 public key
//   TOCLIENT_ECDH_PUBKEY  — server responds with its X25519 public key

/// Result of an X25519 key exchange operation
struct X25519KeyPair
{
        bool success = false;
        std::string error_msg;

        /// X25519 public key (32 bytes, can be sent over the wire)
        std::array<u8, X25519_PUBLIC_KEY_SIZE> public_key{};

        /// X25519 private key (32 bytes, MUST be kept secret and zeroed after use)
        std::array<u8, X25519_PRIVATE_KEY_SIZE> private_key{};
};

/// Result of an X25519 shared secret computation
struct X25519SharedSecret
{
        bool success = false;
        std::string error_msg;

        /// The computed shared secret (32 bytes)
        std::array<u8, X25519_SHARED_SECRET_SIZE> shared_secret{};
};

/// Generate an X25519 ephemeral key pair.
///
/// Uses OpenSSL's EVP_PKEY_keygen for X25519 key generation.
/// The private key MUST be kept secret and zeroed after the session ends.
///
/// @return X25519KeyPair with public and private keys on success
X25519KeyPair x25519_generate_keypair();

/// Compute the X25519 shared secret from a private key and peer's public key.
///
/// Uses OpenSSL's EVP_PKEY_derive for X25519 Diffie-Hellman.
/// The resulting shared secret is mixed into HKDF alongside the SRP session key
/// to derive the final C2S/S2C encryption keys.
///
/// @param our_private_key   Our 32-byte X25519 private key
/// @param peer_public_key   Peer's 32-byte X25519 public key
/// @return X25519SharedSecret with the 32-byte shared secret on success
X25519SharedSecret x25519_compute_shared_secret(
        const u8* our_private_key, size_t our_private_key_len,
        const u8* peer_public_key, size_t peer_public_key_len);

/// Mix an ECDH shared secret into existing key derivation.
///
/// Re-derives C2S key, S2C key, C2S nonce base, S2C nonce base, and
/// session ID using HKDF with BOTH the SRP session key AND the ECDH
/// shared secret as input keying material. The combined IKM ensures
/// that key compromise of either SRP or ECDH alone is insufficient
/// to derive session keys — both are needed.
///
/// @param state           PeerEncryptionState to update (must already be initialized with SRP)
/// @param ecdh_secret     The 32-byte X25519 shared secret
/// @param ecdh_secret_len Length of the shared secret (must be 32)
/// @return true on success, false on failure
bool mixECDHSecretIntoKeys(PeerEncryptionState &state,
        const u8* ecdh_secret, size_t ecdh_secret_len);

// ============================================================================
// v9.1: Server Fingerprint Store (Trust-On-First-Use with Pinning)
// ============================================================================
//
// When connecting to a server for the first time, its fingerprint is recorded.
// On subsequent connections, the fingerprint is verified against the stored one.
// If it changes, the user is warned about a possible MITM attack.
// Once a fingerprint is verified (known server), the certificate_status
// is upgraded from CERT_TRUST_ON_FIRST_USE to CERT_VERIFIED, adding +10
// to the security score.

/// Persistent store for known server fingerprints.
/// Stores fingerprints in a simple text file in the Luanti world/config directory.
class FingerprintStore
{
public:
        /// Load known fingerprints from a file.
        /// Each line is: <server_address>:<port> <fingerprint>
        /// Returns true if the file was loaded (or didn't exist, which is OK).
        bool load(const std::string &filepath);

        /// Save known fingerprints to a file.
        bool save(const std::string &filepath) const;

        /// Check if a server's fingerprint is known and matches.
        /// Returns:
        ///   0 = unknown server (first connection, TOFU)
        ///   1 = fingerprint matches (verified, CERT_VERIFIED)
        ///  -1 = fingerprint MISMATCH (possible MITM!)
        int verify(const std::string &server_address, u16 port,
                   const std::string &fingerprint) const;

        /// Record a server's fingerprint (TOFU: trust on first use).
        void record(const std::string &server_address, u16 port,
                    const std::string &fingerprint);

        /// Get the stored fingerprint for a server (empty string if unknown)
        std::string getStoredFingerprint(const std::string &server_address, u16 port) const;

        /// Number of known server fingerprints
        size_t size() const { return m_fingerprints.size(); }

        /// Clear all stored fingerprints
        void clear() { m_fingerprints.clear(); }

private:
        /// Key: "address:port", Value: fingerprint string
        std::map<std::string, std::string> m_fingerprints;

        /// Build the key used in the map
        static std::string makeKey(const std::string &server_address, u16 port);
};
