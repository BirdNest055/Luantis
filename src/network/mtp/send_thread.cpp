// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-2017 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 celeron55, Loic Blot <loic.blot@unix-experience.fr>

#include "network/mtp/threads_helpers.h"
#include "network/mtp/threads.h"
#include "network/crypto.h"
#include "network/encryption_log.h"
#include "log.h"
#include "profiler.h"
#include "settings.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "util/serialize.h"

namespace con
{

ConnectionSendThread::ConnectionSendThread(unsigned int max_packet_size,
        float timeout) :
        Thread("ConnectionSend"),
        m_max_packet_size(max_packet_size),
        m_timeout(timeout),
        m_max_data_packets_per_iteration(g_settings->getU16(MPPI_SETTING))
{
        auto &mppi = m_max_data_packets_per_iteration;
        mppi = MYMAX(mppi, 1);

        const auto mppi_default = Settings::getLayer(SL_DEFAULTS)->getU16(MPPI_SETTING);
        if (mppi < mppi_default) {
                warningstream << "You are running the network code with a non-default "
                        "configuration (" MPPI_SETTING "=" << mppi << "). "
                        "This is not recommended in production." << std::endl;
        }
}

void *ConnectionSendThread::run()
{
        assert(m_connection);

        LOG(dout_con << m_connection->getDesc()
                << "ConnectionSend thread started" << std::endl);

        u64 curtime = porting::getTimeMs();
        u64 lasttime = curtime;

        PROFILE(std::stringstream ThreadIdentifier);
        PROFILE(ThreadIdentifier << "ConnectionSend: [" << m_connection->getDesc() << "]");

        /* if stop is requested don't stop immediately but try to send all        */
        /* packets first */
        while (!stopRequested() || packetsQueued()) {
                BEGIN_DEBUG_EXCEPTION_HANDLER
                PROFILE(ScopeProfiler sp(g_profiler, ThreadIdentifier.str(), SPT_AVG));

                /* wait for trigger or timeout */
                m_send_sleep_semaphore.wait(50);

                /* remove all triggers */
                while (m_send_sleep_semaphore.wait(0)) {
                }

                lasttime = curtime;
                curtime = porting::getTimeMs();
                float dtime = CALC_DTIME(lasttime, curtime);

                m_iteration_packets_avaialble = m_max_data_packets_per_iteration;
                const auto &calculate_quota = [&] () -> u32 {
                        u32 numpeers = m_connection->getActiveCount();
                        if (numpeers > 0)
                                return MYMAX(1, m_iteration_packets_avaialble / numpeers);
                        return m_iteration_packets_avaialble;
                };

                /* first resend timed-out packets */
                runTimeouts(dtime, calculate_quota());
                if (m_iteration_packets_avaialble == 0) {
                        LOG(warningstream << m_connection->getDesc()
                                << " Packet quota used up after re-sending packets, "
                                << "max=" << m_max_data_packets_per_iteration << std::endl);
                }

                /* translate commands to packets */
                auto c = m_connection->m_command_queue.pop_frontNoEx(0);
                std::vector<session_t> pending_encryption_activations;
                while (c && c->type != CONNCMD_NONE) {
                        if (c->type == CONNCMD_ACTIVATE_ENCRYPTION) {
                                // Don't activate encryption yet — collect peer IDs
                                // and activate AFTER sendPackets() to ensure any
                                // previously queued packets (like AUTH_ACCEPT) are
                                // sent as plaintext first.
                                pending_encryption_activations.push_back(c->peer_id);
                                enclog_activate("Deferring CONNCMD_ACTIVATE_ENCRYPTION")
                                        << EncLog::kv("peer", c->peer_id)
                                        << EncLog::kv("reason", "waiting for queued packets to send as plaintext")
                                        << std::endl;
                        } else if (c->reliable)
                                processReliableCommand(c);
                        else
                                processNonReliableCommand(c);

                        c = m_connection->m_command_queue.pop_frontNoEx(0);
                }

                /* send queued packets */
                sendPackets(dtime, calculate_quota());

                /* v9: Now activate encryption for peers that had CONNCMD_ACTIVATE_ENCRYPTION.
                   This runs AFTER sendPackets(), so all previously queued packets have been
                   sent as plaintext. From this point on, packets will be encrypted. */
                for (session_t pid : pending_encryption_activations) {
                        PeerHelper peer = m_connection->getPeerNoEx(pid);
                        auto *udpPeer = dynamic_cast<UDPPeer *>(&peer);
                        if (udpPeer) {
                                udpPeer->encryption_state.activate();
                                udpPeer->encryption_state.activated_at = porting::getTimeS();

                                // Log the activation with full details
                                enclog_activate("Encryption ACTIVATED for peer")
                                        << EncLog::kv("peer", pid)
                                        << EncLog::kv("session_id", udpPeer->encryption_state.session_id)
                                        << EncLog::kv("fingerprint", udpPeer->encryption_state.server_fingerprint)
                                        << EncLog::kv("status", "ALL_FUTURE_PACKETS_ENCRYPTED")
                                        << std::endl;

                                // Log the security banner
                                EncLog::logSecureConnectionBanner(
                                        udpPeer->encryption_state.session_id,
                                        udpPeer->encryption_state.server_fingerprint,
                                        false,  // forward_secrecy
                                        true,   // replay_protection
                                        70,     // security score
                                        "Fair",
                                        udpPeer->encryption_state.c2s.packets_processed,
                                        udpPeer->encryption_state.s2c.packets_processed);
                        } else {
                                enclog_error("CONNCMD_ACTIVATE_ENCRYPTION: peer not found or not UDPPeer")
                                        << EncLog::kv("peer", pid)
                                        << std::endl;
                        }
                }

                END_DEBUG_EXCEPTION_HANDLER
        }

        PROFILE(g_profiler->remove(ThreadIdentifier.str()));
        return NULL;
}

void ConnectionSendThread::Trigger()
{
        m_send_sleep_semaphore.post();
}

bool ConnectionSendThread::packetsQueued()
{
        std::vector<session_t> peerIds = m_connection->getPeerIDs();

        if (!m_outgoing_queue.empty() && !peerIds.empty())
                return true;

        for (session_t peerId : peerIds) {
                PeerHelper peer = m_connection->getPeerNoEx(peerId);

                if (!peer)
                        continue;

                if (dynamic_cast<UDPPeer *>(&peer) == 0)
                        continue;

                for (Channel &channel : (dynamic_cast<UDPPeer *>(&peer))->channels) {
                        if (!channel.queued_commands.empty()) {
                                return true;
                        }
                }
        }


        return false;
}

void ConnectionSendThread::runTimeouts(float dtime, u32 peer_packet_quota)
{
        std::vector<session_t> timeouted_peers;
        std::vector<session_t> peerIds = m_connection->getPeerIDs();

        for (const session_t peerId : peerIds) {
                PeerHelper peer = m_connection->getPeerNoEx(peerId);

                if (!peer)
                        continue;

                UDPPeer *udpPeer = dynamic_cast<UDPPeer *>(&peer);
                if (!udpPeer)
                        continue;

                PROFILE(std::stringstream peerIdentifier);
                PROFILE(peerIdentifier << "runTimeouts[" << m_connection->getDesc()
                        << ";" << peerId << ";RELIABLE]");
                PROFILE(ScopeProfiler
                peerprofiler(g_profiler, peerIdentifier.str(), SPT_AVG));

                SharedBuffer<u8> data(2); // data for sending ping, required here because of goto

                /*
                        Check peer timeout
                */
                // When the connection is half-open give the peer less time.
                // Note that this time is also fixed since the timeout is not reset in half-open state.
                const float peer_timeout = peer->isHalfOpen() ?
                        std::max(INIT_PHASE_MIN_TIMEOUT, m_timeout / 4) : m_timeout;
                std::string reason;
                if (peer->isTimedOut(peer_timeout, reason)) {
                        infostream << m_connection->getDesc()
                                << "RunTimeouts(): Peer " << peer->id
                                << " has timed out (" << reason << ")"
                                << std::endl;
                        // Add peer to the list
                        timeouted_peers.push_back(peer->id);
                        // Don't bother going through the buffers of this one
                        continue;
                }

                float resend_timeout = udpPeer->getResendTimeout();
                for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
                        auto &channel = udpPeer->channels[ch];

                        // Remove timed out incomplete unreliable split packets
                        channel.incoming_splits.removeUnreliableTimedOuts(dtime, peer_timeout);

                        // Increment reliable packet times
                        channel.outgoing_reliables_sent.incrementTimeouts(dtime);

                        // Re-send timed out outgoing reliables
                        auto timed_outs = channel.outgoing_reliables_sent.getResend(
                                resend_timeout, peer_packet_quota);

                        channel.UpdatePacketLossCounter(timed_outs.size());
                        if (timed_outs.size() > 0)
                                g_profiler->graphAdd("packets_lost", timed_outs.size());

                        // Note that this only happens during connection setup, it would
                        // break badly otherwise.
                        if (peer->isHalfOpen()) {
                                if (!timed_outs.empty()) {
                                        dout_con << m_connection->getDesc() <<
                                                "Skipping re-send of " << timed_outs.size() <<
                                                " timed-out reliables to peer_id=" << udpPeer->id
                                                << " channel=" << ch << " (half-open)." << std::endl;
                                }
                                continue;
                        }

                        if (m_iteration_packets_avaialble > timed_outs.size())
                                m_iteration_packets_avaialble -= timed_outs.size();
                        else
                                m_iteration_packets_avaialble = 0;

                        for (const auto &k : timed_outs)
                                resendReliable(channel, k.get(), resend_timeout);

                        auto ws_old = channel.getWindowSize();
                        channel.UpdateTimers(dtime);
                        auto ws_new = channel.getWindowSize();
                        if (ws_old != ws_new) {
                                dout_con << m_connection->getDesc() <<
                                        "Window size adjusted to " << ws_new << " for peer_id="
                                        << udpPeer->id << " channel=" << ch << std::endl;
                        }
                }

                /* send ping if necessary */
                if (udpPeer->Ping(dtime, data)) {
                        LOG(dout_con << m_connection->getDesc()
                                << "Sending ping for peer_id: " << udpPeer->id << std::endl);
                        rawSendAsPacket(udpPeer->id, 0, data, true);
                }

                udpPeer->RunCommandQueues(m_max_packet_size, m_max_packets_requeued);
        }

        // Remove timed out peers
        for (u16 timeouted_peer : timeouted_peers) {
                LOG(dout_con << m_connection->getDesc()
                        << "RunTimeouts(): Removing peer " << timeouted_peer << std::endl);
                m_connection->deletePeer(timeouted_peer, true);
        }
}

void ConnectionSendThread::resendReliable(Channel &channel, const BufferedPacket *k, float resend_timeout)
{
        assert(k);
        u8 channelnum = readChannel(k->data);
        u16 seqnum = k->getSeqnum();

        channel.UpdateBytesLost(k->size());

        derr_con << m_connection->getDesc()
                << "RE-SENDING timed-out RELIABLE to "
                << k->address.serializeString();
        if (resend_timeout >= 0)
                derr_con << "(t/o=" << resend_timeout << "): ";
        else
                derr_con << "(force): ";
        derr_con
                << "count=" << k->resend_count
                << ", channel=" << ((int) channelnum & 0xff)
                << ", seqnum=" << seqnum
                << std::endl;

        rawSend(k);

        // do not handle rtt here as we can't decide if this packet was
        // lost or really takes more time to transmit
}

void ConnectionSendThread::rawSend(const BufferedPacket *p)
{
        assert(p);
        try {
                // v9: Encrypt the packet if encryption is active for this peer.
                // The base header (7 bytes: protocol_id, peer_id, channel) stays unencrypted
                // so the receiver can route the packet. Everything after the base header
                // is encrypted with AES-256-GCM.
                //
                // Encrypted packet format after base header:
                //   [encrypted_flag(1B)][nonce(12B)][ciphertext(NB)][GCM_tag(16B)]
                //
                // The encrypted_flag byte is included as AAD (Additional Authenticated Data)
                // so it's authenticated but not encrypted — prevents flag tampering.

                // Look up the peer by destination address to check encryption state.
                // We cannot use readPeerId() here because that reads the SENDER peer_id
                // from the packet header. On the server, the sender is always PEER_ID_SERVER,
                // and the server doesn't have a UDPPeer entry for itself. We need the
                // DESTINATION peer to find its encryption state.
                session_t dest_peer_id = m_connection->lookupPeer(p->address);
                // On the server, GetPeerID() returns PEER_ID_SERVER.
                // On the client, GetPeerID() returns the assigned peer ID (not PEER_ID_SERVER).
                bool we_are_server = (m_connection->GetPeerID() == PEER_ID_SERVER);
                PeerHelper peer = m_connection->getPeerNoEx(dest_peer_id);
                auto *udpPeer = dynamic_cast<UDPPeer *>(&peer);

                if (udpPeer && udpPeer->encryption_state.active.load(std::memory_order_acquire)) {
                        // Encryption is active — encrypt the packet.
                        // Use a scope to hold the lock only while accessing key/nonce data.
                        std::vector<u8> encrypted_packet;
                        bool encrypt_ok = false;

                        {
                                auto lock = udpPeer->encryption_state.lock();

                                // Determine which direction key to use.
                                // Server sends with S2C key, client sends with C2S key.
                                DirectionalEncryptionState &dir_state =
                                        we_are_server ? udpPeer->encryption_state.s2c
                                                  : udpPeer->encryption_state.c2s;

                                // Build nonce
                                std::array<u8, GCM_NONCE_SIZE> nonce = dir_state.nextNonce();

                                // AAD: the encrypted flag byte (authenticated but not encrypted)
                                u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

                                // Plaintext is everything after the base header
                                const u8 *plaintext = &p->data[BASE_HEADER_SIZE];
                                size_t plaintext_len = p->size() - BASE_HEADER_SIZE;

                                // Encrypt
                                CryptoResult result = aes256gcm_encrypt(
                                        dir_state.key.data(), dir_state.key.size(),
                                        nonce.data(), nonce.size(),
                                        plaintext, plaintext_len,
                                        &encrypted_flag, 1);

                                if (result.success) {
                                        // Build the encrypted packet:
                                        // [base_header(7B)][encrypted_flag(1B)][nonce(12B)][ciphertext][tag(16B)]
                                        size_t encrypted_size = BASE_HEADER_SIZE + ENCRYPTED_PACKET_OVERHEAD + result.data.size();
                                        encrypted_packet.resize(encrypted_size);

                                        // Copy base header (unencrypted)
                                        memcpy(encrypted_packet.data(), p->data, BASE_HEADER_SIZE);

                                        // Write encrypted flag
                                        encrypted_packet[BASE_HEADER_SIZE] = encrypted_flag;

                                        // Write nonce
                                        memcpy(encrypted_packet.data() + BASE_HEADER_SIZE + 1,
                                                nonce.data(), GCM_NONCE_SIZE);

                                        // Write ciphertext
                                        if (!result.data.empty()) {
                                                memcpy(encrypted_packet.data() + BASE_HEADER_SIZE + 1 + GCM_NONCE_SIZE,
                                                        result.data.data(), result.data.size());
                                        }

                                        // Write GCM tag
                                        memcpy(encrypted_packet.data() + encrypted_size - GCM_TAG_SIZE,
                                                result.tag.data(), GCM_TAG_SIZE);

                                        encrypt_ok = true;

                                        // Log first encrypted packet for this direction
                                        if (dir_state.packets_processed == 1) {
                                                enclog_send("First encrypted packet sent")
                                                        << EncLog::kv("peer", dest_peer_id)
                                                        << EncLog::kv("direction", we_are_server ? "S2C" : "C2S")
                                                        << EncLog::kv("plaintext_size", (u32)plaintext_len)
                                                        << EncLog::kv("encrypted_size", (u32)encrypted_size)
                                                        << EncLog::kv("overhead", (u32)ENCRYPTED_PACKET_OVERHEAD)
                                                        << std::endl;
                                        }
                                } else {
                                        // Encryption failed — this is a critical error.
                                        // Drop the packet and log the error.
                                        dir_state.auth_failures++;
                                        enclog_error("Encryption FAILED for peer")
                                                << EncLog::kv("peer", dest_peer_id)
                                                << EncLog::kv("error", result.error_msg)
                                                << EncLog::kv("action", "PACKET_DROPPED")
                                                << EncLog::kv("auth_failures", dir_state.auth_failures)
                                                << std::endl;
                                }
                        }
                        // Lock is released here

                        if (encrypt_ok) {
                                m_connection->m_udpSocket.Send(p->address,
                                        encrypted_packet.data(), encrypted_packet.size());
                        }
                } else {
                        // No encryption — send plaintext
                        m_connection->m_udpSocket.Send(p->address, p->data, p->size());
                }
        } catch (SendFailedException &e) {
                LOG(derr_con << m_connection->getDesc()
                        << "SendFailedException: " << e.what() << " to "
                        << p->address.serializeString() << std::endl);
        }
}

void ConnectionSendThread::sendAsPacketReliable(BufferedPacketPtr &p, Channel *channel)
{
        try {
                p->absolute_send_time = porting::getTimeMs();
                // Buffer the packet
                channel->outgoing_reliables_sent.insert(p,
                        (channel->readOutgoingSequenceNumber() - MAX_RELIABLE_WINDOW_SIZE)
                                % (MAX_RELIABLE_WINDOW_SIZE + 1));
                // wtf is this calculation?? ^
        }
        catch (AlreadyExistsException &e) {
                LOG(derr_con << m_connection->getDesc()
                        << "WARNING: Going to send a reliable packet"
                        << " in outgoing buffer" << std::endl);
        }

        // Send the packet
        rawSend(p.get());
}

bool ConnectionSendThread::rawSendAsPacket(session_t peer_id, u8 channelnum,
        const SharedBuffer<u8> &data, bool reliable)
{
        PeerHelper peer = m_connection->getPeerNoEx(peer_id);
        if (!peer) {
                LOG(errorstream << m_connection->getDesc()
                        << " dropped " << (reliable ? "reliable " : "")
                        << "packet for non existent peer_id: " << peer_id << std::endl);
                return false;
        }
        Channel *channel = &(dynamic_cast<UDPPeer *>(&peer)->channels[channelnum]);

        if (reliable) {
                bool have_seqnum = false;
                const u16 seqnum = channel->getOutgoingSequenceNumber(have_seqnum);

                if (!have_seqnum)
                        return false;

                SharedBuffer<u8> reliable = makeReliablePacket(data, seqnum);

                // Add base headers and make a packet
                BufferedPacketPtr p = con::makePacket(peer->getAddress(), reliable,
                        m_connection->GetProtocolID(), m_connection->GetPeerID(),
                        channelnum);

                // first check if our send window is already maxed out
                if (channel->outgoing_reliables_sent.size() < channel->getWindowSize()) {
                        LOG(dout_con << m_connection->getDesc()
                                << " INFO: sending a reliable packet to peer_id " << peer_id
                                << " channel: " << (u32)channelnum
                                << " seqnum: " << seqnum << std::endl);
                        sendAsPacketReliable(p, channel);
                        return true;
                }

                LOG(dout_con << m_connection->getDesc()
                        << " INFO: queueing reliable packet for peer_id: " << peer_id
                        << " channel: " << (u32)channelnum
                        << " seqnum: " << seqnum << std::endl);
                channel->queued_reliables.push(p);
                return false;
        }

        // Add base headers and make a packet
        BufferedPacketPtr p = con::makePacket(peer->getAddress(), data,
                m_connection->GetProtocolID(), m_connection->GetPeerID(),
                channelnum);

        // Send the packet
        rawSend(p.get());
        return true;
}

void ConnectionSendThread::processReliableCommand(ConnectionCommandPtr &c)
{
        assert(c->reliable);  // Pre-condition

        switch (c->type) {
                case CONNCMD_NONE:
                        LOG(dout_con << m_connection->getDesc()
                                << "UDP processing reliable CONNCMD_NONE" << std::endl);
                        return;

                case CONNCMD_SEND:
                        LOG(dout_con << m_connection->getDesc()
                                << "UDP processing reliable CONNCMD_SEND" << std::endl);
                        sendReliable(c);
                        return;

                case CONNCMD_SEND_TO_ALL:
                        LOG(dout_con << m_connection->getDesc()
                                << "UDP processing CONNCMD_SEND_TO_ALL" << std::endl);
                        sendToAllReliable(c);
                        return;

                case CONCMD_CREATE_PEER:
                        LOG(dout_con << m_connection->getDesc()
                                << "UDP processing reliable CONCMD_CREATE_PEER" << std::endl);
                        if (!rawSendAsPacket(c->peer_id, c->channelnum, c->data, c->reliable)) {
                                /* put to queue if we couldn't send it immediately */
                                sendReliable(c);
                        }
                        return;

                case CONNCMD_RESEND_ONE: {
                        LOG(dout_con << m_connection->getDesc()
                                << "UDP processing reliable CONNCMD_RESEND_ONE" << std::endl);

                        PeerHelper peer = m_connection->getPeerNoEx(c->peer_id);
                        if (!peer)
                                return;
                        Channel &channel = dynamic_cast<UDPPeer *>(&peer)->channels[c->channelnum];

                        auto list = channel.outgoing_reliables_sent.getResend(0, 1);

                        if (!list.empty()) {
                                auto *packet = list.front().get();
                                // During the init phase, if we want to resend a packet more
                                // often than reasonable (let's say once per second which
                                // the init phase can take), someone is probably flooding us
                                // so stop replying.
                                constexpr u32 limit = INIT_PHASE_MIN_TIMEOUT + 1;
                                if (packet->resend_count > limit)
                                        return;
                                resendReliable(channel, packet, -1);
                        }

                        return;
                }

                case CONNCMD_SERVE:
                case CONNCMD_CONNECT:
                case CONNCMD_DISCONNECT:
                case CONCMD_ACK:
                        FATAL_ERROR("Got command that shouldn't be reliable as reliable command");
                default:
                        LOG(dout_con << m_connection->getDesc()
                                << " Invalid reliable command type: " << c->type << std::endl);
        }
}


void ConnectionSendThread::processNonReliableCommand(ConnectionCommandPtr &c_ptr)
{
        const ConnectionCommand &c = *c_ptr;
        assert(!c.reliable); // Pre-condition

        switch (c.type) {
                case CONNCMD_NONE:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_NONE" << std::endl);
                        return;
                case CONNCMD_SERVE:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_SERVE port="
                                << c.address.serializeString() << std::endl);
                        serve(c.address);
                        return;
                case CONNCMD_CONNECT:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_CONNECT" << std::endl);
                        connect(c.address);
                        return;
                case CONNCMD_DISCONNECT:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_DISCONNECT" << std::endl);
                        disconnect();
                        return;
                case CONNCMD_DISCONNECT_PEER:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_DISCONNECT_PEER" << std::endl);
                        disconnect_peer(c.peer_id);
                        return;
                case CONNCMD_PEER_ID_SET:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_PEER_ID_SET" << std::endl);
                        fix_peer_id(c.peer_id);
                        return;
                case CONNCMD_SEND:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_SEND" << std::endl);
                        send(c.peer_id, c.channelnum, c.data);
                        return;
                case CONNCMD_SEND_TO_ALL:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONNCMD_SEND_TO_ALL" << std::endl);
                        sendToAll(c.channelnum, c.data);
                        return;
                case CONCMD_ACK:
                        LOG(dout_con << m_connection->getDesc()
                                << " UDP processing CONCMD_ACK" << std::endl);
                        sendAsPacket(c.peer_id, c.channelnum, c.data, true);
                        return;
                case CONCMD_CREATE_PEER:
                case CONNCMD_RESEND_ONE:
                        FATAL_ERROR("Got command that should be reliable as unreliable command");
                default:
                        LOG(dout_con << m_connection->getDesc()
                                << " Invalid command type: " << c.type << std::endl);
        }
}

void ConnectionSendThread::serve(Address bind_address)
{
        LOG(dout_con << m_connection->getDesc()
                << "UDP serving at port " << bind_address.serializeString() << std::endl);
        try {
                m_connection->m_udpSocket.Bind(bind_address);
                m_connection->SetPeerID(PEER_ID_SERVER);
        }
        catch (SocketException &e) {
                // Create event
                m_connection->putEvent(ConnectionEvent::bindFailed());
        }
}

void ConnectionSendThread::connect(Address address)
{
        dout_con << m_connection->getDesc() << " connecting to ";
        address.print(dout_con);
        dout_con << std::endl;

        UDPPeer *peer = m_connection->createServerPeer(address);

        // Create event
        m_connection->putEvent(ConnectionEvent::peerAdded(peer->id, peer->address));

        Address bind_addr;
        if (address.isIPv6())
                bind_addr.setAddress(static_cast<IPv6AddressBytes*>(nullptr));
        else
                bind_addr.setAddress(static_cast<u32>(0));

        m_connection->m_udpSocket.Bind(bind_addr);

        // Send a dummy packet to server with peer_id = PEER_ID_INEXISTENT
        m_connection->SetPeerID(PEER_ID_INEXISTENT);
        NetworkPacket pkt(0, 0);
        m_connection->Send(PEER_ID_SERVER, 0, &pkt, true);
}

void ConnectionSendThread::disconnect()
{
        LOG(dout_con << m_connection->getDesc() << " disconnecting" << std::endl);

        // Create and send DISCO packet
        SharedBuffer<u8> data(2);
        writeU8(&data[0], PACKET_TYPE_CONTROL);
        writeU8(&data[1], CONTROLTYPE_DISCO);


        // Send to all
        std::vector<session_t> peerids = m_connection->getPeerIDs();

        for (session_t peerid : peerids) {
                sendAsPacket(peerid, 0, data, false);
        }
}

void ConnectionSendThread::disconnect_peer(session_t peer_id)
{
        LOG(dout_con << m_connection->getDesc() << " disconnecting peer" << std::endl);

        // Create and send DISCO packet
        SharedBuffer<u8> data(2);
        writeU8(&data[0], PACKET_TYPE_CONTROL);
        writeU8(&data[1], CONTROLTYPE_DISCO);
        sendAsPacket(peer_id, 0, data, false);

        PeerHelper peer = m_connection->getPeerNoEx(peer_id);

        if (!peer)
                return;

        if (dynamic_cast<UDPPeer *>(&peer) == 0) {
                return;
        }

        dynamic_cast<UDPPeer *>(&peer)->m_pending_disconnect = true;
}

void ConnectionSendThread::fix_peer_id(session_t own_peer_id)
{
        auto peer_ids = m_connection->getPeerIDs();
        for (const session_t peer_id : peer_ids) {
                PeerHelper peer = m_connection->getPeerNoEx(peer_id);
                if (!peer)
                        continue;

                auto *udp_peer = dynamic_cast<UDPPeer*>(&peer);
                if (!udp_peer)
                        continue;

                for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
                        auto &channel = udp_peer->channels[ch];

                        channel.outgoing_reliables_sent.fixPeerId(own_peer_id);
                }
        }
}

void ConnectionSendThread::send(session_t peer_id, u8 channelnum,
        const SharedBuffer<u8> &data)
{
        assert(channelnum < CHANNEL_COUNT); // Pre-condition

        PeerHelper peer = m_connection->getPeerNoEx(peer_id);
        if (!peer) {
                LOG(dout_con << m_connection->getDesc() << " peer: peer_id=" << peer_id
                        << ">>>NOT<<< found on sending packet"
                        << ", channel " << (channelnum % 0xFF)
                        << ", size: " << data.getSize() << std::endl);
                return;
        }

        LOG(dout_con << m_connection->getDesc() << " sending to peer_id=" << peer_id
                << ", channel " << (channelnum % 0xFF)
                << ", size: " << data.getSize() << std::endl);

        u16 split_sequence_number = peer->getNextSplitSequenceNumber(channelnum);

        u32 chunksize_max = m_max_packet_size - BASE_HEADER_SIZE;
        std::list<SharedBuffer<u8>> originals;

        makeAutoSplitPacket(data, chunksize_max, split_sequence_number, &originals);

        peer->setNextSplitSequenceNumber(channelnum, split_sequence_number);

        for (const SharedBuffer<u8> &original : originals) {
                sendAsPacket(peer_id, channelnum, original);
        }
}

void ConnectionSendThread::sendReliable(ConnectionCommandPtr &c)
{
        PeerHelper peer = m_connection->getPeerNoEx(c->peer_id);
        if (!peer)
                return;

        peer->PutReliableSendCommand(c, m_max_packet_size);
}

void ConnectionSendThread::sendToAll(u8 channelnum, const SharedBuffer<u8> &data)
{
        std::vector<session_t> peerids = m_connection->getPeerIDs();

        for (session_t peerid : peerids) {
                send(peerid, channelnum, data);
        }
}

void ConnectionSendThread::sendToAllReliable(ConnectionCommandPtr &c)
{
        std::vector<session_t> peerids = m_connection->getPeerIDs();

        for (session_t peerid : peerids) {
                PeerHelper peer = m_connection->getPeerNoEx(peerid);

                if (!peer)
                        continue;

                peer->PutReliableSendCommand(c, m_max_packet_size);
        }
}

void ConnectionSendThread::sendPackets(float dtime, u32 peer_packet_quota)
{
        std::vector<session_t> peerIds = m_connection->getPeerIDs();
        std::vector<session_t> pendingDisconnect;
        std::map<session_t, bool> pending_unreliable;

        for (session_t peerId : peerIds) {
                PeerHelper peer = m_connection->getPeerNoEx(peerId);
                //peer may have been removed
                if (!peer) {
                        LOG(dout_con << m_connection->getDesc() << " Peer not found: peer_id="
                                << peerId
                                << std::endl);
                        continue;
                }
                peer->m_increment_packets_remaining = peer_packet_quota;

                UDPPeer *udpPeer = dynamic_cast<UDPPeer *>(&peer);

                if (!udpPeer) {
                        continue;
                }

                if (udpPeer->m_pending_disconnect) {
                        pendingDisconnect.push_back(peerId);
                }

                PROFILE(std::stringstream
                peerIdentifier);
                PROFILE(
                        peerIdentifier << "sendPackets[" << m_connection->getDesc() << ";" << peerId
                                << ";RELIABLE]");
                PROFILE(ScopeProfiler
                peerprofiler(g_profiler, peerIdentifier.str(), SPT_AVG));

                //LOG(dout_con << m_connection->getDesc()
                //      << " Handle per peer queues: peer_id=" << peerId
                //      << " packet quota: " << peer->m_increment_packets_remaining << std::endl);

                // first send queued reliable packets for all peers (if possible)
                for (unsigned int i = 0; i < CHANNEL_COUNT; i++) {
                        Channel &channel = udpPeer->channels[i];

                        // Reduces logging verbosity
                        if (channel.queued_reliables.empty())
                                continue;

                        u16 next_to_ack = 0;
                        channel.outgoing_reliables_sent.getFirstSeqnum(next_to_ack);
                        u16 next_to_receive = 0;
                        channel.incoming_reliables.getFirstSeqnum(next_to_receive);

                        LOG(dout_con << m_connection->getDesc() << "\t channel: "
                                << i << ", peer quota:"
                                << peer->m_increment_packets_remaining
                                << std::endl
                                << "\t\t\treliables on wire: "
                                << channel.outgoing_reliables_sent.size()
                                << ", waiting for ack for " << next_to_ack
                                << std::endl
                                << "\t\t\tincoming_reliables: "
                                << channel.incoming_reliables.size()
                                << ", next reliable packet: "
                                << channel.readNextIncomingSeqNum()
                                << ", next queued: " << next_to_receive
                                << std::endl
                                << "\t\t\treliables queued : "
                                << channel.queued_reliables.size()
                                << std::endl
                                << "\t\t\tqueued commands  : "
                                << channel.queued_commands.size()
                                << std::endl);

                        while (!channel.queued_reliables.empty() &&
                                        channel.outgoing_reliables_sent.size()
                                        < channel.getWindowSize() &&
                                        peer->m_increment_packets_remaining > 0) {
                                BufferedPacketPtr p = channel.queued_reliables.front();
                                channel.queued_reliables.pop();

                                LOG(dout_con << m_connection->getDesc()
                                        << " INFO: sending a queued reliable packet "
                                        << " channel: " << i
                                        << ", seqnum: " << p->getSeqnum()
                                        << std::endl);

                                sendAsPacketReliable(p, &channel);
                                peer->m_increment_packets_remaining--;
                        }
                }
        }

        if (!m_outgoing_queue.empty()) {
                LOG(dout_con << m_connection->getDesc()
                        << " Handle non reliable queue ("
                        << m_outgoing_queue.size() << " pkts)" << std::endl);
        }

        unsigned int initial_queuesize = m_outgoing_queue.size();
        /* send non reliable packets*/
        for (unsigned int i = 0; i < initial_queuesize; i++) {
                OutgoingPacket packet = m_outgoing_queue.front();
                m_outgoing_queue.pop();

                if (packet.reliable)
                        continue;

                PeerHelper peer = m_connection->getPeerNoEx(packet.peer_id);
                if (!peer) {
                        LOG(dout_con << m_connection->getDesc()
                                << " Outgoing queue: peer_id=" << packet.peer_id
                                << ">>>NOT<<< found on sending packet"
                                << ", channel " << (packet.channelnum % 0xFF)
                                << ", size: " << packet.data.getSize() << std::endl);
                        continue;
                }

                /* send acks immediately */
                if (packet.ack || peer->m_increment_packets_remaining > 0 || stopRequested()) {
                        rawSendAsPacket(packet.peer_id, packet.channelnum,
                                packet.data, packet.reliable);
                        if (peer->m_increment_packets_remaining > 0)
                                peer->m_increment_packets_remaining--;
                } else {
                        m_outgoing_queue.push(packet);
                        pending_unreliable[packet.peer_id] = true;
                }
        }

        if (peer_packet_quota > 0 && !stopRequested()) {
                for (session_t peerId : peerIds) {
                        PeerHelper peer = m_connection->getPeerNoEx(peerId);
                        if (!peer)
                                continue;
                        if (peer->m_increment_packets_remaining == 0) {
                                LOG(warningstream << m_connection->getDesc()
                                        << " Packet quota used up for peer_id=" << peerId
                                        << ", was " << peer_packet_quota << " pkts" << std::endl);
                        }
                }
        }

        for (session_t peerId : pendingDisconnect) {
                if (!pending_unreliable[peerId]) {
                        m_connection->deletePeer(peerId, false);
                }
        }
}

void ConnectionSendThread::sendAsPacket(session_t peer_id, u8 channelnum,
        const SharedBuffer<u8> &data, bool ack)
{
        OutgoingPacket packet(peer_id, channelnum, data, false, ack);
        m_outgoing_queue.push(packet);
}

} // namespace con
