// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

/*
 * Server packet handler — Miscellaneous methods
 *
 * handleCommand_Deprecated, handleCommand_RequestMedia, handleCommand_GotBlocks,
 * handleCommand_PlayerPos, handleCommand_DeletedBlocks, handleCommand_RemovedSounds,
 * handleCommand_ModChannelJoin, handleCommand_ModChannelLeave, handleCommand_ModChannelMsg,
 * handleCommand_HaveMedia, handleCommand_UpdateClientInfo
 *
 * Helper functions: process_PlayerPos, pkt_read_formspec_fields
 */

#include "server.h"
#include "serverenvironment.h"
#include "log.h"
#include "modchannels.h"
#include "remoteplayer.h"
#include "scripting_server.h"
#include "settings.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "server/player_sao.h"
#include "clientdynamicinfo.h"

#include <algorithm>

void Server::handleCommand_Deprecated(NetworkPacket* pkt)
{
	auto &h = toServerCommandTable[pkt->getCommand()];
	infostream << "Server: ignoring unsupported " << h.name << " from peer " <<
		pkt->getPeerId() << std::endl;
}

void Server::handleCommand_RequestMedia(NetworkPacket* pkt)
{
	std::unordered_set<std::string> tosend;
	u16 numfiles;

	*pkt >> numfiles;

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

	ClientInterface::AutoLock lock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(pkt->getPeerId());
	if (!client)
		return;

	for (u16 i = 0; i < count; i++) {
		v3s16 p;
		*pkt >> p;
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

void Server::handleCommand_RemovedSounds(NetworkPacket* pkt)
{
	u16 num;
	*pkt >> num;
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

	// @TODO: filter, rate limit

	broadcastModChannelMessage(channel_name, channel_msg, peer_id);
}

void Server::handleCommand_HaveMedia(NetworkPacket *pkt)
{
	std::vector<u32> tokens;
	u8 numtokens;

	*pkt >> numtokens;
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
