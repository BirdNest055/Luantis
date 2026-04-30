/*
 * Clay Rendering Backend for Irrlicht
 *
 * Translates Clay render commands into Irrlicht draw calls.
 * Supports rectangles, text, images (with 9-slice and DPI scaling),
 * borders, custom elements, and overlay color tinting.
 *
 * Part of the Luantis Clay GUI integration (v9.49).
 */

#pragma once

#include <cstdint>
#include <vector>

// Clay header — designated initializers need C++20
// Using relative path from project root (added to include directories)
#include "clay.h"

// Forward declarations for Irrlicht types.
// In this Irrlicht fork, sub-namespaces (core, video, gui, etc.)
// are at the global level — NOT wrapped in an outer irr:: namespace.
class IrrlichtDevice;

namespace video {
class IVideoDriver;
class ITexture;
}

namespace gui {
class IGUIFont;
}

// We can't forward-declare core::rect<s32> easily, so we use
// a void pointer for the clip rect in the header and cast in the .cpp
class FontEngine;

/**
 * Custom render data tags for Clay image commands.
 *
 * Clay's imageData is a void* — we use it to pass either:
 *   - A raw video::ITexture* (simple stretch)
 *   - A ClayImageInfo* (9-slice with middle rect)
 *
 * The tag byte at offset 0 distinguishes them:
 *   0x00 = raw ITexture* (legacy/default)
 *   0x01 = ClayImageInfo* (9-slice)
 */
#define CLAY_IMAGE_TAG_SIMPLE  0x00
#define CLAY_IMAGE_TAG_9SLICE  0x01

struct ClayImageInfo {
        uint8_t tag = CLAY_IMAGE_TAG_9SLICE;
        video::ITexture *texture = nullptr;
        int middleInset = 4;  // Inset from each edge for 9-slice middle rect
};

/** Handles rendering Clay's output via Irrlicht's 2D draw API. */
class ClayIrrlichtRenderer {
public:
        ClayIrrlichtRenderer() = default;
        ~ClayIrrlichtRenderer() = default;

        /** Initialize with the Irrlicht device. Must be called once after device creation. */
        void init(IrrlichtDevice *device);

        /** Render all commands produced by Clay_EndLayout(). */
        void render(const Clay_RenderCommandArray &commands);

        /** Set the font engine for text measurement and rendering. */
        void setFontEngine(FontEngine *engine);

        /**
         * Clay text-measure callback.
         * This is passed to Clay_SetMeasureTextFunction() and must match the
         * Clay_Dimensions (*)(Clay_StringSlice, Clay_TextElementConfig*, void*)
         * signature.
         */
        static Clay_Dimensions measureText(Clay_StringSlice text,
                Clay_TextElementConfig *config, void *userData);

private:
        video::IVideoDriver *m_driver = nullptr;
        FontEngine *m_font_engine = nullptr;

        /** Map Clay fontId -> Irrlicht IGUIFont*. Populated from FontEngine. */
        gui::IGUIFont *getFont(uint16_t fontId, uint16_t fontSize);

        // Internal draw helpers (clip rect passed as void* to avoid Irrlicht
        // header dependency; cast to core::rect<s32>* in the .cpp)
        void drawRectangle(const Clay_RenderCommand &cmd, void *clipRect);
        void drawText(const Clay_RenderCommand &cmd, void *clipRect);
        void drawImage(const Clay_RenderCommand &cmd, void *clipRect);
        void drawBorder(const Clay_RenderCommand &cmd, void *clipRect);
        void drawCustom(const Clay_RenderCommand &cmd, void *clipRect);
};
