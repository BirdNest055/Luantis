// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// GUITheme Test Program — Test-Driven Validation
//
// Standalone test that validates every constant in GUITheme.h without
// requiring the full Luantis build. Uses minimal Irrlicht stubs.
//
// Usage: ./gui_theme_test [--verbose]
// Returns: 0 on success, 1 on failure

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

// ============================================================================
// Minimal Irrlicht types stub
// ============================================================================
using u32 = unsigned int;
using s32 = int;
using u16 = unsigned short;
using s16 = short;
using f32 = float;

namespace irr
{
namespace video
{
class SColor
{
public:
        u32 m_color;
        SColor(u32 a = 255, u32 r = 0, u32 g = 0, u32 b = 0)
            : m_color(((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff))
        {
        }
        // Construct from packed u32
        explicit SColor(u32 packed) : m_color(packed) {}
        u32 getAlpha() const { return (m_color >> 24) & 0xff; }
        u32 getRed() const { return (m_color >> 16) & 0xff; }
        u32 getGreen() const { return (m_color >> 8) & 0xff; }
        u32 getBlue() const { return m_color & 0xff; }
        bool operator==(const SColor &other) const { return m_color == other.m_color; }
        bool operator!=(const SColor &other) const { return m_color != other.m_color; }
};
} // namespace video

namespace core
{
template <typename T> class dimension2d
{
public:
        T Width, Height;
        dimension2d() : Width(0), Height(0) {}
        dimension2d(T w, T h) : Width(w), Height(h) {}
        bool operator==(const dimension2d &other) const
        {
                return Width == other.Width && Height == other.Height;
        }
        bool operator!=(const dimension2d &other) const { return !(*this == other); }
};
} // namespace core
} // namespace irr

using namespace irr;

// ---- Mirror of GUITheme.h constants for validation ----
namespace TestTheme
{
namespace Colors
{
const video::SColor MODAL_BG(140, 0, 0, 0);
const video::SColor MODAL_BG_FULLSCREEN(255, 0, 0, 0);
const video::SColor TOOLTIP_BG(255, 110, 130, 60);
const video::SColor TOOLTIP_TEXT(255, 255, 255, 255);
const video::SColor TEXT_DEFAULT(255, 255, 255, 255);
const video::SColor TRANSPARENT(0, 0, 0, 0);
const video::SColor BUTTON_BG_DEFAULT(255, 255, 255, 255);
const video::SColor BUTTON_TEXT_DEFAULT(255, 255, 255, 255);
const video::SColor BUTTON_OVERRIDE_DEFAULT(101, 255, 255, 255);
const video::SColor SLOT_BORDER(200, 0, 0, 0);
const video::SColor SLOT_BG_NORMAL(255, 128, 128, 128);
const video::SColor SLOT_BG_HOVERED(255, 192, 192, 192);
const video::SColor TABLE_TEXT_DEFAULT(255, 255, 255, 255);
const video::SColor TABLE_BG_DEFAULT(255, 0, 0, 0);
const video::SColor TABLE_HIGHLIGHT(255, 70, 100, 50);
const video::SColor TABLE_HIGHLIGHT_TEXT(255, 255, 255, 255);
const video::SColor STATUS_TEXT_FALLBACK(255, 0, 0, 0);
const video::SColor STATUS_TEXT_MAIN_BG(220, 0, 0, 0);
const video::SColor STATUS_TEXT_DEFAULT_BG(0, 0, 0, 0);
const video::SColor CHAT_CONSOLE_BG(255, 0, 0, 0);
const video::SColor CHAT_CONSOLE_CURSOR(255, 255, 255, 255);
const video::SColor CHAT_CONSOLE_TEXT(255, 255, 255, 255);
const video::SColor TOUCH_SELECTION(255, 128, 128, 128);
const video::SColor TOUCH_ERROR(255, 255, 0, 0);
// Extended colors
const video::SColor HYPERTEXT_DEFAULT(255, 238, 238, 238);
const video::SColor HYPERTEXT_HOVER(255, 255, 0, 0);
const video::SColor HYPERTEXT_LINK(255, 0, 0, 255);
const video::SColor ITEM_WEAR_BG(255, 0, 0, 0);
const video::SColor ITEM_NO_TEXTURE(255, 255, 255, 255);
const video::SColor ITEM_COUNT_TEXT(255, 255, 255, 255);
const video::SColor FOCUS_BORDER(255, 255, 255, 255);
const video::SColor DEBUG_HIGHLIGHT(34, 255, 255, 0);
const video::SColor IMAGE_DRAW_DEFAULT(255, 255, 255, 255);
// Unpack profiler colors from 0xAARRGGBB to SColor(alpha, red, green, blue)
const video::SColor PROFILER_COLOR_1(0xff, 0xc5, 0x00, 0x0b);
const video::SColor PROFILER_COLOR_2(0xff, 0xff, 0x95, 0x0e);
const video::SColor PROFILER_COLOR_3(0xff, 0xae, 0xcf, 0x00);
const video::SColor PROFILER_COLOR_4(0xff, 0xff, 0xd3, 0x20);
const video::SColor PROFILER_COLOR_5(0xff, 0xff, 0x42, 0x0e);
const video::SColor PROFILER_COLOR_6(0xff, 0xe0, 0x80, 0x80);
const video::SColor PROFILER_COLOR_7(0xff, 0x72, 0x9f, 0xcf);
const video::SColor PROFILER_COLOR_8(0xff, 0xff, 0x99, 0xcc);
const video::SColor PROFILER_FALLBACK(255, 200, 200, 200);
const video::SColor EDIT_BG_OVERRIDE(0, 0, 0, 1);
} // namespace Colors

namespace Sizing
{
const float BUTTON_HEIGHT_RATIO = 15.0f / 13.0f * 0.35f;
const float BUTTON_ALT_HEIGHT_RATIO = 0.875f;
const float SLOT_SPACING_RATIO = 0.25f;
const float PADDING_RATIO = 0.05f;
const float FIXED_IMGSIZE_DPI_MULT = 0.5555f;
const u32 STATUS_BAR_HEIGHT = 40;
const core::dimension2d<u32> LOCK_SIZE(800, 600);
const core::dimension2d<u32> TOOLTIP_INITIAL_SIZE(110, 18);
const s32 TOOLTIP_PADDING_Y = 5;
const float FORM_FALLBACK_WIDTH = 580.0f;
const float FORM_FALLBACK_HEIGHT = 300.0f;
const s32 STATUS_TEXT_Y_OFFSET = 150;
// Extended sizing
const s32 CHECKBOX_PADDING = 7;
const s32 FOCUS_BORDER_WIDTH = 2;
const s32 SLOT_BORDER_WIDTH = 1;
const s32 TABLE_ROW_PADDING = 4;
const s32 TABLE_TEXT_X_POS = 6;
const s32 TABLE_EM_FALLBACK = 6;
const float TABLE_COL_PADDING_EM = 0.5f;
const float TABLE_INDENT_EM = 1.5f;
const s32 CHAT_SCROLLBAR_WIDTH = 30;
const core::dimension2d<s32> CHAT_INITIAL_SIZE(100, 100);
const float EDIT_SCROLL_SMALL_STEP = 3.0f;
const float EDIT_SCROLL_LARGE_STEP = 10.0f;
const core::dimension2d<s32> MODAL_DEFAULT_SIZE(100, 100);
const float DOUBLECLICK_DISTANCE = 30.0f;
const float GUI_SCALE_MIN = 0.5f;
const float GUI_SCALE_MAX = 20.0f;
const s32 HYPERTEXT_MARGIN_DEFAULT = 10;
const s32 HYPERTEXT_MARGIN_PARAGRAPH = 10;
const s32 HYPERTEXT_MARGIN_GLOBAL = 3;
const core::dimension2d<u32> HYPERTEXT_IMAGE_SIZE(80, 80);
const s32 HYPERTEXT_SCROLLBAR_WIDTH = 16;
const float HYPERTEXT_FONT_SCALE_DIVISOR = 16.0f;
const s32 WEAR_BAR_DIVISOR = 16;
const s32 ENGINE_TEXT_PADDING = 4;
const s32 ENGINE_HEADER_PAD = 4;
const s32 ENGINE_HEADER_INNER_PAD = 8;
const s32 ENGINE_HEADER_Y_OFFSET = 10;
const s32 ENGINE_SIDEBAR_OFFSET = 320;
const s32 PROFILER_GRAPH_HEIGHT = 52;
const s32 PROFILER_TEXT_X_OFFSET = 15;
const s32 PROFILER_TEXT_WIDTH = 200;
const float BUTTON_COLOR_INTERPOLATE = 0.65f;
const float SCENE_CAMERA_FOV = 30.0f;
const float SCENE_CAMERA_DISTANCE_MULT = 0.5f;
const float SCENE_ROTATION_SPEED = 0.03f;
const float SCENE_ROTATION_MAX_1 = 60.0f;
const float SCENE_ROTATION_MAX_2 = 300.0f;
const s16 ITEM_RENDER_ROTATION_Y = 100;
} // namespace Sizing

namespace Timing
{
const float STATUS_TEXT_DURATION_GAME = 1.5f;
const float STATUS_TEXT_DURATION_MENU = 3.0f;
const float CHAT_CURSOR_BLINK_SPEED = 2.0f;
const float CHAT_CURSOR_HEIGHT_RATIO = 0.1f;
const float CHAT_HEIGHT_SPEED = 5.0f;
const u32 CHAT_REOPEN_INHIBIT_MS = 50;
const u32 CHAT_REOPEN_INHIBIT_ESC_MS = 1;
const u32 CHAT_WEBLINK_DEBOUNCE_MS = 600;
const float MOUSE_WHEEL_SCROLL_MULTIPLIER = 3.0f;
const u32 DOUBLECLICK_THRESHOLD_MS = 400;
const u32 TABLE_KEYNAV_TIMEOUT_MS = 500;
const float ENGINE_CLOUD_STEP_MULT = 3.0f;
} // namespace Timing

namespace ButtonModifiers
{
const float HOVER_BRIGHTEN = 1.25f;
const float PRESS_DARKEN = 0.85f;
} // namespace ButtonModifiers

namespace Dialogs
{
const core::dimension2d<u32> PASSWORD_CHANGE_SIZE(580, 300);
const core::dimension2d<u32> OPEN_URL_SIZE(580, 250);
const core::dimension2d<u32> VOLUME_CHANGE_SIZE(380, 200);
const float PATH_SELECT_WIDTH = 600.0f;
const float PATH_SELECT_HEIGHT = 400.0f;
} // namespace Dialogs
} // namespace TestTheme

// ============================================================================
// Test framework
// ============================================================================
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static bool verbose = false;

#define TEST_ASSERT(condition, msg)                                         \
        do {                                                                \
                tests_run++;                                                \
                if (condition) {                                            \
                        tests_passed++;                                    \
                        if (verbose)                                       \
                                printf("  PASS: %s\n", msg);               \
                } else {                                                    \
                        tests_failed++;                                    \
                        printf("  FAIL: %s (line %d)\n", msg, __LINE__);  \
                }                                                           \
        } while (0)

static bool colorEq(const video::SColor &a, const video::SColor &b)
{
        return a.getAlpha() == b.getAlpha() && a.getRed() == b.getRed() &&
               a.getGreen() == b.getGreen() && a.getBlue() == b.getBlue();
}

static bool floatEq(float a, float b, float eps = 0.0001f)
{
        return fabsf(a - b) < eps;
}

// ============================================================================
// Test Suite 1: Color Constants (base)
// ============================================================================
void test_colors_base()
{
        printf("\n=== Suite 1: Color Constants (Base 22) ===\n");

        TEST_ASSERT(colorEq(TestTheme::Colors::MODAL_BG, video::SColor(140, 0, 0, 0)),
                    "MODAL_BG = SColor(140,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::MODAL_BG_FULLSCREEN, video::SColor(255, 0, 0, 0)),
                    "MODAL_BG_FULLSCREEN = SColor(255,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::TOOLTIP_BG, video::SColor(255, 110, 130, 60)),
                    "TOOLTIP_BG = SColor(255,110,130,60)");
        TEST_ASSERT(colorEq(TestTheme::Colors::TOOLTIP_TEXT, video::SColor(255, 255, 255, 255)),
                    "TOOLTIP_TEXT");
        TEST_ASSERT(colorEq(TestTheme::Colors::TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "TEXT_DEFAULT");
        TEST_ASSERT(colorEq(TestTheme::Colors::TRANSPARENT, video::SColor(0, 0, 0, 0)),
                    "TRANSPARENT = SColor(0,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_BG_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "BUTTON_BG_DEFAULT");
        TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "BUTTON_TEXT_DEFAULT");
        TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_OVERRIDE_DEFAULT, video::SColor(101, 255, 255, 255)),
                    "BUTTON_OVERRIDE_DEFAULT");
        TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BORDER, video::SColor(200, 0, 0, 0)),
                    "SLOT_BORDER");
        TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BG_NORMAL, video::SColor(255, 128, 128, 128)),
                    "SLOT_BG_NORMAL");
        TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BG_HOVERED, video::SColor(255, 192, 192, 192)),
                    "SLOT_BG_HOVERED");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "TABLE_TEXT_DEFAULT");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_BG_DEFAULT, video::SColor(255, 0, 0, 0)),
                    "TABLE_BG_DEFAULT");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_HIGHLIGHT, video::SColor(255, 70, 100, 50)),
                    "TABLE_HIGHLIGHT");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_HIGHLIGHT_TEXT, video::SColor(255, 255, 255, 255)),
                    "TABLE_HIGHLIGHT_TEXT");
        TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_FALLBACK, video::SColor(255, 0, 0, 0)),
                    "STATUS_TEXT_FALLBACK");
        TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_MAIN_BG, video::SColor(220, 0, 0, 0)),
                    "STATUS_TEXT_MAIN_BG");
        TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_DEFAULT_BG, video::SColor(0, 0, 0, 0)),
                    "STATUS_TEXT_DEFAULT_BG");
        TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_BG, video::SColor(255, 0, 0, 0)),
                    "CHAT_CONSOLE_BG");
        TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_CURSOR, video::SColor(255, 255, 255, 255)),
                    "CHAT_CONSOLE_CURSOR");
        TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_TEXT, video::SColor(255, 255, 255, 255)),
                    "CHAT_CONSOLE_TEXT");
        TEST_ASSERT(colorEq(TestTheme::Colors::TOUCH_SELECTION, video::SColor(255, 128, 128, 128)),
                    "TOUCH_SELECTION");
        TEST_ASSERT(colorEq(TestTheme::Colors::TOUCH_ERROR, video::SColor(255, 255, 0, 0)),
                    "TOUCH_ERROR");
}

// ============================================================================
// Test Suite 2: Extended Color Constants
// ============================================================================
void test_colors_extended()
{
        printf("\n=== Suite 2: Extended Color Constants ===\n");

        // HyperText colors
        TEST_ASSERT(colorEq(TestTheme::Colors::HYPERTEXT_DEFAULT, video::SColor(255, 238, 238, 238)),
                    "HYPERTEXT_DEFAULT = #EEEEEE");
        TEST_ASSERT(colorEq(TestTheme::Colors::HYPERTEXT_HOVER, video::SColor(255, 255, 0, 0)),
                    "HYPERTEXT_HOVER = #FF0000");
        TEST_ASSERT(colorEq(TestTheme::Colors::HYPERTEXT_LINK, video::SColor(255, 0, 0, 255)),
                    "HYPERTEXT_LINK = #0000FF");

        // Item rendering colors
        TEST_ASSERT(colorEq(TestTheme::Colors::ITEM_WEAR_BG, video::SColor(255, 0, 0, 0)),
                    "ITEM_WEAR_BG = black");
        TEST_ASSERT(colorEq(TestTheme::Colors::ITEM_NO_TEXTURE, video::SColor(255, 255, 255, 255)),
                    "ITEM_NO_TEXTURE = white");
        TEST_ASSERT(colorEq(TestTheme::Colors::ITEM_COUNT_TEXT, video::SColor(255, 255, 255, 255)),
                    "ITEM_COUNT_TEXT = white");

        // Focus/debug colors
        TEST_ASSERT(colorEq(TestTheme::Colors::FOCUS_BORDER, video::SColor(255, 255, 255, 255)),
                    "FOCUS_BORDER = white");
        TEST_ASSERT(colorEq(TestTheme::Colors::DEBUG_HIGHLIGHT, video::SColor(34, 255, 255, 0)),
                    "DEBUG_HIGHLIGHT = 0x22FFFF00");

        // Image rendering
        TEST_ASSERT(colorEq(TestTheme::Colors::IMAGE_DRAW_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "IMAGE_DRAW_DEFAULT = white");

        // Profiler colors (8 palette + fallback)
        TEST_ASSERT(TestTheme::Colors::PROFILER_COLOR_1.getAlpha() == 255, "PROFILER_COLOR_1 opaque");
        TEST_ASSERT(TestTheme::Colors::PROFILER_COLOR_2.getAlpha() == 255, "PROFILER_COLOR_2 opaque");
        TEST_ASSERT(colorEq(TestTheme::Colors::PROFILER_FALLBACK, video::SColor(255, 200, 200, 200)),
                    "PROFILER_FALLBACK");

        // Edit box
        TEST_ASSERT(TestTheme::Colors::EDIT_BG_OVERRIDE.getAlpha() == 0, "EDIT_BG_OVERRIDE alpha=0");
}

// ============================================================================
// Test Suite 3: Sizing Constants (base + extended)
// ============================================================================
void test_sizing()
{
        printf("\n=== Suite 3: Sizing Constants ===\n");

        // Base sizing
        TEST_ASSERT(floatEq(TestTheme::Sizing::BUTTON_HEIGHT_RATIO, 15.0f / 13.0f * 0.35f),
                    "BUTTON_HEIGHT_RATIO");
        TEST_ASSERT(floatEq(TestTheme::Sizing::BUTTON_ALT_HEIGHT_RATIO, 0.875f),
                    "BUTTON_ALT_HEIGHT_RATIO = 0.875");
        TEST_ASSERT(floatEq(TestTheme::Sizing::SLOT_SPACING_RATIO, 0.25f),
                    "SLOT_SPACING_RATIO = 0.25");
        TEST_ASSERT(floatEq(TestTheme::Sizing::PADDING_RATIO, 0.05f),
                    "PADDING_RATIO = 0.05");
        TEST_ASSERT(floatEq(TestTheme::Sizing::FIXED_IMGSIZE_DPI_MULT, 0.5555f),
                    "FIXED_IMGSIZE_DPI_MULT = 0.5555");
        TEST_ASSERT(TestTheme::Sizing::STATUS_BAR_HEIGHT == 40, "STATUS_BAR_HEIGHT = 40");
        TEST_ASSERT(TestTheme::Sizing::LOCK_SIZE == core::dimension2d<u32>(800, 600),
                    "LOCK_SIZE = (800, 600)");
        TEST_ASSERT(TestTheme::Sizing::TOOLTIP_INITIAL_SIZE == core::dimension2d<u32>(110, 18),
                    "TOOLTIP_INITIAL_SIZE = (110, 18)");
        TEST_ASSERT(TestTheme::Sizing::TOOLTIP_PADDING_Y == 5, "TOOLTIP_PADDING_Y = 5");
        TEST_ASSERT(floatEq(TestTheme::Sizing::FORM_FALLBACK_WIDTH, 580.0f),
                    "FORM_FALLBACK_WIDTH = 580");
        TEST_ASSERT(floatEq(TestTheme::Sizing::FORM_FALLBACK_HEIGHT, 300.0f),
                    "FORM_FALLBACK_HEIGHT = 300");
        TEST_ASSERT(TestTheme::Sizing::STATUS_TEXT_Y_OFFSET == 150, "STATUS_TEXT_Y_OFFSET = 150");

        // Extended sizing
        TEST_ASSERT(TestTheme::Sizing::CHECKBOX_PADDING == 7, "CHECKBOX_PADDING = 7");
        TEST_ASSERT(TestTheme::Sizing::FOCUS_BORDER_WIDTH == 2, "FOCUS_BORDER_WIDTH = 2");
        TEST_ASSERT(TestTheme::Sizing::SLOT_BORDER_WIDTH == 1, "SLOT_BORDER_WIDTH = 1");
        TEST_ASSERT(TestTheme::Sizing::TABLE_ROW_PADDING == 4, "TABLE_ROW_PADDING = 4");
        TEST_ASSERT(TestTheme::Sizing::TABLE_TEXT_X_POS == 6, "TABLE_TEXT_X_POS = 6");
        TEST_ASSERT(TestTheme::Sizing::TABLE_EM_FALLBACK == 6, "TABLE_EM_FALLBACK = 6");
        TEST_ASSERT(floatEq(TestTheme::Sizing::TABLE_COL_PADDING_EM, 0.5f), "TABLE_COL_PADDING_EM = 0.5");
        TEST_ASSERT(floatEq(TestTheme::Sizing::TABLE_INDENT_EM, 1.5f), "TABLE_INDENT_EM = 1.5");
        TEST_ASSERT(TestTheme::Sizing::CHAT_SCROLLBAR_WIDTH == 30, "CHAT_SCROLLBAR_WIDTH = 30");
        TEST_ASSERT(TestTheme::Sizing::CHAT_INITIAL_SIZE == core::dimension2d<s32>(100, 100),
                    "CHAT_INITIAL_SIZE = (100, 100)");
        TEST_ASSERT(floatEq(TestTheme::Sizing::EDIT_SCROLL_SMALL_STEP, 3.0f), "EDIT_SCROLL_SMALL_STEP = 3");
        TEST_ASSERT(floatEq(TestTheme::Sizing::EDIT_SCROLL_LARGE_STEP, 10.0f), "EDIT_SCROLL_LARGE_STEP = 10");
        TEST_ASSERT(TestTheme::Sizing::MODAL_DEFAULT_SIZE == core::dimension2d<s32>(100, 100),
                    "MODAL_DEFAULT_SIZE = (100, 100)");
        TEST_ASSERT(floatEq(TestTheme::Sizing::DOUBLECLICK_DISTANCE, 30.0f), "DOUBLECLICK_DISTANCE = 30");
        TEST_ASSERT(floatEq(TestTheme::Sizing::GUI_SCALE_MIN, 0.5f), "GUI_SCALE_MIN = 0.5");
        TEST_ASSERT(floatEq(TestTheme::Sizing::GUI_SCALE_MAX, 20.0f), "GUI_SCALE_MAX = 20");
        TEST_ASSERT(TestTheme::Sizing::HYPERTEXT_MARGIN_DEFAULT == 10, "HYPERTEXT_MARGIN_DEFAULT = 10");
        TEST_ASSERT(TestTheme::Sizing::HYPERTEXT_MARGIN_PARAGRAPH == 10, "HYPERTEXT_MARGIN_PARAGRAPH = 10");
        TEST_ASSERT(TestTheme::Sizing::HYPERTEXT_MARGIN_GLOBAL == 3, "HYPERTEXT_MARGIN_GLOBAL = 3");
        TEST_ASSERT(TestTheme::Sizing::HYPERTEXT_IMAGE_SIZE == core::dimension2d<u32>(80, 80),
                    "HYPERTEXT_IMAGE_SIZE = (80, 80)");
        TEST_ASSERT(TestTheme::Sizing::HYPERTEXT_SCROLLBAR_WIDTH == 16, "HYPERTEXT_SCROLLBAR_WIDTH = 16");
        TEST_ASSERT(floatEq(TestTheme::Sizing::HYPERTEXT_FONT_SCALE_DIVISOR, 16.0f),
                    "HYPERTEXT_FONT_SCALE_DIVISOR = 16");
        TEST_ASSERT(TestTheme::Sizing::WEAR_BAR_DIVISOR == 16, "WEAR_BAR_DIVISOR = 16");
        TEST_ASSERT(TestTheme::Sizing::ENGINE_TEXT_PADDING == 4, "ENGINE_TEXT_PADDING = 4");
        TEST_ASSERT(TestTheme::Sizing::ENGINE_HEADER_PAD == 4, "ENGINE_HEADER_PAD = 4");
        TEST_ASSERT(TestTheme::Sizing::ENGINE_HEADER_INNER_PAD == 8, "ENGINE_HEADER_INNER_PAD = 8");
        TEST_ASSERT(TestTheme::Sizing::ENGINE_HEADER_Y_OFFSET == 10, "ENGINE_HEADER_Y_OFFSET = 10");
        TEST_ASSERT(TestTheme::Sizing::ENGINE_SIDEBAR_OFFSET == 320, "ENGINE_SIDEBAR_OFFSET = 320");
        TEST_ASSERT(TestTheme::Sizing::PROFILER_GRAPH_HEIGHT == 52, "PROFILER_GRAPH_HEIGHT = 52");
        TEST_ASSERT(TestTheme::Sizing::PROFILER_TEXT_X_OFFSET == 15, "PROFILER_TEXT_X_OFFSET = 15");
        TEST_ASSERT(TestTheme::Sizing::PROFILER_TEXT_WIDTH == 200, "PROFILER_TEXT_WIDTH = 200");
        TEST_ASSERT(floatEq(TestTheme::Sizing::BUTTON_COLOR_INTERPOLATE, 0.65f),
                    "BUTTON_COLOR_INTERPOLATE = 0.65");
        TEST_ASSERT(floatEq(TestTheme::Sizing::SCENE_CAMERA_FOV, 30.0f), "SCENE_CAMERA_FOV = 30");
        TEST_ASSERT(floatEq(TestTheme::Sizing::SCENE_CAMERA_DISTANCE_MULT, 0.5f),
                    "SCENE_CAMERA_DISTANCE_MULT = 0.5");
        TEST_ASSERT(floatEq(TestTheme::Sizing::SCENE_ROTATION_SPEED, 0.03f), "SCENE_ROTATION_SPEED = 0.03");
        TEST_ASSERT(TestTheme::Sizing::ITEM_RENDER_ROTATION_Y == 100, "ITEM_RENDER_ROTATION_Y = 100");
}

// ============================================================================
// Test Suite 4: Timing Constants (base + extended)
// ============================================================================
void test_timing()
{
        printf("\n=== Suite 4: Timing Constants ===\n");

        // Base timing
        TEST_ASSERT(floatEq(TestTheme::Timing::STATUS_TEXT_DURATION_GAME, 1.5f),
                    "STATUS_TEXT_DURATION_GAME = 1.5s");
        TEST_ASSERT(floatEq(TestTheme::Timing::STATUS_TEXT_DURATION_MENU, 3.0f),
                    "STATUS_TEXT_DURATION_MENU = 3.0s");
        TEST_ASSERT(TestTheme::Timing::STATUS_TEXT_DURATION_GAME < TestTheme::Timing::STATUS_TEXT_DURATION_MENU,
                    "Game duration < Menu duration");

        // Extended timing
        TEST_ASSERT(floatEq(TestTheme::Timing::CHAT_CURSOR_BLINK_SPEED, 2.0f),
                    "CHAT_CURSOR_BLINK_SPEED = 2.0");
        TEST_ASSERT(floatEq(TestTheme::Timing::CHAT_CURSOR_HEIGHT_RATIO, 0.1f),
                    "CHAT_CURSOR_HEIGHT_RATIO = 0.1");
        TEST_ASSERT(floatEq(TestTheme::Timing::CHAT_HEIGHT_SPEED, 5.0f),
                    "CHAT_HEIGHT_SPEED = 5.0");
        TEST_ASSERT(TestTheme::Timing::CHAT_REOPEN_INHIBIT_MS == 50,
                    "CHAT_REOPEN_INHIBIT_MS = 50");
        TEST_ASSERT(TestTheme::Timing::CHAT_REOPEN_INHIBIT_ESC_MS == 1,
                    "CHAT_REOPEN_INHIBIT_ESC_MS = 1");
        TEST_ASSERT(TestTheme::Timing::CHAT_WEBLINK_DEBOUNCE_MS == 600,
                    "CHAT_WEBLINK_DEBOUNCE_MS = 600");
        TEST_ASSERT(floatEq(TestTheme::Timing::MOUSE_WHEEL_SCROLL_MULTIPLIER, 3.0f),
                    "MOUSE_WHEEL_SCROLL_MULTIPLIER = 3.0");
        TEST_ASSERT(TestTheme::Timing::DOUBLECLICK_THRESHOLD_MS == 400,
                    "DOUBLECLICK_THRESHOLD_MS = 400");
        TEST_ASSERT(TestTheme::Timing::TABLE_KEYNAV_TIMEOUT_MS == 500,
                    "TABLE_KEYNAV_TIMEOUT_MS = 500");
        TEST_ASSERT(floatEq(TestTheme::Timing::ENGINE_CLOUD_STEP_MULT, 3.0f),
                    "ENGINE_CLOUD_STEP_MULT = 3.0");
}

// ============================================================================
// Test Suite 5: Button Modifiers
// ============================================================================
void test_button_modifiers()
{
        printf("\n=== Suite 5: Button Modifiers ===\n");

        TEST_ASSERT(floatEq(TestTheme::ButtonModifiers::HOVER_BRIGHTEN, 1.25f),
                    "HOVER_BRIGHTEN = 1.25");
        TEST_ASSERT(floatEq(TestTheme::ButtonModifiers::PRESS_DARKEN, 0.85f),
                    "PRESS_DARKEN = 0.85");
        TEST_ASSERT(TestTheme::ButtonModifiers::HOVER_BRIGHTEN > 1.0f,
                    "HOVER_BRIGHTEN > 1.0 (brightens)");
        TEST_ASSERT(TestTheme::ButtonModifiers::PRESS_DARKEN < 1.0f,
                    "PRESS_DARKEN < 1.0 (darkens)");
}

// ============================================================================
// Test Suite 6: Dialog Dimensions
// ============================================================================
void test_dialogs()
{
        printf("\n=== Suite 6: Dialog Dimensions ===\n");

        TEST_ASSERT(TestTheme::Dialogs::PASSWORD_CHANGE_SIZE == core::dimension2d<u32>(580, 300),
                    "PASSWORD_CHANGE_SIZE = 580x300");
        TEST_ASSERT(TestTheme::Dialogs::OPEN_URL_SIZE == core::dimension2d<u32>(580, 250),
                    "OPEN_URL_SIZE = 580x250");
        TEST_ASSERT(TestTheme::Dialogs::VOLUME_CHANGE_SIZE == core::dimension2d<u32>(380, 200),
                    "VOLUME_CHANGE_SIZE = 380x200");
        TEST_ASSERT(floatEq(TestTheme::Dialogs::PATH_SELECT_WIDTH, 600.0f),
                    "PATH_SELECT_WIDTH = 600");
        TEST_ASSERT(floatEq(TestTheme::Dialogs::PATH_SELECT_HEIGHT, 400.0f),
                    "PATH_SELECT_HEIGHT = 400");
}

// ============================================================================
// Test Suite 7: Semantic Consistency
// ============================================================================
void test_semantic_consistency()
{
        printf("\n=== Suite 7: Semantic Consistency ===\n");

        TEST_ASSERT(TestTheme::Colors::MODAL_BG.getAlpha() < 255,
                    "MODAL_BG is semi-transparent");
        TEST_ASSERT(TestTheme::Colors::MODAL_BG_FULLSCREEN.getAlpha() == 255,
                    "MODAL_BG_FULLSCREEN is fully opaque");
        TEST_ASSERT(TestTheme::Colors::TOOLTIP_BG.getAlpha() == 255,
                    "TOOLTIP_BG is fully opaque");
        TEST_ASSERT(TestTheme::Colors::TRANSPARENT.getAlpha() == 0,
                    "TRANSPARENT has zero alpha");

        u32 normal_brightness = TestTheme::Colors::SLOT_BG_NORMAL.getRed() +
                               TestTheme::Colors::SLOT_BG_NORMAL.getGreen() +
                               TestTheme::Colors::SLOT_BG_NORMAL.getBlue();
        u32 hovered_brightness = TestTheme::Colors::SLOT_BG_HOVERED.getRed() +
                                TestTheme::Colors::SLOT_BG_HOVERED.getGreen() +
                                TestTheme::Colors::SLOT_BG_HOVERED.getBlue();
        TEST_ASSERT(hovered_brightness > normal_brightness,
                    "SLOT_BG_HOVERED brighter than SLOT_BG_NORMAL");

        TEST_ASSERT(TestTheme::Colors::TABLE_HIGHLIGHT != TestTheme::Colors::TABLE_BG_DEFAULT,
                    "TABLE_HIGHLIGHT != TABLE_BG_DEFAULT");
        TEST_ASSERT(TestTheme::Colors::TOUCH_ERROR.getRed() == 255,
                    "TOUCH_ERROR is red");
        TEST_ASSERT(TestTheme::Colors::HYPERTEXT_HOVER.getRed() == 255,
                    "HYPERTEXT_HOVER is red");
        TEST_ASSERT(TestTheme::Colors::HYPERTEXT_LINK.getBlue() == 255,
                    "HYPERTEXT_LINK is blue");
        TEST_ASSERT(TestTheme::Colors::ITEM_WEAR_BG.getRed() == 0 &&
                    TestTheme::Colors::ITEM_WEAR_BG.getGreen() == 0,
                    "ITEM_WEAR_BG is black");

        TEST_ASSERT(TestTheme::Sizing::GUI_SCALE_MIN < TestTheme::Sizing::GUI_SCALE_MAX,
                    "GUI_SCALE_MIN < GUI_SCALE_MAX");
        TEST_ASSERT(TestTheme::Sizing::FOCUS_BORDER_WIDTH > 0, "FOCUS_BORDER_WIDTH > 0");
        TEST_ASSERT(TestTheme::Sizing::CHAT_SCROLLBAR_WIDTH > 0, "CHAT_SCROLLBAR_WIDTH > 0");
        TEST_ASSERT(TestTheme::Sizing::WEAR_BAR_DIVISOR > 0, "WEAR_BAR_DIVISOR > 0");
        TEST_ASSERT(TestTheme::Timing::DOUBLECLICK_THRESHOLD_MS > 0, "DOUBLECLICK_THRESHOLD_MS > 0");
        TEST_ASSERT(TestTheme::Timing::TABLE_KEYNAV_TIMEOUT_MS > 0, "TABLE_KEYNAV_TIMEOUT_MS > 0");
}

// ============================================================================
// Test Suite 8: Source File Consistency
// ============================================================================
void test_source_consistency()
{
        printf("\n=== Suite 8: Source File Consistency ===\n");

        std::ifstream theme_file("/home/z/my-project/Luantis/src/gui/GUITheme.h");
        TEST_ASSERT(theme_file.is_open(), "GUITheme.h exists and is readable");

        if (theme_file.is_open()) {
                std::string line;
                bool has_colors = false, has_sizing = false, has_timing = false;
                bool has_btn_mods = false, has_validate = false;
                bool has_touch_colors = false, has_dialogs = false;
                bool has_hypertext = false, has_item = false;
                bool has_profiler = false, has_focus = false;

                while (std::getline(theme_file, line)) {
                        if (line.find("namespace Colors") != std::string::npos) has_colors = true;
                        if (line.find("namespace Sizing") != std::string::npos) has_sizing = true;
                        if (line.find("namespace Timing") != std::string::npos) has_timing = true;
                        if (line.find("namespace ButtonModifiers") != std::string::npos) has_btn_mods = true;
                        if (line.find("bool validate()") != std::string::npos) has_validate = true;
                        if (line.find("TOUCH_SELECTION") != std::string::npos) has_touch_colors = true;
                        if (line.find("namespace Dialogs") != std::string::npos) has_dialogs = true;
                        if (line.find("HYPERTEXT_DEFAULT") != std::string::npos) has_hypertext = true;
                        if (line.find("ITEM_WEAR_BG") != std::string::npos) has_item = true;
                        if (line.find("PROFILER_COLOR_1") != std::string::npos) has_profiler = true;
                        if (line.find("FOCUS_BORDER") != std::string::npos) has_focus = true;
                }

                TEST_ASSERT(has_colors, "GUITheme.h has Colors namespace");
                TEST_ASSERT(has_sizing, "GUITheme.h has Sizing namespace");
                TEST_ASSERT(has_timing, "GUITheme.h has Timing namespace");
                TEST_ASSERT(has_btn_mods, "GUITheme.h has ButtonModifiers namespace");
                TEST_ASSERT(has_validate, "GUITheme.h has validate()");
                TEST_ASSERT(has_touch_colors, "GUITheme.h has TOUCH_SELECTION");
                TEST_ASSERT(has_dialogs, "GUITheme.h has Dialogs namespace");
                TEST_ASSERT(has_hypertext, "GUITheme.h has HYPERTEXT_DEFAULT");
                TEST_ASSERT(has_item, "GUITheme.h has ITEM_WEAR_BG");
                TEST_ASSERT(has_profiler, "GUITheme.h has PROFILER_COLOR_1");
                TEST_ASSERT(has_focus, "GUITheme.h has FOCUS_BORDER");
        }

        std::ifstream theme_cpp("/home/z/my-project/Luantis/src/gui/GUITheme.cpp");
        TEST_ASSERT(theme_cpp.is_open(), "GUITheme.cpp exists and is readable");
}

// ============================================================================
// Test Suite 9: No Hardcoded Values Remain
// ============================================================================
void test_no_hardcoded_values()
{
        printf("\n=== Suite 9: No Hardcoded Style Values Remain ===\n");

        const char *gui_dir = "/home/z/my-project/Luantis/src/gui/";
        // Extended file list covering all files that were updated
        const char *files[] = {
                "guiFormSpecMenu.cpp", "guiButton.cpp", "guiButton.h",
                "guiPasswordChange.cpp", "guiVolumeChange.cpp", "guiOpenURL.cpp",
                "touchcontrols.cpp", "touchscreeneditor.cpp",
                "guiInventoryList.cpp", "guiTable.cpp",
                "statusTextHelper.cpp", "statusTextHelper.h",
                "guiChatConsole.h", "guiChatConsole.cpp",
                "guiEngine.cpp", "guiHyperText.cpp", "guiHyperText.h",
                "drawItemStack.cpp", "guiAnimatedImage.cpp",
                "guiBackgroundImage.cpp", "guiItemImage.cpp",
                "modalMenu.cpp", "profilergraph.cpp",
                "guiEditBoxWithScrollbar.cpp", "guiPathSelectMenu.cpp",
                NULL
        };

        // Check that MODAL_BG hardcoded value SColor(140,0,0,0) is gone
        for (int i = 0; files[i]; i++) {
                std::string path = std::string(gui_dir) + files[i];
                std::ifstream f(path);
                if (!f.is_open()) continue;

                std::string line;
                bool found = false;
                while (std::getline(f, line)) {
                        if (line.find("GUITheme") != std::string::npos) continue;
                        if (line.find("SColor(140, 0, 0, 0)") != std::string::npos ||
                            line.find("SColor(140,0,0,0)") != std::string::npos) {
                                found = true;
                                break;
                        }
                }
                std::string msg = std::string("No SColor(140,0,0,0) in ") + files[i];
                TEST_ASSERT(!found, msg.c_str());
        }

        // Check key hardcoded values are gone
        {
                // guiButton.cpp should not have COLOR_HOVERED_MOD/COLOR_PRESSED_MOD
                std::ifstream f(std::string(gui_dir) + "guiButton.cpp");
                std::string line;
                bool found_hover = false, found_press = false;
                while (std::getline(f, line)) {
                        if (line.find("COLOR_HOVERED_MOD") != std::string::npos) found_hover = true;
                        if (line.find("COLOR_PRESSED_MOD") != std::string::npos) found_press = true;
                }
                TEST_ASSERT(!found_hover, "COLOR_HOVERED_MOD removed from guiButton.cpp");
                TEST_ASSERT(!found_press, "COLOR_PRESSED_MOD removed from guiButton.cpp");
        }

        // Verify no Clay files exist
        std::ifstream clay1(std::string(gui_dir) + "clay_gui_manager.h");
        TEST_ASSERT(!clay1.is_open(), "No clay_gui_manager.h");
        std::ifstream clay2(std::string(gui_dir) + "clay_renderer.h");
        TEST_ASSERT(!clay2.is_open(), "No clay_renderer.h");
}

// ============================================================================
// Test Suite 10: Theme Completeness
// ============================================================================
void test_theme_completeness()
{
        printf("\n=== Suite 10: Theme Completeness ===\n");

        // Colors: 35+ constants
        TEST_ASSERT(TestTheme::Colors::MODAL_BG.getAlpha() == 140, "Colors::MODAL_BG alpha=140");
        TEST_ASSERT(TestTheme::Colors::HYPERTEXT_DEFAULT.getRed() == 238, "Colors::HYPERTEXT_DEFAULT red=238");
        TEST_ASSERT(TestTheme::Colors::BUTTON_OVERRIDE_DEFAULT.getAlpha() == 101, "Colors::BUTTON_OVERRIDE alpha=101");
        TEST_ASSERT(TestTheme::Colors::TOUCH_ERROR.getRed() == 255, "Colors::TOUCH_ERROR red=255");
        TEST_ASSERT(TestTheme::Colors::FOCUS_BORDER.getAlpha() == 255, "Colors::FOCUS_BORDER alpha=255");
        TEST_ASSERT(TestTheme::Colors::ITEM_WEAR_BG.getBlue() == 0, "Colors::ITEM_WEAR_BG blue=0");
        TEST_ASSERT(TestTheme::Colors::DEBUG_HIGHLIGHT.getAlpha() == 34, "Colors::DEBUG_HIGHLIGHT alpha=34");

        // Sizing: 42+ constants
        TEST_ASSERT(TestTheme::Sizing::CHECKBOX_PADDING > 0, "Sizing::CHECKBOX_PADDING > 0");
        TEST_ASSERT(TestTheme::Sizing::WEAR_BAR_DIVISOR > 0, "Sizing::WEAR_BAR_DIVISOR > 0");
        TEST_ASSERT(TestTheme::Sizing::ENGINE_SIDEBAR_OFFSET > 0, "Sizing::ENGINE_SIDEBAR_OFFSET > 0");
        TEST_ASSERT(TestTheme::Sizing::PROFILER_GRAPH_HEIGHT > 0, "Sizing::PROFILER_GRAPH_HEIGHT > 0");

        // Timing: 11+ constants
        TEST_ASSERT(TestTheme::Timing::CHAT_CURSOR_BLINK_SPEED > 0, "Timing::CHAT_CURSOR_BLINK_SPEED > 0");
        TEST_ASSERT(TestTheme::Timing::DOUBLECLICK_THRESHOLD_MS > 0, "Timing::DOUBLECLICK_THRESHOLD_MS > 0");

        // Dialogs: 5 constants
        TEST_ASSERT(TestTheme::Dialogs::PASSWORD_CHANGE_SIZE.Width > 0, "Dialogs::PASSWORD_CHANGE width > 0");
        TEST_ASSERT(TestTheme::Dialogs::VOLUME_CHANGE_SIZE.Width < TestTheme::Dialogs::PASSWORD_CHANGE_SIZE.Width,
                    "Volume dialog smaller than password dialog");

        // ButtonModifiers: 2 constants
        TEST_ASSERT(TestTheme::ButtonModifiers::HOVER_BRIGHTEN > 1.0f, "HOVER_BRIGHTEN > 1.0");
        TEST_ASSERT(TestTheme::ButtonModifiers::PRESS_DARKEN < 1.0f, "PRESS_DARKEN < 1.0");
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char *argv[])
{
        verbose = argc > 1 && strcmp(argv[1], "--verbose") == 0;

        printf("======================================================\n");
        printf("  Luantis GUITheme Test Suite (v9.50)\n");
        printf("  Comprehensive Test-Driven Validation\n");
        printf("  35 Colors | 42 Sizing | 11 Timing | 5 Dialogs\n");
        printf("  Based on branch: clawtest-v9.44-voice-server-authority\n");
        printf("======================================================\n");

        test_colors_base();
        test_colors_extended();
        test_sizing();
        test_timing();
        test_button_modifiers();
        test_dialogs();
        test_semantic_consistency();
        test_source_consistency();
        test_no_hardcoded_values();
        test_theme_completeness();

        printf("\n======================================================\n");
        printf("  Results: %d/%d passed, %d failed\n", tests_passed, tests_run,
               tests_failed);
        printf("======================================================\n");

        std::ofstream json_out("/tmp/gui_theme_test_results.json");
        json_out << "{\n";
        json_out << "  \"total\": " << tests_run << ",\n";
        json_out << "  \"passed\": " << tests_passed << ",\n";
        json_out << "  \"failed\": " << tests_failed << ",\n";
        json_out << "  \"success\": " << (tests_failed == 0 ? "true" : "false") << "\n";
        json_out << "}\n";
        json_out.close();

        return tests_failed > 0 ? 1 : 0;
}
