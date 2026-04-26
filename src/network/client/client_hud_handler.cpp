// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

// This file contains HUD/particle/sound/sky/weather-related packet handlers for Client.
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

void Client::handleCommand_PlaySound(NetworkPacket* pkt)
{
	/*
		[0] s32 server_id
		[4] u16 name length
		[6] char name[len]
		[ 6 + len] f32 gain
		[10 + len] u8 type (SoundLocation)
		[11 + len] v3f pos (in BS-space)
		[23 + len] u16 object_id
		[25 + len] bool loop
		[26 + len] f32 fade
		[30 + len] f32 pitch
		[34 + len] bool ephemeral
		[35 + len] f32 start_time (in seconds)
	*/

	s32 server_id;

	SoundSpec spec;
	SoundLocation type;
	v3f pos;
	u16 object_id;
	bool ephemeral = false;

	*pkt >> server_id >> spec.name >> spec.gain >> (u8 &)type >> pos >> object_id >> spec.loop;
	*pkt >> spec.fade >> spec.pitch;
	pos *= 1.0f/BS;

	do {
		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.2.0-dev
		*pkt >> ephemeral;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.8.0-dev
		*pkt >> spec.start_time;
	} while (0);

	// Generate a new id
	sound_handle_t client_id = (ephemeral && object_id == 0) ? 0 : m_sound->allocateId(2);

	// Start playing
	switch(type) {
	case SoundLocation::Local:
		m_sound->playSound(client_id, spec);
		break;
	case SoundLocation::Position:
		m_sound->playSoundAt(client_id, spec, pos, v3f(0.0f));
		break;
	case SoundLocation::Object: {
		ClientActiveObject *cao = m_env.getActiveObject(object_id);
		v3f vel(0.0f);
		if (cao) {
			pos = cao->getPosition() * (1.0f/BS);
			vel = cao->getVelocity() * (1.0f/BS);
		}
		// Note that the server sends 'pos' correctly even for attached sounds,
		// so this fallback path is not a mistake.
		m_sound->playSoundAt(client_id, spec, pos, vel);
		break;
	}
	default:
		// Unknown SoundLocation, instantly remove sound
		if (client_id != 0)
			m_sound->freeId(client_id, 2);
		if (!ephemeral)
			sendRemovedSounds({server_id});
		return;
	}

	if (client_id != 0) {
		// Note: m_sounds_client_to_server takes 1 ownership
		// For ephemeral sounds, server_id is not meaningful
		if (ephemeral) {
			m_sounds_client_to_server[client_id] = -1;
		} else {
			m_sounds_server_to_client[server_id] = client_id;
			m_sounds_client_to_server[client_id] = server_id;
		}
		if (object_id != 0)
			m_sounds_to_objects[client_id] = object_id;
	}
}

void Client::handleCommand_StopSound(NetworkPacket* pkt)
{
	s32 server_id;

	*pkt >> server_id;

	auto i = m_sounds_server_to_client.find(server_id);
	if (i != m_sounds_server_to_client.end()) {
		int client_id = i->second;
		m_sound->stopSound(client_id);
	}
}

void Client::handleCommand_FadeSound(NetworkPacket *pkt)
{
	s32 sound_id;
	float step;
	float gain;

	*pkt >> sound_id >> step >> gain;

	auto i = m_sounds_server_to_client.find(sound_id);
	if (i != m_sounds_server_to_client.end())
		m_sound->fadeSound(i->second, step, gain);
}

void Client::handleCommand_SpawnParticle(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	ParticleParameters p;
	p.deSerialize(is, m_proto_ver);

	ClientEvent *event = new ClientEvent();
	event->type           = CE_SPAWN_PARTICLE;
	event->spawn_particle = new ParticleParameters(p);

	m_client_event_queue.push(event);
}

void Client::handleCommand_SpawnParticleBatch(NetworkPacket *pkt)
{
	std::stringstream particle_batch_data(std::ios::binary | std::ios::in | std::ios::out);
	{
		std::istringstream compressed(pkt->readLongString(), std::ios::binary);
		decompressZstd(compressed, particle_batch_data);
	}

	while (canRead(particle_batch_data)) {
		auto p = std::make_unique<ParticleParameters>();
		{
			std::istringstream particle_data(deSerializeString32(particle_batch_data), std::ios::binary);
			p->deSerialize(particle_data, m_proto_ver);
		}

		ClientEvent *event = new ClientEvent();
		event->type = CE_SPAWN_PARTICLE;
		event->spawn_particle = p.release();

		m_client_event_queue.push(event);
	}
}

void Client::handleCommand_AddParticleSpawner(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	ParticleSpawnerParameters p;
	u32 server_id;
	u16 attached_id = 0;

	p.amount             = readU16(is);
	p.time               = readF32(is);
	if (p.time < 0)
		throw PacketError("particle spawner time < 0");

	bool missing_end_values = false;
	if (m_proto_ver >= 42) {
		// All tweenable parameters
		p.pos.deSerialize(is);
		p.vel.deSerialize(is);
		p.acc.deSerialize(is);
		p.exptime.deSerialize(is);
		p.size.deSerialize(is);
	} else {
		p.pos.start.legacyDeSerialize(is);
		p.vel.start.legacyDeSerialize(is);
		p.acc.start.legacyDeSerialize(is);
		p.exptime.start.legacyDeSerialize(is);
		p.size.start.legacyDeSerialize(is);
		missing_end_values = true;
	}

	p.collisiondetection = readU8(is);
	p.texture.string     = deSerializeString32(is);

	server_id = readU32(is);

	p.vertical = readU8(is);
	p.collision_removal = readU8(is);

	attached_id = readU16(is);

	p.animation.deSerialize(is, m_proto_ver);
	p.glow = readU8(is);
	p.object_collision = readU8(is);

	do {
		if (!canRead(is))
			break;
		// >= 5.3.0-dev

		p.node.param0 = readU16(is);;
		p.node.param2 = readU8(is);
		p.node_tile   = readU8(is);

		if (m_proto_ver < 42) {
			// v >= 5.6.0
			if (!canRead(is))
				break;

			// initial bias must be stored separately in the stream to preserve
			// backwards compatibility with older clients, which do not support
			// a bias field in their range "format"
			p.pos.start.bias = readF32(is);
			p.vel.start.bias = readF32(is);
			p.acc.start.bias = readF32(is);
			p.exptime.start.bias = readF32(is);
			p.size.start.bias = readF32(is);

			p.pos.end.deSerialize(is);
			p.vel.end.deSerialize(is);
			p.acc.end.deSerialize(is);
			p.exptime.end.deSerialize(is);
			p.size.end.deSerialize(is);

			missing_end_values = false;
		}
		// else: fields are already read by deSerialize() very early

		// properties for legacy texture field
		p.texture.deSerialize(is, m_proto_ver, true);

		p.drag.deSerialize(is);
		p.jitter.deSerialize(is);
		p.bounce.deSerialize(is);
		ParticleParamTypes::deSerializeParameterValue(is, p.attractor_kind);
		using ParticleParamTypes::AttractorKind;
		if (p.attractor_kind != AttractorKind::none) {
			p.attract.deSerialize(is);
			p.attractor_origin.deSerialize(is);
			p.attractor_attachment = readU16(is);
			/* we only check the first bit, in order to allow this value
			 * to be turned into a bit flag field later if needed */
			p.attractor_kill = !!(readU8(is) & 1);
			if (p.attractor_kind != AttractorKind::point) {
				p.attractor_direction.deSerialize(is);
				p.attractor_direction_attachment = readU16(is);
			}
		}
		p.radius.deSerialize(is);

		u16 texpoolsz = readU16(is);
		p.texpool.reserve(texpoolsz);
		for (u16 i = 0; i < texpoolsz; ++i) {
			ServerParticleTexture newtex;
			newtex.deSerialize(is, m_proto_ver);
			p.texpool.push_back(newtex);
		}

		//if (!canRead(is))
		//      break;
		// Add new code here
	} while(0);

	if (missing_end_values) {
		// there's no tweening data to be had, so we need to set the
		// legacy params to constant values, otherwise everything old
		// will tween to zero
		p.pos.end = p.pos.start;
		p.vel.end = p.vel.start;
		p.acc.end = p.acc.start;
		p.exptime.end = p.exptime.start;
		p.size.end = p.size.start;
	}

	auto event = new ClientEvent();
	event->type                            = CE_ADD_PARTICLESPAWNER;
	event->add_particlespawner.p           = new ParticleSpawnerParameters(p);
	event->add_particlespawner.attached_id = attached_id;
	event->add_particlespawner.id          = server_id;

	m_client_event_queue.push(event);
}


void Client::handleCommand_DeleteParticleSpawner(NetworkPacket* pkt)
{
	u32 server_id;
	*pkt >> server_id;

	ClientEvent *event = new ClientEvent();
	event->type = CE_DELETE_PARTICLESPAWNER;
	event->delete_particlespawner.id = server_id;

	m_client_event_queue.push(event);
}

void Client::handleCommand_HudAdd(NetworkPacket* pkt)
{
	u32 server_id;
	u8 type;
	v2f pos;
	std::string name;
	v2f scale;
	std::string text;
	u32 number;
	u32 item;
	u32 dir;
	v2f align;
	v2f offset;
	v3f world_pos;
	v2f size;
	s16 z_index = 0;
	std::string text2;
	u32 style = 0;

	*pkt >> server_id >> type >> pos >> name >> scale >> text >> number >> item
		>> dir >> align >> offset;
	*pkt >> world_pos;

	if (m_proto_ver >= 52) {
		*pkt >> size;
	} else {
		v2s32 old_format;
		*pkt >> old_format;
		size = v2f::from(old_format);
	}

	do {
		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.2.0-dev
		*pkt >> z_index;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.3.0-dev
		*pkt >> text2;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.5.0-dev
		*pkt >> style;
	} while (0);

	ClientEvent *event = new ClientEvent();
	event->type              = CE_HUDADD;
	event->hudadd            = new ClientEventHudAdd();
	event->hudadd->server_id = server_id;
	event->hudadd->type      = type;
	event->hudadd->pos       = pos;
	event->hudadd->name      = name;
	event->hudadd->scale     = scale;
	event->hudadd->text      = text;
	event->hudadd->number    = number;
	event->hudadd->item      = item;
	event->hudadd->dir       = dir;
	event->hudadd->align     = align;
	event->hudadd->offset    = offset;
	event->hudadd->world_pos = world_pos;
	event->hudadd->size      = size;
	event->hudadd->z_index   = z_index;
	event->hudadd->text2     = text2;
	event->hudadd->style     = style;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudRemove(NetworkPacket* pkt)
{
	u32 server_id;

	*pkt >> server_id;

	ClientEvent *event = new ClientEvent();
	event->type     = CE_HUDRM;
	event->hudrm.id = server_id;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudChange(NetworkPacket* pkt)
{
	std::string sdata;
	v2f v2fdata;
	v3f v3fdata;
	u32 intdata = 0;
	u32 server_id;
	u8 stat;

	*pkt >> server_id >> stat;

	// Do nothing if stat is not known
	if (stat >= HudElementStat_END) {
		return;
	}

	// Keep in sync with:server.cpp -> SendHUDChange
	switch (static_cast<HudElementStat>(stat)) {
		case HUD_STAT_POS:
		case HUD_STAT_SCALE:
		case HUD_STAT_ALIGN:
		case HUD_STAT_OFFSET:
			*pkt >> v2fdata;
			break;
		case HUD_STAT_NAME:
		case HUD_STAT_TEXT:
		case HUD_STAT_TEXT2:
			*pkt >> sdata;
			break;
		case HUD_STAT_WORLD_POS:
			*pkt >> v3fdata;
			break;
		case HUD_STAT_SIZE:
			if (m_proto_ver >= 52) {
				*pkt >> v2fdata;
			} else {
				v2s32 old_format;
				*pkt >> old_format;
				v2fdata = v2f::from(old_format);
			}
			break;
		default:
			*pkt >> intdata;
			break;
	}

	ClientEvent *event = new ClientEvent();
	event->type                 = CE_HUDCHANGE;
	event->hudchange            = new ClientEventHudChange();
	event->hudchange->id        = server_id;
	event->hudchange->stat      = static_cast<HudElementStat>(stat);
	event->hudchange->v2fdata   = v2fdata;
	event->hudchange->v3fdata   = v3fdata;
	event->hudchange->sdata     = sdata;
	event->hudchange->data      = intdata;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetFlags(NetworkPacket* pkt)
{
	u32 flags, mask;

	*pkt >> flags >> mask;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	bool was_minimap_radar_visible = player->hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE;

	player->hud_flags &= ~mask;
	player->hud_flags |= flags;

	bool m_minimap_radar_disabled_by_server = !(player->hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE);

	// Not so satisying code to keep compatibility with old fixed mode system
	// -->
	// If radar has been disabled, try to find a non radar mode or fall back to 0
	if (m_minimap && m_minimap_radar_disabled_by_server
			&& was_minimap_radar_visible) {
		while (m_minimap->getModeIndex() > 0 &&
				m_minimap->getModeDef().type == MINIMAP_TYPE_RADAR)
			m_minimap->nextMode();
	}
	// <--
	// End of 'not so satifying code'
}

void Client::handleCommand_HudSetParam(NetworkPacket* pkt)
{
	u16 param; std::string value;

	*pkt >> param >> value;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	if (param == HUD_PARAM_HOTBAR_ITEMCOUNT && value.size() == 4) {
		s32 hotbar_itemcount = readS32((u8*) value.c_str());
		if (hotbar_itemcount > 0 && hotbar_itemcount <= HUD_HOTBAR_ITEMCOUNT_MAX)
			player->hud_hotbar_itemcount = hotbar_itemcount;
	}
	else if (param == HUD_PARAM_HOTBAR_IMAGE) {
		player->hotbar_image = value;
	}
	else if (param == HUD_PARAM_HOTBAR_SELECTED_IMAGE) {
		player->hotbar_selected_image = value;
	}
}

void Client::handleCommand_HudSetSky(NetworkPacket* pkt)
{
	if (m_proto_ver < 39) {
		// Handle Protocol 38 and below servers with old set_sky,
		// ensuring the classic look is kept.
		std::string datastring(pkt->getString(0), pkt->getSize());
		std::istringstream is(datastring, std::ios_base::binary);

		SkyboxParams skybox;
		skybox.bgcolor = video::SColor(readARGB8(is));
		skybox.type = std::string(deSerializeString16(is));
		u16 count = readU16(is);

		for (size_t i = 0; i < count; i++)
			skybox.textures.emplace_back(deSerializeString16(is));

		skybox.clouds = readU8(is) != 0;

		// Use default skybox settings:
		SunParams sun = SkyboxDefaults::getSunDefaults();
		MoonParams moon = SkyboxDefaults::getMoonDefaults();
		StarParams stars = SkyboxDefaults::getStarDefaults();

		// Fix for "regular" skies, as color isn't kept:
		if (skybox.type == "regular") {
			skybox.sky_color = SkyboxDefaults::getSkyColorDefaults();
			skybox.fog_tint_type = "default";
			skybox.fog_moon_tint = video::SColor(255, 255, 255, 255);
			skybox.fog_sun_tint = video::SColor(255, 255, 255, 255);
		} else {
			sun.visible = false;
			sun.sunrise_visible = false;
			moon.visible = false;
			stars.visible = false;
		}

		// Skybox, sun, moon and stars ClientEvents:
		ClientEvent *sky_event = new ClientEvent();
		sky_event->type = CE_SET_SKY;
		sky_event->set_sky = new SkyboxParams(skybox);
		m_client_event_queue.push(sky_event);

		ClientEvent *sun_event = new ClientEvent();
		sun_event->type = CE_SET_SUN;
		sun_event->sun_params = new SunParams(sun);
		m_client_event_queue.push(sun_event);

		ClientEvent *moon_event = new ClientEvent();
		moon_event->type = CE_SET_MOON;
		moon_event->moon_params = new MoonParams(moon);
		m_client_event_queue.push(moon_event);

		ClientEvent *star_event = new ClientEvent();
		star_event->type = CE_SET_STARS;
		star_event->star_params = new StarParams(stars);
		m_client_event_queue.push(star_event);
		return;
	}

	SkyboxParams skybox;

	*pkt >> skybox.bgcolor >> skybox.type >> skybox.clouds >>
		skybox.fog_sun_tint >> skybox.fog_moon_tint >> skybox.fog_tint_type;

	if (skybox.type == "skybox") {
		u16 texture_count;
		std::string texture;
		*pkt >> texture_count;
		for (u16 i = 0; i < texture_count; i++) {
			*pkt >> texture;
			skybox.textures.emplace_back(texture);
		}
	} else if (skybox.type == "regular") {
		auto &c = skybox.sky_color;
		*pkt >> c.day_sky >> c.day_horizon >> c.dawn_sky >> c.dawn_horizon
			>> c.night_sky >> c.night_horizon >> c.indoors;
	}

	do {
		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.7.0-dev
		*pkt >> skybox.body_orbit_tilt;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.8.0-dev
		*pkt >> skybox.fog_distance >> skybox.fog_start;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.9.0-dev
		*pkt >> skybox.fog_color;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.16.0-dev
		*pkt >> skybox.auto_dim_skybox;
	} while (0);

	ClientEvent *event = new ClientEvent();
	event->type = CE_SET_SKY;
	event->set_sky = new SkyboxParams(skybox);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetSun(NetworkPacket *pkt)
{
	SunParams sun;

	*pkt >> sun.visible >> sun.texture>> sun.tonemap
		>> sun.sunrise >> sun.sunrise_visible >> sun.scale;

	ClientEvent *event = new ClientEvent();
	event->type        = CE_SET_SUN;
	event->sun_params  = new SunParams(sun);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetMoon(NetworkPacket *pkt)
{
	MoonParams moon;

	*pkt >> moon.visible >> moon.texture
		>> moon.tonemap >> moon.scale;

	ClientEvent *event = new ClientEvent();
	event->type        = CE_SET_MOON;
	event->moon_params = new MoonParams(moon);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetStars(NetworkPacket *pkt)
{
	StarParams stars = SkyboxDefaults::getStarDefaults();

	*pkt >> stars.visible >> stars.count
		>> stars.starcolor >> stars.scale;
	do {
		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.6.0-dev
		*pkt >> stars.day_opacity;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.15.0-dev
		*pkt >> stars.star_seed;
	} while (0);

	ClientEvent *event = new ClientEvent();
	event->type        = CE_SET_STARS;
	event->star_params = new StarParams(stars);

	m_client_event_queue.push(event);
}

void Client::handleCommand_CloudParams(NetworkPacket* pkt)
{
	f32 density;
	video::SColor color_bright;
	video::SColor color_ambient;
	video::SColor color_shadow = video::SColor(255, 204, 204, 204);
	f32 height;
	f32 thickness;
	v2f speed;

	*pkt >> density >> color_bright >> color_ambient
			>> height >> thickness >> speed;

	if (pkt->hasRemainingBytes()) {
		// >= 5.10.0-dev
		*pkt >> color_shadow;
	}

	ClientEvent *event = new ClientEvent();
	event->type                       = CE_CLOUD_PARAMS;
	event->cloud_params.density       = density;
	// use the underlying u32 representation, because we can't
	// use struct members with constructors here, and this way
	// we avoid using new() and delete() for no good reason
	event->cloud_params.color_bright  = color_bright.color;
	event->cloud_params.color_ambient = color_ambient.color;
	event->cloud_params.color_shadow = color_shadow.color;
	event->cloud_params.height        = height;
	event->cloud_params.thickness     = thickness;
	// same here: deconstruct to skip constructor
	event->cloud_params.speed_x       = speed.X;
	event->cloud_params.speed_y       = speed.Y;
	m_client_event_queue.push(event);
}

void Client::handleCommand_OverrideDayNightRatio(NetworkPacket* pkt)
{
	bool do_override;
	u16 day_night_ratio_u;

	*pkt >> do_override >> day_night_ratio_u;

	float day_night_ratio_f = (float)day_night_ratio_u / 65536;

	ClientEvent *event = new ClientEvent();
	event->type                                 = CE_OVERRIDE_DAY_NIGHT_RATIO;
	event->override_day_night_ratio.do_override = do_override;
	event->override_day_night_ratio.ratio_f     = day_night_ratio_f;
	m_client_event_queue.push(event);
}

void Client::handleCommand_LocalPlayerAnimations(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	for (int i = 0; i < 4; ++i) {
		if (getProtoVersion() >= 46) {
			*pkt >> player->local_animations[i];
		} else {
			v2s32 local_animation;
			*pkt >> local_animation;
			player->local_animations[i] = v2f::from(local_animation);
		}
	}

	*pkt >> player->local_animation_speed;

	player->last_animation = LocalPlayerAnimation::NO_ANIM;
}

void Client::handleCommand_EyeOffset(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	*pkt >> player->eye_offset_first >> player->eye_offset_third;

	// Fallback for older servers
	player->eye_offset_third_front = player->eye_offset_third;

	if (pkt->hasRemainingBytes()) {
		// >= 5.8.0-dev
		*pkt >> player->eye_offset_third_front;
	}
}

void Client::handleCommand_Camera(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);

	u8 tmp;
	*pkt >> tmp;
	player->allowed_camera_mode = static_cast<CameraMode>(tmp);
	if (player->allowed_camera_mode >= CameraMode_END)
		player->allowed_camera_mode = CAMERA_MODE_ANY;

	m_client_event_queue.push(new ClientEvent(CE_UPDATE_CAMERA));
}

void Client::handleCommand_MinimapModes(NetworkPacket *pkt)
{
	u16 count; // modes
	u16 mode;  // wanted current mode index after change

	*pkt >> count >> mode;

	if (m_minimap)
		m_minimap->clearModes();

	for (size_t index = 0; index < count; index++) {
		u16 type;
		std::string label;
		u16 size;
		std::string texture;
		u16 scale;

		*pkt >> type >> label >> size >> texture >> scale;

		if (m_minimap)
			m_minimap->addMode(MinimapType(type), size, label, texture, scale);
	}

	if (m_minimap)
		m_minimap->setModeIndex(mode);
}

void Client::handleCommand_SetLighting(NetworkPacket *pkt)
{
	Lighting& lighting = m_env.getLocalPlayer()->getLighting();

	*pkt >> lighting.shadow_intensity;
	do {
		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.7.0-dev
		*pkt >> lighting.saturation;
		// >= 5.7.0-dev
		*pkt >> lighting.exposure.luminance_min
				>> lighting.exposure.luminance_max
				>> lighting.exposure.exposure_correction
				>> lighting.exposure.speed_dark_bright
				>> lighting.exposure.speed_bright_dark
				>> lighting.exposure.center_weight_power;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.9.0-dev
		*pkt >> lighting.volumetric_light_strength;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.10.0-dev
		*pkt >> lighting.shadow_tint;
		// >= 5.10.0-dev
		*pkt >> lighting.bloom_intensity
				>> lighting.bloom_strength_factor
				>> lighting.bloom_radius;

		if (!pkt->hasRemainingBytes())
			break;
		// >= 5.16.0-dev
		*pkt >> lighting.shadow_direction;
	} while (0);
}
