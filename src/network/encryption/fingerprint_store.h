// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include <string>
#include <map>

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
