/*
 * Clay-based Pause Menu
 *
 * A modern pause menu built with Clay's declarative layout system,
 * replacing the old formspec-based pause menu. This is the first
 * dialog to be ported to Clay as a proof of concept.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

#pragma once

#include "clay_gui_manager.h"

#include <string>
#include <functional>

/** Callbacks for pause menu actions. */
struct ClayPauseMenuCallbacks {
	std::function<void()> on_resume;
	std::function<void()> on_settings;
	std::function<void()> on_change_password;
	std::function<void()> on_volume;
	std::function<void()> on_exit_to_menu;
	std::function<void()> on_exit_to_os;
};

/**
 * Clay-based pause menu panel.
 *
 * Declares its layout using Clay macros each frame and handles
 * button clicks via Clay's pointer interaction system.
 */
class ClayPauseMenu : public ClayUIPanel {
public:
	ClayPauseMenu(ClayPauseMenuCallbacks callbacks);

	void buildLayout() override;
	void onShow() override;
	void onHide() override;

private:
	ClayPauseMenuCallbacks m_callbacks;

	// Style configs — reusable Clay_ElementDeclaration structs
	static Clay_ElementDeclaration s_panel_style;
	static Clay_ElementDeclaration s_title_style;
	static Clay_ElementDeclaration s_button_style;

	/** Shared button component. */
	void buttonComponent(const char *id, const char *label,
		std::function<void()> onClick);

	// Click handling
	static void onButtonClicked(Clay_ElementId elementId,
		Clay_PointerData pointerInfo, void *userData);

	struct ButtonAction {
		std::function<void()> action;
	};
	std::vector<ButtonAction> m_button_actions;
};
