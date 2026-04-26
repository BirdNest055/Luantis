// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include <string>
#include <sstream>
#include <ostream>
#include "network/encryption_log_level.h"  // v9.25: EncLogLine needs EncryptionLogLevel

// Forward-declare the log level check function from encryption_config.
// We can't include encryption_config.h here (pulls in settings.h),
// but we need shouldLog() in EncLogLine. The linker resolves it.
namespace EncryptionConfig { bool shouldLog(EncryptionLogLevel level); }

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

/// Generalized dual-output log stream for ALL encryption log levels (v9.25).
///
/// Like TraceLine, but works at any encryption log level (error, action,
/// or trace). Writes to both the specified standard stream AND the dedicated
/// encryption_trace.log file. This ensures that encryption_trace.log
/// contains ALL encryption events, not just trace-level ones.
///
/// When the encryption log level is below the requested level, the destructor
/// does nothing (the object has no observable side effects in that case).
///
/// Usage via the enclog_* macros:
///   enclog_init("message") << EncLog::kv("key", value) << std::endl;
///
/// The EncLogLine object captures all streamed content, then on destruction
/// (at the semicolon) writes it to both the standard stream and the trace file.
class EncLogLine {
        std::ostringstream m_buf;
        std::ostream *m_stream;  // nullptr if logging is disabled at this level

public:
        /// Construct with a target stream and minimum log level.
        /// If shouldLog(level) is false, m_stream is set to nullptr and
        /// the destructor does nothing — zero overhead.
        EncLogLine(std::ostream &stream, EncryptionLogLevel level);

        ~EncLogLine();

        /// Stream any value type (strings, numbers, kv() results, etc.)
        template<typename T>
        EncLogLine& operator<<(const T& val)
        {
                if (m_stream)
                        m_buf << val;
                return *this;
        }

        /// Stream manipulators like std::endl
        EncLogLine& operator<<(std::ostream& (*manip)(std::ostream&))
        {
                if (m_stream)
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
/// Called automatically by TraceLine and EncLogLine destructors.
void writeTraceFile(const std::string &line);

/// Get the current trace file path (for diagnostics).
std::string getTraceFilePath();

/// v9.25: Ensure the encryption trace log file exists.
/// Creates the file if encryption_log_level != none, even before any
/// log output is generated. This fixes the bug where manually deleting
/// encryption_trace.log and restarting with --log would not recreate
/// the file. Thread-safe.
void ensureEncryptionLogExists();

/// v9.25: Close the trace file and reset initialization state.
/// Allows the file to be reopened (e.g., if deleted externally).
/// Thread-safe.
void closeTraceFile();

} // namespace EncLog
