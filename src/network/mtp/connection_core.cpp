// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "network/mtp/internal.h"
#include "network/mtp/impl_constants.h"
#include "network/encryption_log.h"
#include "network/mtp/threads.h"
#include "network/peerhandler.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "log.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/string.h"

namespace con
{

/*
	ConnectionCommand
 */

ConnectionCommandPtr ConnectionCommand::create(ConnectionCommandType type)
{
	return ConnectionCommandPtr(new ConnectionCommand(type));
}

ConnectionCommandPtr ConnectionCommand::serve(Address address)
{
	auto c = create(CONNCMD_SERVE);
	c->address = address;
	return c;
}

ConnectionCommandPtr ConnectionCommand::connect(Address address)
{
	auto c = create(CONNCMD_CONNECT);
	c->address = address;
	return c;
}

ConnectionCommandPtr ConnectionCommand::disconnect()
{
	return create(CONNCMD_DISCONNECT);
}

ConnectionCommandPtr ConnectionCommand::disconnect_peer(session_t peer_id)
{
	auto c = create(CONNCMD_DISCONNECT_PEER);
	c->peer_id = peer_id;
	return c;
}

ConnectionCommandPtr ConnectionCommand::resend_one(session_t peer_id)
{
	auto c = create(CONNCMD_RESEND_ONE);
	c->peer_id = peer_id;
	c->channelnum = 0; // must be same as createPeer
	c->reliable = true;
	return c;
}

ConnectionCommandPtr ConnectionCommand::peer_id_set(session_t own_peer_id)
{
	auto c = create(CONNCMD_PEER_ID_SET);
	c->peer_id = own_peer_id;
	return c;
}

ConnectionCommandPtr ConnectionCommand::send(session_t peer_id, u8 channelnum,
	NetworkPacket *pkt, bool reliable)
{
	auto c = create(CONNCMD_SEND);
	c->peer_id = peer_id;
	c->channelnum = channelnum;
	c->reliable = reliable;
	c->data = pkt->oldForgePacket();
	return c;
}

ConnectionCommandPtr ConnectionCommand::ack(session_t peer_id, u8 channelnum, const Buffer<u8> &data)
{
	auto c = create(CONCMD_ACK);
	c->peer_id = peer_id;
	c->channelnum = channelnum;
	c->reliable = false;
	data.copyTo(c->data);
	return c;
}

ConnectionCommandPtr ConnectionCommand::createPeer(session_t peer_id, const Buffer<u8> &data)
{
	auto c = create(CONCMD_CREATE_PEER);
	c->peer_id = peer_id;
	c->channelnum = 0;
	c->reliable = true;
	c->raw = true;
	data.copyTo(c->data);
	return c;
}

ConnectionCommandPtr ConnectionCommand::activate_encryption(session_t peer_id)
{
	auto c = create(CONNCMD_ACTIVATE_ENCRYPTION);
	c->peer_id = peer_id;
	return c;
}

/*
	ConnectionEvent
*/

const char *ConnectionEvent::describe() const
{
	switch(type) {
	case CONNEVENT_NONE:
		return "CONNEVENT_NONE";
	case CONNEVENT_DATA_RECEIVED:
		return "CONNEVENT_DATA_RECEIVED";
	case CONNEVENT_PEER_ADDED:
		return "CONNEVENT_PEER_ADDED";
	case CONNEVENT_PEER_REMOVED:
		return "CONNEVENT_PEER_REMOVED";
	case CONNEVENT_BIND_FAILED:
		return "CONNEVENT_BIND_FAILED";
	}
	return "Invalid ConnectionEvent";
}


ConnectionEventPtr ConnectionEvent::create(ConnectionEventType type)
{
	return std::shared_ptr<ConnectionEvent>(new ConnectionEvent(type));
}

ConnectionEventPtr ConnectionEvent::dataReceived(session_t peer_id, const Buffer<u8> &data)
{
	auto e = create(CONNEVENT_DATA_RECEIVED);
	e->peer_id = peer_id;
	data.copyTo(e->data);
	return e;
}

ConnectionEventPtr ConnectionEvent::peerAdded(session_t peer_id, Address address)
{
	auto e = create(CONNEVENT_PEER_ADDED);
	e->peer_id = peer_id;
	e->address = address;
	return e;
}

ConnectionEventPtr ConnectionEvent::peerRemoved(session_t peer_id, bool is_timeout, Address address)
{
	auto e = create(CONNEVENT_PEER_REMOVED);
	e->peer_id = peer_id;
	e->timeout = is_timeout;
	e->address = address;
	return e;
}

ConnectionEventPtr ConnectionEvent::bindFailed()
{
	return create(CONNEVENT_BIND_FAILED);
}

/*
	Connection
*/

Connection::Connection(u32 max_packet_size, float timeout,
		bool ipv6, PeerHandler *peerhandler) :
	m_udpSocket(ipv6),
	m_protocol_id(PROTOCOL_ID),
	m_sendThread(new ConnectionSendThread(max_packet_size, timeout)),
	m_receiveThread(new ConnectionReceiveThread()),
	m_bc_peerhandler(peerhandler)

{
	/* Amount of time Receive() will wait for data, this is entirely different
	 * from the connection timeout */
	m_udpSocket.setTimeoutMs(500);

	m_sendThread->setParent(this);
	m_receiveThread->setParent(this);

	m_sendThread->start();
	m_receiveThread->start();
}


Connection::~Connection()
{
	m_shutting_down = true;
	// request threads to stop
	m_sendThread->stop();
	m_receiveThread->stop();

	// wait for threads to finish
	m_sendThread->wait();
	m_receiveThread->wait();

	// Delete peers
	for (auto &peer : m_peers) {
		delete peer.second;
	}
}

/* Internal stuff */

void Connection::putEvent(ConnectionEventPtr e)
{
	assert(e->type != CONNEVENT_NONE); // Pre-condition
	m_event_queue.push_back(e);
}

void Connection::TriggerSend()
{
	m_sendThread->Trigger();
}

PeerHelper Connection::getPeerNoEx(session_t peer_id)
{
	MutexAutoLock peerlock(m_peers_mutex);
	std::map<session_t, Peer *>::iterator node = m_peers.find(peer_id);

	if (node == m_peers.end()) {
		return PeerHelper(NULL);
	}

	// Error checking
	FATAL_ERROR_IF(node->second->id != peer_id, "Invalid peer id");

	return PeerHelper(node->second);
}

/* find peer_id for address */
session_t Connection::lookupPeer(const Address& sender)
{
	MutexAutoLock peerlock(m_peers_mutex);
	for (auto &it: m_peers) {
		Peer *peer = it.second;
		if (peer->isPendingDeletion())
			continue;

		if (peer->getAddress() == sender)
			return peer->id;
	}

	return PEER_ID_INEXISTENT;
}

u32 Connection::getActiveCount()
{
	MutexAutoLock peerlock(m_peers_mutex);
	u32 count = 0;
	for (auto &it : m_peers) {
		Peer *peer = it.second;
		if (peer->isPendingDeletion())
			continue;
		if (peer->isHalfOpen())
			continue;
		count++;
	}
	return count;
}

bool Connection::deletePeer(session_t peer_id, bool timeout)
{
	Peer *peer = 0;

	/* lock list as short as possible */
	{
		MutexAutoLock peerlock(m_peers_mutex);
		if (m_peers.find(peer_id) == m_peers.end())
			return false;
		peer = m_peers[peer_id];
		m_peers.erase(peer_id);
		auto it = std::find(m_peer_ids.begin(), m_peer_ids.end(), peer_id);
		m_peer_ids.erase(it);
	}

	// Create event
	putEvent(ConnectionEvent::peerRemoved(peer_id, timeout, peer->getAddress()));

	peer->Drop();
	return true;
}

/* Interface */

ConnectionEventPtr Connection::waitEvent(u32 timeout_ms)
{
	try {
		return m_event_queue.pop_front(timeout_ms);
	} catch(ItemNotFoundException &ex) {
		return ConnectionEvent::create(CONNEVENT_NONE);
	}
}

void Connection::putCommand(ConnectionCommandPtr c)
{
	if (!m_shutting_down) {
		m_command_queue.push_back(c);
		m_sendThread->Trigger();
	}
}

void Connection::Serve(Address bind_addr)
{
	putCommand(ConnectionCommand::serve(bind_addr));
}

void Connection::Connect(Address address)
{
	putCommand(ConnectionCommand::connect(address));
}

bool Connection::Connected()
{
	MutexAutoLock peerlock(m_peers_mutex);

	if (m_peers.size() != 1)
		return false;

	std::map<session_t, Peer *>::iterator node = m_peers.find(PEER_ID_SERVER);
	if (node == m_peers.end())
		return false;

	if (m_peer_id == PEER_ID_INEXISTENT)
		return false;

	return true;
}

void Connection::Disconnect()
{
	putCommand(ConnectionCommand::disconnect());
}

bool Connection::ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms)
{
	/*
		Note that this function can potentially wait infinitely if non-data
		events keep happening before the timeout expires.
		This is not considered to be a problem (is it?)
	*/
	for(;;) {
		ConnectionEventPtr e_ptr = waitEvent(timeout_ms);
		const ConnectionEvent &e = *e_ptr;

		if (e.type != CONNEVENT_NONE) {
			LOG(dout_con << getDesc() << ": Receive: got event: "
					<< e.describe() << std::endl);
		}

		switch (e.type) {
		case CONNEVENT_NONE:
			return false;
		case CONNEVENT_DATA_RECEIVED:
			// Data size is lesser than command size, ignoring packet
			if (e.data.getSize() < 2) {
				continue;
			}

			pkt->putRawPacket(*e.data, e.data.getSize(), e.peer_id);
			return true;
		case CONNEVENT_PEER_ADDED: {
			UDPPeer tmp(e.peer_id, e.address, this);
			if (m_bc_peerhandler)
				m_bc_peerhandler->peerAdded(&tmp);
			continue;
		}
		case CONNEVENT_PEER_REMOVED: {
			UDPPeer tmp(e.peer_id, e.address, this);
			if (m_bc_peerhandler)
				m_bc_peerhandler->deletingPeer(&tmp, e.timeout);
			continue;
		}
		case CONNEVENT_BIND_FAILED:
			throw ConnectionBindFailed("Failed to bind socket "
					"(port already in use?)");
		}
	}
	return false;
}

void Connection::Send(session_t peer_id, u8 channelnum,
		NetworkPacket *pkt, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	// approximate check similar to UDPPeer::processReliableSendCommand()
	// to get nicer errors / backtraces if this happens.
	if (reliable && pkt->getSize() > MAX_RELIABLE_WINDOW_SIZE*512) {
		std::ostringstream oss;
		oss << "Packet too big for window, peer_id=" << peer_id
			<< " command=" << pkt->getCommand() << " size=" << pkt->getSize();
		FATAL_ERROR(oss.str().c_str());
	}

	putCommand(ConnectionCommand::send(peer_id, channelnum, pkt, reliable));
}

Address Connection::GetPeerAddress(session_t peer_id)
{
	PeerHelper peer = getPeerNoEx(peer_id);

	if (!peer)
		throw PeerNotFoundException("No address for peer found!");
	return peer->getAddress();
}

float Connection::getPeerStat(session_t peer_id, rtt_stat_type type)
{
	PeerHelper peer = getPeerNoEx(peer_id);
	if (!peer)
		return -1;
	return peer->getStat(type);
}

float Connection::getLocalStat(rate_stat_type type)
{
	PeerHelper peer = getPeerNoEx(PEER_ID_SERVER);

	FATAL_ERROR_IF(!peer, "Connection::getLocalStat we couldn't get our own peer? are you serious???");

	float retval = 0;

	for (Channel &channel : dynamic_cast<UDPPeer *>(&peer)->channels) {
		switch(type) {
			case CUR_DL_RATE:
				retval += channel.getCurrentDownloadRateKB();
				break;
			case AVG_DL_RATE:
				retval += channel.getAvgDownloadRateKB();
				break;
			case CUR_INC_RATE:
				retval += channel.getCurrentIncomingRateKB();
				break;
			case AVG_INC_RATE:
				retval += channel.getAvgIncomingRateKB();
				break;
			case AVG_LOSS_RATE:
				retval += channel.getAvgLossRateKB();
				break;
			case CUR_LOSS_RATE:
				retval += channel.getCurrentLossRateKB();
				break;
		default:
			FATAL_ERROR("Connection::getLocalStat Invalid stat type");
		}
	}
	return retval;
}

session_t Connection::createPeer(const Address &sender, int fd)
{
	// Somebody wants to make a new connection

	// Get a unique peer id
	const session_t minimum = 2;
	const session_t overflow = MAX_UDP_PEERS;

	/*
		Find an unused peer id
	*/

	MutexAutoLock lock(m_peers_mutex);
	session_t peer_id_new;
	for (int tries = 0; tries < 100; tries++) {
		peer_id_new = myrand_range(minimum, overflow - 1);
		if (m_peers.find(peer_id_new) == m_peers.end())
			break;
	}
	if (m_peers.find(peer_id_new) != m_peers.end()) {
		errorstream << getDesc() << " ran out of peer ids" << std::endl;
		return PEER_ID_INEXISTENT;
	}

	// Create a peer
	Peer *peer = 0;
	peer = new UDPPeer(peer_id_new, sender, this);

	m_peers[peer->id] = peer;
	m_peer_ids.push_back(peer->id);

	LOG(dout_con << getDesc()
			<< "createPeer(): giving peer_id=" << peer_id_new << std::endl);

	{
		Buffer<u8> reply(4);
		writeU8(&reply[0], PACKET_TYPE_CONTROL);
		writeU8(&reply[1], CONTROLTYPE_SET_PEER_ID);
		writeU16(&reply[2], peer_id_new);
		putCommand(ConnectionCommand::createPeer(peer_id_new, reply));
	}

	// Create peer addition event
	putEvent(ConnectionEvent::peerAdded(peer_id_new, sender));

	// We're now talking to a valid peer_id
	return peer_id_new;
}

const std::string Connection::getDesc()
{
	MutexAutoLock _(m_info_mutex);
	return std::string("con(")+
			itos(m_udpSocket.GetHandle())+"/"+itos(m_peer_id)+")";
}

void Connection::DisconnectPeer(session_t peer_id)
{
	putCommand(ConnectionCommand::disconnect_peer(peer_id));
}

void Connection::SetPeerEncryptionState(session_t peer_id, const PeerEncryptionState &state)
{
	MutexAutoLock peerlock(m_peers_mutex);
	auto it = m_peers.find(peer_id);
	if (it == m_peers.end()) {
		warningstream << "Connection::SetPeerEncryptionState: peer " << peer_id
			<< " not found" << std::endl;
		return;
	}
	auto *udpPeer = dynamic_cast<UDPPeer *>(it->second);
	if (!udpPeer) {
		warningstream << "Connection::SetPeerEncryptionState: peer " << peer_id
			<< " is not a UDPPeer" << std::endl;
		return;
	}
	// Lock the target's encryption state before writing
	{
		auto enc_lock = udpPeer->encryption_state.lock();
		// Copy all fields except the mutex itself
		udpPeer->encryption_state.active.store(state.active.load(std::memory_order_acquire),
			std::memory_order_release);
		udpPeer->encryption_state.c2s = state.c2s;
		udpPeer->encryption_state.s2c = state.s2c;
		udpPeer->encryption_state.srp_session_key = state.srp_session_key;
		udpPeer->encryption_state.session_id = state.session_id;
		udpPeer->encryption_state.server_fingerprint = state.server_fingerprint;
		udpPeer->encryption_state.ecdh_completed.store(state.ecdh_completed.load(std::memory_order_acquire),
			std::memory_order_release);
		udpPeer->encryption_state.ecdh_private_key = state.ecdh_private_key;
		udpPeer->encryption_state.ecdh_public_key = state.ecdh_public_key;
		udpPeer->encryption_state.ecdh_shared_secret = state.ecdh_shared_secret;
		udpPeer->encryption_state.hkdf_salt = state.hkdf_salt;
		udpPeer->encryption_state.key_rotation_count = state.key_rotation_count;
		udpPeer->encryption_state.activated_at = state.activated_at;
		udpPeer->encryption_state.packets_since_audit = state.packets_since_audit;
		udpPeer->encryption_state.last_audit_time_ms = state.last_audit_time_ms;
	}
	enclog_init("Encryption state set for peer")
		<< EncLog::kv("peer", peer_id)
		<< EncLog::kv("active", state.active.load())
		<< EncLog::kv("session_id", state.session_id)
		<< std::endl;
}

void Connection::ActivatePeerEncryption(session_t peer_id)
{
	// Queue an activation command. The send thread will process this
	// AFTER sending any previously queued packets (like AUTH_ACCEPT),
	// ensuring the transition from plaintext to encrypted is clean.
	putCommand(ConnectionCommand::activate_encryption(peer_id));
	enclog_activate("Queued encryption activation command for peer")
		<< EncLog::kv("peer", peer_id)
		<< EncLog::kv("note", "will activate after queued packets are sent")
		<< std::endl;
}

void Connection::SetPeerID(session_t id)
{
	m_peer_id = id;
	// fix peer id in existing queued reliable packets
	if (id != PEER_ID_INEXISTENT)
		putCommand(ConnectionCommand::peer_id_set(id));
}

void Connection::doResendOne(session_t peer_id)
{
	assert(peer_id != PEER_ID_INEXISTENT);
	putCommand(ConnectionCommand::resend_one(peer_id));
}

void Connection::sendAck(session_t peer_id, u8 channelnum, u16 seqnum)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	LOG(dout_con<<getDesc()
			<<" Queuing ACK command to peer_id: " << peer_id <<
			" channel: " << (channelnum & 0xFF) <<
			" seqnum: " << seqnum << std::endl);

	SharedBuffer<u8> ack(4);
	writeU8(&ack[0], PACKET_TYPE_CONTROL);
	writeU8(&ack[1], CONTROLTYPE_ACK);
	writeU16(&ack[2], seqnum);

	putCommand(ConnectionCommand::ack(peer_id, channelnum, ack));
	m_sendThread->Trigger();
}

UDPPeer* Connection::createServerPeer(const Address &address)
{
	if (ConnectedToServer())
		throw ConnectionException("Already connected to a server");

	UDPPeer *peer = new UDPPeer(PEER_ID_SERVER, address, this);
	peer->SetFullyOpen();

	{
		MutexAutoLock lock(m_peers_mutex);
		m_peers[peer->id] = peer;
		m_peer_ids.push_back(peer->id);
	}

	return peer;
}

} // namespace con
