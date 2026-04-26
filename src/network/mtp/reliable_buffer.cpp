// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <cmath>
#include "network/mtp/internal.h"
#include "network/mtp/impl_constants.h"
#include "network/networkexceptions.h"
#include "util/serialize.h"

namespace con
{

/*
	ReliablePacketBuffer
*/

void ReliablePacketBuffer::print()
{
	MutexAutoLock listlock(m_list_mutex);
	LOG(dout_con<<"Dump of ReliablePacketBuffer:" << std::endl);
	unsigned int index = 0;
	for (BufferedPacketPtr &packet : m_list) {
		LOG(dout_con<<index<< ":" << packet->getSeqnum() << std::endl);
		index++;
	}
}

bool ReliablePacketBuffer::empty()
{
	MutexAutoLock listlock(m_list_mutex);
	return m_list.empty();
}

u32 ReliablePacketBuffer::size()
{
	MutexAutoLock listlock(m_list_mutex);
	return m_list.size();
}

ReliablePacketBuffer::FindResult ReliablePacketBuffer::findPacketNoLock(u16 seqnum)
{
	for (auto it = m_list.begin(); it != m_list.end(); ++it) {
		if ((*it)->getSeqnum() == seqnum)
			return it;
	}
	return m_list.end();
}

bool ReliablePacketBuffer::getFirstSeqnum(u16& result)
{
	MutexAutoLock listlock(m_list_mutex);
	if (m_list.empty())
		return false;
	result = m_list.front()->getSeqnum();
	return true;
}

BufferedPacketPtr ReliablePacketBuffer::popFirst()
{
	MutexAutoLock listlock(m_list_mutex);
	if (m_list.empty())
		throw NotFoundException("Buffer is empty");

	BufferedPacketPtr p(m_list.front());
	m_list.pop_front();

	if (m_list.empty()) {
		m_oldest_non_answered_ack = 0;
	} else {
		m_oldest_non_answered_ack = m_list.front()->getSeqnum();
	}
	return p;
}

BufferedPacketPtr ReliablePacketBuffer::popSeqnum(u16 seqnum)
{
	MutexAutoLock listlock(m_list_mutex);
	auto r = findPacketNoLock(seqnum);
	if (r == m_list.end()) {
		LOG(dout_con<<"Sequence number: " << seqnum
				<< " not found in reliable buffer"<<std::endl);
		throw NotFoundException("seqnum not found in buffer");
	}

	BufferedPacketPtr p(*r);
	m_list.erase(r);

	if (m_list.empty()) {
		m_oldest_non_answered_ack = 0;
	} else {
		m_oldest_non_answered_ack = m_list.front()->getSeqnum();
	}
	return p;
}

void ReliablePacketBuffer::insert(BufferedPacketPtr &p_ptr, u16 next_expected)
{
	MutexAutoLock listlock(m_list_mutex);
	const BufferedPacket &p = *p_ptr;

	if (p.size() < BASE_HEADER_SIZE + 3) {
		errorstream << "ReliablePacketBuffer::insert(): Invalid data size for "
			"reliable packet" << std::endl;
		return;
	}
	u8 type = readU8(&p.data[BASE_HEADER_SIZE + 0]);
	if (type != PACKET_TYPE_RELIABLE) {
		errorstream << "ReliablePacketBuffer::insert(): type is not reliable"
			<< std::endl;
		return;
	}
	const u16 seqnum = p.getSeqnum();

	if (!seqnum_in_window(seqnum, next_expected, MAX_RELIABLE_WINDOW_SIZE)) {
		errorstream << "ReliablePacketBuffer::insert(): seqnum is outside of "
			"expected window " << std::endl;
		return;
	}
	if (seqnum == next_expected) {
		errorstream << "ReliablePacketBuffer::insert(): seqnum is next expected"
			<< std::endl;
		return;
	}

	if (m_list.size() >= SEQNUM_MAX) {
		errorstream << "ReliablePacketBuffer::insert(): buffer full ("
				<< m_list.size() << " packets), dropping oldest" << std::endl;
		// shared_ptr will automatically delete when popped
		m_list.pop_front();
	}
	
	// Find the right place for the packet and insert it there
	// If list is empty, just add it
	if (m_list.empty()) {
		m_list.push_back(p_ptr);
		m_oldest_non_answered_ack = seqnum;
		// Done.
		return;
	}

	// Otherwise find the right place
	auto it = m_list.begin();
	// Find the first packet in the list which has a higher seqnum
	u16 s = (*it)->getSeqnum();

	/* case seqnum is smaller then next_expected seqnum */
	/* this is true e.g. on wrap around */
	if (seqnum < next_expected) {
		while(((s < seqnum) || (s >= next_expected)) && (it != m_list.end())) {
			++it;
			if (it != m_list.end())
				s = (*it)->getSeqnum();
		}
	}
	/* non wrap around case (at least for incoming and next_expected */
	else
	{
		while(((s < seqnum) && (s >= next_expected)) && (it != m_list.end())) {
			++it;
			if (it != m_list.end())
				s = (*it)->getSeqnum();
		}
	}

	if (s == seqnum) {
		/* nothing to do this seems to be a resent packet */
		/* for paranoia reason data should be compared */
		auto &i = *it;
		if (
			(i->getSeqnum() != seqnum) ||
			(i->size() != p.size()) ||
			(i->address != p.address)
			)
		{
			/* if this happens your maximum transfer window may be to big */
			char buf[200];
			snprintf(buf, sizeof(buf),
					"Duplicated seqnum %d non matching packet detected:\n",
					seqnum);
			warningstream << buf;
			snprintf(buf, sizeof(buf),
					"Old: seqnum: %05d size: %04zu, address: %s\n",
					i->getSeqnum(), i->size(),
					i->address.serializeString().c_str());
			warningstream << buf;
			snprintf(buf, sizeof(buf),
					"New: seqnum: %05d size: %04zu, address: %s\n",
					p.getSeqnum(), p.size(),
					p.address.serializeString().c_str());
			warningstream << buf << std::flush;
			throw IncomingDataCorruption("duplicated packet isn't same as original one");
		}
	}
	/* insert or push back */
	else if (it != m_list.end()) {
		m_list.insert(it, p_ptr);
	} else {
		m_list.push_back(p_ptr);
	}

	/* update last packet number */
	m_oldest_non_answered_ack = m_list.front()->getSeqnum();
}

void ReliablePacketBuffer::fixPeerId(session_t new_id)
{
	MutexAutoLock listlock(m_list_mutex);
	for (auto &packet : m_list)
		packet->setSenderPeerId(new_id);
}

void ReliablePacketBuffer::incrementTimeouts(float dtime)
{
	MutexAutoLock listlock(m_list_mutex);
	for (auto &packet : m_list) {
		packet->time += dtime;
		packet->totaltime += dtime;
	}
}

u32 ReliablePacketBuffer::getTimedOuts(float timeout)
{
	MutexAutoLock listlock(m_list_mutex);
	u32 count = 0;
	for (auto &packet : m_list) {
		if (packet->totaltime >= timeout)
			count++;
	}
	return count;
}

std::vector<ConstSharedPtr<BufferedPacket>>
	ReliablePacketBuffer::getResend(float timeout, u32 max_packets)
{
	MutexAutoLock listlock(m_list_mutex);
	std::vector<ConstSharedPtr<BufferedPacket>> timed_outs;
	for (auto &packet : m_list) {
		// resend time scales exponentially with each cycle
		const float pkt_timeout = timeout * powf(RESEND_SCALE_BASE, packet->resend_count);

		if (packet->time < pkt_timeout)
			continue;

		// caller will resend packet so reset time and increase counter
		packet->time = 0.0f;
		packet->resend_count++;

		timed_outs.emplace_back(packet);

		if (timed_outs.size() >= max_packets)
			break;
	}
	return timed_outs;
}

} // namespace con
