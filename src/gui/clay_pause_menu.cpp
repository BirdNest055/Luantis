/*
 * Clay-based Pause Menu
 *
 * A modern pause menu built with Clay's declarative layout system.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

#include "clay_pause_menu.h"

#include "clay_renderer.h"
#include "log.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Style definitions
// ---------------------------------------------------------------------------

// Semi-transparent dark background that covers the whole screen
Clay_ElementDeclaration ClayPauseMenu::s_panel_style = {
        .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                .padding = {0, 0, 0, 0},
                .childGap = 0,
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = {0.0f, 0.0f, 0.0f, 160.0f} // dark semi-transparent
};

// Title style
Clay_ElementDeclaration ClayPauseMenu::s_title_style = {
        .layout = {
                .sizing = {CLAY_SIZING_FIXED(400), CLAY_SIZING_FIXED(60)},
                .padding = {16, 16, 16, 16},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = {30.0f, 30.0f, 40.0f, 255.0f},
        .cornerRadius = {12, 12, 0, 0}
};

// Button style
Clay_ElementDeclaration ClayPauseMenu::s_button_style = {
        .layout = {
                .sizing = {CLAY_SIZING_FIXED(400), CLAY_SIZING_FIXED(50)},
                .padding = {16, 8, 16, 8},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
        },
        .backgroundColor = {50.0f, 50.0f, 60.0f, 255.0f},
        .cornerRadius = {0, 0, 0, 0}
};

// ---------------------------------------------------------------------------
// Button click handler (Clay callback signature)
// ---------------------------------------------------------------------------

void ClayPauseMenu::onButtonClicked(Clay_ElementId elementId,
        Clay_PointerData pointerInfo, void *userData)
{
        if (pointerInfo.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
                return;

        auto *action = static_cast<ButtonAction *>(userData);
        if (action && action->action) {
                action->action();
        }
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ClayPauseMenu::ClayPauseMenu(ClayPauseMenuCallbacks callbacks)
        : ClayUIPanel("pause_menu"), m_callbacks(std::move(callbacks))
{
}

// ---------------------------------------------------------------------------
// Show / Hide
// ---------------------------------------------------------------------------

void ClayPauseMenu::onShow()
{
        infostream << "ClayPauseMenu: shown" << std::endl;
}

void ClayPauseMenu::onHide()
{
        infostream << "ClayPauseMenu: hidden" << std::endl;
}

// ---------------------------------------------------------------------------
// Button component
// ---------------------------------------------------------------------------

void ClayPauseMenu::buttonComponent(const char *id, const char *label,
        std::function<void()> onClick)
{
        // Store the action so it survives the frame
        m_button_actions.push_back({.action = std::move(onClick)});
        ButtonAction *actionPtr = &m_button_actions.back();

        // Use Clay_GetElementId for dynamic (non-literal) string IDs
        Clay_ElementId clayId = Clay_GetElementId(Clay_String{.isStaticallyAllocated = false, .length = static_cast<int32_t>(strlen(id)), .chars = id});

        // Check hover state for this element ID
        bool hovered = Clay_PointerOver(clayId);

        Clay_ElementDeclaration btnStyle = s_button_style;
        if (hovered) {
                btnStyle.backgroundColor = {80.0f, 80.0f, 100.0f, 255.0f};
        }

        CLAY(clayId, btnStyle) {
                Clay_OnHover(onButtonClicked, actionPtr);

                Clay_Color textColor = hovered
                        ? Clay_Color{255.0f, 255.0f, 200.0f, 255.0f}
                        : Clay_Color{220.0f, 220.0f, 220.0f, 255.0f};

                // Construct Clay_String for dynamic label text
                Clay_String labelStr = {.isStaticallyAllocated = false,
                        .length = static_cast<int32_t>(strlen(label)),
                        .chars = label};

                // Clay_TextElementConfig field order: userData, textColor, fontId, fontSize,
                // letterSpacing, lineHeight, wrapMode
                CLAY_TEXT(labelStr,
                        CLAY_TEXT_CONFIG({
                                .textColor = textColor,
                                .fontId = 0,
                                .fontSize = 20,
                        }));
        }
}

// ---------------------------------------------------------------------------
// Build layout — called every frame while visible
// ---------------------------------------------------------------------------

void ClayPauseMenu::buildLayout()
{
        // Clear per-frame action storage and reserve to prevent reallocation
        // (which would invalidate userData pointers stored in Clay_OnHover)
        m_button_actions.clear();
        m_button_actions.reserve(8);

        // Full-screen dark overlay with centered content
        CLAY(CLAY_ID("PauseOverlay"), s_panel_style) {

                // Content column
                CLAY(CLAY_ID("PauseContent"), {
                        .layout = {
                                .sizing = {CLAY_SIZING_FIXED(400), CLAY_SIZING_FIT(0)},
                                .padding = {0, 0, 0, 0},
                                .childGap = 2,
                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = {40.0f, 40.0f, 50.0f, 255.0f},
                        .cornerRadius = {12, 12, 12, 12}
                }) {

                        // Title bar
                        CLAY(CLAY_ID("PauseTitle"), s_title_style) {
                                CLAY_TEXT(CLAY_STRING("Game Paused"),
                                        CLAY_TEXT_CONFIG({
                                                .textColor = {255.0f, 255.0f, 255.0f, 255.0f},
                                                .fontId = 0,
                                                .fontSize = 28,
                                        }));
                        }

                        // Buttons
                        buttonComponent("BtnResume", "Resume Game", m_callbacks.on_resume);
                        buttonComponent("BtnSettings", "Settings", m_callbacks.on_settings);
                        buttonComponent("BtnChangePassword", "Change Password", m_callbacks.on_change_password);
                        buttonComponent("BtnVolume", "Volume", m_callbacks.on_volume);

                        // Separator
                        CLAY(CLAY_ID("PauseSep"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(2)},
                                        .padding = {0, 0, 0, 0},
                                },
                                .backgroundColor = {100.0f, 100.0f, 120.0f, 255.0f}
                        }) {}

                        buttonComponent("BtnExitMenu", "Exit to Menu", m_callbacks.on_exit_to_menu);
                        buttonComponent("BtnExitOS", "Exit to OS", m_callbacks.on_exit_to_os);

                        // Bottom rounded corner padding
                        CLAY(CLAY_ID("PauseBottomPad"), {
                                .layout = {
                                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(16)},
                                        .padding = {0, 0, 0, 0},
                                },
                                .backgroundColor = {40.0f, 40.0f, 50.0f, 255.0f},
                                .cornerRadius = {0, 0, 12, 12}
                        }) {}
                }
        }
}
