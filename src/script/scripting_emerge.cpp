// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 sfan5 <sfan5@live.de>

#include "scripting_emerge.h"
#include "emerge_internal.h"
#include "server.h"
#include "settings.h"
#include "cpp_api/s_internal.h"
#include "common/c_packer.h"
#include "lua_api/l_areastore.h"
#include "lua_api/l_base.h"
#include "lua_api/l_craft.h"
#include "lua_api/l_env.h"
#include "lua_api/l_item.h"
#include "lua_api/l_itemstackmeta.h"
#include "lua_api/l_mapgen.h"
#include "lua_api/l_noise.h"
#include "lua_api/l_server.h"
#include "lua_api/l_util.h"
#include "lua_api/l_vmanip.h"
#include "lua_api/l_settings.h"
#include "lua_api/l_ipc.h"

extern "C" {
#include <lualib.h>
}

EmergeScripting::EmergeScripting(EmergeThread *parent):
		ScriptApiBase(ScriptingType::Emerge)
{
	setGameDef(parent->m_server);
	setEmergeThread(parent);

	SCRIPTAPI_PRECHECKHEADER

	if (g_settings->getBool("secure.enable_security"))
		initializeSecurity();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	InitializeModApi(L, top);

	auto *data = ModApiBase::getServer(L)->m_lua_globals_data.get();
	assert(data);
	script_unpack(L, data);
	lua_setfield(L, top, "transferred_globals");

	lua_pop(L, 1);

	// Push builtin initialization type
	lua_pushstring(L, "emerge");
	lua_setglobal(L, "INIT");
}

void EmergeScripting::InitializeModApi(lua_State *L, int top)
{
	// Register reference classes (userdata)
	ItemStackMetaRef::Register(L);
	LuaAreaStore::Register(L);
	LuaItemStack::Register(L);
	LuaValueNoise::Register(L);
	LuaValueNoiseMap::Register(L);
	LuaPseudoRandom::Register(L);
	LuaPcgRandom::Register(L);
	LuaSecureRandom::Register(L);
	LuaVoxelManip::Register(L);
	LuaSettings::Register(L);

	// Initialize mod api modules
	ModApiCraft::InitializeAsync(L, top);
	ModApiEnvVM::InitializeEmerge(L, top);
	ModApiItem::InitializeAsync(L, top);
	ModApiMapgen::InitializeEmerge(L, top);
	ModApiServer::InitializeAsync(L, top);
	ModApiUtil::InitializeAsync(L, top);
	ModApiIPC::Initialize(L, top);
	// NOTE: The InitializeAsync/InitializeEmerge naming is inconsistent.
	// "Async" implies a threaded context, but "Emerge" is more specific.
	// These should be renamed to InitializeRO (read-only) to clarify that
	// the emerge environment has read-only access to game state, unlike the
	// main server environment which has read-write access. This rename should
	// be applied consistently across all scripting environments:
	//   InitializeAsync → InitializeRO
	//   InitializeEmerge → InitializeRO
	//   InitializeCSM → InitializeRO (for client-side mods)
}
