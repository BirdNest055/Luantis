// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#include "network/encryption/bitmap_replay_protector.h"

bool BitmapReplayProtector::isNotReplay(u64 received_counter)
{
    // Allow packets up to REPLAY_WINDOW_SIZE positions behind the latest seen counter
    if (received_counter > nonce_counter) {
        // Future packet — always accept (counter will be updated by caller)
        return true;
    }

    if (nonce_counter - received_counter > REPLAY_WINDOW_SIZE) {
        // Too far behind — likely a replay attack
        replay_attempts++;
        return false;
    }

    // Within the acceptable window — check if already seen
    if (isAlreadySeen(received_counter)) {
        // This counter was already received — replay!
        replay_attempts++;
        return false;
    }

    return true;
}

void BitmapReplayProtector::markReceived(u64 received_counter)
{
    // Calculate the bit position within the window
    if (nonce_counter == 0) return;
    s64 offset = static_cast<s64>(nonce_counter) - static_cast<s64>(received_counter) - 1;
    if (offset >= 0 && offset < static_cast<s64>(REPLAY_WINDOW_SIZE)) {
        size_t word = static_cast<size_t>(offset) / 64;
        size_t bit = static_cast<size_t>(offset) % 64;
        if (word < REPLAY_BITMAP_WORDS) {
            replay_bitmap[word] |= (1ULL << bit);
        }
    }
}

bool BitmapReplayProtector::isAlreadySeen(u64 received_counter) const
{
    if (nonce_counter == 0) return false;
    s64 offset = static_cast<s64>(nonce_counter) - static_cast<s64>(received_counter) - 1;
    if (offset >= 0 && offset < static_cast<s64>(REPLAY_WINDOW_SIZE)) {
        size_t word = static_cast<size_t>(offset) / 64;
        size_t bit = static_cast<size_t>(offset) % 64;
        if (word < REPLAY_BITMAP_WORDS) {
            return (replay_bitmap[word] & (1ULL << bit)) != 0;
        }
    }
    return false;
}

void BitmapReplayProtector::shiftBitmap(u64 old_counter, u64 new_counter)
{
    u64 shift = new_counter - old_counter;
    if (shift == 0) return;

    // If shift is larger than the window, clear the bitmap
    if (shift >= REPLAY_WINDOW_SIZE) {
        replay_bitmap.fill(0);
        return;
    }

    // Shift the bitmap by 'shift' positions to the right
    // (older entries move further from the high-water mark)
    for (size_t i = REPLAY_BITMAP_WORDS; i > 0; i--) {
        size_t idx = i - 1;
        size_t word_shift = static_cast<size_t>(shift);
        size_t big_shift = word_shift / 64;
        size_t small_shift = word_shift % 64;

        u64 val = 0;
        if (idx >= big_shift) {
            val = replay_bitmap[idx - big_shift] >> small_shift;
            if (small_shift > 0 && idx > big_shift) {
                val |= replay_bitmap[idx - big_shift - 1] << (64 - small_shift);
            }
        }
        replay_bitmap[idx] = val;
    }
}

void BitmapReplayProtector::updateCounter(u64 received_counter)
{
    if (received_counter >= nonce_counter) {
        u64 old_counter = nonce_counter;
        nonce_counter = received_counter + 1;
        shiftBitmap(old_counter, nonce_counter);
    }
}
