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
        // NOTE: This data structure is awfully inefficient — Piece copies the
        // entire Profiler::GraphValues (std::map<std::string, float>) into a
        // deque entry every frame.
        //
        // Root cause: Profiler::GraphValues is a std::map<std::string, float>.
        // Each put() call copies the entire map into a new Piece, then appends
        // it to m_log. With ~60 put() calls/sec and potentially hundreds of
        // profiler keys, this creates significant per-frame allocation overhead.
        //
        // Proposed fix options:
        //   1. Ring buffer of flat arrays: replace std::deque<Piece> with a
        //      fixed-size circular buffer of std::vector<std::pair<std::string,float>>
        //      to avoid map node allocations per frame.
        //   2. Per-graph deques keyed by metric name: store each metric's time
        //      series in its own deque<float>, eliminating redundant string
        //      copies and enabling O(1) per-metric append.
        //   3. Shared string pool: intern metric names once, reference by index
        //      in each frame's data, reducing string copy overhead.
        struct Piece
        {
                Piece(const Profiler::GraphValues &v) : values(v) {}
                Profiler::GraphValues values;
        };
        struct Meta
        {
                float min;
                float max;
                video::SColor color;
                Meta(float initial = 0,
                                video::SColor color = video::SColor(255, 255, 255, 255)) :
                                min(initial),
                                max(initial), color(color)
                {
                }
        };
        std::deque<Piece> m_log;

public:
        u32 m_log_max_size = 200;

        ProfilerGraph() = default;

        void put(const Profiler::GraphValues &values);

        void draw(s32 x_left, s32 y_bottom, video::IVideoDriver *driver,
                        gui::IGUIFont *font) const;
};
