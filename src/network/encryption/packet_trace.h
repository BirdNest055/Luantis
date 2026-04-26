// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <optional>

/// Packet tracing system for debugging and observability.
///
/// Tracks packets through the full lifecycle:
/// send → encrypt → socket → receive → decrypt → dispatch
///
/// Each trace entry records the packet's phase, timestamp, and
/// a summary. Traces can be queried by peer_id, phase, or time range.
class PacketTrace
{
public:
    /// Direction of a packet
    enum class Direction : u8 {
        Sent,
        Received
    };

    /// Phase in the packet lifecycle
    enum class Phase : u8 {
        Queued,        // Packet queued for sending
        Encrypted,     // Packet encrypted (send side)
        Raw,           // Raw bytes on the wire
        Decrypted,     // Packet decrypted (receive side)
        Dispatched,    // Packet dispatched to handler
        Dropped        // Packet dropped (error/timeout)
    };

    /// A single trace entry
    struct Entry {
        u64 timestamp_ms;       // Milliseconds since epoch
        u64 trace_id;           // Unique trace ID (correlation)
        session_t peer_id;      // Source/destination peer
        u8 channel;             // Channel number
        Direction direction;    // Sent or received
        Phase phase;            // Current lifecycle phase
        std::string summary;    // Human-readable description
    };

    /// Record a trace entry
    void record(Entry entry);

    /// Query traces by peer_id
    std::vector<Entry> queryByPeer(session_t peer_id, size_t max_results = 100) const;

    /// Query traces by phase
    std::vector<Entry> queryByPhase(Phase phase, size_t max_results = 100) const;

    /// Get all traces (up to max_results)
    std::vector<Entry> getAll(size_t max_results = 1000) const;

    /// Enable/disable tracing
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    /// Clear all trace entries
    void clear();

    /// Get the number of stored traces
    size_t size() const;

    /// Generate a new unique trace ID
    u64 nextTraceId();

    /// Convert Phase to string
    static const char* phaseToString(Phase phase);
    static const char* directionToString(Direction dir);

private:
    bool m_enabled = false;  // Disabled by default (performance)
    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
    u64 m_next_trace_id = 1;
    static constexpr size_t MAX_ENTRIES = 10000;  // Ring buffer limit
};
