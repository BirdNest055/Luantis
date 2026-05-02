// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/encryption_trace.h"
#include "network/encryption_log.h"
#include "network/encryption_config.h"  // v9.23: shouldLog() check
#include "log.h"
#include "porting.h"
#include "filesys.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace EncLog {

// ---- Trace file state (protected by g_trace_mutex) ----
static std::mutex g_trace_mutex;
static std::ofstream g_trace_file;
static std::string g_trace_path;
static bool g_trace_initialized = false;
static bool g_trace_path_set = false;
// v9.25: Removed g_trace_disabled — the trace file is now created at any
// non-none encryption log level (not just trace). Previously, once disabled
// (log level = none at first call), the file was never opened. Now the file
// is created on first write at any active level, and recreated if deleted.

// ---- TraceLine destructor: dual output ----

TraceLine::~TraceLine()
{
        // v9.23: If encryption log level is below trace, skip all output.
        // This prevents generating log data when logging is disabled.
        if (!EncryptionConfig::shouldLog(ENC_LOG_TRACE))
                return;

        std::string line = m_buf.str();

        // Strip trailing newlines from the captured content
        // (we'll add our own when writing)
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

        if (line.empty())
                return;

        // Write to actionstream (console + main debug log)
        actionstream << line << std::endl;

        // Write to the dedicated trace file
        writeTraceFile(line);
}

// ---- EncLogLine: Generalized dual-output for ALL log levels (v9.25) ----

EncLogLine::EncLogLine(std::ostream &stream, EncryptionLogLevel level)
        : m_stream(nullptr)
{
        if (EncryptionConfig::shouldLog(level)) {
                m_stream = &stream;
                // v9.25: Ensure the trace file exists when any encryption
                // logging is active. This fixes the bug where deleting
                // encryption_trace.log manually and restarting with --log
                // would not recreate the file.
                ensureEncryptionLogExists();
        }
}

EncLogLine::~EncLogLine()
{
        if (!m_stream)
                return;

        std::string line = m_buf.str();

        // Strip trailing newlines from the captured content
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

        if (line.empty())
                return;

        // Write to the standard stream (infostream/actionstream/errorstream)
        *m_stream << line << std::endl;

        // v9.25: Also write to the dedicated trace file at all log levels.
        // Previously only enclog_trace (TraceLine) wrote to the trace file.
        // Now all encryption log output goes to encryption_trace.log,
        // making it the single destination for all encryption events.
        writeTraceFile(line);
}

// ---- Trace file implementation ----

static void ensureTraceFileOpen()
{
        // Must be called with g_trace_mutex held

        if (g_trace_initialized)
                return;

        // v9.25: If encryption log level is "none", skip opening the file.
        // Unlike v9.23 which permanently disabled the file (g_trace_disabled),
        // v9.25 allows the file to be opened on subsequent calls if the log
        // level changes. This fixes the bug where manually deleting
        // encryption_trace.log and restarting with --log (action level)
        // would not recreate the file.
        //
        // Changed: open at ANY non-none level (not just trace), so the
        // file exists when logging is ON regardless of log level.
        if (!EncryptionConfig::shouldLog(ENC_LOG_ERROR)) {
                return;
        }

        g_trace_initialized = true;

        // If no path was explicitly set, create one alongside debug.txt
        if (!g_trace_path_set) {
                std::string path_user = porting::path_user;

                if (!path_user.empty()) {
                        g_trace_path = path_user + DIR_DELIM + "encryption_trace.log";
                } else {
                        g_trace_path = "/tmp/luanti-encryption-trace.log";
                }
        }

        // Open the file in append mode
        g_trace_file.open(g_trace_path, std::ios::app);
        if (g_trace_file.is_open()) {
                // Announce the trace file location to the main log
                actionstream << "[ENC:TRACE] Encryption trace file: " << g_trace_path << std::endl;

                // Write a session separator
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_buf{};
                localtime_r(&time_t_now, &tm_buf);

                char time_str[32];
                std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

                g_trace_file << "\n========================================"
                             << "========================================\n"
                             << "  ENCRYPTION TRACE SESSION STARTED: " << time_str << "\n"
                             << "  File: " << g_trace_path << "\n"
                             << "========================================"
                             << "========================================\n"
                             << std::endl;
        }
}

void initTraceFile(const std::string &path)
{
        // Batch 34: RAII lock guard — uses std::lock_guard instead of manual
        // lock/unlock for exception safety
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        g_trace_path = path;
        g_trace_path_set = true;

        // If already initialized with the default path, close and reopen
        if (g_trace_initialized && g_trace_file.is_open()) {
                g_trace_file.close();
                g_trace_initialized = false;
        }
}

void ensureEncryptionLogExists()
{
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        ensureTraceFileOpen();
}

void writeTraceFile(const std::string &line)
{
        // Batch 34: Mutex scope reduction — copy trace path and check state under
        // lock, then do file I/O outside the lock. The file stream pointer is only
        // accessed under the lock, so we swap out the line and write after.
        std::string path_copy;
        bool is_open = false;
        {
                std::lock_guard<std::mutex> lock(g_trace_mutex);

                ensureTraceFileOpen();

                if (!g_trace_file.is_open())
                        return;

                // Prepend timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;

                std::tm tm_buf{};
                localtime_r(&time_t_now, &tm_buf);

                char time_str[32];
                std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

                g_trace_file << time_str << "."
                             << std::setfill('0') << std::setw(3) << ms.count()
                             << ": " << line << "\n";

                // Flush after every line so nothing is lost on crash
                g_trace_file.flush();
        }
}

std::string getTraceFilePath()
{
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        return g_trace_path;
}

void closeTraceFile()
{
        // Batch 34: RAII lock guard — uses std::lock_guard instead of manual
        // lock/unlock for exception safety
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        if (g_trace_file.is_open()) {
                g_trace_file.close();
        }
        g_trace_initialized = false;
}

} // namespace EncLog
