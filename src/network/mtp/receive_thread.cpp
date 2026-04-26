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

ConnectionReceiveThread::ConnectionReceiveThread() :
        Thread("ConnectionReceive")
{
}

void *ConnectionReceiveThread::run()
{
        assert(m_connection);

        LOG(dout_con << m_connection->getDesc()
                << "ConnectionReceive thread started" << std::endl);

        PROFILE(std::stringstream
        ThreadIdentifier);
        PROFILE(ThreadIdentifier << "ConnectionReceive: [" << m_connection->getDesc() << "]");

        // use IPv6 minimum allowed MTU as receive buffer size as this is
        // theoretical reliable upper boundary of a udp packet for all IPv6 enabled
        // infrastructure
        const unsigned int packet_maxsize = 1500;
        SharedBuffer<u8> packetdata(packet_maxsize);

        bool packet_queued = true;

#ifdef DEBUG_CONNECTION_KBPS
        u64 curtime = porting::getTimeMs();
        u64 lasttime = curtime;
        float debug_print_timer = 0.0;
#endif

        while (!stopRequested()) {
                BEGIN_DEBUG_EXCEPTION_HANDLER
                PROFILE(ScopeProfiler
                sp(g_profiler, ThreadIdentifier.str(), SPT_AVG));

#ifdef DEBUG_CONNECTION_KBPS
                lasttime = curtime;
                curtime = porting::getTimeMs();
                float dtime = CALC_DTIME(lasttime,curtime);
#endif

                /* receive packets */
                receive(packetdata, packet_queued);

#ifdef DEBUG_CONNECTION_KBPS
                debug_print_timer += dtime;
                if (debug_print_timer > 20.0) {
                        debug_print_timer -= 20.0;

                        std::vector<session_t> peerids = m_connection->getPeerIDs();

                        for (auto id : peerids)
                        {
                                PeerHelper peer = m_connection->getPeerNoEx(id);
                                if (!peer)
                                        continue;

                                float peer_current = 0.0;
                                float peer_loss = 0.0;
                                float avg_rate = 0.0;
                                float avg_loss = 0.0;

                                for(u16 j=0; j<CHANNEL_COUNT; j++)
                                {
                                        peer_current +=peer->channels[j].getCurrentDownloadRateKB();
                                        peer_loss += peer->channels[j].getCurrentLossRateKB();
                                        avg_rate += peer->channels[j].getAvgDownloadRateKB();
                                        avg_loss += peer->channels[j].getAvgLossRateKB();
                                }

                                std::stringstream output;
                                output << std::fixed << std::setprecision(1);
                                output << "OUT to Peer " << *i << " RATES (good / loss) " << std::endl;
                                output << "\tcurrent (sum): " << peer_current << "kb/s "<< peer_loss << "kb/s" << std::endl;
                                output << "\taverage (sum): " << avg_rate << "kb/s "<< avg_loss << "kb/s" << std::endl;
                                output << std::setfill(' ');
                                for(u16 j=0; j<CHANNEL_COUNT; j++)
                                {
                                        output << "\tcha " << j << ":"
                                                << " CUR: " << std::setw(6) << peer->channels[j].getCurrentDownloadRateKB() <<"kb/s"
                                                << " AVG: " << std::setw(6) << peer->channels[j].getAvgDownloadRateKB() <<"kb/s"
                                                << " MAX: " << std::setw(6) << peer->channels[j].getMaxDownloadRateKB() <<"kb/s"
                                                << " /"
                                                << " CUR: " << std::setw(6) << peer->channels[j].getCurrentLossRateKB() <<"kb/s"
                                                << " AVG: " << std::setw(6) << peer->channels[j].getAvgLossRateKB() <<"kb/s"
                                                << " MAX: " << std::setw(6) << peer->channels[j].getMaxLossRateKB() <<"kb/s"
                                                << " / WS: " << peer->channels[j].getWindowSize()
                                                << std::endl;
                                }

                                fprintf(stderr,"%s\n",output.str().c_str());
                        }
                }
#endif
                END_DEBUG_EXCEPTION_HANDLER
        }

        PROFILE(g_profiler->remove(ThreadIdentifier.str()));
        return NULL;
}

// Receive packets from the network and buffers and create ConnectionEvents
void ConnectionReceiveThread::receive(SharedBuffer<u8> &packetdata,
                bool &packet_queued)
{
        try {
                // First, see if there any buffered packets we can process now
                if (packet_queued) {
                        session_t peer_id;
                        SharedBuffer<u8> resultdata;
                        while (true) {
                                try {
                                        if (!getFromBuffers(peer_id, resultdata))
                                                break;

                                        m_connection->putEvent(ConnectionEvent::dataReceived(peer_id, resultdata));
                                }
                                catch (ProcessedSilentlyException &e) {
                                        /* try reading again */
                                }
                        }
                        packet_queued = false;
                }

                // Call Receive() to wait for incoming data
                Address sender;
                s32 received_size = m_connection->m_udpSocket.Receive(sender,
                        *packetdata, packetdata.getSize());
                if (received_size < 0)
                        return;

                if ((received_size < BASE_HEADER_SIZE) ||
                                (readU32(&packetdata[0]) != m_connection->GetProtocolID())) {
                        LOG(derr_con << m_connection->getDesc()
                                << "Receive(): Invalid incoming packet, "
                                << "size: " << received_size
                                << ", protocol: "
                                << ((received_size >= 4) ? readU32(&packetdata[0]) : -1)
                                << std::endl);
                        return;
                }

                session_t peer_id = readPeerId(*packetdata);
                u8 channelnum = readChannel(*packetdata);

                if (channelnum >= CHANNEL_COUNT) {
                        LOG(derr_con << m_connection->getDesc()
                                << "Receive(): Invalid channel " << (int)channelnum << std::endl);
                        return;
                }

                const bool knew_peer_id = peer_id != PEER_ID_INEXISTENT;

                if (!m_connection->ConnectedToServer()) {
                        // Try to identify peer by sender address
                        if (peer_id == PEER_ID_INEXISTENT) {
                                peer_id = m_connection->lookupPeer(sender);
                                if (peer_id != PEER_ID_INEXISTENT) {
                                        /* During join it can happen that the CONTROLTYPE_SET_PEER_ID
                                         * packet is lost. Since resends are not active at this stage
                                         * we need to remind the peer manually. */
                                        m_connection->doResendOne(peer_id);
                                }
                        }

                        // Someone new is trying to talk to us. Add them.
                        if (peer_id == PEER_ID_INEXISTENT) {
                                auto &l = m_new_peer_ratelimit;
                                l.tick();
                                if (++l.counter > MAX_NEW_PEERS_PER_SEC) {
                                        if (!l.logged) {
                                                warningstream << m_connection->getDesc()
                                                        << "Receive(): More than " << MAX_NEW_PEERS_PER_SEC
                                                        << " new clients within 1s. Throttling." << std::endl;
                                        }
                                        l.logged = true;
                                        // We simply drop the packet, the client can try again.
                                } else {
                                        peer_id = m_connection->createPeer(sender, 0);
                                }
                        }
                }

                PeerHelper peer = m_connection->getPeerNoEx(peer_id);
                if (!peer) {
                        LOG(dout_con << m_connection->getDesc()
                                << " got packet from unknown peer_id: "
                                << peer_id << " Ignoring." << std::endl);
                        return;
                }

                // Validate peer address

                if (sender != peer->getAddress()) {
                        LOG(derr_con << m_connection->getDesc()
                                << " Peer " << peer_id << " sending from different address."
                                " Ignoring." << std::endl);
                        return;
                }

                if (knew_peer_id) {
                        peer->SetFullyOpen();
                        // Setup phase has a fixed timeout
                        peer->ResetTimeout();
                } else if (!peer->isHalfOpen()) {
                        // If the peer talks to us without a peer ID when it has done so
                        // before something is definitely fishy.
                        LOG(derr_con << m_connection->getDesc()
                                << " Peer " << peer_id << " sending without peer id?!"
                                " Ignoring." << std::endl);
                        return;
                }

                auto *udpPeer = dynamic_cast<UDPPeer *>(&peer);
                if (!udpPeer) {
                        LOG(derr_con << m_connection->getDesc()
                                << "Receive(): peer_id=" << peer_id << " isn't an UDPPeer?!"
                                " Ignoring." << std::endl);
                        return;
                }
                Channel *channel = &udpPeer->channels[channelnum];

                channel->UpdateBytesReceived(received_size);

                // Throw the received packet to channel->processPacket()

                // Make a new SharedBuffer from the data without the base headers
                size_t data_after_header_size = received_size - BASE_HEADER_SIZE;

                // v9.15: Check if this packet is encrypted using the 0x80 flag.
                // The 0x80 flag byte is the SOLE determinant of whether a packet
                // is encrypted. If present, decrypt. If absent, always process as
                // plaintext — regardless of whether encryption is active.
                //
                // This eliminates the need for grace periods entirely:
                // - No 0x80 flag → plaintext, always accepted (no error, no warning)
                // - 0x80 flag present → encrypted, must decrypt
                //
                // During the plaintext→encrypted transition, the peer may still have
                // packets in the network pipeline from before it activated encryption.
                // These plaintext packets simply lack the 0x80 flag, so they are
                // processed normally without any special handling.
                SharedBuffer<u8> strippeddata;

                if (data_after_header_size >= ENCRYPTED_PACKET_OVERHEAD &&
                        packetdata[BASE_HEADER_SIZE] == ENCRYPTED_FLAG_AES_256_GCM) {

                        // This packet has the 0x80 flag — it's encrypted. Decrypt it.
                        // Lock encryption state for thread-safe access
                        auto enc_lock = udpPeer->encryption_state.lock();

                        const u8 *after_header = &packetdata[BASE_HEADER_SIZE];

                        // Parse encrypted packet structure
                        // [0]     encrypted_flag (0x80) — already checked
                        // [1..12] nonce (12 bytes)
                        // [13..N] ciphertext
                        // [N-15..N] GCM tag (16 bytes)
                        const u8 *nonce_ptr = after_header + 1;
                        const u8 *ciphertext_ptr = after_header + 1 + GCM_NONCE_SIZE;
                        size_t remaining = data_after_header_size - 1 - GCM_NONCE_SIZE;
                        if (remaining < GCM_TAG_SIZE) {
                                enclog_error("Encrypted packet too short from peer")
                                        << EncLog::kv("peer", peer_id)
                                        << EncLog::kv("remaining", (u32)remaining)
                                        << EncLog::kv("min_required", (u32)GCM_TAG_SIZE)
                                        << EncLog::kv("action", "PACKET_DROPPED")
                                        << std::endl;
                                return;
                        }
                        size_t ciphertext_len = remaining - GCM_TAG_SIZE;
                        const u8 *tag_ptr = after_header + data_after_header_size - GCM_TAG_SIZE;

                        // Determine which direction key to use for decryption.
                        // If we're the server, we receive packets encrypted with C2S key.
                        // If we're the client, we receive packets encrypted with S2C key.
                        bool we_are_server = (m_connection->GetPeerID() == PEER_ID_SERVER);
                        DirectionalEncryptionState &dir_state =
                                we_are_server ? udpPeer->encryption_state.c2s
                                          : udpPeer->encryption_state.s2c;

                        // Check that decryption key is initialized (not all zeros).
                        // This can happen if we receive an encrypted packet before
                        // our local key derivation is complete (e.g., during the
                        // ECDH handshake when the server activates before the client).
                        bool key_initialized = false;
                        for (size_t i = 0; i < dir_state.key.size(); i++) {
                                if (dir_state.key[i] != 0) {
                                        key_initialized = true;
                                        break;
                                }
                        }
                        if (!key_initialized) {
                                enclog_error("Received encrypted packet but decryption key not initialized")
                                        << EncLog::kv("peer", peer_id)
                                        << EncLog::kv("action", "PACKET_DROPPED")
                                        << EncLog::kv("hint", "key_derivation_may_not_have_completed_yet")
                                        << std::endl;
                                return;
                        }

                        // Extract nonce counter for replay detection
                        u64 received_counter = 0;
                        for (int i = 0; i < 8; i++) {
                                received_counter = (received_counter << 8) | nonce_ptr[NONCE_BASE_SIZE + i];
                        }

                        // Replay protection check
                        if (!dir_state.isNotReplay(received_counter)) {
                                enclog_error("Replay detected from peer")
                                        << EncLog::kv("peer", peer_id)
                                        << EncLog::kv("received_counter", received_counter)
                                        << EncLog::kv("expected_counter", dir_state.nonce_counter)
                                        << EncLog::kv("action", "PACKET_DROPPED")
                                        << EncLog::kv("replay_attempts", dir_state.replay_attempts + 1)
                                        << std::endl;
                                dir_state.replay_attempts++;
                                return;
                        }

                        // AAD: the encrypted flag byte
                        u8 encrypted_flag = ENCRYPTED_FLAG_AES_256_GCM;

                        // Decrypt
                        CryptoResult result = aes256gcm_decrypt(
                                dir_state.key.data(), dir_state.key.size(),
                                nonce_ptr, GCM_NONCE_SIZE,
                                ciphertext_ptr, ciphertext_len,
                                tag_ptr, GCM_TAG_SIZE,
                                &encrypted_flag, 1);

                        if (result.success) {
                                // Decryption succeeded — use decrypted data
                                strippeddata = SharedBuffer<u8>(result.data.size());
                                memcpy(*strippeddata, result.data.data(), result.data.size());

                                // Update high-water mark counter
                                dir_state.updateCounter(received_counter);

                                // Log first decrypted packet for this direction
                                if (dir_state.packets_processed == 0) {
                                        enclog_recv("First encrypted packet received and decrypted")
                                                << EncLog::kv("peer", peer_id)
                                                << EncLog::kv("direction", we_are_server ? "C2S" : "S2C")
                                                << EncLog::kv("ciphertext_size", (u32)ciphertext_len)
                                                << EncLog::kv("plaintext_size", (u32)result.data.size())
                                                << EncLog::kv("s2c_key_fp", keyToFingerprint(udpPeer->encryption_state.s2c.key.data(), AES256_KEY_SIZE).substr(0, 8))
                                                << EncLog::kv("c2s_key_fp", keyToFingerprint(udpPeer->encryption_state.c2s.key.data(), AES256_KEY_SIZE).substr(0, 8))
                                                << EncLog::kv("s2c_nonce_counter", (u64)udpPeer->encryption_state.s2c.nonce_counter)
                                                << EncLog::kv("c2s_nonce_counter", (u64)udpPeer->encryption_state.c2s.nonce_counter)
                                                << std::endl;
                                }
                                dir_state.packets_processed++;

                                // Auto-activate encryption if not already active.
                                // This happens when the peer activates encryption before us.
                                // Receiving a valid encrypted packet proves the peer has activated,
                                // so we should activate too for outgoing packets.
                                if (!udpPeer->encryption_state.active.load(std::memory_order_acquire)) {
                                        udpPeer->encryption_state.activate();
                                        udpPeer->encryption_state.activated_at = porting::getTimeS();
                                        enclog_activate("Encryption AUTO-ACTIVATED on receive")
                                                << EncLog::kv("peer", peer_id)
                                                << EncLog::kv("reason", "received valid encrypted packet from peer")
                                                << EncLog::kv("session_id", udpPeer->encryption_state.session_id)
                                                << EncLog::kv("fingerprint", udpPeer->encryption_state.server_fingerprint)
                                                << EncLog::kv("status", "ALL_FUTURE_PACKETS_ENCRYPTED")
                                                << std::endl;
                                }

                                // Periodic audit logging (every AUDIT_INTERVAL_MS or AUDIT_MIN_PACKETS)
                                udpPeer->encryption_state.packets_since_audit++;
                                u64 now_ms = porting::getTimeMs();
                                if (udpPeer->encryption_state.packets_since_audit >= PeerEncryptionState::AUDIT_MIN_PACKETS &&
                                        now_ms - udpPeer->encryption_state.last_audit_time_ms >= PeerEncryptionState::AUDIT_INTERVAL_MS) {
                                        udpPeer->encryption_state.last_audit_time_ms = now_ms;
                                        udpPeer->encryption_state.packets_since_audit = 0;
                                        EncLog::logEncryptionAudit(
                                                peer_id,
                                                true,
                                                udpPeer->encryption_state.session_id,
                                                udpPeer->encryption_state.c2s.packets_processed,
                                                udpPeer->encryption_state.s2c.packets_processed,
                                                udpPeer->encryption_state.c2s.auth_failures,
                                                udpPeer->encryption_state.s2c.auth_failures,
                                                udpPeer->encryption_state.c2s.replay_attempts,
                                                udpPeer->encryption_state.s2c.replay_attempts);
                                }
                        } else {
                                // Decryption failed — authentication tag mismatch.
                                // This means the packet was tampered with, corrupted,
                                // or encrypted with the wrong key.
                                // Log with diagnostic key fingerprints to help diagnose
                                // key mismatches (v9.15: added nonce counter info).
                                dir_state.auth_failures++;

                                // v9.15: Only log the first few failures at ERROR level,
                                // then throttle to avoid spamming the log with hundreds
                                // of identical messages. After 10 failures, log at most
                                // once per 100 failures. This prevents the log from being
                                // flooded while still providing diagnostic information.
                                bool should_log = (dir_state.auth_failures <= 3) ||
                                        (dir_state.auth_failures % 100 == 0);

                                if (should_log) {
                                        enclog_error("Decryption FAILED from peer")
                                                << EncLog::kv("peer", peer_id)
                                                << EncLog::kv("error", result.error_msg)
                                                << EncLog::kv("action", "PACKET_DROPPED")
                                                << EncLog::kv("auth_failures", dir_state.auth_failures)
                                                << EncLog::kv("direction", we_are_server ? "C2S" : "S2C")
                                                << EncLog::kv("s2c_key_fp", keyToFingerprint(udpPeer->encryption_state.s2c.key.data(), AES256_KEY_SIZE).substr(0, 8))
                                                << EncLog::kv("c2s_key_fp", keyToFingerprint(udpPeer->encryption_state.c2s.key.data(), AES256_KEY_SIZE).substr(0, 8))
                                                << EncLog::kv("s2c_nonce_counter", (u64)udpPeer->encryption_state.s2c.nonce_counter)
                                                << EncLog::kv("c2s_nonce_counter", (u64)udpPeer->encryption_state.c2s.nonce_counter)
                                                << EncLog::kv("received_counter", received_counter)
                                                << EncLog::kv("ecdh_completed", udpPeer->encryption_state.ecdh_completed.load())
                                                << EncLog::kv("session_id", udpPeer->encryption_state.session_id)
                                                << std::endl;
                                }
                                return;
                        }
                } else {
                        // No 0x80 flag → plaintext packet. Always process as plaintext.
                        // This is correct regardless of whether encryption is active:
                        // - Before encryption activates: normal plaintext packets
                        // - During transition: pipeline packets from before peer activated
                        // - After encryption fully active: should not happen, but if it
                        //   does, the packet is simply processed (0x80 flag is authoritative)
                        //
                        // v9.15: NO grace period logic, NO error logging, NO warnings.
                        // The 0x80 flag is the sole determinant. No flag = plaintext. Period.
                        strippeddata = SharedBuffer<u8>(data_after_header_size);
                        memcpy(*strippeddata, &packetdata[BASE_HEADER_SIZE],
                                data_after_header_size);
                }

                try {
                        // Process it (the result is some data with no headers made by us)
                        SharedBuffer<u8> resultdata = processPacket
                                (channel, strippeddata, peer_id, channelnum, false);

                        LOG(dout_con << m_connection->getDesc()
                                << " ProcessPacket from peer_id: " << peer_id
                                << ", channel: " << (u32)channelnum << ", returned "
                                << resultdata.getSize() << " bytes" << std::endl);

                        m_connection->putEvent(ConnectionEvent::dataReceived(peer_id, resultdata));
                }
                catch (ProcessedSilentlyException &e) {
                }
                catch (ProcessedQueued &e) {
                        // we set it to true anyway (see below)
                }

                /* Every time we receive a packet it can happen that a previously
                 * buffered packet is now ready to process. */
                packet_queued = true;
        }
        catch (InvalidIncomingDataException &e) {
        }
}

bool ConnectionReceiveThread::getFromBuffers(session_t &peer_id, SharedBuffer<u8> &dst)
{
        std::vector<session_t> peerids = m_connection->getPeerIDs();

        for (session_t peerid : peerids) {
                PeerHelper peer = m_connection->getPeerNoEx(peerid);
                if (!peer)
                        continue;

                UDPPeer *p = dynamic_cast<UDPPeer *>(&peer);
                if (!p)
                        continue;

                for (Channel &channel : p->channels) {
                        if (checkIncomingBuffers(&channel, peer_id, dst)) {
                                return true;
                        }
                }
        }
        return false;
}

bool ConnectionReceiveThread::checkIncomingBuffers(Channel *channel,
        session_t &peer_id, SharedBuffer<u8> &dst)
{
        u16 firstseqnum = 0;
        if (!channel->incoming_reliables.getFirstSeqnum(firstseqnum))
                return false;

        if (firstseqnum != channel->readNextIncomingSeqNum())
                return false;

        BufferedPacketPtr p = channel->incoming_reliables.popFirst();

        peer_id = readPeerId(p->data); // Carried over to caller function
        u8 channelnum = readChannel(p->data);
        u16 seqnum = p->getSeqnum();

        LOG(dout_con << m_connection->getDesc()
                << "UNBUFFERING TYPE_RELIABLE"
                << " seqnum=" << seqnum
                << " peer_id=" << peer_id
                << " channel=" << ((int) channelnum & 0xff)
                << std::endl);

        channel->incNextIncomingSeqNum();

        u32 headers_size = BASE_HEADER_SIZE + RELIABLE_HEADER_SIZE;
        // Get out the inside packet and re-process it
        SharedBuffer<u8> payload(p->size() - headers_size);
        memcpy(*payload, &p->data[headers_size], payload.getSize());

        dst = processPacket(channel, payload, peer_id, channelnum, true);
        return true;
}

SharedBuffer<u8> ConnectionReceiveThread::processPacket(Channel *channel,
        const SharedBuffer<u8> &packetdata, session_t peer_id, u8 channelnum, bool reliable)
{
        PeerHelper peer = m_connection->getPeerNoEx(peer_id);

        if (!peer) {
                errorstream << "Peer not found (possible timeout)" << std::endl;
                throw ProcessedSilentlyException("Peer not found (possible timeout)");
        }

        if (packetdata.getSize() < 1)
                throw InvalidIncomingDataException("packetdata.getSize() < 1");

        u8 type = readU8(&(packetdata[0]));

        if (MAX_UDP_PEERS <= 65535 && peer_id >= MAX_UDP_PEERS) {
                std::string errmsg = "Invalid peer_id=" + itos(peer_id);
                errorstream << errmsg << std::endl;
                throw InvalidIncomingDataException(errmsg.c_str());
        }

        if (type >= PACKET_TYPE_MAX) {
                derr_con << m_connection->getDesc() << "Got invalid type=" << ((int) type & 0xff)
                        << std::endl;
                throw InvalidIncomingDataException("Invalid packet type");
        }

        const PacketTypeHandler &pHandle = packetTypeRouter[type];
        return (this->*pHandle.handler)(channel, packetdata, &peer, channelnum, reliable);
}

const ConnectionReceiveThread::PacketTypeHandler
        ConnectionReceiveThread::packetTypeRouter[PACKET_TYPE_MAX] = {
        {&ConnectionReceiveThread::handlePacketType_Control},
        {&ConnectionReceiveThread::handlePacketType_Original},
        {&ConnectionReceiveThread::handlePacketType_Split},
        {&ConnectionReceiveThread::handlePacketType_Reliable},
};

SharedBuffer<u8> ConnectionReceiveThread::handlePacketType_Control(Channel *channel,
        const SharedBuffer<u8> &packetdata, Peer *peer, u8 channelnum, bool reliable)
{
        if (packetdata.getSize() < 2)
                throw InvalidIncomingDataException("packetdata.getSize() < 2");

        ControlType controltype = (ControlType)readU8(&(packetdata[1]));

        if (controltype == CONTROLTYPE_ACK) {
                assert(channel != NULL);

                if (packetdata.getSize() < 4) {
                        throw InvalidIncomingDataException(
                                "packetdata.getSize() < 4 (ACK header size)");
                }

                u16 seqnum = readU16(&packetdata[2]);
                LOG(dout_con << m_connection->getDesc() << " [ CONTROLTYPE_ACK: channelnum="
                        << ((int) channelnum & 0xff) << ", peer_id=" << peer->id << ", seqnum="
                        << seqnum << " ]" << std::endl);

                try {
                        BufferedPacketPtr p = channel->outgoing_reliables_sent.popSeqnum(seqnum);

                        // the rtt calculation will be a bit off for re-sent packets but that's okay
                        {
                                // Get round trip time
                                u64 current_time = porting::getTimeMs();

                                // an overflow is quite unlikely but as it'd result in major
                                // rtt miscalculation we handle it here
                                if (current_time > p->absolute_send_time) {
                                        float rtt = (current_time - p->absolute_send_time) / 1000.0f;

                                        // Let peer calculate stuff according to it
                                        // (avg_rtt and resend_timeout)
                                        dynamic_cast<UDPPeer *>(peer)->reportRTT(rtt);
                                } else if (p->totaltime > 0) {
                                        float rtt = p->totaltime;

                                        // Let peer calculate stuff according to it
                                        // (avg_rtt and resend_timeout)
                                        dynamic_cast<UDPPeer *>(peer)->reportRTT(rtt);
                                }
                        }

                        // put bytes for max bandwidth calculation
                        channel->UpdateBytesSent(p->size(), 1);
                        if (channel->outgoing_reliables_sent.size() == 0)
                                m_connection->TriggerSend();
                } catch (NotFoundException &e) {
                        LOG(derr_con << m_connection->getDesc()
                                << "WARNING: ACKed packet not in outgoing queue"
                                << " seqnum=" << seqnum << std::endl);
                        channel->UpdatePacketTooLateCounter();
                }

                throw ProcessedSilentlyException("Got an ACK");
        } else if (controltype == CONTROLTYPE_SET_PEER_ID) {
                // Got a packet to set our peer id
                if (packetdata.getSize() < 4)
                        throw InvalidIncomingDataException
                                ("packetdata.getSize() < 4 (SET_PEER_ID header size)");
                session_t peer_id_new = readU16(&packetdata[2]);
                LOG(dout_con << m_connection->getDesc() << "Got new peer id: " << peer_id_new
                        << "... " << std::endl);

                if (m_connection->GetPeerID() != PEER_ID_INEXISTENT) {
                        LOG(derr_con << m_connection->getDesc()
                                << "WARNING: Not changing existing peer id." << std::endl);
                } else {
                        LOG(dout_con << m_connection->getDesc() << "changing own peer id"
                                << std::endl);
                        m_connection->SetPeerID(peer_id_new);
                }

                throw ProcessedSilentlyException("Got a SET_PEER_ID");
        } else if (controltype == CONTROLTYPE_PING) {
                // Just ignore it, the incoming data already reset
                // the timeout counter
                LOG(dout_con << m_connection->getDesc() << "PING" << std::endl);
                throw ProcessedSilentlyException("Got a PING");
        } else if (controltype == CONTROLTYPE_DISCO) {
                // Just ignore it, the incoming data already reset
                // the timeout counter
                LOG(dout_con << m_connection->getDesc() << "DISCO: Removing peer "
                        << peer->id << std::endl);

                if (!m_connection->deletePeer(peer->id, false)) {
                        derr_con << m_connection->getDesc() << "DISCO: Peer not found" << std::endl;
                }

                throw ProcessedSilentlyException("Got a DISCO");
        } else {
                LOG(derr_con << m_connection->getDesc()
                        << "INVALID controltype="
                        << ((int) controltype & 0xff) << std::endl);
                throw InvalidIncomingDataException("Invalid control type");
        }
}

SharedBuffer<u8> ConnectionReceiveThread::handlePacketType_Original(Channel *channel,
        const SharedBuffer<u8> &packetdata, Peer *peer, u8 channelnum, bool reliable)
{
        if (packetdata.getSize() <= ORIGINAL_HEADER_SIZE)
                throw InvalidIncomingDataException
                        ("packetdata.getSize() <= ORIGINAL_HEADER_SIZE");
        LOG(dout_con << m_connection->getDesc() << "RETURNING TYPE_ORIGINAL to user"
                << std::endl);
        // Get the inside packet out and return it
        SharedBuffer<u8> payload(packetdata.getSize() - ORIGINAL_HEADER_SIZE);
        memcpy(*payload, &(packetdata[ORIGINAL_HEADER_SIZE]), payload.getSize());
        return payload;
}

SharedBuffer<u8> ConnectionReceiveThread::handlePacketType_Split(Channel *channel,
        const SharedBuffer<u8> &packetdata, Peer *peer, u8 channelnum, bool reliable)
{
        // We have to create a packet again for buffering
        // This isn't actually too bad an idea.
        BufferedPacketPtr packet = con::makePacket(peer->getAddress(),
                packetdata,
                m_connection->GetProtocolID(),
                peer->id,
                channelnum);

        // Buffer the packet
        SharedBuffer<u8> data = peer->addSplitPacket(channelnum, packet, reliable);

        if (data.getSize() != 0) {
                LOG(dout_con << m_connection->getDesc()
                        << "RETURNING TYPE_SPLIT: Constructed full data, "
                        << "size=" << data.getSize() << std::endl);
                return data;
        }
        LOG(dout_con << m_connection->getDesc() << "BUFFERED TYPE_SPLIT" << std::endl);
        throw ProcessedSilentlyException("Buffered a split packet chunk");
}

SharedBuffer<u8> ConnectionReceiveThread::handlePacketType_Reliable(Channel *channel,
        const SharedBuffer<u8> &packetdata, Peer *peer, u8 channelnum, bool reliable)
{
        assert(channel != NULL);

        // Recursive reliable packets not allowed
        if (reliable)
                throw InvalidIncomingDataException("Found nested reliable packets");

        if (packetdata.getSize() < RELIABLE_HEADER_SIZE)
                throw InvalidIncomingDataException("packetdata.getSize() < RELIABLE_HEADER_SIZE");

        const u16 seqnum = readU16(&packetdata[1]);
        bool is_future_packet = false;
        bool is_old_packet = false;

        /* packet is within our receive window send ack */
        if (seqnum_in_window(seqnum,
                channel->readNextIncomingSeqNum(), MAX_RELIABLE_WINDOW_SIZE)) {
                m_connection->sendAck(peer->id, channelnum, seqnum);
        } else {
                is_future_packet = seqnum_higher(seqnum, channel->readNextIncomingSeqNum());
                is_old_packet = seqnum_higher(channel->readNextIncomingSeqNum(), seqnum);

                /* packet is not within receive window, don't send ack.           *
                 * if this was a valid packet it's gonna be retransmitted         */
                if (is_future_packet)
                        throw ProcessedSilentlyException(
                                "Received packet newer then expected, not sending ack");

                /* seems like our ack was lost, send another one for an old packet */
                if (is_old_packet) {
                        LOG(dout_con << m_connection->getDesc()
                                << "RE-SENDING ACK: peer_id: " << peer->id
                                << ", channel: " << (channelnum & 0xFF)
                                << ", seqnum: " << seqnum << std::endl;)
                        m_connection->sendAck(peer->id, channelnum, seqnum);

                        throw ProcessedSilentlyException("Retransmitting ack for old packet");
                }
        }

        if (seqnum != channel->readNextIncomingSeqNum()) {
                // This one comes later, buffer it.
                // Actually we have to make a packet to buffer one.
                // Well, we have all the ingredients, so just do it.
                BufferedPacketPtr packet = con::makePacket(
                        peer->getAddress(),
                        packetdata,
                        m_connection->GetProtocolID(),
                        peer->id,
                        channelnum);
                try {
                        channel->incoming_reliables.insert(packet, channel->readNextIncomingSeqNum());

                        LOG(dout_con << m_connection->getDesc()
                                << "BUFFERING, TYPE_RELIABLE peer_id: " << peer->id
                                << ", channel: " << (channelnum & 0xFF)
                                << ", seqnum: " << seqnum << std::endl;)

                        throw ProcessedQueued("Buffered future reliable packet");
                } catch (AlreadyExistsException &e) {
                } catch (IncomingDataCorruption &e) {
                        m_connection->putCommand(ConnectionCommand::disconnect_peer(peer->id));

                        LOG(derr_con << m_connection->getDesc()
                                << "INVALID, TYPE_RELIABLE peer_id: " << peer->id
                                << ", channel: " << (channelnum & 0xFF)
                                << ", seqnum: " << seqnum
                                << "DROPPING CLIENT!" << std::endl;)
                }
        }

        /* we got a packet to process right now */
        LOG(dout_con << m_connection->getDesc()
                << "RECURSIVE, TYPE_RELIABLE peer_id: " << peer->id
                << ", channel: " << (channelnum & 0xFF)
                << ", seqnum: " << seqnum << std::endl;)


        /* check for resend case */
        u16 queued_seqnum = 0;
        if (channel->incoming_reliables.getFirstSeqnum(queued_seqnum)) {
                if (queued_seqnum == seqnum) {
                        BufferedPacketPtr queued_packet = channel->incoming_reliables.popFirst();
                        /** TODO find a way to verify the new against the old packet */
                }
        }

        channel->incNextIncomingSeqNum();

        // Get out the inside packet and re-process it
        SharedBuffer<u8> payload(packetdata.getSize() - RELIABLE_HEADER_SIZE);
        memcpy(*payload, &packetdata[RELIABLE_HEADER_SIZE], payload.getSize());

        return processPacket(channel, payload, peer->id, channelnum, true);
}

}
