// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "tile.h"
#include <cassert>

video::ITexture *AnimationInfo::getTexture(float animation_time) const
{
        if (getFrameCount() == 0)
                return nullptr;

        // Figure out current frame
        u16 frame = (u32)(animation_time * 1000.0f / std::max<u16>(1, m_frame_length_ms))
                        % m_frame_count;

        assert(frame < m_frames->size());
        return (*m_frames)[frame].texture;
}

void AnimationInfo::updateTexture(video::SMaterial &material, float animation_time)
{
        video::ITexture *texture = getTexture(animation_time);
        if (texture) {
                material.setTexture(0, texture);
        }
}

void TileLayer::applyMaterialOptions(video::SMaterial &material, int layer) const
{
        material.setTexture(0, texture);

        material.BackfaceCulling = (material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
        if (!(material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)) {
                material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
                material.TextureLayers[1].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
        }
        if (!(material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL)) {
                material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
                material.TextureLayers[1].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
        }

        /*
         * The second layer is for overlays, but uses the same vertex positions
         * as the first, which easily leads to Z-fighting.
         * To fix this we offset the polygons of the *first layer* away from the camera.
         * This only affects the depth buffer and leads to no visual gaps in geometry.
         *
         * However, doing so intrudes the "Z space" of the overlay of the next node
         * so that leads to inconsistent Z-sorting again. :(
         * HACK: PolygonOffset is applied to the base layer only when an overlay
         * exists. Without this guard, the offset would also shift the base layer
         * relative to adjacent nodes' overlays, causing inconsistent Z-sorting.
         * Removal requires a rendering approach that properly layers base and
         * overlay geometry (e.g., multi-pass rendering or depth-prepass).
         *
         * Root cause: Applying PolygonOffset to the first layer prevents Z-fighting
         * with the overlay, but it also shifts the first layer relative to the
         * *next node's* overlay, causing inconsistent Z-sorting between adjacent
         * nodes. The offset is only needed when an overlay layer exists, so we
         * conditionally enable it via need_polygon_offset.
         *
         * Why this is a hack: The fundamental problem is that two layers of
         * geometry at the same depth cannot be perfectly Z-sorted without
         * per-polygon depth peeling or order-independent transparency. Polygon
         * offset is a workaround that biases one layer slightly forward/backward.
         *
         * Proposed removal: Use a custom shader with gl_FragDepth adjustment
         * for the overlay pass instead of fixed-function PolygonOffset. This
         * would give per-fragment control over depth bias and eliminate the
         * cross-node interference. Alternatively, render overlays in a separate
         * pass with a small depth bias uniform.
         */
        if (need_polygon_offset) {
                material.PolygonOffsetSlopeScale = 1;
                material.PolygonOffsetDepthBias = 1;
        }
}
