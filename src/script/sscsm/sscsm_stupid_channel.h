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

        /// Drain the channel, discarding any pending request or answer.
        /// Useful during shutdown to ensure waiting threads don't block indefinitely.
        void flush()
        {
                auto lock = std::lock_guard(m_mutex);
                m_request = nullptr;
                m_answer = nullptr;
        }
};

// --- IPC Channel Replacement API ---
// The following documents the proposed replacement for StupidChannel.
// This API should be implemented in a new file (e.g., sscsm_ipc_channel.h)
// and will provide:
//
// class SSCSMIPCChannel {
// public:
//   // Enqueue a serialized request (supports multiple pending requests)
//   void sendRequest(std::vector<u8> data);
//
//   // Block until a request is available, then dequeue and return it
//   std::vector<u8> recvRequest();
//
//   // Send a serialized answer back
//   void sendAnswer(std::vector<u8> data);
//
//   // Block until an answer is available
//   std::vector<u8> recvAnswer();
//
//   // Bidirectional async: check if request/answer available without blocking
//   bool hasRequest() const;
//   bool hasAnswer() const;
//
//   // Shutdown: unblock any waiting recv calls with a "poison pill"
//   void shutdown();
//
//   // Flush all pending data
//   void flush();
// };
//
// Key improvements over StupidChannel:
//   (1) Thread-safe queue (std::queue protected by mutex) for multiple concurrent requests.
//   (2) Bidirectional async messaging.
//   (3) Proper shutdown signaling so recv calls exit cleanly.
//   (4) Optional OS-level IPC (pipe, socket) for true process isolation.
