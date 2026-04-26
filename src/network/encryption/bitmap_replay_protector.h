// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "network/encryption/ireplay_protector.h"
#include "irrlichttypes.h"
#include <array>
#include <cstdint>

/// Bitmap-based replay protection using a sliding window.
///
/// Tracks which counters within the sliding window have been seen
/// using a bitmap. This prevents replay attacks WITHIN the window,
/// not just outside it. Extracted from DirectionalEncryptionState
/// to decouple replay protection from key/nonce management.
class BitmapReplayProtector : public IReplayProtector
{
public:
    static constexpr size_t REPLAY_WINDOW_SIZE = 64;
    static constexpr size_t REPLAY_BITMAP_WORDS = REPLAY_WINDOW_SIZE / (sizeof(u64) * 8);

    BitmapReplayProtector() { replay_bitmap.fill(0); }

    bool isNotReplay(u64 received_counter) override;
    void markReceived(u64 received_counter) override;
    void updateCounter(u64 received_counter) override;
    u64 getCurrentCounter() const override { return nonce_counter; }
    u64 getReplayAttempts() const override { return replay_attempts; }

private:
    /// Check if a specific counter has already been seen (within the window).
    bool isAlreadySeen(u64 received_counter) const;

    /// Shift the replay bitmap when the high-water mark advances.
    void shiftBitmap(u64 old_counter, u64 new_counter);

    u64 nonce_counter = 0;
    u64 replay_attempts = 0;
    std::array<u64, REPLAY_BITMAP_WORDS> replay_bitmap{};
};
