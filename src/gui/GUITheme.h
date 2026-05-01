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
        namespace Colors
        {
                // Modal overlay backgrounds
                // Used by: guiFormSpecMenu, guiPasswordChange, guiVolumeChange,
                //          guiOpenURL, touchscreeneditor, touchcontrols
                const video::SColor MODAL_BG              = video::SColor(140, 0, 0, 0);
                // Fully opaque black background (fullscreen formspecs)
                const video::SColor MODAL_BG_FULLSCREEN    = video::SColor(255, 0, 0, 0);

                // Tooltip colors
                // Used by: guiFormSpecMenu tooltip rendering
                const video::SColor TOOLTIP_BG             = video::SColor(255, 110, 130, 60);
                const video::SColor TOOLTIP_TEXT           = video::SColor(255, 255, 255, 255);

                // Default text color
                // Used by: guiFormSpecMenu field/tab text, guiButton default text,
                //          guiTable, guiChatConsole, guiHyperText, drawItemStack
                const video::SColor TEXT_DEFAULT           = video::SColor(255, 255, 255, 255);

                // Transparent color (alpha=0, fully invisible)
                // Used by: guiButton getActiveColor() fallback, guiEditBoxWithScrollbar bg
                const video::SColor TRANSPARENT            = video::SColor(0, 0, 0, 0);

                // Button colors
                // Used by: guiButton when no style override is set
                const video::SColor BUTTON_BG_DEFAULT      = video::SColor(255, 255, 255, 255);
                const video::SColor BUTTON_TEXT_DEFAULT     = video::SColor(255, 255, 255, 255);
                // Override color for pressed-state blending (semi-transparent white)
                const video::SColor BUTTON_OVERRIDE_DEFAULT = video::SColor(101, 255, 255, 255);

                // Inventory slot colors
                // Used by: guiInventoryList, touchscreeneditor (selection)
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

                // Chat console colors
                // Used by: guiChatConsole
                const video::SColor CHAT_CONSOLE_BG        = video::SColor(255, 0, 0, 0);
                const video::SColor CHAT_CONSOLE_CURSOR    = video::SColor(255, 255, 255, 255);
                const video::SColor CHAT_CONSOLE_TEXT      = video::SColor(255, 255, 255, 255);

                // Touch screen editor colors
                // Used by: touchscreeneditor
                const video::SColor TOUCH_SELECTION        = video::SColor(255, 128, 128, 128);
                const video::SColor TOUCH_ERROR            = video::SColor(255, 255, 0, 0);

                // HyperText default colors (CSS-style hex)
                // Used by: guiHyperText m_root_tag defaults
                const video::SColor HYPERTEXT_DEFAULT      = video::SColor(255, 238, 238, 238); // #EEEEEE
                const video::SColor HYPERTEXT_HOVER        = video::SColor(255, 255, 0, 0);     // #FF0000
                const video::SColor HYPERTEXT_LINK         = video::SColor(255, 0, 0, 255);     // #0000FF

                // Item rendering colors
                // Used by: drawItemStack
                const video::SColor ITEM_WEAR_BG           = video::SColor(255, 0, 0, 0);       // Tool durability bar bg
                const video::SColor ITEM_NO_TEXTURE        = video::SColor(255, 255, 255, 255);  // No-texture fallback
                const video::SColor ITEM_COUNT_TEXT        = video::SColor(255, 255, 255, 255);  // Stack count text

                // Focus/debug colors
                // Used by: guiFormSpecMenu
                const video::SColor FOCUS_BORDER           = video::SColor(255, 255, 255, 255);  // Focused element border
                const video::SColor DEBUG_HIGHLIGHT        = video::SColor(34, 255, 255, 0);     // 0x22FFFF00 debug rect

                // Image rendering color (opaque white = draw as-is)
                // Used by: guiAnimatedImage, guiBackgroundImage, guiItemImage
                const video::SColor IMAGE_DRAW_DEFAULT     = video::SColor(255, 255, 255, 255);

                // Profiler graph color palette
                // Used by: profilergraph.cpp
                const video::SColor PROFILER_COLOR_1       = video::SColor(0xffc5000b);
                const video::SColor PROFILER_COLOR_2       = video::SColor(0xffff950e);
                const video::SColor PROFILER_COLOR_3       = video::SColor(0xffaecf00);
                const video::SColor PROFILER_COLOR_4       = video::SColor(0xffffd320);
                const video::SColor PROFILER_COLOR_5       = video::SColor(0xffff420e);
                const video::SColor PROFILER_COLOR_6       = video::SColor(0xffe08080);
                const video::SColor PROFILER_COLOR_7       = video::SColor(0xff729fcf);
                const video::SColor PROFILER_COLOR_8       = video::SColor(0xffff99cc);
                const video::SColor PROFILER_FALLBACK      = video::SColor(255, 200, 200, 200);

                // Nearly transparent background for edit boxes
                // Used by: guiEditBoxWithScrollbar
                const video::SColor EDIT_BG_OVERRIDE       = video::SColor(0x00000001);
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

                // Fixed imgsize DPI multiplier (approx 0.53 inches)
                // Used by: guiFormSpecMenu fixed imgsize scaling
                const float FIXED_IMGSIZE_DPI_MULT         = 0.5555f;

                // Main menu status bar height in pixels
                // Used by: statusTextHelper
                const u32 STATUS_BAR_HEIGHT                = 40;

                // Locked screen size for formspec menus
                // Used by: guiFormSpecMenu, guiEngine
                const core::dimension2d<u32> LOCK_SIZE     = core::dimension2d<u32>(800, 600);

                // Tooltip initial rectangle size
                // Used by: guiFormSpecMenu
                const core::dimension2d<u32> TOOLTIP_INITIAL_SIZE = core::dimension2d<u32>(110, 18);

                // Tooltip Y-axis padding in pixels
                // Used by: guiFormSpecMenu
                const s32 TOOLTIP_PADDING_Y                = 5;

                // Form fallback dimensions for non-sized forms
                // Used by: guiFormSpecMenu, guiPasswordChange, guiOpenURL, guiVolumeChange
                const float FORM_FALLBACK_WIDTH            = 580.0f;
                const float FORM_FALLBACK_HEIGHT           = 300.0f;

                // Status text Y offset from bottom of screen
                // Used by: statusTextHelper
                const s32 STATUS_TEXT_Y_OFFSET             = 150;

                // Checkbox padding in pixels
                // Used by: guiFormSpecMenu checkbox element
                const s32 CHECKBOX_PADDING                 = 7;

                // Focus border width in pixels
                // Used by: guiFormSpecMenu focus highlight
                const s32 FOCUS_BORDER_WIDTH               = 2;

                // Inventory slot border width in pixels
                // Used by: guiInventoryList
                const s32 SLOT_BORDER_WIDTH                = 1;

                // Table row height padding in pixels (added to font height)
                // Used by: guiTable
                const s32 TABLE_ROW_PADDING                = 4;

                // Table text list default X position in pixels
                // Used by: guiTable
                const s32 TABLE_TEXT_X_POS                 = 6;

                // Table default em width fallback in pixels
                // Used by: guiTable
                const s32 TABLE_EM_FALLBACK                = 6;

                // Table column padding as fraction of em
                // Used by: guiTable
                const float TABLE_COL_PADDING_EM           = 0.5f;

                // Table indent width as multiple of em
                // Used by: guiTable
                const float TABLE_INDENT_EM                = 1.5f;

                // Chat console scrollbar width in pixels
                // Used by: guiChatConsole
                const s32 CHAT_SCROLLBAR_WIDTH             = 30;

                // Chat console initial element size
                // Used by: guiChatConsole
                const core::dimension2d<s32> CHAT_INITIAL_SIZE = core::dimension2d<s32>(100, 100);

                // Edit box scrollbar step multipliers
                // Used by: guiEditBoxWithScrollbar
                const float EDIT_SCROLL_SMALL_STEP         = 3.0f;  // In font heights
                const float EDIT_SCROLL_LARGE_STEP         = 10.0f; // In font heights

                // Modal menu default element size
                // Used by: modalMenu
                const core::dimension2d<s32> MODAL_DEFAULT_SIZE = core::dimension2d<s32>(100, 100);

                // Double-click position threshold in pixels
                // Used by: modalMenu
                const float DOUBLECLICK_DISTANCE           = 30.0f;

                // GUI scale clamping range
                // Used by: modalMenu
                const float GUI_SCALE_MIN                  = 0.5f;
                const float GUI_SCALE_MAX                  = 20.0f;

                // HyperText default margins in pixels
                // Used by: guiHyperText
                const s32 HYPERTEXT_MARGIN_DEFAULT         = 10;
                const s32 HYPERTEXT_MARGIN_PARAGRAPH       = 10;
                const s32 HYPERTEXT_MARGIN_GLOBAL          = 3;

                // HyperText default image/item size
                // Used by: guiHyperText
                const core::dimension2d<u32> HYPERTEXT_IMAGE_SIZE = core::dimension2d<u32>(80, 80);

                // HyperText scrollbar width fallback in pixels
                // Used by: guiHyperText
                const s32 HYPERTEXT_SCROLLBAR_WIDTH        = 16;

                // HyperText font size scaling divisor
                // Used by: guiHyperText
                const float HYPERTEXT_FONT_SCALE_DIVISOR   = 16.0f;

                // Item wear bar dimensions (slot_size / WEAR_BAR_DIVISOR)
                // Used by: drawItemStack
                const s32 WEAR_BAR_DIVISOR                 = 16;

                // Item count text Y offset from bottom of slot (fraction)
                // Used by: drawItemStack (not a constant currently, but derived)

                // Main menu header/footer padding
                // Used by: guiEngine
                const s32 ENGINE_TEXT_PADDING              = 4;
                const s32 ENGINE_HEADER_PAD                = 4;
                const s32 ENGINE_HEADER_INNER_PAD          = 8;
                const s32 ENGINE_HEADER_Y_OFFSET           = 10;
                const s32 ENGINE_SIDEBAR_OFFSET            = 320;

                // Profiler graph dimensions
                // Used by: profilergraph
                const s32 PROFILER_GRAPH_HEIGHT            = 52;
                const s32 PROFILER_TEXT_X_OFFSET           = 15;
                const s32 PROFILER_TEXT_WIDTH              = 200;

                // guiButton interpolation factor
                // Used by: guiButton setColor()
                const float BUTTON_COLOR_INTERPOLATE       = 0.65f;

                // 3D scene camera defaults
                // Used by: guiScene
                const float SCENE_CAMERA_FOV               = 30.0f;
                const float SCENE_CAMERA_DISTANCE_MULT     = 0.5f;
                const float SCENE_ROTATION_SPEED           = 0.03f;
                const float SCENE_ROTATION_MAX_1           = 60.0f;
                const float SCENE_ROTATION_MAX_2           = 300.0f;

                // Item rendering default rotation speed
                // Used by: drawItemStack
                const s16 ITEM_RENDER_ROTATION_Y           = 100;

                // Inventory list spacing X ratio (old coordinate system)
                // Used by: guiFormSpecMenu inventory list X spacing
                const float SLOT_SPACING_X_RATIO           = 5.0f / 4.0f;

                // Inventory list spacing Y ratio (old coordinate system)
                // Used by: guiFormSpecMenu inventory list Y spacing
                const float SLOT_SPACING_Y_RATIO           = 15.0f / 13.0f;

                // Inventory padding ratio (old coordinate system)
                // Used by: guiFormSpecMenu inventory padding
                const float INV_PADDING_RATIO              = 3.0f / 8.0f;

                // Button height vertical offset ratio
                // Used by: guiFormSpecMenu button Y offset
                const float BTN_HEIGHT_OFFSET_RATIO        = 2.0f / 3.0f;

                // Font line height ratio
                // Used by: guiFormSpecMenu font line height calculation
                const float FONT_LINE_HEIGHT_RATIO         = 2.0f / 5.0f;

                // Old coordinate system button height multiplier
                // Used by: guiFormSpecMenu old coordinate system
                const float BTN_HEIGHT_MULT                = 0.35f;

                // Old coordinate system button offset ratio
                // Used by: guiFormSpecMenu old coordinate system button offset
                const float BTN_OFFSET_RATIO               = 0.85f;

                // Old coordinate system padding half ratio
                // Used by: guiFormSpecMenu old coordinate system padding
                const float PADDING_HALF_RATIO             = 0.5f;
        }

        // ------------------------------------------------------------------
        // Timing
        // ------------------------------------------------------------------
        namespace Timing
        {
                // Status text display durations in seconds
                // Used by: statusTextHelper
                const float STATUS_TEXT_DURATION_GAME      = 1.5f;
                const float STATUS_TEXT_DURATION_MENU      = 3.0f;

                // Chat console cursor blink speed
                // Used by: guiChatConsole
                const float CHAT_CURSOR_BLINK_SPEED        = 2.0f;

                // Chat console cursor height ratio
                // Used by: guiChatConsole
                const float CHAT_CURSOR_HEIGHT_RATIO       = 0.1f;

                // Chat console open/close animation speed
                // Used by: guiChatConsole
                const float CHAT_HEIGHT_SPEED              = 5.0f;

                // Chat console reopen inhibit time in ms
                // Used by: guiChatConsole
                const u32 CHAT_REOPEN_INHIBIT_MS           = 50;
                const u32 CHAT_REOPEN_INHIBIT_ESC_MS       = 1;

                // Weblink click debounce in ms
                // Used by: guiChatConsole
                const u32 CHAT_WEBLINK_DEBOUNCE_MS         = 600;

                // Mouse wheel scroll multiplier
                // Used by: guiChatConsole, guiTable
                const float MOUSE_WHEEL_SCROLL_MULTIPLIER  = 3.0f;

                // Double-click detection threshold in ms
                // Used by: modalMenu
                const u32 DOUBLECLICK_THRESHOLD_MS         = 400;

                // Table keyboard navigation timeout in ms
                // Used by: guiTable
                const u32 TABLE_KEYNAV_TIMEOUT_MS          = 500;

                // Cloud animation step multiplier
                // Used by: guiEngine
                const float ENGINE_CLOUD_STEP_MULT         = 3.0f;
        }

        // ------------------------------------------------------------------
        // Button modifiers
        // ------------------------------------------------------------------
        // Used by: guiButton for hover/press color interpolation
        namespace ButtonModifiers
        {
                // Color multiplier when hovering over a button (brightens 25%)
                const float HOVER_BRIGHTEN                 = 1.25f;
                // Color multiplier when pressing a button (darkens 15%)
                const float PRESS_DARKEN                    = 0.85f;
        }

        // ------------------------------------------------------------------
        // Fonts
        // ------------------------------------------------------------------
        namespace Fonts
        {
                // Font size mode identifiers matching FontEngine modes
                // Used by: guiFormSpecMenu font resolution, guiHyperText
                const std::string FONT_MODE_NORMAL         = "normal";
                const std::string FONT_MODE_MONO           = "mono";
                const std::string FONT_MODE_BOLD           = "bold";
                const std::string FONT_MODE_ITALIC         = "italic";

                // Default font size strings
                // Used by: guiHyperText
                const std::string FONT_SIZE_DEFAULT        = "16";
                const std::string FONT_SIZE_BIG            = "24";
                const std::string FONT_SIZE_BIGGER         = "36";

                // Font size scaling divisor
                // Used by: guiHyperText, guiTable
                const float FONT_SCALE_DIVISOR             = 16.0f;
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
        // Dialog dimensions
        // ------------------------------------------------------------------
        // Standard dialog sizes used by modal menus
        namespace Dialogs
        {
                // Password change dialog
                const core::dimension2d<u32> PASSWORD_CHANGE_SIZE = core::dimension2d<u32>(580, 300);
                // Open URL dialog
                const core::dimension2d<u32> OPEN_URL_SIZE = core::dimension2d<u32>(580, 250);
                // Volume change dialog
                const core::dimension2d<u32> VOLUME_CHANGE_SIZE = core::dimension2d<u32>(380, 200);
                // Path select dialog
                const float PATH_SELECT_WIDTH              = 600.0f;
                const float PATH_SELECT_HEIGHT             = 400.0f;
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
                // Status bar height must be positive
                if (Sizing::STATUS_BAR_HEIGHT == 0) return false;
                // Timing durations must be positive
                if (Timing::STATUS_TEXT_DURATION_GAME <= 0) return false;
                if (Timing::STATUS_TEXT_DURATION_MENU <= 0) return false;
                // Dialog sizes must be positive
                if (Dialogs::PASSWORD_CHANGE_SIZE.Width == 0) return false;
                if (Dialogs::OPEN_URL_SIZE.Width == 0) return false;
                if (Dialogs::VOLUME_CHANGE_SIZE.Width == 0) return false;
                // Focus border width must be positive
                if (Sizing::FOCUS_BORDER_WIDTH <= 0) return false;
                // Chat timing must be positive
                if (Timing::CHAT_CURSOR_BLINK_SPEED <= 0) return false;
                if (Timing::CHAT_HEIGHT_SPEED <= 0) return false;
                return true;
        }

} // namespace GUITheme
