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
 * v9.37: Per-username keypair support.
 * Each username gets its own Ed25519 keypair so that different characters
 * on the same server each have independent keys. This means:
 *   - "warrior" has keypair A → registered as "warrior" on server X
 *   - "builder" has keypair B → registered as "builder" on server X
 *   Both can connect to the same server, each authenticating with their own key.
 *
 * Keypair files are stored as:
 *   <keydir>/client_ed25519_keys/<username>.key
 * Each file contains [32 bytes private key seed][32 bytes public key] (64 bytes total).
 *
 * The old single-keypair file (client_ed25519_key) is automatically migrated:
 * if a username-specific key doesn't exist but the legacy file does, the legacy
 * keypair is copied as that username's keypair on first use.
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
	 * @param keydir  Directory where keypairs and server-user mapping are stored.
	 *                Typically the Luanti user data path (porting::path_user).
	 */
	explicit KeypairManager(const std::string &keydir);

	~KeypairManager();

	// Non-copyable
	KeypairManager(const KeypairManager &) = delete;
	KeypairManager &operator=(const KeypairManager &) = delete;

	/**
	 * Ensure a keypair exists for the given username.
	 * If one is already loaded for this username, this is a no-op.
	 * If a different username's keypair is loaded, it will be unloaded first.
	 * If a keypair file exists for this username, loads it.
	 * If no keypair file exists, generates a new one and saves it.
	 *
	 * v9.37: If no username-specific keypair exists but a legacy
	 * client_ed25519_key file does exist, the legacy keypair is migrated
	 * (copied) to become this username's keypair.
	 *
	 * Must be called before sign() or getPublicKey().
	 * Returns true on success, false on failure.
	 */
	bool ensureKeypair(const std::string &username);

	/**
	 * Get the raw Ed25519 public key (32 bytes) for the currently loaded username.
	 * Returns empty string if no keypair is loaded.
	 */
	std::string getPublicKey() const;

	/**
	 * Get the base64-encoded public key for the currently loaded username.
	 * Returns empty string if no keypair is loaded.
	 */
	std::string getPublicKeyBase64() const;

	/**
	 * Sign a message with the currently loaded username's private key.
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
	 * Get the currently loaded username (empty if no keypair loaded).
	 */
	std::string getCurrentUsername() const;

	/**
	 * Check whether a keypair file exists on disk for the given username.
	 * @param username  The username to check (empty = legacy keypair).
	 */
	bool keypairFileExists(const std::string &username = "") const;

	/**
	 * Regenerate the keypair for the currently loaded username
	 * (deleting the old one and creating a new one).
	 * WARNING: This will invalidate all server registrations that used the old key.
	 * Returns true on success.
	 */
	bool regenerateKeypair();

	/**
	 * List all usernames that have keypair files on disk.
	 * v9.37: Scans the client_ed25519_keys/ directory.
	 * Also includes the legacy keypair as a special entry if it exists.
	 * @return A vector of usernames (without .key extension).
	 */
	std::vector<std::string> listKeypairs() const;

	/**
	 * Delete the keypair file for a given username.
	 * @param username  The username whose keypair to delete.
	 * @return true if the file existed and was deleted.
	 */
	bool deleteKeypair(const std::string &username);

	/**
	 * Get the base64-encoded public key for a specific username without
	 * changing the currently loaded keypair. Loads the key from disk.
	 * @param username  The username to look up.
	 * @return The base64-encoded public key, or empty string on failure.
	 */
	std::string getPublicKeyBase64ForUser(const std::string &username) const;

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
	 * Save the current keypair to disk for the given username.
	 * File format: [32 bytes private key seed][32 bytes public key]
	 * File permissions: 0600 (owner read/write only).
	 */
	bool saveKeypair(const std::string &username) const;

	/**
	 * Load the keypair from disk for the given username.
	 * Returns true on success.
	 */
	bool loadKeypair(const std::string &username);

	/**
	 * Migrate the legacy single-keypair file to a per-username keypair.
	 * Called when ensureKeypair(username) finds no per-username file
	 * but the legacy client_ed25519_key file exists.
	 * @param username  The username to migrate the keypair for.
	 * @return true if migration happened, false if not needed or failed.
	 */
	bool migrateLegacyKeypair(const std::string &username);

	/**
	 * Load the per-server username mapping from the JSON file.
	 */
	void loadServerUsers() const;

	/**
	 * Save the per-server username mapping to the JSON file.
	 */
	void saveServerUsers() const;

	/**
	 * Sanitize a username for use as a filename.
	 * Only allows alphanumeric characters and underscores.
	 */
	static std::string sanitizeUsername(const std::string &username);

	/**
	 * Get the keypair file path for a given username.
	 */
	std::string getKeypairPath(const std::string &username) const;

	/// Directory for keypair and mapping files
	std::string m_keydir;

	/// Directory for per-username keypair files
	std::string m_keys_dir;

	/// Path to the legacy keypair file (for migration)
	std::string m_legacy_keypair_path;

	/// Path to the server-username mapping file
	std::string m_server_users_path;

	/// Currently loaded username (empty if no keypair loaded)
	std::string m_current_username;

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
