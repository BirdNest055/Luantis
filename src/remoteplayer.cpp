// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2016 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2014-2016 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "remoteplayer.h"
#include <json/json.h>
#include "gamedef.h"
#include "server.h"
#include "settings.h"
#include "server/player_sao.h"

/*
        RemotePlayer
*/

// static config cache for remoteplayer
bool RemotePlayer::m_setting_cache_loaded = false;
float RemotePlayer::m_setting_chat_message_limit_per_10sec = 0.0f;
u16 RemotePlayer::m_setting_chat_message_limit_trigger_kick = 0;

RemotePlayer::RemotePlayer(const std::string &name, IItemDefManager *idef):
        Player(name, idef)
{
        // Batch 36: Always re-read settings so runtime changes take effect.
        // The old one-time cache meant settings changes were never picked up.
        {
                float chat_limit = 8.0f;
                g_settings->getFloatNoEx("chat_message_limit_per_10sec", chat_limit);
                RemotePlayer::m_setting_chat_message_limit_per_10sec = std::max(chat_limit, 0.0f);
                u16 kick_limit = 50;
                g_settings->getU16NoEx("chat_message_limit_trigger_kick", kick_limit);
                RemotePlayer::m_setting_chat_message_limit_trigger_kick = kick_limit;
        }

        // Batch 36: Clamp all movement physics to non-negative to prevent
        // division-by-zero or inverted physics from corrupt settings values.
        movement_acceleration_default   = std::max(g_settings->getFloat("movement_acceleration_default"),   0.0f) * BS;
        movement_acceleration_air       = std::max(g_settings->getFloat("movement_acceleration_air"),       0.0f) * BS;
        movement_acceleration_fast      = std::max(g_settings->getFloat("movement_acceleration_fast"),      0.0f) * BS;
        movement_speed_walk             = std::max(g_settings->getFloat("movement_speed_walk"),             0.0f) * BS;
        movement_speed_crouch           = std::max(g_settings->getFloat("movement_speed_crouch"),           0.0f) * BS;
        movement_speed_fast             = std::max(g_settings->getFloat("movement_speed_fast"),             0.0f) * BS;
        movement_speed_climb            = std::max(g_settings->getFloat("movement_speed_climb"),            0.0f) * BS;
        movement_speed_jump             = std::max(g_settings->getFloat("movement_speed_jump"),             0.0f) * BS;
        movement_liquid_fluidity        = std::max(g_settings->getFloat("movement_liquid_fluidity"),        0.0f) * BS;
        movement_liquid_fluidity_smooth = std::max(g_settings->getFloat("movement_liquid_fluidity_smooth"), 0.0f) * BS;
        movement_liquid_sink            = std::max(g_settings->getFloat("movement_liquid_sink"),            0.0f) * BS;
        movement_gravity                = std::max(g_settings->getFloat("movement_gravity"),                0.0f) * BS;

        // Skybox defaults:
        m_cloud_params  = SkyboxDefaults::getCloudDefaults();
        m_skybox_params = SkyboxDefaults::getSkyDefaults();
        m_sun_params    = SkyboxDefaults::getSunDefaults();
        m_moon_params   = SkyboxDefaults::getMoonDefaults();
        m_star_params   = SkyboxDefaults::getStarDefaults();
}

RemotePlayer::~RemotePlayer()
{
        if (m_sao)
                m_sao->setPlayer(nullptr);
}

RemotePlayerChatResult RemotePlayer::canSendChatMessage()
{
        // Rate limit messages
        u32 now = time(NULL);
        float time_passed = now - m_last_chat_message_sent;
        m_last_chat_message_sent = now;

        // If this feature is disabled
        if (m_setting_chat_message_limit_per_10sec <= 0.0) {
                return RPLAYER_CHATRESULT_OK;
        }

        m_chat_message_allowance += time_passed * (m_setting_chat_message_limit_per_10sec / 8.0f);
        if (m_chat_message_allowance > m_setting_chat_message_limit_per_10sec) {
                m_chat_message_allowance = m_setting_chat_message_limit_per_10sec;
        }

        if (m_chat_message_allowance < 1.0f) {
                infostream << "Player " << m_name
                                << " chat limited due to excessive message amount." << std::endl;

                // Kick player if flooding is too intensive
                m_message_rate_overhead++;
                if (m_message_rate_overhead > RemotePlayer::m_setting_chat_message_limit_trigger_kick) {
                        return RPLAYER_CHATRESULT_KICK;
                }

                return RPLAYER_CHATRESULT_FLOODING;
        }

        // Reinit message overhead
        if (m_message_rate_overhead > 0) {
                m_message_rate_overhead = 0;
        }

        m_chat_message_allowance -= 1.0f;
        return RPLAYER_CHATRESULT_OK;
}

void RemotePlayer::onSuccessfulSave()
{
        setModified(false);
        if (m_sao)
                m_sao->getMeta().setModified(false);
}
