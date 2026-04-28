/*
Luantis — Clay Text Measurer
Bridges FontEngine to Clay's text measurement callback.
*/

#pragma once

#include <clay.h>
#include <unordered_map>

namespace irr { namespace gui { class IGUIFont; } }
class FontEngine;

/// Provides text measurement for Clay's layout engine by bridging
/// to Luantis's existing FontEngine (which uses CGUITTFont / FreeType).
class ClayTextMeasurer {
public:
        ClayTextMeasurer(FontEngine *font_engine);

        /// Install this as Clay's measureText callback via Clay_SetMeasureTextFunction
        void install();

        /// The static callback that Clay calls. Forwards to the instance.
        static Clay_Dimensions measureText(Clay_StringSlice text,
                Clay_TextElementConfig *config, void *userData);

        /// Encode font parameters into a uintptr_t for Clay_TextElementConfig.userData
        /// @param size    Font size in pixels
        /// @param mode    FontMode (FM_Standard=0, FM_Mono=1, FM_Fallback=2)
        /// @param bold    Bold flag
        /// @param italic  Italic flag
        static uintptr_t encodeFontSpec(u32 size, u32 mode = 0, bool bold = false, bool italic = false);

        /// Decode font parameters from a uintptr_t
        static void decodeFontSpec(uintptr_t encoded, u32 &size, u32 &mode, bool &bold, bool &italic);

private:
        FontEngine *m_font_engine;
};
