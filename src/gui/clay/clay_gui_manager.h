/*
Luantis — Clay GUI Manager
Lifecycle manager for Clay UI: init, per-frame update, rendering, event routing.

Note: This header does NOT include clay.h because Clay requires C++20
(designated initializers). The Clay types are forward-declared here.
The actual Clay API is used only in the .cpp files which are compiled with C++20.
*/

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace irr {
namespace video { class IVideoDriver; }
struct SEvent;
}

class FontEngine;
class IClayPanel;
class ClayIrrlichtRenderer;
class ClayTextMeasurer;
class ClayEventBridge;

// Forward declarations for Clay types (matching clay.h definitions)
struct Clay_ElementId;
struct Clay_ErrorData;
struct Clay_RenderCommandArray;

/// Central orchestrator for the Clay UI system.
/// Owns the Clay memory arena, drives the per-frame layout/render cycle,
/// routes events to panels, and manages the list of active panels.
class ClayGUIManager {
public:
	ClayGUIManager(irr::video::IVideoDriver *driver, FontEngine *font_engine);
	~ClayGUIManager();

	/// Initialize Clay with a memory arena. Call once at startup.
	/// @param arena_size  Size in bytes (default 4MB)
	void init(size_t arena_size = 1 << 22);

	/// Called when the window is resized.
	void onResize(uint32_t width, uint32_t height);

	/// Per-frame update: feed input to Clay, run layout, render all visible panels.
	/// @param dtime  Delta time in seconds since last frame
	void update(float dtime);

	/// Feed an Irrlicht event. Returns true if Clay consumed it.
	bool handleEvent(const irr::SEvent &event);

	/// Register a UI panel.
	void addPanel(IClayPanel *panel);

	/// Remove a UI panel.
	void removePanel(IClayPanel *panel);

	/// Find a panel by ID. Returns nullptr if not found.
	IClayPanel *getPanel(const std::string &id);

	/// Check if any Clay panel is currently visible (for input blocking)
	bool hasVisiblePanels() const;

	ClayGUIManager(const ClayGUIManager &) = delete;
	ClayGUIManager &operator=(const ClayGUIManager &) = delete;

private:
	/// Error handler callback for Clay
	static void clayErrorHandler(Clay_ErrorData error_data);

	irr::video::IVideoDriver *m_driver;
	FontEngine *m_font_engine;

	std::unique_ptr<ClayIrrlichtRenderer> m_renderer;
	std::unique_ptr<ClayTextMeasurer> m_text_measurer;
	std::unique_ptr<ClayEventBridge> m_event_bridge;

	std::vector<IClayPanel *> m_panels;

	uint32_t m_screen_width = 0;
	uint32_t m_screen_height = 0;
	bool m_initialized = false;
	bool m_left_click_consumed = false;
};
