// Luanti-Secure
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti-Secure contributors

#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <ctime>

/**
 * Ed25519 keypair management for passwordless authentication.
 *
 * This module provides a self-contained keypair manager that handles:
 * - Ed25519 keypair generation (via OpenSSL 3.0+ EVP API)
 * - Keypair persistence (file-based storage with restricted permissions)
 * - Digital signature creation and verification
 * - Per-server username memory (JSON-based)
 *
 * Protocol overview (v9.29):
 *   Registration: Client sends public key → Server stores it → Auth accepted
 *   Login:        Server sends challenge nonce → Client signs it → Server verifies → Auth accepted
 *
 * The keypair is device-bound (stored locally with 0600 permissions).
 * Public keys on the server are stored in the existing AuthEntry::password
 * field using "#2#" prefix encoding (parallel to SRP's "#1#" encoding).
 */

/// Ed25519 public key size in bytes
constexpr size_t ED25519_PUBLIC_KEY_SIZE = 32;

/// Ed25519 private key size in bytes (seed only, 32 bytes)
constexpr size_t ED25519_PRIVATE_KEY_SIZE = 32;

/// Ed25519 signature size in bytes
constexpr size_t ED25519_SIGNATURE_SIZE = 64;

/// Challenge nonce size in bytes
constexpr size_t KEYPAIR_CHALLENGE_SIZE = 32;

class KeypairManager {
public:
        /**
         * Construct a KeypairManager.
         * @param keydir  Directory where keypair and server-user mapping are stored.
         *                Typically the Luanti user data path (porting::path_user).
         */
        explicit KeypairManager(const std::string &keydir);

        ~KeypairManager();

        // Non-copyable
        KeypairManager(const KeypairManager &) = delete;
        KeypairManager &operator=(const KeypairManager &) = delete;

        /**
         * Ensure a keypair exists. If one is already loaded, this is a no-op.
         * If no keypair file exists on disk, generates a new one and saves it.
         * If a keypair file exists, loads it.
         *
         * Must be called before sign() or getPublicKey().
         * Returns true on success, false on failure.
         */
        bool ensureKeypair();

        /**
         * Get the raw Ed25519 public key (32 bytes).
         * Returns empty string if no keypair is loaded.
         */
        std::string getPublicKey() const;

        /**
         * Get the base64-encoded public key (for storage/transmission).
         * Returns empty string if no keypair is loaded.
         */
        std::string getPublicKeyBase64() const;

        /**
         * Sign a message with the loaded private key.
         * @param message  The message to sign (typically a challenge nonce).
         * @return The 64-byte Ed25519 signature, or empty string on failure.
         */
        std::string sign(const std::string &message) const;

        /**
         * Verify a signature against a public key (static utility).
         * @param public_key  The 32-byte Ed25519 public key.
         * @param message     The message that was signed.
         * @param signature   The 64-byte Ed25519 signature.
         * @return true if the signature is valid.
         */
        static bool verify(const std::string &public_key,
                const std::string &message,
                const std::string &signature);

        /**
         * Generate a cryptographically random challenge nonce.
         * @return A random string of KEYPAIR_CHALLENGE_SIZE bytes.
         */
        static std::string generateChallenge();

        /**
         * Check whether a keypair is currently loaded in memory.
         */
        bool hasKeypair() const;

        /**
         * Check whether a keypair file exists on disk (without loading it).
         */
        bool keypairFileExists() const;

        /**
         * Regenerate the keypair (deleting the old one and creating a new one).
         * WARNING: This will invalidate all server registrations that used the old key.
         * Returns true on success.
         */
        bool regenerateKeypair();

        /// Per-server entry with metadata
        struct ServerEntry {
                std::string username;
                std::string created_at;   ///< ISO 8601 timestamp when first registered
                std::string last_used_at; ///< ISO 8601 timestamp of last successful login
        };

        /**
         * Get all remembered server entries with metadata.
         * @return A vector of (server_address, ServerEntry) pairs.
         */
        std::vector<std::pair<std::string, ServerEntry>> getServerUserList() const;

        /**
         * Remove the remembered username for a given server address.
         * @param server_address  The server address to forget.
         * @return true if the entry existed and was removed.
         */
        bool forgetServerUser(const std::string &server_address);

        // ---- Per-server username memory ----

        /**
         * Remember the last username used for a given server address.
         * @param server_address  The server address (host:port).
         * @param username        The username used.
         */
        void rememberServerUser(const std::string &server_address,
                const std::string &username);

        /**
         * Get the last username used for a given server address.
         * @param server_address  The server address (host:port).
         * @return The remembered username, or empty string if none.
         */
        std::string getServerUser(const std::string &server_address) const;

        /**
         * Check whether a username is remembered for a given server address.
         */
        bool hasServerUser(const std::string &server_address) const;

private:
        /**
         * Generate a new Ed25519 keypair (overwriting any existing one in memory).
         * Returns true on success.
         */
        bool generateKeypair();

        /**
         * Save the current keypair to disk.
         * File format: [32 bytes private key seed][32 bytes public key]
         * File permissions: 0600 (owner read/write only).
         */
        bool saveKeypair() const;

        /**
         * Load the keypair from disk.
         * Returns true on success.
         */
        bool loadKeypair();

        /**
         * Load the per-server username mapping from the JSON file.
         */
        void loadServerUsers() const;

        /**
         * Save the per-server username mapping to the JSON file.
         */
        void saveServerUsers() const;

        /// Directory for keypair and mapping files
        std::string m_keydir;

        /// Path to the keypair file
        std::string m_keypair_path;

        /// Path to the server-username mapping file
        std::string m_server_users_path;

        /// Private key seed (32 bytes). Empty if not loaded.
        std::string m_private_key;

        /// Public key (32 bytes). Empty if not loaded.
        std::string m_public_key;

        /// Whether the keypair file was newly created (first registration)
        bool m_is_new_keypair = false;

        /// Cached server → ServerEntry mapping (mutable for lazy loading)
        mutable std::map<std::string, ServerEntry> m_server_users;

        /// Whether m_server_users has been loaded from disk
        mutable bool m_server_users_loaded = false;
};
