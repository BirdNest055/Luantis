// Luanti
// SPDX-FileCopyright-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti developers

#pragma once

// REFACTOR: This is the proposed interface for InteractSound, extracted from
// SoundMaker. It handles player interaction sounds: footsteps, jumps, punches.
// The actual implementation should be moved from sound_maker.h/cpp.
// See src/client/sound_maker.h for the REFACTOR comment with the full plan.

class ISoundManager;
class NodeDefManager;
class MtEventManager;

class InteractSound
{
public:
        InteractSound(ISoundManager *sound, const NodeDefManager *ndef);
        ~InteractSound() = default;

        // TODO: implement — move from SoundMaker
        void update(float dtime, bool makes_footstep_sound,
                        const SoundSpec &sound_footstep);

        // TODO: implement — move from SoundMaker
        void registerReceiver(MtEventManager *mgr);

private:
        ISoundManager *m_sound = nullptr;
        const NodeDefManager *m_ndef = nullptr;

        float m_player_step_timer = 0.0f;
        float m_player_jump_timer = 0.0f;
        bool makes_footstep_sound = true;
        SoundSpec m_player_step_sound;

        // TODO: implement — move event handlers from SoundMaker
        // static void viewBobbingStep(MtEvent *e, void *data);
        // static void playerRegainGround(MtEvent *e, void *data);
        // static void playerJump(MtEvent *e, void *data);
        // static void cameraPunchLeft(MtEvent *e, void *data);
        // static void cameraPunchRight(MtEvent *e, void *data);
        // static void playerDamage(MtEvent *e, void *data);
        // static void playerFallingDamage(MtEvent *e, void *data);
};
