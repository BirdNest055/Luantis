// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti developers

#include "interact_sound.h"

#include "../mtevent.h"
#include "../nodedef.h"
#include "../sound.h"

InteractSound::InteractSound(ISoundManager *sound, const NodeDefManager *ndef) :
        m_sound(sound), m_ndef(ndef)
{
}

void InteractSound::playPlayerStep()
{
        if (m_player_step_timer <= 0.0f && m_player_step_sound.exists()) {
                m_player_step_timer = 0.03f;
                if (makes_footstep_sound)
                        m_sound->playSound(0, m_player_step_sound);
        }
}

void InteractSound::playPlayerJump()
{
        if (m_player_jump_timer <= 0.0f) {
                m_player_jump_timer = 0.2f;
                m_sound->playSound(0, SoundSpec("player_jump", 0.5f));
        }
}

void InteractSound::viewBobbingStep(MtEvent *e, void *data)
{
        InteractSound *is = static_cast<InteractSound*>(data);
        is->playPlayerStep();
}

void InteractSound::playerRegainGround(MtEvent *e, void *data)
{
        InteractSound *is = static_cast<InteractSound*>(data);
        is->playPlayerStep();
}

void InteractSound::playerJump(MtEvent *e, void *data)
{
        InteractSound *is = static_cast<InteractSound*>(data);
        is->playPlayerJump();
}

void InteractSound::registerReceiver(MtEventManager *mgr)
{
        mgr->reg(MtEvent::VIEW_BOBBING_STEP, InteractSound::viewBobbingStep, this);
        mgr->reg(MtEvent::PLAYER_REGAIN_GROUND, InteractSound::playerRegainGround, this);
        mgr->reg(MtEvent::PLAYER_JUMP, InteractSound::playerJump, this);
}

void InteractSound::update(float dtime, bool makes_footstep_sound,
                const SoundSpec &sound_footstep)
{
        this->makes_footstep_sound = makes_footstep_sound;
        if (makes_footstep_sound) {
                m_player_step_timer -= dtime;
                m_player_jump_timer -= dtime;
        }
        m_player_step_sound = sound_footstep;
}
