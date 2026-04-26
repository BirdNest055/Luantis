// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include <string>
#include <sstream>

namespace EncLog {

/// Dual-output trace stream: writes to both actionstream (console/log)
/// AND a dedicated encryption_trace.log file alongside debug.txt.
///
/// Usage via the enclog_trace macro:
///   enclog_trace("message") << EncLog::kv("key", value) << std::endl;
///
/// The TraceLine object captures all streamed content, then on destruction
/// (at the semicolon) writes it to both actionstream and the trace file.
class TraceLine {
	std::ostringstream m_buf;

public:
	TraceLine() = default;

	~TraceLine();

	/// Stream any value type (strings, numbers, kv() results, etc.)
	template<typename T>
	TraceLine& operator<<(const T& val)
	{
		m_buf << val;
		return *this;
	}

	/// Stream manipulators like std::endl
	TraceLine& operator<<(std::ostream& (*manip)(std::ostream&))
	{
		m_buf << manip;
		return *this;
	}
};

/// Initialize the trace file at a specific path.
/// Call once at startup (from main.cpp or the encryption init path).
/// If never called, the trace file will be created lazily on first use
/// alongside the main debug.txt log (using porting::path_user).
void initTraceFile(const std::string &path);

/// Write a trace line to the trace file.
/// Thread-safe (uses internal mutex).
/// Called automatically by TraceLine destructor.
void writeTraceFile(const std::string &line);

/// Get the current trace file path (for diagnostics).
std::string getTraceFilePath();

} // namespace EncLog
