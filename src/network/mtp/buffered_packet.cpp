// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "network/mtp/internal.h"
#include "network/mtp/impl_constants.h"
#include "util/serialize.h"

namespace con
{

u16 BufferedPacket::getSeqnum() const
{
	if (size() < BASE_HEADER_SIZE + 3)
		return 0; // should never happen

	return readU16(&data[BASE_HEADER_SIZE + 1]);
}

void BufferedPacket::setSenderPeerId(session_t id)
{
	if (size() < BASE_HEADER_SIZE) {
		assert(false); // should never happen
		return;
	}
	writeU16(&data[4], id);
}

BufferedPacketPtr makePacket(const Address &address, const SharedBuffer<u8> &data,
		u32 protocol_id, session_t sender_peer_id, u8 channel)
{
	u32 packet_size = data.getSize() + BASE_HEADER_SIZE;

	auto p = std::make_shared<BufferedPacket>(packet_size);
	p->address = address;

	writeU32(&p->data[0], protocol_id);
	writeU16(&p->data[4], sender_peer_id);
	writeU8(&p->data[6], channel);

	memcpy(&p->data[BASE_HEADER_SIZE], *data, data.getSize());

	return p;
}

SharedBuffer<u8> makeOriginalPacket(const SharedBuffer<u8> &data)
{
	u32 header_size = 1;
	u32 packet_size = data.getSize() + header_size;
	SharedBuffer<u8> b(packet_size);

	writeU8(&(b[0]), PACKET_TYPE_ORIGINAL);
	if (data.getSize() > 0) {
		memcpy(&(b[header_size]), *data, data.getSize());
	}
	return b;
}

// Split data in chunks and add TYPE_SPLIT headers to them
void makeSplitPacket(const SharedBuffer<u8> &data, u32 chunksize_max, u16 seqnum,
		std::list<SharedBuffer<u8>> *chunks)
{
	// Chunk packets, containing the TYPE_SPLIT header
	const u32 chunk_header_size = 7;
	const u32 maximum_data_size = chunksize_max - chunk_header_size;
	u32 start = 0, end = 0;
	u16 chunk_num = 0;
	do {
		end = start + maximum_data_size - 1;
		if (end > data.getSize() - 1)
			end = data.getSize() - 1;

		u32 payload_size = end - start + 1;
		u32 packet_size = chunk_header_size + payload_size;

		SharedBuffer<u8> chunk(packet_size);

		writeU8(&chunk[0], PACKET_TYPE_SPLIT);
		writeU16(&chunk[1], seqnum);
		// [3] u16 chunk_count is written at next stage
		writeU16(&chunk[5], chunk_num);
		memcpy(&chunk[chunk_header_size], &data[start], payload_size);

		chunks->push_back(chunk);

		start = end + 1;
		sanity_check(chunk_num < 0xFFFF); // overflow
		chunk_num++;
	}
	while (end != data.getSize() - 1);

	for (auto &chunk : *chunks) {
		// Write chunk_count
		writeU16(&chunk[3], chunk_num);
	}
}

void makeAutoSplitPacket(const SharedBuffer<u8> &data, u32 chunksize_max,
		u16 &split_seqnum, std::list<SharedBuffer<u8>> *list)
{
	u32 original_header_size = 1;

	if (data.getSize() + original_header_size > chunksize_max) {
		makeSplitPacket(data, chunksize_max, split_seqnum, list);
		split_seqnum++;
		return;
	}

	list->push_back(makeOriginalPacket(data));
}

SharedBuffer<u8> makeReliablePacket(const SharedBuffer<u8> &data, u16 seqnum)
{
	u32 header_size = 3;
	u32 packet_size = data.getSize() + header_size;
	SharedBuffer<u8> b(packet_size);

	writeU8(&b[0], PACKET_TYPE_RELIABLE);
	writeU16(&b[1], seqnum);

	memcpy(&b[header_size], *data, data.getSize());

	return b;
}

} // namespace con
