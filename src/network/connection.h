// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "network/crypto.h"
#include "networkprotocol.h" // session_t
#include "socket.h" // Address

class NetworkPacket;
class PeerHandler;

namespace con
{

enum rtt_stat_type : int {
        MIN_RTT,
        MAX_RTT,
        AVG_RTT,
        MIN_JITTER,
        MAX_JITTER,
        AVG_JITTER
};

enum rate_stat_type : int {
        CUR_DL_RATE,
        AVG_DL_RATE,
        CUR_INC_RATE,
        AVG_INC_RATE,
        CUR_LOSS_RATE,
        AVG_LOSS_RATE,
};

class IPeer {
public:
        // Unique id of the peer
        const session_t id;

        virtual const Address &getAddress() const = 0;

protected:
        IPeer(session_t id) : id(id) {}
        ~IPeer() {}
};

class IConnection
{
public:
        virtual ~IConnection() = default;

        virtual void Serve(Address bind_addr) = 0;
        virtual void Connect(Address address) = 0;
        virtual bool Connected() = 0;
        virtual void Disconnect() = 0;
        virtual void DisconnectPeer(session_t peer_id) = 0;

        virtual bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms) = 0;
        bool TryReceive(NetworkPacket *pkt) {
                return ReceiveTimeoutMs(pkt, 0);
        }

        virtual void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable) = 0;

        virtual session_t GetPeerID() const = 0;
        virtual Address GetPeerAddress(session_t peer_id) = 0;
        virtual float getPeerStat(session_t peer_id, rtt_stat_type type) = 0;
        virtual float getLocalStat(rate_stat_type type) = 0;

        // v9: Push encryption state to the peer in the connection layer.
        // Called by Client/Server after SRP auth succeeds, to activate
        // packet encryption for this peer.
        virtual void SetPeerEncryptionState(session_t peer_id, const PeerEncryptionState &state) = 0;

        // v9: Activate encryption for a peer after ensuring that all
        // previously queued packets have been sent as plaintext.
        // This queues a command that the send thread processes AFTER
        // sending any pending packets. This avoids the chicken-and-egg
        // problem where AUTH_ACCEPT would be encrypted before the
        // client can decrypt it.
        virtual void ActivatePeerEncryption(session_t peer_id) = 0;

        // v9.19: Update ONLY the ECDH keypair fields on the peer's
        // encryption state in the connection layer, WITHOUT overwriting
        // the SRP-derived keys, nonce bases, session_id, etc.
        // This prevents a full SetPeerEncryptionState from clobbering
        // good keys when the source state has been reset.
        virtual void UpdatePeerECDHKeypair(session_t peer_id,
                const std::array<u8, X25519_PRIVATE_KEY_SIZE> &ecdh_private_key,
                const std::array<u8, X25519_PUBLIC_KEY_SIZE> &ecdh_public_key) = 0;

        // v9.19: Mix the ECDH shared secret into the peer's encryption keys
        // DIRECTLY on the connection layer's encryption state (udpPeer).
        // This ensures the mixing happens on the CORRECT SRP-derived keys,
        // not on a potentially-reset client->encryption_state object.
        // Returns true if mixing succeeded, false on failure.
        virtual bool MixECDHSecretOnPeer(session_t peer_id,
                const u8 *ecdh_shared_secret, size_t shared_secret_len) = 0;
};

// MTP = Minetest Protocol
IConnection *createMTP(float timeout, bool ipv6, PeerHandler *handler);

} // namespace
