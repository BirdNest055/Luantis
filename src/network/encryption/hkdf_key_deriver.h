// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "network/encryption/ikey_deriver.h"

/// HKDF-SHA256 based key derivation.
///
/// Implements the IKeyDeriver interface using HKDF-SHA256 for
/// key derivation. This is the same algorithm used in the existing
/// initFromSRPSessionKey(), rotateKeys(), and mixECDHSecretIntoKeys()
/// functions, but unified into a single parameterized implementation
/// to eliminate ~200 lines of code duplication.
class HKDFKeyDeriver : public IKeyDeriver
{
public:
    bool deriveKeys(
        const u8* ikm, size_t ikm_len,
        bool is_server,
        const std::string& version_info,
        DerivedDirectionalKeys& out) override;
};
