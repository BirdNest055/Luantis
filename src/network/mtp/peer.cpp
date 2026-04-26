// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <cmath>
#include "network/mtp/internal.h"
#include "network/mtp/impl_constants.h"
#include "network/mtp/threads.h"
#include "log.h"
#include "porting.h"
#include "util/serialize.h"
#include "util/string.h"
#include "profiler.h"

namespace con
{

/*
	Peer
*/

PeerHelper::~PeerHelper()
{
	if (m_peer)
		m_peer->DecUseCount();

	m_peer = nullptr;
}

PeerHelper& PeerHelper::operator=(Peer* peer)
{
	if (m_peer)
		m_peer->DecUseCount();
	m_peer = peer;
	if (peer && !peer->IncUseCount())
		m_peer = nullptr;
	return *this;
}

bool Peer::IncUseCount()
{
	MutexAutoLock lock(m_exclusive_access_mutex);

	if (!m_pending_deletion) {
		this->m_usage++;
		return true;
	}

	return false;
}

void Peer::DecUseCount()
{
	{
		MutexAutoLock lock(m_exclusive_access_mutex);
		sanity_check(m_usage > 0);
		m_usage--;

		if (!((m_pending_deletion) && (m_usage == 0)))
			return;
	}
	delete this;
}

void Peer::RTTStatistics(float rtt, const std::string &profiler_id,
		unsigned int num_samples) {

	if (m_last_rtt > 0) {
		/* set min max values */
		if (rtt < m_rtt.min_rtt)
			m_rtt.min_rtt = rtt;
		if (rtt >= m_rtt.max_rtt)
			m_rtt.max_rtt = rtt;

		/* do average calculation */
		if (m_rtt.avg_rtt < 0)
			m_rtt.avg_rtt  = rtt;
		else
			m_rtt.avg_rtt  = m_rtt.avg_rtt * (num_samples/(num_samples-1)) +
							rtt * (1/num_samples);

		/* do jitter calculation */

		//just use some neutral value at beginning
		float jitter = m_rtt.jitter_min;

		if (rtt > m_last_rtt)
			jitter = rtt-m_last_rtt;

		if (rtt <= m_last_rtt)
			jitter = m_last_rtt - rtt;

		if (jitter < m_rtt.jitter_min)
			m_rtt.jitter_min = jitter;
		if (jitter >= m_rtt.jitter_max)
			m_rtt.jitter_max = jitter;

		if (m_rtt.jitter_avg < 0)
			m_rtt.jitter_avg  = jitter;
		else
			m_rtt.jitter_avg  = m_rtt.jitter_avg * (num_samples/(num_samples-1)) +
							jitter * (1/num_samples);

		if (!profiler_id.empty()) {
			g_profiler->graphAdd(profiler_id + " RTT [ms]", rtt * 1000.f);
			g_profiler->graphAdd(profiler_id + " jitter [ms]", jitter * 1000.f);
		}
	}
	/* save values required for next loop */
	m_last_rtt = rtt;
}

bool Peer::isTimedOut(float timeout, std::string &reason)
{
	MutexAutoLock lock(m_exclusive_access_mutex);

	{
		u64 current_time = porting::getTimeMs();
		float dtime = CALC_DTIME(m_last_timeout_check, current_time);
		m_last_timeout_check = current_time;
		m_timeout_counter += dtime;
	}
	if (m_timeout_counter > timeout) {
		reason = "timeout counter";
		return true;
	}

	return false;
}

void Peer::Drop()
{
	{
		MutexAutoLock usage_lock(m_exclusive_access_mutex);
		m_pending_deletion = true;
		if (m_usage != 0)
			return;
	}

	PROFILE(std::stringstream peerIdentifier1);
	PROFILE(peerIdentifier1 << "runTimeouts[" << m_connection->getDesc()
			<< ";" << id << ";RELIABLE]");
	PROFILE(g_profiler->remove(peerIdentifier1.str()));
	PROFILE(std::stringstream peerIdentifier2);
	PROFILE(peerIdentifier2 << "sendPackets[" << m_connection->getDesc()
			<< ";" << id << ";RELIABLE]");
	PROFILE(ScopeProfiler peerprofiler(g_profiler, peerIdentifier2.str(), SPT_AVG));

	delete this;
}

UDPPeer::UDPPeer(session_t id, const Address &address, Connection *connection) :
	Peer(id, address, connection)
{
	for (Channel &channel : channels)
		channel.setWindowSize(START_RELIABLE_WINDOW_SIZE);
}

bool UDPPeer::isTimedOut(float timeout, std::string &reason)
{
	if (Peer::isTimedOut(timeout, reason))
		return true;

	MutexAutoLock lock(m_exclusive_access_mutex);

	for (int i = 0; i < CHANNEL_COUNT; i++) {
		Channel &channel = channels[i];
		if (channel.outgoing_reliables_sent.getTimedOuts(timeout) > 0) {
			reason = "outgoing reliables channel=" + itos(i);
			return true;
		}
	}

	return false;
}

void UDPPeer::reportRTT(float rtt)
{
	if (rtt < 0)
		return;
	RTTStatistics(rtt, "network", MAX_RELIABLE_WINDOW_SIZE*10);

	// use this value to decide the resend timeout
	const float rtt_stat = getStat(AVG_RTT);
	if (rtt_stat < 0)
		return;
	float timeout = rtt_stat * RESEND_TIMEOUT_FACTOR;
	if (timeout < RESEND_TIMEOUT_MIN)
		timeout = RESEND_TIMEOUT_MIN;
	if (timeout > RESEND_TIMEOUT_MAX)
		timeout = RESEND_TIMEOUT_MAX;

	float timeout_old = getResendTimeout();
	setResendTimeout(timeout);

	if (std::abs(timeout - timeout_old) >= 0.001f) {
		dout_con << m_connection->getDesc() << " set resend timeout " << timeout
			<< " (rtt=" << rtt_stat << ") for peer id: " << id << std::endl;
	}
}

bool UDPPeer::Ping(float dtime,SharedBuffer<u8>& data)
{
	m_ping_timer += dtime;
	if (!isHalfOpen() && m_ping_timer >= PING_INTERVAL)
	{
		// Create and send PING packet
		writeU8(&data[0], PACKET_TYPE_CONTROL);
		writeU8(&data[1], CONTROLTYPE_PING);
		m_ping_timer = 0.0f;
		return true;
	}
	return false;
}

void UDPPeer::PutReliableSendCommand(ConnectionCommandPtr &c,
		unsigned int max_packet_size)
{
	if (m_pending_disconnect)
		return;

	Channel &chan = channels[c->channelnum];

	if (chan.queued_commands.empty() &&
			/* don't queue more packets then window size */
			(chan.queued_reliables.size() + 1 < chan.getWindowSize() / 2)) {
		LOG(dout_con<<m_connection->getDesc()
				<<" processing reliable command for peer id: " << c->peer_id
				<<" data size: " << c->data.getSize() << std::endl);
		if (processReliableSendCommand(c, max_packet_size))
			return;
	} else {
		LOG(dout_con<<m_connection->getDesc()
				<<" Queueing reliable command for peer id: " << c->peer_id
				<<" data size: " << c->data.getSize() <<std::endl);

		if (chan.queued_commands.size() + 1 >= chan.getWindowSize() / 2) {
			LOG(derr_con << m_connection->getDesc()
					<< "Possible packet stall to peer id: " << c->peer_id
					<< " queued_commands=" << chan.queued_commands.size()
					<< std::endl);
		}
	}
	chan.queued_commands.push_back(c);
}

bool UDPPeer::processReliableSendCommand(
				ConnectionCommandPtr &c_ptr,
				unsigned int max_packet_size)
{
	if (m_pending_disconnect)
		return true;

	const auto &c = *c_ptr;
	Channel &chan = channels[c.channelnum];

	const u32 chunksize_max = max_packet_size
							- BASE_HEADER_SIZE
							- RELIABLE_HEADER_SIZE;

	std::list<SharedBuffer<u8>> originals;

	if (c.raw) {
		originals.emplace_back(c.data);
	} else {
		u16 split_seqnum = chan.readNextSplitSeqNum();
		makeAutoSplitPacket(c.data, chunksize_max, split_seqnum, &originals);
		chan.setNextSplitSeqNum(split_seqnum);
	}

	sanity_check(originals.size() < MAX_RELIABLE_WINDOW_SIZE);

	bool have_sequence_number = false;
	bool have_initial_sequence_number = false;
	std::queue<BufferedPacketPtr> toadd;
	u16 initial_sequence_number = 0;

	for (SharedBuffer<u8> &original : originals) {
		u16 seqnum = chan.getOutgoingSequenceNumber(have_sequence_number);

		/* oops, we don't have enough sequence numbers to send this packet */
		if (!have_sequence_number)
			break;

		if (!have_initial_sequence_number)
		{
			initial_sequence_number = seqnum;
			have_initial_sequence_number = true;
		}

		SharedBuffer<u8> reliable = makeReliablePacket(original, seqnum);

		// Add base headers and make a packet
		BufferedPacketPtr p = con::makePacket(address, reliable,
				m_connection->GetProtocolID(), m_connection->GetPeerID(),
				c.channelnum);

		toadd.push(p);
	}

	if (have_sequence_number) {
		while (!toadd.empty()) {
			BufferedPacketPtr p = toadd.front();
			toadd.pop();
//			LOG(dout_con<<connection->getDesc()
//					<< " queuing reliable packet for peer_id: " << c.peer_id
//					<< " channel: " << (c.channelnum&0xFF)
//					<< " seqnum: " << readU16(&p.data[BASE_HEADER_SIZE+1])
//					<< std::endl)
			chan.queued_reliables.push(p);
		}
		sanity_check(chan.queued_reliables.size() < 0xFFFF);
		return true;
	}

	u16 packets_available = toadd.size();
	/* we didn't get a single sequence number no need to fill queue */
	if (!have_initial_sequence_number) {
		dout_con << m_connection->getDesc() << " No sequence numbers available!" << std::endl;
		return false;
	}

	while (!toadd.empty()) {
		/* remove packet */
		toadd.pop();

		bool successfully_put_back_sequence_number
			= chan.putBackSequenceNumber(
				(initial_sequence_number+toadd.size() % (SEQNUM_MAX+1)));

		FATAL_ERROR_IF(!successfully_put_back_sequence_number, "error");
	}

	u32 n_queued = chan.outgoing_reliables_sent.size();

	LOG(dout_con<<m_connection->getDesc()
			<< " Windowsize exceeded on reliable sending "
			<< c.data.getSize() << " bytes"
			<< std::endl << "\t\tinitial_sequence_number: "
			<< initial_sequence_number
			<< std::endl << "\t\tgot at most            : "
			<< packets_available << " packets"
			<< std::endl << "\t\tpackets queued         : "
			<< n_queued
			<< std::endl);

	return false;
}

void UDPPeer::RunCommandQueues(
					unsigned int max_packet_size,
					unsigned int maxtransfer)
{

	for (Channel &channel : channels) {

		if ((!channel.queued_commands.empty()) &&
				(channel.queued_reliables.size() < maxtransfer)) {
			try {
				ConnectionCommandPtr c = channel.queued_commands.front();

				LOG(dout_con << m_connection->getDesc()
						<< " processing queued reliable command " << std::endl);

				// Packet is processed, remove it from queue
				if (processReliableSendCommand(c, max_packet_size)) {
					channel.queued_commands.pop_front();
				} else {
					LOG(dout_con << m_connection->getDesc()
							<< " Failed to queue packets for peer_id: " << c->peer_id
							<< ", delaying sending of " << c->data.getSize()
							<< " bytes" << std::endl);
				}
			}
			catch (ItemNotFoundException &e) {
				// intentionally empty
			}
		}
	}
}

u16 UDPPeer::getNextSplitSequenceNumber(u8 channel)
{
	assert(channel < CHANNEL_COUNT); // Pre-condition
	return channels[channel].readNextSplitSeqNum();
}

void UDPPeer::setNextSplitSequenceNumber(u8 channel, u16 seqnum)
{
	assert(channel < CHANNEL_COUNT); // Pre-condition
	channels[channel].setNextSplitSeqNum(seqnum);
}

SharedBuffer<u8> UDPPeer::addSplitPacket(u8 channel, BufferedPacketPtr &toadd,
	bool reliable)
{
	assert(channel < CHANNEL_COUNT); // Pre-condition
	return channels[channel].incoming_splits.insert(toadd, reliable);
}

} // namespace con
