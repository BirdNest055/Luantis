// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "test.h"
#include <algorithm>
#include "content/subgames.h"
#include "server/mods.h"
#include "settings.h"

#define SUBGAME_ID "devtest"

class TestServerModManager : public TestBase
{
public:
        TestServerModManager() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestServerModManager"; }

        void runTests(IGameDef *gamedef);

        std::string m_worlddir;

        static ServerModManager makeManager(const std::string &worldpath) {
                return ServerModManager(worldpath, findWorldSubgame(worldpath));
        }

        void testCreation();
        void testIsConsistent();
        void testUnsatisfiedMods();
        void testGetMods();
        void testGetModsWrongDir();
        void testGetModspec();
        void testGetModNamesWrongDir();
        void testGetModNames();
        void testGetModMediaPathsWrongDir();
        void testGetModMediaPaths();

        // Test stub for LUANTI_GAME_PATH environment variable handling.
        // When set, it should be used as an additional game search path.
        // Test cases needed:
        //   1. LUANTI_GAME_PATH set to valid dir → games found there
        //   2. LUANTI_GAME_PATH set to invalid dir → graceful fallback
        //   3. LUANTI_GAME_PATH + world.mt gameid → correct resolution order
        // Requires: setenv("LUANTI_GAME_PATH", test_game_dir, 1) before
        // constructing ServerModManager, then unsetenv() after.
        void testGamePathEnvVar();
};

static TestServerModManager g_test_instance;

void TestServerModManager::runTests(IGameDef *gamedef)
{
        if (!findSubgame(SUBGAME_ID).isValid()) {
                warningstream << "Can't find game " SUBGAME_ID ", skipping this module." << std::endl;
                return;
        }

        auto test_mods = getTestTempDirectory().append(DIR_DELIM "test_mods");
        {
                auto p = test_mods + (DIR_DELIM "test_mod" DIR_DELIM);
                fs::CreateAllDirs(p);
                std::ofstream ofs1(p + "mod.conf", std::ios::out | std::ios::binary);
                ofs1 << "name = test_mod\n"
                        << "description = Does nothing\n";
                std::ofstream ofs2(p + "init.lua", std::ios::out | std::ios::binary);
                ofs2 << "-- intentionally empty\n";
        }

        setenv("LUANTI_MOD_PATH", test_mods.c_str(), 1);

        m_worlddir = getTestTempDirectory().append(DIR_DELIM "world");
        fs::CreateDir(m_worlddir);

        TEST(testCreation);
        TEST(testIsConsistent);
        TEST(testGetModsWrongDir);
        TEST(testUnsatisfiedMods);
        TEST(testGetMods);
        TEST(testGetModspec);
        TEST(testGetModNamesWrongDir);
        TEST(testGetModNames);
        TEST(testGetModMediaPathsWrongDir);
        TEST(testGetModMediaPaths);
        // NOTE: testGamePathEnvVar is disabled until LUANTI_GAME_PATH handling
        // is fully implemented. The test outline exists in the method below.
        // TEST(testGamePathEnvVar);

        unsetenv("LUANTI_MOD_PATH");
}

void TestServerModManager::testCreation()
{
        std::string path = m_worlddir + DIR_DELIM + "world.mt";
        Settings world_config;
        world_config.set("gameid", SUBGAME_ID);
        world_config.set("load_mod_test_mod", "true");
        UASSERTEQ(bool, world_config.updateConfigFile(path.c_str()), true);

        auto sm = makeManager(m_worlddir);
}

void TestServerModManager::testGetModsWrongDir()
{
        // Test in non worlddir to ensure no mods are found
        auto sm = makeManager(m_worlddir + DIR_DELIM "..");
        UASSERTEQ(bool, sm.getMods().empty(), true);
}

void TestServerModManager::testUnsatisfiedMods()
{
        auto sm = makeManager(m_worlddir);
        UASSERTEQ(bool, sm.getUnsatisfiedMods().empty(), true);
}

void TestServerModManager::testIsConsistent()
{
        auto sm = makeManager(m_worlddir);
        UASSERTEQ(bool, sm.isConsistent(), true);
}

void TestServerModManager::testGetMods()
{
        auto sm = makeManager(m_worlddir);
        const auto &mods = sm.getMods();
        // `ls ./games/devtest/mods | wc -l` + 1 (test mod)
        UASSERTEQ(std::size_t, mods.size(), 35 + 1);

        // Ensure we found basenodes mod (part of devtest)
        // and test_mod (for testing MINETEST_MOD_PATH).
        bool default_found = false;
        bool test_mod_found = false;
        for (const auto &m : mods) {
                if (m.name == "basenodes")
                        default_found = true;
                if (m.name == "test_mod")
                        test_mod_found = true;

                // Verify if paths are not empty
                UASSERTEQ(bool, m.path.empty(), false);
        }

        UASSERTEQ(bool, default_found, true);
        UASSERTEQ(bool, test_mod_found, true);

        UASSERT(mods.front().name == "first_mod");
        UASSERT(mods.back().name == "last_mod");
}

void TestServerModManager::testGetModspec()
{
        auto sm = makeManager(m_worlddir);
        UASSERTEQ(const ModSpec *, sm.getModSpec("wrongmod"), NULL);
        UASSERT(sm.getModSpec("basenodes") != NULL);
}

void TestServerModManager::testGetModNamesWrongDir()
{
        auto sm = makeManager(m_worlddir + DIR_DELIM "..");
        std::vector<std::string> result;
        sm.getModNames(result);
        UASSERTEQ(bool, result.empty(), true);
}

void TestServerModManager::testGetModNames()
{
        auto sm = makeManager(m_worlddir);
        std::vector<std::string> result;
        sm.getModNames(result);
        UASSERTEQ(bool, result.empty(), false);
        UASSERT(std::find(result.begin(), result.end(), "basenodes") != result.end());
}

void TestServerModManager::testGetModMediaPathsWrongDir()
{
        auto sm = makeManager(m_worlddir + DIR_DELIM "..");
        std::vector<std::string> result;
        sm.getModsMediaPaths(result);
        UASSERTEQ(bool, result.empty(), true);
}

void TestServerModManager::testGetModMediaPaths()
{
        auto sm = makeManager(m_worlddir);
        std::vector<std::string> result;
        sm.getModsMediaPaths(result);
        UASSERTEQ(bool, result.empty(), false);

        // Test media overriding:
        // unittests depends on basenodes to override default_dirt.png,
        // thus the unittests texture path must come first in the returned media paths to take priority
        auto it = std::find(result.begin(), result.end(), sm.getModSpec("unittests")->path + DIR_DELIM + "textures");
        UASSERT(it != result.end());
        UASSERT(std::find(++it, result.end(), sm.getModSpec("basenodes")->path + DIR_DELIM + "textures") != result.end());
}

void TestServerModManager::testGamePathEnvVar()
{
        // Test stub for LUANTI_GAME_PATH environment variable handling.
        // NOTE: Test cases are outlined below. Enable once LUANTI_GAME_PATH
        //
        // Case 1: LUANTI_GAME_PATH set to a valid directory containing a game.
        //   auto test_game_dir = getTestTempDirectory() + DIR_DELIM + "test_game";
        //   Create a minimal game structure in test_game_dir with game.conf and init.lua
        //   setenv("LUANTI_GAME_PATH", test_game_dir.c_str(), 1);
        //   Verify: findSubgame() can locate the game via LUANTI_GAME_PATH
        //   unsetenv("LUANTI_GAME_PATH");
        //
        // Case 2: LUANTI_GAME_PATH set to a nonexistent directory.
        //   setenv("LUANTI_GAME_PATH", "/nonexistent/path", 1);
        //   Verify: no crash, falls back to default game search paths
        //   unsetenv("LUANTI_GAME_PATH");
        //
        // Case 3: LUANTI_GAME_PATH combined with world.mt gameid.
        //   Set LUANTI_GAME_PATH to a directory with an alternate version of devtest
        //   Verify: the correct game is resolved based on world.mt gameid + path priority
        //   unsetenv("LUANTI_GAME_PATH");
        //
        // This stub is a placeholder; actual implementation blocked on
        // LUANTI_GAME_PATH support in findSubgame() / ServerModManager.
}
