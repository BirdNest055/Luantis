// Luanti-Secure
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti-Secure contributors

#include "keypair.h"

#include "base64.h"
#include "log.h"
#include "filesys.h"
#include "util/hashing.h"
#include "json/json.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>

#if USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#endif

KeypairManager::KeypairManager(const std::string &keydir)
        : m_keydir(keydir)
        , m_keys_dir(keydir + DIR_DELIM + "client_ed25519_keys")
        , m_legacy_keypair_path(keydir + DIR_DELIM + "client_ed25519_key")
        , m_server_users_path(keydir + DIR_DELIM + "keypair_server_users.json")
{
}

KeypairManager::~KeypairManager()
{
        // Wipe private key from memory
        if (!m_private_key.empty()) {
                for (char &c : m_private_key) {
                        c = '\0';
                }
        }
}

std::string KeypairManager::sanitizeUsername(const std::string &username)
{
        std::string result;
        for (char c : username) {
                // Allow alphanumeric and underscore only
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_') {
                        result += c;
                }
        }
        return result;
}

std::string KeypairManager::getKeypairPath(const std::string &username) const
{
        return m_keys_dir + DIR_DELIM + sanitizeUsername(username) + ".key";
}

bool KeypairManager::ensureKeypair(const std::string &username)
{
        if (username.empty()) {
                errorstream << "KeypairManager::ensureKeypair: Empty username!" << std::endl;
                return false;
        }

        // If we already have this username's keypair loaded, nothing to do
        if (hasKeypair() && m_current_username == username)
                return true;

        // Unload any previously loaded keypair (wipe private key securely)
        if (!m_private_key.empty()) {
                for (char &c : m_private_key) {
                        c = '\0';
                }
                m_private_key.clear();
        }
        m_public_key.clear();
        m_current_username.clear();

        // Ensure the keys directory exists
        if (!fs::PathExists(m_keys_dir)) {
                fs::CreateDir(m_keys_dir);
        }

        std::string keypair_path = getKeypairPath(username);

        // Try loading an existing keypair for this username first
        if (fs::PathExists(keypair_path)) {
                if (loadKeypair(username)) {
                        m_current_username = username;
                        return true;
                }
                // If loading failed, fall through to generate a new one
                warningstream << "KeypairManager: Failed to load keypair for '"
                        << username << "', generating a new one." << std::endl;
        } else {
                // v9.37: Try migrating the legacy single-keypair file
                if (migrateLegacyKeypair(username)) {
                        if (loadKeypair(username)) {
                                m_current_username = username;
                                infostream << "KeypairManager: Migrated legacy keypair for '"
                                        << username << "'." << std::endl;
                                return true;
                        }
                }
        }

        // Generate a new keypair for this username
        if (!generateKeypair()) {
                errorstream << "KeypairManager: Failed to generate Ed25519 keypair for '"
                        << username << "'!" << std::endl;
                return false;
        }

        if (!saveKeypair(username)) {
                errorstream << "KeypairManager: Failed to save Ed25519 keypair for '"
                        << username << "'!" << std::endl;
                return false;
        }

        m_current_username = username;
        m_is_new_keypair = true;
        infostream << "KeypairManager: Generated new Ed25519 keypair for '"
                << username << "'." << std::endl;
        return true;
}

std::string KeypairManager::getPublicKey() const
{
        return m_public_key;
}

std::string KeypairManager::getPublicKeyBase64() const
{
        if (m_public_key.empty())
                return "";
        return base64_encode(m_public_key);
}

std::string KeypairManager::sign(const std::string &message) const
{
#if USE_OPENSSL
        if (m_private_key.empty() || m_public_key.empty()) {
                errorstream << "KeypairManager::sign: No keypair loaded!" << std::endl;
                return "";
        }

        // Reconstruct the EVP_PKEY from raw key bytes.
        // Ed25519 private key for EVP_PKEY is the 32-byte seed.
        EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(
                EVP_PKEY_ED25519, nullptr,
                reinterpret_cast<const unsigned char *>(m_private_key.data()),
                m_private_key.size());

        if (!pkey) {
                errorstream << "KeypairManager::sign: Failed to create EVP_PKEY from "
                        << "private key seed." << std::endl;
                return "";
        }

        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
                EVP_PKEY_free(pkey);
                return "";
        }

        // Ed25519: one-shot sign using EVP_DigestSign.
        // NOTE: OpenSSL 3.5+ no longer supports the streaming API
        // (Init/Update/Final) for Ed25519 — it returns
        // "provider signature not supported" on Update/Final.
        // The one-shot EVP_DigestSign() must be used instead.
        if (EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, pkey) != 1) {
                errorstream << "KeypairManager::sign: DigestSignInit failed." << std::endl;
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return "";
        }

        // Determine the maximum signature length
        size_t sig_len = ED25519_SIGNATURE_SIZE;
        std::string signature(sig_len, '\0');

        if (EVP_DigestSign(mdctx,
                        reinterpret_cast<unsigned char *>(&signature[0]),
                        &sig_len,
                        reinterpret_cast<const unsigned char *>(message.data()),
                        message.size()) != 1) {
                errorstream << "KeypairManager::sign: EVP_DigestSign failed." << std::endl;
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return "";
        }

        signature.resize(sig_len);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return signature;
#else
        errorstream << "KeypairManager::sign: OpenSSL is required for Ed25519 signing!"
                << std::endl;
        return "";
#endif
}

bool KeypairManager::verify(const std::string &public_key,
        const std::string &message,
        const std::string &signature)
{
#if USE_OPENSSL
        if (public_key.size() != ED25519_PUBLIC_KEY_SIZE) {
                errorstream << "KeypairManager::verify: Invalid public key size ("
                        << public_key.size() << ", expected " << ED25519_PUBLIC_KEY_SIZE
                        << ")." << std::endl;
                return false;
        }

        if (signature.size() != ED25519_SIGNATURE_SIZE) {
                errorstream << "KeypairManager::verify: Invalid signature size ("
                        << signature.size() << ", expected " << ED25519_SIGNATURE_SIZE
                        << ")." << std::endl;
                return false;
        }

        EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
                EVP_PKEY_ED25519, nullptr,
                reinterpret_cast<const unsigned char *>(public_key.data()),
                public_key.size());

        if (!pkey) {
                errorstream << "KeypairManager::verify: Failed to create EVP_PKEY from "
                        << "public key." << std::endl;
                return false;
        }

        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
                EVP_PKEY_free(pkey);
                return false;
        }

        // Ed25519: one-shot verify using EVP_DigestVerify.
        // NOTE: OpenSSL 3.5+ no longer supports the streaming API
        // (Init/Update/Final) for Ed25519 — same as signing.
        if (EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pkey) != 1) {
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return false;
        }

        int result = EVP_DigestVerify(mdctx,
                reinterpret_cast<const unsigned char *>(signature.data()),
                signature.size(),
                reinterpret_cast<const unsigned char *>(message.data()),
                message.size());

        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);

        return result == 1;
#else
        errorstream << "KeypairManager::verify: OpenSSL is required for Ed25519 "
                << "verification!" << std::endl;
        return false;
#endif
}

std::string KeypairManager::generateChallenge()
{
#if USE_OPENSSL
        std::string nonce(KEYPAIR_CHALLENGE_SIZE, '\0');
        if (RAND_bytes(reinterpret_cast<unsigned char *>(&nonce[0]),
                        KEYPAIR_CHALLENGE_SIZE) != 1) {
                errorstream << "KeypairManager::generateChallenge: RAND_bytes failed!"
                        << std::endl;
                return "";
        }
        return nonce;
#else
        // Fallback: use hashing module's secure random if OpenSSL not available
        errorstream << "KeypairManager::generateChallenge: OpenSSL required!" << std::endl;
        return "";
#endif
}

bool KeypairManager::hasKeypair() const
{
        return !m_private_key.empty() && !m_public_key.empty();
}

std::string KeypairManager::getCurrentUsername() const
{
        return m_current_username;
}

bool KeypairManager::keypairFileExists(const std::string &username) const
{
        if (username.empty()) {
                // Legacy: check the old single-keypair file
                return fs::PathExists(m_legacy_keypair_path);
        }
        return fs::PathExists(getKeypairPath(username));
}

bool KeypairManager::regenerateKeypair()
{
        if (m_current_username.empty()) {
                errorstream << "KeypairManager::regenerateKeypair: No username loaded!" << std::endl;
                return false;
        }

        // Clear existing keypair from memory
        if (!m_private_key.empty()) {
                for (char &c : m_private_key) {
                        c = '\0';
                }
                m_private_key.clear();
        }
        m_public_key.clear();

        // Delete the old keypair file for this username
        std::string keypair_path = getKeypairPath(m_current_username);
        if (fs::PathExists(keypair_path)) {
                fs::DeleteSingleFileOrEmptyDirectory(keypair_path);
        }

        // Generate a new keypair
        if (!generateKeypair()) {
                errorstream << "KeypairManager::regenerateKeypair: Failed to generate new keypair!"
                        << std::endl;
                return false;
        }

        if (!saveKeypair(m_current_username)) {
                errorstream << "KeypairManager::regenerateKeypair: Failed to save new keypair!"
                        << std::endl;
                return false;
        }

        m_is_new_keypair = true;
        infostream << "KeypairManager: Regenerated Ed25519 keypair for '"
                << m_current_username << "'." << std::endl;
        return true;
}

std::vector<std::string> KeypairManager::listKeypairs() const
{
        std::vector<std::string> result;

        // Ensure the keys directory exists
        if (!fs::PathExists(m_keys_dir))
                return result;

        // Scan the keys directory for .key files
        std::vector<fs::DirListNode> entries = fs::GetDirListing(m_keys_dir);

        for (const auto &entry : entries) {
                // Skip directories
                if (entry.dir)
                        continue;
                // Only include .key files
                const std::string &name = entry.name;
                if (name.size() > 4 && name.substr(name.size() - 4) == ".key") {
                        result.push_back(name.substr(0, name.size() - 4));
                }
        }

        return result;
}

bool KeypairManager::deleteKeypair(const std::string &username)
{
        if (username.empty())
                return false;

        std::string keypair_path = getKeypairPath(username);
        if (!fs::PathExists(keypair_path))
                return false;

        fs::DeleteSingleFileOrEmptyDirectory(keypair_path);

        // If this was the currently loaded keypair, unload it
        if (m_current_username == username) {
                if (!m_private_key.empty()) {
                        for (char &c : m_private_key) {
                                c = '\0';
                        }
                        m_private_key.clear();
                }
                m_public_key.clear();
                m_current_username.clear();
        }

        infostream << "KeypairManager: Deleted keypair for '" << username << "'." << std::endl;
        return true;
}

std::string KeypairManager::getPublicKeyBase64ForUser(const std::string &username) const
{
        if (username.empty())
                return "";

        // If this user is currently loaded, return directly
        if (m_current_username == username && hasKeypair())
                return getPublicKeyBase64();

        // Otherwise, load from disk temporarily
        std::string keypair_path = getKeypairPath(username);
        if (!fs::PathExists(keypair_path))
                return "";

        std::ifstream file(keypair_path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
                return "";

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        const size_t expected_size = ED25519_PRIVATE_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE;
        if (size != static_cast<std::streamsize>(expected_size))
                return "";

        std::string data(expected_size, '\0');
        if (!file.read(&data[0], expected_size))
                return "";

        // Extract just the public key (second 32 bytes)
        std::string pubkey = data.substr(ED25519_PRIVATE_KEY_SIZE, ED25519_PUBLIC_KEY_SIZE);
        return base64_encode(pubkey);
}

bool KeypairManager::generateKeypair()
{
#if USE_OPENSSL
        EVP_PKEY *pkey = nullptr;
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
        if (!ctx) {
                errorstream << "KeypairManager: Failed to create EVP_PKEY_CTX." << std::endl;
                return false;
        }

        if (EVP_PKEY_keygen_init(ctx) <= 0) {
                errorstream << "KeypairManager: EVP_PKEY_keygen_init failed." << std::endl;
                EVP_PKEY_CTX_free(ctx);
                return false;
        }

        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
                errorstream << "KeypairManager: EVP_PKEY_keygen failed." << std::endl;
                EVP_PKEY_CTX_free(ctx);
                return false;
        }

        EVP_PKEY_CTX_free(ctx);

        // Extract private key (seed)
        size_t priv_len = 0;
        if (EVP_PKEY_get_raw_private_key(pkey, nullptr, &priv_len) != 1) {
                errorstream << "KeypairManager: Failed to query private key size." << std::endl;
                EVP_PKEY_free(pkey);
                return false;
        }

        m_private_key.resize(priv_len);
        if (EVP_PKEY_get_raw_private_key(pkey,
                        reinterpret_cast<unsigned char *>(&m_private_key[0]),
                        &priv_len) != 1) {
                errorstream << "KeypairManager: Failed to extract private key." << std::endl;
                m_private_key.clear();
                EVP_PKEY_free(pkey);
                return false;
        }

        // Extract public key
        size_t pub_len = 0;
        if (EVP_PKEY_get_raw_public_key(pkey, nullptr, &pub_len) != 1) {
                errorstream << "KeypairManager: Failed to query public key size." << std::endl;
                m_private_key.clear();
                EVP_PKEY_free(pkey);
                return false;
        }

        m_public_key.resize(pub_len);
        if (EVP_PKEY_get_raw_public_key(pkey,
                        reinterpret_cast<unsigned char *>(&m_public_key[0]),
                        &pub_len) != 1) {
                errorstream << "KeypairManager: Failed to extract public key." << std::endl;
                m_private_key.clear();
                m_public_key.clear();
                EVP_PKEY_free(pkey);
                return false;
        }

        EVP_PKEY_free(pkey);
        infostream << "KeypairManager: Generated Ed25519 keypair (pubkey_len="
                << m_public_key.size() << ")." << std::endl;
        return true;
#else
        errorstream << "KeypairManager: Cannot generate keypair without OpenSSL!"
                << std::endl;
        return false;
#endif
}

bool KeypairManager::saveKeypair(const std::string &username) const
{
        if (m_private_key.empty() || m_public_key.empty()) {
                errorstream << "KeypairManager::saveKeypair: No keypair to save!" << std::endl;
                return false;
        }

        if (username.empty()) {
                errorstream << "KeypairManager::saveKeypair: No username specified!" << std::endl;
                return false;
        }

        // Ensure the keys directory exists
        if (!fs::PathExists(m_keys_dir)) {
                fs::CreateDir(m_keys_dir);
        }

        std::string keypair_path = getKeypairPath(username);

        // File format: [32 bytes private key seed][32 bytes public key]
        std::string data = m_private_key + m_public_key;

        std::ofstream file(keypair_path, std::ios::binary);
        if (!file.is_open()) {
                errorstream << "KeypairManager::saveKeypair: Failed to open file: "
                        << keypair_path << std::endl;
                return false;
        }

        file.write(data.data(), data.size());
        file.close();

        // Set restrictive file permissions (owner read/write only)
        chmod(keypair_path.c_str(), S_IRUSR | S_IWUSR);

        infostream << "KeypairManager: Saved Ed25519 keypair for '"
                << username << "' to " << keypair_path << std::endl;
        return true;
}

bool KeypairManager::loadKeypair(const std::string &username)
{
        if (username.empty()) {
                errorstream << "KeypairManager::loadKeypair: No username specified!" << std::endl;
                return false;
        }

        std::string keypair_path = getKeypairPath(username);

        std::ifstream file(keypair_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
                errorstream << "KeypairManager::loadKeypair: Failed to open file: "
                        << keypair_path << std::endl;
                return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Expected: 32 (private) + 32 (public) = 64 bytes
        const size_t expected_size = ED25519_PRIVATE_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE;
        if (size != static_cast<std::streamsize>(expected_size)) {
                errorstream << "KeypairManager::loadKeypair: Invalid keypair file size ("
                        << size << ", expected " << expected_size << ")." << std::endl;
                return false;
        }

        std::string data(expected_size, '\0');
        if (!file.read(&data[0], expected_size)) {
                errorstream << "KeypairManager::loadKeypair: Failed to read keypair file."
                        << std::endl;
                return false;
        }

        m_private_key = data.substr(0, ED25519_PRIVATE_KEY_SIZE);
        m_public_key = data.substr(ED25519_PRIVATE_KEY_SIZE, ED25519_PUBLIC_KEY_SIZE);

        infostream << "KeypairManager: Loaded Ed25519 keypair for '"
                << username << "' from " << keypair_path << std::endl;
        return true;
}

bool KeypairManager::migrateLegacyKeypair(const std::string &username)
{
        // Check if the legacy keypair file exists
        if (!fs::PathExists(m_legacy_keypair_path))
                return false;

        // Check if the target already exists (shouldn't happen, caller checks too)
        std::string target_path = getKeypairPath(username);
        if (fs::PathExists(target_path))
                return false;

        // Ensure the keys directory exists
        if (!fs::PathExists(m_keys_dir)) {
                fs::CreateDir(m_keys_dir);
        }

        // Read the legacy keypair file
        std::ifstream src(m_legacy_keypair_path, std::ios::binary);
        if (!src.is_open()) {
                warningstream << "KeypairManager: Failed to open legacy keypair for migration."
                        << std::endl;
                return false;
        }

        std::string data((std::istreambuf_iterator<char>(src)),
                std::istreambuf_iterator<char>());
        src.close();

        // Validate size
        const size_t expected_size = ED25519_PRIVATE_KEY_SIZE + ED25519_PUBLIC_KEY_SIZE;
        if (data.size() != expected_size) {
                warningstream << "KeypairManager: Legacy keypair has invalid size ("
                        << data.size() << "), skipping migration." << std::endl;
                return false;
        }

        // Write to the new per-username file
        std::ofstream dst(target_path, std::ios::binary);
        if (!dst.is_open()) {
                warningstream << "KeypairManager: Failed to write migrated keypair for '"
                        << username << "'." << std::endl;
                return false;
        }

        dst.write(data.data(), data.size());
        dst.close();

        // Set restrictive permissions
        chmod(target_path.c_str(), S_IRUSR | S_IWUSR);

        // Delete the legacy file after successful migration
        fs::DeleteSingleFileOrEmptyDirectory(m_legacy_keypair_path);

        infostream << "KeypairManager: Migrated legacy keypair to per-username file for '"
                << username << "'." << std::endl;
        return true;
}

/// Helper: format current time as ISO 8601 string
static std::string formatISO8601()
{
        std::time_t now = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
        return std::string(buf);
}

void KeypairManager::loadServerUsers() const
{
        if (m_server_users_loaded)
                return;

        m_server_users.clear();

        if (!fs::PathExists(m_server_users_path)) {
                m_server_users_loaded = true;
                return;
        }

        std::ifstream file(m_server_users_path);
        if (!file.is_open()) {
                m_server_users_loaded = true;
                return;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream json_stream(content);

        if (!Json::parseFromStream(builder, json_stream, &root, &errors)) {
                warningstream << "KeypairManager: Failed to parse server users file: "
                        << errors << std::endl;
                m_server_users_loaded = true;
                return;
        }

        if (root.isObject()) {
                for (const auto &key : root.getMemberNames()) {
                        const Json::Value &val = root[key];
                        ServerEntry entry;
                        if (val.isObject()) {
                                // v9.35 format: {"username": "...", "created_at": "...", "last_used_at": "..."}
                                entry.username = val.get("username", "").asString();
                                entry.created_at = val.get("created_at", "").asString();
                                entry.last_used_at = val.get("last_used_at", "").asString();
                        } else if (val.isString()) {
                                // Legacy format (v9.29-v9.34): plain string username
                                entry.username = val.asString();
                                entry.created_at = "";
                                entry.last_used_at = "";
                        }
                        m_server_users[key] = entry;
                }
        }

        m_server_users_loaded = true;
}

void KeypairManager::saveServerUsers() const
{
        Json::Value root(Json::objectValue);
        for (const auto &[server, entry] : m_server_users) {
                Json::Value obj(Json::objectValue);
                obj["username"] = entry.username;
                obj["created_at"] = entry.created_at;
                obj["last_used_at"] = entry.last_used_at;
                root[server] = obj;
        }

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::string content = Json::writeString(builder, root);

        std::ofstream file(m_server_users_path);
        if (!file.is_open()) {
                errorstream << "KeypairManager: Failed to save server users file: "
                        << m_server_users_path << std::endl;
                return;
        }

        file << content;
        file.close();
}

void KeypairManager::rememberServerUser(const std::string &server_address,
        const std::string &username)
{
        loadServerUsers();
        auto it = m_server_users.find(server_address);
        std::string now = formatISO8601();
        if (it != m_server_users.end()) {
                // Existing entry: update username and last_used_at
                it->second.username = username;
                it->second.last_used_at = now;
                // Preserve created_at if it was already set
                if (it->second.created_at.empty())
                        it->second.created_at = now;
        } else {
                // New entry
                ServerEntry entry;
                entry.username = username;
                entry.created_at = now;
                entry.last_used_at = now;
                m_server_users[server_address] = entry;
        }
        saveServerUsers();
}

std::vector<std::pair<std::string, KeypairManager::ServerEntry>> KeypairManager::getServerUserList() const
{
        loadServerUsers();
        std::vector<std::pair<std::string, ServerEntry>> result;
        result.reserve(m_server_users.size());
        for (const auto &[server, entry] : m_server_users) {
                result.emplace_back(server, entry);
        }
        return result;
}

bool KeypairManager::forgetServerUser(const std::string &server_address)
{
        loadServerUsers();
        auto it = m_server_users.find(server_address);
        if (it == m_server_users.end())
                return false;
        m_server_users.erase(it);
        saveServerUsers();
        infostream << "KeypairManager: Forgot server user for " << server_address << std::endl;
        return true;
}

std::string KeypairManager::getServerUser(const std::string &server_address) const
{
        loadServerUsers();
        auto it = m_server_users.find(server_address);
        if (it != m_server_users.end())
                return it->second.username;
        return "";
}

bool KeypairManager::hasServerUser(const std::string &server_address) const
{
        loadServerUsers();
        return m_server_users.find(server_address) != m_server_users.end();
}
