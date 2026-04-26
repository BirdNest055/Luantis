// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"
#include <cstdint>

/// Abstract interface for replay attack protection.
///
/// Decouples replay protection logic from the encryption state,
/// making it independently testable and swappable.
class IReplayProtector
{
public:
    virtual ~IReplayProtector() = default;

    /// Check if a received counter value is not a replay.
    ///
    /// @param received_counter  The counter from the received packet
    /// @return true if the packet is not a replay (safe to process)
    virtual bool isNotReplay(u64 received_counter) = 0;

    /// Mark a counter as received after successful processing.
    ///
    /// @param received_counter  The counter to mark as seen
    virtual void markReceived(u64 received_counter) = 0;

    /// Update the high-water mark counter after accepting a packet.
    ///
    /// @param received_counter  The counter from the received packet
    virtual void updateCounter(u64 received_counter) = 0;

    /// Get the current counter value (for statistics).
    virtual u64 getCurrentCounter() const = 0;

    /// Get the number of replay attempts detected.
    virtual u64 getReplayAttempts() const = 0;
};
