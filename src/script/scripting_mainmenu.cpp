// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "scripting_mainmenu.h"
#include "cpp_api/s_internal.h"
#include "lua_api/l_base.h"
#include "lua_api/l_http.h"
#include "lua_api/l_mainmenu.h"
#include "lua_api/l_mainmenu_sound.h"
#include "lua_api/l_menu_common.h"
#include "lua_api/l_util.h"
#include "lua_api/l_settings.h"
#include "log.h"
#include "filesys.h"
#include "porting.h"

extern "C" {
#include "lualib.h"
}

#define MAINMENU_NUM_ASYNC_THREADS 2


MainMenuScripting::MainMenuScripting(GUIEngine* guiengine):
		ScriptApiBase(ScriptingType::MainMenu)
{
	setGuiEngine(guiengine);

	SCRIPTAPI_PRECHECKHEADER

	initializeSecurity();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	lua_newtable(L);
	lua_setglobal(L, "gamedata");

	// Initialize our lua_api modules
	initializeModApi(L, top);
	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "mainmenu");
	lua_setglobal(L, "INIT");

	infostream << "SCRIPTAPI: Initialized main menu modules" << std::endl;
}

void MainMenuScripting::initializeModApi(lua_State *L, int top)
{
	registerLuaClasses(L, top);

	// Initialize mod API modules
	ModApiMenuCommon::Initialize(L, top);
	ModApiMainMenu::Initialize(L, top);
	ModApiUtil::Initialize(L, top);
	ModApiMainMenuSound::Initialize(L, top);
	ModApiHttp::Initialize(L, top);

	asyncEngine.registerStateInitializer(registerLuaClasses);
	asyncEngine.registerStateInitializer(ModApiMenuCommon::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiMainMenu::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiUtil::InitializeAsync);
	asyncEngine.registerStateInitializer(ModApiHttp::InitializeAsync);

	// Initialize async environment
	asyncEngine.initialize(MAINMENU_NUM_ASYNC_THREADS);
}

void MainMenuScripting::registerLuaClasses(lua_State *L, int top)
{
	LuaSettings::Register(L);
	MainMenuSoundHandle::Register(L);
}

bool MainMenuScripting::mayModifyPath(const std::string &path)
{
	std::string path_temp = fs::AbsolutePathPartial(fs::TempPath());
	if (fs::PathStartsWith(path, path_temp))
		return true;

	std::string path_user = fs::AbsolutePathPartial(porting::path_user);
	if (fs::PathStartsWith(path, path_user))
		return true;

	if (fs::PathStartsWith(path, fs::AbsolutePathPartial(porting::path_cache)))
		return true;

	return false;
}

bool MainMenuScripting::checkPathAccess(const std::string &abs_path, bool write_required,
	bool *write_allowed)
{
	if (mayModifyPath(abs_path)) {
		if (write_allowed)
			*write_allowed = true;
		return true;
	}
	// NOTE: Allowing global read access (returning true when !write_required)
	// sounds too broad - a mainmenu script could read any file on the system.
	// Root cause: The check only verifies write access via mayModifyPath().
	// For read access, it always returns true if write is not required,
	// regardless of the path. There is no read-path allowlist.
	// Proposed fix: Add a read-path allowlist that restricts reads to
	// known safe directories (e.g., game paths, mod paths, world paths,
	// temp directory, config directory). Paths outside these directories
	// should require explicit user consent. This is a security-sensitive
	// change that needs careful design to avoid breaking legitimate mod
	// use cases (e.g., reading texture packs from arbitrary locations).
	return !write_required;
}

void MainMenuScripting::step()
{
	asyncEngine.step(getStack());
}

u32 MainMenuScripting::queueAsync(std::string &&serialized_func,
		std::string &&serialized_param)
{
	return asyncEngine.queueAsyncJob(std::move(serialized_func), std::move(serialized_param));
}

