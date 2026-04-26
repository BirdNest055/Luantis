// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "packet_trace.h"
#include <chrono>
#include <algorithm>

// ---- String converters ----

const char* PacketTrace::phaseToString(Phase phase)
{
	switch (phase) {
	case Phase::Queued:     return "Queued";
	case Phase::Encrypted:  return "Encrypted";
	case Phase::Raw:        return "Raw";
	case Phase::Decrypted:  return "Decrypted";
	case Phase::Dispatched: return "Dispatched";
	case Phase::Dropped:    return "Dropped";
	}
	return "Unknown";
}

const char* PacketTrace::directionToString(Direction dir)
{
	switch (dir) {
	case Direction::Sent:     return "Sent";
	case Direction::Received: return "Received";
	}
	return "Unknown";
}

// ---- Timestamp helper ----

static u64 nowMs()
{
	auto now = std::chrono::system_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch());
	return static_cast<u64>(ms.count());
}

// ---- Record a trace entry ----

void PacketTrace::record(Entry entry)
{
	if (!m_enabled)
		return;

	std::lock_guard<std::mutex> lock(m_mutex);

	// Ring buffer: drop oldest entries when at capacity
	if (m_entries.size() >= MAX_ENTRIES) {
		// Remove the oldest 10% to amortize the cost
		size_t to_remove = MAX_ENTRIES / 10;
		if (to_remove < 1)
			to_remove = 1;
		m_entries.erase(m_entries.begin(),
			m_entries.begin() + static_cast<ptrdiff_t>(to_remove));
	}

	// Fill in timestamp if not provided
	if (entry.timestamp_ms == 0)
		entry.timestamp_ms = nowMs();

	m_entries.push_back(std::move(entry));
}

// ---- Query by peer_id ----

std::vector<PacketTrace::Entry> PacketTrace::queryByPeer(
	session_t peer_id, size_t max_results) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	std::vector<Entry> results;
	results.reserve(std::min(max_results, m_entries.size()));

	// Search from newest to oldest
	for (auto it = m_entries.rbegin(); it != m_entries.rend() && results.size() < max_results; ++it) {
		if (it->peer_id == peer_id)
			results.push_back(*it);
	}

	return results;
}

// ---- Query by phase ----

std::vector<PacketTrace::Entry> PacketTrace::queryByPhase(
	Phase phase, size_t max_results) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	std::vector<Entry> results;
	results.reserve(std::min(max_results, m_entries.size()));

	// Search from newest to oldest
	for (auto it = m_entries.rbegin(); it != m_entries.rend() && results.size() < max_results; ++it) {
		if (it->phase == phase)
			results.push_back(*it);
	}

	return results;
}

// ---- Get all traces ----

std::vector<PacketTrace::Entry> PacketTrace::getAll(size_t max_results) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	size_t count = std::min(max_results, m_entries.size());

	std::vector<Entry> results;
	results.reserve(count);

	// Return newest entries first
	for (auto it = m_entries.rbegin();
	     it != m_entries.rend() && results.size() < count; ++it) {
		results.push_back(*it);
	}

	return results;
}

// ---- Clear all entries ----

void PacketTrace::clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_entries.clear();
}

// ---- Get size ----

size_t PacketTrace::size() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_entries.size();
}

// ---- Generate next trace ID ----

u64 PacketTrace::nextTraceId()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_next_trace_id++;
}
