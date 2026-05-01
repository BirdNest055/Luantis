# AI Agent Instructions — Luanti-Secure Project

> **Purpose:** This file consolidates ALL instructions, conventions, rules, and guidelines that AI agents (like coding assistants) must follow when working on this project. Read this file before making any changes.
> **Last Updated:** 2026-05-02 | **Applicable Versions:** v7, v8, v9.x, v9.24, v9.50+, v9.56, and all future development

---

## 1. Test-Driven Development (TDD) — MANDATORY

**This is the single most important rule.** Every feature must be developed TDD-first.

### 1.1 TDD Workflow

1. **RED:** Write failing tests that define the expected behavior BEFORE any implementation code
2. **GREEN:** Write the minimum implementation code to make the tests pass
3. **REFACTOR:** Clean up the code while keeping tests green

### 1.2 Test File Locations

| Component | Test File | Location |
|-----------|-----------|----------|
| C++ security logic | `test_connection_security.cpp`, `test_connection_security_info.cpp` | `src/unittest/` |
| C++ GameUI | `test_gameui.cpp` | `src/unittest/` |
| C++ crypto | `test_encrypted_connection.cpp` | `src/unittest/` |
| Bash build script | `test_build_linux.sh` | Project root |
| Bash server script | `test_start_server.sh` | Project root |
| Bash encryption toggle | `test_encryption_toggle.sh` | Project root |
| C++ security score v9.9 | `test_security_score_v99.cpp` | `src/unittest/` |

### 1.3 C++ Test Conventions

- Inherit from `TestBase`
- Register with `TestManager::registerTestModule(this)` in constructor
- Use `UASSERT(condition)` and `UASSERTEQ(type, a, b)` macros
- Each test method must be declared in the class and listed in `runTests()`
- Static instance `g_test_instance` for auto-registration
- Add test source to `src/unittest/CMakeLists.txt`

### 1.4 Bash Test Conventions

- Use `assert_eq`, `assert_contains`, `assert_not_contains`, `assert_file_exists` helpers
- Each test is a function named `test_<descriptive_name>`
- Tests run with `set -uo pipefail` (NOT `set -e`)
- Count pass/fail and print summary

---

## 2. Coding Standards

### 2.1 C++ Conventions (Follow Luanti Upstream)

- **Language:** C++17
- **Indentation:** Tabs (8-column width for display)
- **Naming:**
  - Classes: `PascalCase` (e.g., `ConnectionSecurityInfo`)
  - Functions/Methods: `camelCase` (e.g., `isConnectionSecure`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `ENCRYPTION_AES_256_GCM`)
  - Member variables: `m_` prefix (e.g., `m_security_info`)
  - Static members: `s_` prefix (rare in this project)
  - Namespaces: `PascalCase` (e.g., `EncryptionConfig`)
- **Headers:** Use `#pragma once` as include guard
- **License header:** Every file must start with the Luanti SPDX header:
  ```cpp
  // Luanti
  // SPDX-License-Identifier: LGPL-2.1-or-later
  // Copyright (C) 2026 Luanti contributors
  ```
- **Includes:** Use double quotes for project headers, angle brackets for system/external
- **Comments:** Use `/** doc */` style for public API, `//` for inline comments

### 2.2 Lua Conventions

- **Indentation:** Tabs
- **Naming:** `snake_case` for variables and functions
- **License header:**
  ```lua
  -- Luanti
  -- Copyright (C) 2026 Luanti contributors
  -- SPDX-License-Identifier: LGPL-2.1-or-later
  ```
- **Settings access:** Use `core.settings:get("setting_name")` for reading, `core.settings:set(...)` for writing
- **Formspec:** Use `core.formspec_escape()` for all user-visible strings, `core.colorize()` for colored text
- **Internationalization:** Use `fgettext("string")` for all user-visible text

### 2.3 Bash Conventions

- **Shell:** Bash 4+ compatible
- **Strict mode:** `set -uo pipefail` (NEVER `set -e` — too many false positives with pipelines)
- **Colors:** Use ANSI-C quoting: `RED=$'\033[0;31m'` (NOT `RED='\033[0;31m'` — those print literally)
- **User input:** Use `read -rp "prompt: " variable` (NOT `tput` — crashes under certain conditions)
- **Error handling:** Use `|| exit 1` at every step that must succeed
- **Array handling:** Use parallel arrays via `_add_group()` helper (NOT bash indirect array expansion — crashes silently)
- **Platform detection:** Always detect distro/version before installing packages
  - Ubuntu 24.04+: use `libfreetype-dev` (NOT `libfreetype6-dev` — unmet deps)
- **CMake cache:** Use `sed 's/^[^=]*=//'` to extract values (NOT `cut -d: -f2-` — fails on stale cache)
- **Binary path:** Check `bin/` directory first (CMake outputs there, NOT `build/`)
- **Exit handling:** Always include a `read -rp "Press Enter to close"` at the end of interactive scripts so the terminal doesn't close on crash

---

## 3. Git Workflow

### 3.1 Branch Strategy

| Branch | Purpose | Base |
|--------|---------|------|
| `main` | Upstream Luanti 5.16.0-dev | Origin |
| `clawtest-upload` | Luanti-Secure development (previous) | `main` |
| `clawtest-v9.5` | v9.5–v9.6 development | `clawtest-upload` |
| `clawtest-v9.11` | v9.11 development | `clawtest-v9.10` |
| `clawtest-v9.24-fix-settingtypes-context` | v9.24 development | `clawtest-v9.23-log-toggle` |
| `clawtest-v9.44-voice-server-authority` | Voice chat server authority | Previous branch |
| `clawtest-v9.50-centralized-gui` | GUITheme centralized system (93+ constants) | `clawtest-v9.44-voice-server-authority` |
| `clawtest-v9.51-gui-theme-editor` | WYSIWYG GUITheme web editor (Svelte) | `clawtest-v9.50-centralized-gui` |
| `clawtest-v9.53-guitheme-test-fix` | GUITheme test improvements + drift detection | `clawtest-v9.51-gui-theme-editor` |
| `clawtest-v9.54-fix-todo-items` | TODO/FIXME resolution + compiler warning fixes (140+ items, 9 batches) | `clawtest-v9.53-guitheme-test-fix` |
| `clawtest-v9.55-fix-todo-items-2` | TODO/FIXME/HACK round 2 (124 items, batches 10-15, zero markers in src/) | `clawtest-v9.54-fix-todo-items` |
| `clawtest-v9.56-fix-todo-items-b16-21` | Code quality round 3 (120+ items, batches 16-21 — null safety, const-correctness, dead code, serialization, error handling) | `clawtest-v9.55-fix-todo-items-2` |
| Future: `clawtest-v9.X` | Next version | Previous version branch |

**Rules:**
- Branch names include the version: `clawtest-v9.3`, `clawtest-v9.5`, `clawtest-v9.7`, `clawtest-v9.11`
- Each version branch contains a self-contained, buildable state
- Never merge forward until the current version is stable and tested
- The current branch is `clawtest-v9.56-fix-todo-items-b16-21`

### 3.2 Commit Conventions

- Write clear, descriptive commit messages
- Group related changes together
- Reference the version/feature in the commit message
- Example: `Luanti-Secure v9.3 - Modular encryption toggle, interactive start scripts, version numbering`

### 3.3 Tags

- Git tags mark release versions: `clawtest-v9.6`, `clawtest-v9.7`, etc.
- Tags should be annotated: `git tag -a clawtest-v9.6 -m "v9.6: Portable build system, CI warnings tracked"`

### 3.4 Version Numbering

- `VERSION` file in project root contains just the number (e.g., "9.3")
- `CMakeLists.txt` sets `VERSION_EXTRA` to "v9.3" (used in `--version` output)
- Zip filenames use the pattern: `Luanti-Secure-v9.3.zip`
- When bumping version: update BOTH `VERSION` file AND `CMakeLists.txt` `VERSION_EXTRA`

---

## 4. Settings Architecture

### 4.1 Adding a New Setting

Follow this exact checklist:

1. **`src/defaultsettings.cpp`**: Add `settings->setDefault("name", "value")` in the correct section
2. **`builtin/settingtypes.txt`**: Add the setting definition with description in the correct section
3. **`minetest.conf.example`**: Add the setting with full documentation comment
4. **Read the setting:** In C++ use `g_settings->getBool("name")` or `g_settings->get("name")`; in Lua use `core.settings:get("name")`

### 4.2 Overlay Settings Pattern

For any new overlay/indicator, follow this pattern:

1. Add `bool show_*` flag in `GameUI::Flags` with a comment referencing the pattern
2. Add `show_*` setting in `defaultsettings.cpp` (default: typically `true` for new features)
3. Add it to `settingtypes.txt` under `[**HUD]`
4. Add it to `minetest.conf.example`
5. Check the flag in `GameUI::initFlags()` and `GameUI::update()`
6. Write TDD tests first

### 4.3 Runtime Settings Bridge

For passing data from C++ to the Lua settings dialog:

1. Define the setting in `defaultsettings.cpp` with a default like `"N/A"` or `"Not Connected"`
2. In C++ code, use `g_settings->set("setting_name", value)` to update at runtime
3. In Lua, use `core.settings:get("setting_name")` to read
4. These settings are **NOT saved to disk** — they represent transient connection state
5. Document this clearly with a comment like: `// Runtime settings — populated by client when connected, not saved to disk`

---

## 5. Security Feature Development Rules

### 5.1 Wire Protocol Changes

- **Forward compatibility is mandatory.** New fields must be optional so old clients/servers still work
- Use the pattern: wrap new fields in a try-catch or version check
- Example from `clientpackethandler.cpp`:
  ```cpp
  u8 security_flags = 0;
  try {
      *pkt >> security_flags;
  } catch (PacketError &e) {
      // Old server — no security flags, assume insecure
  }
  ```

### 5.2 Security Flags

- The `security_flags` byte is a bitfield — new flags must use currently-unused bits
- Never repurpose existing bit positions
- Document every flag bit in `connection_security.h` with a comment explaining its meaning

### 5.3 Security Info Integrity

- Security info displayed to the user must be **derived from the actual connection state**, not from user-configurable settings
- The server advertises what security it provides; the client should not allow users to "fake" a secure indicator
- Runtime settings (`security_info_*`) are read-only in the UI — users cannot modify them through the settings dialog

### 5.4 Encryption Toggle Policy

- The `EncryptionConfig` namespace in `encryption_config.h/cpp` is the **single point of truth** for encryption decisions
- All code that checks whether encryption should be active MUST use `EncryptionConfig::shouldEncrypt()` — do NOT read `g_settings->getBool("secure_connection")` directly in packet handlers
- When `shouldEncrypt()` returns false: SRP auth still runs (for password verification), but `encryption_state.disable()` is called instead of `initFromSRPSessionKey()`
- When `shouldEncrypt()` returns true: normal flow — SRP key is used to derive AES-256-GCM keys

---

## 6. Build System Notes

### 6.1 CMake Configuration

- Master file: `CMakeLists.txt` at project root
- Key options: `BUILD_CLIENT`, `BUILD_SERVER`, `BUILD_UNITTESTS`, `ENABLE_LTO`, `RUN_IN_PLACE`
- C++17 standard required
- Dependencies listed in `vcpkg.json`
- OpenSSL is REQUIRED (not optional) for the encryption module

### 6.2 Adding New Source Files

1. Create the `.h` and `.cpp` files in the appropriate directory
2. Add the `.cpp` to the corresponding `CMakeLists.txt` (e.g., `src/network/CMakeLists.txt`, `src/unittest/CMakeLists.txt`)
3. For test files, add to `src/unittest/CMakeLists.txt` in the test sources list

### 6.3 Dependency Management

- `libfreetype-dev` on Ubuntu 24.04+ (NOT `libfreetype6-dev`)
- The build script handles distro detection automatically
- OpenSSL is already in `vcpkg.json` for the crypto layer
- Build environment auto-detects `local-prefix/` via `build_env.sh` or `--local-prefix` flag (no hardcoded paths since v9.6)

---

## 7. Documentation Rules

### 7.1 Project Map (`docs/luanti-project-map.md`)

- This is the **living document** — it MUST be updated every time a file is added, renamed, or its purpose changes
- It contains ~1,460 lines covering every file in the project
- When adding a new file, add it to the correct section with purpose and key dependencies
- Update the "Last Updated" date and "Map Version" when making changes

### 7.2 AI Codebase Reference (`docs/ai-codebase-reference.md`)

- Read this file to understand the current state of all modifications
- It summarizes what changed in each version, the data flow, and key file descriptions
- Update it when adding new features or making significant changes

### 7.3 This File (`docs/ai-agent-instructions.md`)

- Contains ALL rules and guidelines that AI agents must follow
- Update when adding new conventions, patterns, or rules
- Always read this file before starting any work

---

## 8. Common Pitfalls & Fixes

| Pitfall | Fix |
|---------|-----|
| `set -e` causing silent exits | Use `set -uo pipefail` instead; use `|| exit 1` explicitly |
| Raw ANSI codes printing literally | Use ANSI-C quoting: `RED=$'\033[0;31m'` |
| `tput` crashing in scripts | Use `read -rp` for user input instead |
| Bash indirect array expansion crashing | Use parallel arrays via `_add_group()` helper |
| `libfreetype6-dev` unmet deps on Ubuntu 24.04 | Auto-detect Ubuntu version; use `libfreetype-dev` for 24.04+ |
| Hardcoded `/home/user/...` paths in build files | NEVER hardcode absolute paths — use auto-detection (`SCRIPT_DIR`, `LOCAL_PREFIX`, `CMAKE_PREFIX_PATH`) |
| Build fails on different PC due to path assumptions | Use `--local-prefix` flag or `build_env.sh` auto-detection |
| CMake cache stale false positives | Use `sed 's/^[^=]*=//'` not `cut -d: -f2-` |
| Binary not found after build | Check `bin/` first (CMake outputs there, not `build/`) |
| Script auto-installing deps without asking | Always show interactive menu first |
| `NC: unbound variable` on line 746 | Ensure all variables used under `set -u` are defined in all code paths |
| False build success after failure | Add `|| exit 1` at every step |
| Server start script closes at end | Add `read -rp "Press Enter to close"` at end of script |
| Insecure mode still encrypts | Use `EncryptionConfig::shouldEncrypt()` — don't read settings directly |
| No version numbers in zips | Use `Luanti-Secure-v9.X.zip` naming pattern |
| `i64` type not declared in crypto.h | Use `s64` instead — the project uses `s64` (from irrTypes.h), not `i64` |
| `populateRealSecurityInfo` too many arguments | 11-param overload MUST be defined BEFORE the 10-param overload (C++ forward declaration issue) |
| Random HKDF salt causes key mismatch | Salt MUST be derived deterministically from SRP session key, not generated with `secure_random()` — both sides need the same salt |
| `[server,client]` context in settingtypes.txt | Luanti's settingtypes parser (`builtin/common/settings/settingtypes.lua`) only accepts SINGLE context values: `common`, `client`, `server`, `world_creation`. Use `[common]` for settings that apply to both server and client — NEVER use comma-separated contexts like `[server,client]` |
| Encryption log spam generates 180MB files | Use `--no-log` (default) in start scripts to prevent debug.txt creation. Use `encryption_log_level = none` to suppress all encryption log messages. Never set `encryption_log_level = trace` for normal gameplay — it generates per-packet diagnostics |

---

## 9. File Checklist for New Features

When adding a new feature to this project, ensure ALL of these are done:

- [ ] TDD tests written FIRST (RED phase)
- [ ] Implementation code written to pass tests (GREEN phase)
- [ ] Code refactored while keeping tests green (REFACTOR phase)
- [ ] `defaultsettings.cpp` updated (if new settings needed)
- [ ] `settingtypes.txt` updated (if new settings needed)
- [ ] `minetest.conf.example` updated (if new settings needed)
- [ ] `src/unittest/CMakeLists.txt` updated (if new test files added)
- [ ] `src/network/CMakeLists.txt` updated (if new network source files added)
- [ ] `docs/luanti-project-map.md` updated (file list, dependencies)
- [ ] `docs/ai-codebase-reference.md` updated (new feature summary)
- [ ] `VERSION` file updated (version bump)
- [ ] `CMakeLists.txt` `VERSION_EXTRA` updated (version bump)
- [ ] `docs/ENCRYPTION_DATA_FLOW.md` updated (if encryption behavior changes)
- [ ] `docs/V9_PLAN.md` updated (progress tracker and feature descriptions)
- [ ] New test files registered in `src/unittest/CMakeLists.txt`
- [ ] Git commit with descriptive message referencing the version

---

## 10. Repository Information

- **GitHub URL:** https://github.com/BirdNest055/Luantis
- **Current branch:** `clawtest-v9.56-fix-todo-items-b16-21`
- **Previous branch:** `clawtest-v9.55-fix-todo-items-2`
- **Upstream Luanti:** https://github.com/luanti-org/luanti (version 5.16.0-dev)
- **License:** LGPL 2.1 (same as upstream Luanti)
- **Current version:** v9.56

---

## 11. GUITheme System (v9.50+)

### 11.1 Overview

The GUITheme system provides centralized GUI styling constants in `src/gui/GUITheme.h` and `src/gui/GUITheme.cpp`. It replaces scattered hardcoded values across 14+ GUI files.

### 11.2 Namespaces

| Namespace | Count | Purpose |
|-----------|-------|---------|
| `GUITheme::Colors` | 35 | All GUI colors (backgrounds, text, borders) |
| `GUITheme::Sizing` | 42 | Dimensions, padding, margins, radii |
| `GUITheme::Timing` | 11 | Animation durations, fade times |
| `GUITheme::ButtonModifiers` | 2 | Button state multipliers |
| `GUITheme::Fonts` | 8 | Font paths and sizes |
| `GUITheme::Sounds` | 1 | UI sound effect paths |
| `GUITheme::Dialogs` | 6 | Dialog dimensions and positions |
| `GUITheme::validate()` | - | Runtime validation (drift detection) |

### 11.3 Rules

- ALL GUI styling constants MUST go in `GUITheme.h` — never hardcode in individual GUI files
- Use `GUITheme::Colors::BUTTON_BG` instead of `video::SColor(255, 60, 60, 60)` etc.
- Run `GUITheme::validate()` in debug builds to catch drift (constants that don't match their intended values)
- 14 GUI files have been refactored to use GUITheme constants
- 89 unit tests in `gui_theme_test/gui_theme_test.cpp` cover all constants

### 11.4 WYSIWYG Editor

A Svelte-based web editor lives in `gui-theme-editor/`:
- Live preview of formspec elements
- Import/export of `GUITheme.h` files
- Preset themes (Default, Dark, Light, Compact, Retro)
- Run with `npm run dev` from `gui-theme-editor/`
