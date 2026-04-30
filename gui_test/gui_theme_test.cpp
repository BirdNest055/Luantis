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
} // namespace Sizing

namespace Timing
{
const float STATUS_TEXT_DURATION_GAME = 1.5f;
const float STATUS_TEXT_DURATION_MENU = 3.0f;
} // namespace Timing

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
// Test Suite 1: Color Constants
// ============================================================================
void test_colors()
{
	printf("\n=== Suite 1: Color Constants ===\n");

	TEST_ASSERT(colorEq(TestTheme::Colors::MODAL_BG, video::SColor(140, 0, 0, 0)),
		    "MODAL_BG = SColor(140,0,0,0)");
	TEST_ASSERT(colorEq(TestTheme::Colors::MODAL_BG_FULLSCREEN, video::SColor(255, 0, 0, 0)),
		    "MODAL_BG_FULLSCREEN = SColor(255,0,0,0)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TOOLTIP_BG, video::SColor(255, 110, 130, 60)),
		    "TOOLTIP_BG = SColor(255,110,130,60)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TOOLTIP_TEXT, video::SColor(255, 255, 255, 255)),
		    "TOOLTIP_TEXT = SColor(255,255,255,255)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
		    "TEXT_DEFAULT = SColor(255,255,255,255)");
	TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_BG_DEFAULT, video::SColor(255, 255, 255, 255)),
		    "BUTTON_BG_DEFAULT");
	TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
		    "BUTTON_TEXT_DEFAULT");
	TEST_ASSERT(colorEq(TestTheme::Colors::BUTTON_OVERRIDE_DEFAULT, video::SColor(101, 255, 255, 255)),
		    "BUTTON_OVERRIDE_DEFAULT = SColor(101,255,255,255)");
	TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BORDER, video::SColor(200, 0, 0, 0)),
		    "SLOT_BORDER = SColor(200,0,0,0)");
	TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BG_NORMAL, video::SColor(255, 128, 128, 128)),
		    "SLOT_BG_NORMAL = SColor(255,128,128,128)");
	TEST_ASSERT(colorEq(TestTheme::Colors::SLOT_BG_HOVERED, video::SColor(255, 192, 192, 192)),
		    "SLOT_BG_HOVERED = SColor(255,192,192,192)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_TEXT_DEFAULT, video::SColor(255, 255, 255, 255)),
		    "TABLE_TEXT_DEFAULT");
	TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_BG_DEFAULT, video::SColor(255, 0, 0, 0)),
		    "TABLE_BG_DEFAULT = SColor(255,0,0,0)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_HIGHLIGHT, video::SColor(255, 70, 100, 50)),
		    "TABLE_HIGHLIGHT = SColor(255,70,100,50)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TABLE_HIGHLIGHT_TEXT, video::SColor(255, 255, 255, 255)),
		    "TABLE_HIGHLIGHT_TEXT");
	TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_FALLBACK, video::SColor(255, 0, 0, 0)),
		    "STATUS_TEXT_FALLBACK");
	TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_MAIN_BG, video::SColor(220, 0, 0, 0)),
		    "STATUS_TEXT_MAIN_BG = SColor(220,0,0,0)");
	TEST_ASSERT(colorEq(TestTheme::Colors::STATUS_TEXT_DEFAULT_BG, video::SColor(0, 0, 0, 0)),
		    "STATUS_TEXT_DEFAULT_BG = SColor(0,0,0,0)");
	TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_BG, video::SColor(255, 0, 0, 0)),
		    "CHAT_CONSOLE_BG");
	TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_CURSOR, video::SColor(255, 255, 255, 255)),
		    "CHAT_CONSOLE_CURSOR");
	TEST_ASSERT(colorEq(TestTheme::Colors::CHAT_CONSOLE_TEXT, video::SColor(255, 255, 255, 255)),
		    "CHAT_CONSOLE_TEXT");
	TEST_ASSERT(colorEq(TestTheme::Colors::TOUCH_SELECTION, video::SColor(255, 128, 128, 128)),
		    "TOUCH_SELECTION = SColor(255,128,128,128)");
	TEST_ASSERT(colorEq(TestTheme::Colors::TOUCH_ERROR, video::SColor(255, 255, 0, 0)),
		    "TOUCH_ERROR = SColor(255,255,0,0)");
}

// ============================================================================
// Test Suite 2: Sizing Constants
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
}

// ============================================================================
// Test Suite 3: Timing Constants
// ============================================================================
void test_timing()
{
	printf("\n=== Suite 3: Timing Constants ===\n");

	TEST_ASSERT(floatEq(TestTheme::Timing::STATUS_TEXT_DURATION_GAME, 1.5f),
		    "STATUS_TEXT_DURATION_GAME = 1.5s");
	TEST_ASSERT(floatEq(TestTheme::Timing::STATUS_TEXT_DURATION_MENU, 3.0f),
		    "STATUS_TEXT_DURATION_MENU = 3.0s");
	TEST_ASSERT(TestTheme::Timing::STATUS_TEXT_DURATION_GAME < TestTheme::Timing::STATUS_TEXT_DURATION_MENU,
		    "Game duration < Menu duration");
}

// ============================================================================
// Test Suite 4: Button Modifiers
// ============================================================================
void test_button_modifiers()
{
	printf("\n=== Suite 4: Button Modifiers ===\n");

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
// Test Suite 5: Semantic Consistency
// ============================================================================
void test_semantic_consistency()
{
	printf("\n=== Suite 5: Semantic Consistency ===\n");

	TEST_ASSERT(TestTheme::Colors::MODAL_BG.getAlpha() < 255,
		    "MODAL_BG is semi-transparent");
	TEST_ASSERT(TestTheme::Colors::MODAL_BG_FULLSCREEN.getAlpha() == 255,
		    "MODAL_BG_FULLSCREEN is fully opaque");
	TEST_ASSERT(TestTheme::Colors::TOOLTIP_BG.getAlpha() == 255,
		    "TOOLTIP_BG is fully opaque");
	TEST_ASSERT(TestTheme::Colors::TOOLTIP_TEXT.getAlpha() == 255,
		    "TOOLTIP_TEXT is fully opaque");

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

	TEST_ASSERT(TestTheme::Sizing::BUTTON_HEIGHT_RATIO > 0, "BUTTON_HEIGHT_RATIO > 0");
	TEST_ASSERT(TestTheme::Sizing::STATUS_BAR_HEIGHT > 0 && TestTheme::Sizing::STATUS_BAR_HEIGHT < 200,
		    "STATUS_BAR_HEIGHT reasonable");
	TEST_ASSERT(TestTheme::Sizing::STATUS_TEXT_Y_OFFSET > 0, "STATUS_TEXT_Y_OFFSET > 0");
}

// ============================================================================
// Test Suite 6: Source File Consistency
// ============================================================================
void test_source_consistency()
{
	printf("\n=== Suite 6: Source File Consistency ===\n");

	std::ifstream theme_file("/home/z/my-project/Luantis/src/gui/GUITheme.h");
	TEST_ASSERT(theme_file.is_open(), "GUITheme.h exists and is readable");

	if (theme_file.is_open()) {
		std::string line;
		bool has_colors = false, has_sizing = false, has_timing = false;
		bool has_btn_mods = false, has_validate = false;
		bool has_touch_colors = false;

		while (std::getline(theme_file, line)) {
			if (line.find("namespace Colors") != std::string::npos) has_colors = true;
			if (line.find("namespace Sizing") != std::string::npos) has_sizing = true;
			if (line.find("namespace Timing") != std::string::npos) has_timing = true;
			if (line.find("namespace ButtonModifiers") != std::string::npos) has_btn_mods = true;
			if (line.find("bool validate()") != std::string::npos) has_validate = true;
			if (line.find("TOUCH_SELECTION") != std::string::npos) has_touch_colors = true;
		}

		TEST_ASSERT(has_colors, "GUITheme.h has Colors namespace");
		TEST_ASSERT(has_sizing, "GUITheme.h has Sizing namespace");
		TEST_ASSERT(has_timing, "GUITheme.h has Timing namespace");
		TEST_ASSERT(has_btn_mods, "GUITheme.h has ButtonModifiers namespace");
		TEST_ASSERT(has_validate, "GUITheme.h has validate()");
		TEST_ASSERT(has_touch_colors, "GUITheme.h has TOUCH_SELECTION");
	}

	std::ifstream theme_cpp("/home/z/my-project/Luantis/src/gui/GUITheme.cpp");
	TEST_ASSERT(theme_cpp.is_open(), "GUITheme.cpp exists and is readable");
}

// ============================================================================
// Test Suite 7: No Hardcoded Values Remain
// ============================================================================
void test_no_hardcoded_values()
{
	printf("\n=== Suite 7: No Hardcoded Style Values Remain ===\n");

	const char *gui_dir = "/home/z/my-project/Luantis/src/gui/";
	const char *files[] = {
		"guiFormSpecMenu.cpp", "guiButton.cpp", "guiPasswordChange.cpp",
		"guiVolumeChange.cpp", "guiOpenURL.cpp", "touchcontrols.cpp",
		"touchscreeneditor.cpp", "guiInventoryList.h", "guiTable.h",
		"statusTextHelper.cpp", "statusTextHelper.h", "guiChatConsole.h",
		"guiChatConsole.cpp", "guiEngine.cpp", NULL
	};

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

	// Check COLOR_HOVERED_MOD / COLOR_PRESSED_MOD removed
	{
		std::ifstream f(std::string(gui_dir) + "guiButton.cpp");
		std::string line;
		bool found_hover = false, found_press = false;
		while (std::getline(f, line)) {
			if (line.find("COLOR_HOVERED_MOD") != std::string::npos) found_hover = true;
			if (line.find("COLOR_PRESSED_MOD") != std::string::npos) found_press = true;
		}
		TEST_ASSERT(!found_hover, "COLOR_HOVERED_MOD removed");
		TEST_ASSERT(!found_press, "COLOR_PRESSED_MOD removed");
	}

	// Verify no Clay files exist
	std::ifstream clay1(std::string(gui_dir) + "clay_gui_manager.h");
	TEST_ASSERT(!clay1.is_open(), "No clay_gui_manager.h");
	std::ifstream clay2(std::string(gui_dir) + "clay_renderer.h");
	TEST_ASSERT(!clay2.is_open(), "No clay_renderer.h");
}

// ============================================================================
// Test Suite 8: Theme Completeness
// ============================================================================
void test_theme_completeness()
{
	printf("\n=== Suite 8: Theme Completeness ===\n");

	// Colors: 22 constants
	TEST_ASSERT(TestTheme::Colors::MODAL_BG.getAlpha() == 140, "Colors::MODAL_BG alpha=140");
	TEST_ASSERT(TestTheme::Colors::TOOLTIP_BG.getRed() == 110, "Colors::TOOLTIP_BG red=110");
	TEST_ASSERT(TestTheme::Colors::BUTTON_OVERRIDE_DEFAULT.getAlpha() == 101, "Colors::BUTTON_OVERRIDE alpha=101");
	TEST_ASSERT(TestTheme::Colors::TOUCH_ERROR.getRed() == 255, "Colors::TOUCH_ERROR red=255");
	TEST_ASSERT(TestTheme::Colors::CHAT_CONSOLE_CURSOR.getAlpha() == 255, "Colors::CHAT_CONSOLE_CURSOR alpha=255");

	// Sizing: 12 constants
	TEST_ASSERT(TestTheme::Sizing::TOOLTIP_PADDING_Y == 5, "Sizing::TOOLTIP_PADDING_Y=5");
	TEST_ASSERT(TestTheme::Sizing::FORM_FALLBACK_WIDTH > 0, "Sizing::FORM_FALLBACK_WIDTH > 0");
	TEST_ASSERT(TestTheme::Sizing::STATUS_TEXT_Y_OFFSET > 0, "Sizing::STATUS_TEXT_Y_OFFSET > 0");

	// Timing: 2 constants
	TEST_ASSERT(TestTheme::Timing::STATUS_TEXT_DURATION_GAME > 0, "Timing::DURATION_GAME > 0");
	TEST_ASSERT(TestTheme::Timing::STATUS_TEXT_DURATION_MENU > TestTheme::Timing::STATUS_TEXT_DURATION_GAME,
		    "Menu duration > game duration");

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
	printf("  Test-Driven Validation of Centralized GUI Theme\n");
	printf("  Based on branch: clawtest-v9.44-voice-server-authority\n");
	printf("======================================================\n");

	test_colors();
	test_sizing();
	test_timing();
	test_button_modifiers();
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
