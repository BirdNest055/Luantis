// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti developers

#pragma once

#include "sound_spec.h"

class ISoundManager;
class NodeDefManager;
class MtEventManager;
class MtEvent;

// NOTE: NodeDugEvent and related event classes should be moved to their own
// file (e.g., src/game_events.h). Currently they live in sound_maker.h which
// conflates audio concerns with game interaction events. Migration: create
// src/game_events.h, move all MtEvent subclasses there, update includes in
// sound_maker.h, game.cpp, and any other consumers.
// REFACTOR: SoundMaker should be split into separate concerns:
//   1. InteractSound (player step, jump, punch sounds) → src/client/sound/interact_sound.h
//   2. NodeSound (dig, place, node sounds) → src/client/sound/node_sound.h
//   3. Event dispatch should go to src/game_events.h
// This would reduce coupling and make it easier to test sound logic independently.
#include "mtevent.h"
#include "mapnode.h"
class NodeDugEvent : public MtEvent
{
public:
        v3s16 p;
        MapNode n;

        NodeDugEvent(v3s16 p, MapNode n):
                p(p),
                n(n)
        {}
        Type getType() const { return NODE_DUG; }
};


// This class handles the playing of sound on MtEventManager events
// and stores which sounds to play.

class SoundMaker
{
        ISoundManager *m_sound;
        const NodeDefManager *m_ndef;

        float m_player_step_timer = 0.0f;
        float m_player_jump_timer = 0.0f;
        bool makes_footstep_sound = true;
        SoundSpec m_player_step_sound;

public:
        SoundSpec m_player_leftpunch_sound;
        // Second sound made on left punch, currently used for item 'use' sound
        SoundSpec m_player_leftpunch_sound2;
        SoundSpec m_player_rightpunch_sound;

        SoundMaker(ISoundManager *sound, const NodeDefManager *ndef) :
                m_sound(sound), m_ndef(ndef) {}

        // NOTE if the SoundMaker got registered as a receiver,
        // it must not be destructed before the event manager.
        void registerReceiver(MtEventManager *mgr);

        void update(f32 dtime, bool makes_footstep_sound, const SoundSpec &sound_footstep);

private:
        void playPlayerStep();
        void playPlayerJump();

        static void viewBobbingStep(MtEvent *e, void *data);
        static void playerRegainGround(MtEvent *e, void *data);
        static void playerJump(MtEvent *e, void *data);
        static void cameraPunchLeft(MtEvent *e, void *data);
        static void cameraPunchRight(MtEvent *e, void *data);
        static void nodeDug(MtEvent *e, void *data);
        static void playerDamage(MtEvent *e, void *data);
        static void playerFallingDamage(MtEvent *e, void *data);
};
