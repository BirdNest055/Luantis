// Luanti-Secure
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti-Secure contributors
//
// v9.38: Unit tests for server info overlay features.
// - SERVERINFO key type exists in the KeyType enum
// - keymap_serverinfo default setting

#include "test.h"

#include "client/keys.h"
#include "settings.h"

class TestServerInfoOverlay : public TestBase
{
public:
	TestServerInfoOverlay() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestServerInfoOverlay"; }

	void runTests(IGameDef *gamedef) override
	{
		TEST(testServerInfoKeyTypeExists);
		TEST(testKeymapServerinfoSetting);
	}

	void testServerInfoKeyTypeExists()
	{
		// The SERVERINFO key type must exist in the enum
		UASSERT(KeyType::SERVERINFO > KeyType::SCREENSHOT);
		UASSERT(KeyType::SERVERINFO < KeyType::TOGGLE_BLOCK_BOUNDS);
		// Must be a valid index within the enum
		UASSERT(KeyType::SERVERINFO < KeyType::INTERNAL_ENUM_COUNT);
	}

	void testKeymapServerinfoSetting()
	{
		// The default setting for keymap_serverinfo should be KEY_TAB (scancode 15)
		std::string val = g_settings->get("keymap_serverinfo");
		UASSERT(!val.empty());
		// The default should reference SYSTEM_SCANCODE_15 (Tab key)
		UASSERT(val.find("SYSTEM_SCANCODE_15") != std::string::npos);
	}
};

static TestServerInfoOverlay g_test_instance;
