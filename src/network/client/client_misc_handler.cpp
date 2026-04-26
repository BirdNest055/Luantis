// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

// This file contains miscellaneous packet handlers for Client.
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

void Client::handleCommand_Deprecated(NetworkPacket* pkt)
{
	infostream << "Got deprecated command "
			<< toClientCommandTable[pkt->getCommand()].name << " from peer "
			<< pkt->getPeerId() << "!" << std::endl;
}

void Client::handleCommand_TimeOfDay(NetworkPacket* pkt)
{
	if (pkt->getSize() < 2)
		return;

	u16 time_of_day;
	*pkt >> time_of_day;
	time_of_day      = time_of_day % 24000;

	float time_speed;
	*pkt >> time_speed;

	// Update environment
	m_env.setTimeOfDay(time_of_day);
	m_env.setTimeOfDaySpeed(time_speed);
}

void Client::handleCommand_ChatMessage(NetworkPacket *pkt)
{
	/*
		u8 version
		u8 message_type
		u16 sendername length
		wstring sendername
		u16 length
		wstring message
	 */

	ChatMessage *chatMessage = new ChatMessage();
	u8 version, message_type;
	*pkt >> version >> message_type;

	if (version != 1 || message_type >= CHATMESSAGE_TYPE_MAX) {
		delete chatMessage;
		return;
	}

	u64 timestamp;
	*pkt >> chatMessage->sender >> chatMessage->message >> timestamp;
	chatMessage->timestamp = static_cast<std::time_t>(timestamp);

	chatMessage->type = (ChatMessageType) message_type;

	// log the chat message
	actionstream << "CHAT: " << wide_to_utf8(unescape_enriched(chatMessage->message)) << std::endl;

	// @TODO send this to CSM using ChatMessage object
	if (modsLoaded() && m_script->on_receiving_message(
			wide_to_utf8(chatMessage->message))) {
		// Message was consumed by CSM and should not be handled by client
		delete chatMessage;
	} else {
		pushToChatQueue(chatMessage);
	}
}

void Client::handleCommand_UpdatePlayerList(NetworkPacket* pkt)
{
	u8 type;
	u16 num_players;
	*pkt >> type >> num_players;
	PlayerListModifer notice_type = (PlayerListModifer) type;

	for (u16 i = 0; i < num_players; i++) {
		std::string name;
		*pkt >> name;
		switch (notice_type) {
		case PLAYER_LIST_INIT:
		case PLAYER_LIST_ADD:
			m_env.addPlayerName(name);
			continue;
		case PLAYER_LIST_REMOVE:
			m_env.removePlayerName(name);
			continue;
		}
	}
}

void Client::handleCommand_FormspecPrepend(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	// Store formspec in LocalPlayer
	*pkt >> player->formspec_prepend;
}

void Client::handleCommand_CSMRestrictionFlags(NetworkPacket *pkt)
{
	*pkt >> m_csm_restriction_flags >> m_csm_restriction_noderange;

	// Restrictions were received -> load mods if it's enabled
	// Note: this should be moved after mods receptions from server instead
	loadMods();
}

void Client::handleCommand_ModChannelMsg(NetworkPacket *pkt)
{
	std::string channel_name, sender, channel_msg;
	*pkt >> channel_name >> sender >> channel_msg;

	verbosestream << "Mod channel message received from server " << pkt->getPeerId()
		<< " on channel " << channel_name << ". sender: `" << sender << "`, message: "
		<< channel_msg << std::endl;

	if (!m_modchannel_mgr->channelRegistered(channel_name)) {
		verbosestream << "Server sent us messages on unregistered channel "
			<< channel_name << ", ignoring." << std::endl;
		return;
	}

	m_script->on_modchannel_message(channel_name, sender, channel_msg);
}

void Client::handleCommand_ModChannelSignal(NetworkPacket *pkt)
{
	u8 signal_tmp;
	ModChannelSignal signal;
	std::string channel;

	*pkt >> signal_tmp >> channel;

	signal = (ModChannelSignal)signal_tmp;

	bool valid_signal = true;
	// @TODO: send Signal to Lua API
	switch (signal) {
		case MODCHANNEL_SIGNAL_JOIN_OK:
			m_modchannel_mgr->setChannelState(channel, MODCHANNEL_STATE_READ_WRITE);
			infostream << "Server ack our mod channel join on channel `" << channel
				<< "`, joining." << std::endl;
			break;
		case MODCHANNEL_SIGNAL_JOIN_FAILURE:
			// Unable to join, remove channel
			m_modchannel_mgr->leaveChannel(channel, 0);
			infostream << "Server refused our mod channel join on channel `" << channel
				<< "`" << std::endl;
			break;
		case MODCHANNEL_SIGNAL_LEAVE_OK:
#ifndef NDEBUG
			infostream << "Server ack our mod channel leave on channel " << channel
				<< "`, leaving." << std::endl;
#endif
			break;
		case MODCHANNEL_SIGNAL_LEAVE_FAILURE:
			infostream << "Server refused our mod channel leave on channel `" << channel
				<< "`" << std::endl;
			break;
		case MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED:
#ifndef NDEBUG
			// Generally unused, but ensure we don't do an implementation error
			infostream << "Server tells us we sent a message on channel `" << channel
				<< "` but we are not registered. Message was dropped." << std::endl;
#endif
			break;
		case MODCHANNEL_SIGNAL_SET_STATE: {
			u8 state;
			*pkt >> state;

			if (state == MODCHANNEL_STATE_INIT || state >= MODCHANNEL_STATE_MAX) {
				infostream << "Received wrong channel state " << state
						<< ", ignoring." << std::endl;
				return;
			}

			m_modchannel_mgr->setChannelState(channel, (ModChannelState) state);
			infostream << "Server sets mod channel `" << channel
					<< "` in read-only mode." << std::endl;
			break;
		}
		default:
#ifndef NDEBUG
			warningstream << "Received unhandled mod channel signal ID "
				<< signal << ", ignoring." << std::endl;
#endif
			valid_signal = false;
			break;
	}

	// If signal is valid, forward it to client side mods
	if (valid_signal)
		m_script->on_modchannel_signal(channel, signal);
}
