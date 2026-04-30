/*
 * Clay-based Pause Menu
 *
 * A modern pause menu built with Clay's declarative layout system.
 * All visual styling is centralized in clay_theme.h — edit that
 * single file to change colors, sizes, spacing, fonts, and labels.
 *
 * Part of the Luantis Clay GUI integration (v9.48).
 */

#include "clay_pause_menu.h"

#include "clay_theme.h"
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
// Button component — uses Theme::Styles for all visual properties
// ---------------------------------------------------------------------------

void ClayPauseMenu::buttonComponent(const char *id, const char *label,
		std::function<void()> onClick)
{
	// Store the action so it survives the frame
	m_button_actions.push_back({.action = std::move(onClick)});
	ButtonAction *actionPtr = &m_button_actions.back();

	// Use Clay_GetElementId for dynamic (non-literal) string IDs
	Clay_ElementId clayId = Clay_GetElementId(Clay_String{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(strlen(id)),
		.chars = id});

	// Check hover state for this element ID
	bool hovered = Clay_PointerOver(clayId);

	// Select style based on interaction state — all from Theme
	Clay_ElementDeclaration btnStyle = hovered
		? Theme::Styles::buttonHover()
		: Theme::Styles::button();

	CLAY(clayId, btnStyle) {
		Clay_OnHover(onButtonClicked, actionPtr);

		// Text config from Theme — hover vs normal
		Clay_TextElementConfig textCfg = hovered
			? Theme::Styles::buttonHoverTextConfig()
			: Theme::Styles::buttonTextConfig();

		// Construct Clay_String for dynamic label text
		Clay_String labelStr = {.isStaticallyAllocated = false,
			.length = static_cast<int32_t>(strlen(label)),
			.chars = label};

		CLAY_TEXT(labelStr, textCfg);
	}
}

// ---------------------------------------------------------------------------
// Build layout — called every frame while visible
// All IDs and styles come from clay_theme.h
// ---------------------------------------------------------------------------

void ClayPauseMenu::buildLayout()
{
	// Clear per-frame action storage and reserve to prevent reallocation
	// (which would invalidate userData pointers stored in Clay_OnHover)
	m_button_actions.clear();
	m_button_actions.reserve(8);

	// Full-screen dark overlay with centered content
	CLAY(CLAY_ID(THEME_ID_PAUSE_OVERLAY), Theme::Styles::panelOverlay()) {

		// Content column
		CLAY(CLAY_ID(THEME_ID_PAUSE_CONTENT), Theme::Styles::contentColumn()) {

			// Title bar
			CLAY(CLAY_ID(THEME_ID_PAUSE_TITLE), Theme::Styles::titleBar()) {
				CLAY_TEXT(CLAY_STRING(THEME_LABEL_PAUSE_TITLE),
					Theme::Styles::titleTextConfig());
			}

			// Buttons (IDs from Theme::IDs::PauseMenu, labels from Theme::Labels::PauseMenu)
			buttonComponent(Theme::IDs::PauseMenu::btnResume,
				Theme::Labels::PauseMenu::resume, m_callbacks.on_resume);
			buttonComponent(Theme::IDs::PauseMenu::btnSettings,
				Theme::Labels::PauseMenu::settings, m_callbacks.on_settings);
			buttonComponent(Theme::IDs::PauseMenu::btnPassword,
				Theme::Labels::PauseMenu::changePwd, m_callbacks.on_change_password);
			buttonComponent(Theme::IDs::PauseMenu::btnVolume,
				Theme::Labels::PauseMenu::volume, m_callbacks.on_volume);

			// Separator
			CLAY(CLAY_ID(THEME_ID_PAUSE_SEPARATOR), Theme::Styles::separator()) {}

			buttonComponent(Theme::IDs::PauseMenu::btnExitMenu,
				Theme::Labels::PauseMenu::exitToMenu, m_callbacks.on_exit_to_menu);
			buttonComponent(Theme::IDs::PauseMenu::btnExitOS,
				Theme::Labels::PauseMenu::exitToOS, m_callbacks.on_exit_to_os);

			// Bottom rounded corner padding
			CLAY(CLAY_ID(THEME_ID_PAUSE_BOTTOM_PAD), Theme::Styles::bottomPad()) {}
		}
	}
}
