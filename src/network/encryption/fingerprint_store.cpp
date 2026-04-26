// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/encryption/fingerprint_store.h"
#include "encryption_log.h"
#include <fstream>

// ============================================================================
// v9.1: FingerprintStore — Server Fingerprint Pinning
// ============================================================================

std::string FingerprintStore::makeKey(const std::string &server_address, u16 port)
{
        return server_address + ":" + std::to_string(port);
}

bool FingerprintStore::load(const std::string &filepath)
{
        std::ifstream file(filepath);
        if (!file.is_open()) {
                // File doesn't exist yet — that's OK (first run)
                return true;
        }

        std::string line;
        while (std::getline(file, line)) {
                // Skip empty lines and comments
                if (line.empty() || line[0] == '#')
                        continue;

                // Format: <server_address>:<port> <fingerprint>
                // Find the space separating key from fingerprint
                size_t space_pos = line.find(' ');
                if (space_pos == std::string::npos)
                        continue;

                std::string key = line.substr(0, space_pos);
                std::string fingerprint = line.substr(space_pos + 1);

                // Trim whitespace from fingerprint
                while (!fingerprint.empty() && (fingerprint.back() == '\r' || fingerprint.back() == ' '))
                        fingerprint.pop_back();

                if (!key.empty() && !fingerprint.empty()) {
                        m_fingerprints[key] = fingerprint;
                }
        }

        enclog_init("FingerprintStore loaded")
                << EncLog::kv("filepath", filepath)
                << EncLog::kv("entries", (u32)m_fingerprints.size())
                << std::endl;

        return true;
}

bool FingerprintStore::save(const std::string &filepath) const
{
        std::ofstream file(filepath);
        if (!file.is_open()) {
                enclog_error("FingerprintStore: failed to open file for writing")
                        << EncLog::kv("filepath", filepath)
                        << std::endl;
                return false;
        }

        file << "# Luanti v9.1 Server Fingerprint Store\n";
        file << "# Format: <address>:<port> <fingerprint>\n";
        file << "# DO NOT EDIT — managed by Luanti encryption subsystem\n\n";

        for (const auto &entry : m_fingerprints) {
                file << entry.first << " " << entry.second << "\n";
        }

        enclog_init("FingerprintStore saved")
                << EncLog::kv("filepath", filepath)
                << EncLog::kv("entries", (u32)m_fingerprints.size())
                << std::endl;

        return true;
}

int FingerprintStore::verify(const std::string &server_address, u16 port,
        const std::string &fingerprint) const
{
        std::string key = makeKey(server_address, port);
        auto it = m_fingerprints.find(key);

        if (it == m_fingerprints.end()) {
                // Unknown server — first connection (TOFU)
                return 0;
        }

        if (it->second == fingerprint) {
                // Fingerprint matches — verified!
                return 1;
        }

        // Fingerprint MISMATCH — possible MITM!
        enclog_error("FINGERPRINT MISMATCH — possible MITM attack!")
                << EncLog::kv("server", server_address)
                << EncLog::kv("port", (u16)port)
                << EncLog::kv("expected", it->second)
                << EncLog::kv("got", fingerprint)
                << std::endl;
        return -1;
}

void FingerprintStore::record(const std::string &server_address, u16 port,
        const std::string &fingerprint)
{
        std::string key = makeKey(server_address, port);
        m_fingerprints[key] = fingerprint;

        enclog_init("Server fingerprint recorded (TOFU)")
                << EncLog::kv("server", server_address)
                << EncLog::kv("port", (u16)port)
                << EncLog::kv("fingerprint", fingerprint)
                << std::endl;
}

std::string FingerprintStore::getStoredFingerprint(const std::string &server_address, u16 port) const
{
        std::string key = makeKey(server_address, port);
        auto it = m_fingerprints.find(key);
        if (it != m_fingerprints.end())
                return it->second;
        return "";
}
