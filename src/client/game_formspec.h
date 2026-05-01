// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 cx384

#pragma once

#include <memory>
#include <string>
#include "irr_v3d.h"
#include "scripting_pause_menu.h"

class Client;
class RenderingEngine;
class InputHandler;
class GUIFormSpecMenu;

/*
This object is intended to contain the core formspec functionality.
It includes:
  - methods to show specific formspec menus
  - storing the opened formspec
  - handling formspec related callbacks
 */
struct GameFormSpec
{
        void init(Client *client, RenderingEngine *rendering_engine, InputHandler *input);

        ~GameFormSpec() { reset(); }

        void showFormSpec(const std::string &formspec, const std::string &formname);
        void showCSMFormSpec(const std::string &formspec, const std::string &formname);
        // Used by the Lua pause menu environment to show formspecs.
        // Currently only used for the in-game settings menu.
        void showPauseMenuFormSpec(const std::string &formspec, const std::string &formname);
        void showNodeFormspec(const std::string &formspec, const v3s16 &nodepos);
        /// If `!fs_override`: Uses `player->inventory_formspec`.
        /// If ` fs_override`: Uses a temporary formspec until an update is received.
        void showPlayerInventory(const std::string *fs_override);
        void showDeathFormspecLegacy();
        // Shows the hardcoded "main" pause menu.
        void showPauseMenu();

        void update();
        void disableDebugView();

        bool handleCallbacks();
        void reset();

#ifdef __ANDROID__
        // Returns false if no formspec open
        bool handleAndroidUIInput();
#endif

private:
        Client *m_client;
        RenderingEngine *m_rendering_engine;
        InputHandler *m_input;
        std::unique_ptr<PauseMenuScripting> m_pause_script;

        /// The currently open formspec that is not a submenu of the pause menu.
        /// Layering is already managed by `GUIModalMenu` (`g_menumgr`).
        /// DEPRECATED: This pointer is redundant with g_menumgr and can become
        /// stale when pause-menu submenus are open. Long-term, all access should
        /// go through g_menumgr.tryGetTopMenu() instead.
        [[deprecated("Use g_menumgr.tryGetTopMenu() instead of m_formspec")]]
        GUIFormSpecMenu *m_formspec = nullptr;

        bool handleEmptyFormspec(const std::string &formspec, const std::string &formname);

        void deleteFormspec();
};
