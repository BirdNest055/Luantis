// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

// This file contains media/asset-related packet handlers for Client.
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

void Client::handleCommand_ActiveObjectRemoveAdd(NetworkPacket* pkt)
{
	/*
		u16 count of removed objects
		for all removed objects {
			u16 id
		}
		u16 count of added objects
		for all added objects {
			u16 id
			u8 type
			u32 initialization data length
			string initialization data
		}
	*/

	do {
		u8 type;
		u16 removed_count, added_count, id;

		// Read removed objects
		*pkt >> removed_count;

		for (u16 i = 0; i < removed_count; i++) {
			*pkt >> id;
			m_env.removeActiveObject(id);
			// Object-attached sounds MUST NOT be removed here because they might
			// have started to play immediately before the entity was removed.
		}

		// Read added objects
		*pkt >> added_count;

		for (u16 i = 0; i < added_count; i++) {
			*pkt >> id >> type;
			m_env.addActiveObject(id, type, pkt->readLongString());
		}
	} while (0);

	// m_activeobjects_received is false before the first
	// TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD packet is received
	m_activeobjects_received = true;
}

void Client::handleCommand_ActiveObjectMessages(NetworkPacket* pkt)
{
	/*
		for all objects
		{
			u16 id
			u16 message length
			string message
		}
	*/
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	while (canRead(is)) {
		u16 id = readU16(is);
		std::string message = deSerializeString16(is);

		// Pass on to the environment
		m_env.processActiveObjectMessage(id, message);
	}
}

void Client::handleCommand_AnnounceMedia(NetworkPacket* pkt)
{
	infostream << "Client: Received media announcement: packet size: "
			<< pkt->getSize() << std::endl;

	if (m_media_downloader == NULL ||
			m_media_downloader->isStarted()) {
		const char *problem = m_media_downloader ?
			"we already saw another announcement" :
			"all media has been received already";
		errorstream << "Client: Received media announcement but "
			<< problem << "!" << std::endl;
		return;
	}

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_manager->isRunning());

	if (m_proto_ver >= 48) {
		// compressed table of media names
		std::vector<std::string> names;
		{
			std::istringstream iss(pkt->readLongString(), std::ios::binary);
			std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
			decompressZstd(iss, ss);
			names = deserializeString16Array(ss);
		}

		// raw hash for each media file
		for (auto &name : names) {
			auto sha1_raw = pkt->readRawString(20);
			m_media_downloader->addFile(name, sha1_raw);
		}
	} else {
		u16 num_files;
		*pkt >> num_files;

		std::string name, sha1_base64;
		for (u16 i = 0; i < num_files; i++) {
			*pkt >> name >> sha1_base64;

			std::string sha1_raw = base64_decode(sha1_base64);
			m_media_downloader->addFile(name, sha1_raw);
		}
	}

	{
		// Remote media servers
		std::string str;
		*pkt >> str;

		Strfnd sf(str);
		while (!sf.at_end()) {
			std::string baseurl = trim(sf.next(","));
			if (!baseurl.empty()) {
				m_remote_media_servers.emplace_back(baseurl);
				m_media_downloader->addRemoteServer(baseurl);
			}
		}
	}

	m_media_downloader->step(this);
}

void Client::handleCommand_Media(NetworkPacket* pkt)
{
	u16 num_bunches;
	u16 bunch_i;
	u32 num_files;

	*pkt >> num_bunches >> bunch_i >> num_files;

	infostream << "Client: Received files: bunch " << bunch_i << "/"
			<< num_bunches << " files=" << num_files
			<< " size=" << pkt->getSize() << std::endl;

	if (num_files == 0)
		return;

	bool init_phase = m_media_downloader && m_media_downloader->isStarted();

	if (init_phase) {
		// Mesh update thread must be stopped while
		// updating content definitions
		sanity_check(!m_mesh_update_manager->isRunning());
	}

	for (u32 i = 0; i < num_files; i++) {
		std::string name, data;

		*pkt >> name;
		data = pkt->readLongString();
		if (m_proto_ver >= 48) {
			std::istringstream iss(data, std::ios::binary);
			std::ostringstream oss(std::ios::binary);
			decompressZstd(iss, oss);
			data = oss.str();
		}

		bool ok = false;
		if (init_phase) {
			ok = m_media_downloader->conventionalTransferDone(name, data, this);
		} else {
			// Check pending dynamic transfers, one of them must be it
			for (const auto &it : m_pending_media_downloads) {
				if (it.d->conventionalTransferDone(name, data, this)) {
					ok = true;
					break;
				}
			}
		}
		if (!ok) {
			errorstream << "Client: Received media \"" << name
				<< "\" but no downloads pending. " << num_bunches << " bunches, "
				<< num_files << " in this one. (init_phase=" << init_phase
				<< ")" << std::endl;
		}
	}
}

void Client::handleCommand_ItemDef(NetworkPacket* pkt)
{
	infostream << "Client: Received item definitions: packet size: "
			<< pkt->getSize() << std::endl;

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_manager->isRunning());

	// Decompress item definitions
	std::istringstream tmp_is(pkt->readLongString(), std::ios::binary);
	std::stringstream tmp_os(std::ios::binary | std::ios::in | std::ios::out);
	if (m_proto_ver >= 48)
		decompressZstd(tmp_is, tmp_os);
	else
		decompressZlib(tmp_is, tmp_os);

	// Deserialize node definitions
	m_itemdef->deSerialize(tmp_os, m_proto_ver);
	m_itemdef_received = true;
}

void Client::handleCommand_MediaPush(NetworkPacket *pkt)
{
	std::string raw_hash, filename, filedata;
	u32 token;
	bool cached;

	*pkt >> raw_hash >> filename >> cached;
	if (m_proto_ver >= 40)
		*pkt >> token;
	else
		filedata = pkt->readLongString();

	if (raw_hash.size() != 20 || filename.empty() ||
			(m_proto_ver < 40 && filedata.empty()) ||
			!string_allowed(filename, TEXTURENAME_ALLOWED_CHARS)) {
		throw PacketError("Illegal filename, data or hash");
	}

	verbosestream << "Server pushes media file \"" << filename << "\" ";
	if (filedata.empty())
		verbosestream << "to be fetched ";
	else
		verbosestream << "with " << filedata.size() << " bytes ";
	verbosestream << "(cached=" << cached << ")" << std::endl;

	if (!filedata.empty()) {
		// LEGACY CODEPATH
		// Compute and check checksum of data
		std::string computed_hash = hashing::sha1(filedata);
		if (raw_hash != computed_hash) {
			verbosestream << "Hash of file data mismatches, ignoring." << std::endl;
			return;
		}

		// Actually load media
		loadMedia(filedata, filename, true);

		// Cache file for the next time when this client joins the same server
		if (cached)
			clientMediaUpdateCache(raw_hash, filedata);
		return;
	}

	auto it = std::find_if(m_pending_media_downloads.begin(),
		m_pending_media_downloads.end(), [&] (const PendingMediaDownload &pend) {
		return pend.name == filename;
	});
	if (it != m_pending_media_downloads.end()) {
		// The server sent another push for a file we're already downloading.
		verbosestream << "Merged with ongoing identical request." << std::endl;
		it->tokens.push_back(token);
		return;
	}

	// Create a downloader for this file
	auto downloader(std::make_shared<SingleMediaDownloader>(cached));
	m_pending_media_downloads.emplace_back(token, filename, downloader);
	downloader->addFile(filename, raw_hash);
	for (const auto &baseurl : m_remote_media_servers)
		downloader->addRemoteServer(baseurl);

	downloader->step(this);
}
