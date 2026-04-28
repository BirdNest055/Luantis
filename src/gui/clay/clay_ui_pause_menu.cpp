/*
Luantis — Clay Pause Menu Implementation
*/

#include "clay_ui_pause_menu.h"
#include "clay_text_measurer.h"

#include "client/client.h"
#include "gettext.h"
#include "log.h"
#include "porting.h"
#include "version.h"

#include <irrlicht.h>

ClayPauseMenu::ClayPauseMenu(Client *client, bool simple_singleplayer_mode)
	: m_client(client)
	, m_simple_singleplayer_mode(simple_singleplayer_mode)
{
}

const std::string &ClayPauseMenu::getId()
{
	static std::string id = "pause_menu";
	return id;
}

bool ClayPauseMenu::isVisible()
{
	return m_visible;
}

void ClayPauseMenu::setVisible(bool v)
{
	m_visible = v;
}

void ClayPauseMenu::resetActions()
{
	m_wants_continue = false;
	m_wants_disconnect = false;
	m_wants_exit_to_os = false;
	m_wants_settings = false;
	m_wants_sound_volume = false;
	m_wants_change_password = false;
}

void ClayPauseMenu::menuButton(const char *id, const char *label, float scale)
{
	uintptr_t font_spec = ClayTextMeasurer::encodeFontSpec(
		(u32)(BUTTON_TEXT_SIZE * scale));

	Clay_Color btn_bg = {0.15f, 0.15f, 0.2f, 0.9f};
	Clay_Color btn_hover = {0.25f, 0.25f, 0.35f, 0.95f};
	Clay_Color btn_text_color = {0.9f, 0.9f, 0.95f, 1.0f};

	// Check hover state
	bool hovered = Clay_Hovered();
	if (hovered) {
		btn_bg = btn_hover;
	}

	CLAY(CLAY_ID(id), {
		.layout = {
			.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(BUTTON_HEIGHT * scale)
			},
			.padding = {0, 0, 0, 0},
			.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
		},
		.backgroundColor = btn_bg,
		.cornerRadius = {6 * scale, 6 * scale, 6 * scale, 6 * scale}
	}) {
		Clay_OnHover([](Clay_ElementId elementId, Clay_PointerData pointerData, uintptr_t userData) {
			// Hover handler — just marks the element as hovered for visual feedback
		}, 0);

		CLAY_TEXT(CLAY_STRING(label), CLAY_TEXT_CONFIG({
			.fontSize = (u16)(BUTTON_TEXT_SIZE * scale),
			.textColor = btn_text_color,
			.userData = font_spec
		}));
	}
}

void ClayPauseMenu::buildLayout()
{
	if (!m_visible)
		return;

	float scale = 1.0f;
	// TODO: Get actual HUD scaling from settings
	// float scale = g_settings->getFloat("hud_scaling", 0.5f, 20.0f);

	uintptr_t title_font = ClayTextMeasurer::encodeFontSpec((u32)(TITLE_SIZE * scale));
	uintptr_t subtitle_font = ClayTextMeasurer::encodeFontSpec((u32)(SUBTITLE_SIZE * scale));
	uintptr_t info_font = ClayTextMeasurer::encodeFontSpec((u32)(INFO_TEXT_SIZE * scale));

	// Full-screen semi-transparent overlay
	CLAY(CLAY_ID("PauseOverlay"), {
		.layout = {
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
			.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
		},
		.backgroundColor = {0.0f, 0.0f, 0.0f, 0.55f}
	}) {
		// Inner panel
		CLAY(CLAY_ID("PausePanel"), {
			.layout = {
				.sizing = {
					.width = CLAY_SIZING_FIXED(PANEL_WIDTH * scale),
					.height = CLAY_SIZING_FIXED(0) // auto-height
				},
				.padding = {
					PANEL_PADDING * scale, PANEL_PADDING * scale,
					PANEL_PADDING * scale, PANEL_PADDING * scale
				},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.childGap = BUTTON_GAP * scale
			},
			.backgroundColor = {0.08f, 0.08f, 0.12f, 0.95f},
			.cornerRadius = {12 * scale, 12 * scale, 12 * scale, 12 * scale}
		}) {
			// Title
			CLAY_TEXT(CLAY_STRING("Luanti"), CLAY_TEXT_CONFIG({
				.fontSize = (u16)(TITLE_SIZE * scale),
				.textColor = {0.95f, 0.95f, 1.0f, 1.0f},
				.userData = title_font
			}));

			// Subtitle
			CLAY_TEXT(CLAY_STRING("Game Paused"), CLAY_TEXT_CONFIG({
				.fontSize = (u16)(SUBTITLE_SIZE * scale),
				.textColor = {0.6f, 0.6f, 0.7f, 1.0f},
				.userData = subtitle_font
			}));

			// Spacer
			CLAY(CLAY_ID("Spacer1"), {
				.layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(12 * scale)}}
			}) {};

			// Continue button (always shown)
			menuButton("btn_continue", strgettext("Continue"), scale);

			// Change Password button (multiplayer only)
			if (!m_simple_singleplayer_mode) {
				menuButton("btn_change_password", strgettext("Change Password"), scale);
			}

			// Settings
			menuButton("btn_settings", strgettext("Settings"), scale);

			// Sound Volume
			menuButton("btn_sound", strgettext("Sound Volume"), scale);

			// Exit to Menu
			menuButton("btn_exit_menu", strgettext("Exit to Menu"), scale);

			// Exit to OS
			menuButton("btn_exit_os", strgettext("Exit to OS"), scale);

			// Spacer
			CLAY(CLAY_ID("Spacer2"), {
				.layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(8 * scale)}}
			}) {};

			// Game info text
			std::string info_text;
			if (m_client) {
				info_text = std::string(g_version_hash) + " | "
					+ m_client->getAddressName() + ":"
					+ std::to_string(m_client->getServerAddress().Port);
			} else {
				info_text = std::string(g_version_hash);
			}

			CLAY_TEXT(CLAY_STRING(info_text.c_str()), CLAY_TEXT_CONFIG({
				.fontSize = (u16)(INFO_TEXT_SIZE * scale),
				.textColor = {0.5f, 0.5f, 0.55f, 1.0f},
				.userData = info_font
			}));
		}
	}
}

void ClayPauseMenu::onLayoutComplete()
{
	// Post-layout processing — currently nothing needed
}

bool ClayPauseMenu::handleClick(const Clay_ElementId &element_id)
{
	std::string id(element_id.stringId.chars, element_id.stringId.length);

	if (id == "btn_continue") {
		m_wants_continue = true;
		return true;
	} else if (id == "btn_change_password") {
		m_wants_change_password = true;
		return true;
	} else if (id == "btn_settings") {
		m_wants_settings = true;
		return true;
	} else if (id == "btn_sound") {
		m_wants_sound_volume = true;
		return true;
	} else if (id == "btn_exit_menu") {
		m_wants_disconnect = true;
		return true;
	} else if (id == "btn_exit_os") {
		m_wants_exit_to_os = true;
		return true;
	}

	return false;
}
