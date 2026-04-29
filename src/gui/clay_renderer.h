/*
 * Clay Rendering Backend for Irrlicht
 *
 * Translates Clay render commands into Irrlicht draw calls.
 * Clay outputs a flat array of render commands (rectangles, text, images,
 * custom elements, etc.) and this renderer composites them onto the screen
 * using Irrlicht's 2D drawing primitives.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

#pragma once

#include <cstdint>
#include <vector>

// Clay header — designated initializers need C++20
// Using relative path from project root (added to include directories)
#include "clay.h"

namespace irr {
class IrrlichtDevice;
namespace video {
class IVideoDriver;
}
namespace gui {
class IGUIFont;
}
}

class FontEngine;

/** Handles rendering Clay's output via Irrlicht's 2D draw API. */
class ClayIrrlichtRenderer {
public:
        ClayIrrlichtRenderer() = default;
        ~ClayIrrlichtRenderer() = default;

        /** Initialize with the Irrlicht device. Must be called once after device creation. */
        void init(irr::IrrlichtDevice *device);

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
        irr::video::IVideoDriver *m_driver = nullptr;
        FontEngine *m_font_engine = nullptr;

        /** Map Clay fontId → Irrlicht IGUIFont*. Populated from FontEngine. */
        irr::gui::IGUIFont *getFont(uint16_t fontId, uint16_t fontSize);

        void drawRectangle(const Clay_RenderCommand &cmd);
        void drawText(const Clay_RenderCommand &cmd);
        void drawImage(const Clay_RenderCommand &cmd);
        void drawBorder(const Clay_RenderCommand &cmd);
        void drawCustom(const Clay_RenderCommand &cmd);
};
