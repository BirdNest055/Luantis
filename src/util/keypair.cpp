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
        , m_keypair_path(keydir + DIR_DELIM + "client_ed25519_key")
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

bool KeypairManager::ensureKeypair()
{
        if (hasKeypair())
                return true;

        // Try loading an existing keypair first
        if (fs::PathExists(m_keypair_path)) {
                if (loadKeypair())
                        return true;
                // If loading failed, fall through to generate a new one
                warningstream << "KeypairManager: Failed to load existing keypair, "
                        << "generating a new one." << std::endl;
        }

        // Generate a new keypair
        if (!generateKeypair()) {
                errorstream << "KeypairManager: Failed to generate Ed25519 keypair!" << std::endl;
                return false;
        }

        if (!saveKeypair()) {
                errorstream << "KeypairManager: Failed to save Ed25519 keypair!" << std::endl;
                return false;
        }

        m_is_new_keypair = true;
        infostream << "KeypairManager: Generated new Ed25519 keypair." << std::endl;
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

        // Ed25519: one-shot sign (no streaming needed, but we use Init/Update/Final
        // for API compatibility)
        if (EVP_DigestSignInit(mdctx, nullptr, nullptr, nullptr, pkey) != 1) {
                errorstream << "KeypairManager::sign: DigestSignInit failed." << std::endl;
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return "";
        }

        if (EVP_DigestSignUpdate(mdctx, message.data(), message.size()) != 1) {
                errorstream << "KeypairManager::sign: DigestSignUpdate failed." << std::endl;
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return "";
        }

        size_t sig_len = ED25519_SIGNATURE_SIZE;
        std::string signature(sig_len, '\0');

        if (EVP_DigestSignFinal(mdctx,
                        reinterpret_cast<unsigned char *>(&signature[0]),
                        &sig_len) != 1) {
                errorstream << "KeypairManager::sign: DigestSignFinal failed." << std::endl;
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

        if (EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pkey) != 1) {
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return false;
        }

        if (EVP_DigestVerifyUpdate(mdctx, message.data(), message.size()) != 1) {
                EVP_MD_CTX_free(mdctx);
                EVP_PKEY_free(pkey);
                return false;
        }

        int result = EVP_DigestVerifyFinal(mdctx,
                reinterpret_cast<const unsigned char *>(signature.data()),
                signature.size());

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

bool KeypairManager::keypairFileExists() const
{
        return fs::PathExists(m_keypair_path);
}

bool KeypairManager::regenerateKeypair()
{
        // Clear existing keypair from memory
        if (!m_private_key.empty()) {
                for (char &c : m_private_key) {
                        c = '\0';
                }
                m_private_key.clear();
        }
        m_public_key.clear();

        // Delete the old keypair file
        if (fs::PathExists(m_keypair_path)) {
                fs::DeleteSingleFileOrEmptyDirectory(m_keypair_path);
        }

        // Generate a new keypair
        if (!generateKeypair()) {
                errorstream << "KeypairManager::regenerateKeypair: Failed to generate new keypair!"
                        << std::endl;
                return false;
        }

        if (!saveKeypair()) {
                errorstream << "KeypairManager::regenerateKeypair: Failed to save new keypair!"
                        << std::endl;
                return false;
        }

        m_is_new_keypair = true;
        infostream << "KeypairManager: Regenerated Ed25519 keypair." << std::endl;
        return true;
}

std::vector<std::pair<std::string, std::string>> KeypairManager::getServerUserList() const
{
        loadServerUsers();
        std::vector<std::pair<std::string, std::string>> result;
        result.reserve(m_server_users.size());
        for (const auto &[server, username] : m_server_users) {
                result.emplace_back(server, username);
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

bool KeypairManager::saveKeypair() const
{
        if (m_private_key.empty() || m_public_key.empty()) {
                errorstream << "KeypairManager::saveKeypair: No keypair to save!" << std::endl;
                return false;
        }

        // Ensure directory exists
        if (!fs::PathExists(m_keydir)) {
                fs::CreateDir(m_keydir);
        }

        // File format: [32 bytes private key seed][32 bytes public key]
        std::string data = m_private_key + m_public_key;

        std::ofstream file(m_keypair_path, std::ios::binary);
        if (!file.is_open()) {
                errorstream << "KeypairManager::saveKeypair: Failed to open file: "
                        << m_keypair_path << std::endl;
                return false;
        }

        file.write(data.data(), data.size());
        file.close();

        // Set restrictive file permissions (owner read/write only)
        chmod(m_keypair_path.c_str(), S_IRUSR | S_IWUSR);

        infostream << "KeypairManager: Saved Ed25519 keypair to "
                << m_keypair_path << std::endl;
        return true;
}

bool KeypairManager::loadKeypair()
{
        std::ifstream file(m_keypair_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
                errorstream << "KeypairManager::loadKeypair: Failed to open file: "
                        << m_keypair_path << std::endl;
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

        infostream << "KeypairManager: Loaded Ed25519 keypair from "
                << m_keypair_path << std::endl;
        return true;
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
                        m_server_users[key] = root[key].asString();
                }
        }

        m_server_users_loaded = true;
}

void KeypairManager::saveServerUsers() const
{
        Json::Value root(Json::objectValue);
        for (const auto &[server, username] : m_server_users) {
                root[server] = username;
        }

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
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
        m_server_users[server_address] = username;
        saveServerUsers();
}

std::string KeypairManager::getServerUser(const std::string &server_address) const
{
        loadServerUsers();
        auto it = m_server_users.find(server_address);
        if (it != m_server_users.end())
                return it->second;
        return "";
}

bool KeypairManager::hasServerUser(const std::string &server_address) const
{
        loadServerUsers();
        return m_server_users.find(server_address) != m_server_users.end();
}
