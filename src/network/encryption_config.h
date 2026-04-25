/*
 * encryption_config.h — Modular encryption toggle for Clawtest v9.3+
 *
 * This module provides a centralized, single-point-of-truth system for
 * controlling whether encryption is active on a connection. All code
 * that checks or enforces encryption policy should go through this API.
 *
 * Design goals:
 *   1. Single place to check "should encryption be active?"
 *   2. Easy to toggle encryption on/off from config or code
 *   3. Clear logging for every encryption decision
 *   4. Testable from scripts without rebuilding
 */

#pragma once

#include <string>
#include "irrlichttypes.h"

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

} // namespace EncryptionConfig
