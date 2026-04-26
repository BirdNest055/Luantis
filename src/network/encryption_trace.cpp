// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/encryption_trace.h"
#include "network/encryption_log.h"
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

// ---- TraceLine destructor: dual output ----

TraceLine::~TraceLine()
{
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

// ---- Trace file implementation ----

static void ensureTraceFileOpen()
{
        // Must be called with g_trace_mutex held

        if (g_trace_initialized)
                return;

        g_trace_initialized = true;

        // If no path was explicitly set, create one alongside debug.txt
        if (!g_trace_path_set) {
                std::string path_user;
                try {
                        path_user = porting::path_user;
                } catch (...) {
                        // If porting::path_user isn't available yet, fall back to /tmp
                        path_user = "/tmp";
                }

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
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        g_trace_path = path;
        g_trace_path_set = true;

        // If already initialized with the default path, close and reopen
        if (g_trace_initialized && g_trace_file.is_open()) {
                g_trace_file.close();
                g_trace_initialized = false;
        }
}

void writeTraceFile(const std::string &line)
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

std::string getTraceFilePath()
{
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        return g_trace_path;
}

} // namespace EncLog
