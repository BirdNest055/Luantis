/*
 * Clay GUI Manager
 *
 * Manages Clay's lifecycle (init, layout, render) and provides a bridge
 * between the Clay declarative layout system and Luantis's existing
 * GUI framework. This allows Clay-based UIs to coexist with the
 * traditional formspec system.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

#pragma once

#include "clay_renderer.h"
#include "clay_theme.h"

#include "clay.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// IrrlichtDevice and SEvent are in the global namespace in this Irrlicht fork
class IrrlichtDevice;
struct SEvent;

class FontEngine;

/**
 * Represents a Clay-based UI panel/dialog that can be shown or hidden.
 *
 * Subclass this to create specific UI panels (e.g., ClayPauseMenu,
 * ClaySettingsDialog, etc.) and override buildLayout() to declare the
 * Clay UI hierarchy.
 */
class ClayUIPanel {
public:
        ClayUIPanel(const std::string &name) : m_name(name) {}
        virtual ~ClayUIPanel() = default;

        /** Called every frame to declare the Clay layout. */
        virtual void buildLayout() = 0;

        /** Called when the panel is first shown. */
        virtual void onShow() {}

        /** Called when the panel is hidden. */
        virtual void onHide() {}

        const std::string &getName() const { return m_name; }
        bool isVisible() const { return m_visible; }
        void setVisible(bool v) { m_visible = v; }

private:
        std::string m_name;
        bool m_visible = false;
};

/**
 * Central manager for the Clay GUI system.
 *
 * Owns the Clay arena/context and the Irrlicht renderer.
 * Manages a stack of ClayUIPanels and orchestrates the
 * per-frame layout -> render cycle.
 */
class ClayGUIManager {
public:
        ClayGUIManager() = default;
        ~ClayGUIManager();

        /** Non-copyable */
        ClayGUIManager(const ClayGUIManager &) = delete;
        ClayGUIManager &operator=(const ClayGUIManager &) = delete;

        /**
         * Initialize the Clay system.
         * @param device  Irrlicht device (global ::IrrlichtDevice)
         * @param fontEngine  Font engine for text measurement
         * @param maxElementCount  Max Clay elements per frame (default 8192)
         */
        void init(IrrlichtDevice *device, FontEngine *fontEngine,
                int32_t maxElementCount = Theme::Renderer::defaultMaxElements);

        /** Shut down and free the Clay arena. */
        void shutdown();

        /** Called every frame: updates Clay state and renders all visible panels. */
        void update(float dtime, int screenWidth, int screenHeight,
                int mouseX, int mouseY,
                float scrollDeltaX, float scrollDeltaY);

        /** Register a panel. The manager does NOT take ownership. */
        void addPanel(ClayUIPanel *panel);

        /** Show a panel by name. */
        void showPanel(const std::string &name);

        /** Hide a panel by name. */
        void hidePanel(const std::string &name);

        /** Hide all panels. */
        void hideAll();

        /** Check if any Clay panel is currently visible (consumes input). */
        bool hasVisiblePanels() const;

        /** Get the renderer (for advanced use). */
        ClayIrrlichtRenderer &getRenderer() { return m_renderer; }

        /**
         * Handle an input event from the game's event receiver.
         * Returns true if the event was consumed by a Clay panel.
         *
         * Mouse state is tracked internally for use by update().
         * Clay_SetPointerState() is NOT called here — only in update() —
         * to prevent PRESSED_THIS_FRAME from being promoted to PRESSED
         * before the layout pass runs.
         */
        bool handleInput(const SEvent &event);

private:
        ClayIrrlichtRenderer m_renderer;
        Clay_Context *m_clay_context = nullptr;
        void *m_arena_memory = nullptr;
        int32_t m_max_element_count = Theme::Renderer::defaultMaxElements;

        std::vector<ClayUIPanel *> m_panels;
        std::unordered_map<std::string, ClayUIPanel *> m_panel_by_name;

        int m_mouse_x = 0;
        int m_mouse_y = 0;
        bool m_mouse_left_down = false;
        float m_scroll_x = 0.0f;
        float m_scroll_y = 0.0f;

        bool m_initialized = false;
};

// Global Clay GUI manager pointer, set by Game during init.
// Used by MyEventReceiver to forward events for Clay panel input.
extern ClayGUIManager *g_clay_gui_manager;
