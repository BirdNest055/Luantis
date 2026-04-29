/*
 * Clay-based Pause Menu (Responsive)
 *
 * A modern, responsive pause menu built with Clay's declarative layout
 * system. Uses clay_responsive.h utilities for breakpoint detection,
 * font/padding scaling, and adaptive sizing.
 *
 * Part of the Luantis Clay GUI integration (v9.47).
 */

#include "clay_pause_menu.h"

#include "clay_renderer.h"
#include "log.h"
#include <cstring>

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
// Responsive button component
// ---------------------------------------------------------------------------

void ClayPauseMenu::buttonComponent(const char *id, const char *label,
	std::function<void()> onClick)
{
	// Store the action so it survives the frame
	m_button_actions.push_back({.action = std::move(onClick)});
	ButtonAction *actionPtr = &m_button_actions.back();

	// Get responsive parameters
	auto bp = clayGetBreakpoint();
	float btnMinH = (bp == ClayBreakpoint::Mobile) ? 44.0f : 50.0f;
	uint16_t padH = static_cast<uint16_t>(clayScalePadding(16));
	uint16_t padV = static_cast<uint16_t>(clayScalePadding(8));
	uint16_t btnFont = clayScaleFontSize(20);

	Clay_ElementId clayId = Clay_GetElementId(Clay_String{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(strlen(id)),
		.chars = id});

	// Check hover state for this element ID
	bool hovered = Clay_PointerOver(clayId);

	// Button background color (lighter on hover)
	Clay_Color btnBg = hovered
		? Clay_Color{80, 80, 100, 255}
		: Clay_Color{50, 50, 60, 255};

	CLAY(clayId, {
		.layout = {
			.sizing = {clayButtonWidth(), clayButtonHeight(btnMinH)},
			.padding = {padH, padH, padV, padV},
			.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
		},
		.backgroundColor = btnBg
	}) {
		Clay_OnHover(onButtonClicked, actionPtr);

		Clay_Color textColor = hovered
			? Clay_Color{255, 255, 200, 255}
			: Clay_Color{220, 220, 220, 255};

		Clay_String labelStr = {.isStaticallyAllocated = false,
			.length = static_cast<int32_t>(strlen(label)),
			.chars = label};

		CLAY_TEXT(labelStr, CLAY_TEXT_CONFIG({
			.textColor = textColor,
			.fontId = 0,
			.fontSize = btnFont,
		}));
	}
}

// ---------------------------------------------------------------------------
// Build layout — called every frame while visible
// ---------------------------------------------------------------------------

void ClayPauseMenu::buildLayout()
{
	// Clear per-frame action storage
	m_button_actions.clear();
	m_button_actions.reserve(8);

	// Get responsive parameters for this frame's viewport
	auto bp = clayGetBreakpoint();
	Clay_Dimensions viewport = clayGetViewport();

	// Content column sizing — adapts to viewport
	float contentMaxW = (bp == ClayBreakpoint::Mobile)
		? std::min(400.0f, viewport.width * 0.92f) : 400.0f;
	float contentMinW = (bp == ClayBreakpoint::Mobile)
		? std::min(200.0f, viewport.width * 0.90f) : 280.0f;

	uint16_t titleFont = clayScaleFontSize(28);
	uint16_t padH = static_cast<uint16_t>(clayScalePadding(16));
	uint16_t padV = static_cast<uint16_t>(clayScalePadding(8));
	uint16_t childGap = static_cast<uint16_t>(clayScaleGap(2));
	uint16_t cr = static_cast<uint16_t>(clayScaleCornerRadius(12));

	// Full-screen dark overlay with centered content
	CLAY(CLAY_ID("PauseOverlay"), {
		.layout = {
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
			.childGap = 0,
			.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = {0, 0, 0, 160}
	}) {
		// Content column — responsive width
		CLAY(CLAY_ID("PauseContent"), {
			.layout = {
				.sizing = {CLAY_SIZING_GROW(contentMinW, contentMaxW),
				           CLAY_SIZING_FIT(0)},
				.childGap = childGap,
				.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			},
			.backgroundColor = {40, 40, 50, 255},
			.cornerRadius = {cr, cr, cr, cr}
		}) {
			// Title bar
			CLAY(CLAY_ID("PauseTitle"), {
				.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(40, 80)},
					.padding = {padH, padH, padV, padV},
					.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
				},
				.backgroundColor = {30, 30, 40, 255},
				.cornerRadius = {cr, cr, 0, 0}
			}) {
				CLAY_TEXT(CLAY_STRING("Game Paused"),
					CLAY_TEXT_CONFIG({
						.textColor = {255, 255, 255, 255},
						.fontId = 0,
						.fontSize = titleFont,
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
				},
				.backgroundColor = {100, 100, 120, 255}
			}) {}

			buttonComponent("BtnExitMenu", "Exit to Menu", m_callbacks.on_exit_to_menu);
			buttonComponent("BtnExitOS", "Exit to OS", m_callbacks.on_exit_to_os);

			// Bottom padding
			CLAY(CLAY_ID("PauseBottomPad"), {
				.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(16)},
				},
				.backgroundColor = {40, 40, 50, 255},
				.cornerRadius = {0, 0, cr, cr}
			}) {}
		}
	}
}
