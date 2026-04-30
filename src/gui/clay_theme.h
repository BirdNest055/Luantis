/*
 * Clay Theme — Central GUI Configuration
 *
 * ONE FILE to customize ALL Clay-based GUI appearance.
 * Change colors, sizes, spacing, fonts, labels, and element IDs here.
 * All Clay GUI panels read from these constants — no more hunting
 * through source files for hardcoded values.
 *
 * Part of the Luantis Clay GUI integration (v9.48).
 */

#pragma once

#include "clay.h"

// ============================================================================
//  HOW TO USE THIS FILE
// ============================================================================
//
//  1. To change a COLOR        → edit Theme::Colors::*
//  2. To change a SIZE          → edit Theme::Sizing::*
//  3. To change SPACING/PADDING → edit Theme::Spacing::*
//  4. To change FONT size/ID    → edit Theme::Fonts::*
//  5. To change CORNER RADIUS   → edit Theme::Radius::*
//  6. To change BUTTON LABELS   → edit Theme::Labels::PauseMenu::*
//  7. To change ELEMENT IDs     → edit Theme::IDs::PauseMenu::*
//  8. To change RENDERER CONFIG → edit Theme::Renderer::*
//
//  All values are constexpr — zero runtime overhead, compile-time checked.
//  Clay_Color uses float fields in 0-255 range (Clay convention).
//
// ============================================================================

namespace Theme {

// ============================================================================
//  COLOR PALETTE
// ============================================================================
namespace Colors {

        // --- Backgrounds ---
        // Full-screen dark overlay behind dialogs
        constexpr Clay_Color overlayBg        = {0.0f,   0.0f,   0.0f,   160.0f};
        // Main content panel background
        constexpr Clay_Color contentBg        = {40.0f,  40.0f,  50.0f,  255.0f};
        // Title bar background (darker than content)
        constexpr Clay_Color titleBg          = {30.0f,  30.0f,  40.0f,  255.0f};
        // Button default background
        constexpr Clay_Color buttonBg         = {50.0f,  50.0f,  60.0f,  255.0f};
        // Button hover background
        constexpr Clay_Color buttonHoverBg    = {80.0f,  80.0f,  100.0f, 255.0f};
        // Button pressed background (add if needed)
        constexpr Clay_Color buttonPressedBg  = {65.0f,  65.0f,  80.0f,  255.0f};

        // --- Text ---
        // Title text color
        constexpr Clay_Color titleText        = {255.0f, 255.0f, 255.0f, 255.0f};
        // Normal button text color
        constexpr Clay_Color buttonText       = {220.0f, 220.0f, 220.0f, 255.0f};
        // Hovered button text color (warm highlight)
        constexpr Clay_Color buttonHoverText  = {255.0f, 255.0f, 200.0f, 255.0f};
        // Pressed button text color
        constexpr Clay_Color buttonPressedText = {200.0f, 200.0f, 180.0f, 255.0f};
        // General body/label text color
        constexpr Clay_Color bodyText         = {200.0f, 200.0f, 210.0f, 255.0f};
        // Dimmed/disabled text color
        constexpr Clay_Color dimText          = {140.0f, 140.0f, 150.0f, 255.0f};

        // --- Separators & Borders ---
        // Horizontal separator line
        constexpr Clay_Color separator        = {100.0f, 100.0f, 120.0f, 255.0f};
        // Border around panels
        constexpr Clay_Color panelBorder      = {70.0f,  70.0f,  90.0f,  255.0f};
        // Border around input fields
        constexpr Clay_Color inputBorder      = {90.0f,  90.0f,  110.0f, 255.0f};
        // Border around focused input
        constexpr Clay_Color inputFocusBorder = {120.0f, 140.0f, 200.0f, 255.0f};

        // --- Accents ---
        // Primary accent (for selected items, active tabs)
        constexpr Clay_Color accent           = {100.0f, 140.0f, 220.0f, 255.0f};
        // Danger/destructive action accent
        constexpr Clay_Color danger           = {220.0f, 80.0f,  80.0f,  255.0f};
        // Success/positive accent
        constexpr Clay_Color success          = {80.0f,  200.0f, 120.0f, 255.0f};
        // Warning accent
        constexpr Clay_Color warning          = {240.0f, 180.0f, 60.0f,  255.0f};

        // --- Overlay & Scrim ---
        // Transparent (no overlay)
        constexpr Clay_Color transparent      = {0.0f,   0.0f,   0.0f,   0.0f};

} // namespace Colors

// ============================================================================
//  SIZING (pixel dimensions)
// ============================================================================
namespace Sizing {

        // --- Pause Menu ---
        constexpr int menuWidth            = 400;   // Width of the dialog column
        constexpr int titleHeight          = 60;    // Title bar height
        constexpr int buttonHeight         = 50;    // Standard button height
        constexpr int separatorHeight      = 2;     // Horizontal separator thickness
        constexpr int bottomPadding        = 16;    // Bottom rounded-corner spacer

        // --- General Dialog ---
        constexpr int dialogMinWidth       = 300;   // Minimum dialog width
        constexpr int dialogMaxWidth       = 800;   // Maximum dialog width
        constexpr int inputHeight          = 36;    // Text input field height
        constexpr int checkboxSize         = 20;    // Checkbox square size
        constexpr int sliderHeight         = 20;    // Slider track height
        constexpr int scrollbarWidth       = 16;    // Scrollbar width

        // --- HUD ---
        constexpr int hudPadding           = 12;    // Padding around HUD elements
        constexpr int hotbarSlotSize       = 48;    // Hotbar item slot size
        constexpr int hotbarGap            = 4;     // Gap between hotbar slots

} // namespace Sizing

// ============================================================================
//  SPACING (padding, margins, gaps)
// ============================================================================
namespace Spacing {

        // --- Pause Menu ---
        constexpr int titlePadH            = 16;    // Title horizontal padding
        constexpr int titlePadV            = 16;    // Title vertical padding
        constexpr int buttonPadH           = 16;    // Button horizontal padding
        constexpr int buttonPadV           = 8;     // Button vertical padding
        constexpr int contentChildGap      = 2;     // Gap between content children

        // --- General ---
        constexpr int panelPadding         = 16;    // Default panel inner padding
        constexpr int sectionGap           = 12;    // Gap between sections
        constexpr int itemGap              = 8;     // Gap between items in a list
        constexpr int tinyGap              = 4;     // Tight spacing
        constexpr int largeGap             = 24;    // Generous spacing

} // namespace Spacing

// ============================================================================
//  CORNER RADIUS (pixels)
// ============================================================================
namespace Radius {

        constexpr int dialogRadius         = 12;    // Dialog/panel corner radius
        constexpr int buttonRadius         = 0;     // Button corner radius
        constexpr int inputRadius          = 4;     // Input field corner radius
        constexpr int tooltipRadius        = 6;     // Tooltip corner radius
        constexpr int badgeRadius          = 8;     // Badge/pill corner radius

} // namespace Radius

// ============================================================================
//  FONTS
// ============================================================================
namespace Fonts {

        // Font IDs — map to FontEngine modes in clay_renderer.cpp
        constexpr uint16_t standardFontId  = 0;     // FM_Standard
        constexpr uint16_t monoFontId      = 3;     // FM_Mono

        // --- Pause Menu ---
        constexpr uint16_t titleFontSize   = 28;    // Title text size
        constexpr uint16_t buttonFontSize  = 20;    // Button label size

        // --- General ---
        constexpr uint16_t bodyFontSize    = 16;    // Body text size
        constexpr uint16_t smallFontSize   = 14;    // Small/secondary text
        constexpr uint16_t headingFontSize = 24;    // Section heading size
        constexpr uint16_t hudFontSize     = 16;    // HUD text size
        constexpr uint16_t chatFontSize    = 14;    // Chat message text size

} // namespace Fonts

// ============================================================================
//  ELEMENT IDS (Clay string IDs for interaction/hover tracking)
// ============================================================================
//
//  ELEMENT IDS
//
//  Clay's CLAY_ID() macro requires C string literals, so we use #define
//  macros for IDs that are passed to CLAY_ID(). For IDs used with
//  Clay_GetElementId() (e.g., dynamic button IDs), use the constexpr
//  versions in IDs::PauseMenu below.
//
//  When adding new panels, follow the same pattern:
//    #define THEME_ID_<PANEL>_<ELEMENT> "<String>"
// ============================================================================

// Pause Menu IDs (for use with CLAY_ID())
#define THEME_ID_PAUSE_OVERLAY     "PauseOverlay"
#define THEME_ID_PAUSE_CONTENT     "PauseContent"
#define THEME_ID_PAUSE_TITLE       "PauseTitle"
#define THEME_ID_PAUSE_SEPARATOR   "PauseSep"
#define THEME_ID_PAUSE_BOTTOM_PAD  "PauseBottomPad"

// Pause Menu button IDs (for use with Clay_GetElementId / buttonComponent)
namespace IDs {
        namespace PauseMenu {
                constexpr const char *btnResume   = "BtnResume";
                constexpr const char *btnSettings = "BtnSettings";
                constexpr const char *btnPassword = "BtnChangePassword";
                constexpr const char *btnVolume   = "BtnVolume";
                constexpr const char *btnExitMenu = "BtnExitMenu";
                constexpr const char *btnExitOS   = "BtnExitOS";
        }

        // Future panels — add namespaces for Settings, Inventory, Chat, etc.
        // namespace Settings { ... }
        // namespace Inventory { ... }
        // namespace ChatConsole { ... }

} // namespace IDs

// ============================================================================
//  TEXT LABELS (translatable strings for UI elements)
//
//  For use with CLAY_STRING(), use the #define macros.
//  For use with buttonComponent() / Clay_String construction,
//  use the constexpr versions in Labels::PauseMenu.
// ============================================================================

// Pause Menu labels (for use with CLAY_STRING() / CLAY_TEXT())
#define THEME_LABEL_PAUSE_TITLE       "Game Paused"
#define THEME_LABEL_PAUSE_RESUME      "Resume Game"
#define THEME_LABEL_PAUSE_SETTINGS    "Settings"
#define THEME_LABEL_PAUSE_CHANGEPWD   "Change Password"
#define THEME_LABEL_PAUSE_VOLUME      "Volume"
#define THEME_LABEL_PAUSE_EXIT_MENU   "Exit to Menu"
#define THEME_LABEL_PAUSE_EXIT_OS     "Exit to OS"

namespace Labels {
        namespace PauseMenu {
                constexpr const char *title        = "Game Paused";
                constexpr const char *resume       = "Resume Game";
                constexpr const char *settings     = "Settings";
                constexpr const char *changePwd    = "Change Password";
                constexpr const char *volume       = "Volume";
                constexpr const char *exitToMenu   = "Exit to Menu";
                constexpr const char *exitToOS     = "Exit to OS";
        }

        // Future panels — add namespaces for other dialogs
        // namespace Settings { ... }

} // namespace Labels

// ============================================================================
//  RENDERER CONFIG (debug colors, limits, behavior)
// ============================================================================
namespace Renderer {

        // --- Placeholder / Debug Colors (Irrlicht SColor format: A,R,G,B) ---
        // Missing image placeholder fill (magenta)
        constexpr uint32_t placeholderFillColor = 0xFFFF00FF;
        // Missing image placeholder X lines (black)
        constexpr uint32_t placeholderXColor    = 0xFF000000;
        // Custom element debug outline (semi-transparent gray)
        constexpr uint32_t debugOutlineColor    = 0x50808080;
        // Default image tint (white = no tint)
        constexpr uint32_t defaultImageTint     = 0xFFFFFFFF;
        // Default no-tint detection color (all zeros)
        constexpr uint32_t noTintColor          = 0x00000000;

        // --- Text Measurement ---
        // Fallback width multiplier for monospace estimate when no font available
        constexpr float fallbackTextWidthRatio  = 0.6f;

        // --- Scrolling ---
        // Pixel delta per mouse wheel event (Irrlicht wheel * this = scroll pixels)
        constexpr float scrollWheelMultiplier   = 50.0f;

        // --- Arena ---
        // Default max Clay elements per frame
        constexpr int32_t defaultMaxElements    = 8192;

        // --- Render Logging ---
        // Max frames to log before stopping (prevents huge files)
        constexpr int renderLogMaxFrames        = 5;
        // Path for JSONL render log output
        constexpr const char *renderLogPath     = "/tmp/clay_render_log.jsonl";

} // namespace Renderer

// ============================================================================
//  PRE-BUILT STYLE DECLARATIONS
//
//  These compose the raw values above into ready-to-use Clay_ElementDeclaration
//  structs. Panels reference these instead of building styles inline.
//  Change the values above → these auto-update → entire UI updates.
// ============================================================================

namespace Styles {

        // --- Pause Menu Styles ---

        // Full-screen dark overlay (centered, semi-transparent)
        inline Clay_ElementDeclaration panelOverlay()
        {
                return {
                        .layout = {
                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                .padding = {0, 0, 0, 0},
                                .childGap = 0,
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = Colors::overlayBg,
                };
        }

        // Content column (rounded, sized to menuWidth)
        inline Clay_ElementDeclaration contentColumn()
        {
                return {
                        .layout = {
                                .sizing = {CLAY_SIZING_FIXED(Sizing::menuWidth), CLAY_SIZING_FIT(0)},
                                .padding = {0, 0, 0, 0},
                                .childGap = Spacing::contentChildGap,
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = Colors::contentBg,
                        .cornerRadius = {Radius::dialogRadius, Radius::dialogRadius,
                                Radius::dialogRadius, Radius::dialogRadius},
                };
        }

        // Title bar (top corners rounded)
        inline Clay_ElementDeclaration titleBar()
        {
                return {
                        .layout = {
                                .sizing = {CLAY_SIZING_FIXED(Sizing::menuWidth), CLAY_SIZING_FIXED(Sizing::titleHeight)},
                                .padding = {Spacing::titlePadH, Spacing::titlePadH,
                                        Spacing::titlePadV, Spacing::titlePadV},
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                        .backgroundColor = Colors::titleBg,
                        .cornerRadius = {Radius::dialogRadius, Radius::dialogRadius, 0, 0},
                };
        }

        // Standard button
        inline Clay_ElementDeclaration button()
        {
                return {
                        .layout = {
                                .sizing = {CLAY_SIZING_FIXED(Sizing::menuWidth), CLAY_SIZING_FIXED(Sizing::buttonHeight)},
                                .padding = {Spacing::buttonPadH, Spacing::buttonPadH,
                                        Spacing::buttonPadV, Spacing::buttonPadV},
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                        .backgroundColor = Colors::buttonBg,
                        .cornerRadius = {Radius::buttonRadius, Radius::buttonRadius,
                                Radius::buttonRadius, Radius::buttonRadius},
                };
        }

        // Hovered button
        inline Clay_ElementDeclaration buttonHover()
        {
                Clay_ElementDeclaration s = button();
                s.backgroundColor = Colors::buttonHoverBg;
                return s;
        }

        // Pressed button
        inline Clay_ElementDeclaration buttonPressed()
        {
                Clay_ElementDeclaration s = button();
                s.backgroundColor = Colors::buttonPressedBg;
                return s;
        }

        // Separator line
        inline Clay_ElementDeclaration separator()
        {
                return {
                        .layout = {
                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(Sizing::separatorHeight)},
                                .padding = {0, 0, 0, 0},
                        },
                        .backgroundColor = Colors::separator,
                };
        }

        // Bottom rounded corner spacer
        inline Clay_ElementDeclaration bottomPad()
        {
                return {
                        .layout = {
                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(Sizing::bottomPadding)},
                                .padding = {0, 0, 0, 0},
                        },
                        .backgroundColor = Colors::contentBg,
                        .cornerRadius = {0, 0, Radius::dialogRadius, Radius::dialogRadius},
                };
        }

        // --- Text Config Helpers ---

        // Title text config
        inline Clay_TextElementConfig titleTextConfig()
        {
                return {
                        .textColor = Colors::titleText,
                        .fontId = Fonts::standardFontId,
                        .fontSize = Fonts::titleFontSize,
                };
        }

        // Button text config (normal state)
        inline Clay_TextElementConfig buttonTextConfig()
        {
                return {
                        .textColor = Colors::buttonText,
                        .fontId = Fonts::standardFontId,
                        .fontSize = Fonts::buttonFontSize,
                };
        }

        // Button text config (hover state)
        inline Clay_TextElementConfig buttonHoverTextConfig()
        {
                return {
                        .textColor = Colors::buttonHoverText,
                        .fontId = Fonts::standardFontId,
                        .fontSize = Fonts::buttonFontSize,
                };
        }

        // Button text config (pressed state)
        inline Clay_TextElementConfig buttonPressedTextConfig()
        {
                return {
                        .textColor = Colors::buttonPressedText,
                        .fontId = Fonts::standardFontId,
                        .fontSize = Fonts::buttonFontSize,
                };
        }

        // Body text config
        inline Clay_TextElementConfig bodyTextConfig()
        {
                return {
                        .textColor = Colors::bodyText,
                        .fontId = Fonts::standardFontId,
                        .fontSize = Fonts::bodyFontSize,
                };
        }

        // Small/dim text config
        inline Clay_TextElementConfig dimTextConfig()
        {
                return {
                        .textColor = Colors::dimText,
                        .fontId = Fonts::standardFontId,
                        .fontSize = Fonts::smallFontSize,
                };
        }

} // namespace Styles

} // namespace Theme
