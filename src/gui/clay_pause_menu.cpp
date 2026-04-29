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
		.sizing = {{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
		.padding = {0, 0, 0, 0},
		.childGap = 0,
		.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
		.layoutDirection = CLAY_TOP_TO_BOTTOM,
	},
	.backgroundColor = {0, 0, 0, 160} // dark semi-transparent
};

// Title style
Clay_ElementDeclaration ClayPauseMenu::s_title_style = {
	.layout = {
		.sizing = {{.width = CLAY_SIZING_FIXED(400), .height = CLAY_SIZING_FIXED(60)}},
		.padding = {16, 16, 16, 16},
		.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
	},
	.backgroundColor = {30, 30, 40, 255},
	.cornerRadius = {12, 12, 0, 0}
};

// Button style
Clay_ElementDeclaration ClayPauseMenu::s_button_style = {
	.layout = {
		.sizing = {{.width = CLAY_SIZING_FIXED(400), .height = CLAY_SIZING_FIXED(50)}},
		.padding = {16, 8, 16, 8},
		.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
	},
	.backgroundColor = {50, 50, 60, 255},
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

	// Determine hover state for visual feedback
	bool hovered = Clay_Hovered();

	Clay_ElementDeclaration btnStyle = s_button_style;
	if (hovered) {
		btnStyle.backgroundColor = {80, 80, 100, 255};
	}

	CLAY(CLAY_ID(id), btnStyle) {
		Clay_OnHover(onButtonClicked, actionPtr);

		Clay_Color textColor = hovered
			? Clay_Color{255, 255, 200, 255}
			: Clay_Color{220, 220, 220, 255};

		CLAY_TEXT(CLAY_STRING(label),
			Clay_TextElementConfig{
				.fontSize = 20,
				.textColor = textColor,
				.fontId = 1, // bold
			});
	}
}

// ---------------------------------------------------------------------------
// Build layout — called every frame while visible
// ---------------------------------------------------------------------------

void ClayPauseMenu::buildLayout()
{
	// Clear per-frame action storage
	m_button_actions.clear();

	// Full-screen dark overlay with centered content
	CLAY(CLAY_ID("PauseOverlay"), s_panel_style) {

		// Content column
		CLAY(CLAY_ID("PauseContent"), {
			.layout = {
				.sizing = {{.width = CLAY_SIZING_FIXED(400), .height = CLAY_SIZING_FIT(0)}},
				.padding = {0, 0, 0, 0},
				.childGap = 2,
				.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			},
			.backgroundColor = {40, 40, 50, 255},
			.cornerRadius = {12, 12, 12, 12}
		}) {

			// Title bar
			CLAY(CLAY_ID("PauseTitle"), s_title_style) {
				CLAY_TEXT(CLAY_STRING("Game Paused"),
					Clay_TextElementConfig{
						.fontSize = 28,
						.textColor = {255, 255, 255, 255},
						.fontId = 1, // bold
					});
			}

			// Buttons
			buttonComponent("BtnResume", "Resume Game", m_callbacks.on_resume);
			buttonComponent("BtnSettings", "Settings", m_callbacks.on_settings);
			buttonComponent("BtnChangePassword", "Change Password", m_callbacks.on_change_password);
			buttonComponent("BtnVolume", "Volume", m_callbacks.on_volume);

			// Separator
			CLAY(CLAY_ID("PauseSep"), {
				.layout = {
					.sizing = {{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(2)}},
					.padding = {0, 0, 0, 0},
				},
				.backgroundColor = {100, 100, 120, 255}
			}) {}

			buttonComponent("BtnExitMenu", "Exit to Menu", m_callbacks.on_exit_to_menu);
			buttonComponent("BtnExitOS", "Exit to OS", m_callbacks.on_exit_to_os);

			// Bottom rounded corner padding
			CLAY(CLAY_ID("PauseBottomPad"), {
				.layout = {
					.sizing = {{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(16)}},
					.padding = {0, 0, 0, 0},
				},
				.backgroundColor = {40, 40, 50, 255},
				.cornerRadius = {0, 0, 12, 12}
			}) {}
		}
	}
}
