// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

// This file contains encryption-related packet handlers for Client.
// Split from clientpackethandler.cpp for maintainability.

#include "client/client.h"

#include "exceptions.h"
#include "irr_v2d.h"
#include "util/base64.h"
#include "client/camera.h"
#include "client/mesh_generator_thread.h"
#include "chatmessage.h"
#include "client/clientmedia.h"
#include "log.h"
#include "servermap.h"
#include "mapsector.h"
#include "client/minimap.h"
#include "itemdef.h"
#include "modchannels.h"
#include "nodedef.h"
#include "serialization.h"
#include "util/strfnd.h"
#include "util/numeric.h"
#include "client/clientevent.h"
#include "client/sound.h"
#include "client/localplayer.h"
#include "network/clientopcodes.h"
#include "network/connection.h"
#include "network/connection_security.h"
#include "network/crypto.h"
#include "network/encryption_config.h"
#include "network/encryption_log.h"
#include "network/networkpacket.h"
#include "settings.h"
#include "script/scripting_client.h"
#include "util/serialize.h"
#include "util/srp.h"
#include "util/hashing.h"
#include "porting.h"
#include "tileanimation.h"
#include "gettext.h"
#include "skyparams.h"
#include "particles.h"
#include <memory>
#include <sstream>
#include <ctime>

// v9.11: ECDH X25519 forward secrecy — receive server's public key
void Client::handleCommand_EcdhPubkey(NetworkPacket *pkt)
{
	// Read server's X25519 public key (32 bytes)
	std::string pubkey_str = pkt->readLongString();

	if (pubkey_str.size() != X25519_PUBLIC_KEY_SIZE) {
		errorstream << "Client::handleCommand_EcdhPubkey: invalid public key size ("
			<< pubkey_str.size() << ", expected " << X25519_PUBLIC_KEY_SIZE << ")"
			<< std::endl;
		return;
	}
	const u8* server_pubkey = reinterpret_cast<const u8*>(pubkey_str.data());

	if (!EncryptionConfig::shouldEncrypt() || m_internal_server) {
		// v9.13: Also skip in singleplayer (localhost doesn't need encryption)
		infostream << "Client::handleCommand_EcdhPubkey: ignoring, encryption disabled"
			<< (m_internal_server ? " (singleplayer)" : "")
			<< std::endl;
		return;
	}

	infostream << "Client::handleCommand_EcdhPubkey: received server ECDH public key" << std::endl;

	// Generate our own X25519 key pair
	X25519KeyPair client_kp = x25519_generate_keypair();
	if (!client_kp.success) {
		errorstream << "Client::handleCommand_EcdhPubkey: failed to generate X25519 key pair" << std::endl;
		return;
	}

	// Compute the ECDH shared secret
	X25519SharedSecret shared_secret = x25519_compute_shared_secret(
		client_kp.private_key.data(), client_kp.private_key.size(),
		server_pubkey, X25519_PUBLIC_KEY_SIZE);
	if (!shared_secret.success) {
		errorstream << "Client::handleCommand_EcdhPubkey: failed to compute ECDH shared secret" << std::endl;
		return;
	}

	// Store ECDH key pair on the encryption state
	m_encryption_state.ecdh_private_key = client_kp.private_key;
	m_encryption_state.ecdh_public_key = client_kp.public_key;

	// Mix the ECDH shared secret into the encryption keys
	bool ok = mixECDHSecretIntoKeys(m_encryption_state,
		shared_secret.shared_secret.data(), shared_secret.shared_secret.size());
	if (!ok) {
		errorstream << "Client::handleCommand_EcdhPubkey: mixECDHSecretIntoKeys failed" << std::endl;
		return;
	}

	// Update the connection with the new ECDH-mixed keys
	m_con->SetPeerEncryptionState(PEER_ID_SERVER, m_encryption_state);

	// Send our public key to the server
	NetworkPacket ecdh_pkt(TOSERVER_ECDH_PUBKEY, X25519_PUBLIC_KEY_SIZE);
	std::string client_pubkey_str(reinterpret_cast<const char*>(client_kp.public_key.data()),
		X25519_PUBLIC_KEY_SIZE);
	ecdh_pkt.putLongString(client_pubkey_str);
	Send(&ecdh_pkt);

	// v9.12: Do NOT activate encryption here. Defer activation until the
	// server sends its first encrypted packet. This avoids a race condition
	// where the client activates encryption immediately after sending
	// TOSERVER_ECDH_PUBKEY, but the server hasn't activated yet — causing
	// the client to send encrypted packets the server can't read, and the
	// server to send plaintext packets the client logs as errors.
	//
	// Instead, the receive path (ConnectionReceiveThread::receive) will
	// auto-activate encryption when it successfully decrypts the first
	// encrypted packet from the server. This guarantees symmetric
	// activation: both sides encrypt from the same point onward.
	//
	// m_encryption_state.activated_at will be set by the auto-activation
	// in the receive path when encryption actually activates.

	// Populate HONEST security info with ECDH forward secrecy
	Address remote = m_con->GetPeerAddress(PEER_ID_SERVER);
	m_security_info = populateRealSecurityInfo(
		true /* encryption_active */,
		m_encryption_state.ecdh_completed.load() /* ecdh_completed */,
		false /* fingerprint_pinned */,
		0 /* fingerprint_verify_result */,
		m_encryption_state.session_id,
		m_encryption_state.server_fingerprint,
		m_encryption_state.activated_at,
		m_proto_ver,
		m_address_name,
		remote.getPort(),
		PeerEncryptionState::KEY_ROTATION_SUPPORTED /* key_rotation_supported */);

	infostream << "Client::handleCommand_EcdhPubkey: ECDH forward secrecy established"
		<< " (session_id=" << m_encryption_state.session_id
		<< ", score=" << m_security_info.getSecurityScoreString() << ")" << std::endl;
}
