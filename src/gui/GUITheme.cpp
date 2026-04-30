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

	infostream << "GUITheme: Initialized successfully (v9.50)" << std::endl;

	// Log color categories
	infostream << "  Colors: " << "22 base + 13 extended = 35 total" << std::endl;
	infostream << "    MODAL_BG: alpha=" << GUITheme::Colors::MODAL_BG.getAlpha()
		   << " r=" << GUITheme::Colors::MODAL_BG.getRed()
		   << " g=" << GUITheme::Colors::MODAL_BG.getGreen()
		   << " b=" << GUITheme::Colors::MODAL_BG.getBlue() << std::endl;
	infostream << "    HYPERTEXT_DEFAULT: r=" << GUITheme::Colors::HYPERTEXT_DEFAULT.getRed()
		   << " g=" << GUITheme::Colors::HYPERTEXT_DEFAULT.getGreen()
		   << " b=" << GUITheme::Colors::HYPERTEXT_DEFAULT.getBlue() << std::endl;
	infostream << "    ITEM_WEAR_BG: r=" << GUITheme::Colors::ITEM_WEAR_BG.getRed()
		   << " g=" << GUITheme::Colors::ITEM_WEAR_BG.getGreen()
		   << " b=" << GUITheme::Colors::ITEM_WEAR_BG.getBlue() << std::endl;

	// Log sizing categories
	infostream << "  Sizing: 12 base + 30 extended = 42 total" << std::endl;
	infostream << "    CHECKBOX_PADDING=" << GUITheme::Sizing::CHECKBOX_PADDING
		   << " FOCUS_BORDER_WIDTH=" << GUITheme::Sizing::FOCUS_BORDER_WIDTH
		   << " SLOT_BORDER_WIDTH=" << GUITheme::Sizing::SLOT_BORDER_WIDTH << std::endl;
	infostream << "    CHAT_SCROLLBAR_WIDTH=" << GUITheme::Sizing::CHAT_SCROLLBAR_WIDTH
		   << " PROFILER_GRAPH_HEIGHT=" << GUITheme::Sizing::PROFILER_GRAPH_HEIGHT << std::endl;

	// Log timing categories
	infostream << "  Timing: 2 base + 9 extended = 11 total" << std::endl;
	infostream << "    CHAT_CURSOR_BLINK_SPEED=" << GUITheme::Timing::CHAT_CURSOR_BLINK_SPEED
		   << " CHAT_HEIGHT_SPEED=" << GUITheme::Timing::CHAT_HEIGHT_SPEED << std::endl;
	infostream << "    DOUBLECLICK_THRESHOLD_MS=" << GUITheme::Timing::DOUBLECLICK_THRESHOLD_MS
		   << " TABLE_KEYNAV_TIMEOUT_MS=" << GUITheme::Timing::TABLE_KEYNAV_TIMEOUT_MS << std::endl;

	// Log button modifiers
	infostream << "  ButtonModifiers: hover mult=" << GUITheme::ButtonModifiers::HOVER_BRIGHTEN
		   << " press mult=" << GUITheme::ButtonModifiers::PRESS_DARKEN << std::endl;

	// Log dialog dimensions
	infostream << "  Dialogs: PASSWORD_CHANGE="
		   << GUITheme::Dialogs::PASSWORD_CHANGE_SIZE.Width << "x"
		   << GUITheme::Dialogs::PASSWORD_CHANGE_SIZE.Height
		   << " OPEN_URL=" << GUITheme::Dialogs::OPEN_URL_SIZE.Width << "x"
		   << GUITheme::Dialogs::OPEN_URL_SIZE.Height
		   << " VOLUME_CHANGE=" << GUITheme::Dialogs::VOLUME_CHANGE_SIZE.Width << "x"
		   << GUITheme::Dialogs::VOLUME_CHANGE_SIZE.Height << std::endl;

	// Log fonts
	infostream << "  Fonts: modes=[" << GUITheme::Fonts::FONT_MODE_NORMAL
		   << "," << GUITheme::Fonts::FONT_MODE_MONO
		   << "," << GUITheme::Fonts::FONT_MODE_BOLD
		   << "," << GUITheme::Fonts::FONT_MODE_ITALIC << "]"
		   << " sizes=[" << GUITheme::Fonts::FONT_SIZE_DEFAULT
		   << "," << GUITheme::Fonts::FONT_SIZE_BIG
		   << "," << GUITheme::Fonts::FONT_SIZE_BIGGER << "]" << std::endl;
}
