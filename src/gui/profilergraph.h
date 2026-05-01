// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <SColor.h>
#include <deque>
#include <utility>
#include <IGUIFont.h>
#include "profiler.h"

namespace video {
        class IVideoDriver;
}

/* Profiler display */
class ProfilerGraph
{
private:
        // TODO: This data structure is awfully inefficient — Piece copies the
        // entire Profiler::GraphValues (std::map<std::string, float>) into a
        // deque entry every frame. Consider using a ring buffer of flat arrays
        // or per-graph deques keyed by metric name to reduce per-frame allocations.
        struct Piece
        {
                Piece(const Profiler::GraphValues &v) : values(v) {}
                Profiler::GraphValues values;
        };
        std::deque<Piece> m_log;

public:
        u32 m_log_max_size = 200;

        ProfilerGraph() = default;

        void put(const Profiler::GraphValues &values);

        void draw(s32 x_left, s32 y_bottom, video::IVideoDriver *driver,
                        gui::IGUIFont *font) const;
};
