// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

// This file contains authentication-related packet handlers for Client.
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

const char *accessDeniedStrings[SERVER_ACCESSDENIED_MAX] = {
	N_("Invalid password"),
	N_("Your client sent something the server didn't expect.  Try reconnecting or updating your client."),
	N_("The server is running in singleplayer mode.  You cannot connect."),
	N_("Your client's version is not supported.\nPlease contact the server administrator."),
	N_("Player name contains disallowed characters"),
	N_("Player name not allowed"),
	N_("Too many users"),
	N_("Empty passwords are disallowed.  Set a password and try again."),
	N_("Another client is connected with this name.  If your client closed unexpectedly, try again in a minute."),
	N_("Internal server error"),
	"",
	N_("Server shutting down"),
	N_("The server has experienced an internal error.  You will now be disconnected.")
};

void Client::handleCommand_Hello(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	u8 serialization_ver; // negotiated value
	u16 proto_ver;
	u16 unused_compression_mode;
	u32 auth_mechs;
	std::string unused;
	*pkt >> serialization_ver >> unused_compression_mode >> proto_ver
		>> auth_mechs >> unused;

	// Parse optional security_flags byte (forward-compatible: old servers won't send it)
	u8 security_flags = 0;
	if (pkt->hasRemainingBytes()) {
		*pkt >> security_flags;
	}

	// Build detailed security info from the server's advertised flags
	m_security_info = connectionSecurityInfoFromFlags(security_flags);

	// Populate additional connection details
	m_security_info.protocol_version = proto_ver;
	m_security_info.server_address = m_address_name;
	// Port is set from the connection's address
	if (m_con) {
		Address addr = m_con->GetPeerAddress(PEER_ID_SERVER);
		m_security_info.server_port = addr.getPort();
	}

	// Determine authentication method from the negotiated auth mechanism
	AuthMechanism chosen_auth_mechanism = choseAuthMech(auth_mechs);
	if (chosen_auth_mechanism == AUTH_MECHANISM_SRP ||
	    chosen_auth_mechanism == AUTH_MECHANISM_LEGACY_PASSWORD) {
		m_security_info.authentication = ConnectionSecurityInfo::AUTH_SRP;
	}

	// Write security info to runtime settings so the Lua settings dialog
	// can display it in the "Connection Security" tab. These are transient
	// values that reflect the current connection state — they are not
	// persisted to the config file.
	g_settings->set("security_info_state", m_security_info.getStateString());
	g_settings->set("security_info_encryption", m_security_info.getEncryptionString());
	g_settings->set("security_info_key_exchange", m_security_info.getKeyExchangeString());
	g_settings->set("security_info_authentication", m_security_info.getAuthenticationString());
	g_settings->set("security_info_cipher_suite", m_security_info.getCipherSuiteString());
	g_settings->set("security_info_cert_status", m_security_info.getCertificateStatusString());
	g_settings->set("security_info_forward_secrecy", m_security_info.isForwardSecret() ? "Yes" : "No");
	g_settings->set("security_info_replay_protection", m_security_info.isReplayProtected() ? "Yes" : "No");
	g_settings->set("security_info_protocol_version", std::to_string(m_security_info.protocol_version));
	g_settings->set("security_info_server_address", m_security_info.server_address);
	g_settings->set("security_info_server_port", std::to_string(m_security_info.server_port));

	// v9: Don't generate fake session data here.
	// Session ID and fingerprint will be derived from the SRP session key
	// after authentication succeeds (in handleCommand_AuthAccept).
	// For now, set honest placeholder values.
	m_security_info.session_id = "Pending authentication";
	m_security_info.connected_since = 0;
	m_security_info.server_fingerprint = "Pending authentication";

	// Write v8 security info to runtime settings
	g_settings->set("security_info_session_id", m_security_info.session_id);
	g_settings->set("security_info_connected_since", std::to_string(m_security_info.connected_since));
	g_settings->set("security_info_server_fingerprint", m_security_info.server_fingerprint);
	g_settings->set("security_info_tls_version", m_security_info.getTlsVersionString());
	g_settings->set("security_info_security_score", m_security_info.getSecurityScoreString());

	// Chose an auth method we support
	infostream << "Client: TOCLIENT_HELLO received with "
			<< "serialization_ver=" << (u32)serialization_ver
			<< ", auth_mechs=" << auth_mechs
			<< ", proto_ver=" << proto_ver
			<< ", security_flags=" << (u32)security_flags
			<< " (connection " << (m_security_info.isSecure() ? "ENCRYPTED" : "INSECURE") << ")"
			<< ", encryption=" << m_security_info.getEncryptionString()
			<< ", key_exchange=" << m_security_info.getKeyExchangeString()
			<< ", forward_secrecy=" << (m_security_info.forward_secrecy ? "yes" : "no")
			<< ", replay_protection=" << (m_security_info.replay_protection ? "yes" : "no")
			<< ". Doing auth with mech " << chosen_auth_mechanism << std::endl;

	if (!ser_ver_supported_read(serialization_ver)) {
		infostream << "Client: TOCLIENT_HELLO: Server sent "
				<< "unsupported ser_fmt_ver=" << (int)serialization_ver << std::endl;
		return;
	}

	m_server_ser_ver = serialization_ver;
	m_proto_ver = proto_ver;

	if (m_chosen_auth_mech != AUTH_MECHANISM_NONE) {
		// we received a TOCLIENT_HELLO while auth was already going on
		errorstream << "Client: TOCLIENT_HELLO while auth was already going on"
			<< "(chosen_mech=" << m_chosen_auth_mech << ")." << std::endl;
		if (m_chosen_auth_mech == AUTH_MECHANISM_SRP ||
				m_chosen_auth_mech == AUTH_MECHANISM_LEGACY_PASSWORD) {
			srp_user_delete((SRPUser *) m_auth_data);
			m_auth_data = 0;
		}
	}

	// Authenticate using that method, or abort if there wasn't any method found
	if (chosen_auth_mechanism != AUTH_MECHANISM_NONE) {
		bool is_register = chosen_auth_mechanism == AUTH_MECHANISM_FIRST_SRP;
		ELoginRegister mode = is_register ? ELoginRegister::Register : ELoginRegister::Login;
		if (m_allow_login_or_register != ELoginRegister::Any &&
				m_allow_login_or_register != mode) {
			m_chosen_auth_mech = AUTH_MECHANISM_NONE;
			m_access_denied = true;
			if (m_allow_login_or_register == ELoginRegister::Login) {
				m_access_denied_reason =
						gettext("Name is not registered. To create an account on this server, click 'Register'");
			} else {
				m_access_denied_reason =
						gettext("Name is taken. Please choose another name");
			}
			m_con->Disconnect();
		} else {
			startAuth(chosen_auth_mechanism);
		}
	} else {
		m_chosen_auth_mech = AUTH_MECHANISM_NONE;
		m_access_denied = true;
		m_access_denied_reason = "Unknown";
		m_con->Disconnect();
	}

}

void Client::handleCommand_AuthAccept(NetworkPacket* pkt)
{
	// v9: Extract SRP session key BEFORE deleting auth data.
	// The session key is a 32-byte shared secret produced by the SRP exchange.
	// After successful SRP authentication, both client and server have the same key.
	// We use this key to derive encryption keys for the transport layer.
	// If we don't capture it here, deleteAuthData() will zero it out.
	//
	// IMPORTANT: We support ALL SRP-based auth mechanisms:
	//   - AUTH_MECHANISM_SRP: Normal login with existing account
	//   - AUTH_MECHANISM_LEGACY_PASSWORD: Legacy password migration
	//   - AUTH_MECHANISM_FIRST_SRP: New user registration (also produces a session key)
	// All three mechanisms create an SRPUser that has a session key after
	// srp_user_process_challenge() is called.
	bool encryption_initialized = false;
	enclog_init("AUTH_ACCEPT received, checking SRP session key")
		<< EncLog::kv("auth_mech", (int)m_chosen_auth_mech)
		<< EncLog::kv("auth_data_present", m_auth_data ? "yes" : "no")
		<< std::endl;

	if (m_chosen_auth_mech == AUTH_MECHANISM_SRP ||
			m_chosen_auth_mech == AUTH_MECHANISM_LEGACY_PASSWORD ||
			m_chosen_auth_mech == AUTH_MECHANISM_FIRST_SRP) {
		SRPUser *usr = (SRPUser *) m_auth_data;
		// v9: We do NOT check srp_user_is_authenticated() here because
		// the Luanti protocol never sends the HAMK (server proof) to the
		// client, so srp_user_verify_session() is never called, and
		// usr->authenticated is always 0. This is fine — the session key
		// IS available after srp_user_process_challenge() was called (in
		// handleCommand_SRPBytesSandB), and receiving AUTH_ACCEPT proves
		// the server accepted our M, meaning both sides computed the same
		// session key. We just need the SRPUser to exist and have a key.
		if (usr) {
			size_t key_len = 0;
			const unsigned char *session_key = srp_user_get_session_key(usr, &key_len);
			enclog_init("SRP session key extracted")
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

			if (!EncryptionConfig::shouldEncrypt() || m_internal_server) {
				// Encryption disabled or singleplayer: SRP still runs for
				// password authentication, but the session key is NOT used
				// to activate AES-256-GCM encryption.
				if (m_internal_server) {
					enclog_init("Singleplayer: skipping encryption (local connection)")
						<< EncLog::kv("mode", "singleplayer")
						<< EncLog::kv("reason", "localhost_does_not_need_encryption")
						<< std::endl;
				} else {
					EncryptionConfig::logEncryptionDecision(0, false, false);
				}
				m_encryption_state.disable();
			} else if (session_key && key_len == SRP_SESSION_KEY_SIZE) {
				bool ok = m_encryption_state.initFromSRPSessionKey(
					session_key, key_len, false /* is_server=false */);
				if (ok) {
					// Push encryption state (with active=false) to the connection layer.
					// Keys are loaded but encryption is NOT active yet.
					m_con->SetPeerEncryptionState(PEER_ID_SERVER, m_encryption_state);

					// v9.12: Do NOT activate encryption here. The flow is:
					// 1. AuthAccept: init keys (active=false), push to connection
					// 2. handleCommand_EcdhPubkey: mix ECDH secret into keys,
					//    push updated keys to connection (still active=false)
					// 3. Server activates first (it calls ActivatePeerEncryption
					//    after processing TOSERVER_ECDH_PUBKEY)
					// 4. Server sends first encrypted packet
					// 5. Client receive path auto-activates when it successfully
					//    decrypts that first encrypted packet (see threads.cpp)
					//
					// This deferred activation eliminates the race condition where
					// the client would activate before the server, causing
					// encrypted packets to be sent to a server still in plaintext
					// mode. Both sides now activate symmetrically.
					//
					// If the server does NOT support ECDH (old version), the
					// ECDH pubkey packet will never arrive, and encryption
					// remains inactive. We handle this fallback by checking
					// in the main client loop — if ECDH doesn't arrive within
					// a timeout, we activate with SRP-only keys.
					//
					// For now, we populate security info as "not yet active"
					// and let the auto-activation in the receive path update it.
					encryption_initialized = true;
					enclog_activate("Client encryption initialized and activation queued")
						<< EncLog::kv("session_id", m_encryption_state.session_id)
						<< EncLog::kv("fingerprint", m_encryption_state.server_fingerprint)
						<< EncLog::kv("role", "client")
						<< EncLog::kv("cipher", "AES-256-GCM")
						<< std::endl;
				} else {
					enclog_error("Failed to initialize encryption from SRP session key")
						<< EncLog::kv("reason", "HKDF key derivation failed")
						<< EncLog::kv("suggestion", "Check that OpenSSL is properly linked")
						<< std::endl;
					m_encryption_state.disable();
				}
			} else {
				enclog_error("SRP session key unavailable or wrong size")
					<< EncLog::kv("key_len", (u32)key_len)
					<< EncLog::kv("expected", (u32)SRP_SESSION_KEY_SIZE)
					<< EncLog::kv("reason", session_key ? "key present but wrong size" : "key is NULL")
					<< EncLog::kv("result", "ENCRYPTION_NOT_POSSIBLE")
					<< std::endl;
			}
		} else {
			enclog_error("SRPUser is NULL, cannot extract session key")
				<< EncLog::kv("auth_mech", (int)m_chosen_auth_mech)
				<< std::endl;
		}
	} else {
		enclog_init("Auth mechanism does not use SRP, encryption not applicable")
			<< EncLog::kv("auth_mech", (int)m_chosen_auth_mech)
			<< std::endl;
	}

	if (!encryption_initialized) {
		if (m_internal_server) {
			// Singleplayer: encryption is unnecessary for localhost connections.
			// Don't log scary INSECURE banners — the connection is local
			// and inherently safe from network interception.
			enclog_init("Singleplayer: encryption not active (local connection)")
				<< EncLog::kv("mode", "singleplayer")
				<< EncLog::kv("reason", "localhost_does_not_need_encryption")
				<< std::endl;

			// Set singleplayer-appropriate security info
			// (local connection, no network risk)
			m_security_info.state = ConnectionSecurity::Encrypted;
			m_security_info.encryption_algorithm = ConnectionSecurityInfo::ENCRYPTION_NONE;
			m_security_info.key_exchange = ConnectionSecurityInfo::KEY_EXCHANGE_NONE;
			m_security_info.authentication = ConnectionSecurityInfo::AUTH_NONE;
			m_security_info.cipher_suite = ConnectionSecurityInfo::CIPHER_NONE;
			m_security_info.replay_protection = false;
			m_security_info.forward_secrecy = false;
			m_security_info.ecdh_forward_secrecy = false;
			m_security_info.fingerprint_pinned = false;
			m_security_info.fingerprint_verify_result = 0;
			m_security_info.certificate_status = ConnectionSecurityInfo::CERT_NOT_VERIFIED;
			m_security_info.tls_version = ConnectionSecurityInfo::TLS_NONE;
			m_security_info.session_id = "local";
			m_security_info.server_fingerprint = "local";
			m_security_info.connected_since = porting::getTimeS();
		} else {
			// Multiplayer: encryption failure IS a security concern
			EncLog::logInsecureConnectionBanner(
				"SRP key exchange failed or auth mechanism does not support encryption");
			enclog_security("CONNECTION INSECURE")
				<< EncLog::kv("auth_mech", (int)m_chosen_auth_mech)
				<< EncLog::kv("auth_data", m_auth_data ? "present" : "NULL")
				<< EncLog::kv("encryption_initialized", false)
				<< std::endl;
		}
	}

	deleteAuthData();

	v3f unused;
	*pkt >> unused >> m_map_seed >> m_recommended_send_interval
		>> m_sudo_auth_methods;

	infostream << "Client: received map seed: " << m_map_seed << std::endl;
	infostream << "Client: received recommended send interval "
					<< m_recommended_send_interval<<std::endl;

	// Reply to server
	/* TRANSLATORS: DO NOT TRANSLATE THIS LITERALLY!
	This is a special string which needs to contain the translation's
	language code (e.g. "de" for German). */
	std::string lang = gettext("LANG_CODE");
	if (lang == "LANG_CODE")
		lang.clear();

	NetworkPacket resp_pkt(TOSERVER_INIT2, sizeof(u16) + lang.size());
	resp_pkt << lang;
	Send(&resp_pkt);

	m_state = LC_Init;

	// v9: Update runtime security settings with REAL data
	// In singleplayer, override the state string to "Local" since the
	// connection is to localhost and doesn't need network encryption.
	if (m_internal_server) {
		g_settings->set("security_info_state", "Local");
		g_settings->set("security_info_encryption", "N/A (Local)");
		g_settings->set("security_info_key_exchange", "N/A (Local)");
		g_settings->set("security_info_authentication", "N/A (Local)");
		g_settings->set("security_info_cipher_suite", "N/A (Local)");
		g_settings->set("security_info_cert_status", "N/A (Local)");
		g_settings->set("security_info_forward_secrecy", "N/A (Local)");
		g_settings->set("security_info_replay_protection", "N/A (Local)");
		g_settings->set("security_info_session_id", "local");
		g_settings->set("security_info_connected_since",
			std::to_string(m_security_info.connected_since));
		g_settings->set("security_info_server_fingerprint", "local");
		g_settings->set("security_info_tls_version", "N/A (Local)");
		g_settings->set("security_info_security_score", "N/A (Local)");
	} else {
		g_settings->set("security_info_state", m_security_info.getStateString());
		g_settings->set("security_info_encryption", m_security_info.getEncryptionString());
		g_settings->set("security_info_key_exchange", m_security_info.getKeyExchangeString());
		g_settings->set("security_info_authentication", m_security_info.getAuthenticationString());
		g_settings->set("security_info_cipher_suite", m_security_info.getCipherSuiteString());
		g_settings->set("security_info_cert_status", m_security_info.getCertificateStatusString());
		g_settings->set("security_info_forward_secrecy", m_security_info.isForwardSecret() ? "Yes" : "No");
		g_settings->set("security_info_replay_protection", m_security_info.isReplayProtected() ? "Yes" : "No");
		g_settings->set("security_info_session_id", m_security_info.session_id);
		g_settings->set("security_info_connected_since", std::to_string(m_security_info.connected_since));
		g_settings->set("security_info_server_fingerprint", m_security_info.server_fingerprint);
		g_settings->set("security_info_tls_version", m_security_info.getTlsVersionString());
		g_settings->set("security_info_security_score", m_security_info.getSecurityScoreString());
	}

	// Log meaningful info
	if (!m_internal_server) {
		Address remote = m_con->GetPeerAddress(PEER_ID_SERVER);
		enclog_security("Connected to server")
			<< EncLog::kv("address", m_address_name)
			<< EncLog::kv("port", remote.getPort())
			<< EncLog::kv("secure", encryption_initialized)
			<< EncLog::kv("session_id", m_security_info.session_id)
			<< EncLog::kv("fingerprint", m_security_info.server_fingerprint)
			<< EncLog::kv("score", m_security_info.getSecurityScoreString())
			<< std::endl;

		// Log the full security banner on the client side too
		if (encryption_initialized) {
			EncLog::logSecureConnectionBanner(
				m_security_info.session_id,
				m_security_info.server_fingerprint,
				m_security_info.isForwardSecret(),
				m_security_info.isReplayProtected(),
				m_security_info.getSecurityScore(),
				m_security_info.getSecurityScoreString().substr(
					m_security_info.getSecurityScoreString().find('(') + 1,
					m_security_info.getSecurityScoreString().find(')') -
					m_security_info.getSecurityScoreString().find('(') - 1),
				m_encryption_state.c2s.packets_processed,
				m_encryption_state.s2c.packets_processed);
		}
	}
}

void Client::handleCommand_AcceptSudoMode(NetworkPacket* pkt)
{
	deleteAuthData();

	m_password = m_new_password;

	verbosestream << "Client: Received TOCLIENT_ACCEPT_SUDO_MODE." << std::endl;

	// send packet to actually set the password
	startAuth(AUTH_MECHANISM_FIRST_SRP);

	// reset again
	m_chosen_auth_mech = AUTH_MECHANISM_NONE;
}

void Client::handleCommand_DenySudoMode(NetworkPacket* pkt)
{
	ChatMessage *chatMessage = new ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
			L"Password change denied. Password NOT changed.");
	pushToChatQueue(chatMessage);
	// reset everything and be sad
	deleteAuthData();
}

void Client::handleCommand_AccessDenied(NetworkPacket* pkt)
{
	// The server didn't like our password. Note, this needs
	// to be processed even if the serialization format has
	// not been agreed yet, the same as TOCLIENT_AUTH_ACCEPT.
	m_access_denied = true;

	if (pkt->getCommand() != TOCLIENT_ACCESS_DENIED) {
		// Servers older than 5.6 still send TOCLIENT_ACCESS_DENIED_LEGACY sometimes.
		// see commit a65f6f07f3a5601207b790edcc8cc945133112f7
		if (pkt->getSize() >= 2) {
			std::wstring wide_reason;
			*pkt >> wide_reason;
			m_access_denied_reason = wide_to_utf8(wide_reason);
		}
		return;
	}

	if (pkt->getSize() < 1)
		return;

	u8 denyCode;
	u8 reconnect = 0; // default of 'm_access_denied_reconnect'
	*pkt >> denyCode;

	do {
		if (!pkt->hasRemainingBytes())
			break;
		// Reliably available since 5.10.0-dev
		*pkt >> m_access_denied_reason;

		if (!pkt->hasRemainingBytes())
			break;
		// Reliably available since 5.10.0-dev
		*pkt >> reconnect;
	} while (0);


	if (m_access_denied_reason.empty()) {
		if (denyCode >= SERVER_ACCESSDENIED_MAX) {
			m_access_denied_reason = gettext("Unknown disconnect reason.");
		} else if (denyCode != SERVER_ACCESSDENIED_CUSTOM_STRING) {
			m_access_denied_reason = gettext(accessDeniedStrings[denyCode]);
		}
	}

	if (denyCode == SERVER_ACCESSDENIED_TOO_MANY_USERS) {
		m_access_denied_reconnect = true;
	} else {
		m_access_denied_reconnect = reconnect & 1;
	}
}

void Client::handleCommand_SrpBytesSandB(NetworkPacket* pkt)
{
	// v9 fix: Also accept FIRST_SRP since we now create an SRPUser
	// for the key exchange after sending the registration verifier.
	if (m_chosen_auth_mech != AUTH_MECHANISM_SRP &&
			m_chosen_auth_mech != AUTH_MECHANISM_LEGACY_PASSWORD &&
			m_chosen_auth_mech != AUTH_MECHANISM_FIRST_SRP) {
		errorstream << "Client: Received SRP S_B login message,"
			<< " but wasn't supposed to (chosen_mech="
			<< m_chosen_auth_mech << ")." << std::endl;
		return;
	}

	char *bytes_M = 0;
	size_t len_M = 0;
	SRPUser *usr = (SRPUser *) m_auth_data;
	std::string s;
	std::string B;
	*pkt >> s >> B;

	infostream << "Client: Received TOCLIENT_SRP_BYTES_S_B." << std::endl;

	srp_user_process_challenge(usr, (const unsigned char *) s.c_str(), s.size(),
		(const unsigned char *) B.c_str(), B.size(),
		(unsigned char **) &bytes_M, &len_M);

	if ( !bytes_M ) {
		errorstream << "Client: SRP-6a S_B safety check violation!" << std::endl;
		return;
	}

	NetworkPacket resp_pkt(TOSERVER_SRP_BYTES_M, 0);
	resp_pkt << std::string(bytes_M, len_M);
	Send(&resp_pkt);
}
