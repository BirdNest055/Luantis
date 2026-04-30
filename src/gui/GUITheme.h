// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Centralized GUI Theme Configuration
//
// All GUI styling constants (colors, sizes, fonts, spacing, etc.) are defined
// here in one place. To customize the entire GUI appearance, edit this file.
// No other source file should contain hardcoded style values.
//
// TEST-DRIVEN: Every constant in this file must have a corresponding test
// in gui_test/gui_theme_test.cpp that validates its value and usage.

#pragma once

#include "irrlichttypes_bloated.h"
#include <string>

// ============================================================================
// GUITheme — Single source of truth for all GUI styling
// ============================================================================
//
// Design principles:
//   1. All magic numbers for colors, sizes, spacing, and multipliers live here.
//   2. Other files reference GUITheme::Colors::MODAL_BG, never raw SColor values.
//   3. Theme can be reloaded at runtime for hot-reload support.
//   4. Every constant is documented with its purpose and the file(s) that use it.
//
// Color format: video::SColor(alpha, red, green, blue) — ARGB
//
// NOTE: We use a namespace (not a class) because C++ does not allow nested
// namespaces inside a class. Using a namespace allows clean hierarchical access
// like GUITheme::Colors::MODAL_BG while keeping all constants together.
// ============================================================================

namespace GUITheme
{
	// ------------------------------------------------------------------
	// Colors
	// ------------------------------------------------------------------
	// Used by: guiFormSpecMenu, guiPasswordChange, guiVolumeChange,
	//          guiOpenURL, touchscreeneditor, touchcontrols
	namespace Colors
	{
		// Modal overlay backgrounds
		// Semi-transparent black overlay behind modal menus
		const video::SColor MODAL_BG              = video::SColor(140, 0, 0, 0);
		// Fully opaque black background (fullscreen formspecs)
		const video::SColor MODAL_BG_FULLSCREEN    = video::SColor(255, 0, 0, 0);

		// Tooltip colors
		// Used by: guiFormSpecMenu tooltip rendering
		const video::SColor TOOLTIP_BG             = video::SColor(255, 110, 130, 60);
		const video::SColor TOOLTIP_TEXT           = video::SColor(255, 255, 255, 255);
		// Initial tooltip rectangle size
		const core::dimension2d<u32> TOOLTIP_INITIAL_SIZE = core::dimension2d<u32>(110, 18);

		// Default text color
		// Used by: guiFormSpecMenu field/tab text, guiButton default text
		const video::SColor TEXT_DEFAULT           = video::SColor(255, 255, 255, 255);

		// Button colors
		// Used by: guiButton when no style override is set
		const video::SColor BUTTON_BG_DEFAULT      = video::SColor(255, 255, 255, 255);
		const video::SColor BUTTON_TEXT_DEFAULT     = video::SColor(255, 255, 255, 255);
		// Override color for pressed-state blending
		const video::SColor BUTTON_OVERRIDE_DEFAULT = video::SColor(101, 255, 255, 255);

		// Inventory slot colors
		// Used by: guiInventoryList
		const video::SColor SLOT_BORDER            = video::SColor(200, 0, 0, 0);
		const video::SColor SLOT_BG_NORMAL         = video::SColor(255, 128, 128, 128);
		const video::SColor SLOT_BG_HOVERED        = video::SColor(255, 192, 192, 192);

		// Table colors
		// Used by: guiTable
		const video::SColor TABLE_TEXT_DEFAULT     = video::SColor(255, 255, 255, 255);
		const video::SColor TABLE_BG_DEFAULT       = video::SColor(255, 0, 0, 0);
		const video::SColor TABLE_HIGHLIGHT        = video::SColor(255, 70, 100, 50);
		const video::SColor TABLE_HIGHLIGHT_TEXT   = video::SColor(255, 255, 255, 255);

		// Status text colors
		// Used by: statusTextHelper
		const video::SColor STATUS_TEXT_FALLBACK   = video::SColor(255, 0, 0, 0);
		const video::SColor STATUS_TEXT_MAIN_BG    = video::SColor(220, 0, 0, 0);
		const video::SColor STATUS_TEXT_DEFAULT_BG = video::SColor(0, 0, 0, 0);

		// Chat console background
		// Used by: guiChatConsole
		const video::SColor CHAT_CONSOLE_BG        = video::SColor(255, 0, 0, 0);
	}

	// ------------------------------------------------------------------
	// Sizing & Layout
	// ------------------------------------------------------------------
	namespace Sizing
	{
		// Button height ratio relative to use_imgsize
		// Used by: guiFormSpecMenu button height calculation
		const float BUTTON_HEIGHT_RATIO            = 15.0f / 13.0f * 0.35f;

		// Alternative button height ratio relative to font line height
		// Used by: guiFormSpecMenu alternative button sizing
		const float BUTTON_ALT_HEIGHT_RATIO        = 0.875f;

		// Default slot spacing ratio relative to imgsize
		// Used by: guiFormSpecMenu inventory list spacing
		const float SLOT_SPACING_RATIO             = 0.25f;

		// Default padding ratio relative to screensize
		// Used by: guiFormSpecMenu padding calculation
		const float PADDING_RATIO                  = 0.05f;

		// Fixed imgsize DPI multiplier
		// Used by: guiFormSpecMenu fixed imgsize scaling
		const float FIXED_IMGSIZE_DPI_MULT         = 0.5555f;

		// Main menu status bar height in pixels
		// Used by: statusTextHelper
		const u32 STATUS_BAR_HEIGHT                = 40;

		// Locked main menu screen size
		// Used by: guiEngine when no device size available
		const core::dimension2d<u32> MAINMENU_LOCKED_SIZE = core::dimension2d<u32>(800, 600);
	}

	// ------------------------------------------------------------------
	// Spacing
	// ------------------------------------------------------------------
	namespace Spacing
	{
		// Modal dialog base sizes (pixels, before scaling)
		// Used by: guiPasswordChange, guiVolumeChange, guiOpenURL, guiPathSelectMenu
		const core::dimension2d<u32> PASSWORD_DIALOG_SIZE = core::dimension2d<u32>(580, 300);
		const core::dimension2d<u32> VOLUME_DIALOG_SIZE   = core::dimension2d<u32>(380, 200);
		const core::dimension2d<u32> OPENURL_DIALOG_SIZE   = core::dimension2d<u32>(580, 300);
	}

	// ------------------------------------------------------------------
	// Button modifiers
	// ------------------------------------------------------------------
	// Used by: guiButton for hover/press color interpolation
	namespace ButtonModifiers
	{
		// Color multiplier when hovering over a button
		const float HOVER_BRIGHTEN                 = 1.25f;
		// Color multiplier when pressing a button
		const float PRESS_DARKEN                    = 0.85f;
	}

	// ------------------------------------------------------------------
	// Fonts
	// ------------------------------------------------------------------
	namespace Fonts
	{
		// Font size mode identifiers matching FontEngine modes
		// Used by: guiFormSpecMenu font resolution
		const std::string FONT_MODE_NORMAL         = "normal";
		const std::string FONT_MODE_MONO           = "mono";
		const std::string FONT_MODE_BOLD           = "bold";
		const std::string FONT_MODE_ITALIC         = "italic";
	}

	// ------------------------------------------------------------------
	// Sounds
	// ------------------------------------------------------------------
	namespace Sounds
	{
		// Default button click sound
		// Used by: guiButton when no style override specifies a sound
		const std::string BUTTON_CLICK             = "";
	}

	// ------------------------------------------------------------------
	// Theme validation (used by tests and init)
	// ------------------------------------------------------------------
	inline bool validate()
	{
		// Colors must have alpha in 0-255 range
		if (Colors::MODAL_BG.getAlpha() > 255) return false;
		if (Colors::TOOLTIP_BG.getAlpha() > 255) return false;
		if (Colors::TEXT_DEFAULT.getAlpha() > 255) return false;
		// Sizing ratios must be positive
		if (Sizing::BUTTON_HEIGHT_RATIO <= 0) return false;
		if (Sizing::SLOT_SPACING_RATIO < 0) return false;
		if (Sizing::PADDING_RATIO < 0) return false;
		// Button modifiers must be positive
		if (ButtonModifiers::HOVER_BRIGHTEN <= 0) return false;
		if (ButtonModifiers::PRESS_DARKEN <= 0) return false;
		return true;
	}

} // namespace GUITheme
