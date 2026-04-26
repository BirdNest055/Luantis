// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Mock implementation of IReplayProtector for testing.
//
// Records all method calls for verification and allows configurable
// behavior (always valid, specific counter tracking, etc.).

#pragma once
#include "network/encryption/ireplay_protector.h"
#include <vector>

class MockReplayProtector : public IReplayProtector
{
public:
	struct Call {
		std::string method;
		u64 counter;
	};
	std::vector<Call> calls;

	bool always_valid = true;
	u64 current_counter = 0;
	u64 replay_attempts = 0;

	bool isNotReplay(u64 counter) override {
		calls.push_back({"isNotReplay", counter});
		return always_valid;
	}
	void markReceived(u64 counter) override {
		calls.push_back({"markReceived", counter});
	}
	void updateCounter(u64 counter) override {
		calls.push_back({"updateCounter", counter});
		if (counter >= current_counter)
			current_counter = counter + 1;
	}
	u64 getCurrentCounter() const override { return current_counter; }
	u64 getReplayAttempts() const override { return replay_attempts; }
	void reset() { calls.clear(); }
};
