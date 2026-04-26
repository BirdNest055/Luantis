// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

/*
 * Server packet handler — Authentication methods
 *
 * handleCommand_Init, handleCommand_Init2, handleCommand_ClientReady,
 * handleCommand_FirstSrp, handleCommand_SrpBytesA, handleCommand_SrpBytesM
 */

#include "chatmessage.h"
#include "server.h"
#include "serverenvironment.h"
#include "log.h"
#include "porting.h" // strcasecmp
#include "remoteplayer.h"
#include "scripting_server.h"
#include "serialization.h"
#include "settings.h"
#include "network/connection.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "network/encryption_config.h"
#include "server/player_sao.h"
#include "util/auth.h"
#include "util/base64.h"
#include "util/srp.h"
#include "network/connection_security.h"
#include "network/crypto.h"
#include "network/encryption_log.h"
#include "clientdynamicinfo.h"

#include <algorithm>

void Server::handleCommand_Init(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Created);

	Address addr;
	std::string addr_s;
	try {
		addr = m_con->GetPeerAddress(peer_id);
		addr_s = addr.serializeString();
	} catch (con::PeerNotFoundException &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server: peer " << peer_id << " not found during INIT?!"
			<< std::endl;
		return;
	}

	if (client->getState() > CS_Created) {
		verbosestream << "Server: Ignoring multiple TOSERVER_INITs from " <<
			addr_s << " (peer_id=" << peer_id << ")" << std::endl;
		return;
	}

	client->setCachedAddress(addr);

	verbosestream << "Server: Got TOSERVER_INIT from " << addr_s <<
		" (peer_id=" << peer_id << ")" << std::endl;

	if (denyIfBanned(peer_id))
		return;

	u8 max_ser_ver; // SER_FMT_VER_HIGHEST_READ (of client)
	u16 unused;
	u16 min_net_proto_version;
	u16 max_net_proto_version;
	std::string playerName;

	*pkt >> max_ser_ver >> unused
			>> min_net_proto_version >> max_net_proto_version
			>> playerName;

	// Use the highest version supported by both
	const u8 serialization_ver = std::min(max_ser_ver, SER_FMT_VER_HIGHEST_WRITE);

	if (!ser_ver_supported_write(serialization_ver)) {
		actionstream << "Server: A mismatched client tried to connect from " <<
			addr_s << " ser_fmt_max=" << (int)serialization_ver << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	client->setPendingSerializationVersion(serialization_ver);

	/*
		Read and check network protocol version
	*/

	u16 net_proto_version = 0;

	// Figure out a working version if it is possible at all
	if (max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
			min_net_proto_version <= LATEST_PROTOCOL_VERSION) {
		// If maximum is larger than our maximum, go with our maximum
		if (max_net_proto_version > LATEST_PROTOCOL_VERSION)
			net_proto_version = LATEST_PROTOCOL_VERSION;
		// Else go with client's maximum
		else
			net_proto_version = max_net_proto_version;
	}

	verbosestream << "Server: " << addr_s << ": Protocol version: min: "
			<< min_net_proto_version << ", max: " << max_net_proto_version
			<< ", chosen: " << net_proto_version << std::endl;

	client->net_proto_version = net_proto_version;

	if (net_proto_version < Server::getProtocolVersionMin() ||
			net_proto_version > Server::getProtocolVersionMax()) {
		actionstream << "Server: A mismatched client tried to connect from " <<
			addr_s << " proto_max=" << (int)max_net_proto_version << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	/*
		Validate player name
	*/
	const char* playername = playerName.c_str();

	size_t pns = playerName.size();
	if (pns == 0 || pns > PLAYERNAME_SIZE) {
		actionstream << "Server: Player with " <<
			((pns > PLAYERNAME_SIZE) ? "a too long" : "an empty") <<
			" name tried to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	if (!string_allowed(playerName, PLAYERNAME_ALLOWED_CHARS)) {
		actionstream << "Server: Player with an invalid name tried to connect "
			"from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_CHARS_IN_NAME);
		return;
	}

	// Do not allow multiple players in simple singleplayer mode
	if (isSingleplayer() && !m_clients.getClientIDs(CS_HelloSent).empty()) {
		infostream << "Server: Not allowing another client (" << addr_s <<
			") to connect in simple singleplayer mode" << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SINGLEPLAYER);
		return;
	}
	// Or the "singleplayer" name to be used on regular servers
	if (!isSingleplayer() && strcasecmp(playername, "singleplayer") == 0) {
		actionstream << "Server: Player with the name \"singleplayer\" tried "
			"to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	{
		RemotePlayer *player = m_env->getPlayer(playername, true);
		// If player is already connected, cancel
		if (player && player->getPeerId() != PEER_ID_INEXISTENT) {
			actionstream << "Server: Player with name \"" << playername <<
				"\" tried to connect, but player with same name is already connected" << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			return;
		}
	}

	client->setName(playerName);

	{
		std::string reason;
		if (m_script->on_prejoinplayer(playername, addr_s, &reason)) {
			actionstream << "Server: Player with the name \"" << playerName <<
				"\" tried to connect from " << addr_s <<
				" but was disallowed for the following reason: " << reason <<
				std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING, reason);
			return;
		}
	}

	infostream << "Server: New connection: \"" << playerName << "\" from " <<
		addr_s << " (peer_id=" << peer_id << ")" << std::endl;

	// Early check for user limit, so the client doesn't need to run
	// through the join process only to be denied.
	if (checkUserLimit(playerName, addr_s)) {
		actionstream << "Server: " << playername << " tried to join from " <<
			addr_s << ", but the user limit was reached." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	/*
		Compose auth methods for answer
	*/
	std::string encpwd; // encrypted Password field for the user
	bool has_auth = m_script->getAuth(playername, &encpwd, nullptr);
	u32 auth_mechs = 0;

	client->chosen_mech = AUTH_MECHANISM_NONE;

	if (has_auth) {
		std::vector<std::string> pwd_components = str_split(encpwd, '#');
		if (pwd_components.size() == 4) {
			if (pwd_components[1] == "1") { // 1 means srp
				auth_mechs |= AUTH_MECHANISM_SRP;
				client->enc_pwd = encpwd;
			} else {
				actionstream << "User " << playername << " tried to log in, "
					"but password field was invalid (unknown mechcode)." <<
					std::endl;
				DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
				return;
			}
		} else if (base64_is_valid(encpwd)) {
			auth_mechs |= AUTH_MECHANISM_LEGACY_PASSWORD;
			client->enc_pwd = encpwd;
		} else {
			actionstream << "User " << playername << " tried to log in, but "
				"password field was invalid (invalid base64)." << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
	} else {
		std::string default_password = g_settings->get("default_password");
		if (isSingleplayer() || default_password.length() == 0) {
			auth_mechs |= AUTH_MECHANISM_FIRST_SRP;
		} else {
			// Take care of default passwords.
			client->enc_pwd = get_encoded_srp_verifier(playerName, default_password);
			auth_mechs |= AUTH_MECHANISM_SRP;
			// Allocate player in db, but only on successful login.
			client->create_player_on_auth_success = true;
		}
	}

	/*
		Answer with a TOCLIENT_HELLO
	*/

	verbosestream << "Sending TOCLIENT_HELLO with auth method field: "
		<< auth_mechs << std::endl;

	NetworkPacket resp_pkt(TOCLIENT_HELLO, 0, peer_id);

	// v9.3: Use modular encryption config for security flags.
	// All encryption policy decisions go through EncryptionConfig.
	// When secure_connection = false, no flags are advertised.
	// When secure_connection = true, ENCRYPTION_SUPPORTED and AUTHENTICATED
	// are set. The actual ENCRYPTED flag is set after SRP auth succeeds.
	u8 security_flags = EncryptionConfig::getSecurityFlags();

	resp_pkt << serialization_ver << u16(0) /* unused */
		<< net_proto_version
		<< auth_mechs << std::string_view() /* unused */
		<< security_flags;

	Send(&resp_pkt);

	client->allowed_auth_mechs = auth_mechs;

	m_clients.event(peer_id, CSE_Hello);
}

void Server::handleCommand_Init2(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	verbosestream << "Server: Got TOSERVER_INIT2 from " << peer_id << std::endl;

	RemoteClient *client = getClientNoEx(peer_id, CS_Invalid);
	if (!client || client->getState() != CS_AwaitingInit2) {
		actionstream << "Server: Ignoring TOSERVER_INIT2 in wrong state from "
			<< peer_id << std::endl;
		return;
	}

	m_clients.event(peer_id, CSE_GotInit2);
	u16 protocol_version = m_clients.getProtocolVersion(peer_id);

	std::string lang;
	if (pkt->getSize() > 0)
		*pkt >> lang;

	/*
		Send some initialization data
	*/

	infostream << "Server: Sending content to " << getPlayerName(peer_id) <<
		std::endl;

	// Send item definitions
	SendItemDef(peer_id, m_itemdef, protocol_version);

	// Send node definitions
	SendNodeDef(peer_id, m_nodedef, protocol_version);

	m_clients.event(peer_id, CSE_SetDefinitionsSent);

	// Send media announcement
	sendMediaAnnouncement(peer_id, lang);

	// Keep client language for server translations
	client->setLangCode(lang);

	// Send active objects
	{
		PlayerSAO *sao = getPlayerSAO(peer_id);
		if (sao)
			SendActiveObjectRemoveAdd(client, sao);
	}

	// Send detached inventories
	sendDetachedInventories(peer_id, false);

	// Send player movement settings
	SendMovement(peer_id);

	// Send time of day
	u16 time = m_env->getTimeOfDay();
	float time_speed = g_settings->getFloat("time_speed");
	SendTimeOfDay(peer_id, time, time_speed);

	SendCSMRestrictionFlags(peer_id);
}

void Server::handleCommand_ClientReady(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Created);
	assert(client);

	// decode all information first
	u8 major_ver, minor_ver, patch_ver, reserved;
	u16 formspec_ver = 1; // v1 for clients older than 5.1.0-dev
	std::string full_ver;

	*pkt >> major_ver >> minor_ver >> patch_ver >> reserved >> full_ver;
	if (pkt->getRemainingBytes() >= 2)
		*pkt >> formspec_ver;

	client->setVersionInfo(major_ver, minor_ver, patch_ver, full_ver);

	// Since only active clients count for the user limit, two could race the
	// join process so we have to do a final check for the user limit here.
	std::string addr_s = client->getAddress().serializeString();
	if (checkUserLimit(client->getName(), addr_s)) {
		actionstream << "Server: " << client->getName() << " tried to join from " <<
			addr_s << ", but the user limit was reached (late)." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	// Emerge player
	PlayerSAO* playersao = StageTwoClientInit(peer_id);
	if (!playersao) {
		errorstream << "Server: stage 2 client init failed "
			"peer_id=" << peer_id << std::endl;
		DisconnectPeer(peer_id);
		return;
	}

	playersao->getPlayer()->formspec_version = formspec_ver;
	m_clients.event(peer_id, CSE_SetClientReady);

	// Send player list to this client
	{
		const std::vector<std::string> &players = m_clients.getPlayerNames();
		NetworkPacket list_pkt(TOCLIENT_UPDATE_PLAYER_LIST, 0, peer_id);
		list_pkt << (u8) PLAYER_LIST_INIT << (u16) players.size();
		for (const auto &player : players)
			list_pkt << player;
		Send(peer_id, &list_pkt);
	}

	s64 last_login;
	m_script->getAuth(playersao->getPlayer()->getName(), nullptr, nullptr, &last_login);
	m_script->on_joinplayer(playersao, last_login);

	// Send shutdown timer if shutdown has been scheduled
	if (m_shutdown_state.isTimerRunning())
		SendChatMessage(peer_id, m_shutdown_state.getShutdownTimerMessage());
}

void Server::handleCommand_FirstSrp(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();
	const std::string playername = client->getName();

	std::string salt, verification_key;

	std::string addr_s = client->getAddress().serializeString();
	u8 is_empty;

	*pkt >> salt >> verification_key >> is_empty;

	verbosestream << "Server: Got TOSERVER_FIRST_SRP from " << addr_s
		<< ", with is_empty=" << (is_empty == 1) << std::endl;

	const bool empty_disallowed = !isSingleplayer() && is_empty == 1 &&
		g_settings->getBool("disallow_empty_password");

	// Either this packet is sent because the user is new or to change the password
	if (cstate == CS_HelloSent) {
		if (!client->isMechAllowed(AUTH_MECHANISM_FIRST_SRP)) {
			actionstream << "Server: Client from " << addr_s
					<< " tried to set password without being "
					<< "authenticated, or the username being new." << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}

		if (empty_disallowed) {
			actionstream << "Server: " << playername
					<< " supplied empty password from " << addr_s << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_EMPTY_PASSWORD);
			return;
		}

		std::string encpwd = encode_srp_verifier(verification_key, salt);

		// It is possible for multiple connections to get this far with the same
		// player name. In the end only one player with a given name will be emerged
		// (see Server::StateTwoClientInit) but we still have to be careful here.
		if (m_script->getAuth(playername, nullptr, nullptr)) {
			// Another client beat us to it
			actionstream << "Server: Client from " << addr_s
				<< " tried to register " << playername << " a second time."
				<< std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			return;
		}

		m_script->createAuth(playername, encpwd);
		client->setEncryptedPassword(encpwd);

		m_script->on_authplayer(playername, addr_s, true);

		// v9 fix: Instead of calling acceptAuth() immediately, we now
		// perform a full SRP key exchange so both client and server
		// derive the same session key for encryption. The client sends
		// SRP_BYTES_A after the FIRST_SRP packet for this purpose.
		// We store the fact that FIRST_SRP registration is done but
		// the SRP exchange is still pending. When we receive the
		// client's SRP_BYTES_A, handleCommand_SrpBytesA will handle
		// the exchange and call acceptAuth() after key derivation.
		//
		// The client now ALWAYS sends SRP_BYTES_A after FIRST_SRP,
		// even for empty passwords. Both sides derive a session key
		// through the SRP exchange, enabling encryption for all
		// connections regardless of password status.
		// Wait for SRP_BYTES_A from the client to complete
		// the key exchange. handleCommand_SrpBytesA will call acceptAuth().
	} else {
		if (cstate < CS_SudoMode) {
			infostream << "Server: Ignoring TOSERVER_FIRST_SRP from "
					<< addr_s << ": " << "Client has wrong state " << cstate << "."
					<< std::endl;
			return;
		}
		m_clients.event(peer_id, CSE_SudoLeave);

		if (empty_disallowed) {
			actionstream << "Server: " << playername
					<< " supplied empty password" << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				L"Changing to an empty password is not allowed."));
			return;
		}

		std::string encpwd = encode_srp_verifier(verification_key, salt);
		bool success = m_script->setPassword(playername, encpwd);
		if (success) {
			actionstream << playername << " changes password" << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				L"Password change successful."));
			client->setEncryptedPassword(encpwd);
		} else {
			actionstream << playername <<
				" tries to change password but it fails" << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				L"Password change failed or unavailable."));
		}
	}
}

void Server::handleCommand_SrpBytesA(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();

	std::string addr_s = client->getAddress().serializeString();

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		actionstream << "Server: got SRP _A packet in wrong state " << cstate <<
			" from " << addr_s <<
			". Ignoring." << std::endl;
		return;
	}

	const bool wantSudo = (cstate == CS_Active);

	if (client->chosen_mech != AUTH_MECHANISM_NONE) {
		actionstream << "Server: got SRP _A packet, while auth is already "
			"going on with mech " << client->chosen_mech << " from " <<
			addr_s <<
			" (wantSudo=" << wantSudo << "). Ignoring." << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	std::string bytes_A;
	u8 based_on;
	*pkt >> bytes_A >> based_on;

	infostream << "Server: TOSERVER_SRP_BYTES_A received with "
		<< "based_on=" << int(based_on) << " and len_A="
		<< bytes_A.length() << std::endl;

	AuthMechanism chosen = (based_on == 0) ?
		AUTH_MECHANISM_LEGACY_PASSWORD : AUTH_MECHANISM_SRP;

	// v9 fix: If the client just completed FIRST_SRP (account registration)
	// and is now sending SRP_BYTES_A for the key exchange, allow it even
	// though FIRST_SRP wasn't set as chosen_mech. The FIRST_SRP handler
	// leaves chosen_mech as NONE to signal that the SRP exchange is pending.
	bool is_first_srp_key_exchange = !wantSudo &&
		client->enc_pwd.size() > 0 &&
		client->chosen_mech == AUTH_MECHANISM_NONE &&
		cstate == CS_HelloSent &&
		client->isMechAllowed(AUTH_MECHANISM_FIRST_SRP);

	if (wantSudo) {
		// Right now, the auth mechs don't change between login and sudo mode.
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Player \"" << client->getName() <<
				"\" from " << addr_s <<
				" tried to change password using unallowed mech " << chosen <<
				std::endl;
			DenySudoAccess(peer_id);
			return;
		}
	} else if (is_first_srp_key_exchange) {
		// v9 fix: Allow SRP_BYTES_A after FIRST_SRP registration
		// so we can derive a session key for encryption.
		chosen = AUTH_MECHANISM_SRP;
	} else {
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Client tried to authenticate from " <<
				addr_s <<
				" using unallowed mech " << chosen << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	client->chosen_mech = chosen;

	std::string salt, verifier;

	if (based_on == 0) {

		generate_srp_verifier_and_salt(client->getName(), client->enc_pwd,
			&verifier, &salt);
	} else if (!decode_srp_verifier_and_salt(client->enc_pwd, &verifier, &salt)) {
		// Non-base64 errors should have been catched in the init handler
		actionstream << "Server: User " << client->getName() <<
			" tried to log in, but srp verifier field was invalid (most likely "
			"invalid base64)." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		return;
	}

	char *bytes_B = 0;
	size_t len_B = 0;

	client->auth_data = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		client->getName().c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		(const unsigned char *) bytes_A.c_str(), bytes_A.size(),
		NULL, 0,
		(unsigned char **) &bytes_B, &len_B, NULL, NULL);

	// v9.1 fix: Check that srp_verifier_new() succeeded.
	// If it returns NULL (memory allocation failure) but bytes_B is also NULL,
	// the existing !bytes_B check below will handle it. But if verifier_new
	// returns NULL with bytes_B also NULL, we must also check auth_data.
	if (!client->auth_data && !bytes_B) {
		actionstream << "Server: User " << client->getName()
			<< " srp_verifier_new() failed (memory allocation error)."
			<< std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		return;
	}

	if (!bytes_B) {
		actionstream << "Server: User " << client->getName()
			<< " tried to log in, SRP-6a safety check violated in _A handler."
			<< std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	NetworkPacket resp_pkt(TOCLIENT_SRP_BYTES_S_B, 0, peer_id);
	resp_pkt << salt << std::string(bytes_B, len_B);
	Send(&resp_pkt);
}

void Server::handleCommand_SrpBytesM(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();
	const std::string addr_s = client->getAddress().serializeString();
	const std::string playername = client->getName();

	const bool wantSudo = (cstate == CS_Active);

	verbosestream << "Server: Received TOSERVER_SRP_BYTES_M." << std::endl;

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		warningstream << "Server: got SRP_M packet in wrong state "
			<< cstate << " from " << addr_s << ". Ignoring." << std::endl;
		return;
	}

	if (client->chosen_mech != AUTH_MECHANISM_SRP &&
			client->chosen_mech != AUTH_MECHANISM_LEGACY_PASSWORD) {
		warningstream << "Server: got SRP_M packet, while auth "
			"is going on with mech " << client->chosen_mech << " from "
			<< addr_s << " (wantSudo=" << wantSudo << "). Denying." << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	std::string bytes_M;
	*pkt >> bytes_M;

	if (srp_verifier_get_session_key_length((SRPVerifier *) client->auth_data)
			!= bytes_M.size()) {
		actionstream << "Server: User " << playername << " at " << addr_s
			<< " sent bytes_M with invalid length " << bytes_M.size() << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	unsigned char *bytes_HAMK = 0;

	srp_verifier_verify_session((SRPVerifier *) client->auth_data,
		(unsigned char *)bytes_M.c_str(), &bytes_HAMK);

	// skip authentication check for singleplayer world.
	const bool is_true_singleplayer = isSingleplayer() && (strcasecmp(playername.c_str(), "singleplayer") == 0);
	if (!bytes_HAMK && !is_true_singleplayer) {
		if (wantSudo) {
			actionstream << "Server: User " << playername << " at " << addr_s
				<< " tried to change their password, but supplied wrong"
				<< " (SRP) password for authentication." << std::endl;
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		actionstream << "Server: User " << playername << " at " << addr_s
			<< " supplied wrong password (auth mechanism: SRP)." << std::endl;
		m_script->on_authplayer(playername, addr_s, false);
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_PASSWORD);
		return;
	}

	if (client->create_player_on_auth_success) {
		m_script->createAuth(playername, client->enc_pwd);

		if (!m_script->getAuth(playername, nullptr, nullptr)) {
			errorstream << "Server: " << playername <<
				" cannot be authenticated (auth handler does not work?)" <<
				std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
		client->create_player_on_auth_success = false;
	}

	m_script->on_authplayer(playername, addr_s, true);

	// v9: Extract SRP session key BEFORE acceptAuth() calls resetChosenMech()
	// which deletes the SRPVerifier (and zeroes the session key).
	// Both server and client must capture the key at this point.
	//
	// CRITICAL: We must derive the keys and push them to the connection layer
	// BEFORE calling acceptAuth(), but we must NOT activate encryption yet.
	// acceptAuth() sends AUTH_ACCEPT, which MUST be sent as plaintext so the
	// client can receive it and activate its own encryption. We activate
	// encryption AFTER acceptAuth() via ActivatePeerEncryption(), which
	// queues the activation to happen after all pending packets are sent.
	bool encryption_initialized = false;
	enclog_init("SRP auth succeeded, checking session key for encryption")
		<< EncLog::kv("peer", peer_id)
		<< EncLog::kv("auth_data_present", client->auth_data ? "yes" : "no")
		<< EncLog::kv("chosen_mech", (int)client->chosen_mech)
		<< std::endl;
	if (client->auth_data) {
		SRPVerifier *ver = (SRPVerifier *) client->auth_data;
		size_t key_len = 0;
		const unsigned char *session_key = srp_verifier_get_session_key(ver, &key_len);
		enclog_init("SRP session key extracted")
			<< EncLog::kv("peer", peer_id)
			<< EncLog::kv("key_len", (u32)key_len)
			<< EncLog::kv("expected", (u32)SRP_SESSION_KEY_SIZE)
			<< EncLog::kv("key_present", session_key ? "yes" : "no")
			<< std::endl;

		// v9.3: Use modular encryption config for encryption policy.
		// See encryption_config.h for the centralized toggle.
		//
		// v9.13: Singleplayer connections skip encryption entirely.
		// There is no network risk on localhost — encryption adds
		// unnecessary latency and the transition period causes
		// "Plaintext packet received while encryption active" errors.

		if (!EncryptionConfig::shouldEncrypt() || isSingleplayer()) {
			// Encryption disabled or singleplayer: SRP still runs for
			// password authentication, but the session key is NOT used
			// to activate AES-256-GCM encryption.
			if (isSingleplayer()) {
				enclog_init("Singleplayer: skipping encryption (local connection)")
					<< EncLog::kv("peer", peer_id)
					<< EncLog::kv("mode", "singleplayer")
					<< EncLog::kv("reason", "localhost_does_not_need_encryption")
					<< std::endl;
			} else {
				EncryptionConfig::logEncryptionDecision(peer_id, true, false);
			}
			client->encryption_state.disable();
		} else if (session_key && key_len == SRP_SESSION_KEY_SIZE) {
			bool ok = client->encryption_state.initFromSRPSessionKey(
				session_key, key_len, true /* is_server=true */);
			if (ok) {
				// Push encryption state (with active=false) to the connection layer.
				// Keys are loaded but encryption is NOT active yet.
				m_con->SetPeerEncryptionState(peer_id, client->encryption_state);
				encryption_initialized = true;
				enclog_init("Encryption keys derived for peer (not yet active)")
					<< EncLog::kv("peer", peer_id)
					<< EncLog::kv("session_id", client->encryption_state.session_id)
					<< EncLog::kv("fingerprint", client->encryption_state.server_fingerprint)
					<< EncLog::kv("role", "server")
					<< EncLog::kv("active", false)
					<< std::endl;
			} else {
				enclog_error("initFromSRPSessionKey FAILED for peer")
					<< EncLog::kv("peer", peer_id)
					<< EncLog::kv("reason", "HKDF key derivation error")
					<< std::endl;
				client->encryption_state.disable();
			}
		} else {
			enclog_error("SRP session key unavailable or wrong size for peer")
				<< EncLog::kv("peer", peer_id)
				<< EncLog::kv("key_len", (u32)key_len)
				<< EncLog::kv("expected", (u32)SRP_SESSION_KEY_SIZE)
				<< EncLog::kv("key_present", session_key ? "yes" : "no")
				<< std::endl;
		}
	} else {
		enclog_error("auth_data is NULL for peer, encryption not possible")
			<< EncLog::kv("peer", peer_id)
			<< std::endl;
	}

	// Send AUTH_ACCEPT as the LAST plaintext packet from server to client.
	// The client needs this packet to be unencrypted so it can derive
	// its own encryption keys from the SRP session key.
	acceptAuth(peer_id, wantSudo);

	// v9.11: ECDH forward secrecy — if encryption is initialized,
	// generate an X25519 key pair and send the public key to the client
	// BEFORE activating encryption. The client will respond with its
	// public key, and the server activates encryption in handleCommand_EcdhPubkey.
	//
	// This ensures that both sides derive ECDH+SRP combined keys before
	// encryption starts, providing REAL forward secrecy from the first
	// encrypted packet.
	if (encryption_initialized) {
		X25519KeyPair server_kp = x25519_generate_keypair();
		if (server_kp.success) {
			// Store the ECDH key pair in the encryption state
			client->encryption_state.ecdh_private_key = server_kp.private_key;
			client->encryption_state.ecdh_public_key = server_kp.public_key;

			// Update the connection state with the stored keypair
			m_con->SetPeerEncryptionState(peer_id, client->encryption_state);

			// Send the ECDH public key to the client (plaintext, before activation)
			NetworkPacket ecdh_pkt(TOCLIENT_ECDH_PUBKEY, X25519_PUBLIC_KEY_SIZE, peer_id);
			std::string pubkey_str(reinterpret_cast<const char*>(server_kp.public_key.data()),
				X25519_PUBLIC_KEY_SIZE);
			ecdh_pkt.putLongString(pubkey_str);
			Send(&ecdh_pkt);

			enclog_init("ECDH public key sent to peer, waiting for client response")
				<< EncLog::kv("peer", peer_id)
				<< EncLog::kv("session_id", client->encryption_state.session_id)
				<< EncLog::kv("note", "encryption will activate after ECDH exchange")
				<< std::endl;
		} else {
			// ECDH key generation failed — fall back to SRP-only activation
			errorstream << "ECDH key generation failed for peer " << peer_id
				<< ", falling back to SRP-only encryption" << std::endl;
			m_con->ActivatePeerEncryption(peer_id);
			client->encryption_state.activated_at = porting::getTimeS();
		}
	} else {
		// Singleplayer: encryption is unnecessary for localhost connections.
		// Don't log scary INSECURE banners — the connection is local
		// and inherently safe from network interception.
		if (!isSingleplayer()) {
			EncLog::logInsecureConnectionBanner(
				"SRP key exchange failed or auth_data was NULL");
			enclog_security("Peer connection INSECURE")
				<< EncLog::kv("peer", peer_id)
				<< EncLog::kv("encryption_initialized", false)
				<< std::endl;
		} else {
			enclog_init("Singleplayer: encryption not active (local connection)")
				<< EncLog::kv("peer", peer_id)
				<< EncLog::kv("mode", "singleplayer")
				<< EncLog::kv("reason", "localhost_does_not_need_encryption")
				<< std::endl;
		}
	}
}
