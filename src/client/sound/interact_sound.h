// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti developers

#pragma once

#include "sound_spec.h"

class ISoundManager;
class NodeDefManager;
class MtEventManager;
class MtEvent;

// InteractSound handles player interaction sounds: footsteps, jumps, punches.
// This class was extracted from SoundMaker (src/client/sound_maker.h) to
// separate player interaction sounds from node sounds and reduce coupling.
// SoundMaker continues to handle node-specific sounds (nodeDug) and
// punch sounds; InteractSound focuses on movement-related audio feedback.
class InteractSound
{
public:
	InteractSound(ISoundManager *sound, const NodeDefManager *ndef);
	~InteractSound() = default;

	// Update step/jump timers and footstep sound spec.
	// Called every client tick with the current dtime and footstep parameters.
	void update(float dtime, bool makes_footstep_sound,
			const SoundSpec &sound_footstep);

	// Register this InteractSound as a receiver for movement-related events:
	// VIEW_BOBBING_STEP, PLAYER_REGAIN_GROUND, PLAYER_JUMP.
	// Note: The event manager must outlive this object.
	void registerReceiver(MtEventManager *mgr);

private:
	ISoundManager *m_sound = nullptr;
	const NodeDefManager *m_ndef = nullptr;

	float m_player_step_timer = 0.0f;
	float m_player_jump_timer = 0.0f;
	bool makes_footstep_sound = true;
	SoundSpec m_player_step_sound;

	void playPlayerStep();
	void playPlayerJump();

	// Event handlers for movement-related sounds
	static void viewBobbingStep(MtEvent *e, void *data);
	static void playerRegainGround(MtEvent *e, void *data);
	static void playerJump(MtEvent *e, void *data);
};
