// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

/*
 * Server packet handler — Encryption methods
 *
 * handleCommand_EcdhPubkey
 */

#include "server.h"
#include "serverenvironment.h"
#include "log.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "network/encryption_config.h"
#include "network/connection_security.h"
#include "network/crypto.h"
#include "network/encryption_log.h"

// v9.11: ECDH X25519 forward secrecy — receive client's public key
void Server::handleCommand_EcdhPubkey(NetworkPacket *pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);

	// Read client's X25519 public key (32 bytes)
	std::string pubkey_str = pkt->readLongString();

	if (pubkey_str.size() != X25519_PUBLIC_KEY_SIZE) {
		errorstream << "Server::handleCommand_EcdhPubkey: invalid public key size from peer "
			<< peer_id << " (" << pubkey_str.size()
			<< ", expected " << X25519_PUBLIC_KEY_SIZE << ")" << std::endl;
		return;
	}
	const u8* client_pubkey = reinterpret_cast<const u8*>(pubkey_str.data());

	if (!EncryptionConfig::shouldEncrypt() || isSingleplayer()) {
		// v9.13: Also skip in singleplayer (localhost doesn't need encryption)
		infostream << "Server::handleCommand_EcdhPubkey: ignoring, encryption disabled"
			<< (isSingleplayer() ? " (singleplayer)" : "")
			<< std::endl;
		return;
	}

	// Check that we have a stored ECDH key pair (generated during SRP auth)
	bool our_keypair_valid = false;
	for (size_t i = 0; i < X25519_PRIVATE_KEY_SIZE; i++) {
		if (client->encryption_state.ecdh_private_key[i] != 0) {
			our_keypair_valid = true;
			break;
		}
	}
	if (!our_keypair_valid) {
		errorstream << "Server::handleCommand_EcdhPubkey: no stored ECDH key pair for peer "
			<< peer_id << std::endl;
		return;
	}

	infostream << "Server::handleCommand_EcdhPubkey: received client ECDH public key from peer "
		<< peer_id << std::endl;

	// Compute the ECDH shared secret using our stored private key and client's public key
	X25519SharedSecret shared_secret = x25519_compute_shared_secret(
		client->encryption_state.ecdh_private_key.data(),
		client->encryption_state.ecdh_private_key.size(),
		client_pubkey, X25519_PUBLIC_KEY_SIZE);
	if (!shared_secret.success) {
		errorstream << "Server::handleCommand_EcdhPubkey: failed to compute ECDH shared secret for peer "
			<< peer_id << std::endl;
		return;
	}

	// Mix the ECDH shared secret into the encryption keys
	bool ok = mixECDHSecretIntoKeys(client->encryption_state,
		shared_secret.shared_secret.data(), shared_secret.shared_secret.size());
	if (!ok) {
		errorstream << "Server::handleCommand_EcdhPubkey: mixECDHSecretIntoKeys failed for peer "
			<< peer_id << std::endl;
		return;
	}

	// Update the connection with the new ECDH-mixed keys
	m_con->SetPeerEncryptionState(peer_id, client->encryption_state);

	// Activate encryption with ECDH+SRP keys
	m_con->ActivatePeerEncryption(peer_id);

	infostream << "Server::handleCommand_EcdhPubkey: ECDH forward secrecy established for peer "
		<< peer_id << " (session_id=" << client->encryption_state.session_id << ")" << std::endl;
}
