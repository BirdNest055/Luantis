// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "encryption_logger.h"
#include "log.h"
#include <iostream>

// ---- Singleton ----

EncryptionLogger& EncryptionLogger::instance()
{
	static EncryptionLogger s_instance;
	return s_instance;
}

// ---- Level tag mapping ----

const char* EncryptionLogger::getLevelTag(Level level)
{
	switch (level) {
	case Level::Init:     return "[ENC:INIT] ";
	case Level::Activate: return "[ENC:ACTIVATE] ";
	case Level::Send:     return "[ENC:SEND] ";
	case Level::Recv:     return "[ENC:RECV] ";
	case Level::Security: return "[ENC:SECURITY] ";
	case Level::Error:    return "[ENC:ERROR] ";
	case Level::Disable:  return "[ENC:DISABLE] ";
	case Level::Audit:    return "[ENC:AUDIT] ";
	}
	return "[ENC:UNKNOWN] ";
}

// ---- Internal helper: build the structured log line ----

static std::string buildLogLine(
	EncryptionLogger::Level level,
	session_t peer_id,
	bool has_peer,
	const std::string& message,
	std::initializer_list<EncryptionLogger::KV> fields)
{
	std::ostringstream oss;
	oss << EncryptionLogger::getLevelTag(level);

	if (has_peer)
		oss << "peer=" << peer_id << " ";

	// Quote the message if it contains spaces
	oss << "msg=\"" << message << "\"";

	for (const auto& kv : fields)
		oss << " " << kv.key << "=" << kv.value;

	return oss.str();
}

// ---- Log with peer context ----

void EncryptionLogger::log(Level level, session_t peer_id, const std::string& message,
	std::initializer_list<KV> fields)
{
	if (!m_enabled)
		return;

	if (static_cast<u8>(level) < static_cast<u8>(m_min_level))
		return;

	std::string line = buildLogLine(level, peer_id, true, message, fields);

	std::lock_guard<std::mutex> lock(m_mutex);

	switch (level) {
	case Level::Error:
		errorstream << line << std::endl;
		break;
	case Level::Security:
		actionstream << line << std::endl;
		break;
	default:
		infostream << line << std::endl;
		break;
	}
}

// ---- Log without peer context ----

void EncryptionLogger::log(Level level, const std::string& message,
	std::initializer_list<KV> fields)
{
	if (!m_enabled)
		return;

	if (static_cast<u8>(level) < static_cast<u8>(m_min_level))
		return;

	std::string line = buildLogLine(level, 0, false, message, fields);

	std::lock_guard<std::mutex> lock(m_mutex);

	switch (level) {
	case Level::Error:
		errorstream << line << std::endl;
		break;
	case Level::Security:
		actionstream << line << std::endl;
		break;
	default:
		infostream << line << std::endl;
		break;
	}
}
