// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include "util/pointer.h"
#include <optional>
#include <string>

namespace con { class PeerEncryptionState; }

/// Abstract interface for packet encryption/decryption.
///
/// This interface decouples the transport layer (ConnectionSendThread,
/// ConnectionReceiveThread) from the specific encryption implementation
/// (currently AES-256-GCM). The transport layer only depends on this
/// interface, making it possible to:
/// - Test transport logic with a mock encryptor
/// - Swap cipher suites (e.g., add ChaCha20-Poly1305)
/// - Measure encryption overhead independently
class IPacketEncryptor
{
public:
    virtual ~IPacketEncryptor() = default;

    /// Encrypt a plaintext packet for the given peer.
    ///
    /// @param peer_id    The peer to encrypt for (look up encryption state)
    /// @param plaintext  The plaintext packet data (including base header)
    /// @return The encrypted packet with 0x80 flag prepended, or std::nullopt on failure
    virtual std::optional<SharedBuffer<u8>> encrypt(
        session_t peer_id,
        const SharedBuffer<u8>& plaintext) = 0;

    /// Decrypt a ciphertext packet from the given peer.
    ///
    /// @param peer_id     The peer that sent the packet
    /// @param ciphertext  The encrypted packet data (after 0x80 flag)
    /// @return The decrypted plaintext, or std::nullopt on auth failure
    virtual std::optional<SharedBuffer<u8>> decrypt(
        session_t peer_id,
        const SharedBuffer<u8>& ciphertext) = 0;

    /// Check if a peer has active encryption.
    ///
    /// @param peer_id  The peer to check
    /// @return true if encryption is active and packets should be encrypted
    virtual bool isEncrypted(session_t peer_id) const = 0;

    /// Check if a packet's first byte indicates encryption (0x80 flag).
    ///
    /// @param data    Pointer to packet data
    /// @param size    Size of packet data
    /// @return true if the 0x80 encrypted flag is set
    virtual bool isEncryptedFlag(const u8* data, size_t size) const = 0;
};
