/*
 * encryption_config.h — Modular encryption toggle for Clawtest v9.3+
 *
 * This module provides a centralized, single-point-of-truth system for
 * controlling whether encryption is active on a connection. All code
 * that checks or enforces encryption policy should go through this API.
 *
 * v9.23: Added encryption log level control. The `encryption_log_level`
 * setting controls which [ENC:...] log lines are emitted:
 *   "none"    — suppress ALL encryption log output (except hard crashes)
 *   "error"   — only [ENC:ERROR] lines
 *   "action"  — ACTIVATE, SECURITY, ERROR, DISABLE (default)
 *   "trace"   — everything including TRACE and per-packet diagnostics
 *
 * Design goals:
 *   1. Single place to check "should encryption be active?"
 *   2. Easy to toggle encryption on/off from config or code
 *   3. Clear logging for every encryption decision
 *   4. Testable from scripts without rebuilding
 *   5. v9.23: Controllable log verbosity for production vs debugging
 */

#pragma once

#include <string>
#include "irrlichttypes.h"
#include "network/encryption_log_level.h"  // v9.23: EncryptionLogLevel enum

/**
 * EncryptionConfig — Centralized encryption policy manager.
 *
 * Reads the `secure_connection` setting from g_settings and provides
 * a consistent answer to "should encryption be activated?" for both
 * server and client code paths.
 */
namespace EncryptionConfig {

/**
 * Returns true if encryption should be activated for connections.
 *
 * This reads the `secure_connection` setting from the global config.
 * Default is true (secure).
 *
 * When false:
 *   - SRP authentication still runs (for password verification)
 *   - But the SRP session key is NOT used to activate AES-256-GCM
 *   - All game traffic is sent as plaintext
 *
 * When true:
 *   - SRP authentication runs
 *   - The SRP session key is used to derive AES-256-GCM keys
 *   - All game traffic is encrypted after auth
 */
bool shouldEncrypt();

/**
 * Returns the current encryption mode as a human-readable string.
 * Either "secure" or "insecure".
 */
std::string getModeString();

/**
 * Log the encryption decision for a connection.
 *
 * @param peer_id      The peer ID of the connection
 * @param is_server    True if called from server code
 * @param activated    True if encryption was actually activated
 */
void logEncryptionDecision(u16 peer_id, bool is_server, bool activated);

/**
 * Get the security flags for TOCLIENT_HELLO packet.
 *
 * When secure_connection = true:  ENCRYPTION_SUPPORTED | AUTHENTICATED
 * When secure_connection = false: 0 (no flags)
 *
 * @return Bitfield of ConnectionSecurityFlags
 */
u8 getSecurityFlags();

// ---- v9.23: Encryption log level control ----

/**
 * Get the current encryption log level.
 * Reads `encryption_log_level` from g_settings.
 * Default: "action" (ENC_LOG_ACTION).
 */
EncryptionLogLevel getLogLevel();

/**
 * Check if a given encryption log category should be emitted
 * at the current log level.
 *
 * @param level  The minimum level required for this log line
 * @return true if the log line should be emitted
 */
bool shouldLog(EncryptionLogLevel level);

/**
 * Get the current log level as a human-readable string.
 * One of: "none", "error", "action", "trace"
 */
std::string getLogLevelString();

/**
 * Parse a log level string to the enum value.
 * Returns ENC_LOG_ACTION for unrecognized values (safe default).
 *
 * @param str  One of: "none", "error", "action", "trace"
 */
EncryptionLogLevel parseLogLevel(const std::string &str);

} // namespace EncryptionConfig
