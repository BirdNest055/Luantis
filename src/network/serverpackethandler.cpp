// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "chatmessage.h"
#include "server.h"
#include "serverenvironment.h"
#include "log.h"
#include "emerge.h"
#include "itemdef.h"
#include "mapblock.h"
#include "modchannels.h"
#include "nodedef.h"
#include "porting.h" // strcasecmp
#include "remoteplayer.h"
#include "rollback_interface.h"
#include "scripting_server.h"
#include "serialization.h"
#include "settings.h"
#include "tool.h"
#include "network/connection.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "network/encryption_config.h"
#include "server/player_sao.h"
#include "server/serverinventorymgr.h"
#include "util/auth.h"
#include "util/base64.h"
#include "util/keypair.h"
#include "util/pointedthing.h"
#include "util/srp.h"
#include "util/string.h" // Batch 28: for is_valid_utf8, sanitize_untrusted
#include "network/connection_security.h"
#include "network/crypto.h"
#include "network/encryption_log.h"
#include "clientdynamicinfo.h"

#include <algorithm>

void Server::handleCommand_Deprecated(NetworkPacket* pkt)
{
        auto &h = toServerCommandTable[pkt->getCommand()];
        infostream << "Server: ignoring unsupported " << h.name << " from peer " <<
                pkt->getPeerId() << std::endl;
}

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

        // Rate limiter for connection attempts from the same IP
        // Max 5 attempts per 10 seconds
        {
                u64 now_s = porting::getTimeS();
                auto &rl = m_connect_rate_limit[addr_s];
                if (now_s - rl.window_start_s > 10) {
                        rl.count = 0;
                        rl.window_start_s = now_s;
                }
                rl.count++;
                if (rl.count > 5) {
                        warningstream << "Server: Rate limiting connection attempt from "
                                << addr_s << " (" << rl.count << " in 10s)" << std::endl;
                        DenyAccess(peer_id, SERVER_ACCESSDENIED_RATE_LIMIT,
                                "Too many connection attempts, try again later");
                        return;
                }
        }

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
                // v9.29: Check for keypair auth (#2# prefix) before SRP (#1# prefix)
                if (is_keypair_auth(encpwd)) {
                        auth_mechs |= AUTH_MECHANISM_KEYPAIR;
                        client->enc_pwd = encpwd;
                } else {
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
                }
        } else {
                std::string default_password = g_settings->get("default_password");
                if (isSingleplayer() || default_password.length() == 0) {
                        auth_mechs |= AUTH_MECHANISM_FIRST_SRP;
                        // v9.29: Also offer keypair auth for new accounts if enabled
                        if (g_settings->getBool("keypair_auth"))
                                auth_mechs |= AUTH_MECHANISM_KEYPAIR;
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

void Server::handleCommand_RequestMedia(NetworkPacket* pkt)
{
        std::unordered_set<std::string> tosend;
        u16 numfiles;

        *pkt >> numfiles;

        // Batch 33: Limit the number of media files a client can request at once
        // to prevent DoS via excessive file requests. 1000 is generous; typical
        // games request far fewer.
        constexpr u16 MAX_MEDIA_REQUESTS = 1000;
        if (numfiles > MAX_MEDIA_REQUESTS) {
                warningstream << "Server: Client " << getPlayerName(pkt->getPeerId())
                        << " requested too many media files (" << numfiles
                        << "), limiting to " << MAX_MEDIA_REQUESTS << std::endl;
                numfiles = MAX_MEDIA_REQUESTS;
        }

        session_t peer_id = pkt->getPeerId();
        verbosestream << "Client " << getPlayerName(peer_id)
                << " requested media file(s):\n";

        for (u16 i = 0; i < numfiles; i++) {
                std::string name;

                *pkt >> name;

                tosend.emplace(name);
                verbosestream << "  " << name << "\n";
        }
        verbosestream << std::flush;

        sendRequestedMedia(peer_id, tosend);
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
                // Batch 30: Clamp player count to u16 range to prevent silent truncation
                list_pkt << (u8) PLAYER_LIST_INIT << (u16)std::min(players.size(), (size_t)U16_MAX);
                for (const auto &player : players)
                        list_pkt << player;
                Send(peer_id, &list_pkt);
        }

        s64 last_login;
        m_script->getAuth(playersao->getPlayer()->getName(), nullptr, nullptr, &last_login);
        m_script->on_joinplayer(playersao, last_login);

        // v9.39: Send voice chat state to this client
        {
                bool voice_enabled = g_settings->getBool("enable_voice_chat_server");
                bool e2ee_required = g_settings->getBool("voice_chat_e2ee_required");
                NetworkPacket voice_pkt(TOCLIENT_VOICE_STATE, 0, peer_id);
                voice_pkt << (u8)(voice_enabled ? 1 : 0)
                          << (u8)(e2ee_required ? 1 : 0)
                          << (u16)0; // voice_port = 0 (use same connection)
                Send(peer_id, &voice_pkt);
        }

        // Send shutdown timer if shutdown has been scheduled
        if (m_shutdown_state.isTimerRunning())
                SendChatMessage(peer_id, m_shutdown_state.getShutdownTimerMessage());
}

void Server::handleCommand_GotBlocks(NetworkPacket* pkt)
{
        if (pkt->getSize() < 1)
                return;

        /*
                [0] u16 command
                [2] u8 count
                [3] v3s16 pos_0
                [3+6] v3s16 pos_1
                ...
        */

        u8 count;
        *pkt >> count;

        // Batch 33: Verify remaining bytes are sufficient for the claimed count.
        // Each block position is 6 bytes (v3s16). If count * 6 > remaining,
        // the packet is malformed — clamp to what's available.
        const u32 bytes_needed = (u32)count * 6;
        const u32 bytes_available = pkt->getRemainingBytes();
        if (bytes_needed > bytes_available) {
                u8 safe_count = static_cast<u8>(bytes_available / 6);
                warningstream << "Server: GotBlocks count " << (int)count
                        << " exceeds packet data (need " << bytes_needed
                        << " bytes, have " << bytes_available
                        << "), clamping to " << (int)safe_count << std::endl;
                count = safe_count;
        }

        ClientInterface::AutoLock lock(m_clients);
        RemoteClient *client = m_clients.lockedGetClientNoEx(pkt->getPeerId());
        if (!client)
                return;

        for (u16 i = 0; i < count; i++) {
                v3s16 p;
                *pkt >> p;
                // Batch 37: Validate block coordinates from network packet
                if (blockpos_over_max_limit(p)) {
                        warningstream << "Server: GotBlocks: ignoring out-of-range block ("
                                << p.X << "," << p.Y << "," << p.Z << ") from peer "
                                << pkt->getPeerId() << std::endl;
                        continue;
                }
                client->GotBlock(p);
        }
}

void Server::process_PlayerPos(RemotePlayer *player, PlayerSAO *playersao,
        NetworkPacket *pkt)
{
        v3s32 ps, ss;
        s32 f32pitch, f32yaw;
        u8 f32fov;

        *pkt >> ps;
        *pkt >> ss;
        *pkt >> f32pitch;
        *pkt >> f32yaw;

        f32 pitch = (f32)f32pitch / 100.0f;
        f32 yaw = (f32)f32yaw / 100.0f;
        u32 keyPressed = 0;

        f32 fov = 0;
        u8 wanted_range = 0;
        u8 bits = 0; // bits instead of bool so it is extensible later

        *pkt >> keyPressed;
        player->control.unpackKeysPressed(keyPressed);

        *pkt >> f32fov;
        fov = (f32)f32fov / 80.0f;
        *pkt >> wanted_range;

        bool have_movement_data = false;
        do {
                if (!pkt->hasRemainingBytes())
                        break;
                // >= 5.8.0-dev
                *pkt >> bits;

                if (!pkt->hasRemainingBytes())
                        break;
                // >= 5.10.0-dev
                f32 movement_speed;
                *pkt >> movement_speed;
                if (movement_speed != movement_speed) // NaN
                        movement_speed = 0.0f;
                player->control.movement_speed = std::clamp(movement_speed, 0.0f, 1.0f);
                *pkt >> player->control.movement_direction;
                have_movement_data = true;
        } while (0);

        if (!have_movement_data) {
                player->control.movement_speed = 0.0f;
                player->control.movement_direction = 0.0f;
                player->control.setMovementFromKeys();
        }

        v3f position((f32)ps.X / 100.0f, (f32)ps.Y / 100.0f, (f32)ps.Z / 100.0f);
        v3f speed((f32)ss.X / 100.0f, (f32)ss.Y / 100.0f, (f32)ss.Z / 100.0f);

        pitch = modulo360f(pitch);
        yaw = wrapDegrees_0_360(yaw);

        if (!playersao->isAttached()) {
                // Only update player positions when moving freely
                // to not interfere with attachment handling
                playersao->setBasePosition(position);
                player->setSpeed(speed);
        }
        playersao->setLookPitch(pitch);
        playersao->setPlayerYaw(yaw);
        playersao->setFov(fov);
        playersao->setWantedRange(wanted_range);
        playersao->setCameraInverted(bits & 0x01);

        if (playersao->checkMovementCheat()) {
                // Call callbacks
                m_script->on_cheat(playersao, "moved_too_fast");
                SendMovePlayer(playersao);
        }
}

void Server::handleCommand_PlayerPos(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();

        // Batch 33: Validate client is in Active state before processing position
        RemoteClient *client = getClientNoEx(peer_id, CS_Active);
        if (!client) {
                // Silently ignore — position packets before active state are normal during join
                return;
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao) {
                warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
                return;
        }

        // If player is dead we don't care of this packet
        if (playersao->isDead()) {
                verbosestream << "TOSERVER_PLAYERPOS: " << player->getName()
                                << " is dead. Ignoring packet";
                return;
        }

        process_PlayerPos(player, playersao, pkt);
}

void Server::handleCommand_DeletedBlocks(NetworkPacket* pkt)
{
        if (pkt->getSize() < 1)
                return;

        /*
                [0] u16 command
                [2] u8 count
                [3] v3s16 pos_0
                [3+6] v3s16 pos_1
                ...
        */

        u8 count;
        *pkt >> count;

        // Batch 33: Verify remaining bytes are sufficient for the claimed count.
        // Each block position is 6 bytes (v3s16). If count * 6 > remaining,
        // the packet is malformed — clamp to what's available.
        const u32 bytes_needed = (u32)count * 6;
        const u32 bytes_available = pkt->getRemainingBytes();
        if (bytes_needed > bytes_available) {
                u8 safe_count = static_cast<u8>(bytes_available / 6);
                warningstream << "Server: DeletedBlocks count " << (int)count
                        << " exceeds packet data (need " << bytes_needed
                        << " bytes, have " << bytes_available
                        << "), clamping to " << (int)safe_count << std::endl;
                count = safe_count;
        }

        ClientInterface::AutoLock lock(m_clients);
        RemoteClient *client = m_clients.lockedGetClientNoEx(pkt->getPeerId());
        if (!client)
                return;

        for (u16 i = 0; i < count; i++) {
                v3s16 p;
                *pkt >> p;
                client->SetBlockNotSent(p);
        }
}

void Server::handleCommand_InventoryAction(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();

        // Rate limiter for inventory actions: max 10 per second per player
        {
                u64 now_s = porting::getTimeS();
                auto &rl = m_inventory_rate_limit[peer_id];
                if (now_s - rl.second > 1) {
                        rl.first = 0;
                        rl.second = now_s;
                }
                rl.first++;
                if (rl.first > 10) {
                        warningstream << "Server: Rate limiting inventory actions for peer_id="
                                << peer_id << " (" << rl.first << " in 1s)" << std::endl;
                        return;
                }
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao) {
                warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
                return;
        }

        // Strip command and create a stream
        std::string datastring(pkt->getString(0), pkt->getSize());
        std::istringstream is(datastring, std::ios_base::binary);
        // Create an action
        std::unique_ptr<InventoryAction> a(InventoryAction::deSerialize(is));
        if (!a) {
                infostream << "TOSERVER_INVENTORY_ACTION: "
                                << "InventoryAction::deSerialize() returned NULL"
                                << std::endl;
                return;
        }

        // If something goes wrong, this player is to blame
        RollbackScopeActor rollback_scope(m_rollback,
                        "player:" + player->getName());

        /*
                Note: Always set inventory not sent, to repair cases
                where the client made a bad prediction.
        */

        auto mark_player_inv_list_dirty = [this](const InventoryLocation &loc,
                        const std::string &list_name) {

                // Undo the client prediction of the affected list. See `clientApply`.
                if (loc.type != InventoryLocation::PLAYER)
                        return;

                Inventory *inv = m_inventory_mgr->getInventory(loc);
                if (!inv)
                        return;

                InventoryList *list = inv->getList(list_name);
                if (!list)
                        return;

                list->setModified(true);
        };

        const bool player_has_interact = checkPriv(player->getName(), "interact");

        auto check_inv_access = [player, player_has_interact, this] (
                        const InventoryLocation &loc) -> bool {

                // Players without interact may modify their own inventory
                if (!player_has_interact && loc.type != InventoryLocation::PLAYER) {
                        infostream << "Cannot modify foreign inventory: "
                                        << "No interact privilege" << std::endl;
                        return false;
                }

                switch (loc.type) {
                case InventoryLocation::CURRENT_PLAYER:
                        // Only used internally on the client, never sent
                        return false;
                case InventoryLocation::PLAYER:
                        // Allow access to own inventory in all cases
                        return loc.name == player->getName();
                case InventoryLocation::NODEMETA:
                        {
                                // Check for out-of-range interaction
                                v3f node_pos   = intToFloat(loc.p, BS);
                                v3f player_pos = player->getPlayerSAO()->getEyePosition();
                                f32 d = player_pos.getDistanceFrom(node_pos);
                                return checkInteractDistance(player, d, "inventory");
                        }
                case InventoryLocation::DETACHED:
                        return getInventoryMgr()->checkDetachedInventoryAccess(loc, player->getName());
                default:
                        return false;
                }
        };

        /*
                Handle restrictions and special cases of the move action
        */
        if (a->getType() == IAction::Move) {
                IMoveAction *ma = (IMoveAction*)a.get();

                ma->from_inv.applyCurrentPlayer(player->getName());
                ma->to_inv.applyCurrentPlayer(player->getName());

                m_inventory_mgr->setInventoryModified(ma->from_inv);
                mark_player_inv_list_dirty(ma->from_inv, ma->from_list);
                bool inv_different = ma->from_inv != ma->to_inv;
                if (inv_different)
                        m_inventory_mgr->setInventoryModified(ma->to_inv);
                if (inv_different || ma->from_list != ma->to_list)
                        mark_player_inv_list_dirty(ma->to_inv, ma->to_list);

                if (!check_inv_access(ma->from_inv) ||
                                !check_inv_access(ma->to_inv))
                        return;

                /*
                        Disable moving items out of craftpreview
                */
                if (ma->from_list == "craftpreview") {
                        infostream << "Ignoring IMoveAction from "
                                        << (ma->from_inv.dump()) << ":" << ma->from_list
                                        << " to " << (ma->to_inv.dump()) << ":" << ma->to_list
                                        << " because src is " << ma->from_list << std::endl;
                        return;
                }

                /*
                        Disable moving items into craftresult and craftpreview
                */
                if (ma->to_list == "craftpreview" || ma->to_list == "craftresult") {
                        infostream << "Ignoring IMoveAction from "
                                        << (ma->from_inv.dump()) << ":" << ma->from_list
                                        << " to " << (ma->to_inv.dump()) << ":" << ma->to_list
                                        << " because dst is " << ma->to_list << std::endl;
                        return;
                }
        }
        /*
                Handle restrictions and special cases of the drop action
        */
        else if (a->getType() == IAction::Drop) {
                IDropAction *da = (IDropAction*)a.get();

                da->from_inv.applyCurrentPlayer(player->getName());

                m_inventory_mgr->setInventoryModified(da->from_inv);
                mark_player_inv_list_dirty(da->from_inv, da->from_list);

                /*
                        Disable dropping items out of craftpreview
                */
                if (da->from_list == "craftpreview") {
                        infostream << "Ignoring IDropAction from "
                                        << (da->from_inv.dump()) << ":" << da->from_list
                                        << " because src is " << da->from_list << std::endl;
                        return;
                }

                // Disallow dropping items if not allowed to interact
                if (!player_has_interact || !check_inv_access(da->from_inv))
                        return;

                // Disallow dropping items if dead
                if (playersao->isDead()) {
                        infostream << "Ignoring IDropAction from "
                                        << (da->from_inv.dump()) << ":" << da->from_list
                                        << " because player is dead." << std::endl;
                        return;
                }
        }
        /*
                Handle restrictions and special cases of the craft action
        */
        else if (a->getType() == IAction::Craft) {
                ICraftAction *ca = (ICraftAction*)a.get();

                ca->craft_inv.applyCurrentPlayer(player->getName());

                m_inventory_mgr->setInventoryModified(ca->craft_inv);
                // Note: `ICraftAction::clientApply` is empty, thus nothing to revert.

                // Disallow crafting if not allowed to interact
                if (!player_has_interact) {
                        infostream << "Cannot craft: "
                                        << "No interact privilege" << std::endl;
                        return;
                }

                if (!check_inv_access(ca->craft_inv))
                        return;
        } else {
                // Unknown action. Ignored.
                return;
        }

        // Do the action
        a->apply(m_inventory_mgr.get(), playersao, this);
}

void Server::handleCommand_ChatMessage(NetworkPacket* pkt)
{
        std::wstring message;
        *pkt >> message;

        session_t peer_id = pkt->getPeerId();

        // Batch 33: Validate client is in Active state before processing chat
        RemoteClient *client = getClientNoEx(peer_id, CS_Active);
        if (!client) {
                warningstream << FUNCTION_NAME
                        << ": ignoring chat from non-active client peer_id=" << peer_id << std::endl;
                return;
        }

        // Batch 33: Rate limit chat messages to prevent spam flooding.
        // Max 5 messages per 2 seconds per player.
        {
                static std::unordered_map<session_t, std::pair<u32, u64>> chat_rate_limit;
                u64 now_s = porting::getTimeS();
                auto &rl = chat_rate_limit[peer_id];
                if (now_s - rl.second > 2) {
                        rl.first = 0;
                        rl.second = now_s;
                }
                rl.first++;
                if (rl.first > 5) {
                        warningstream << "Server: Rate limiting chat for peer_id="
                                << peer_id << " (" << rl.first << " in 2s)" << std::endl;
                        return;
                }
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        // Batch 28: Sanitize chat message from network to strip control chars
        // and validate UTF-8 (the wstring was deserialized from network data)
        std::string message_utf8 = wide_to_utf8(message);
        if (!is_valid_utf8(message_utf8)) {
                message_utf8 = wide_to_utf8(utf8_to_wide(message_utf8));
        }
        message_utf8 = sanitize_untrusted(message_utf8);
        message = utf8_to_wide(message_utf8);

        const auto &name = player->getName();

        std::wstring answer_to_sender = handleChat(name, message, true, player);
        if (!answer_to_sender.empty()) {
                // Send the answer to sender
                SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
                        answer_to_sender));
        }
}

void Server::handleCommand_Damage(NetworkPacket* pkt)
{
        u16 damage;

        *pkt >> damage;

        session_t peer_id = pkt->getPeerId();

        // Batch 33: Validate client is in Active state before processing damage
        RemoteClient *client = getClientNoEx(peer_id, CS_Active);
        if (!client) {
                warningstream << FUNCTION_NAME
                        << ": ignoring damage from non-active client peer_id=" << peer_id << std::endl;
                return;
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao) {
                warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
                return;
        }

        if (!playersao->isImmortal()) {
                if (playersao->isDead()) {
                        verbosestream << "Server: "
                                "Ignoring damage as player " << player->getName()
                                << " is already dead" << std::endl;
                        return;
                }

                actionstream << player->getName() << " damaged by "
                                << (int)damage << " hp at " << (playersao->getBasePosition() / BS)
                                << std::endl;

                PlayerHPChangeReason reason(PlayerHPChangeReason::FALL);
                playersao->setHP((s32)playersao->getHP() - (s32)damage, reason, true);
        }
}

void Server::handleCommand_PlayerItem(NetworkPacket* pkt)
{
        if (pkt->getSize() < 2)
                return;

        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao) {
                warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
                return;
        }

        u16 item;

        *pkt >> item;

        if (player->getMaxHotbarItemcount() == 0) {
                return; // ignore silently
        } else if (item >= player->getMaxHotbarItemcount()) {
                actionstream << "Player " << player->getName()
                        << " tried to access item=" << item
                        << " out of hotbar_itemcount="
                        << player->getMaxHotbarItemcount()
                        << "; ignoring." << std::endl;
                return;
        }

        playersao->getPlayer()->setWieldIndex(item);
}

bool Server::checkInteractDistance(RemotePlayer *player, const f32 d, const std::string &what)
{
        ItemStack selected_item, hand_item;
        const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);
        f32 max_d = BS * getToolRange(tool_item, hand_item, m_itemdef);

        // Cube diagonal * 1.5 for maximal supported node extents:
        // sqrt(3) * 1.5 ≅ 2.6
        if (d > max_d + 2.6f * BS) {
                actionstream << "Player " << player->getName()
                                << " tried to access " << what
                                << " from too far: "
                                << "d=" << d << ", max_d=" << max_d
                                << "; ignoring." << std::endl;
                // Call callbacks
                m_script->on_cheat(player->getPlayerSAO(), "interacted_too_far");
                return false;
        }
        return true;
}

// Tiny helper to retrieve the selected item into an std::optional
static inline void getWieldedItem(const PlayerSAO *playersao, std::optional<ItemStack> &ret)
{
        ret = ItemStack();
        playersao->getWieldedItem(&(*ret));
}

void Server::handleCommand_Interact(NetworkPacket *pkt)
{
        /*
                [0] u16 command
                [2] u8 action
                [3] u16 item
                [5] u32 length of the next item (plen)
                [9] serialized PointedThing
                [9 + plen] player position information
        */

        InteractAction action;
        u16 item_i;

        *pkt >> (u8 &)action;
        *pkt >> item_i;

        std::istringstream tmp_is(pkt->readLongBinaryString(), std::ios::binary);
        PointedThing pointed;
        pointed.deSerialize(tmp_is);

        verbosestream << "TOSERVER_INTERACT: action=" << (int)action << ", item="
                        << item_i << ", pointed=" << pointed.dump() << std::endl;

        session_t peer_id = pkt->getPeerId();

        // Batch 33: Validate client is in Active state before processing interact
        RemoteClient *client = getClientNoEx(peer_id, CS_Active);
        if (!client) {
                warningstream << FUNCTION_NAME
                        << ": ignoring interact from non-active client peer_id=" << peer_id << std::endl;
                return;
        }

        // Batch 33: Rate limit interact actions to prevent server overload.
        // Max 20 interactions per second per player.
        {
                static std::unordered_map<session_t, std::pair<u32, u64>> interact_rate_limit;
                u64 now_s = porting::getTimeS();
                auto &rl = interact_rate_limit[peer_id];
                if (now_s - rl.second > 1) {
                        rl.first = 0;
                        rl.second = now_s;
                }
                rl.first++;
                if (rl.first > 20) {
                        warningstream << "Server: Rate limiting interact for peer_id="
                                << peer_id << " (" << rl.first << " in 1s)" << std::endl;
                        return;
                }
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao) {
                warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
                return;
        }

        if (playersao->isDead()) {
                actionstream << "Server: " << player->getName()
                                << " tried to interact while dead; ignoring." << std::endl;
                if (pointed.type == POINTEDTHING_NODE) {
                        // Re-send block to revert change on client-side
                        RemoteClient *client = getClient(peer_id);
                        v3s16 blockpos = getNodeBlockPos(pointed.node_undersurface);
                        client->SetBlockNotSent(blockpos);
                }
                // Call callbacks
                m_script->on_cheat(playersao, "interacted_while_dead");
                return;
        }

        process_PlayerPos(player, playersao, pkt);

        v3f player_pos = playersao->getLastGoodPosition();

        // Update wielded item
        if (player->getMaxHotbarItemcount() == 0) {
                return; // ignore silently
        } else if (item_i >= player->getMaxHotbarItemcount()) {
                actionstream << "Player " << player->getName()
                        << " tried to access item=" << item_i
                        << " out of hotbar_itemcount="
                        << player->getMaxHotbarItemcount()
                        << "; ignoring." << std::endl;
                return;
        }

        player->setWieldIndex(item_i);

        // Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
        ServerActiveObject *pointed_object = NULL;
        if (pointed.type == POINTEDTHING_OBJECT) {
                pointed_object = m_env->getActiveObject(pointed.object_id);
                if (pointed_object == NULL) {
                        verbosestream << "TOSERVER_INTERACT: "
                                "pointed object is NULL" << std::endl;
                        return;
                }

        }

        /*
                Make sure the player is allowed to do it
        */
        if (!checkPriv(player->getName(), "interact")) {
                actionstream << player->getName() << " attempted to interact with " <<
                                pointed.dump() << " without 'interact' privilege" << std::endl;

                if (pointed.type != POINTEDTHING_NODE)
                        return;

                // Re-send block to revert change on client-side
                RemoteClient *client = getClient(peer_id);
                // Digging completed -> under
                if (action == INTERACT_DIGGING_COMPLETED) {
                        v3s16 blockpos = getNodeBlockPos(pointed.node_undersurface);
                        client->SetBlockNotSent(blockpos);
                }
                // Placement -> above
                else if (action == INTERACT_PLACE) {
                        v3s16 blockpos = getNodeBlockPos(pointed.node_abovesurface);
                        client->SetBlockNotSent(blockpos);
                }
                return;
        }

        /*
                Check that target is reasonably close
        */
        static thread_local const u32 anticheat_flags =
                g_settings->getFlagStr("anticheat_flags", flagdesc_anticheat, nullptr);

        if ((action == INTERACT_START_DIGGING || action == INTERACT_DIGGING_COMPLETED ||
                        action == INTERACT_PLACE || action == INTERACT_USE) &&
                        (anticheat_flags & AC_INTERACTION) && !isSingleplayer()) {
                v3f target_pos = player_pos;
                if (pointed.type == POINTEDTHING_NODE) {
                        target_pos = intToFloat(pointed.node_undersurface, BS);
                } else if (pointed.type == POINTEDTHING_OBJECT) {
                        if (playersao->getId() == pointed_object->getId()) {
                                actionstream << "Server: " << player->getName()
                                        << " attempted to interact with themselves" << std::endl;
                                m_script->on_cheat(playersao, "interacted_with_self");
                                return;
                        }
                        target_pos = pointed_object->getBasePosition();
                }
                float d = playersao->getEyePosition().getDistanceFrom(target_pos);

                if (!checkInteractDistance(player, d, pointed.dump())) {
                        if (pointed.type == POINTEDTHING_NODE) {
                                // Re-send block to revert change on client-side
                                RemoteClient *client = getClient(peer_id);
                                v3s16 blockpos = getNodeBlockPos(pointed.node_undersurface);
                                client->SetBlockNotSent(blockpos);
                        }
                        return;
                }
        }

        /*
                If something goes wrong, this player is to blame
        */
        RollbackScopeActor rollback_scope(m_rollback,
                        "player:" + player->getName());

        switch (action) {
        // Start digging or punch object
        case INTERACT_START_DIGGING: {
                if (pointed.type == POINTEDTHING_NODE) {
                        MapNode n(CONTENT_IGNORE);
                        bool pos_ok;

                        v3s16 p_under = pointed.node_undersurface;
                        n = m_env->getMap().getNode(p_under, &pos_ok);
                        if (!pos_ok) {
                                infostream << "Server: Not punching: Node not found. "
                                        "Adding block to emerge queue." << std::endl;
                                m_emerge->enqueueBlockEmerge(peer_id,
                                        getNodeBlockPos(pointed.node_abovesurface), false);
                        }

                        if (n.getContent() != CONTENT_IGNORE)
                                m_script->node_on_punch(p_under, n, playersao, pointed);

                        // Cheat prevention
                        playersao->noCheatDigStart(p_under);

                        return;
                }

                // Skip if the object can't be interacted with anymore
                if (pointed.type != POINTEDTHING_OBJECT || pointed_object->isGone())
                        return;

                ItemStack selected_item, hand_item;
                ItemStack tool_item = playersao->getWieldedItem(&selected_item, &hand_item);
                const ToolCapabilities &toolcap =
                                tool_item.getToolCapabilities(m_itemdef, &hand_item);
                v3f dir = (pointed_object->getBasePosition() -
                                (playersao->getBasePosition() + playersao->getEyeOffset())
                                        ).normalize();
                float time_from_last_punch =
                        playersao->resetTimeFromLastPunch();

                u32 wear = pointed_object->punch(dir, toolcap, playersao,
                                time_from_last_punch, tool_item.wear);

                // Callback may have changed item, so get it again
                playersao->getWieldedItem(&selected_item);
                bool changed = selected_item.addWear(wear, m_itemdef);
                if (changed)
                        playersao->setWieldedItem(selected_item);

                return;
        } // action == INTERACT_START_DIGGING

        case INTERACT_STOP_DIGGING:
                // Nothing to do
                return;

        case INTERACT_DIGGING_COMPLETED: {
                // Only digging of nodes
                if (pointed.type != POINTEDTHING_NODE)
                        return;
                bool pos_ok;
                v3s16 p_under = pointed.node_undersurface;
                MapNode n = m_env->getMap().getNode(p_under, &pos_ok);
                if (!pos_ok) {
                        infostream << "Server: Not finishing digging: Node not found. "
                                "Adding block to emerge queue." << std::endl;
                        m_emerge->enqueueBlockEmerge(peer_id,
                                getNodeBlockPos(pointed.node_abovesurface), false);
                }

                /* Cheat prevention */
                bool is_valid_dig = true;
                if ((anticheat_flags & AC_DIGGING) && !isSingleplayer()) {
                        v3s16 nocheat_p = playersao->getNoCheatDigPos();
                        float nocheat_t = playersao->getNoCheatDigTime();
                        playersao->noCheatDigEnd();
                        // If player didn't start digging this, ignore dig
                        if (nocheat_p != p_under) {
                                infostream << "Server: " << player->getName()
                                                << " started digging "
                                                << nocheat_p << " and completed digging "
                                                << p_under << "; not digging." << std::endl;
                                is_valid_dig = false;
                                // Call callbacks
                                m_script->on_cheat(playersao, "finished_unknown_dig");
                        }

                        // Get player's wielded item
                        // See also: Game::handleDigging
                        ItemStack selected_item, hand_item;
                        ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

                        // Get diggability and expected digging time
                        DigParams params = getDigParams(m_nodedef->get(n).groups,
                                        &tool_item.getToolCapabilities(m_itemdef, &hand_item),
                                        tool_item.wear);
                        // If can't dig, try hand
                        if (!params.diggable) {
                                params = getDigParams(m_nodedef->get(n).groups,
                                        &hand_item.getToolCapabilities(m_itemdef));
                        }
                        // If can't dig, ignore dig
                        if (!params.diggable) {
                                infostream << "Server: " << player->getName()
                                                << " completed digging " << p_under
                                                << ", which is not diggable with tool; not digging."
                                                << std::endl;
                                is_valid_dig = false;
                                // Call callbacks
                                m_script->on_cheat(playersao, "dug_unbreakable");
                        }
                        // Check digging time
                        // If already invalidated, we don't have to
                        if (!is_valid_dig) {
                                // Well not our problem then
                        }
                        // Clean and long dig
                        else if (params.time > 2.0 && nocheat_t * 1.2 > params.time) {
                                // All is good, but grab time from pool; don't care if
                                // it's actually available
                                playersao->getDigPool().grab(params.time);
                        }
                        // Short or laggy dig
                        // Try getting the time from pool
                        else if (playersao->getDigPool().grab(params.time)) {
                                // All is good
                        }
                        // Dig not possible
                        else {
                                infostream << "Server: " << player->getName()
                                                << " completed digging " << p_under
                                                << "too fast; not digging." << std::endl;
                                is_valid_dig = false;
                                // Call callbacks
                                m_script->on_cheat(playersao, "dug_too_fast");
                        }
                }

                /* Actually dig node */

                if (is_valid_dig && n.getContent() != CONTENT_IGNORE)
                        m_script->node_on_dig(p_under, n, playersao);

                v3s16 blockpos = getNodeBlockPos(p_under);
                RemoteClient *client = getClient(peer_id);
                // Send unusual result (that is, node not being removed)
                if (m_env->getMap().getNode(p_under).getContent() != CONTENT_AIR)
                        // Re-send block to revert change on client-side
                        client->SetBlockNotSent(blockpos);
                else
                        client->ResendBlockIfOnWire(blockpos);

                return;
        } // action == INTERACT_DIGGING_COMPLETED

        // Place block or right-click object
        case INTERACT_PLACE: {
                std::optional<ItemStack> selected_item;
                getWieldedItem(playersao, selected_item);

                const bool had_prediction = !selected_item->getDefinition(m_itemdef).
                        node_placement_prediction.empty();

                if (pointed.type == POINTEDTHING_OBJECT) {
                        // Right click object

                        // Skip if object can't be interacted with anymore
                        if (pointed_object->isGone())
                                return;

                        actionstream << player->getName() << " right-clicks object "
                                        << pointed.object_id << ": "
                                        << pointed_object->getDescription() << std::endl;

                        // Do stuff
                        if (m_script->item_OnSecondaryUse(selected_item, playersao, pointed)) {
                                if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
                                        SendInventory(player, true);
                        }

                        // on_secondary_use might have removed the object
                        if (pointed_object->isGone())
                                return;

                        pointed_object->rightClick(playersao);
                } else if (m_script->item_OnPlace(selected_item, playersao, pointed)) {
                        // Placement was handled in lua

                        // Apply returned ItemStack
                        if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
                                SendInventory(player, true);
                }

                if (pointed.type != POINTEDTHING_NODE)
                        return;

                getClient(peer_id)->m_time_from_building = 0;

                // If item has node placement prediction, always send the
                // blocks to make sure the client knows what exactly happened
                RemoteClient *client = getClient(peer_id);
                v3s16 blockpos = getNodeBlockPos(pointed.node_abovesurface);
                v3s16 blockpos2 = getNodeBlockPos(pointed.node_undersurface);
                if (had_prediction) {
                        client->SetBlockNotSent(blockpos);
                        if (blockpos2 != blockpos)
                                client->SetBlockNotSent(blockpos2);
                } else {
                        client->ResendBlockIfOnWire(blockpos);
                        if (blockpos2 != blockpos)
                                client->ResendBlockIfOnWire(blockpos2);
                }

                return;
        } // action == INTERACT_PLACE

        case INTERACT_USE: {
                std::optional<ItemStack> selected_item;
                getWieldedItem(playersao, selected_item);

                actionstream << player->getName() << " uses " << selected_item->name
                                << ", pointing at " << pointed.dump() << std::endl;

                if (m_script->item_OnUse(selected_item, playersao, pointed)) {
                        // Apply returned ItemStack
                        if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
                                SendInventory(player, true);
                }

                return;
        }

        // Rightclick air
        case INTERACT_ACTIVATE: {
                std::optional<ItemStack> selected_item;
                getWieldedItem(playersao, selected_item);

                actionstream << player->getName() << " activates "
                                << selected_item->name << std::endl;

                pointed.type = POINTEDTHING_NOTHING; // can only ever be NOTHING

                if (m_script->item_OnSecondaryUse(selected_item, playersao, pointed)) {
                        // Apply returned ItemStack
                        if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
                                SendInventory(player, true);
                }

                return;
        }

        default:
                warningstream << "Server: Invalid action " << action << std::endl;

        }
}

void Server::handleCommand_RemovedSounds(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();

        // Batch 33: Validate client is in Active state before processing
        RemoteClient *client = getClientNoEx(peer_id, CS_Active);
        if (!client) {
                warningstream << FUNCTION_NAME
                        << ": ignoring from non-active client peer_id=" << peer_id << std::endl;
                return;
        }

        u16 num;
        *pkt >> num;

        // Batch 33: Limit count of removed sounds per packet to prevent DoS.
        // 1000 is far more than any reasonable client would need.
        constexpr u16 MAX_REMOVED_SOUNDS = 1000;
        if (num > MAX_REMOVED_SOUNDS) {
                warningstream << "Server: Client " << getPlayerName(peer_id)
                        << " sent too many removed sounds (" << num
                        << "), limiting to " << MAX_REMOVED_SOUNDS << std::endl;
                num = MAX_REMOVED_SOUNDS;
        }

        for (u16 k = 0; k < num; k++) {
                s32 id;

                *pkt >> id;

                auto i = m_playing_sounds.find(id);
                if (i == m_playing_sounds.end())
                        continue;

                ServerPlayingSound &psound = i->second;
                psound.clients.erase(pkt->getPeerId());
                if (psound.clients.empty())
                        m_playing_sounds.erase(i);
        }
}

static bool pkt_read_formspec_fields(NetworkPacket *pkt, StringMap &fields)
{
        u16 field_count;
        *pkt >> field_count;

        // Batch 33: Limit the number of formspec fields to prevent DoS.
        // 500 fields is far more than any reasonable formspec would need.
        constexpr u16 MAX_FORMSPEC_FIELDS = 500;
        if (field_count > MAX_FORMSPEC_FIELDS) {
                warningstream << "pkt_read_formspec_fields: too many fields ("
                        << field_count << "), limiting to " << MAX_FORMSPEC_FIELDS << std::endl;
                field_count = MAX_FORMSPEC_FIELDS;
        }

        size_t length = 0;
        for (u16 k = 0; k < field_count; k++) {
                std::string fieldname, fieldvalue;
                *pkt >> fieldname;
                fieldvalue = pkt->readLongString();

                fieldname = sanitize_untrusted(fieldname, false);
                // We'd love to strip escapes here but some formspec elements reflect data
                // from the server (e.g. dropdown), which can contain translations.
                fieldvalue = sanitize_untrusted(fieldvalue);

                length += fieldname.size() + fieldvalue.size();

                fields[std::move(fieldname)] = std::move(fieldvalue);
        }

        // 640K ought to be enough for anyone
        return length < 640 * 1024;
}

void Server::handleCommand_NodeMetaFields(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();

        // Batch 33: Rate limit formspec submissions to prevent server overload.
        // Max 5 node meta field submissions per 2 seconds per player.
        {
                static std::unordered_map<session_t, std::pair<u32, u64>> nodemeta_rate_limit;
                u64 now_s = porting::getTimeS();
                auto &rl = nodemeta_rate_limit[peer_id];
                if (now_s - rl.second > 2) {
                        rl.first = 0;
                        rl.second = now_s;
                }
                rl.first++;
                if (rl.first > 5) {
                        warningstream << "Server: Rate limiting nodemeta fields for peer_id="
                                << peer_id << " (" << rl.first << " in 2s)" << std::endl;
                        return;
                }
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player) {
                warningstream << FUNCTION_NAME << ": player is null" << std::endl;
                return;
        }

        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao) {
                warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
                return;
        }

        v3s16 p;
        std::string formname;
        StringMap fields;

        *pkt >> p >> formname;

        if (!pkt_read_formspec_fields(pkt, fields)) {
                warningstream << "Too large formspec fields! Ignoring for pos="
                        << p << ", player=" << player->getName() << std::endl;
                return;
        }

        // If something goes wrong, this player is to blame
        RollbackScopeActor rollback_scope(m_rollback,
                        "player:" + player->getName());

        // Check the target node for rollback data; leave others unnoticed
        RollbackNode rn_old(&m_env->getMap(), p, this);

        m_script->node_on_receive_fields(p, formname, fields, playersao);

        // Report rollback data
        RollbackNode rn_new(&m_env->getMap(), p, this);
        if (rollback() && rn_new != rn_old) {
                RollbackAction action;
                action.setSetNode(p, rn_old, rn_new);
                rollback()->reportAction(action);
        }
}

void Server::handleCommand_InventoryFields(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();

        // Batch 33: Rate limit inventory formspec submissions to prevent server overload.
        // Max 5 inventory field submissions per 2 seconds per player.
        {
                static std::unordered_map<session_t, std::pair<u32, u64>> invfields_rate_limit;
                u64 now_s = porting::getTimeS();
                auto &rl = invfields_rate_limit[peer_id];
                if (now_s - rl.second > 2) {
                        rl.first = 0;
                        rl.second = now_s;
                }
                rl.first++;
                if (rl.first > 5) {
                        warningstream << "Server: Rate limiting inventory fields for peer_id="
                                << peer_id << " (" << rl.first << " in 2s)" << std::endl;
                        return;
                }
        }

        RemotePlayer *player = m_env->getPlayer(peer_id);

        if (!player)
                return;
        PlayerSAO *playersao = player->getPlayerSAO();
        if (!playersao)
                return;

        std::string client_formspec_name;
        StringMap fields;

        *pkt >> client_formspec_name;

        if (!pkt_read_formspec_fields(pkt, fields)) {
                warningstream << "Too large formspec fields! Ignoring for formname=\""
                        << client_formspec_name << "\", player=" << player->getName() << std::endl;
                return;
        }

        if (client_formspec_name.empty()) { // pass through inventory submits
                m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
                return;
        }

        // verify that we displayed the formspec to the user
        const auto it = m_formspec_state_data.find(peer_id);
        if (it != m_formspec_state_data.end()) {
                const auto &server_formspec_name = it->second;
                if (client_formspec_name == server_formspec_name) {
                        // delete state if formspec was closed
                        auto it2 = fields.find("quit");
                        if (it2 != fields.end() && it2->second == "true")
                                m_formspec_state_data.erase(it);

                        m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
                        return;
                }
                actionstream << player->getName()
                        << " submitted formspec ('" << client_formspec_name
                        << "') but the name of the formspec doesn't match the"
                        " expected name ('" << server_formspec_name << "')";

        } else {
                actionstream << player->getName()
                        << " submitted formspec ('" << client_formspec_name
                        << "') but server hasn't sent formspec to client";
        }
        actionstream << ", possible exploitation attempt" << std::endl;
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

        // Use readBinaryString to avoid UTF-8 validation corrupting SRP binary data
        salt = pkt->readBinaryString();
        verification_key = pkt->readBinaryString();
        *pkt >> is_empty;

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

        // Use readBinaryString to avoid UTF-8 validation corrupting SRP binary data
        std::string bytes_A = pkt->readBinaryString();
        u8 based_on;
        *pkt >> based_on;

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
                !client->enc_pwd.empty() && // Batch 29: use !empty() instead of size()>0
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

        // Use readBinaryString to avoid UTF-8 validation corrupting SRP binary data
        std::string bytes_M = pkt->readBinaryString();

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

                        // v9.19 FIX: Use UpdatePeerECDHKeypair instead of SetPeerEncryptionState.
                        // A full SetPeerEncryptionState would overwrite ALL fields in the
                        // connection layer's encryption state (including the SRP-derived keys
                        // that were pushed earlier). If client->encryption_state has been
                        // reset (e.g., due to RemoteClient re-creation after acceptAuth),
                        // the full replace would clobber good keys with all-zeros.
                        // UpdatePeerECDHKeypair ONLY writes the ecdh_private_key and
                        // ecdh_public_key fields, preserving the SRP-derived keys.
                        m_con->UpdatePeerECDHKeypair(peer_id,
                                server_kp.private_key, server_kp.public_key);

                        // v9.19-trace: Log the ECDH keypair we just pushed
                        enclog_trace("Server ECDH: stored ECDH keypair in connection layer (SRP keys PRESERVED)")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("server_pubkey_hex", EncLog::hexDump(server_kp.public_key.data(), X25519_PUBLIC_KEY_SIZE))
                                << std::endl;

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

/*
 * Mod channels
 */

void Server::handleCommand_ModChannelJoin(NetworkPacket *pkt)
{
        std::string channel_name;
        *pkt >> channel_name;

        session_t peer_id = pkt->getPeerId();
        NetworkPacket resp_pkt(TOCLIENT_MODCHANNEL_SIGNAL,
                1 + 2 + channel_name.size(), peer_id);

        // Send signal to client to notify join succeed or not
        if (g_settings->getBool("enable_mod_channels") &&
                        m_modchannel_mgr->joinChannel(channel_name, peer_id)) {
                resp_pkt << (u8) MODCHANNEL_SIGNAL_JOIN_OK;
                infostream << "Peer " << peer_id << " joined channel " <<
                        channel_name << std::endl;
        }
        else {
                resp_pkt << (u8)MODCHANNEL_SIGNAL_JOIN_FAILURE;
                infostream << "Peer " << peer_id << " tried to join channel " <<
                        channel_name << ", but was already registered." << std::endl;
        }
        resp_pkt << channel_name;
        Send(&resp_pkt);
}

void Server::handleCommand_ModChannelLeave(NetworkPacket *pkt)
{
        std::string channel_name;
        *pkt >> channel_name;

        session_t peer_id = pkt->getPeerId();
        NetworkPacket resp_pkt(TOCLIENT_MODCHANNEL_SIGNAL,
                1 + 2 + channel_name.size(), peer_id);

        // Send signal to client to notify join succeed or not
        if (g_settings->getBool("enable_mod_channels") &&
                        m_modchannel_mgr->leaveChannel(channel_name, peer_id)) {
                resp_pkt << (u8)MODCHANNEL_SIGNAL_LEAVE_OK;
                infostream << "Peer " << peer_id << " left channel " << channel_name <<
                        std::endl;
        } else {
                resp_pkt << (u8) MODCHANNEL_SIGNAL_LEAVE_FAILURE;
                infostream << "Peer " << peer_id << " left channel " << channel_name <<
                        ", but was not registered." << std::endl;
        }
        resp_pkt << channel_name;
        Send(&resp_pkt);
}

void Server::handleCommand_ModChannelMsg(NetworkPacket *pkt)
{
        std::string channel_name, channel_msg;
        *pkt >> channel_name >> channel_msg;

        session_t peer_id = pkt->getPeerId();

        // Batch 33: Rate limit mod channel messages to prevent flooding.
        // Max 10 messages per second per player.
        {
                static std::unordered_map<session_t, std::pair<u32, u64>> modchannel_rate_limit;
                u64 now_s = porting::getTimeS();
                auto &rl = modchannel_rate_limit[peer_id];
                if (now_s - rl.second > 1) {
                        rl.first = 0;
                        rl.second = now_s;
                }
                rl.first++;
                if (rl.first > 10) {
                        warningstream << "Server: Rate limiting mod channel messages for peer_id="
                                << peer_id << " (" << rl.first << " in 1s)" << std::endl;
                        return;
                }
        }

        verbosestream << "Mod channel message received from peer " << peer_id <<
                " on channel " << channel_name << " message: " << channel_msg <<
                std::endl;

        // If mod channels are not enabled, discard message
        if (!g_settings->getBool("enable_mod_channels")) {
                return;
        }

        // If channel not registered, signal it and ignore message
        if (!m_modchannel_mgr->channelRegistered(channel_name)) {
                NetworkPacket resp_pkt(TOCLIENT_MODCHANNEL_SIGNAL,
                        1 + 2 + channel_name.size(), peer_id);
                resp_pkt << (u8)MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED << channel_name;
                Send(&resp_pkt);
                return;
        }

        // NOTE: Mod channel messages from clients have no rate limiting or
        // content filtering. A malicious client could flood a mod channel.
        // Root cause: broadcastModChannelMessage() is called directly without
        // any checks on message frequency or content.
        // Proposed fix: (1) Per-player rate limit: track per-peer message counts
        // in a sliding window (e.g., max 10 messages/second per channel per player).
        // (2) Per-channel rate limit: cap total messages per channel per second.
        // (3) Content filter: optionally sanitize or reject messages containing
        // control characters or exceeding a size limit. The rate limiter could be
        // implemented similarly to the chat message rate limiting in
        // handleCommand_ChatMessage().

        broadcastModChannelMessage(channel_name, channel_msg, peer_id);
}

void Server::handleCommand_HaveMedia(NetworkPacket *pkt)
{
        std::vector<u32> tokens;
        u8 numtokens;

        *pkt >> numtokens;

        // Batch 33: Limit the number of media callback tokens per packet.
        // 255 is the max u8 value, but even 100 is far beyond typical use.
        constexpr u8 MAX_MEDIA_TOKENS = 100;
        if (numtokens > MAX_MEDIA_TOKENS) {
                warningstream << "Server: Client " << getPlayerName(pkt->getPeerId())
                        << " sent too many media tokens (" << (int)numtokens
                        << "), limiting to " << (int)MAX_MEDIA_TOKENS << std::endl;
                numtokens = MAX_MEDIA_TOKENS;
        }

        for (u16 i = 0; i < numtokens; i++) {
                u32 n;
                *pkt >> n;
                tokens.emplace_back(n);
        }

        const session_t peer_id = pkt->getPeerId();
        auto player = m_env->getPlayer(peer_id);

        for (const u32 token : tokens) {
                auto it = m_pending_dyn_media.find(token);
                if (it == m_pending_dyn_media.end())
                        continue;
                if (it->second.waiting_players.count(peer_id)) {
                        it->second.waiting_players.erase(peer_id);
                        if (player)
                                getScriptIface()->on_dynamic_media_added(token, player->getName());
                }
        }
}

void Server::handleCommand_UpdateClientInfo(NetworkPacket *pkt)
{
        ClientDynamicInfo info;
        *pkt >> info.render_target_size.X;
        *pkt >> info.render_target_size.Y;
        *pkt >> info.real_gui_scaling;
        *pkt >> info.real_hud_scaling;
        *pkt >> info.max_fs_size.X;
        *pkt >> info.max_fs_size.Y;
        info.touch_controls = false;

        if (pkt->hasRemainingBytes()) {
                // >= 5.9.0-dev
                *pkt >> info.touch_controls;
        }

        session_t peer_id = pkt->getPeerId();
        RemoteClient *client = getClient(peer_id, CS_Invalid);
        client->setDynamicInfo(info);
}

// v9.11: ECDH X25519 forward secrecy — receive client's public key
void Server::handleCommand_EcdhPubkey(NetworkPacket *pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemoteClient *client = getClient(peer_id, CS_Invalid);

        // Read client's X25519 public key (32 bytes)
        // Use readLongBinaryString to avoid UTF-8 validation corrupting the raw key bytes
        std::string pubkey_str = pkt->readLongBinaryString();

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

        // v9.19-trace: Log the client's public key we received
        enclog_trace("Server ECDH: received client public key")
                << EncLog::kv("peer", peer_id)
                << EncLog::kv("client_pubkey_hex", EncLog::hexDump(client_pubkey, X25519_PUBLIC_KEY_SIZE))
                << std::endl;

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

        // v9.19 FIX: Mix ECDH secret DIRECTLY into the connection layer's encryption
        // state (udpPeer->encryption_state), NOT into client->encryption_state.
        // The connection layer has the correct SRP-derived keys (pushed at line 1824),
        // while client->encryption_state may have been reset to all-zeros after
        // acceptAuth triggered a client state change.
        bool ok = m_con->MixECDHSecretOnPeer(peer_id,
                shared_secret.shared_secret.data(), shared_secret.shared_secret.size());
        if (!ok) {
                errorstream << "Server::handleCommand_EcdhPubkey: MixECDHSecretOnPeer failed for peer "
                        << peer_id << std::endl;
                return;
        }

        // v9.19-trace: Log server state BEFORE activation
        enclog_trace("Server ECDH: about to ACTIVATE encryption")
                << EncLog::kv("peer", peer_id)
                << EncLog::kv("shared_secret_fp", keyToFingerprint(shared_secret.shared_secret.data(), X25519_SHARED_SECRET_SIZE))
                << EncLog::kv("session_id", client->encryption_state.session_id)
                << EncLog::kv("ecdh_completed", client->encryption_state.ecdh_completed.load())
                << EncLog::kv("active_before", client->encryption_state.active.load())
                << std::endl;

        // Activate encryption with ECDH+SRP keys
        m_con->ActivatePeerEncryption(peer_id);

        infostream << "Server::handleCommand_EcdhPubkey: ECDH forward secrecy established for peer "
                << peer_id << " (session_id=" << client->encryption_state.session_id << ")" << std::endl;
}

void Server::handleCommand_KeypairRegister(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemoteClient *client = getClient(peer_id, CS_Invalid);
        ClientState cstate = client->getState();
        const std::string playername = client->getName();

        // Use readBinaryString to avoid UTF-8 validation corrupting the raw key bytes
        std::string public_key = pkt->readBinaryString();

        std::string addr_s = client->getAddress().serializeString();

        verbosestream << "Server: Got TOSERVER_KEYPAIR_REGISTER from " << addr_s
                << " (pubkey_len=" << public_key.size() << ")" << std::endl;

        if (cstate != CS_HelloSent) {
                infostream << "Server: Ignoring TOSERVER_KEYPAIR_REGISTER from "
                        << addr_s << ": Client has wrong state " << cstate << "." << std::endl;
                return;
        }

        if (!client->isMechAllowed(AUTH_MECHANISM_KEYPAIR)) {
                actionstream << "Server: Client from " << addr_s
                        << " tried to register keypair without being "
                        << "allowed to use keypair auth." << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
                return;
        }

        // Validate public key size
        if (public_key.size() != ED25519_PUBLIC_KEY_SIZE) {
                actionstream << "Server: " << playername
                        << " supplied invalid public key size from " << addr_s << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
                return;
        }

        // Store the public key as a keypair auth entry (#2# format)
        std::string encpwd = encode_keypair_pubkey(public_key);

        // Check if account already exists (race condition protection)
        if (m_script->getAuth(playername, nullptr, nullptr)) {
                actionstream << "Server: Client from " << addr_s
                        << " tried to register " << playername << " a second time."
                        << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
                return;
        }

        m_script->createAuth(playername, encpwd);
        client->setEncryptedPassword(encpwd);

        m_script->on_authplayer(playername, addr_s, true);

        infostream << "Server: Keypair registration accepted for " << playername
                << " from " << addr_s << std::endl;

        // v9.33: Derive encryption keys from keypair auth material.
        // For registration, the challenge is derived from the public key itself
        // (there was no challenge-response exchange for registration).
        // We use the public key as both challenge and "signature" since
        // registration is trust-on-first-use — the server has no prior key to verify.
        // This provides encryption with the same ECDH forward secrecy flow as SRP.
        bool encryption_initialized = false;
        if (EncryptionConfig::shouldEncrypt() && !isSingleplayer()) {
                // Use the public key as the shared material for key derivation
                const u8* challenge_data = reinterpret_cast<const u8*>(public_key.data());
                size_t challenge_data_len = public_key.size();
                const u8* sig_data = reinterpret_cast<const u8*>(public_key.data());
                size_t sig_data_len = public_key.size();

                bool ok = client->encryption_state.initFromKeypairAuth(
                        challenge_data, challenge_data_len,
                        sig_data, sig_data_len, true /* is_server */);
                if (ok) {
                        m_con->SetPeerEncryptionState(peer_id, client->encryption_state);
                        encryption_initialized = true;
                        enclog_init("Keypair registration: encryption keys derived (not yet active)")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("session_id", client->encryption_state.session_id)
                                << EncLog::kv("auth_method", "Ed25519")
                                << std::endl;
                } else {
                        enclog_error("Keypair registration: failed to derive encryption keys")
                                << EncLog::kv("peer", peer_id) << std::endl;
                        client->encryption_state.disable();
                }
        }

        // Send AUTH_ACCEPT as plaintext (client needs it to set up encryption)
        acceptAuth(peer_id, false);

        // v9.33: ECDH forward secrecy — same flow as SRP auth
        if (encryption_initialized) {
                X25519KeyPair server_kp = x25519_generate_keypair();
                if (server_kp.success) {
                        client->encryption_state.ecdh_private_key = server_kp.private_key;
                        client->encryption_state.ecdh_public_key = server_kp.public_key;
                        m_con->UpdatePeerECDHKeypair(peer_id,
                                server_kp.private_key, server_kp.public_key);

                        NetworkPacket ecdh_pkt(TOCLIENT_ECDH_PUBKEY, X25519_PUBLIC_KEY_SIZE, peer_id);
                        std::string pubkey_str(reinterpret_cast<const char*>(server_kp.public_key.data()),
                                X25519_PUBLIC_KEY_SIZE);
                        ecdh_pkt.putLongString(pubkey_str);
                        Send(&ecdh_pkt);

                        enclog_init("ECDH public key sent to peer after keypair registration")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("session_id", client->encryption_state.session_id)
                                << std::endl;
                }
        }
}

void Server::handleCommand_KeypairLogin(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemoteClient *client = getClient(peer_id, CS_Invalid);
        ClientState cstate = client->getState();
        const std::string playername = client->getName();

        std::string addr_s = client->getAddress().serializeString();

        verbosestream << "Server: Got TOSERVER_KEYPAIR_LOGIN from " << addr_s << std::endl;

        if (cstate != CS_HelloSent) {
                infostream << "Server: Ignoring TOSERVER_KEYPAIR_LOGIN from "
                        << addr_s << ": Client has wrong state " << cstate << "." << std::endl;
                return;
        }

        if (!client->isMechAllowed(AUTH_MECHANISM_KEYPAIR)) {
                actionstream << "Server: Client from " << addr_s
                        << " tried keypair login without being "
                        << "allowed to use keypair auth." << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
                return;
        }

        // Decode the stored public key from the password field
        std::string stored_pubkey;
        if (!decode_keypair_pubkey(client->enc_pwd, &stored_pubkey)) {
                actionstream << "Server: " << playername
                        << " has invalid keypair auth entry from " << addr_s << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
                return;
        }

        // Generate a challenge nonce and send it
        std::string nonce = KeypairManager::generateChallenge();
        if (nonce.empty()) {
                errorstream << "Server: Failed to generate keypair challenge nonce" << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
                return;
        }

        // Store the nonce in the client for later verification
        client->keypair_challenge_nonce = nonce;

        // Send the challenge
        NetworkPacket challenge_pkt(TOCLIENT_KEYPAIR_CHALLENGE, 0, peer_id);
        challenge_pkt << nonce;
        Send(&challenge_pkt);

        infostream << "Server: Sent TOCLIENT_KEYPAIR_CHALLENGE to " << playername
                << " from " << addr_s << std::endl;
}

void Server::handleCommand_KeypairResponse(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemoteClient *client = getClient(peer_id, CS_Invalid);
        const std::string playername = client->getName();

        // Use readBinaryString to avoid UTF-8 validation corrupting the raw signature bytes
        std::string signature = pkt->readBinaryString();

        std::string addr_s = client->getAddress().serializeString();

        verbosestream << "Server: Got TOSERVER_KEYPAIR_RESPONSE from " << addr_s
                << " (sig_len=" << signature.size() << ")" << std::endl;

        if (client->keypair_challenge_nonce.empty()) {
                actionstream << "Server: " << playername
                        << " sent keypair response without a challenge from " << addr_s << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
                return;
        }

        // Decode the stored public key
        std::string stored_pubkey;
        if (!decode_keypair_pubkey(client->enc_pwd, &stored_pubkey)) {
                actionstream << "Server: " << playername
                        << " has invalid keypair auth entry from " << addr_s << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
                return;
        }

        // Verify the signature against the challenge nonce
        bool valid = KeypairManager::verify(stored_pubkey,
                client->keypair_challenge_nonce, signature);

        // v9.33: Save the challenge for encryption key derivation BEFORE clearing
        std::string saved_challenge = client->keypair_challenge_nonce;

        // Clear the challenge nonce (one-time use)
        client->keypair_challenge_nonce.clear();

        if (!valid) {
                actionstream << "Server: " << playername
                        << " failed keypair authentication from " << addr_s << std::endl;
                DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_PASSWORD);
                return;
        }

        m_script->on_authplayer(playername, addr_s, true);

        infostream << "Server: Keypair login accepted for " << playername
                << " from " << addr_s << std::endl;

        // v9.33: Derive encryption keys from the challenge+signature material.
        // Both server and client can compute SHA-256(challenge || signature),
        // providing a shared secret that an eavesdropper cannot derive.
        bool encryption_initialized = false;
        if (EncryptionConfig::shouldEncrypt() && !isSingleplayer()) {
                bool ok = client->encryption_state.initFromKeypairAuth(
                        reinterpret_cast<const u8*>(saved_challenge.data()),
                        saved_challenge.size(),
                        reinterpret_cast<const u8*>(signature.data()),
                        signature.size(), true /* is_server */);
                if (ok) {
                        m_con->SetPeerEncryptionState(peer_id, client->encryption_state);
                        encryption_initialized = true;
                        enclog_init("Keypair login: encryption keys derived (not yet active)")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("session_id", client->encryption_state.session_id)
                                << EncLog::kv("auth_method", "Ed25519")
                                << std::endl;
                } else {
                        enclog_error("Keypair login: failed to derive encryption keys")
                                << EncLog::kv("peer", peer_id) << std::endl;
                        client->encryption_state.disable();
                }
        }

        // Send AUTH_ACCEPT as plaintext (client needs it to set up encryption)
        acceptAuth(peer_id, false);

        // v9.33: ECDH forward secrecy — same flow as SRP auth
        if (encryption_initialized) {
                X25519KeyPair server_kp = x25519_generate_keypair();
                if (server_kp.success) {
                        client->encryption_state.ecdh_private_key = server_kp.private_key;
                        client->encryption_state.ecdh_public_key = server_kp.public_key;
                        m_con->UpdatePeerECDHKeypair(peer_id,
                                server_kp.private_key, server_kp.public_key);

                        NetworkPacket ecdh_pkt(TOCLIENT_ECDH_PUBKEY, X25519_PUBLIC_KEY_SIZE, peer_id);
                        std::string pubkey_str(reinterpret_cast<const char*>(server_kp.public_key.data()),
                                X25519_PUBLIC_KEY_SIZE);
                        ecdh_pkt.putLongString(pubkey_str);
                        Send(&ecdh_pkt);

                        enclog_init("ECDH public key sent to peer after keypair login")
                                << EncLog::kv("peer", peer_id)
                                << EncLog::kv("session_id", client->encryption_state.session_id)
                                << std::endl;
                }
        }
}

// ============================================================
// v9.39 Voice Chat Server Handlers
// ============================================================

void Server::sendVoicePeerListToAll()
{
        // Build the peer list of all voice-enabled players
        std::vector<std::pair<u16, RemotePlayer*>> voice_peers;
        for (RemotePlayer *p : m_env->getPlayers()) {
                if (p && p->voice_chat_enabled)
                        voice_peers.emplace_back(p->getPeerId(), p);
        }

        // Send to each voice-enabled player
        for (RemotePlayer *target : m_env->getPlayers()) {
                if (!target || !target->voice_chat_enabled)
                        continue;

                NetworkPacket list_pkt(TOCLIENT_VOICE_PEER_LIST, 0, target->getPeerId());
                list_pkt << (u16)voice_peers.size();
                for (auto &vp : voice_peers) {
                        list_pkt << vp.first;
                        list_pkt << (u8)(vp.second->voice_chat_enabled ? 1 : 0);
                        list_pkt << (u8)0; // is_muted_by_us — client-side, always 0 from server
                        list_pkt << (u8)(vp.second->voice_is_talking ? 1 : 0);
                        list_pkt << vp.second->getName();
                }
                Send(&list_pkt);
        }
}

void Server::sendVoiceGroupUpdate(u32 group_id, u8 update_type)
{
        auto it = m_voice_groups.find(group_id);
        if (it == m_voice_groups.end())
                return;

        auto &group = it->second;

        // Build per-peer packets
        // Send to all group members
        for (u16 member_peer_id : group.members) {
                NetworkPacket update_pkt(TOCLIENT_VOICE_GROUP_UPDATE, 0, member_peer_id);
                update_pkt << group_id << update_type << (u16)group.members.size();
                for (u16 mid : group.members) {
                        RemotePlayer *member = m_env->getPlayer(mid);
                        std::string name = member ? member->getName() : "Unknown";
                        update_pkt << mid << name;
                }
                Send(&update_pkt);
        }
}

void Server::handleCommand_VoiceEnable(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player)
                return;

        u8 enabled;
        *pkt >> enabled;

        player->voice_chat_enabled = (enabled != 0);

        infostream << "Server: Player " << player->getName()
                   << " voice chat " << (enabled ? "enabled" : "disabled") << std::endl;

        // Broadcast updated peer list to all voice-enabled players
        sendVoicePeerListToAll();
}

void Server::handleCommand_VoiceStart(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player || !player->voice_chat_enabled)
                return;

        u8 channel_id;
        *pkt >> channel_id;

        player->voice_is_talking = true;
        player->voice_channel = channel_id;

        // Relay to all other voice-enabled players
        // Send to all connected voice-enabled peers (server-wide, no distance fade)
        for (RemotePlayer *other : m_env->getPlayers()) {
                if (other && other != player && other->voice_chat_enabled) {
                        NetworkPacket start_pkt(TOCLIENT_VOICE_PEER_START, 0, other->getPeerId());
                        start_pkt << peer_id << channel_id << player->getName();
                        Send(&start_pkt);
                }
        }
}

void Server::handleCommand_VoiceData(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player || !player->voice_chat_enabled || !player->voice_is_talking)
                return;

        u8 channel_id;
        u16 seq_num;
        u16 data_length;

        *pkt >> channel_id >> seq_num >> data_length;

        // Batch 33: Limit voice data size to prevent bandwidth amplification attacks.
        // Opus frames are typically 100-1000 bytes; 4000 bytes is very generous.
        constexpr u16 MAX_VOICE_DATA_LENGTH = 4000;
        if (data_length > MAX_VOICE_DATA_LENGTH) {
                warningstream << "Server: Voice data too large (" << data_length
                        << " bytes) from peer_id=" << peer_id << ", dropping" << std::endl;
                return;
        }

        // Read the raw opus data
        std::string raw_data = pkt->readRawString(data_length);

        // Check for E2EE nonce after opus data
        std::string nonce;
        if (pkt->getRemainingBytes() >= 12) {
                nonce = pkt->readRawString(12);
        }

        // Relay voice data to all other voice-enabled players
        // The server does NOT decrypt — it only relays encrypted data
        for (RemotePlayer *other : m_env->getPlayers()) {
                if (other && other != player && other->voice_chat_enabled) {
                        NetworkPacket relay_pkt(TOCLIENT_VOICE_DATA, 0, other->getPeerId());
                        relay_pkt << peer_id << channel_id << seq_num << data_length;
                        relay_pkt.putRawString(raw_data.c_str(), raw_data.size());
                        if (!nonce.empty()) {
                                relay_pkt.putRawString(nonce.c_str(), nonce.size());
                        }
                        Send(&relay_pkt);
                }
        }
}

void Server::handleCommand_VoiceStop(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player)
                return;

        player->voice_is_talking = false;

        // Relay stop to all voice-enabled players
        for (RemotePlayer *other : m_env->getPlayers()) {
                if (other && other != player && other->voice_chat_enabled) {
                        NetworkPacket stop_pkt(TOCLIENT_VOICE_PEER_STOP, 0, other->getPeerId());
                        stop_pkt << peer_id;
                        Send(&stop_pkt);
                }
        }
}

void Server::handleCommand_VoiceMute(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player)
                return;

        u16 target_peer_id;
        u8 muted;
        *pkt >> target_peer_id >> muted;

        // Muting is client-side only — the server doesn't need to do anything
        // except optionally log it for moderation purposes
        infostream << "Server: Player " << player->getName()
                   << (muted ? " muted" : " unmuted") << " peer " << target_peer_id << std::endl;
}

void Server::handleCommand_VoiceGroupCreate(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player || !player->voice_chat_enabled)
                return;

        std::string group_name;
        *pkt >> group_name;

        // Check max groups limit
        // Batch 36: Clamp voice_chat_max_groups to [1, 100] — use getU16 for unsigned semantics
        int max_groups = rangelim(g_settings->getU16("voice_chat_max_groups"), 1, 100);
        if ((int)m_voice_groups.size() >= max_groups) {
                infostream << "Server: Voice group limit reached (" << max_groups << ")" << std::endl;
                return;
        }

        // Create the group
        u32 group_id = m_voice_group_next_id++;
        VoiceGroupState &group = m_voice_groups[group_id];
        group.group_id = group_id;
        group.name = group_name;
        group.owner_peer_id = peer_id;
        group.members.push_back(peer_id);
        group.active = true;

        infostream << "Server: Player " << player->getName()
                   << " created voice group '" << group_name
                   << "' (id=" << group_id << ")" << std::endl;

        // Send group update to the creator
        sendVoiceGroupUpdate(group_id, 0); // 0 = created
}

void Server::handleCommand_VoiceGroupInvite(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player || !player->voice_chat_enabled)
                return;

        u32 group_id;
        u16 target_peer_id;
        *pkt >> group_id >> target_peer_id;

        // Check group exists and inviter is the owner
        auto it = m_voice_groups.find(group_id);
        if (it == m_voice_groups.end() || it->second.owner_peer_id != peer_id)
                return;

        RemotePlayer *target = m_env->getPlayer(target_peer_id);
        if (!target || !target->voice_chat_enabled)
                return;

        // Check max members limit
        // Batch 36: Clamp voice_chat_group_max_members to [1, 64] — use getU16 for unsigned semantics
        int max_members = rangelim(g_settings->getU16("voice_chat_group_max_members"), 1, 64);
        if ((int)it->second.members.size() >= max_members)
                return;

        // Send invite to target
        NetworkPacket invite_pkt(TOCLIENT_VOICE_GROUP_INVITE, 0, target_peer_id);
        invite_pkt << group_id << it->second.name << peer_id << player->getName();
        Send(&invite_pkt);

        infostream << "Server: Player " << player->getName()
                   << " invited " << target->getName() << " to voice group " << group_id << std::endl;
}

void Server::handleCommand_VoiceGroupJoin(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player || !player->voice_chat_enabled)
                return;

        u32 group_id;
        *pkt >> group_id;

        auto it = m_voice_groups.find(group_id);
        if (it == m_voice_groups.end() || !it->second.active)
                return;

        // Check max members limit
        // Batch 36: Clamp voice_chat_group_max_members to [1, 64] — use getU16 for unsigned semantics
        int max_members = rangelim(g_settings->getU16("voice_chat_group_max_members"), 1, 64);
        if ((int)it->second.members.size() >= max_members)
                return;

        // Add player to group if not already a member
        auto &members = it->second.members;
        if (std::find(members.begin(), members.end(), peer_id) == members.end()) {
                members.push_back(peer_id);
        }

        infostream << "Server: Player " << player->getName()
                   << " joined voice group " << group_id << std::endl;

        // Notify all group members
        sendVoiceGroupUpdate(group_id, 1); // 1 = member_joined
}

void Server::handleCommand_VoiceGroupLeave(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player)
                return;

        u32 group_id;
        *pkt >> group_id;

        auto it = m_voice_groups.find(group_id);
        if (it == m_voice_groups.end())
                return;

        // Remove player from group
        auto &members = it->second.members;
        members.erase(std::remove(members.begin(), members.end(), peer_id), members.end());

        infostream << "Server: Player " << player->getName()
                   << " left voice group " << group_id << std::endl;

        // If the owner left or no members remain, disband the group
        if (it->second.owner_peer_id == peer_id || members.empty()) {
                // Notify remaining members before removing
                sendVoiceGroupUpdate(group_id, 3); // 3 = disbanded
                m_voice_groups.erase(it);
        } else {
                // Notify remaining members
                sendVoiceGroupUpdate(group_id, 2); // 2 = member_left
        }
}

void Server::handleCommand_VoiceKeyExchange(NetworkPacket* pkt)
{
        session_t peer_id = pkt->getPeerId();
        RemotePlayer *player = m_env->getPlayer(peer_id);
        if (!player || !player->voice_chat_enabled)
                return;

        // Read the 32-byte X25519 public key
        std::string pubkey_str = pkt->readRawString(32);

        player->voice_pubkey.assign(pubkey_str.begin(), pubkey_str.end());

        infostream << "Server: Player " << player->getName()
                   << " sent voice E2EE public key" << std::endl;

        // Relay this public key to all other voice-enabled players
        for (RemotePlayer *other : m_env->getPlayers()) {
                if (other && other != player && other->voice_chat_enabled) {
                        NetworkPacket exchange_pkt(TOCLIENT_VOICE_KEY_EXCHANGE, 0, other->getPeerId());
                        exchange_pkt << peer_id;
                        exchange_pkt.putRawString(pubkey_str.c_str(), 32);
                        Send(&exchange_pkt);
                }
        }

        // Also send existing peers' keys to this new player
        for (RemotePlayer *other : m_env->getPlayers()) {
                if (other && other != player && other->voice_chat_enabled
                        && other->voice_pubkey.size() == 32) {
                        NetworkPacket existing_pkt(TOCLIENT_VOICE_KEY_EXCHANGE, 0, peer_id);
                        existing_pkt << other->getPeerId();
                        existing_pkt.putRawString(
                                reinterpret_cast<const char*>(other->voice_pubkey.data()), 32);
                        Send(&existing_pkt);
                }
        }
}
