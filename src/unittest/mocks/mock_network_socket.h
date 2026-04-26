// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors
//
// Mock implementation of INetworkSocket for testing.
//
// Provides a fully in-memory socket that records sent packets and
// allows queuing received packets. This eliminates the need for
// real network I/O in tests.

#pragma once
#include "network/socket/inetwork_socket.h"
#include <vector>
#include <queue>
#include <cstring>

class MockNetworkSocket : public INetworkSocket
{
public:
	struct SentPacket {
		Address destination;
		std::vector<u8> data;
	};

	struct ReceivedPacket {
		Address sender;
		std::vector<u8> data;
	};

	std::vector<SentPacket> sent_packets;
	std::queue<ReceivedPacket> receive_queue;
	unsigned int send_count = 0;
	unsigned int receive_count = 0;
	bool bind_called = false;
	Address bind_address;

	void Bind(Address addr) override {
		bind_called = true;
		bind_address = addr;
	}

	void Send(const Address& addr, const void* data, size_t len) override {
		SentPacket pkt;
		pkt.destination = addr;
		pkt.data.resize(len);
		memcpy(pkt.data.data(), data, len);
		sent_packets.push_back(std::move(pkt));
		send_count += len;
	}

	int Receive(Address& sender, void* data, size_t maxlen) override {
		if (receive_queue.empty()) return -1;
		auto& pkt = receive_queue.front();
		if (pkt.data.size() > maxlen) return -1;
		sender = pkt.sender;
		memcpy(data, pkt.data.data(), pkt.data.size());
		size_t len = pkt.data.size();
		receive_count += len;
		receive_queue.pop();
		return len;
	}

	unsigned int SendCount() override { return send_count; }
	unsigned int ReceiveCount() override { return receive_count; }
	void incrementDropCount() override {}

	void queuePacket(const Address& sender, const void* data, size_t len) {
		ReceivedPacket pkt;
		pkt.sender = sender;
		pkt.data.resize(len);
		memcpy(pkt.data.data(), data, len);
		receive_queue.push(std::move(pkt));
	}

	void reset() {
		sent_packets.clear();
		while (!receive_queue.empty()) receive_queue.pop();
		send_count = 0;
		receive_count = 0;
		bind_called = false;
	}
};
