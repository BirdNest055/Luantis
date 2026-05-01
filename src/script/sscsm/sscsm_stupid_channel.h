// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>
#include "sscsm_irequest.h"

// NOTE: StupidChannel is a minimal mutex+condvar IPC channel for SSCSM
// (Server-Side Client-Side Modding) communication. It should be replaced
// with a proper IPC channel implementation.
// Root cause: StupidChannel uses a shared mutex and condition variable
// between the server and SSCSM threads. It only supports one pending
// request at a time (no queue). If sendA() is called while a previous
// request is still pending, the old request is silently overwritten.
// Proposed replacement: A proper IPC channel with:
//   (1) A thread-safe request queue (e.g., std::queue protected by mutex)
//       to support multiple concurrent requests.
//   (2) Bidirectional async messaging (not just synchronous exchange).
//   (3) Proper shutdown signaling (a "poison pill" message type) so that
//       recvA()/recvB() can exit cleanly during shutdown instead of
//       blocking indefinitely on m_condvar.wait().
//   (4) Optional: Use OS-level IPC (pipe, socket) instead of shared memory
//       for true process isolation if SSCSM is ever sandboxed in a
//       separate process.
class StupidChannel
{
	std::mutex m_mutex;
	std::condition_variable m_condvar;
	SerializedSSCSMRequest m_request;
	SerializedSSCSMAnswer m_answer;

public:
	void sendA(SerializedSSCSMRequest request)
	{
		{
			auto lock = std::lock_guard(m_mutex);

			m_request = std::move(request);
		}

		m_condvar.notify_one();
	}

	SerializedSSCSMAnswer recvA()
	{
		auto lock = std::unique_lock(m_mutex);

		while (!m_answer) {
			m_condvar.wait(lock);
		}

		auto answer = std::move(m_answer);
		m_answer = nullptr;

		return answer;
	}

	SerializedSSCSMAnswer exchangeA(SerializedSSCSMRequest request)
	{
		sendA(std::move(request));

		return recvA();
	}

	void sendB(SerializedSSCSMAnswer answer)
	{
		{
			auto lock = std::lock_guard(m_mutex);

			m_answer = std::move(answer);
		}

		m_condvar.notify_one();
	}

	SerializedSSCSMRequest recvB()
	{
		auto lock = std::unique_lock(m_mutex);

		while (!m_request) {
			m_condvar.wait(lock);
		}

		auto request = std::move(m_request);
		m_request = nullptr;

		return request;
	}

	SerializedSSCSMRequest exchangeB(SerializedSSCSMAnswer answer)
	{
		sendB(std::move(answer));

		return recvB();
	}
};
