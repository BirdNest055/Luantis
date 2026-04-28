/*
Luantis — Clay Pause Menu
A Clay-based replacement for the formspec pause menu.
*/

#pragma once

#include "clay_integration.h"
#include <clay.h>
#include <string>

class Client;

/// Clay-based pause menu panel.
/// Replaces the formspec-based pause menu when the `clay_pause_menu` setting is true.
class ClayPauseMenu : public IClayPanel {
public:
	ClayPauseMenu(Client *client, bool simple_singleplayer_mode);

	// IClayPanel interface
	const std::string &getId() const override;
	bool isVisible() const override;
	void setVisible(bool v) override;
	void buildLayout() override;
	void onLayoutComplete() override;
	bool handleClick(const Clay_ElementId &element_id) override;

	/// Reset all action flags (call after processing actions)
	void resetActions();

	// Action flags — read by GameFormSpec
	bool wantsContinue() const { return m_wants_continue; }
	bool wantsDisconnect() const { return m_wants_disconnect; }
	bool wantsExitToOS() const { return m_wants_exit_to_os; }
	bool wantsSettings() const { return m_wants_settings; }
	bool wantsSoundVolume() const { return m_wants_sound_volume; }
	bool wantsChangePassword() const { return m_wants_change_password; }

private:
	/// Build a styled menu button
	void menuButton(const char *id, const char *label, float scale);

	Client *m_client;
	bool m_simple_singleplayer_mode;
	bool m_visible = false;

	// Action flags
	bool m_wants_continue = false;
	bool m_wants_disconnect = false;
	bool m_wants_exit_to_os = false;
	bool m_wants_settings = false;
	bool m_wants_sound_volume = false;
	bool m_wants_change_password = false;

	// Style constants
	static constexpr float PANEL_WIDTH = 380.0f;
	static constexpr float PANEL_PADDING = 24.0f;
	static constexpr float BUTTON_HEIGHT = 44.0f;
	static constexpr float BUTTON_GAP = 8.0f;
	static constexpr float TITLE_SIZE = 28.0f;
	static constexpr float SUBTITLE_SIZE = 14.0f;
	static constexpr float BUTTON_TEXT_SIZE = 18.0f;
	static constexpr float INFO_TEXT_SIZE = 12.0f;
};
