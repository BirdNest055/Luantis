/*
 * encryption_config.cpp — Modular encryption toggle for Clawtest v9.3+
 *
 * Implementation of the centralized encryption policy manager.
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

} // namespace EncryptionConfig
