// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 sfan5 <sfan5@live.de>

#include "test.h"

#include <string>
#include "exceptions.h"
#include "client/keycode.h"

class TestKeycode : public TestBase {
public:
        TestKeycode() { TestManager::registerTestModule(this); }
        const char *getName() { return "TestKeycode"; }

        void runTests(IGameDef *gamedef);

        /* NOTE: Re-introduce unittests after fully switching to SDL.
         * The old tests relied on Irrlicht-specific keycodes (KEY_KEY_W, etc.)
         * which are no longer valid under SDL input handling.
         *
         * Why they're disabled: The SDL migration changed the key representation
         * from Irrlicht's EKEY_CODE enum (KEY_KEY_W, KEY_RSHIFT, etc.) to SDL's
         * SDL_Keycode system (SDLK_w, SDLK_RSHIFT, etc.). The KeyPress class was
         * updated to use SDL key names internally, but the test data still references
         * Irrlicht identifiers. Running the tests with SDL would produce incorrect
         * results because:
         *   - KeyPress("R") no longer maps to "KEY_KEY_R" (now "KEY_R" or SDL name)
         *   - SEvent::SKeyInput no longer has .Key/.Char fields (replaced by SDL_Event)
         *   - KeyPress comparison semantics changed (SDL uses scancodes + keycodes)
         *
         * To re-enable:
         * 1. Update testCreateFromString() to use SDL key names ("KEY_W", etc.)
         *    instead of Irrlicht names ("KEY_KEY_W")
         * 2. Update testCreateFromSKeyInput() to use SDL_KeyboardEvent fields
         *    (keysym.scancode, keysym.sym) instead of SEvent::SKeyInput
         * 3. Update testCompare() for the new KeyPress comparison semantics
         *    (SDL scancode-based matching instead of Irrlicht keycode matching)
         * 4. Remove the #if 0 guards once all three tests pass
         * 5. Consider adding a test for SDL_SCANCODE_TO_KEYCODE() equivalence
        void testCreateFromString();
        void testCreateFromSKeyInput();
        void testCompare();
        */
};

static TestKeycode g_test_instance;

void TestKeycode::runTests(IGameDef *gamedef)
{
        /*
        TEST(testCreateFromString);
        TEST(testCreateFromSKeyInput);
        TEST(testCompare);
        */
}

#if 0

////////////////////////////////////////////////////////////////////////////////

#define UASSERTEQ_STR(one, two) UASSERT(strcmp(one, two) == 0)

void TestKeycode::testCreateFromString()
{
        KeyPress k;

        // Character key, from char
        k = KeyPress("R");
        UASSERTEQ_STR(k.sym(), "KEY_KEY_R");
        UASSERTCMP(int, >, strlen(k.name()), 0); // should have human description

        // Character key, from identifier
        k = KeyPress("KEY_KEY_B");
        UASSERTEQ_STR(k.sym(), "KEY_KEY_B");
        UASSERTCMP(int, >, strlen(k.name()), 0);

        // Non-Character key, from identifier
        k = KeyPress("KEY_UP");
        UASSERTEQ_STR(k.sym(), "KEY_UP");
        UASSERTCMP(int, >, strlen(k.name()), 0);

        k = KeyPress("KEY_F6");
        UASSERTEQ_STR(k.sym(), "KEY_F6");
        UASSERTCMP(int, >, strlen(k.name()), 0);

        // Irrlicht-unknown key, from char
        k = KeyPress("/");
        UASSERTEQ_STR(k.sym(), "/");
        UASSERTCMP(int, >, strlen(k.name()), 0);
}

void TestKeycode::testCreateFromSKeyInput()
{
        KeyPress k;
        SEvent::SKeyInput in;

        // Character key
        in.Key = KEY_KEY_3;
        in.Char = L'3';
        k = KeyPress(in);
        UASSERTEQ_STR(k.sym(), "KEY_KEY_3");

        // Non-Character key
        in.Key = KEY_RSHIFT;
        in.Char = L'\0';
        k = KeyPress(in);
        UASSERTEQ_STR(k.sym(), "KEY_RSHIFT");

        // Irrlicht-unknown key
        in.Key = KEY_KEY_CODES_COUNT;
        in.Char = L'?';
        k = KeyPress(in);
        UASSERTEQ_STR(k.sym(), "?");

        // prefer_character mode
        in.Key = KEY_COMMA;
        in.Char = L'G';
        k = KeyPress(in, true);
        UASSERTEQ_STR(k.sym(), "KEY_KEY_G");
}

void TestKeycode::testCompare()
{
        // Basic comparison
        UASSERT(KeyPress("5") == KeyPress("KEY_KEY_5"));
        UASSERT(!(KeyPress("5") == KeyPress("KEY_NUMPAD5")));

        // Matching char suffices
        // note: This is a real-world example, Irrlicht maps XK_equal to KEY_PLUS on Linux
        SEvent::SKeyInput in;
        in.Key = KEY_PLUS;
        in.Char = L'=';
        UASSERT(KeyPress("=") == KeyPress(in));

        // Matching keycode suffices
        SEvent::SKeyInput in2;
        in.Key = in2.Key = KEY_OEM_CLEAR;
        in.Char = L'\0';
        in2.Char = L';';
        UASSERT(KeyPress(in) == KeyPress(in2));
}

#endif
