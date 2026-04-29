/*
 * Clay GUI Manager
 *
 * Manages Clay lifecycle, panel stack, and input routing.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

#include "clay_gui_manager.h"

#include <irrlicht.h>
#include "client/fontengine.h"
#include "client/renderingengine.h"
#include "log.h"

#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Clay error handler
// ---------------------------------------------------------------------------

static void clayErrorHandler(Clay_ErrorData errorData)
{
	// Log Clay errors but don't crash
	std::string msg(errorData.errorText.chars, errorData.errorText.length);
	errorstream << "Clay error: " << msg << std::endl;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ClayGUIManager::~ClayGUIManager()
{
	shutdown();
}

void ClayGUIManager::init(irr::IrrlichtDevice *device, FontEngine *fontEngine,
	uint32_t maxElementCount)
{
	if (m_initialized)
		return;

	m_max_element_count = maxElementCount;

	// Set max element count before querying memory
	Clay_SetMaxElementCount(m_max_element_count);

	// Calculate required memory and allocate arena
	uint64_t totalMemorySize = Clay_MinMemorySize();
	m_arena_memory = malloc(totalMemorySize);
	if (!m_arena_memory) {
		errorstream << "Clay: failed to allocate " << totalMemorySize
			<< " bytes for arena" << std::endl;
		return;
	}

	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(
		totalMemorySize, m_arena_memory);

	// Get screen dimensions from Irrlicht
	auto *driver = device->getVideoDriver();
	int screenW = driver->getScreenSize().Width;
	int screenH = driver->getScreenSize().Height;

	m_clay_context = Clay_Initialize(arena,
		Clay_Dimensions{static_cast<float>(screenW), static_cast<float>(screenH)},
		(Clay_ErrorHandler){clayErrorHandler});

	// Set up text measurement
	m_renderer.init(device);
	m_renderer.setFontEngine(fontEngine);
	Clay_SetMeasureTextFunction(ClayIrrlichtRenderer::measureText, &m_renderer);

	m_initialized = true;
	infostream << "Clay GUI initialized with " << m_max_element_count
		<< " max elements, " << totalMemorySize << " bytes arena" << std::endl;
}

void ClayGUIManager::shutdown()
{
	if (!m_initialized)
		return;

	Clay_SetCurrentContext(m_clay_context);
	// Clay doesn't have a shutdown function — we just free the arena
	if (m_arena_memory) {
		free(m_arena_memory);
		m_arena_memory = nullptr;
	}

	m_initialized = false;
	infostream << "Clay GUI shut down" << std::endl;
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void ClayGUIManager::update(float dtime, int screenWidth, int screenHeight,
	int mouseX, int mouseY, bool mouseLeftDown,
	float scrollDeltaX, float scrollDeltaY)
{
	if (!m_initialized)
		return;

	// Activate our Clay context (important for multi-context scenarios)
	Clay_SetCurrentContext(m_clay_context);

	// 1. Update layout dimensions (for resize support)
	Clay_SetLayoutDimensions(
		Clay_Dimensions{static_cast<float>(screenWidth),
			static_cast<float>(screenHeight)});

	// 2. Update pointer state
	m_mouse_x = mouseX;
	m_mouse_y = mouseY;
	m_mouse_left_down = mouseLeftDown;
	Clay_SetPointerState(
		Clay_Vector2{static_cast<float>(mouseX), static_cast<float>(mouseY)},
		mouseLeftDown);

	// 3. Update scroll containers
	Clay_UpdateScrollContainers(true,
		Clay_Vector2{scrollDeltaX, scrollDeltaY}, dtime);

	// 4. Begin layout
	Clay_BeginLayout();

	// 5. Let each visible panel declare its layout
	for (auto *panel : m_panels) {
		if (panel->isVisible()) {
			panel->buildLayout();
		}
	}

	// 6. End layout and get render commands
	Clay_RenderCommandArray renderCommands = Clay_EndLayout(dtime);

	// 7. Render
	m_renderer.render(renderCommands);
}

// ---------------------------------------------------------------------------
// Panel management
// ---------------------------------------------------------------------------

void ClayGUIManager::addPanel(ClayUIPanel *panel)
{
	m_panels.push_back(panel);
	m_panel_by_name[panel->getName()] = panel;
}

void ClayGUIManager::showPanel(const std::string &name)
{
	auto it = m_panel_by_name.find(name);
	if (it != m_panel_by_name.end()) {
		it->second->setVisible(true);
		it->second->onShow();
	}
}

void ClayGUIManager::hidePanel(const std::string &name)
{
	auto it = m_panel_by_name.find(name);
	if (it != m_panel_by_name.end()) {
		it->second->onHide();
		it->second->setVisible(false);
	}
}

void ClayGUIManager::hideAll()
{
	for (auto *panel : m_panels) {
		if (panel->isVisible()) {
			panel->onHide();
			panel->setVisible(false);
		}
	}
}

bool ClayGUIManager::hasVisiblePanels() const
{
	for (const auto *panel : m_panels) {
		if (panel->isVisible())
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

bool ClayGUIManager::handleInput(const irr::SEvent &event)
{
	if (!m_initialized || !hasVisiblePanels())
		return false;

	// Basic mouse event handling — check if the click is inside any panel
	// Clay's pointer state is updated in the main update(), so we can
	// query hover/click state after layout.

	// For now, always consume mouse events if panels are visible
	// This prevents the formspec system from also processing the event
	if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
		return true;
	}

	// Consume key events for ESC handling etc.
	if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
		// ESC closes the topmost panel
		if (event.KeyInput.Key == irr::KEY_ESCAPE && event.KeyInput.PressedDown) {
			// Hide the last visible panel (top of stack)
			for (auto it = m_panels.rbegin(); it != m_panels.rend(); ++it) {
				if ((*it)->isVisible()) {
					(*it)->onHide();
					(*it)->setVisible(false);
					return true;
				}
			}
		}
		return true;
	}

	return false;
}
