// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "sound_singleton.h"

namespace sound {

bool SoundManagerSingleton::init()
{
        if (!(m_device = unique_ptr_alcdevice(alcOpenDevice(nullptr)))) {
                errorstream << "Audio: Global Initialization: Failed to open device" << std::endl;
                return false;
        }

        if (!(m_context = unique_ptr_alccontext(alcCreateContext(m_device.get(), nullptr)))) {
                errorstream << "Audio: Global Initialization: Failed to create context" << std::endl;
                return false;
        }

        if (!alcMakeContextCurrent(m_context.get())) {
                errorstream << "Audio: Global Initialization: Failed to make current context" << std::endl;
                return false;
        }

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

        // Speed of sound in nodes per second.
        // Assumes 1 node sidelength = 1 meter and "normal" air at ~20°C.
        // NOTE: Speed of sound is hardcoded at 343.3 m/s. This should be
        // mod-controllable so that mods can change it (e.g., for underwater
        // acoustics or sci-fi environments).
        // Proposed implementation: (1) Add a setting "sound_speed_of_sound"
        // (float, default 343.3) read via g_settings->getFloat(). (2) Add a
        // Lua API: minetest.set_soundspeed(float) that calls alSpeedOfSound().
        // (3) Call alSpeedOfSound() whenever the setting changes, or in the
        // sound step function. This requires passing the new value through
        // the sound manager interface.
        static constexpr float SPEED_OF_SOUND = 343.3f;
        alSpeedOfSound(SPEED_OF_SOUND);

        // doppler effect turned off for now, for best backwards compatibility
        alDopplerFactor(0.0f);

        if (alGetError() != AL_NO_ERROR) {
                errorstream << "Audio: Global Initialization: OpenAL Error " << alGetError() << std::endl;
                return false;
        }

        infostream << "Audio: Global Initialized: OpenAL " << alGetString(AL_VERSION)
                << ", using " << alcGetString(m_device.get(), ALC_DEVICE_SPECIFIER)
                << std::endl;

        return true;
}

SoundManagerSingleton::~SoundManagerSingleton()
{
        infostream << "Audio: Global Deinitialized." << std::endl;
}

} // namespace sound
