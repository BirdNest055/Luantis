// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include "networkprotocol.h"
#include <string>
#include <sstream>
#include <initializer_list>
#include <mutex>

/// Structured encryption logging system.
///
/// Replaces the enclog_* macros with a proper logger that supports:
/// - Runtime enable/disable
/// - Level-based filtering
/// - Thread-safe operation
/// - Test-friendly (output can be redirected/captured)
/// - Structured key-value pairs for machine parsing
class EncryptionLogger
{
public:
    /// Log level for encryption events
    enum class Level : u8 {
        Init = 0,       // Encryption initialization
        Activate = 1,   // Encryption activation
        Send = 2,       // Packet encryption (send side)
        Recv = 3,       // Packet decryption (receive side)
        Security = 4,   // Security state changes
        Error = 5,      // Security errors (auth failures, etc.)
        Disable = 6,    // Encryption disabled
        Audit = 7       // Periodic audit logging
    };

    /// Key-value pair for structured logging
    struct KV {
        std::string key;
        std::string value;
        KV(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}
        KV(std::string k, int v) : key(std::move(k)), value(std::to_string(v)) {}
        KV(std::string k, u64 v) : key(std::move(k)), value(std::to_string(v)) {}
        KV(std::string k, bool v) : key(std::move(k)), value(v ? "yes" : "no") {}
    };

    /// Get the singleton instance
    static EncryptionLogger& instance();

    /// Log a message with level, peer, and key-value pairs
    void log(Level level, session_t peer_id, const std::string& message,
             std::initializer_list<KV> fields = {});

    /// Log a message with level only (no peer context)
    void log(Level level, const std::string& message,
             std::initializer_list<KV> fields = {});

    /// Configuration
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setMinLevel(Level level) { m_min_level = level; }
    bool isEnabled() const { return m_enabled; }

    /// Get the tag string for a log level
    static const char* getLevelTag(Level level);

private:
    EncryptionLogger() = default;
    bool m_enabled = true;
    Level m_min_level = Level::Init;
    std::mutex m_mutex;
};
