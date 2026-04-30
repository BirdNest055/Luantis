// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Centralized GUI Theme — Implementation
//
// This file provides the runtime support for GUITheme:
//   - Theme validation
//   - Future: theme loading from config files
//   - Future: hot-reload support

#include "GUITheme.h"
#include "log.h"

// Initialize the theme system.
// Called once at startup; logs the active theme configuration.
void GUITheme_Init()
{
        if (!GUITheme::validate()) {
                errorstream << "GUITheme: Theme validation failed!" << std::endl;
                return;
        }

        infostream << "GUITheme: Initialized successfully" << std::endl;
        infostream << "  Modal BG: alpha=" << GUITheme::Colors::MODAL_BG.getAlpha()
                   << " r=" << GUITheme::Colors::MODAL_BG.getRed()
                   << " g=" << GUITheme::Colors::MODAL_BG.getGreen()
                   << " b=" << GUITheme::Colors::MODAL_BG.getBlue() << std::endl;
        infostream << "  Button hover mult=" << GUITheme::ButtonModifiers::HOVER_BRIGHTEN
                   << " press mult=" << GUITheme::ButtonModifiers::PRESS_DARKEN << std::endl;
}
