// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include <array>
#include <string>

/// Size of an AES-256 key in bytes
constexpr size_t AES256_KEY_SIZE_D = 32;

/// Size of a GCM nonce base in bytes
constexpr size_t NONCE_BASE_SIZE_D = 4;

/// Result of key derivation for both directions.
struct DerivedDirectionalKeys
{
    /// Client-to-server encryption key
    std::array<u8, AES256_KEY_SIZE_D> c2s_key{};

    /// Server-to-client encryption key
    std::array<u8, AES256_KEY_SIZE_D> s2c_key{};

    /// Client-to-server nonce base
    std::array<u8, NONCE_BASE_SIZE_D> c2s_nonce_base{};

    /// Server-to-client nonce base
    std::array<u8, NONCE_BASE_SIZE_D> s2c_nonce_base{};

    /// Session identifier (hex string derived from key material)
    std::string session_id;

    /// Server fingerprint (derived from key material)
    std::string server_fingerprint;
};

/// Abstract interface for key derivation.
///
/// Decouples key derivation from specific algorithms (HKDF-SHA256),
/// making it possible to test key-dependent logic without real crypto
/// and to swap KDF algorithms.
class IKeyDeriver
{
public:
    virtual ~IKeyDeriver() = default;

    /// Derive directional encryption keys from input keying material.
    ///
    /// @param ikm           Input keying material (e.g., SRP session key)
    /// @param ikm_len       Length of IKM
    /// @param is_server     Whether this is the server side (affects key assignment)
    /// @param version_info  Version prefix for HKDF info strings (e.g., "Luanti v9")
    /// @return Derived directional keys on success
    virtual bool deriveKeys(
        const u8* ikm, size_t ikm_len,
        bool is_server,
        const std::string& version_info,
        DerivedDirectionalKeys& out) = 0;
};
