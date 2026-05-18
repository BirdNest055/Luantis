// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"

#include <functional>
#include <vector3d.h>

typedef core::vector3df v3f;
typedef core::vector3d<double> v3d;
typedef core::vector3d<s16> v3s16;
typedef core::vector3d<u16> v3u16;
typedef core::vector3d<s32> v3s32;

// Enable v3s16 as key for std::unordered_map
template <>
struct std::hash<v3s16> {
        size_t operator()(const v3s16 &v) const noexcept {
                size_t h = std::hash<s16>{}(v.X);
                h ^= std::hash<s16>{}(v.Y) + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<s16>{}(v.Z) + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
        }
};
