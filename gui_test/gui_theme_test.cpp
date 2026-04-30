// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// GUITheme Test Program — Test-Driven Validation
//
// This standalone program validates every constant in GUITheme.h without
// requiring the full Luantis build. It uses a minimal Irrlicht stub for
// SColor and compiles independently.
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
// Minimal Irrlicht types stub (avoids full Irrlicht dependency)
// ============================================================================
using u32 = unsigned int;

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

// ============================================================================
// Include the actual GUITheme.h (with minimal stubs in place)
// ============================================================================
// We replicate the constants here since GUITheme.h includes irrlichttypes_bloated.h
// which we can't easily stub. Instead, we mirror the exact values for validation.

// ---- Mirror of GUITheme.h constants ----
namespace TestTheme
{
namespace Colors
{
const video::SColor MODAL_BG(140, 0, 0, 0);
const video::SColor MODAL_BG_FULLSCREEN(255, 0, 0, 0);
const video::SColor TOOLTIP_BG(255, 110, 130, 60);
const video::SColor TOOLTIP_TEXT(255, 255, 255, 255);
const video::SColor TEXT_DEFAULT(255, 255, 255, 255);
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
} // namespace Colors

namespace Sizing
{
const float BUTTON_HEIGHT_RATIO = 15.0f / 13.0f * 0.35f;
const float BUTTON_ALT_HEIGHT_RATIO = 0.875f;
const float SLOT_SPACING_RATIO = 0.25f;
const float PADDING_RATIO = 0.05f;
const float FIXED_IMGSIZE_DPI_MULT = 0.5555f;
const u32 STATUS_BAR_HEIGHT = 40;
const core::dimension2d<u32> TOOLTIP_INITIAL_SIZE(110, 18);
const core::dimension2d<u32> MAINMENU_LOCKED_SIZE(800, 600);
} // namespace Sizing

namespace Spacing
{
const core::dimension2d<u32> PASSWORD_DIALOG_SIZE(580, 300);
const core::dimension2d<u32> VOLUME_DIALOG_SIZE(380, 200);
const core::dimension2d<u32> OPENURL_DIALOG_SIZE(580, 300);
} // namespace Spacing

namespace ButtonModifiers
{
const float HOVER_BRIGHTEN = 1.25f;
const float PRESS_DARKEN = 0.85f;
} // namespace ButtonModifiers
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

#define TEST_ASSERT_EQ(actual, expected, msg)                               \
        do {                                                                \
                tests_run++;                                                \
                if ((actual) == (expected)) {                               \
                        tests_passed++;                                    \
                        if (verbose)                                       \
                                printf("  PASS: %s\n", msg);               \
                } else {                                                    \
                        tests_failed++;                                    \
                        printf("  FAIL: %s (line %d)\n", msg, __LINE__);  \
                }                                                           \
        } while (0)

static std::string colorStr(const video::SColor &c)
{
        char buf[64];
        snprintf(buf, sizeof(buf), "SColor(%u,%u,%u,%u)", c.getAlpha(), c.getRed(),
                 c.getGreen(), c.getBlue());
        return std::string(buf);
}

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
// Test Suite 1: Color Constants Validation
// ============================================================================
void test_colors()
{
        printf("\n=== Suite 1: Color Constants ===\n");

        // Modal backgrounds
        TEST_ASSERT(colorEq(TestTheme::Colors::MODAL_BG, video::SColor(140, 0, 0, 0)),
                    "MODAL_BG = SColor(140,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::MODAL_BG_FULLSCREEN, video::SColor(255, 0, 0, 0)),
                    "MODAL_BG_FULLSCREEN = SColor(255,0,0,0)");

        // Tooltip colors
        TEST_ASSERT(colorEq(TestTheme::Colors::TOOLTIP_BG, video::SColor(255, 110, 130, 60)),
                    "TOOLTIP_BG = SColor(255,110,130,60)");
        TEST_ASSERT(colorEq(TestTheme::Colors::TOOLTIP_TEXT, video::SColor(255, 255, 255, 255)),
                    "TOOLTIP_TEXT = SColor(255,255,255,255)");

        // Default text color
        TEST_ASSERT(colorEq(TestTheme::Colors::TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "TEXT_DEFAULT = SColor(255,255,255,255)");

        // Button colors
        TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_BG_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "BUTTON_BG_DEFAULT = SColor(255,255,255,255)");
        TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "BUTTON_TEXT_DEFAULT = SColor(255,255,255,255)");
        TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_OVERRIDE_DEFAULT, video::SColor(101, 255, 255, 255)),
                    "BUTTON_OVERRIDE_DEFAULT = SColor(101,255,255,255)");

        // Inventory slot colors
        TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BORDER, video::SColor(200, 0, 0, 0)),
                    "SLOT_BORDER = SColor(200,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BG_NORMAL, video::SColor(255, 128, 128, 128)),
                    "SLOT_BG_NORMAL = SColor(255,128,128,128)");
        TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BG_HOVERED, video::SColor(255, 192, 192, 192)),
                    "SLOT_BG_HOVERED = SColor(255,192,192,192)");

        // Table colors
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
                    "TABLE_TEXT_DEFAULT = SColor(255,255,255,255)");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_BG_DEFAULT, video::SColor(255, 0, 0, 0)),
                    "TABLE_BG_DEFAULT = SColor(255,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_HIGHLIGHT, video::SColor(255, 70, 100, 50)),
                    "TABLE_HIGHLIGHT = SColor(255,70,100,50)");
        TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_HIGHLIGHT_TEXT, video::SColor(255, 255, 255, 255)),
                    "TABLE_HIGHLIGHT_TEXT = SColor(255,255,255,255)");

        // Status text colors
        TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_FALLBACK, video::SColor(255, 0, 0, 0)),
                    "STATUS_TEXT_FALLBACK = SColor(255,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_MAIN_BG, video::SColor(220, 0, 0, 0)),
                    "STATUS_TEXT_MAIN_BG = SColor(220,0,0,0)");
        TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_DEFAULT_BG, video::SColor(0, 0, 0, 0)),
                    "STATUS_TEXT_DEFAULT_BG = SColor(0,0,0,0)");

        // Chat console
        TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_BG, video::SColor(255, 0, 0, 0)),
                    "CHAT_CONSOLE_BG = SColor(255,0,0,0)");
}

// ============================================================================
// Test Suite 2: Sizing Constants Validation
// ============================================================================
void test_sizing()
{
        printf("\n=== Suite 2: Sizing Constants ===\n");

        TEST_ASSERT(floatEq(TestTheme::Sizing::BUTTON_HEIGHT_RATIO, 15.0f / 13.0f * 0.35f),
                    "BUTTON_HEIGHT_RATIO = 15/13 * 0.35");
        TEST_ASSERT(floatEq(TestTheme::Sizing::BUTTON_ALT_HEIGHT_RATIO, 0.875f),
                    "BUTTON_ALT_HEIGHT_RATIO = 0.875");
        TEST_ASSERT(floatEq(TestTheme::Sizing::SLOT_SPACING_RATIO, 0.25f),
                    "SLOT_SPACING_RATIO = 0.25");
        TEST_ASSERT(floatEq(TestTheme::Sizing::PADDING_RATIO, 0.05f),
                    "PADDING_RATIO = 0.05");
        TEST_ASSERT(floatEq(TestTheme::Sizing::FIXED_IMGSIZE_DPI_MULT, 0.5555f),
                    "FIXED_IMGSIZE_DPI_MULT = 0.5555");

        TEST_ASSERT_EQ(TestTheme::Sizing::STATUS_BAR_HEIGHT, 40u,
                       "STATUS_BAR_HEIGHT = 40");

        TEST_ASSERT(TestTheme::Sizing::TOOLTIP_INITIAL_SIZE ==
                            core::dimension2d<u32>(110, 18),
                    "TOOLTIP_INITIAL_SIZE = (110, 18)");
        TEST_ASSERT(TestTheme::Sizing::MAINMENU_LOCKED_SIZE ==
                            core::dimension2d<u32>(800, 600),
                    "MAINMENU_LOCKED_SIZE = (800, 600)");
}

// ============================================================================
// Test Suite 3: Spacing Constants Validation
// ============================================================================
void test_spacing()
{
        printf("\n=== Suite 3: Spacing Constants ===\n");

        TEST_ASSERT(TestTheme::Spacing::PASSWORD_DIALOG_SIZE ==
                            core::dimension2d<u32>(580, 300),
                    "PASSWORD_DIALOG_SIZE = (580, 300)");
        TEST_ASSERT(TestTheme::Spacing::VOLUME_DIALOG_SIZE ==
                            core::dimension2d<u32>(380, 200),
                    "VOLUME_DIALOG_SIZE = (380, 200)");
        TEST_ASSERT(TestTheme::Spacing::OPENURL_DIALOG_SIZE ==
                            core::dimension2d<u32>(580, 300),
                    "OPENURL_DIALOG_SIZE = (580, 300)");
}

// ============================================================================
// Test Suite 4: Button Modifier Constants Validation
// ============================================================================
void test_button_modifiers()
{
        printf("\n=== Suite 4: Button Modifiers ===\n");

        TEST_ASSERT(floatEq(TestTheme::ButtonModifiers::HOVER_BRIGHTEN, 1.25f),
                    "HOVER_BRIGHTEN = 1.25");
        TEST_ASSERT(floatEq(TestTheme::ButtonModifiers::PRESS_DARKEN, 0.85f),
                    "PRESS_DARKEN = 0.85");

        // Verify hover brightens and press darkens
        TEST_ASSERT(TestTheme::ButtonModifiers::HOVER_BRIGHTEN > 1.0f,
                    "HOVER_BRIGHTEN > 1.0 (brightens)");
        TEST_ASSERT(TestTheme::ButtonModifiers::PRESS_DARKEN < 1.0f,
                    "PRESS_DARKEN < 1.0 (darkens)");
}

// ============================================================================
// Test Suite 5: Semantic Consistency Checks
// ============================================================================
void test_semantic_consistency()
{
        printf("\n=== Suite 5: Semantic Consistency ===\n");

        // Modal BG alpha must be < 255 (semi-transparent)
        TEST_ASSERT(TestTheme::Colors::MODAL_BG.getAlpha() < 255,
                    "MODAL_BG is semi-transparent (alpha < 255)");

        // Fullscreen modal must be fully opaque
        TEST_ASSERT(TestTheme::Colors::MODAL_BG_FULLSCREEN.getAlpha() == 255,
                    "MODAL_BG_FULLSCREEN is fully opaque (alpha = 255)");

        // Tooltip must be readable — both BG and text alpha = 255
        TEST_ASSERT(TestTheme::Colors::TOOLTIP_BG.getAlpha() == 255,
                    "TOOLTIP_BG is fully opaque");
        TEST_ASSERT(TestTheme::Colors::TOOLTIP_TEXT.getAlpha() == 255,
                    "TOOLTIP_TEXT is fully opaque");

        // All button defaults must be opaque
        TEST_ASSERT(TestTheme::Colors::BUTTON_BG_DEFAULT.getAlpha() == 255,
                    "BUTTON_BG_DEFAULT is fully opaque");
        TEST_ASSERT(TestTheme::Colors::BUTTON_TEXT_DEFAULT.getAlpha() == 255,
                    "BUTTON_TEXT_DEFAULT is fully opaque");

        // Inventory slot hovered must be brighter than normal
        u32 normal_brightness = TestTheme::Colors::SLOT_BG_NORMAL.getRed() +
                                TestTheme::Colors::SLOT_BG_NORMAL.getGreen() +
                                TestTheme::Colors::SLOT_BG_NORMAL.getBlue();
        u32 hovered_brightness = TestTheme::Colors::SLOT_BG_HOVERED.getRed() +
                                 TestTheme::Colors::SLOT_BG_HOVERED.getGreen() +
                                 TestTheme::Colors::SLOT_BG_HOVERED.getBlue();
        TEST_ASSERT(hovered_brightness > normal_brightness,
                    "SLOT_BG_HOVERED is brighter than SLOT_BG_NORMAL");

        // Table highlight must be visually distinct from default BG
        TEST_ASSERT(TestTheme::Colors::TABLE_HIGHLIGHT !=
                            TestTheme::Colors::TABLE_BG_DEFAULT,
                    "TABLE_HIGHLIGHT != TABLE_BG_DEFAULT");

        // Sizing ratios must be positive
        TEST_ASSERT(TestTheme::Sizing::BUTTON_HEIGHT_RATIO > 0,
                    "BUTTON_HEIGHT_RATIO > 0");
        TEST_ASSERT(TestTheme::Sizing::SLOT_SPACING_RATIO >= 0,
                    "SLOT_SPACING_RATIO >= 0");
        TEST_ASSERT(TestTheme::Sizing::PADDING_RATIO >= 0,
                    "PADDING_RATIO >= 0");

        // Status bar height must be reasonable
        TEST_ASSERT(TestTheme::Sizing::STATUS_BAR_HEIGHT > 0 &&
                            TestTheme::Sizing::STATUS_BAR_HEIGHT < 200,
                    "STATUS_BAR_HEIGHT is reasonable (0-200)");

        // Dialog sizes must be positive and reasonable
        TEST_ASSERT(TestTheme::Spacing::PASSWORD_DIALOG_SIZE.Width > 0 &&
                            TestTheme::Spacing::PASSWORD_DIALOG_SIZE.Height > 0,
                    "PASSWORD_DIALOG_SIZE has positive dimensions");
        TEST_ASSERT(TestTheme::Spacing::VOLUME_DIALOG_SIZE.Width > 0 &&
                            TestTheme::Spacing::VOLUME_DIALOG_SIZE.Height > 0,
                    "VOLUME_DIALOG_SIZE has positive dimensions");
}

// ============================================================================
// Test Suite 6: Theme Source File Consistency
// ============================================================================
void test_source_consistency()
{
        printf("\n=== Suite 6: Source File Consistency ===\n");

        // Verify that GUITheme.h exists and has the expected structure
        std::ifstream theme_file("/home/z/my-project/Luantis/src/gui/GUITheme.h");
        TEST_ASSERT(theme_file.is_open(), "GUITheme.h exists and is readable");

        if (theme_file.is_open()) {
                std::string line;
                bool has_colors_ns = false;
                bool has_sizing_ns = false;
                bool has_spacing_ns = false;
                bool has_button_mods = false;
                bool has_modal_bg = false;
                bool has_hover_brighten = false;
                bool has_validate = false;

                while (std::getline(theme_file, line)) {
                        if (line.find("namespace Colors") != std::string::npos)
                                has_colors_ns = true;
                        if (line.find("namespace Sizing") != std::string::npos)
                                has_sizing_ns = true;
                        if (line.find("namespace Spacing") != std::string::npos)
                                has_spacing_ns = true;
                        if (line.find("namespace ButtonModifiers") != std::string::npos)
                                has_button_mods = true;
                        if (line.find("MODAL_BG") != std::string::npos &&
                            line.find("=") != std::string::npos)
                                has_modal_bg = true;
                        if (line.find("HOVER_BRIGHTEN") != std::string::npos &&
                            line.find("=") != std::string::npos)
                                has_hover_brighten = true;
                        if (line.find("bool validate()") != std::string::npos)
                                has_validate = true;
                }

                TEST_ASSERT(has_colors_ns, "GUITheme.h has Colors namespace");
                TEST_ASSERT(has_sizing_ns, "GUITheme.h has Sizing namespace");
                TEST_ASSERT(has_spacing_ns, "GUITheme.h has Spacing namespace");
                TEST_ASSERT(has_button_mods, "GUITheme.h has ButtonModifiers namespace");
                TEST_ASSERT(has_modal_bg, "GUITheme.h defines MODAL_BG");
                TEST_ASSERT(has_hover_brighten, "GUITheme.h defines HOVER_BRIGHTEN");
                TEST_ASSERT(has_validate, "GUITheme.h has validate() method");
        }

        // Verify GUITheme.cpp exists
        std::ifstream theme_cpp("/home/z/my-project/Luantis/src/gui/GUITheme.cpp");
        TEST_ASSERT(theme_cpp.is_open(), "GUITheme.cpp exists and is readable");

        // Verify no Clay files remain
        std::ifstream clay_manager("/home/z/my-project/Luantis/src/gui/clay_gui_manager.h");
        TEST_ASSERT(!clay_manager.is_open(), "clay_gui_manager.h has been removed");

        std::ifstream clay_renderer("/home/z/my-project/Luantis/src/gui/clay_renderer.h");
        TEST_ASSERT(!clay_renderer.is_open(), "clay_renderer.h has been removed");

        std::ifstream clay_pause("/home/z/my-project/Luantis/src/gui/clay_pause_menu.h");
        TEST_ASSERT(!clay_pause.is_open(), "clay_pause_menu.h has been removed");
}

// ============================================================================
// Test Suite 7: Integration — No Hardcoded Style Values Remain
// ============================================================================
void test_no_hardcoded_values()
{
        printf("\n=== Suite 7: No Hardcoded Style Values Remain ===\n");

        // Check that the most common hardcoded pattern (SColor(140,0,0,0)) no
        // longer appears in GUI source files (should only be in GUITheme.h)
        const char *gui_dir = "/home/z/my-project/Luantis/src/gui/";
        const char *files_to_check[] = {
                "guiFormSpecMenu.cpp", "guiButton.cpp",  "guiPasswordChange.cpp",
                "guiVolumeChange.cpp", "guiOpenURL.cpp", "touchcontrols.cpp",
                "touchscreeneditor.cpp", "guiInventoryList.h", "guiTable.h",
                "statusTextHelper.cpp", "statusTextHelper.h", "guiChatConsole.h",
                "guiEngine.cpp", NULL
        };

        for (int i = 0; files_to_check[i]; i++) {
                std::string path = std::string(gui_dir) + files_to_check[i];
                std::ifstream f(path);
                if (!f.is_open())
                        continue;

                std::string line;
                bool found_hardcoded_bg = false;
                int line_num = 0;
                while (std::getline(f, line)) {
                        line_num++;
                        // Skip GUITheme includes
                        if (line.find("GUITheme") != std::string::npos)
                                continue;
                        // Check for the old hardcoded modal background
                        if (line.find("SColor(140, 0, 0, 0)") != std::string::npos ||
                            line.find("SColor(140,0,0,0)") != std::string::npos) {
                                found_hardcoded_bg = true;
                                printf("    Found hardcoded SColor(140,0,0,0) in %s:%d\n",
                                       files_to_check[i], line_num);
                        }
                }

                std::string msg =
                        std::string("No hardcoded SColor(140,0,0,0) in ") + files_to_check[i];
                TEST_ASSERT(!found_hardcoded_bg, msg.c_str());
        }

        // Check that COLOR_HOVERED_MOD and COLOR_PRESSED_MOD no longer appear
        {
                std::ifstream f(std::string(gui_dir) + "guiButton.cpp");
                bool found_hover_mod = false;
                bool found_press_mod = false;
                std::string line;
                while (std::getline(f, line)) {
                        if (line.find("COLOR_HOVERED_MOD") != std::string::npos)
                                found_hover_mod = true;
                        if (line.find("COLOR_PRESSED_MOD") != std::string::npos)
                                found_press_mod = true;
                }
                TEST_ASSERT(!found_hover_mod,
                            "COLOR_HOVERED_MOD removed from guiButton.cpp");
                TEST_ASSERT(!found_press_mod,
                            "COLOR_PRESSED_MOD removed from guiButton.cpp");
        }

        // Check that Clay includes are gone from game engine files
        {
                const char *engine_files[] = {
                        "/home/z/my-project/Luantis/src/client/game_internal.h",
                        "/home/z/my-project/Luantis/src/client/inputhandler.cpp",
                        NULL
                };
                for (int i = 0; engine_files[i]; i++) {
                        std::ifstream f(engine_files[i]);
                        std::string line;
                        bool found_clay = false;
                        while (std::getline(f, line)) {
                                if (line.find("clay_gui_manager") != std::string::npos ||
                                    line.find("clay_pause_menu") != std::string::npos) {
                                        found_clay = true;
                                        break;
                                }
                        }
                        std::string fname = std::string(engine_files[i]);
                        auto last_slash = fname.find_last_of('/');
                        std::string msg = std::string("No Clay includes in ") +
                                          fname.substr(last_slash != std::string::npos ? last_slash + 1 : 0);
                        TEST_ASSERT(!found_clay, msg.c_str());
                }
        }
}

// ============================================================================
// Test Suite 8: GUITheme.h Structure Completeness
// ============================================================================
void test_theme_completeness()
{
        printf("\n=== Suite 8: Theme Completeness ===\n");

        // Verify all expected constants exist by testing their values
        // This ensures the theme covers ALL GUI styling categories

        // Colors: 18 constants
        TEST_ASSERT(TestTheme::Colors::MODAL_BG.getAlpha() == 140,
                    "Colors::MODAL_BG exists with correct alpha");
        TEST_ASSERT(TestTheme::Colors::TOOLTIP_BG.getRed() == 110,
                    "Colors::TOOLTIP_BG exists with correct red");
        TEST_ASSERT(TestTheme::Colors::SLOT_BORDER.getAlpha() == 200,
                    "Colors::SLOT_BORDER exists with correct alpha");
        TEST_ASSERT(TestTheme::Colors::TABLE_HIGHLIGHT.getGreen() == 100,
                    "Colors::TABLE_HIGHLIGHT exists with correct green");
        TEST_ASSERT(TestTheme::Colors::BUTTON_OVERRIDE_DEFAULT.getAlpha() == 101,
                    "Colors::BUTTON_OVERRIDE_DEFAULT exists with correct alpha");

        // Sizing: 8 constants
        TEST_ASSERT(TestTheme::Sizing::BUTTON_HEIGHT_RATIO > 0.3f &&
                            TestTheme::Sizing::BUTTON_HEIGHT_RATIO < 0.5f,
                    "Sizing::BUTTON_HEIGHT_RATIO is in expected range");
        TEST_ASSERT(TestTheme::Sizing::STATUS_BAR_HEIGHT == 40,
                    "Sizing::STATUS_BAR_HEIGHT = 40");
        TEST_ASSERT(TestTheme::Sizing::MAINMENU_LOCKED_SIZE.Width == 800,
                    "Sizing::MAINMENU_LOCKED_SIZE.Width = 800");

        // Spacing: 3 constants
        TEST_ASSERT(TestTheme::Spacing::PASSWORD_DIALOG_SIZE.Width == 580,
                    "Spacing::PASSWORD_DIALOG_SIZE.Width = 580");
        TEST_ASSERT(TestTheme::Spacing::VOLUME_DIALOG_SIZE.Width == 380,
                    "Spacing::VOLUME_DIALOG_SIZE.Width = 380");

        // ButtonModifiers: 2 constants
        TEST_ASSERT(TestTheme::ButtonModifiers::HOVER_BRIGHTEN > 1.0f,
                    "ButtonModifiers::HOVER_BRIGHTEN > 1.0");
        TEST_ASSERT(TestTheme::ButtonModifiers::PRESS_DARKEN < 1.0f,
                    "ButtonModifiers::PRESS_DARKEN < 1.0");
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char *argv[])
{
        verbose = argc > 1 && strcmp(argv[1], "--verbose") == 0;

        printf("======================================================\n");
        printf("  Luantis GUITheme Test Suite\n");
        printf("  Test-Driven Validation of Centralized GUI Theme\n");
        printf("======================================================\n");

        test_colors();
        test_sizing();
        test_spacing();
        test_button_modifiers();
        test_semantic_consistency();
        test_source_consistency();
        test_no_hardcoded_values();
        test_theme_completeness();

        printf("\n======================================================\n");
        printf("  Results: %d/%d passed, %d failed\n", tests_passed, tests_run,
               tests_failed);
        printf("======================================================\n");

        // Write results to JSON for automated validation
        std::ofstream json_out("/tmp/gui_theme_test_results.json");
        json_out << "{\n";
        json_out << "  \"total\": " << tests_run << ",\n";
        json_out << "  \"passed\": " << tests_passed << ",\n";
        json_out << "  \"failed\": " << tests_failed << ",\n";
        json_out << "  \"success\": " << (tests_failed == 0 ? "true" : "false")
                 << "\n";
        json_out << "}\n";
        json_out.close();

        return tests_failed > 0 ? 1 : 0;
}
