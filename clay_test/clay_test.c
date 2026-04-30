/*
 * Standalone Clay UI Test Program
 *
 * Generates Clay render commands for multiple UI layouts and outputs
 * them as JSONL to stdout for visual verification via the Python
 * renderer (render_clay_log.py).
 *
 * Layouts tested:
 *   1. Pause Menu — centered modal with buttons
 *   2. Settings Dialog — tabbed settings panel
 *   3. HUD Overlay — in-game heads-up display
 *   4. Chat Console — scrollable text area
 *   5. Inventory Grid — grid-based inventory
 *
 * Build:
 *   cc -O2 -std=c11 -o clay_test clay_test.c -lm
 *
 * Run:
 *   ./clay_test > /tmp/clay_render_log.jsonl
 *   python3 render_clay_log.py /tmp/clay_render_log.jsonl /tmp/clay_screenshots
 */

#define CLAY_IMPLEMENTATION
#include "../lib/clay/clay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
// Clay text measurement (monospace approximation)
// ---------------------------------------------------------------------------

static Clay_Dimensions measureText(Clay_StringSlice text,
        Clay_TextElementConfig *config, void *userData)
{
        (void)userData;
        float width = (float)text.length * (float)config->fontSize * 0.6f;
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
// Layout builders
// ---------------------------------------------------------------------------

static void buildPauseMenu(void)
{
        /* Full-screen dark overlay */
        CLAY(CLAY_ID("PauseOverlay"), {
                .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = { 0, 0, 0, 160 }
        }) {
                /* Content card */
                CLAY(CLAY_ID("PauseCard"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_FIXED(400), CLAY_SIZING_FIT(0) },
                                .padding = { 0, 0, 0, 0 },
                                .childGap = 2,
                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = { 40, 40, 50, 255 },
                        .cornerRadius = { 12, 12, 12, 12 }
                }) {
                        /* Title */
                        CLAY(CLAY_ID("PauseTitle"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(60) },
                                        .padding = { 16, 16, 16, 16 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 30, 30, 40, 255 },
                                .cornerRadius = { 12, 12, 0, 0 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Game Paused"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontId = 0, .fontSize = 28 }));
                        }

                        /* Buttons — use CLAY_ID for static literal strings */
                        CLAY(CLAY_ID("BtnResume"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(50) },
                                        .padding = { 16, 8, 16, 8 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Resume Game"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 20 }));
                        }
                        CLAY(CLAY_ID("BtnSettings"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(50) },
                                        .padding = { 16, 8, 16, 8 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Settings"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 20 }));
                        }
                        CLAY(CLAY_ID("BtnPassword"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(50) },
                                        .padding = { 16, 8, 16, 8 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Change Password"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 20 }));
                        }
                        CLAY(CLAY_ID("BtnVolume"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(50) },
                                        .padding = { 16, 8, 16, 8 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Volume Control"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 20 }));
                        }

                        /* Separator */
                        CLAY(CLAY_ID("PauseSep"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(2) } },
                                .backgroundColor = { 100, 100, 120, 255 }
                        }) {}

                        /* Exit buttons */
                        CLAY(CLAY_ID("BtnExitMenu"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(50) },
                                        .padding = { 16, 8, 16, 8 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Exit to Menu"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 20 }));
                        }
                        CLAY(CLAY_ID("BtnExitOS"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(50) },
                                        .padding = { 16, 8, 16, 8 },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Exit to OS"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 20 }));
                        }

                        /* Bottom padding */
                        CLAY(CLAY_ID("PauseBottom"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(16) } },
                                .backgroundColor = { 40, 40, 50, 255 },
                                .cornerRadius = { 0, 0, 12, 12 }
                        }) {}
                }
        }
}

static void buildSettingsDialog(void)
{
        CLAY(CLAY_ID("SettingsOverlay"), {
                .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = { 0, 0, 0, 140 }
        }) {
                CLAY(CLAY_ID("SettingsCard"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_FIXED(600), CLAY_SIZING_FIXED(450) },
                                .padding = { 16, 16, 16, 16 },
                                .childGap = 12,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = { 35, 35, 45, 255 },
                        .cornerRadius = { 12, 12, 12, 12 }
                }) {
                        /* Title bar */
                        CLAY(CLAY_ID("SettingsTitle"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40) },
                                        .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 25, 25, 35, 255 },
                                .cornerRadius = { 8, 8, 0, 0 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Settings"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontId = 0, .fontSize = 24 }));
                        }

                        /* Tab bar */
                        CLAY(CLAY_ID("TabBar"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) },
                                        .childGap = 4,
                                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                }
                        }) {
                                /* Active tab */
                                CLAY(CLAY_ID("TabGraphics"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_PERCENT(0.25f), CLAY_SIZING_GROW(0) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 60, 60, 80, 255 },
                                        .cornerRadius = { 6, 6, 0, 0 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Graphics"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 200, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                                CLAY(CLAY_ID("TabSound"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_PERCENT(0.25f), CLAY_SIZING_GROW(0) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 45, 45, 55, 255 },
                                        .cornerRadius = { 6, 6, 0, 0 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Sound"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                                CLAY(CLAY_ID("TabControls"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_PERCENT(0.25f), CLAY_SIZING_GROW(0) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 45, 45, 55, 255 },
                                        .cornerRadius = { 6, 6, 0, 0 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Controls"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                                CLAY(CLAY_ID("TabPrivacy"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_PERCENT(0.25f), CLAY_SIZING_GROW(0) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 45, 45, 55, 255 },
                                        .cornerRadius = { 6, 6, 0, 0 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Privacy"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                        }

                        /* Content area with setting rows */
                        CLAY(CLAY_ID("SettingsContent"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                                        .padding = { 12, 12, 12, 12 },
                                        .childGap = 8,
                                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                },
                                .backgroundColor = { 50, 50, 60, 255 }
                        }) {
                                /* Row 1: Render Distance */
                                CLAY(CLAY_ID("SettingRow1"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) },
                                                .padding = { 8, 4, 8, 4 },
                                                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                        },
                                        .backgroundColor = { 55, 55, 65, 255 },
                                        .cornerRadius = { 4, 4, 4, 4 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Render Distance"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 200, 200, 200, 255 }, .fontId = 0, .fontSize = 16 }));
                                        CLAY(CLAY_ID("Spacer1"), {
                                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                                        }) {}
                                        CLAY(CLAY_ID("Toggle1"), {
                                                .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(24) } },
                                                .backgroundColor = { 80, 160, 80, 255 },
                                                .cornerRadius = { 12, 12, 12, 12 }
                                        }) {}
                                }
                                /* Row 2: Smooth Lighting */
                                CLAY(CLAY_ID("SettingRow2"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) },
                                                .padding = { 8, 4, 8, 4 },
                                                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                        },
                                        .backgroundColor = { 55, 55, 65, 255 },
                                        .cornerRadius = { 4, 4, 4, 4 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Smooth Lighting"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 200, 200, 200, 255 }, .fontId = 0, .fontSize = 16 }));
                                        CLAY(CLAY_ID("Spacer2"), {
                                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                                        }) {}
                                        CLAY(CLAY_ID("Toggle2"), {
                                                .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(24) } },
                                                .backgroundColor = { 80, 80, 80, 255 },
                                                .cornerRadius = { 12, 12, 12, 12 }
                                        }) {}
                                }
                                /* Row 3: Fancy Leaves */
                                CLAY(CLAY_ID("SettingRow3"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) },
                                                .padding = { 8, 4, 8, 4 },
                                                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                        },
                                        .backgroundColor = { 55, 55, 65, 255 },
                                        .cornerRadius = { 4, 4, 4, 4 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Fancy Leaves"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 200, 200, 200, 255 }, .fontId = 0, .fontSize = 16 }));
                                        CLAY(CLAY_ID("Spacer3"), {
                                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                                        }) {}
                                        CLAY(CLAY_ID("Toggle3"), {
                                                .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(24) } },
                                                .backgroundColor = { 80, 160, 80, 255 },
                                                .cornerRadius = { 12, 12, 12, 12 }
                                        }) {}
                                }
                                /* Row 4: VSync */
                                CLAY(CLAY_ID("SettingRow4"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) },
                                                .padding = { 8, 4, 8, 4 },
                                                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                        },
                                        .backgroundColor = { 55, 55, 65, 255 },
                                        .cornerRadius = { 4, 4, 4, 4 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("VSync"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 200, 200, 200, 255 }, .fontId = 0, .fontSize = 16 }));
                                        CLAY(CLAY_ID("Spacer4"), {
                                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                                        }) {}
                                        CLAY(CLAY_ID("Toggle4"), {
                                                .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(24) } },
                                                .backgroundColor = { 80, 80, 80, 255 },
                                                .cornerRadius = { 12, 12, 12, 12 }
                                        }) {}
                                }
                        }

                        /* Button bar */
                        CLAY(CLAY_ID("SettingsButtons"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                                        .childGap = 8,
                                        .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
                                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                }
                        }) {
                                CLAY(CLAY_ID("BtnCancel"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(34) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 70, 70, 80, 255 },
                                        .cornerRadius = { 6, 6, 6, 6 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Cancel"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                                CLAY(CLAY_ID("BtnApply"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(34) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 60, 100, 60, 255 },
                                        .cornerRadius = { 6, 6, 6, 6 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("Apply"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                                CLAY(CLAY_ID("BtnOK"), {
                                        .layout = {
                                                .sizing = { CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(34) },
                                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                        },
                                        .backgroundColor = { 60, 80, 140, 255 },
                                        .cornerRadius = { 6, 6, 6, 6 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("OK"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 220, 220, 220, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                        }
                }
        }
}

static void buildHUD(void)
{
        /* Full-screen container (transparent background) */
        CLAY(CLAY_ID("HUDRoot"), {
                .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                }
        }) {
                /* Top bar: health, armor, breath */
                CLAY(CLAY_ID("HUDTopBar"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40) },
                                .padding = { 8, 4, 8, 4 },
                                .childGap = 8,
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        }
                }) {
                        /* Health bar */
                        CLAY(CLAY_ID("HealthBar"), {
                                .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0) } },
                                .backgroundColor = { 40, 40, 40, 180 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY(CLAY_ID("HealthFill"), {
                                        .layout = { .sizing = { CLAY_SIZING_PERCENT(0.85f), CLAY_SIZING_GROW(0) } },
                                        .backgroundColor = { 200, 40, 40, 255 },
                                        .cornerRadius = { 4, 4, 4, 4 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("17/20"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                        }
                        /* Armor bar */
                        CLAY(CLAY_ID("ArmorBar"), {
                                .layout = { .sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_GROW(0) } },
                                .backgroundColor = { 40, 40, 40, 180 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY(CLAY_ID("ArmorFill"), {
                                        .layout = { .sizing = { CLAY_SIZING_PERCENT(0.6f), CLAY_SIZING_GROW(0) } },
                                        .backgroundColor = { 60, 60, 200, 255 },
                                        .cornerRadius = { 4, 4, 4, 4 }
                                }) {
                                        CLAY_TEXT(CLAY_STRING("12/20"),
                                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontId = 0, .fontSize = 14 }));
                                }
                        }
                        /* Spacer */
                        CLAY(CLAY_ID("TopSpacer"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                        }) {}
                        /* Minimap placeholder */
                        CLAY(CLAY_ID("Minimap"), {
                                .layout = { .sizing = { CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(80) } },
                                .backgroundColor = { 30, 60, 30, 200 },
                                .cornerRadius = { 40, 40, 40, 40 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Map"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 200, 200, 200, 200 }, .fontId = 0, .fontSize = 12 }));
                        }
                }

                /* Center area: spacer */
                CLAY(CLAY_ID("HUDCenter"), {
                        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                }) {}

                /* Bottom bar: hotbar */
                CLAY(CLAY_ID("HUDBottom"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(60) },
                                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        }
                }) {
                        /* Hotbar slots — explicit IDs for each */
                        CLAY(CLAY_ID("Hotbar0"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 80, 80, 100, 255 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("1"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG0"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar1"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("2"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG1"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar2"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("3"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG2"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar3"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("4"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG3"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar4"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("5"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG4"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar5"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("6"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG5"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar6"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("7"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG6"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar7"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("8"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                        CLAY(CLAY_ID("HG7"), { .layout = { .sizing = { CLAY_SIZING_FIXED(4), CLAY_SIZING_FIXED(48) } } }) {}
                        CLAY(CLAY_ID("Hotbar8"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 40, 40, 50, 220 },
                                .cornerRadius = { 4, 4, 4, 4 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("9"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 180, 200 }, .fontId = 0, .fontSize = 16 }));
                        }
                }
        }
}

static void buildChatConsole(void)
{
        CLAY(CLAY_ID("ChatRoot"), {
                .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM },
                }
        }) {
                /* Chat messages area */
                CLAY(CLAY_ID("ChatMessages"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_FIXED(400), CLAY_SIZING_FIXED(200) },
                                .padding = { 8, 8, 8, 8 },
                                .childGap = 2,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = { 0, 0, 0, 120 }
                }) {
                        CLAY_TEXT(CLAY_STRING("[Admin] Welcome to the server!"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 200, 50, 255 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[Player1] Hello everyone!"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 230 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[Player2] Anyone found diamonds?"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 230 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[Player1] I found some at y=-59"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 230 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[System] Player3 joined the game"),
                                CLAY_TEXT_CONFIG({ .textColor = { 200, 200, 200, 255 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[Player2] Nice! What coords?"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 230 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[Player1] Sorry, secret base :P"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 230 }, .fontId = 0, .fontSize = 13 }));
                        CLAY_TEXT(CLAY_STRING("[Admin] Remember: no griefing!"),
                                CLAY_TEXT_CONFIG({ .textColor = { 255, 200, 50, 255 }, .fontId = 0, .fontSize = 13 }));
                }
                /* Chat input */
                CLAY(CLAY_ID("ChatInput"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_FIXED(400), CLAY_SIZING_FIXED(30) },
                                .padding = { 8, 4, 8, 4 },
                                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = { 20, 20, 30, 220 },
                        .cornerRadius = { 0, 0, 4, 4 }
                }) {
                        CLAY_TEXT(CLAY_STRING("Type a message..."),
                                CLAY_TEXT_CONFIG({ .textColor = { 150, 150, 150, 255 }, .fontId = 0, .fontSize = 14 }));
                }
        }
}

static void buildInventoryGrid(void)
{
        CLAY(CLAY_ID("InvOverlay"), {
                .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = { 0, 0, 0, 160 }
        }) {
                CLAY(CLAY_ID("InvCard"), {
                        .layout = {
                                .sizing = { CLAY_SIZING_FIXED(440), CLAY_SIZING_FIT(0) },
                                .padding = { 12, 12, 12, 12 },
                                .childGap = 8,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = { 50, 50, 60, 240 },
                        .cornerRadius = { 8, 8, 8, 8 }
                }) {
                        /* Title */
                        CLAY(CLAY_ID("InvTitle"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                                .backgroundColor = { 30, 30, 40, 255 }
                        }) {
                                CLAY_TEXT(CLAY_STRING("Inventory"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontId = 0, .fontSize = 20 }));
                        }

                        /* Inventory grid rows - expanded inline */
                        /* Row 1 */
                        CLAY(CLAY_ID("InvR0"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                                        .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT }
                        }) {
                                CLAY(CLAY_ID("IS0"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = { 80, 120, 80, 255 }, .cornerRadius = { 3,3,3,3 } }) { CLAY_TEXT(CLAY_STRING("Item1"), CLAY_TEXT_CONFIG({ .textColor = { 200,200,200,255 }, .fontId = 0, .fontSize = 10 })); }
                                CLAY(CLAY_ID("IS1"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = { 60, 50, 40, 255 }, .cornerRadius = { 3,3,3,3 } }) { CLAY_TEXT(CLAY_STRING("Item2"), CLAY_TEXT_CONFIG({ .textColor = { 200,200,200,255 }, .fontId = 0, .fontSize = 10 })); }
                                CLAY(CLAY_ID("IS2"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = { 60, 50, 40, 255 }, .cornerRadius = { 3,3,3,3 } }) { CLAY_TEXT(CLAY_STRING("Item3"), CLAY_TEXT_CONFIG({ .textColor = { 200,200,200,255 }, .fontId = 0, .fontSize = 10 })); }
                                CLAY(CLAY_ID("IS3"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = { 60, 50, 40, 255 }, .cornerRadius = { 3,3,3,3 } }) { CLAY_TEXT(CLAY_STRING("Item4"), CLAY_TEXT_CONFIG({ .textColor = { 200,200,200,255 }, .fontId = 0, .fontSize = 10 })); }
                                CLAY(CLAY_ID("IS4"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = { 60, 50, 40, 255 }, .cornerRadius = { 3,3,3,3 } }) { CLAY_TEXT(CLAY_STRING("Item5"), CLAY_TEXT_CONFIG({ .textColor = { 200,200,200,255 }, .fontId = 0, .fontSize = 10 })); }
                                CLAY(CLAY_ID("IS5"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS6"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS7"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                        }
                        /* Row 2 */
                        CLAY(CLAY_ID("InvR1"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                                        .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT }
                        }) {
                                CLAY(CLAY_ID("IS8"),  { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS9"),  { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS10"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS11"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS12"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS13"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS14"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS15"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                        }
                        /* Row 3 */
                        CLAY(CLAY_ID("InvR2"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                                        .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT }
                        }) {
                                CLAY(CLAY_ID("IS16"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS17"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS18"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS19"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS20"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS21"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS22"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS23"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                        }
                        /* Row 4 */
                        CLAY(CLAY_ID("InvR3"), {
                                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                                        .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT }
                        }) {
                                CLAY(CLAY_ID("IS24"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS25"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS26"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS27"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS28"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS29"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS30"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                CLAY(CLAY_ID("IS31"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(40) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                        }

                        /* Crafting section */
                        CLAY(CLAY_ID("CraftingTitle"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28) },
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                                },
                        }) {
                                CLAY_TEXT(CLAY_STRING("Crafting"),
                                        CLAY_TEXT_CONFIG({ .textColor = { 180, 180, 200, 255 }, .fontId = 0, .fontSize = 14 }));
                        }

                        /* 3x3 crafting grid */
                        CLAY(CLAY_ID("CraftGrid"), {
                                .layout = {
                                        .sizing = { CLAY_SIZING_FIXED(156), CLAY_SIZING_FIXED(156) },
                                        .childGap = 4,
                                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
                                }
                        }) {
                                CLAY(CLAY_ID("CR0"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(48) }, .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
                                        CLAY(CLAY_ID("CS00"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                        CLAY(CLAY_ID("CS01"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                        CLAY(CLAY_ID("CS02"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                }
                                CLAY(CLAY_ID("CR1"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(48) }, .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
                                        CLAY(CLAY_ID("CS10"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                        CLAY(CLAY_ID("CS11"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                        CLAY(CLAY_ID("CS12"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                }
                                CLAY(CLAY_ID("CR2"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(48) }, .childGap = 4, .layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
                                        CLAY(CLAY_ID("CS20"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                        CLAY(CLAY_ID("CS21"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                        CLAY(CLAY_ID("CS22"), { .layout = { .sizing = { CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48) } }, .backgroundColor = { 35, 35, 45, 255 }, .cornerRadius = { 3,3,3,3 } }) {}
                                }
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// JSONL output
// ---------------------------------------------------------------------------

static const char *commandTypeName(Clay_RenderCommandType t)
{
        switch (t) {
        case CLAY_RENDER_COMMAND_TYPE_NONE:                    return "none";
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:               return "rectangle";
        case CLAY_RENDER_COMMAND_TYPE_TEXT:                    return "text";
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:                   return "image";
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:           return "scissor_start";
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:             return "scissor_end";
        case CLAY_RENDER_COMMAND_TYPE_BORDER:                  return "border";
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:                  return "custom";
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:     return "overlay_start";
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:       return "overlay_end";
        default: return "unknown";
        }
}

static void outputRenderCommandsJSONL(const Clay_RenderCommandArray *cmds, const char *layoutName, int frameNum)
{
        printf("{\"layout\":\"%s\",\"frame\":%d,\"commands\":[", layoutName, frameNum);

        for (int32_t i = 0; i < cmds->length; i++) {
                const Clay_RenderCommand *cmd = &cmds->internalArray[i];
                Clay_BoundingBox bb = cmd->boundingBox;

                if (i > 0) printf(",");

                printf("{\"type\":\"%s\"", commandTypeName(cmd->commandType));
                printf(",\"x\":%.0f", bb.x);
                printf(",\"y\":%.0f", bb.y);
                printf(",\"w\":%.0f", bb.width);
                printf(",\"h\":%.0f", bb.height);

                switch (cmd->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                        Clay_Color c = cmd->renderData.rectangle.backgroundColor;
                        printf(",\"bg\":[%.0f,%.0f,%.0f,%.0f]", c.r, c.g, c.b, c.a);
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                        Clay_TextRenderData td = cmd->renderData.text;
                        printf(",\"text\":\"");
                        writeJsonEscaped(td.stringContents.chars, td.stringContents.length);
                        printf("\"");
                        Clay_Color tc = td.textColor;
                        printf(",\"textColor\":[%.0f,%.0f,%.0f,%.0f]", tc.r, tc.g, tc.b, tc.a);
                        printf(",\"fontSize\":%d", td.fontSize);
                        printf(",\"fontId\":%d", td.fontId);
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                        Clay_BorderRenderData bd = cmd->renderData.border;
                        printf(",\"borderColor\":[%.0f,%.0f,%.0f,%.0f]", bd.color.r, bd.color.g, bd.color.b, bd.color.a);
                        printf(",\"borderWidth\":{\"l\":%d,\"r\":%d,\"t\":%d,\"b\":%d}",
                                bd.width.left, bd.width.right, bd.width.top, bd.width.bottom);
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                        printf(",\"hasImageData\":%s", cmd->renderData.image.imageData ? "true" : "false");
                        break;
                case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
                        Clay_Color c = cmd->renderData.custom.backgroundColor;
                        printf(",\"customBg\":[%.0f,%.0f,%.0f,%.0f]", c.r, c.g, c.b, c.a);
                        break;
                }
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
        /* Initialize Clay with 1280x720 viewport */
        int screenW = 1280;
        int screenH = 720;

        Clay_SetMaxElementCount(8192);
        uint32_t memSize = Clay_MinMemorySize();
        void *mem = malloc(memSize);
        if (!mem) {
                fprintf(stderr, "Failed to allocate %u bytes for Clay arena\n", memSize);
                return 1;
        }

        Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memSize, mem);
        Clay_Initialize(arena,
                (Clay_Dimensions){ (float)screenW, (float)screenH },
                (Clay_ErrorHandler){ .errorHandlerFunction = errorHandler, .userData = NULL });

        Clay_SetMeasureTextFunction(measureText, NULL);

        /* Define layout tests */
        typedef void (*LayoutBuilder)(void);
        struct { const char *name; LayoutBuilder builder; } layouts[] = {
                { "pause_menu",      buildPauseMenu },
                { "settings_dialog", buildSettingsDialog },
                { "hud_overlay",     buildHUD },
                { "chat_console",    buildChatConsole },
                { "inventory_grid",  buildInventoryGrid },
        };

        int numLayouts = sizeof(layouts) / sizeof(layouts[0]);

        for (int i = 0; i < numLayouts; i++) {
                /* Reset pointer state */
                Clay_SetPointerState((Clay_Vector2){ -1, -1 }, false);

                /* Begin layout */
                Clay_BeginLayout();
                layouts[i].builder();
                Clay_RenderCommandArray cmds = Clay_EndLayout(0.016f);

                /* Output as JSONL */
                outputRenderCommandsJSONL(&cmds, layouts[i].name, 0);

                fprintf(stderr, "Layout '%s': %d render commands\n",
                        layouts[i].name, cmds.length);
        }

        free(mem);
        fprintf(stderr, "All layouts rendered successfully.\n");
        return 0;
}
