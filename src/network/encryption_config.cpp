/*
 * encryption_config.cpp — Modular encryption toggle for Clawtest v9.3+
 *
 * Implementation of the centralized encryption policy manager.
 * v9.23: Added encryption log level control.
 */

#include "encryption_config.h"
#include "connection_security.h"
#include "settings.h"
#include "log.h"

namespace EncryptionConfig {

bool shouldEncrypt()
{
	// Read from global settings; default is true (secure)
	return g_settings->getBool("secure_connection");
}

std::string getModeString()
{
	return shouldEncrypt() ? "secure" : "insecure";
}

void logEncryptionDecision(u16 peer_id, bool is_server, bool activated)
{
	const char *role = is_server ? "server" : "client";

	if (shouldEncrypt()) {
		if (activated) {
			infostream << "[ENC:CONFIG] Encryption ACTIVATED for peer "
				<< peer_id << " (role=" << role
				<< ", mode=secure, cipher=AES-256-GCM)" << std::endl;
		} else {
			warningstream << "[ENC:CONFIG] Encryption FAILED for peer "
				<< peer_id << " (role=" << role
				<< ", mode=secure, reason=SRP_key_exchange_failed)" << std::endl;
		}
	} else {
		infostream << "[ENC:CONFIG] Encryption DISABLED for peer "
			<< peer_id << " (role=" << role
			<< ", mode=insecure, reason=secure_connection=false)" << std::endl;
	}
}

u8 getSecurityFlags()
{
	u8 flags = 0;

	if (shouldEncrypt()) {
		// Use the canonical flag values from connection_security.h
		flags |= ConnectionSecurityFlags::ENCRYPTION_SUPPORTED;
		flags |= ConnectionSecurityFlags::AUTHENTICATED;
	}

	// When shouldEncrypt() is false, flags = 0 (no encryption advertised)

	return flags;
}

// ---- v9.23: Encryption log level control ----

EncryptionLogLevel parseLogLevel(const std::string &str)
{
	if (str == "none")
		return ENC_LOG_NONE;
	if (str == "error")
		return ENC_LOG_ERROR;
	if (str == "action")
		return ENC_LOG_ACTION;
	if (str == "trace")
		return ENC_LOG_TRACE;
	// Unrecognized: safe default is "action"
	return ENC_LOG_ACTION;
}

EncryptionLogLevel getLogLevel()
{
	std::string level_str = g_settings->get("encryption_log_level");
	return parseLogLevel(level_str);
}

bool shouldLog(EncryptionLogLevel level)
{
	return getLogLevel() >= level;
}

std::string getLogLevelString()
{
	switch (getLogLevel()) {
	case ENC_LOG_NONE:   return "none";
	case ENC_LOG_ERROR:  return "error";
	case ENC_LOG_ACTION: return "action";
	case ENC_LOG_TRACE:  return "trace";
	default:             return "action";
	}
}

} // namespace EncryptionConfig
