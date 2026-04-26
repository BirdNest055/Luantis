// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Unit tests for BitmapReplayProtector — the sliding-window replay
// protection used by DirectionalEncryptionState.
//
// These tests prove that:
// - Fresh protector accepts all packets
// - Counter advances correctly
// - Replay within window is detected (exact bitmap tracking)
// - Replay outside window is rejected
// - Bitmap shift works correctly when high-water mark advances
// - Mark received tracks duplicates within the window
// - High-water mark advancement is correct
// - Empty window accepts all
// - Large counter gaps clear the bitmap

#include "test.h"

#include "network/crypto.h"
#include "config.h"

#include <cstring>
#include <vector>
#include <array>
#include <set>

class TestReplayProtection : public TestBase
{
public:
	TestReplayProtection() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestReplayProtection"; }

	void runTests(IGameDef *gamedef);

	// Fresh protector behavior
	void testFreshProtectorAcceptsAll();

	// Counter advancement
	void testCounterAdvancesCorrectly();
	void testCounterDoesNotGoBackwards();

	// Replay detection within window
	void testReplayWithinWindowDetected();
	void testExactDuplicateDetected();

	// Replay outside window
	void testReplayOutsideWindowRejected();

	// Bitmap shift
	void testBitmapShiftOnCounterAdvance();
	void testBitmapShiftPreservesRecentEntries();

	// Mark received tracking
	void testMarkReceivedTracksDuplicates();
	void testMarkReceivedOnlyAffectsWindow();

	// High-water mark
	void testHighWaterMarkAdvancement();
	void testHighWaterMarkWithFuturePackets();

	// Empty window
	void testEmptyWindowAcceptsAll();

	// Large counter gaps
	void testLargeCounterGapClearsBitmap();
};

static TestReplayProtection g_test_instance;

void TestReplayProtection::runTests(IGameDef *gamedef)
{
	TEST(testFreshProtectorAcceptsAll);

	TEST(testCounterAdvancesCorrectly);
	TEST(testCounterDoesNotGoBackwards);

	TEST(testReplayWithinWindowDetected);
	TEST(testExactDuplicateDetected);

	TEST(testReplayOutsideWindowRejected);

	TEST(testBitmapShiftOnCounterAdvance);
	TEST(testBitmapShiftPreservesRecentEntries);

	TEST(testMarkReceivedTracksDuplicates);
	TEST(testMarkReceivedOnlyAffectsWindow);

	TEST(testHighWaterMarkAdvancement);
	TEST(testHighWaterMarkWithFuturePackets);

	TEST(testEmptyWindowAcceptsAll);

	TEST(testLargeCounterGapClearsBitmap);
}

// ============================================================================
// Fresh Protector Tests
// ============================================================================

void TestReplayProtection::testFreshProtectorAcceptsAll()
{
	// A freshly constructed DirectionalEncryptionState has nonce_counter=0
	// and an empty bitmap. All packets should be accepted.
	DirectionalEncryptionState dir;

	// Counter 0 should be accepted
	UASSERT(dir.isNotReplay(0));

	// Counter 1 should be accepted
	UASSERT(dir.isNotReplay(1));

	// Large counter should be accepted
	UASSERT(dir.isNotReplay(10000));
}

// ============================================================================
// Counter Advancement Tests
// ============================================================================

void TestReplayProtection::testCounterAdvancesCorrectly()
{
	DirectionalEncryptionState dir;
	dir.nonce_counter = 10;

	// Receiving counter 20 should advance the high-water mark to 21
	dir.updateCounter(20);
	UASSERTEQ(u64, dir.nonce_counter, 21u);

	// Receiving counter 30 should advance to 31
	dir.updateCounter(30);
	UASSERTEQ(u64, dir.nonce_counter, 31u);

	// Receiving counter 31 should advance to 32
	dir.updateCounter(31);
	UASSERTEQ(u64, dir.nonce_counter, 32u);
}

void TestReplayProtection::testCounterDoesNotGoBackwards()
{
	DirectionalEncryptionState dir;
	dir.nonce_counter = 50;

	// Old packet should not move counter back
	dir.updateCounter(30);
	UASSERTEQ(u64, dir.nonce_counter, 50u);

	// Same as current should not change
	dir.updateCounter(49);
	UASSERTEQ(u64, dir.nonce_counter, 50u);
}

// ============================================================================
// Replay Detection Within Window
// ============================================================================

void TestReplayProtection::testReplayWithinWindowDetected()
{
	// After receiving packet with counter N, a duplicate packet with
	// the same counter should be detected as a replay.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 10;

	// Receive counter 10 — should be accepted
	UASSERT(dir.isNotReplay(10));

	// Mark it as received
	dir.markReceived(10);

	// Receiving counter 10 again — should be detected as replay
	UASSERT(!dir.isNotReplay(10));

	// Counter 9 should be within window and also not yet seen
	UASSERT(dir.isNotReplay(9));

	// Mark 9 as received
	dir.markReceived(9);

	// Counter 9 again should now be replay
	UASSERT(!dir.isNotReplay(9));
}

void TestReplayProtection::testExactDuplicateDetected()
{
	// Test the exact bitmap tracking: after marking a specific counter
	// as received, only that exact counter is detected as replay,
	// while adjacent counters remain valid.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 100;

	// Mark counters 95, 97, 99 as received
	dir.markReceived(95);
	dir.markReceived(97);
	dir.markReceived(99);

	// These should be detected as replay (already seen)
	UASSERT(!dir.isNotReplay(95));
	UASSERT(!dir.isNotReplay(97));
	UASSERT(!dir.isNotReplay(99));

	// Adjacent counters should still be accepted
	UASSERT(dir.isNotReplay(96));
	UASSERT(dir.isNotReplay(98));
	UASSERT(dir.isNotReplay(100));
}

// ============================================================================
// Replay Outside Window
// ============================================================================

void TestReplayProtection::testReplayOutsideWindowRejected()
{
	// Packets more than REPLAY_WINDOW_SIZE (64) positions behind
	// the current high-water mark should be rejected outright.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 200;

	// 100 positions behind — rejected
	UASSERT(!dir.isNotReplay(100));

	// Way behind — rejected
	UASSERT(!dir.isNotReplay(0));

	// 65 positions behind — just outside window
	UASSERT(!dir.isNotReplay(135));

	// 64 positions behind — at boundary, accepted
	UASSERT(dir.isNotReplay(136));

	// 63 positions behind — inside window, accepted
	UASSERT(dir.isNotReplay(137));
}

// ============================================================================
// Bitmap Shift Tests
// ============================================================================

void TestReplayProtection::testBitmapShiftOnCounterAdvance()
{
	// When the high-water mark advances, the bitmap should shift
	// so that older entries move further from the current position.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 10;

	// Mark counter 8 as received
	dir.markReceived(8);

	// Counter 8 should be detected as replay
	UASSERT(!dir.isNotReplay(8));

	// Advance the high-water mark significantly
	u64 old_counter = dir.nonce_counter;
	dir.nonce_counter = 50;
	dir.shiftBitmap(old_counter, dir.nonce_counter);

	// Counter 8 is now 42 positions behind (8 is at offset 42 from 50).
	// REPLAY_WINDOW_SIZE is 64, so it's within the window.
	// But the bitmap was shifted, so let's check if it's still tracked.
	// With shift of 40, the bit for counter 8 (which was at offset 1
	// from old_counter=10) moves to offset 41 from new_counter=50.
	// That's still within the 64-bit window (single word).
	// The bit should still be set if the shift was correct.
	UASSERT(dir.isAlreadySeen(8));
}

void TestReplayProtection::testBitmapShiftPreservesRecentEntries()
{
	// After a small counter advancement, recently seen entries
	// should still be tracked in the bitmap.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 10;

	// Mark counters 9 and 8 as received
	dir.markReceived(9);
	dir.markReceived(8);

	// Advance counter by 2
	u64 old_counter = dir.nonce_counter;
	dir.nonce_counter = 12;
	dir.shiftBitmap(old_counter, dir.nonce_counter);

	// Counters 9 and 8 should still be tracked
	UASSERT(dir.isAlreadySeen(9));
	UASSERT(dir.isAlreadySeen(8));

	// But they should be detected as replay
	UASSERT(!dir.isNotReplay(9));
	UASSERT(!dir.isNotReplay(8));
}

// ============================================================================
// Mark Received Tracking
// ============================================================================

void TestReplayProtection::testMarkReceivedTracksDuplicates()
{
	// markReceived should record which counters have been seen,
	// preventing replay of those exact counters.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 50;

	// Before marking, counter 49 is not yet seen
	UASSERT(!dir.isAlreadySeen(49));
	UASSERT(dir.isNotReplay(49));

	// Mark it
	dir.markReceived(49);

	// Now it should be seen
	UASSERT(dir.isAlreadySeen(49));
	UASSERT(!dir.isNotReplay(49));
}

void TestReplayProtection::testMarkReceivedOnlyAffectsWindow()
{
	// markReceived should only track counters within the current
	// window. Counters too far behind should not be tracked.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 100;

	// Counter 10 is 90 positions behind — outside the window
	dir.markReceived(10);
	// It should NOT be tracked (too far behind)
	UASSERT(!dir.isAlreadySeen(10));
}

// ============================================================================
// High-Water Mark Tests
// ============================================================================

void TestReplayProtection::testHighWaterMarkAdvancement()
{
	// The high-water mark (nonce_counter) should advance when
	// a packet with a counter >= the current mark is processed.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 0;

	// Receive packet 0 — should advance to 1
	dir.updateCounter(0);
	UASSERTEQ(u64, dir.nonce_counter, 1u);

	// Receive packet 5 — should advance to 6
	dir.updateCounter(5);
	UASSERTEQ(u64, dir.nonce_counter, 6u);

	// Receive packet 3 — should NOT advance (3 < 6)
	dir.updateCounter(3);
	UASSERTEQ(u64, dir.nonce_counter, 6u);
}

void TestReplayProtection::testHighWaterMarkWithFuturePackets()
{
	// Receiving a future packet (counter > current high-water mark)
	// should advance the mark and allow the packet through.
	DirectionalEncryptionState dir;
	dir.nonce_counter = 10;

	// Counter 100 is far in the future — accepted
	UASSERT(dir.isNotReplay(100));

	// After updateCounter, high-water mark should advance
	dir.updateCounter(100);
	UASSERTEQ(u64, dir.nonce_counter, 101u);

	// Old packets within the new window should still be checked
	// Counter 99 should be accepted (not yet seen, within window)
	UASSERT(dir.isNotReplay(99));

	// Mark it and verify replay detection
	dir.markReceived(99);
	UASSERT(!dir.isNotReplay(99));
}

// ============================================================================
// Empty Window Tests
// ============================================================================

void TestReplayProtection::testEmptyWindowAcceptsAll()
{
	// With nonce_counter=0, the bitmap is empty and all packets
	// should be accepted (no replays possible).
	DirectionalEncryptionState dir;

	// isAlreadySeen should return false for everything
	UASSERT(!dir.isAlreadySeen(0));
	UASSERT(!dir.isAlreadySeen(1));
	UASSERT(!dir.isAlreadySeen(100));

	// isNotReplay should accept everything
	UASSERT(dir.isNotReplay(0));
	UASSERT(dir.isNotReplay(1));
	UASSERT(dir.isNotReplay(1000));
}

// ============================================================================
// Large Counter Gap Tests
// ============================================================================

void TestReplayProtection::testLargeCounterGapClearsBitmap()
{
	// When the high-water mark advances by more than REPLAY_WINDOW_SIZE,
	// the bitmap should be completely cleared (all old entries are
	// now outside the window).
	DirectionalEncryptionState dir;
	dir.nonce_counter = 10;

	// Mark several counters as received
	dir.markReceived(9);
	dir.markReceived(8);
	dir.markReceived(7);
	dir.markReceived(6);

	// Verify they're tracked
	UASSERT(dir.isAlreadySeen(9));
	UASSERT(dir.isAlreadySeen(8));
	UASSERT(dir.isAlreadySeen(7));
	UASSERT(dir.isAlreadySeen(6));

	// Advance counter by more than the window size (64)
	u64 old_counter = dir.nonce_counter;
	dir.nonce_counter = 200;  // Gap of 190 > 64
	dir.shiftBitmap(old_counter, dir.nonce_counter);

	// All old entries should be cleared
	UASSERT(!dir.isAlreadySeen(9));
	UASSERT(!dir.isAlreadySeen(8));
	UASSERT(!dir.isAlreadySeen(7));
	UASSERT(!dir.isAlreadySeen(6));

	// New window should accept new packets
	UASSERT(dir.isNotReplay(199));
	UASSERT(dir.isNotReplay(200));
}
