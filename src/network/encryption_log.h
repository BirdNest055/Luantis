// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "log.h"
#include "irrlichttypes.h"
#include "network/networkprotocol.h"  // for session_t
#include "network/encryption_trace.h"
#include "network/encryption_log_level.h"  // v9.23: EncryptionLogLevel enum (lightweight, no settings.h)

// Forward-declare the log level check function from encryption_config.
// We can't include encryption_config.h here (pulls in settings.h),
// but we need shouldLog() in the macro guards. The linker resolves it.
namespace EncryptionConfig { bool shouldLog(EncryptionLogLevel level); }

#include <string>
#include <sstream>
#include <ctime>

// ============================================================================
// Structured Encryption Logging System
// ============================================================================
//
// This module provides a consistent, structured logging format for all
// encryption-related events. The goal is to make it trivially easy to:
//
//   1. Grep logs for "[ENC]" to find ALL encryption events
//   2. See at a glance whether a connection is SECURE or INSECURE
//   3. Track the full encryption lifecycle per session
//   4. Audit key derivation, activation, and error events
//   5. Get a security summary for each peer connection
//
// Log format:
//   [ENC:<category>] <message> | key=value | key=value | ...
//
// Categories:
//   INIT     - Key derivation and initialization
//   ACTIVATE - Encryption activation (plaintext->encrypted transition)
//   SEND     - Outbound packet encryption events
//   RECV     - Inbound packet decryption events
//   SECURITY - Security state changes and summaries
//   ERROR    - Encryption failures and security violations
//   DISABLE  - Encryption teardown and key zeroing
//   AUDIT    - Periodic security audits and statistics
//
// Usage:
//   ENC_LOG_INIT("Keys derived from SRP session key")
//     << " | peer=" << peer_id
//     << " | session=" << session_id;
//
// This produces:
//   [ENC:INIT] Keys derived from SRP session key | peer=42 | session=a1b2c3...
// ============================================================================

// Category tags for structured log lines
#define ENC_TAG_INIT     "[ENC:INIT] "
#define ENC_TAG_ACTIVATE "[ENC:ACTIVATE] "
#define ENC_TAG_SEND     "[ENC:SEND] "
#define ENC_TAG_RECV     "[ENC:RECV] "
#define ENC_TAG_SECURITY "[ENC:SECURITY] "
#define ENC_TAG_ERROR    "[ENC:ERROR] "
#define ENC_TAG_DISABLE  "[ENC:DISABLE] "
#define ENC_TAG_AUDIT    "[ENC:AUDIT] "
#define ENC_TAG_TRACE    "[ENC:TRACE] "

// ---- Convenience macros for structured encryption logging ----
// Each macro streams to `actionstream` (always logged) or `infostream`
// depending on severity. The format is:
//   [ENC:<CATEGORY>] <message> | key=value | key=value

// v9.23: All enclog macros now respect the `encryption_log_level` setting.
// Log levels required per category:
//   INIT     → ACTION (key derivation is important operational info)
//   ACTIVATE → ACTION (always logged when log level >= action)
//   SEND     → TRACE  (only in trace mode)
//   RECV     → TRACE  (only in trace mode)
//   SECURITY → ACTION (always logged when log level >= action)
//   ERROR    → ERROR  (always logged unless log level = none)
//   DISABLE  → ACTION (always logged when log level >= action)
//   AUDIT    → TRACE  (only in trace mode)
//   TRACE    → TRACE  (only in trace mode)
//
// When encryption_log_level = "none", ALL encryption log output is
// suppressed (even errors) — use only when you want zero log noise.
// When "error", only [ENC:ERROR] lines appear.
// When "action" (default), ACTIVATE/SECURITY/DISABLE/ERROR lines appear.
// When "trace", everything appears including per-packet diagnostics.

// INIT: Key derivation and initialization events
#define enclog_init(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_ACTION)) \
                infostream << ENC_TAG_INIT << msg

// ACTIVATE: Encryption activation events (action level)
#define enclog_activate(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_ACTION)) \
                actionstream << ENC_TAG_ACTIVATE << msg

// SEND: Outbound encryption events (trace level)
#define enclog_send(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_TRACE)) \
                infostream << ENC_TAG_SEND << msg

// RECV: Inbound decryption events (trace level)
#define enclog_recv(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_TRACE)) \
                infostream << ENC_TAG_RECV << msg

// SECURITY: Security state changes (action level)
#define enclog_security(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_ACTION)) \
                actionstream << ENC_TAG_SECURITY << msg

// ERROR: Encryption failures (error level — shown unless log level = none)
#define enclog_error(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_ERROR)) \
                errorstream << ENC_TAG_ERROR << msg

// DISABLE: Encryption teardown (action level)
#define enclog_disable(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_ACTION)) \
                actionstream << ENC_TAG_DISABLE << msg

// AUDIT: Periodic statistics (trace level)
#define enclog_audit(msg) \
        if (EncryptionConfig::shouldLog(ENC_LOG_TRACE)) \
                infostream << ENC_TAG_AUDIT << msg

// TRACE: Detailed per-packet diagnostic tracing — dual output.
// Writes to BOTH actionstream (console + debug.txt) AND the dedicated
// encryption_trace.log file. Use for logging every packet routing
// decision, key state at decision points, hex dumps, etc.
// Only active when encryption_log_level >= trace.
//
// The TraceLine object checks shouldLog() in its destructor. When the
// log level is below trace, the destructor does nothing, so all the
// streaming work is wasted — but the compiler can optimize it away
// since the object has no observable side effects in that case.
//
// Usage:
//   enclog_trace("msg") << EncLog::kv("key", val) << std::endl;
#define enclog_trace(msg) \
        EncLog::TraceLine() << ENC_TAG_TRACE << msg

// ---- Security status banner helpers ----

/// Generate a visual security status banner for a connection.
/// This produces a multi-line block that clearly shows whether
/// the connection is SECURE or INSECURE with all relevant details.
///
/// Example SECURE output:
///   ╔══════════════════════════════════════════════════════════╗
///   ║  CONNECTION SECURE — AES-256-GCM Encryption Active     ║
///   ╠══════════════════════════════════════════════════════════╣
///   ║  Cipher:      AES-256-GCM                              ║
///   ║  Key Exchange: SRP (Secure Remote Password)            ║
///   ║  Session ID:   a1b2c3d4e5f6...                         ║
///   ║  Fingerprint:  SHA256:9f8e7d...                        ║
///   ║  Forward Secrecy: No (SRP-derived keys)                ║
///   ║  Replay Protection: Yes (sliding window)               ║
///   ║  Score: 70/100 (Fair)                                  ║
///   ╚══════════════════════════════════════════════════════════╝
///
/// Example INSECURE output:
///   ╔══════════════════════════════════════════════════════════╗
///   ║  ⚠ CONNECTION INSECURE — No Encryption                 ║
///   ╠══════════════════════════════════════════════════════════╣
///   ║  All traffic is sent as PLAINTEXT UDP                   ║
///   ║  Any network observer can read and modify packets       ║
///   ║  Reason: OpenSSL not linked or SRP key derivation failed║
///   ╚══════════════════════════════════════════════════════════╝

namespace EncLog {

/// Format a key=value pair for structured logging
inline std::string kv(const char* key, const std::string& value)
{
        return std::string(" | ") + key + "=" + value;
}

/// Hex-dump a byte array as a compact string (no spaces, lowercase hex).
/// For tracing packet contents and key material at decision points.
/// @param data  Pointer to the byte array
/// @param len   Number of bytes to dump
/// @param max_bytes  Maximum bytes to include (0 = no limit, default 32)
/// @return Hex string, with "..." appended if truncated
inline std::string hexDump(const u8* data, size_t len, size_t max_bytes = 32)
{
        if (!data || len == 0) return "<empty>";
        size_t show = (max_bytes > 0 && len > max_bytes) ? max_bytes : len;
        std::string result;
        result.reserve(show * 2);
        static const char hex[] = "0123456789abcdef";
        for (size_t i = 0; i < show; i++) {
                result += hex[(data[i] >> 4) & 0x0f];
                result += hex[data[i] & 0x0f];
        }
        if (show < len) result += "...";
        return result;
}

/// Format a u8 byte value as "0xXX"
inline std::string hexByte(u8 b)
{
        static const char hex[] = "0123456789abcdef";
        return std::string("0x") + hex[(b >> 4) & 0x0f] + hex[b & 0x0f];
}

inline std::string kv(const char* key, int value)
{
        return std::string(" | ") + key + "=" + std::to_string(value);
}

inline std::string kv(const char* key, u16 value)
{
        return std::string(" | ") + key + "=" + std::to_string(value);
}

inline std::string kv(const char* key, u32 value)
{
        return std::string(" | ") + key + "=" + std::to_string(value);
}

inline std::string kv(const char* key, u64 value)
{
        return std::string(" | ") + key + "=" + std::to_string(value);
}

inline std::string kv(const char* key, bool value)
{
        return std::string(" | ") + key + "=" + (value ? "yes" : "no");
}

inline std::string kv(const char* key, const char* value)
{
        return std::string(" | ") + key + "=" + value;
}

/// Log the SECURE connection banner with all details
inline void logSecureConnectionBanner(
        const std::string& session_id,
        const std::string& fingerprint,
        bool forward_secrecy,
        bool replay_protection,
        int security_score,
        const std::string& score_label,
        u64 c2s_packets,
        u64 s2c_packets)
{
        if (!EncryptionConfig::shouldLog(ENC_LOG_ACTION))
                return;

        actionstream
                << ENC_TAG_SECURITY
                << "========================================================" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  CONNECTION SECURE -- AES-256-GCM Encryption Active" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "========================================================" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Cipher:           AES-256-GCM (256-bit key)" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Key Exchange:     SRP (Secure Remote Password)" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Authentication:   SRP mutual" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Session ID:       " << session_id << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Fingerprint:      " << fingerprint << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Forward Secrecy:  " << (forward_secrecy ? "Yes" : "No (SRP-derived keys)") << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Replay Protection:" << (replay_protection ? " Yes (sliding window)" : " No") << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Score:            " << security_score << "/100 (" << score_label << ")" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  C2S packets:      " << c2s_packets << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  S2C packets:      " << s2c_packets << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "========================================================" << std::endl;
}

/// Log the INSECURE connection banner with the reason
inline void logInsecureConnectionBanner(const std::string& reason)
{
        if (!EncryptionConfig::shouldLog(ENC_LOG_ACTION))
                return;

        actionstream
                << ENC_TAG_SECURITY
                << "========================================================" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  !! CONNECTION INSECURE -- No Encryption !!" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "========================================================" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  All traffic is sent as PLAINTEXT UDP" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Any network observer can read and modify packets" << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "  Reason: " << reason << std::endl;
        actionstream
                << ENC_TAG_SECURITY
                << "========================================================" << std::endl;
}

/// Log a periodic encryption audit summary for a peer.
/// Called every N packets to give a health check on the encryption.
inline void logEncryptionAudit(
        session_t peer_id,
        bool active,
        const std::string& session_id,
        u64 c2s_packets,
        u64 s2c_packets,
        u64 c2s_auth_failures,
        u64 s2c_auth_failures,
        u64 c2s_replay_attempts,
        u64 s2c_replay_attempts)
{
        enclog_audit("Encryption audit")
                << kv("peer", peer_id)
                << kv("active", active)
                << kv("session", session_id.substr(0, 16))
                << kv("c2s_pkts", c2s_packets)
                << kv("s2c_pkts", s2c_packets)
                << kv("c2s_auth_fail", c2s_auth_failures)
                << kv("s2c_auth_fail", s2c_auth_failures)
                << kv("c2s_replay", c2s_replay_attempts)
                << kv("s2c_replay", s2c_replay_attempts)
                << std::endl;
}

} // namespace EncLog
