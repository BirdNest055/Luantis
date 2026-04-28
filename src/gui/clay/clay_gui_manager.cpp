/*
Luantis — Clay GUI Manager Implementation

Note: This file is compiled with C++20 for Clay designated initializer support.
CLAY_IMPLEMENTATION is defined in lib/clay/clay_impl.c (compiled as C99).
*/

#include <clay.h>

#include "clay_gui_manager.h"
#include "clay_irrlicht_renderer.h"
#include "clay_text_measurer.h"
#include "clay_event_bridge.h"
#include "clay_integration.h"

#include "client/fontengine.h"
#include "log.h"
#include "settings.h"

#include <irrlicht.h>
#include <cstdlib>
#include <algorithm>

ClayGUIManager::ClayGUIManager(irr::video::IVideoDriver *driver,
		FontEngine *font_engine)
	: m_driver(driver)
	, m_font_engine(font_engine)
{
	m_renderer = std::make_unique<ClayIrrlichtRenderer>(driver, font_engine);
	m_text_measurer = std::make_unique<ClayTextMeasurer>(font_engine);
	m_event_bridge = std::make_unique<ClayEventBridge>();
}

ClayGUIManager::~ClayGUIManager() = default;

void ClayGUIManager::clayErrorHandler(Clay_ErrorData error_data)
{
	errorstream << "Clay UI error: " << std::string(error_data.errorText.chars,
		error_data.errorText.length) << std::endl;
}

void ClayGUIManager::init(size_t arena_size)
{
	void *arena_memory = malloc(arena_size);
	if (!arena_memory) {
		errorstream << "Clay: Failed to allocate " << arena_size
			<< " bytes for arena" << std::endl;
		return;
	}

	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(arena_size, arena_memory);

	Clay_Dimensions dims = {(float)m_screen_width, (float)m_screen_height};
	Clay_Initialize(arena, dims,
		(Clay_ErrorHandler) {
			.errorHandlerFunction = &ClayGUIManager::clayErrorHandler,
			.userData = 0
		});

	m_text_measurer->install();

	m_initialized = true;
	infostream << "Clay UI initialized with " << arena_size
		<< " byte arena, screen " << m_screen_width << "x" << m_screen_height << std::endl;
}

void ClayGUIManager::onResize(uint32_t width, uint32_t height)
{
	m_screen_width = width;
	m_screen_height = height;
	if (m_initialized) {
		Clay_SetLayoutDimensions({(float)width, (float)height});
	}
}

void ClayGUIManager::update(float dtime)
{
	if (!m_initialized)
		return;

	Clay_SetLayoutDimensions({(float)m_screen_width, (float)m_screen_height});

	ClayEventVec2 ep = m_event_bridge->getPointerPos();
	bool is_down = m_event_bridge->isPointerDown();
	Clay_SetPointerState({ep.x, ep.y}, is_down);

	ClayEventVec2 scroll = m_event_bridge->getScrollDelta();
	Clay_UpdateScrollContainers(true, {scroll.x, scroll.y}, dtime);

	Clay_BeginLayout();

	for (auto *panel : m_panels) {
		if (panel->isVisible()) {
			panel->buildLayout();
		}
	}

	Clay_RenderCommandArray commands = Clay_EndLayout();

	for (auto *panel : m_panels) {
		if (panel->isVisible()) {
			panel->onLayoutComplete();
		}
	}

	m_renderer->render(commands);

	m_event_bridge->resetFrameState();
}

bool ClayGUIManager::handleEvent(const irr::SEvent &event)
{
	if (!m_initialized)
		return false;

	bool relevant = m_event_bridge->onEvent(event);

	if (relevant && hasVisiblePanels()) {
		if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
			switch (event.MouseInput.Event) {
			case irr::EMIE_LMOUSE_PRESSED_DOWN:
			case irr::EMIE_LMOUSE_LEFT_UP:
			case irr::EMIE_MOUSE_WHEEL:
				return true;
			default:
				break;
			}
		}
	}

	return false;
}

void ClayGUIManager::addPanel(IClayPanel *panel)
{
	m_panels.push_back(panel);
}

void ClayGUIManager::removePanel(IClayPanel *panel)
{
	m_panels.erase(std::remove(m_panels.begin(), m_panels.end(), panel), m_panels.end());
}

IClayPanel *ClayGUIManager::getPanel(const std::string &id)
{
	for (auto *panel : m_panels) {
		if (panel->getId() == id)
			return panel;
	}
	return nullptr;
}

bool ClayGUIManager::hasVisiblePanels() const
{
	for (const auto *panel : m_panels) {
		if (panel->isVisible())
			return true;
	}
	return false;
}
