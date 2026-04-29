/*
 * Clay-based Pause Menu
 *
 * A modern, responsive pause menu built with Clay's declarative layout
 * system. Adapts to Mobile/Tablet/Desktop viewports using the
 * clay_responsive.h utilities.
 *
 * Part of the Luantis Clay GUI integration (v9.47).
 */

#pragma once

#include "clay_gui_manager.h"
#include "clay_responsive.h"

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
 * Fully responsive — adapts to Mobile, Tablet, and Desktop viewports.
 */
class ClayPauseMenu : public ClayUIPanel {
public:
	ClayPauseMenu(ClayPauseMenuCallbacks callbacks);

	void buildLayout() override;
	void onShow() override;
	void onHide() override;

private:
	ClayPauseMenuCallbacks m_callbacks;

	/** Shared button component (responsive). */
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
