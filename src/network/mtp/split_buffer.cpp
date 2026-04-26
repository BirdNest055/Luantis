// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "network/mtp/internal.h"
#include "network/mtp/impl_constants.h"
#include "util/serialize.h"

namespace con
{

/*
	IncomingSplitPacket
*/

bool IncomingSplitPacket::insert(u32 chunk_num, SharedBuffer<u8> &chunkdata)
{
	sanity_check(chunk_num < chunk_count);

	// If chunk already exists, ignore it.
	// Sometimes two identical packets may arrive when there is network
	// lag and the server re-sends stuff.
	if (chunks.find(chunk_num) != chunks.end())
		return false;

	// Set chunk data in buffer
	chunks[chunk_num] = chunkdata;

	return true;
}

SharedBuffer<u8> IncomingSplitPacket::reassemble()
{
	sanity_check(allReceived());

	// Calculate total size
	u32 totalsize = 0;
	for (const auto &chunk : chunks)
		totalsize += chunk.second.getSize();

	SharedBuffer<u8> fulldata(totalsize);

	// Copy chunks to data buffer
	u32 start = 0;
	for (u32 chunk_i = 0; chunk_i < chunk_count; chunk_i++) {
		const SharedBuffer<u8> &buf = chunks[chunk_i];
		memcpy(&fulldata[start], *buf, buf.getSize());
		start += buf.getSize();
	}

	return fulldata;
}

/*
	IncomingSplitBuffer
*/

IncomingSplitBuffer::~IncomingSplitBuffer()
{
	MutexAutoLock listlock(m_map_mutex);
	for (auto &i : m_buf) {
		delete i.second;
	}
}

SharedBuffer<u8> IncomingSplitBuffer::insert(BufferedPacketPtr &p_ptr, bool reliable)
{
	MutexAutoLock listlock(m_map_mutex);
	const BufferedPacket &p = *p_ptr;

	u32 headersize = BASE_HEADER_SIZE + 7;
	if (p.size() < headersize) {
		errorstream << "Invalid data size for split packet" << std::endl;
		return SharedBuffer<u8>();
	}
	u8 type = readU8(&p.data[BASE_HEADER_SIZE+0]);
	u16 seqnum = readU16(&p.data[BASE_HEADER_SIZE+1]);
	u16 chunk_count = readU16(&p.data[BASE_HEADER_SIZE+3]);
	u16 chunk_num = readU16(&p.data[BASE_HEADER_SIZE+5]);

	if (type != PACKET_TYPE_SPLIT) {
		errorstream << "IncomingSplitBuffer::insert(): type is not split"
			<< std::endl;
		return SharedBuffer<u8>();
	}
	if (chunk_num >= chunk_count) {
		errorstream << "IncomingSplitBuffer::insert(): chunk_num=" << chunk_num
				<< " >= chunk_count=" << chunk_count << std::endl;
		return SharedBuffer<u8>();
	}

	// Add if doesn't exist
	IncomingSplitPacket *sp;
	if (m_buf.find(seqnum) == m_buf.end()) {
		sp = new IncomingSplitPacket(chunk_count, reliable);
		m_buf[seqnum] = sp;
	} else {
		sp = m_buf[seqnum];
	}

	if (chunk_count != sp->chunk_count) {
		errorstream << "IncomingSplitBuffer::insert(): chunk_count="
				<< chunk_count << " != sp->chunk_count=" << sp->chunk_count
				<< std::endl;
		return SharedBuffer<u8>();
	}
	if (reliable != sp->reliable)
		LOG(derr_con<<"Connection: WARNING: reliable="<<reliable
				<<" != sp->reliable="<<sp->reliable
				<<std::endl);

	// Cut chunk data out of packet
	u32 chunkdatasize = p.size() - headersize;
	SharedBuffer<u8> chunkdata(chunkdatasize);
	memcpy(*chunkdata, &(p.data[headersize]), chunkdatasize);

	if (!sp->insert(chunk_num, chunkdata))
		return SharedBuffer<u8>();

	// If not all chunks are received, return empty buffer
	if (!sp->allReceived())
		return SharedBuffer<u8>();

	SharedBuffer<u8> fulldata = sp->reassemble();

	// Remove sp from buffer
	m_buf.erase(seqnum);
	delete sp;

	return fulldata;
}

void IncomingSplitBuffer::removeUnreliableTimedOuts(float dtime, float timeout)
{
	MutexAutoLock listlock(m_map_mutex);
	std::vector<u16> remove_queue;
	{
		for (const auto &i : m_buf) {
			IncomingSplitPacket *p = i.second;
			// Reliable ones are not removed by timeout
			if (p->reliable)
				continue;
			p->time += dtime;
			if (p->time >= timeout)
				remove_queue.push_back(i.first);
		}
	}
	for (u16 j : remove_queue) {
		LOG(dout_con<<"NOTE: Removing timed out unreliable split packet"<<std::endl);
		auto it = m_buf.find(j);
		delete it->second;
		m_buf.erase(it);
	}
}

} // namespace con
