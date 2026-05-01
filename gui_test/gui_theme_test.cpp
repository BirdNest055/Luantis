// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// GUITheme Test Program — TRUE Test-Driven Validation (v9.53)
//
// This test validates GUITheme in two ways:
// 1. PARSE-AND-COMPARE: Parses GUITheme.h at runtime and verifies the
//    actual constant values match expectations — no copy drift possible
// 2. USAGE VERIFICATION: Scans source files to verify they USE GUITheme::
//    constants (not just removed old hardcoded values)
// 3. NO-REGRESSION: Scans for hardcoded style values that should be gone
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
#include <regex>
#include <map>

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

static bool floatEq(float a, float b, float eps = 0.0001f)
{
        return fabsf(a - b) < eps;
}

// ============================================================================
// GUITheme.h Parser — Reads the REAL header file and extracts values
// ============================================================================
// This approach prevents copy drift: we parse the actual source file
// instead of duplicating constants in the test.

struct ParsedColor {
        std::string name;
        u32 a, r, g, b;
        bool found;
};

struct ParsedFloat {
        std::string name;
        float value;
        bool found;
};

struct ParsedInt {
        std::string name;
        long long value;
        bool found;
};

static std::string gui_dir = "/home/z/my-project/Luantis/src/gui/";
static std::string theme_h_path = gui_dir + "GUITheme.h";

// Read entire file into string
static std::string readFile(const std::string &path)
{
        std::ifstream f(path);
        if (!f.is_open()) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
}

// Extract SColor(a, r, g, b) values from a line
static bool parseSColor(const std::string &line, u32 &a, u32 &r, u32 &g, u32 &b)
{
        // Match SColor(140, 0, 0, 0) or SColor(0xffc5000b)
        if (line.find("SColor(") == std::string::npos) return false;

        // Try ARGB format: SColor(A, R, G, B)
        auto start = line.find("SColor(");
        if (start == std::string::npos) return false;
        auto args = line.substr(start + 7);
        auto end = args.find(')');
        if (end == std::string::npos) return false;
        args = args.substr(0, end);

        // Check for hex format: 0x...
        if (args.find("0x") != std::string::npos || args.find("0X") != std::string::npos) {
                // Packed format: SColor(0xAARRGGBB)
                u32 packed = (u32)strtoul(args.c_str(), nullptr, 16);
                a = (packed >> 24) & 0xff;
                r = (packed >> 16) & 0xff;
                g = (packed >> 8) & 0xff;
                b = packed & 0xff;
                return true;
        }

        // Parse comma-separated values
        std::vector<u32> vals;
        std::istringstream iss(args);
        std::string token;
        while (std::getline(iss, token, ',')) {
                // Trim whitespace
                size_t start2 = token.find_first_not_of(" \t");
                size_t end2 = token.find_last_not_of(" \t");
                if (start2 != std::string::npos)
                        token = token.substr(start2, end2 - start2 + 1);
                vals.push_back((u32)atoi(token.c_str()));
        }
        if (vals.size() == 4) {
                a = vals[0]; r = vals[1]; g = vals[2]; b = vals[3];
                return true;
        }
        return false;
}

// ============================================================================
// Test Suite 1: Parse GUITheme.h and Validate Values (DRIFT DETECTION)
// ============================================================================
void test_drift_detection()
{
        printf("\n=== Suite 1: GUITheme.h Value Parsing (Drift Detection) ===\n");

        std::string content = readFile(theme_h_path);
        TEST_ASSERT(!content.empty(), "GUITheme.h is readable and non-empty");

        if (content.empty()) return; // Can't proceed without the file

        // Key color constants that MUST have specific values
        struct ExpectedColor {
                const char *name;
                u32 a, r, g, b;
        };
        ExpectedColor expected_colors[] = {
                {"MODAL_BG", 140, 0, 0, 0},
                {"MODAL_BG_FULLSCREEN", 255, 0, 0, 0},
                {"TOOLTIP_BG", 255, 110, 130, 60},
                {"TRANSPARENT", 0, 0, 0, 0},
                {"BUTTON_OVERRIDE_DEFAULT", 101, 255, 255, 255},
                {"SLOT_BG_NORMAL", 255, 128, 128, 128},
                {"SLOT_BG_HOVERED", 255, 192, 192, 192},
                {"TABLE_HIGHLIGHT", 255, 70, 100, 50},
                {"STATUS_TEXT_MAIN_BG", 220, 0, 0, 0},
                {"HYPERTEXT_DEFAULT", 255, 238, 238, 238},
                {"HYPERTEXT_HOVER", 255, 255, 0, 0},
                {"HYPERTEXT_LINK", 255, 0, 0, 255},
                {"TOUCH_ERROR", 255, 255, 0, 0},
                {"DEBUG_HIGHLIGHT", 34, 255, 255, 0},
                {NULL, 0, 0, 0, 0}
        };

        for (int i = 0; expected_colors[i].name; i++) {
                std::string search = std::string("const video::SColor ") + expected_colors[i].name;
                bool found = false;
                u32 pa = 0, pr = 0, pg = 0, pb = 0;

                std::istringstream iss(content);
                std::string line;
                while (std::getline(iss, line)) {
                        if (line.find(search) != std::string::npos) {
                                if (parseSColor(line, pa, pr, pg, pb)) {
                                        found = true;
                                        break;
                                }
                        }
                }

                if (found) {
                        std::string msg = std::string(expected_colors[i].name) +
                                          " = SColor(" + std::to_string(expected_colors[i].a) + "," +
                                          std::to_string(expected_colors[i].r) + "," +
                                          std::to_string(expected_colors[i].g) + "," +
                                          std::to_string(expected_colors[i].b) + ")";
                        TEST_ASSERT(pa == expected_colors[i].a &&
                                    pr == expected_colors[i].r &&
                                    pg == expected_colors[i].g &&
                                    pb == expected_colors[i].b, msg.c_str());
                } else {
                        std::string msg = std::string(expected_colors[i].name) + " found in GUITheme.h";
                        TEST_ASSERT(false, msg.c_str());
                }
        }
}

// ============================================================================
// Test Suite 2: Structure Verification (Namespaces, validate())
// ============================================================================
void test_structure()
{
        printf("\n=== Suite 2: GUITheme.h Structure ===\n");

        std::string content = readFile(theme_h_path);
        TEST_ASSERT(!content.empty(), "GUITheme.h readable for structure check");

        if (content.empty()) return;

        TEST_ASSERT(content.find("namespace GUITheme") != std::string::npos,
                    "Has 'namespace GUITheme'");
        TEST_ASSERT(content.find("namespace Colors") != std::string::npos,
                    "Has 'namespace Colors'");
        TEST_ASSERT(content.find("namespace Sizing") != std::string::npos,
                    "Has 'namespace Sizing'");
        TEST_ASSERT(content.find("namespace Timing") != std::string::npos,
                    "Has 'namespace Timing'");
        TEST_ASSERT(content.find("namespace ButtonModifiers") != std::string::npos,
                    "Has 'namespace ButtonModifiers'");
        TEST_ASSERT(content.find("namespace Fonts") != std::string::npos,
                    "Has 'namespace Fonts'");
        TEST_ASSERT(content.find("namespace Sounds") != std::string::npos,
                    "Has 'namespace Sounds'");
        TEST_ASSERT(content.find("namespace Dialogs") != std::string::npos,
                    "Has 'namespace Dialogs'");
        TEST_ASSERT(content.find("bool validate()") != std::string::npos,
                    "Has validate() function");
        TEST_ASSERT(content.find("#pragma once") != std::string::npos,
                    "Has #pragma once include guard");
        TEST_ASSERT(content.find("GUITheme.cpp") == std::string::npos,
                    "Does not reference GUITheme.cpp (no cross-deps in header)");
}

// ============================================================================
// Test Suite 3: GUITheme:: Usage Verification — Source files USE the constants
// ============================================================================
void test_guitheme_usage()
{
        printf("\n=== Suite 3: GUITheme:: Usage in Source Files ===\n");

        struct FileExpectation {
                const char *filename;
                const char *required_ref;
        };
        FileExpectation expectations[] = {
                {"guiFormSpecMenu.cpp", "GUITheme::Colors::MODAL_BG"},
                {"guiButton.cpp", "GUITheme::ButtonModifiers"},
                {"guiPasswordChange.cpp", "GUITheme::Dialogs"},
                {"guiVolumeChange.cpp", "GUITheme::Dialogs"},
                {"guiOpenURL.cpp", "GUITheme::Dialogs"},
                {"guiChatConsole.cpp", "GUITheme::Colors"},
                {"guiInventoryList.cpp", "GUITheme::Sizing"},
                {"guiTable.cpp", "GUITheme::Colors"},
                {"statusTextHelper.cpp", "GUITheme::Colors"},
                {"guiEngine.cpp", "GUITheme::Sizing"},
                {"guiHyperText.cpp", "GUITheme::Colors"},
                {"drawItemStack.cpp", "GUITheme::Colors"},
                {"modalMenu.cpp", "GUITheme::Sizing"},
                {"profilergraph.cpp", "GUITheme::Colors"},
                {"guiScene.cpp", "GUITheme::Sizing"},
                {NULL, NULL}
        };

        for (int i = 0; expectations[i].filename; i++) {
                std::string path = gui_dir + expectations[i].filename;
                std::ifstream f(path);
                bool found = false;
                if (f.is_open()) {
                        std::string line;
                        while (std::getline(f, line)) {
                                if (line.find(expectations[i].required_ref) != std::string::npos) {
                                        found = true;
                                        break;
                                }
                        }
                }
                std::string msg = std::string(expectations[i].filename) +
                                  " uses " + expectations[i].required_ref;
                TEST_ASSERT(found, msg.c_str());
        }
}

// ============================================================================
// Test Suite 4: Include Verification — Source files include GUITheme.h
// ============================================================================
void test_guitheme_include()
{
        printf("\n=== Suite 4: #include 'GUITheme.h' in Source Files ===\n");

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
                "guiScene.cpp",
                NULL
        };

        for (int i = 0; files[i]; i++) {
                std::string path = gui_dir + files[i];
                std::ifstream f(path);
                bool found = false;
                if (f.is_open()) {
                        std::string line;
                        while (std::getline(f, line)) {
                                if (line.find("#include") != std::string::npos &&
                                    line.find("GUITheme") != std::string::npos) {
                                        found = true;
                                        break;
                                }
                        }
                }
                std::string msg = std::string(files[i]) + " #includes GUITheme.h";
                TEST_ASSERT(found, msg.c_str());
        }
}

// ============================================================================
// Test Suite 5: No Hardcoded Values Remain
// ============================================================================
void test_no_hardcoded_values()
{
        printf("\n=== Suite 5: No Hardcoded Style Values Remain ===\n");

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
                "guiScene.cpp",
                NULL
        };

        // Check that key hardcoded MODAL_BG value SColor(140,0,0,0) is gone
        for (int i = 0; files[i]; i++) {
                std::string path = gui_dir + files[i];
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

        // guiButton.cpp should not have COLOR_HOVERED_MOD/COLOR_PRESSED_MOD
        {
                std::ifstream f(gui_dir + "guiButton.cpp");
                std::string line;
                bool found_hover = false, found_press = false;
                while (std::getline(f, line)) {
                        if (line.find("COLOR_HOVERED_MOD") != std::string::npos) found_hover = true;
                        if (line.find("COLOR_PRESSED_MOD") != std::string::npos) found_press = true;
                }
                TEST_ASSERT(!found_hover, "COLOR_HOVERED_MOD removed from guiButton.cpp");
                TEST_ASSERT(!found_press, "COLOR_PRESSED_MOD removed from guiButton.cpp");
        }

        // guiScene.cpp should NOT have local ROTATION_MAX constants
        {
                std::ifstream f(gui_dir + "guiScene.cpp");
                std::string line;
                bool found_local_rot = false;
                while (std::getline(f, line)) {
                        if (line.find("ROTATION_MAX_1 = 60") != std::string::npos ||
                            line.find("ROTATION_MAX_2 = 300") != std::string::npos) {
                                found_local_rot = true;
                                break;
                        }
                }
                TEST_ASSERT(!found_local_rot,
                            "guiScene.cpp: no local ROTATION_MAX constants (uses GUITheme)");
        }

        // No Clay files
        std::ifstream clay1(gui_dir + "clay_gui_manager.h");
        TEST_ASSERT(!clay1.is_open(), "No clay_gui_manager.h");
        std::ifstream clay2(gui_dir + "clay_renderer.h");
        TEST_ASSERT(!clay2.is_open(), "No clay_renderer.h");
}

// ============================================================================
// Test Suite 6: validate() Function Check
// ============================================================================
void test_validate_function()
{
        printf("\n=== Suite 6: validate() Function in GUITheme.h ===\n");

        std::string content = readFile(theme_h_path);
        TEST_ASSERT(!content.empty(), "GUITheme.h readable for validate() check");

        if (content.empty()) return;

        // validate() must check key invariants
        TEST_ASSERT(content.find("MODAL_BG.getAlpha()") != std::string::npos,
                    "validate() checks MODAL_BG alpha");
        TEST_ASSERT(content.find("BUTTON_HEIGHT_RATIO <= 0") != std::string::npos,
                    "validate() checks BUTTON_HEIGHT_RATIO > 0");
        TEST_ASSERT(content.find("HOVER_BRIGHTEN <= 0") != std::string::npos,
                    "validate() checks HOVER_BRIGHTEN > 0");
        TEST_ASSERT(content.find("STATUS_TEXT_DURATION_GAME <= 0") != std::string::npos,
                    "validate() checks STATUS_TEXT_DURATION_GAME > 0");
        TEST_ASSERT(content.find("PASSWORD_CHANGE_SIZE.Width == 0") != std::string::npos,
                    "validate() checks PASSWORD_CHANGE_SIZE.Width > 0");
        TEST_ASSERT(content.find("return true") != std::string::npos,
                    "validate() returns true on success");
}

// ============================================================================
// Test Suite 7: GUITheme.cpp Exists and Has Init Stub
// ============================================================================
void test_guitheme_cpp()
{
        printf("\n=== Suite 7: GUITheme.cpp Implementation ===\n");

        std::string cpp_path = gui_dir + "GUITheme.cpp";
        std::string content = readFile(cpp_path);
        TEST_ASSERT(!content.empty(), "GUITheme.cpp exists and is readable");

        if (content.empty()) return;

        TEST_ASSERT(content.find("GUITheme_Init") != std::string::npos,
                    "GUITheme.cpp has GUITheme_Init() stub for hot-reload");
        TEST_ASSERT(content.find("#include \"GUITheme.h\"") != std::string::npos,
                    "GUITheme.cpp includes GUITheme.h");
}

// ============================================================================
// Test Suite 8: New v9.53 Constants Present
// ============================================================================
void test_v953_constants()
{
        printf("\n=== Suite 8: v9.53 New Constants ===\n");

        std::string content = readFile(theme_h_path);
        TEST_ASSERT(!content.empty(), "GUITheme.h readable for v9.53 check");

        if (content.empty()) return;

        TEST_ASSERT(content.find("SLOT_SPACING_X_RATIO") != std::string::npos,
                    "Has SLOT_SPACING_X_RATIO");
        TEST_ASSERT(content.find("SLOT_SPACING_Y_RATIO") != std::string::npos,
                    "Has SLOT_SPACING_Y_RATIO");
        TEST_ASSERT(content.find("INV_PADDING_RATIO") != std::string::npos,
                    "Has INV_PADDING_RATIO");
        TEST_ASSERT(content.find("BTN_HEIGHT_OFFSET_RATIO") != std::string::npos,
                    "Has BTN_HEIGHT_OFFSET_RATIO");
        TEST_ASSERT(content.find("FONT_LINE_HEIGHT_RATIO") != std::string::npos,
                    "Has FONT_LINE_HEIGHT_RATIO");
        TEST_ASSERT(content.find("BTN_HEIGHT_MULT") != std::string::npos,
                    "Has BTN_HEIGHT_MULT");
        TEST_ASSERT(content.find("BTN_OFFSET_RATIO") != std::string::npos,
                    "Has BTN_OFFSET_RATIO");
        TEST_ASSERT(content.find("PADDING_HALF_RATIO") != std::string::npos,
                    "Has PADDING_HALF_RATIO");
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char *argv[])
{
        verbose = argc > 1 && strcmp(argv[1], "--verbose") == 0;

        printf("======================================================\n");
        printf("  Luantis GUITheme Test Suite (v9.53)\n");
        printf("  TRUE Test-Driven Validation\n");
        printf("  PARSES REAL GUITheme.h — No copy drift possible\n");
        printf("  Verifies values + usage + no regression\n");
        printf("======================================================\n");

        test_drift_detection();
        test_structure();
        test_guitheme_usage();
        test_guitheme_include();
        test_no_hardcoded_values();
        test_validate_function();
        test_guitheme_cpp();
        test_v953_constants();

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
