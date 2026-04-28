/*
Luantis — Clay GUI Manager Implementation

Note: CLAY_IMPLEMENTATION is defined in lib/clay/clay_impl.c which
compiles Clay as C99 (designated initializers require C99/C++20).
This file only includes the Clay header for type declarations.
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
        // Allocate the Clay arena
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

        // Install the text measurement callback
        m_text_measurer->install();

        m_initialized = true;
        infostream << "Clay UI initialized with " << arena_size
                << " byte arena, screen " << m_screen_width << "x" << m_screen_height << std::endl;
}

void ClayGUIManager::onResize(irr::u32 width, irr::u32 height)
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

        // 1. Update layout dimensions
        Clay_SetLayoutDimensions({(float)m_screen_width, (float)m_screen_height});

        // 2. Update pointer state
        Clay_Vector2 pointer = m_event_bridge->getPointerPos();
        bool is_down = m_event_bridge->isPointerDown();
        Clay_SetPointerState(pointer, is_down);

        // 3. Update scroll containers
        Clay_Vector2 scroll = m_event_bridge->getScrollDelta();
        Clay_UpdateScrollContainers(true, scroll, dtime);

        // 4. Begin layout
        Clay_BeginLayout();

        // 5. Each visible panel builds its layout
        for (auto *panel : m_panels) {
                if (panel->isVisible()) {
                        panel->buildLayout();
                }
        }

        // 6. End layout — get render commands
        Clay_RenderCommandArray commands = Clay_EndLayout();

        // 7. Let panels do post-layout processing
        for (auto *panel : m_panels) {
                if (panel->isVisible()) {
                        panel->onLayoutComplete();
                }
        }

        // 8. Render all commands
        m_renderer->render(commands);

        // 9. Handle click detection
        // Clay detects clicks via pointer state changes (down then up on same element)
        // We check for clicked elements by querying Clay_Hovered + pointer up
        if (!is_down && m_event_bridge->isPointerDown() == false) {
                // This frame: pointer was released. Check if any Clay element was hovered.
                // Clay_OnHover callbacks fire during layout, so panels already know.
        }

        // 10. Reset per-frame state
        m_event_bridge->resetFrameState();
}

bool ClayGUIManager::handleEvent(const irr::SEvent &event)
{
        if (!m_initialized)
                return false;

        bool relevant = m_event_bridge->onEvent(event);

        // If we have visible panels and it's a mouse event, consume it
        // to prevent click-through to the game underneath
        if (relevant && hasVisiblePanels()) {
                // For mouse moves and clicks, check if the pointer is over a Clay element
                // For now, consume all mouse events when panels are visible
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
