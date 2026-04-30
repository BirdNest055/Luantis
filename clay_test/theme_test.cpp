/*
 * Theme-Driven Clay UI E2E Test
 *
 * Tests that clay_theme.h produces correct visual output.
 * Builds ALL layouts using Theme::Styles and Theme constants,
 * then outputs JSONL render commands for the Python renderer.
 *
 * Test 1: Pause Menu — uses Theme::Styles::panelOverlay(), button(), etc.
 * Test 2: Pause Menu with CUSTOM theme overrides (dark green theme)
 * Test 3: Settings Dialog — uses Theme::Colors, Fonts, Sizing
 * Test 4: HUD — uses Theme::Sizing, Colors
 * Test 5: Component test — individual style isolation
 *
 * Build:
 *   g++ -std=c++20 -O2 -I.. -I../lib/clay -o theme_test theme_test.cpp -lm
 *
 * Run:
 *   ./theme_test > /tmp/theme_test.jsonl
 *   python3 render_clay_log.py /tmp/theme_test.jsonl /tmp/theme_screenshots
 */

#define CLAY_IMPLEMENTATION
#include "../lib/clay/clay.h"
#include "../src/gui/clay_theme.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

// ---------------------------------------------------------------------------
// JSON escape helper
// ---------------------------------------------------------------------------

static void writeJsonEscaped(const char *s, int len)
{
        for (int i = 0; i < len; i++) {
                unsigned char c = (unsigned char)s[i];
                switch (c) {
                case '"':  printf("\\\""); break;
                case '\\': printf("\\\\"); break;
                case '\n': printf("\\n");  break;
                case '\r': printf("\\r");  break;
                case '\t': printf("\\t");  break;
                default:
                        if (c < 0x20) {
                                printf("\\u%04x", c);
                        } else {
                                putchar(c);
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// Clay text measurement (monospace approximation — uses Theme ratio)
// ---------------------------------------------------------------------------

static Clay_Dimensions measureText(Clay_StringSlice text,
        Clay_TextElementConfig *config, void *userData)
{
        (void)userData;
        float width = (float)text.length * (float)config->fontSize
                * Theme::Renderer::fallbackTextWidthRatio;
        float height = (float)config->fontSize * 1.2f;
        return (Clay_Dimensions){ .width = width, .height = height };
}

// ---------------------------------------------------------------------------
// Clay error handler
// ---------------------------------------------------------------------------

static void errorHandler(Clay_ErrorData err)
{
        fprintf(stderr, "Clay error: %.*s\n", err.errorText.length, err.errorText.chars);
}

// ---------------------------------------------------------------------------
// Dynamic ID helper — creates Clay_ElementId from a C string variable
// ---------------------------------------------------------------------------

static Clay_ElementId makeId(const char *s)
{
        return Clay_GetElementId((Clay_String){
                .isStaticallyAllocated = false,
                .length = (int32_t)strlen(s),
                .chars = s
        });
}

// ---------------------------------------------------------------------------
// Button component using Theme::Styles (mirrors ClayPauseMenu)
// ---------------------------------------------------------------------------

static void themeButton(const char *id, const char *label)
{
        Clay_ElementId clayId = makeId(id);
        Clay_ElementDeclaration btnStyle = Theme::Styles::button();
        Clay_TextElementConfig textCfg = Theme::Styles::buttonTextConfig();

        CLAY(clayId, btnStyle) {
                Clay_String labelStr = {.isStaticallyAllocated = false,
                        .length = (int32_t)strlen(label), .chars = label};
                CLAY_TEXT(labelStr, textCfg);
        }
}

static void themeButtonHover(const char *id, const char *label)
{
        Clay_ElementId clayId = makeId(id);
        Clay_ElementDeclaration btnStyle = Theme::Styles::buttonHover();
        Clay_TextElementConfig textCfg = Theme::Styles::buttonHoverTextConfig();

        CLAY(clayId, btnStyle) {
                Clay_String labelStr = {.isStaticallyAllocated = false,
                        .length = (int32_t)strlen(label), .chars = label};
                CLAY_TEXT(labelStr, textCfg);
        }
}

// ---------------------------------------------------------------------------
// Layout 1: Pause Menu — FULLY driven by Theme::Styles
// ---------------------------------------------------------------------------

static void buildPauseMenuThemed(void)
{
        CLAY(CLAY_ID(THEME_ID_PAUSE_OVERLAY), Theme::Styles::panelOverlay()) {
                CLAY(CLAY_ID(THEME_ID_PAUSE_CONTENT), Theme::Styles::contentColumn()) {
                        // Title bar
                        CLAY(CLAY_ID(THEME_ID_PAUSE_TITLE), Theme::Styles::titleBar()) {
                                CLAY_TEXT(CLAY_STRING(THEME_LABEL_PAUSE_TITLE),
                                        Theme::Styles::titleTextConfig());
                        }

                        // Buttons (normal state)
                        themeButton(Theme::IDs::PauseMenu::btnResume,
                                Theme::Labels::PauseMenu::resume);
                        themeButton(Theme::IDs::PauseMenu::btnSettings,
                                Theme::Labels::PauseMenu::settings);
                        themeButton(Theme::IDs::PauseMenu::btnPassword,
                                Theme::Labels::PauseMenu::changePwd);

                        // One button shown in hover state to demonstrate
                        themeButtonHover(Theme::IDs::PauseMenu::btnVolume,
                                Theme::Labels::PauseMenu::volume);

                        // Separator
                        CLAY(CLAY_ID(THEME_ID_PAUSE_SEPARATOR), Theme::Styles::separator()) {}

                        themeButton(Theme::IDs::PauseMenu::btnExitMenu,
                                Theme::Labels::PauseMenu::exitToMenu);
                        themeButton(Theme::IDs::PauseMenu::btnExitOS,
                                Theme::Labels::PauseMenu::exitToOS);

                        // Bottom pad
                        CLAY(CLAY_ID(THEME_ID_PAUSE_BOTTOM_PAD), Theme::Styles::bottomPad()) {}
                }
        }
}

// ---------------------------------------------------------------------------
// Layout 2: Pause Menu with CUSTOM theme (dark green military theme)
//   We override specific Theme values inline to prove customization works
// ---------------------------------------------------------------------------

// Custom green palette (override Theme defaults in this scope)
namespace GreenTheme {
        static constexpr Clay_Color overlayBg     = {0.0f, 10.0f, 0.0f,  180.0f};
        static constexpr Clay_Color contentBg     = {20.0f, 40.0f, 20.0f, 255.0f};
        static constexpr Clay_Color titleBg       = {10.0f, 30.0f, 10.0f, 255.0f};
        static constexpr Clay_Color buttonBg      = {30.0f, 60.0f, 30.0f, 255.0f};
        static constexpr Clay_Color buttonHoverBg = {50.0f, 100.0f, 50.0f, 255.0f};
        static constexpr Clay_Color titleText     = {100.0f, 255.0f, 100.0f, 255.0f};
        static constexpr Clay_Color buttonText    = {180.0f, 220.0f, 180.0f, 255.0f};
        static constexpr Clay_Color buttonHoverText = {220.0f, 255.0f, 220.0f, 255.0f};
        static constexpr Clay_Color separator     = {60.0f, 100.0f, 60.0f, 255.0f};

        static constexpr int menuWidth = 500;
        static constexpr int cornerRadius = 8;
}

static void buildPauseMenuGreen(void)
{
        CLAY(CLAY_ID("GreenOverlay"), {
                .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = GreenTheme::overlayBg
        }) {
                CLAY(CLAY_ID("GreenContent"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_FIXED(GreenTheme::menuWidth), CLAY_SIZING_FIT(0)},
                                .childGap = Theme::Spacing::contentChildGap,
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = GreenTheme::contentBg,
                        .cornerRadius = {GreenTheme::cornerRadius, GreenTheme::cornerRadius,
                                GreenTheme::cornerRadius, GreenTheme::cornerRadius}
                }) {
                        // Title
                        CLAY(CLAY_ID("GreenTitle"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_FIXED(GreenTheme::menuWidth), CLAY_SIZING_FIXED(Theme::Sizing::titleHeight)},
                                        .padding = {Theme::Spacing::titlePadH, Theme::Spacing::titlePadH,
                                                Theme::Spacing::titlePadV, Theme::Spacing::titlePadV},
                                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                },
                                .backgroundColor = GreenTheme::titleBg,
                                .cornerRadius = {GreenTheme::cornerRadius, GreenTheme::cornerRadius, 0, 0}
                        }) {
                                CLAY_TEXT(CLAY_STRING("Tactical Interface"),
                                        CLAY_TEXT_CONFIG({
                                                .textColor = GreenTheme::titleText,
                                                .fontId = Theme::Fonts::standardFontId,
                                                .fontSize = Theme::Fonts::titleFontSize
                                        }));
                        }

                        // Green buttons (reusing Theme::Sizing/Spacing, overriding colors)
                        const char *greenBtns[] = {"Resume Mission", "Loadout", "Map", "Comms", "Exit"};
                        const char *greenIds[] = {"GBtn0", "GBtn1", "GBtn2", "GBtn3", "GBtn4"};
                        for (int i = 0; i < 5; i++) {
                                CLAY(makeId(greenIds[i]), {
                                        .layout = {
                                                .sizing = {CLAY_SIZING_FIXED(GreenTheme::menuWidth), CLAY_SIZING_FIXED(Theme::Sizing::buttonHeight)},
                                                .padding = {Theme::Spacing::buttonPadH, Theme::Spacing::buttonPadH,
                                                        Theme::Spacing::buttonPadV, Theme::Spacing::buttonPadV},
                                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                        },
                                        .backgroundColor = (i == 2) ? GreenTheme::buttonHoverBg : GreenTheme::buttonBg,
                                        .cornerRadius = {Theme::Radius::buttonRadius, Theme::Radius::buttonRadius,
                                                Theme::Radius::buttonRadius, Theme::Radius::buttonRadius}
                                }) {
                                        Clay_String lbl = {.isStaticallyAllocated = false,
                                                .length = (int32_t)strlen(greenBtns[i]), .chars = greenBtns[i]};
                                        CLAY_TEXT(lbl, CLAY_TEXT_CONFIG({
                                                .textColor = (i == 2) ? GreenTheme::buttonHoverText : GreenTheme::buttonText,
                                                .fontId = Theme::Fonts::standardFontId,
                                                .fontSize = Theme::Fonts::buttonFontSize
                                        }));
                                }
                        }

                        // Bottom pad
                        CLAY(CLAY_ID("GreenBottom"), {
                                .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(Theme::Sizing::bottomPadding)}},
                                .backgroundColor = GreenTheme::contentBg,
                                .cornerRadius = {0, 0, GreenTheme::cornerRadius, GreenTheme::cornerRadius}
                        }) {}
                }
        }
}

// ---------------------------------------------------------------------------
// Layout 3: Settings Dialog using Theme constants
// ---------------------------------------------------------------------------

static void buildSettingsThemed(void)
{
        CLAY(CLAY_ID("SettingsOverlay"), {
                .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = Theme::Colors::overlayBg
        }) {
                CLAY(CLAY_ID("SettingsCard"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_FIXED(600), CLAY_SIZING_FIXED(450)},
                                .padding = CLAY_PADDING_ALL(Theme::Spacing::panelPadding),
                                .childGap = Theme::Spacing::sectionGap,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = Theme::Colors::contentBg,
                        .cornerRadius = {Theme::Radius::dialogRadius, Theme::Radius::dialogRadius,
                                Theme::Radius::dialogRadius, Theme::Radius::dialogRadius}
                }) {
                        // Title bar
                        CLAY(CLAY_ID("SettingsTitle"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40)},
                                        .padding = CLAY_PADDING_ALL(8),
                                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                                },
                                .backgroundColor = Theme::Colors::titleBg,
                                .cornerRadius = {Theme::Radius::dialogRadius, Theme::Radius::dialogRadius, 0, 0}
                        }) {
                                CLAY_TEXT(CLAY_STRING("Settings"),
                                        CLAY_TEXT_CONFIG({
                                                .textColor = Theme::Colors::titleText,
                                                .fontId = Theme::Fonts::standardFontId,
                                                .fontSize = Theme::Fonts::headingFontSize
                                        }));
                        }

                        // Tab bar
                        CLAY(CLAY_ID("TabBar"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                                        .childGap = Theme::Spacing::tinyGap,
                                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                }
                        }) {
                                const char *tabNames[] = {"Graphics", "Sound", "Controls", "Privacy"};
                                const char *tabIds[] = {"Tab0", "Tab1", "Tab2", "Tab3"};
                                for (int i = 0; i < 4; i++) {
                                        bool active = (i == 0);
                                        CLAY(makeId(tabIds[i]), {
                                                .layout = {
                                                        .sizing = {CLAY_SIZING_PERCENT(0.25f), CLAY_SIZING_GROW(0)},
                                                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                                },
                                                .backgroundColor = active ? Theme::Colors::accent : Theme::Colors::buttonBg,
                                                .cornerRadius = {6, 6, 0, 0}
                                        }) {
                                                Clay_String t = {.isStaticallyAllocated = false,
                                                        .length = (int32_t)strlen(tabNames[i]), .chars = tabNames[i]};
                                                CLAY_TEXT(t, CLAY_TEXT_CONFIG({
                                                        .textColor = active ? Theme::Colors::buttonHoverText : Theme::Colors::dimText,
                                                        .fontId = Theme::Fonts::standardFontId,
                                                        .fontSize = Theme::Fonts::smallFontSize
                                                }));
                                        }
                                }
                        }

                        // Content area
                        CLAY(CLAY_ID("SettingsContent"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                        .padding = CLAY_PADDING_ALL(Theme::Spacing::panelPadding),
                                        .childGap = Theme::Spacing::itemGap,
                                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                },
                                .backgroundColor = Theme::Colors::buttonBg
                        }) {
                                const char *settingNames[] = {"Render Distance", "Smooth Lighting", "Fancy Leaves", "VSync"};
                                for (int i = 0; i < 4; i++) {
                                        char rowId[16]; snprintf(rowId, sizeof(rowId), "SRow%d", i);
                                        CLAY(makeId(rowId), {
                                                .layout = {
                                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(Theme::Sizing::inputHeight)},
                                                        .padding = {8, 4, 8, 4},
                                                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                                                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                                },
                                                .backgroundColor = Theme::Colors::buttonHoverBg,
                                                .cornerRadius = {Theme::Radius::inputRadius, Theme::Radius::inputRadius,
                                                        Theme::Radius::inputRadius, Theme::Radius::inputRadius}
                                        }) {
                                                Clay_String sn = {.isStaticallyAllocated = false,
                                                        .length = (int32_t)strlen(settingNames[i]), .chars = settingNames[i]};
                                                CLAY_TEXT(sn, CLAY_TEXT_CONFIG({
                                                        .textColor = Theme::Colors::bodyText,
                                                        .fontId = Theme::Fonts::standardFontId,
                                                        .fontSize = Theme::Fonts::bodyFontSize
                                                }));
                                                char spId[16]; snprintf(spId, sizeof(spId), "SRowSp%d", i);
                                                CLAY(makeId(spId), {
                                                        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}
                                                }) {}
                                                // Toggle
                                                char togId[16]; snprintf(togId, sizeof(togId), "Toggle%d", i);
                                                CLAY(makeId(togId), {
                                                        .layout = {.sizing = {CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(24)}},
                                                        .backgroundColor = (i % 2 == 0) ? Theme::Colors::success : Theme::Colors::buttonBg,
                                                        .cornerRadius = {12, 12, 12, 12}
                                                }) {}
                                        }
                                }
                        }

                        // Button bar using accent colors
                        CLAY(CLAY_ID("SettingsBtns"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44)},
                                        .childGap = Theme::Spacing::itemGap,
                                        .childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER},
                                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                }
                        }) {
                                const char *btnLabels[] = {"Cancel", "Apply", "OK"};
                                const Clay_Color btnColors[] = {Theme::Colors::buttonBg, Theme::Colors::success, Theme::Colors::accent};
                                for (int i = 0; i < 3; i++) {
                                        char btnId[16]; snprintf(btnId, sizeof(btnId), "SBtn%d", i);
                                        CLAY(makeId(btnId), {
                                                .layout = {
                                                        .sizing = {CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(34)},
                                                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                                },
                                                .backgroundColor = btnColors[i],
                                                .cornerRadius = {6, 6, 6, 6}
                                        }) {
                                                Clay_String bl = {.isStaticallyAllocated = false,
                                                        .length = (int32_t)strlen(btnLabels[i]), .chars = btnLabels[i]};
                                                CLAY_TEXT(bl, CLAY_TEXT_CONFIG({
                                                        .textColor = Theme::Colors::buttonText,
                                                        .fontId = Theme::Fonts::standardFontId,
                                                        .fontSize = Theme::Fonts::smallFontSize
                                                }));
                                        }
                                }
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// Layout 4: HUD using Theme::Sizing and Colors
// ---------------------------------------------------------------------------

static void buildHUDThemed(void)
{
        CLAY(CLAY_ID("HUDRoot"), {
                .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                }
        }) {
                // Top bar
                CLAY(CLAY_ID("HUDTop"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40)},
                                .padding = {8, 4, 8, 4},
                                .childGap = Theme::Spacing::tinyGap,
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        }
                }) {
                        // Health bar
                        CLAY(CLAY_ID("HPBar"), {
                                .layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0)}},
                                .backgroundColor = {40, 40, 40, 180},
                                .cornerRadius = {4, 4, 4, 4}
                        }) {
                                CLAY(CLAY_ID("HPFill"), {
                                        .layout = {.sizing = {CLAY_SIZING_PERCENT(0.85f), CLAY_SIZING_GROW(0)}},
                                        .backgroundColor = Theme::Colors::danger,
                                        .cornerRadius = {4, 4, 4, 4}
                                }) {
                                        CLAY_TEXT(CLAY_STRING("17/20"), Theme::Styles::bodyTextConfig());
                                }
                        }
                        // Armor bar
                        CLAY(CLAY_ID("ArmBar"), {
                                .layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0)}},
                                .backgroundColor = {40, 40, 40, 180},
                                .cornerRadius = {4, 4, 4, 4}
                        }) {
                                CLAY(CLAY_ID("ArmFill"), {
                                        .layout = {.sizing = {CLAY_SIZING_PERCENT(0.6f), CLAY_SIZING_GROW(0)}},
                                        .backgroundColor = Theme::Colors::accent,
                                        .cornerRadius = {4, 4, 4, 4}
                                }) {
                                        CLAY_TEXT(CLAY_STRING("12/20"), Theme::Styles::bodyTextConfig());
                                }
                        }
                        CLAY(CLAY_ID("TopSp"), {
                                .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}
                        }) {}
                        // Minimap
                        CLAY(CLAY_ID("Minimap"), {
                                .layout = {.sizing = {CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(80)}},
                                .backgroundColor = {30, 60, 30, 200},
                                .cornerRadius = {40, 40, 40, 40}
                        }) {
                                CLAY_TEXT(CLAY_STRING("Map"), Theme::Styles::dimTextConfig());
                        }
                }

                // Center spacer
                CLAY(CLAY_ID("HUDCenter"), {
                        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}
                }) {}

                // Bottom hotbar using Theme::Sizing constants
                CLAY(CLAY_ID("HUDBottom"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(60)},
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        }
                }) {
                        static char slotLabels[9][4];  // persistent across loop iterations
                        for (int i = 0; i < 9; i++) {
                                char id[32];
                                snprintf(id, sizeof(id), "HSlot%d", i);
                                snprintf(slotLabels[i], sizeof(slotLabels[i]), "%d", i + 1);

                                CLAY(makeId(id), {
                                        .layout = {
                                                .sizing = {CLAY_SIZING_FIXED(Theme::Sizing::hotbarSlotSize),
                                                        CLAY_SIZING_FIXED(Theme::Sizing::hotbarSlotSize)},
                                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                        },
                                        .backgroundColor = (i == 0) ? Theme::Colors::buttonHoverBg : Theme::Colors::contentBg,
                                        .cornerRadius = {4, 4, 4, 4}
                                }) {
                                        Clay_String ls = {.isStaticallyAllocated = false,
                                                .length = (int32_t)strlen(slotLabels[i]), .chars = slotLabels[i]};
                                        CLAY_TEXT(ls, CLAY_TEXT_CONFIG({
                                                .textColor = Theme::Colors::dimText,
                                                .fontId = Theme::Fonts::standardFontId,
                                                .fontSize = Theme::Fonts::hudFontSize
                                        }));
                                }
                                if (i < 8) {
                                        char gid[32];
                                        snprintf(gid, sizeof(gid), "HGap%d", i);
                                        CLAY(makeId(gid), {
                                                .layout = {.sizing = {CLAY_SIZING_FIXED(Theme::Sizing::hotbarGap),
                                                        CLAY_SIZING_FIXED(Theme::Sizing::hotbarSlotSize)}}
                                        }) {}
                                }
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// Layout 5: Component isolation test — render each style individually
// ---------------------------------------------------------------------------

static void buildComponentTest(void)
{
        CLAY(CLAY_ID("CompTestBg"), {
                .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(20),
                        .childGap = 16,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
                .backgroundColor = {20, 20, 30, 255}
        }) {
                // Column 1: Button states
                CLAY(CLAY_ID("Col1"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_PERCENT(0.33f), CLAY_SIZING_GROW(0)},
                                .childGap = 8,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        }
                }) {
                        CLAY_TEXT(CLAY_STRING("Button States"), Theme::Styles::titleTextConfig());
                        // Normal
                        CLAY(CLAY_ID("CBtn1"), Theme::Styles::button()) {
                                CLAY_TEXT(CLAY_STRING("Normal"), Theme::Styles::buttonTextConfig());
                        }
                        // Hover
                        CLAY(CLAY_ID("CBtn2"), Theme::Styles::buttonHover()) {
                                CLAY_TEXT(CLAY_STRING("Hover"), Theme::Styles::buttonHoverTextConfig());
                        }
                        // Pressed
                        CLAY(CLAY_ID("CBtn3"), Theme::Styles::buttonPressed()) {
                                CLAY_TEXT(CLAY_STRING("Pressed"), Theme::Styles::buttonPressedTextConfig());
                        }
                        // Separator
                        CLAY(CLAY_ID("CSep"), Theme::Styles::separator()) {}
                        // Accent button
                        CLAY(CLAY_ID("CBtn4"), {
                                .layout = Theme::Styles::button().layout,
                                .backgroundColor = Theme::Colors::accent,
                        }) {
                                CLAY_TEXT(CLAY_STRING("Accent"), Theme::Styles::buttonTextConfig());
                        }
                        // Danger button
                        CLAY(CLAY_ID("CBtn5"), {
                                .layout = Theme::Styles::button().layout,
                                .backgroundColor = Theme::Colors::danger,
                        }) {
                                CLAY_TEXT(CLAY_STRING("Danger"), Theme::Styles::buttonTextConfig());
                        }
                }

                // Column 2: Text styles
                CLAY(CLAY_ID("Col2"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_PERCENT(0.33f), CLAY_SIZING_GROW(0)},
                                .childGap = 8,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        }
                }) {
                        CLAY_TEXT(CLAY_STRING("Text Styles"), Theme::Styles::titleTextConfig());
                        CLAY_TEXT(CLAY_STRING("Title Text (28px)"), Theme::Styles::titleTextConfig());
                        CLAY_TEXT(CLAY_STRING("Body Text (16px)"), Theme::Styles::bodyTextConfig());
                        CLAY_TEXT(CLAY_STRING("Dim Text (14px)"), Theme::Styles::dimTextConfig());
                        CLAY_TEXT(CLAY_STRING("Button Text (20px)"), Theme::Styles::buttonTextConfig());
                        CLAY_TEXT(CLAY_STRING("Hover Text (20px)"), Theme::Styles::buttonHoverTextConfig());
                }

                // Column 3: Colors showcase
                CLAY(CLAY_ID("Col3"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_PERCENT(0.33f), CLAY_SIZING_GROW(0)},
                                .childGap = 8,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        }
                }) {
                        CLAY_TEXT(CLAY_STRING("Color Palette"), Theme::Styles::titleTextConfig());
                        // Show each named color as a swatch
                        CLAY(CLAY_ID("Sw1"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::contentBg}) {
                                CLAY_TEXT(CLAY_STRING("contentBg"), Theme::Styles::dimTextConfig());
                        }
                        CLAY(CLAY_ID("Sw2"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::titleBg}) {
                                CLAY_TEXT(CLAY_STRING("titleBg"), Theme::Styles::dimTextConfig());
                        }
                        CLAY(CLAY_ID("Sw3"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::buttonBg}) {
                                CLAY_TEXT(CLAY_STRING("buttonBg"), Theme::Styles::dimTextConfig());
                        }
                        CLAY(CLAY_ID("Sw4"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::buttonHoverBg}) {
                                CLAY_TEXT(CLAY_STRING("buttonHoverBg"), Theme::Styles::dimTextConfig());
                        }
                        CLAY(CLAY_ID("Sw5"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::accent}) {
                                CLAY_TEXT(CLAY_STRING("accent"), CLAY_TEXT_CONFIG({.textColor = {255,255,255,255}, .fontId = 0, .fontSize = 14}));
                        }
                        CLAY(CLAY_ID("Sw6"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::danger}) {
                                CLAY_TEXT(CLAY_STRING("danger"), CLAY_TEXT_CONFIG({.textColor = {255,255,255,255}, .fontId = 0, .fontSize = 14}));
                        }
                        CLAY(CLAY_ID("Sw7"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::success}) {
                                CLAY_TEXT(CLAY_STRING("success"), CLAY_TEXT_CONFIG({.textColor = {0,0,0,255}, .fontId = 0, .fontSize = 14}));
                        }
                        CLAY(CLAY_ID("Sw8"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::warning}) {
                                CLAY_TEXT(CLAY_STRING("warning"), CLAY_TEXT_CONFIG({.textColor = {0,0,0,255}, .fontId = 0, .fontSize = 14}));
                        }
                        CLAY(CLAY_ID("Sw9"), {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)}}, .backgroundColor = Theme::Colors::separator}) {
                                CLAY_TEXT(CLAY_STRING("separator"), Theme::Styles::dimTextConfig());
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// JSONL output (same as clay_test.c logRenderCommands)
// ---------------------------------------------------------------------------

static void logRenderCommands(const Clay_RenderCommandArray &commands, const char *layoutName, int frameNum)
{
        printf("{\"frame\":%d,\"layout\":\"%s\",\"commands\":[", frameNum, layoutName);

        for (int32_t i = 0; i < commands.length; i++) {
                const Clay_RenderCommand &cmd = commands.internalArray[i];
                auto &bb = cmd.boundingBox;

                if (i > 0) printf(",");

                printf("{\"type\":%d", static_cast<int>(cmd.commandType));
                printf(",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d",
                        static_cast<int>(bb.x), static_cast<int>(bb.y),
                        static_cast<int>(bb.width), static_cast<int>(bb.height));

                switch (cmd.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                        printf(",\"bg\":[%d,%d,%d,%d]",
                                static_cast<int>(cmd.renderData.rectangle.backgroundColor.r),
                                static_cast<int>(cmd.renderData.rectangle.backgroundColor.g),
                                static_cast<int>(cmd.renderData.rectangle.backgroundColor.b),
                                static_cast<int>(cmd.renderData.rectangle.backgroundColor.a));
                        break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                        auto &td = cmd.renderData.text;
                        std::string text(td.stringContents.chars, td.stringContents.length);
                        printf(",\"text\":\"");
                        writeJsonEscaped(text.c_str(), text.length());
                        printf("\"");
                        printf(",\"textColor\":[%d,%d,%d,%d]",
                                static_cast<int>(td.textColor.r), static_cast<int>(td.textColor.g),
                                static_cast<int>(td.textColor.b), static_cast<int>(td.textColor.a));
                        printf(",\"fontSize\":%d", static_cast<int>(td.fontSize));
                        printf(",\"fontId\":%d", static_cast<int>(td.fontId));
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                        auto &bd = cmd.renderData.border;
                        printf(",\"borderColor\":[%d,%d,%d,%d]",
                                static_cast<int>(bd.color.r), static_cast<int>(bd.color.g),
                                static_cast<int>(bd.color.b), static_cast<int>(bd.color.a));
                        printf(",\"borderWidth\":{\"l\":%d,\"r\":%d,\"t\":%d,\"b\":%d}",
                                bd.width.left, bd.width.right, bd.width.top, bd.width.bottom);
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                        printf(",\"hasImageData\":%s", cmd.renderData.image.imageData ? "true" : "false");
                        break;
                case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                        printf(",\"customBg\":[%d,%d,%d,%d]",
                                static_cast<int>(cmd.renderData.custom.backgroundColor.r),
                                static_cast<int>(cmd.renderData.custom.backgroundColor.g),
                                static_cast<int>(cmd.renderData.custom.backgroundColor.b),
                                static_cast<int>(cmd.renderData.custom.backgroundColor.a));
                        break;
                default:
                        break;
                }

                printf("}");
        }

        printf("]}\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void)
{
        // Initialize Clay
        uint32_t screenW = 1280, screenH = 720;

        Clay_SetMaxElementCount(Theme::Renderer::defaultMaxElements);
        uint32_t totalMemory = Clay_MinMemorySize();
        void *arenaMem = malloc(totalMemory);

        Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemory, arenaMem);
        Clay_Initialize(arena,
                Clay_Dimensions{(float)screenW, (float)screenH},
                (Clay_ErrorHandler){.errorHandlerFunction = errorHandler, .userData = nullptr});
        Clay_SetMeasureTextFunction(measureText, nullptr);

        // --- Test 1: Pause Menu (default theme) ---
        Clay_SetLayoutDimensions((Clay_Dimensions){(float)screenW, (float)screenH});
        Clay_BeginLayout();
        buildPauseMenuThemed();
        Clay_RenderCommandArray cmds1 = Clay_EndLayout(0);
        logRenderCommands(cmds1, "01_pause_menu_default", 0);

        // --- Test 2: Pause Menu (green custom theme) ---
        Clay_SetLayoutDimensions((Clay_Dimensions){(float)screenW, (float)screenH});
        Clay_BeginLayout();
        buildPauseMenuGreen();
        Clay_RenderCommandArray cmds2 = Clay_EndLayout(0);
        logRenderCommands(cmds2, "02_pause_menu_green", 1);

        // --- Test 3: Settings Dialog (themed) ---
        Clay_SetLayoutDimensions((Clay_Dimensions){(float)screenW, (float)screenH});
        Clay_BeginLayout();
        buildSettingsThemed();
        Clay_RenderCommandArray cmds3 = Clay_EndLayout(0);
        logRenderCommands(cmds3, "03_settings_themed", 2);

        // --- Test 4: HUD (themed) ---
        Clay_SetLayoutDimensions((Clay_Dimensions){(float)screenW, (float)screenH});
        Clay_BeginLayout();
        buildHUDThemed();
        Clay_RenderCommandArray cmds4 = Clay_EndLayout(0);
        logRenderCommands(cmds4, "04_hud_themed", 3);

        // --- Test 5: Component isolation test ---
        Clay_SetLayoutDimensions((Clay_Dimensions){(float)screenW, (float)screenH});
        Clay_BeginLayout();
        buildComponentTest();
        Clay_RenderCommandArray cmds5 = Clay_EndLayout(0);
        logRenderCommands(cmds5, "05_component_test", 4);

        free(arenaMem);

        fprintf(stderr, "\n=== Theme E2E Test Results ===\n");
        fprintf(stderr, "Test 1 (Pause Menu Default): %d commands\n", cmds1.length);
        fprintf(stderr, "Test 2 (Pause Menu Green):   %d commands\n", cmds2.length);
        fprintf(stderr, "Test 3 (Settings Themed):    %d commands\n", cmds3.length);
        fprintf(stderr, "Test 4 (HUD Themed):         %d commands\n", cmds4.length);
        fprintf(stderr, "Test 5 (Component Test):     %d commands\n", cmds5.length);
        fprintf(stderr, "All layouts rendered successfully.\n");

        return 0;
}
