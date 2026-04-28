# Luanti-Secure Project — Complete File & Dependency Map

> **Repository:** [BirdNest055/Clawtest](https://github.com/BirdNest055/Clawtest) (fork of Luanti with real encrypted communications)
> **Version:** 5.16.1-v9.3 | **Language:** C++17 + Lua 5.x | **License:** LGPL 2.1
> **Total Source Files:** ~330 C++ (.h/.cpp) + ~120 Lua (.lua) + 79 IrrlichtMt C++ + GLSL Shaders + 12 Crypto layer (integrated)
> **Last Updated:** 2026-04-25 | **Map Version:** v4

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Layer Diagram](#2-layer-diagram)
3. [Top Hub Files (Most-Included)](#3-top-hub-files-most-included)
4. [Root-Level Files](#4-root-level-files)
5. [src/ — Core C++ Engine](#5-src--core-c-engine)
6. [src/client/ — Client Engine](#6-srcclient--client-engine)
7. [src/server/ — Server Logic](#7-srcserver--server-logic)
8. [src/network/ — Networking](#8-srcnetwork--networking)
9. [src/mapgen/ — Map Generation](#9-srcmapgen--map-generation)
10. [src/database/ — Persistence](#10-srcdatabase--persistence)
11. [src/script/ — Scripting System](#11-srcscript--scripting-system)
12. [src/gui/ — GUI System](#12-srcgui--gui-system)
13. [src/util/ — Utility Library](#13-srcutil--utility-library)
14. [src/threading/ — Threading Primitives](#14-srcthreading--threading-primitives)
15. [src/content/ — Content/Mod Management](#15-srccontent--contentmod-management)
16. [src/benchmark/ & src/test/](#16-srcbenchmark--srctest)
17. [builtin/ — Lua Scripts](#17builtin--lua-scripts)
18. [client/ — Runtime Shaders](#18client--runtime-shaders)
19. [irr/ — IrrlichtMt Engine](#19irr--irrlichtmt-engine)
20. [lib/ — Vendored Libraries](#20lib--vendored-libraries)
21. [games/ — Game Packages](#21games--game-packages)
22. [Other Directories](#22-other-directories)
23. [Cross-Subsystem Dependency Patterns](#23-cross-subsystem-dependency-patterns)
24. [C++ ↔ Lua Bridge](#24-c-lua-bridge)
25. [Build System](#25-build-system)
26. [Git Branch Strategy & Project Workflow](#26-git-branch-strategy--project-workflow)

---

## 1. Architecture Overview

Luanti is a **voxel-based game engine** with a **client-server architecture**. The server manages the world state (map, entities, players, mods) and the client handles rendering, input, and audio. They communicate over a custom UDP-based protocol (MTProtocol). Both sides embed a **Lua scripting engine** for modding — the server runs the full game API, the client runs a restricted CSM (Client-Side Mod) API.

**Core subsystems:**

| Subsystem | Location | Role |
|-----------|----------|------|
| Server | `src/server/`, `src/serverenvironment.h` | World simulation, mod execution, player management |
| Client | `src/client/` | Rendering, input, audio, network client |
| Map System | `src/map.h`, `src/servermap.h`, `src/client/clientmap.h` | Voxel world storage (16³ blocks) |
| Map Generation | `src/mapgen/` | Procedural terrain (8 biome algorithms) |
| Networking | `src/network/` | MTProtocol over UDP, packet serialization |
| Encryption | `src/network/encryption_config.h/cpp`, `src/network/crypto.h/cpp`, `src/network/encrypted_connection.h/cpp`, `src/network/crypto/` | Real AES-256-GCM encryption via SRP session key + modular toggle (v9.3) |
| Scripting | `src/script/` | C++ ↔ Lua bridge (42 Lua API modules) |
| GUI | `src/gui/` | Formspec-based UI, dialogs, main menu |
| Database | `src/database/` | Map/player storage (SQLite3, PostgreSQL, LevelDB, Redis) |
| Rendering | `src/client/render/`, `src/client/shadows/`, `client/shaders/` | OpenGL pipeline, shadows, post-processing |
| Sound | `src/client/sound/` | OpenAL-based audio system |
| IrrlichtMt | `irr/` | Forked Irrlicht 3D rendering engine |

---

## 2. Layer Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  main.cpp                                                        │
│  clientlauncher.cpp                                              │  Application
├──────────────────────────────────────────────────────────────────┤
│  server.h/cpp              │  client/client.h/cpp               │  Core Hubs
│  serverenvironment.h/cpp   │  client/clientenvironment.h/cpp    │
├──────────────────────────────────────────────────────────────────┤
│  scripting_server           │  scripting_client                  │  Script System
│  script/lua_api/l_*        │  script/lua_api/l_client*          │
├──────────────────────────────────────────────────────────────────┤
│  servermap    │ clientmap    │ mapgen/*    │ network/*           │  Subsystems
│  database/*   │ gui/*        │ emerge      │ modchannels         │
├──────────────────────────────────────────────────────────────────┤
│  map.h → mapblock.h → mapnode.h → voxel.h → voxelalgorithms.h  │  World Data
│  nodedef.h   │ itemdef.h    │ craftdef.h  │ inventory.h         │  Content Defs
├──────────────────────────────────────────────────────────────────┤
│  settings.h  │ filesys.h    │ porting.h   │ log.h    │ debug.h  │  Infrastructure
│  util/*      │ threading/*  │ irrlicht_changes/*                │
├──────────────────────────────────────────────────────────────────┤
│  irrlichttypes.h  │ irr_v3d.h  │ irr_aabb3d.h  │ constants.h   │  Foundation
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. Top Hub Files (Most-Included)

These headers form the backbone of the codebase — included by the most other files:

| Rank | Header | Included By | Role |
|------|--------|-------------|------|
| 1 | `settings.h` | 102 files | Global configuration system |
| 2 | `log.h` | 95 files | Logging — universal dependency |
| 3 | `irrlichttypes.h` | 76 files | Base Irrlicht types (u8, u16, f32, etc.) |
| 4 | `util/string.h` | 75 files | String utilities — parsing/formatting |
| 5 | `porting.h` | 74 files | Platform abstraction — OS-specific code |
| 6 | `util/numeric.h` | 68 files | Math/numeric utilities |
| 7 | `nodedef.h` | 66 files | Node definitions — core content system |
| 8 | `filesys.h` | 55 files | Filesystem operations |
| 9 | `server.h` | 47 files | Server class — central server-side hub |
| 10 | `irrlichttypes_bloated.h` | 46 files | Extended Irrlicht types (v2d, v3d, aabb, SColor) |
| 11 | `irr_v3d.h` | 46 files | 3D vector type |
| 12 | `exceptions.h` | 46 files | Custom exceptions |
| 13 | `config.h` | 45 files | Build configuration |
| 14 | `util/serialize.h` | 42 files | Binary serialization — network/disk I/O |
| 15 | `map.h` | 39 files | Core Map class |
| 16 | `debug.h` | 39 files | Debug macros and assertions |
| 17 | `noise.h` | 36 files | Noise/RNG — mapgen and rendering |
| 18 | `lua_api/l_base.h` | 36 files | Base Lua API binding |
| 19 | `constants.h` | 36 files | Engine constants |
| 20 | `client/client.h` | 36 files | Client class — central client-side hub |

---

## 4. Root-Level Files

| File | Purpose | Connects To |
|------|---------|-------------|
| `CMakeLists.txt` | Master build file. Version 5.16.1-v9.3, C++17. Options: BUILD_CLIENT, BUILD_SERVER, BUILD_UNITTESTS, ENABLE_LTO, RUN_IN_PLACE, BUILD_WITH_TRACY. OpenSSL REQUIRED. | All subdirectories |
| `CMakePresets.json` | CMake preset configurations | `CMakeLists.txt` |
| `vcpkg.json` | vcpkg dependency manifest: zlib, zstd, openssl, curl, openal-soft, libvorbis, libogg, libjpeg-turbo, sqlite3, freetype, luajit, gmp, jsoncpp, gettext, sdl2 | `CMakeLists.txt` |
| `VERSION` | Luanti-Secure version file (currently "9.3") | `CMakeLists.txt` |
| `minetest.conf.example` | Example configuration with all settings documented | `src/defaultsettings.cpp`, `builtin/settingtypes.txt` |
| `README.md` | Project readme — Luanti-Secure fork with encrypted comms | — |
| `LICENSE.txt` / `COPYING.LESSER` | LGPL 2.1 license | — |
| `Dockerfile` | Docker container for server deployment | `CMakeLists.txt` (build), `src/main.cpp` (run) |
| `shell.nix` | Nix shell for reproducible development | — |
| `build_linux.sh` | Fully automated Linux build script (Debian/Fedora/Arch/Alpine/openSUSE) | `CMakeLists.txt`, `vcpkg.json` |
| `build_env.sh` | Build environment setup (local prefix, CMake flags) | `build_linux.sh` |
| `start_server.sh` | Interactive server start script with secure/insecure mode selection | `bin/luantiserver` |
| `start_client.sh` | Interactive client start script with secure/insecure mode selection | `bin/luanti` |
| `test_build_linux.sh` | TDD test suite for build_linux.sh (53 tests) | `build_linux.sh`, `CMakeLists.txt` |
| `test_encryption_toggle.sh` | TDD test suite for encryption toggle (14 tests) | `bin/luantiserver`, `bin/luanti` |
| `docs/V9_PLAN.md` | v9 encryption implementation plan with progress tracker | — |
| `docs/ai-agent-instructions.md` | Conventions and rules for AI agents | — |
| `docs/ai-codebase-reference.md` | Current codebase state summary | — |
| `docs/TODO_FIXME_LIST.md` | Auto-generated list of TODO/FIXME/HACK comments | — |
| `docs/OPENSECURE_GUIDE.md` | OpenSecure CLI guide | — |
| `docs/ENCRYPTION_DATA_FLOW.md` | Comprehensive encryption data flow guide | — |
| `docs/GUI_EDITING_GUIDE.md` | GUI editing reference | — |

---

## 5. src/ — Core C++ Engine

The heart of the engine. 66 files in the root of `src/`.

### 5.1 Entry Point & Application

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `main.cpp` | **Application entry point** — parses args, launches server or client | `server.h`, `client/clientlauncher.h`, `settings.h`, `porting.h`, `gettext.h`, `filesys.h` |
| `defaultsettings.h/cpp` | Initializes all default settings | `settings.h`, `porting.h`, `server.h` |

### 5.2 Core World Data

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `map.h/cpp` | **Core Map class** — voxel world storage, block loading, lighting | `irrlichttypes_bloated.h`, `mapblock.h`, `mapnode.h`, `voxel.h`, `nodedef.h`, `gamedef.h` |
| `mapblock.h/cpp` | MapBlock (16³ voxel chunk) — the fundamental world unit | `irr_v3d.h`, `mapnode.h`, `staticobject.h`, `nodemetadata.h`, `nodetimer.h` |
| `mapnode.h/cpp` | MapNode — single voxel (content ID + light + param2) | `irrlichttypes_bloated.h`, `light.h`, `nodedef.h` |
| `mapsector.h/cpp` | MapSector (16×16 blocks in XZ plane) | `irr_v2d.h`, `mapblock.h` |
| `servermap.h/cpp` | ServerMap — persistent map with database backend | `map.h`, `util/container.h`, `map_settings_manager.h`, `database/database.h` |
| `voxel.h/cpp` | VoxelManipulator — bulk voxel editing for mapgen | `irrlichttypes.h`, `mapnode.h`, `exceptions.h` |
| `voxelalgorithms.h/cpp` | Voxel algorithms (lighting, flood fill, blur) | `mapnode.h`, `mapblock.h`, `map.h` |

### 5.3 Content Definitions

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `nodedef.h/cpp` | **Node definition system** — ContentFeatures, draw types, tiles | `mapnode.h`, `nameidmapping.h`, `itemgroup.h`, `tileanimation.h`, `sound_spec.h` |
| `itemdef.h/cpp` | Item definition system — ItemStack features | `irrlichttypes_bloated.h`, `itemgroup.h`, `sound_spec.h`, `tool.h`, `tileanimation.h` |
| `craftdef.h/cpp` | Crafting system definitions — recipes, groups | `gamedef.h`, `inventory.h`, `itemdef.h` |
| `inventory.h/cpp` | Inventory and ItemStack system | `irrlichttypes.h`, `itemstackmetadata.h`, `itemdef.h` |
| `inventorymanager.h/cpp` | Inventory location and management | `irr_v3d.h`, `serverenvironment.h` |
| `tool.h/cpp` | Tool capabilities and dig groups | `irrlichttypes.h`, `itemgroup.h` |
| `itemgroup.h` | Item group mapping (string → int) | `<string>`, `<unordered_map>` |
| `itemstackmetadata.h/cpp` | ItemStack metadata extension | `metadata.h`, `tool.h` |
| `metadata.h/cpp` | Generic key-value metadata | `util/string.h` |
| `nodemetadata.h/cpp` | Node metadata (inventories on nodes) | `metadata.h` |
| `nodetimer.h/cpp` | Node timer system | `irr_v3d.h` |
| `content_mapnode.h/cpp` | Legacy map node content registration | `mapnode.h`, `nodedef.h` |
| `content_nodemeta.h/cpp` | Legacy node metadata registration | `nodemetadata.h`, `inventory.h` |

### 5.4 Environment & Players

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `environment.h/cpp` | Base environment class (world state container) | `irr_v3d.h`, `util/basic_macros.h` |
| `serverenvironment.h/cpp` | Server-side environment — manages SAOs, time, weather | `environment.h`, `map.h`, `server/activeobjectmgr.h`, `emerge.h` |
| `player.h/cpp` | Base Player class | `irrlichttypes_bloated.h`, `inventory.h`, `hud_element.h` |
| `remoteplayer.h/cpp` | Remote player (server-side representation) | `player.h`, `skyparams.h`, `networkprotocol.h` |
| `activeobject.h` | Base class for all active objects in the world | `irr_aabb3d.h`, `irr_v3d.h` |
| `activeobjectmgr.h` | Base active object manager template | `util/container.h`, `irrlichttypes.h` |
| `staticobject.h/cpp` | Static object storage (in mapblocks) | `irrlichttypes_bloated.h`, `debug.h`, `log.h` |
| `object_properties.h/cpp` | Object property serialization (visual, collision, etc.) | `irrlichttypes_bloated.h`, `mapnode.h` |

### 5.5 Core Systems

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `server.h/cpp` | **Main Server class** — the central hub of server logic | `map.h`, `gamedef.h`, `server/clientiface.h`, `serverenvironment.h`, `network/connection.h`, `scripting_server.h`, `mods.h`, +many more |
| `emerge.h/cpp` | Map emerge (chunk loading/generation) interface | `map.h`, `mapgen/mapgen.h`, `networkprotocol.h` |
| `emerge_internal.h` | Emerge thread internals | `emerge.h`, `util/thread.h`, `threading/event.h` |
| `gamedef.h` | IGameDef interface — core game definition | `irrlichttypes.h` |
| `gameparams.h` | Game parameters struct | `content/subgames.h` |
| `collision.h/cpp` | Collision detection engine | `irrlichttypes_bloated.h`, `mapblock.h`, `nodedef.h` |
| `pathfinder.h/cpp` | A* pathfinding on voxel grid | `irr_v3d.h`, `map.h`, `nodedef.h` |
| `raycast.h/cpp` | Ray casting for pointing/selection | `voxelalgorithms.h`, `util/pointedthing.h` |
| `reflowscan.h/cpp` | Liquid reflow scanning | `util/container.h`, `irr_v3d.h` |
| `particles.h/cpp` | Particle spawning definitions | `irrlichttypes_bloated.h`, `tileanimation.h`, `mapnode.h` |
| `modchannels.h/cpp` | Mod channel communication | `networkprotocol.h`, `irrlichttypes.h` |
| `rollback_interface.h/cpp` | Rollback (undo) system interface | `irr_v3d.h`, `inventory.h` |
| `daynightratio.h` | Day/night light ratio constants | (none) |
| `hud_element.h/cpp` | HUD element definitions | `irrlichttypes_bloated.h`, `util/enum_string.h` |
| `skyparams.h` | Sky rendering parameters | `<SColor.h>`, `irr_v2d.h` |
| `light.h/cpp` | Light level calculations | `config.h`, `irrlichttypes.h` |
| `lighting.h` | Lighting state struct | `<SColor.h>`, `irr_v3d.h` |
| `sound_spec.h/cpp` | Sound specification structs | `irrlichttypes_bloated.h` |

### 5.6 Infrastructure

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `settings.h/cpp` | Settings/INI configuration system — **#1 most-included header** | `irrlichttypes_bloated.h`, `util/string.h` |
| `filesys.h/cpp` | Filesystem abstraction layer | `config.h`, `util/string.h` |
| `porting.h/cpp` | Platform abstraction (OS, filesystem, signals) | `config.h`, `debug.h`, `irrlichttypes.h` |
| `porting_android.h/cpp` | Android-specific platform code | `irrlichttypes_bloated.h`, `<jni.h>` |
| `log.h/cpp` | Logging system — **#2 most-included** | `util/basic_macros.h`, `util/stream.h` |
| `log_internal.h` | Internal logging implementation details | `threading/mutex_auto_lock.h`, `irrlichttypes.h` |
| `debug.h/cpp` | Debug/assert macros, error handling | `log.h`, `exceptions.h` |
| `exceptions.h` | Custom exception classes | `<exception>` |
| `serialization.h/cpp` | Serialization format versioning | `irrlichttypes.h`, `util/serialize.h`, `<zlib.h>` |
| `profiler.h/cpp` | Performance profiler | `irrlichttypes.h`, `threading/mutex_auto_lock.h` |
| `gettext.h/cpp` | Internationalization (i18n) wrapper | `config.h`, `porting.h` |
| `gettext_plural_form.h/cpp` | Plural form handling for i18n | `<string_view>`, `<functional>` |
| `translation.h/cpp` | Translation file manager | `gettext_plural_form.h` |
| `version.h/cpp` | Version string constants | `config.h` |
| `config.h` | Build config defines (auto-generated) | (none) |
| `constants.h` | Game engine constants (BS, MAP_BLOCKSIZE, etc.) | (none) |
| `convert_json.h/cpp` | JSON conversion utilities | `<json/json.h>` |
| `httpfetch.h/cpp` | HTTP request system (cURL-based) | `util/string.h`, `util/container.h`, `<curl/curl.h>` |

### 5.7 Irrlicht Type Wrappers

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `irrlichttypes.h` | Base Irrlicht type includes (u8, u16, f32, etc.) | `<cstdint>`, `<irrTypes.h>` |
| `irrlichttypes_bloated.h` | Extended Irrlicht types (aggregates v2d, v3d, aabb, color) | `irrlichttypes.h`, `irr_v2d.h`, `irr_v3d.h`, `irr_aabb3d.h` |
| `irr_v2d.h` | Irrlicht 2D vector wrapper | `irrlichttypes.h`, `<vector2d.h>` |
| `irr_v3d.h` | Irrlicht 3D vector wrapper | `irrlichttypes.h`, `<vector3d.h>` |
| `irr_aabb3d.h` | Irrlicht AABB3D wrapper | `irrlichttypes.h`, `<aabbox3d.h>` |
| `irr_gui_ptr.h` | Shared ptr for IGUIElement | `<memory>`, `<IGUIElement.h>` |

### 5.8 Other Root Files

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `chat.h/cpp` | Chat message formatting and history | `irrlichttypes.h`, `util/enriched_string.h` |
| `chat_interface.h` | Chat interface (queue of chat messages) | `irrlichttypes.h`, `util/container.h` |
| `chatmessage.h` | Simple chat message struct | `<string>`, `<ctime>` |
| `clientdynamicinfo.h/cpp` | Client display/dynamic info | `irrlichttypes_bloated.h`, `settings.h` |
| `dummygamedef.h` | Mock IGameDef for testing | `gamedef.h`, `nodedef.h`, `craftdef.h`, `database-dummy.h` |
| `dummymap.h` | Mock Map for testing | `map.h`, `mapsector.h` |
| `face_position_cache.h/cpp` | Cache for face position lookups | `irr_v3d.h` |
| `json-forwards.h` | JSON forward declarations | `<json/forwards.h>` |
| `map_settings_manager.h/cpp` | Map settings management | `settings.h` |
| `migratesettings.h` | Settings migration definitions | `settings.h`, `server.h` |
| `modifiedstate.h` | Modified state enum | `irrlichttypes.h` |
| `nameidmapping.h/cpp` | Name↔ID bidirectional mapping | `irrlichttypes.h` |
| `noise.h/cpp` | Noise generation (Perlin, PcgRandom) | `irr_v3d.h`, `exceptions.h`, `util/string.h` |
| `objdef.h/cpp` | Object definition base (for mapgen) | `irrlichttypes.h` |
| `terminal_chat_console.h/cpp` | Terminal-based chat (ncurses) | `chat.h`, `threading/thread.h` |
| `texture_override.h/cpp` | Texture override definitions | `irrlichttypes.h` |
| `tileanimation.h/cpp` | Tile animation parameters | `irrlichttypes_bloated.h` |

---

## 6. src/client/ — Client Engine

43 files handling rendering, input, audio, and the game loop.

### 6.1 Core Client

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `client.h/cpp` | **Main Client class** — network client, world reception, media download | `clientenvironment.h`, `gamedef.h`, `network/address.h`, `networkprotocol.h`, `server.h` |
| `clientenvironment.h/cpp` | Client-side environment | `environment.h`, `activeobjectmgr.h` |
| `clientlauncher.h/cpp` | Client startup/launcher | `<string>`, `renderingengine.h` |
| `clientmap.h/cpp` | Client-side map (rendering) — extends Map with mesh management | `map.h`, `<ISceneNode.h>` |
| `game.h/cpp` | **Main game loop** — the biggest .cpp file, handles all in-game logic | `config.h`, `client.h`, `camera.h`, `hud.h`, `gui/guiFormSpecMenu.h` |
| `game_internal.h` | Game loop internal state struct | `game.h`, `camera.h`, `client.h`, `sky.h` |
| `gameui.h/cpp` | Game UI overlay (HUD, chat) | `game.h`, `gui/statusTextHelper.h` |
| `game_formspec.h/cpp` | Formspec dialog handler | `irr_v3d.h`, `scripting_pause_menu.h` |
| `clientevent.h` | Client event definitions | `hud_element.h` |
| `clientmedia.h/cpp` | Media (texture/sound) downloader | `filecache.h`, `util/basic_macros.h` |
| `mod_vfs.h/cpp` | Client mod virtual filesystem | `<string>`, `<unordered_map>` |

### 6.2 Rendering

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `renderingengine.h/cpp` | Rendering engine abstraction — device, driver, window | `inputhandler.h`, `shader.h`, `render/core.h`, `shadows/dynamicshadowsrender.h` |
| `camera.h/cpp` | Camera controller (1st/3rd person, zoom) | `inventory.h`, `util/numeric.h`, `localplayer.h` |
| `sky.h/cpp` | Sky rendering scene node | `skyparams.h`, `camera.h`, `<ISceneNode.h>` |
| `clouds.h/cpp` | Cloud rendering scene node | `skyparams.h`, `<ISceneNode.h>` |
| `shader.h/cpp` | Shader loading/compilation — bridges C++ uniforms to GLSL | `nodedef.h`, `tile.h`, `irr_ptr.h` |
| `tile.h/cpp` | Tile (material/texture) definitions | `irrlichttypes.h`, `<SMaterial.h>` |
| `texturesource.h/cpp` | Texture loading/caching source | `irrlichttypes.h`, `<SColor.h>` |
| `texturepaths.h/cpp` | Texture path resolution | `<string>`, `<vector>` |
| `imagesource.h/cpp` | Image loading/compositing source | `<IImage.h>` |
| `imagefilters.h/cpp` | Image processing filters | `irrlichttypes.h`, `<SColor.h>` |
| `mesh.h/cpp` | Mesh utility functions | `irrlichttypes_bloated.h` |
| `mapblock_mesh.h/cpp` | MapBlock mesh (rendering data for terrain) | `irr_ptr.h`, `tile.h`, `voxel.h`, `<CMeshBuffer.h>` |
| `content_mapblock.h/cpp` | Map block mesh generation | `nodedef.h`, `tile.h` |
| `mesh_generator_thread.h/cpp` | Async mesh generation thread | `threading/mutex_auto_lock.h`, `util/thread.h` |
| `node_visuals.h/cpp` | Node visual/appearance manager | `nodedef.h`, `tile.h` |
| `wieldmesh.h/cpp` | Wielded item mesh rendering | `tile.h`, `nodedef.h`, `<IMeshSceneNode.h>` |
| `item_visuals_manager.h/cpp` | Item visual (model) management | `<thread>`, `<unordered_map>` |
| `guiscalingfilter.h/cpp` | GUI scaling filter for hi-DPI | `irrlichttypes.h`, `<SColor.h>` |
| `minimap.h/cpp` | Minimap rendering | `hud_element.h`, `mapnode.h`, `util/thread.h` |
| `hud.h/cpp` | HUD rendering | `hud_element.h`, `irr_ptr.h`, `<CMeshBuffer.h>` |

### 6.3 Objects & Particles

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `activeobjectmgr.h/cpp` | Client-side active object manager | `../activeobjectmgr.h`, `clientobject.h` |
| `clientobject.h/cpp` | Client-side active object base | `activeobject.h` |
| `content_cao.h/cpp` | Client-active object (entity rendering) | `clientobject.h`, `object_properties.h`, `tile.h` |
| `content_cso.h/cpp` | Client-simple object (falling nodes) | `clientsimpleobject.h` |
| `clientsimpleobject.h` | Simple client object base | `irrlichttypes_bloated.h` |
| `localplayer.h/cpp` | Local player (client-side) | `player.h`, `constants.h`, `lighting.h` |
| `particles.h/cpp` | Client-side particle rendering | `../particles.h`, `<ISceneNode.h>`, `<CMeshBuffer.h>` |

### 6.4 Input & Sound

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `inputhandler.h/cpp` | Keyboard/mouse input handler | `joystick_controller.h`, `keycode.h`, `settings.h` |
| `joystick_controller.h/cpp` | Joystick/gamepad input | `<IEventReceiver.h>`, `keys.h` |
| `keycode.h/cpp` | Key code mapping | `keys.h`, `<Keycodes.h>` |
| `keys.h` | Key enumeration | (none) |
| `sound.h/cpp` | Sound system interface (abstract) | `irr_v3d.h`, `config.h` |
| `sound_maker.h/cpp` | Sound event trigger (footsteps etc.) | `sound_spec.h`, `mtevent.h`, `mapnode.h` |
| `fontengine.h/cpp` | Font loading/rendering engine | `irr_ptr.h`, `threading/mutex_auto_lock.h` |
| `filecache.h/cpp` | Media file cache | `<iostream>`, `<string>` |

### 6.5 Subdirectories

**src/client/meshgen/** (Mesh Generation):
| File | Purpose | Dependencies |
|------|---------|-------------|
| `collector.h/cpp` | Mesh data collector (vertices, indices) | `irrlichttypes.h`, `irr_v3d.h`, `<S3DVertex.h>`, `client/tile.h` |

**src/client/render/** (Rendering Pipeline):
| File | Purpose | Dependencies |
|------|---------|-------------|
| `pipeline.h/cpp` | Render pipeline abstraction | `irrlichttypes_bloated.h`, `<IrrlichtDevice.h>` |
| `core.h/cpp` | Core render target | `irr_v2d.h`, `<SColor.h>` |
| `factory.h/cpp` | Rendering mode factory | `core.h` |
| `plain.h/cpp` | Plain mono rendering step | `core.h`, `pipeline.h` |
| `secondstage.h/cpp` | Second-stage post-processing (bloom, tone mapping) | `pipeline.h` |
| `stereo.h/cpp` | Stereo 3D base class | `pipeline.h` |
| `anaglyph.h/cpp` | Anaglyph 3D rendering | `pipeline.h` |
| `sidebyside.h/cpp` | Side-by-side 3D rendering | `stereo.h` |

**src/client/shadows/** (Dynamic Shadows):
| File | Purpose | Dependencies |
|------|---------|-------------|
| `dynamicshadows.h/cpp` | Shadow camera/scene calculation | `irrlichttypes_bloated.h`, `<matrix4.h>` |
| `dynamicshadowsrender.h/cpp` | Shadow map rendering pass | `dynamicshadows.h`, `<ISceneManager.h>` |
| `shadowsScreenQuad.h/cpp` | Shadow screen quad rendering | `<IMaterialRendererServices.h>`, `client/shader.h` |
| `shadowsshadercallbacks.h/cpp` | Shadow shader callbacks | `<IMaterialRendererServices.h>`, `client/shader.h` |

**src/client/sound/** (OpenAL Sound System):
| File | Purpose | Dependencies |
|------|---------|-------------|
| `al_helpers.h/cpp` | OpenAL helper functions | `log.h`, `irr_v3d.h` |
| `al_extensions.h/cpp` | OpenAL extension detection | `al_helpers.h` |
| `ogg_file.h/cpp` | OGG Vorbis file loader | `al_helpers.h`, `<vorbis/vorbisfile.h>` |
| `sound_data.h/cpp` | Sound data buffer management | `ogg_file.h` |
| `playing_sound.h/cpp` | Active sound instance | `sound_data.h` |
| `sound_manager.h/cpp` | Sound manager thread | `playing_sound.h`, `al_extensions.h`, `threading/thread.h` |
| `sound_manager_messages.h` | Sound manager message types | `client/sound.h`, `sound_spec.h` |
| `sound_openal.h/cpp` | OpenAL sound backend | `client/sound.h` |
| `sound_singleton.h/cpp` | OpenAL device/context singleton | `al_helpers.h` |
| `proxy_sound_manager.h/cpp` | Proxied sound manager (thread-safe) | `sound_manager.h` |
| `sound_constants.h` | Sound constants | (none) |

---

## 7. src/server/ — Server Logic

13 files implementing server-side game management.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `activeobjectmgr.h/cpp` | Server-side active object manager | `../activeobjectmgr.h`, `serveractiveobject.h`, `util/k_d_tree.h` |
| `serveractiveobject.h/cpp` | Base server active object (SAO) | `activeobject.h`, `itemgroup.h` |
| `unit_sao.h/cpp` | Unit SAO — shared entity/player base | `object_properties.h`, `serveractiveobject.h` |
| `luaentity_sao.h/cpp` | Lua entity SAO — mod-defined entities | `unit_sao.h`, `util/guid.h` |
| `player_sao.h/cpp` | Player SAO — connected player on server | `unit_sao.h`, `inventorymanager.h`, `metadata.h` |
| `clientiface.h/cpp` | Client connection interface — session management | `network/address.h`, `networkprotocol.h`, `clientdynamicinfo.h` |
| `ban.h/cpp` | IP ban management | `util/string.h` |
| `blockmodifier.h/cpp` | LoadingBlockModifier (LBM) system | `irr_v3d.h`, `mapnode.h` |
| `mods.h/cpp` | Server mod manager | `content/mod_configuration.h` |
| `rollback.h/cpp` | Rollback (undo) database | `rollback_interface.h`, `<sqlite3.h>` |
| `serverinventorymgr.h/cpp` | Server inventory manager | `inventorymanager.h` |
| `serverlist.h/cpp` | Server list (announce/fetch) | `config.h`, `content/mods.h` |

---

## 8. src/network/ — Networking

Custom UDP-based protocol (MTProtocol) with reliable delivery.

### 8.1 Core Network

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `networkprotocol.h/cpp` | Protocol version & packet type enums | `irrlichttypes.h` |
| `networkpacket.h/cpp` | Network packet serialization/deserialization | `util/pointer.h`, `networkprotocol.h` |
| `connection.h/cpp` | Network connection (MTProtocol) — reliable channels | `networkprotocol.h`, `socket.h` |
| `socket.h/cpp` | Low-level socket wrapper (UDP/TCP) | `irrlichttypes.h` |
| `address.h/cpp` | Network address (IPv4/IPv6) abstraction | `irrlichttypes.h`, platform sockets |
| `networkexceptions.h` | Network-specific exceptions | `exceptions.h` |
| `peerhandler.h` | Peer handler interface | (none) |

### 8.2 Packet Handlers

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `clientopcodes.h/cpp` | Client packet opcode table — maps packet types to handlers | `client/client.h`, `networkprotocol.h` |
| `serveropcodes.h/cpp` | Server packet opcode table | `server.h` |
| `clientpackethandler.cpp` | Client packet handler (processes server→client packets) | `client/client.h`, +many headers |
| `serverpackethandler.cpp` | Server packet handler (processes client→server packets) | `server.h`, `serverenvironment.h` |

### 8.3 MT Protocol Internals

**src/network/mtp/**:

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `impl.h/cpp` | MTProtocol implementation core — retransmission, ordering | `network/connection.h`, `util/container.h`, `util/pointer.h` |
| `internal.h` | Internal protocol types (packet flags, channel config) | `impl.h`, `util/numeric.h` |
| `threads.h/cpp` | MTProtocol send/receive threads | `threading/thread.h`, `internal.h` |

### 8.4 Encrypted Communications Layer (Integrated — v9.3)

> **Status:** Integrated and working. AES-256-GCM encryption is active when `secure_connection = true`. Modular toggle via `EncryptionConfig` namespace.
> **Branch:** `clawtest-v9.3`
> **Depends on:** OpenSSL (required, in `vcpkg.json`)

This layer provides real, verifiable encrypted communications between server and client using SRP-derived AES-256-GCM authenticated encryption with HKDF-SHA256 key derivation. The `EncryptionConfig` module provides centralized policy management. Two parallel crypto API layers exist for future reconciliation; the top-level API is currently used for the integrated encryption.

#### 8.4.1 Top-Level Crypto API (src/network/)

Simplified API wrapping OpenSSL primitives for the encrypted connection layer.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `crypto.h` | X25519 `KeyPair`, `AES256GCM`, `HKDF`, `Random`, `SessionKeys`, `CryptoException` | `irrlichttypes.h`, OpenSSL EVP/RAND/KDF headers |
| `crypto.cpp` | Implementation of all crypto primitives using OpenSSL 3.x EVP API | `crypto.h`, `log.h`, `util/serialize.h`, `<openssl/evp.h>`, `<openssl/rand.h>`, `<openssl/kdf.h>` |
| `encrypted_connection.h` | `EncryptedPacket` (wire format), `EncryptedConnection::Handshake`, `EncryptedConnection::SecureChannel` | `network/crypto.h`, `network/connection.h`, `irrlichttypes.h` |
| `encrypted_connection.cpp` | Encrypted packet create/decrypt, handshake key exchange, SecureChannel send/receive | `encrypted_connection.h`, `log.h`, `porting.h`, `util/serialize.h`, `network/networkpacket.h` |

**Key classes in top-level API:**
- `KeyPair` — X25519 ephemeral key pair for ECDH. Generates on construction, computes shared secrets.
- `AES256GCM` — Static encrypt/decrypt with 12-byte nonce, optional AAD. Ciphertext = plaintext + 16-byte tag.
- `HKDF` — HKDF-SHA256 key derivation with salt, IKM, info, and output length.
- `Random` — CSPRNG via `RAND_bytes`, generates nonces.
- `SessionKeys` — Derives bidirectional keys (C2S, S2C) from shared secret + public keys. Manages incrementing nonce counters per direction (thread-safe via mutex).
- `EncryptedPacket` — Wire format: `[1B version][8B counter][12B nonce][N ciphertext + 16B tag]`. Header + AAD binds version+counter to ciphertext.
- `EncryptedConnection::Handshake` — Simple 2-message key exchange: both sides exchange public keys, compute shared secret, derive session keys. Deterministic client/server role by lexicographic comparison of public keys.
- `EncryptedConnection::SecureChannel` — Wraps `con::IConnection` to add encryption. Uses command IDs `0xFF00` (handshake) and `0xFF01` (encrypted data).

#### 8.4.2 Detailed Crypto Subsystem (src/network/crypto/)

More detailed implementation with P-256 curves, ECDSA signatures, and a full 3-message handshake protocol.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `crypto.h` | P-256 `ECDHKeyPair`, `ECDSAKeyPair`, SHA-256, HKDF, `AES256GCM` (with `EncryptedData` struct), `NonceCounter` (with replay window), `SessionKeys` (separate struct), `secureRandom` | `irrlichttypes.h`, `<array>`, `<memory>`, `<optional>` |
| `crypto.cpp` | Full implementation of P-256 ECDH/ECDSA, AES-256-GCM, SHA-256, HKDF, NonceCounter with 64-entry sliding window bitmap | `crypto.h`, OpenSSL headers |
| `encrypted_packet.h` | `EncryptedPacket` wire format (magic `0xEC`, version, packet_num, nonce, auth_tag, ciphertext), `SessionKeys` derivation, `EncryptedSession` state machine, `SessionState` enum | `crypto.h`, `irrlichttypes.h` |
| `encrypted_packet.cpp` | Serialization/deserialization, session key derivation, encrypt/decrypt with session state management | `encrypted_packet.h` |
| `handshake.h` | 3-message handshake protocol: `ClientHello` → `ServerHello` → `ClientFinish`. `Handshake` state machine with role-based API, transcript hash, ECDSA signature verification | `crypto.h`, `encrypted_packet.h` |

**Key classes in detailed API:**
- `ECDHKeyPair` — P-256 ECDH key pair (65-byte uncompressed public key). PIMPL idiom for OpenSSL types.
- `ECDSAKeyPair` — P-256 ECDSA signing key pair. 64-byte signatures (r‖s).
- `verifySignature()` — Standalone ECDSA verification against a public key.
- `NonceCounter` — Incrementing send counter + sliding window (64-bit bitmap) receive validation for replay protection.
- `EncryptedSession` — Full session state machine (NONE → HANDSHAKE_INIT → HANDSHAKE_ACK → ESTABLISHED → FAILED).
- `Handshake` (in `handshake.h`) — 3-message protocol with identity verification:
  1. Client → Server: `ClientHello` (ECDH pubkey + random nonce)
  2. Server → Client: `ServerHello` (ECDH pubkey + ECDSA signature over transcript + random nonce)
  3. Client → Server: `ClientFinish` (ECDSA signature over transcript)
  - Provides: confidentiality (AES-256-GCM), authentication (ECDSA), forward secrecy (ephemeral ECDH), integrity (GCM tags), replay protection (nonce counters + sliding window).

#### 8.4.3 Unit Tests (src/unittest/)

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `test_encrypted_connection.cpp` | 31 test cases covering all crypto primitives and the encrypted connection layer | `test.h`, `network/crypto.h`, `network/encrypted_connection.h`, `network/peerhandler.h` |

**Test categories (31 tests):**
- Crypto primitives (21 tests): KeyPair generation, shared secret computation/consistency, invalid key handling, AES-256-GCM encrypt/decrypt/empty/large/tamper/wrong-key/wrong-nonce/AAD/tampered-AAD, HKDF derivation/different-info/different-salt, random generation/uniqueness, SessionKeys init/key-separation/nonce-increment/bidirectional
- Encrypted connection layer (10 tests): packet format, handshake, send/receive, bidirectional, large packets, replay protection, wrong key rejection, packet integrity, server-client simulation, multiple messages

#### 8.4.4 Build System Changes

- `src/network/CMakeLists.txt`: Added `crypto.cpp` and `encrypted_connection.cpp` to `common_network_SRCS`
- `src/CMakeLists.txt`: Updated (major reformat)
- `src/unittest/CMakeLists.txt`: Added `test_encrypted_connection.cpp` to test sources

#### 8.4.5 Architecture Notes — Reconciliation Needed

The two crypto layers have overlapping class names (`SessionKeys`, `EncryptedPacket`) but different designs:

| Aspect | Top-Level (src/network/) | Detailed (src/network/crypto/) |
|--------|--------------------------|-------------------------------|
| Key Exchange | X25519 (Curve25519) | P-256 ECDH |
| Authentication | None (no identity verification) | ECDSA P-256 signatures |
| Handshake | 2-message (simple key exchange) | 3-message (with identity signatures) |
| Nonce Management | Simple incrementing counter | Counter + 64-bit sliding window replay protection |
| Session State | Boolean (completed or not) | Full state machine (5 states) |
| Wire Format | Version byte + counter + nonce + ciphertext | Magic byte + version + packet_num + nonce + tag + ciphertext |
| Key Derivation | HKDF with client/server info labels | HKDF with handshake transcript hash |

**Recommendation:** Merge the best of both — use X25519 for key exchange (faster, safer), add ECDSA verification from the detailed layer, use the replay-protected NonceCounter, and adopt the 3-message handshake for identity verification.

---

## 9. src/mapgen/ — Map Generation

8 terrain algorithms plus cave/dungeon/tree generation.

### 9.1 Base & Utilities

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `mapgen.h/cpp` | **Base Mapgen class** + MapgenParams — shared interface | `constants.h`, `noise.h`, `nodedef.h`, `voxel.h`, `emerge.h` |
| `mg_biome.h/cpp` | Biome definition/resolution — biome heat/humidity | `objdef.h`, `nodedef.h`, `noise.h` |
| `mg_decoration.h/cpp` | Decoration placement (plants, grass, etc.) | `objdef.h`, `noise.h`, `nodedef.h` |
| `mg_ore.h/cpp` | Ore placement (veins, sheets, strata) | `objdef.h`, `noise.h`, `nodedef.h` |
| `mg_schematic.h/cpp` | Schematic (structure) placement — .mts file loading | `mg_decoration.h`, `util/string.h` |
| `cavegen.h/cpp` | Cave generation (tunnels, caverns) | `mapgen.h`, `mg_biome.h` |
| `dungeongen.h/cpp` | Dungeon generation (rooms, corridors) | `voxel.h`, `noise.h`, `mapgen.h` |
| `treegen.h/cpp` | Tree generation (L-system + parametric) | `irr_v3d.h`, `nodedef.h`, `mapnode.h` |

### 9.2 Terrain Algorithms

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `mapgen_carpathian.h/cpp` | Carpathian terrain generator | `mapgen.h` |
| `mapgen_flat.h/cpp` | Flat terrain generator | `mapgen.h` |
| `mapgen_fractal.h/cpp` | Fractal terrain generator | `mapgen.h` |
| `mapgen_singlenode.h/cpp` | Single-node (empty) generator | `mapgen.h` |
| `mapgen_v5.h/cpp` | V5 terrain generator | `mapgen.h` |
| `mapgen_v6.h/cpp` | V6 terrain generator (classic) | `mapgen.h`, `noise.h` |
| `mapgen_v7.h/cpp` | V7 terrain generator (default) | `mapgen.h` |
| `mapgen_valleys.h/cpp` | Valleys terrain generator | `mapgen.h` |

---

## 10. src/database/ — Persistence

Pluggable database backends for map and player storage.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `database.h/cpp` | **Abstract Database interface** — MapDatabase, PlayerDatabase, AuthDatabase | `irr_v3d.h`, `util/string.h` |
| `database-dummy.h/cpp` | In-memory dummy database (for testing) | `database.h` |
| `database-files.h/cpp` | File-based player database (JSON) | `database.h`, `<json/json.h>` |
| `database-sqlite3.h/cpp` | SQLite3 database backend (default) | `database.h`, `<sqlite3.h>` |
| `database-leveldb.h/cpp` | LevelDB database backend | `database.h`, `<leveldb/db.h>` |
| `database-postgresql.h/cpp` | PostgreSQL database backend | `database.h`, `<libpq-fe.h>` |
| `database-redis.h/cpp` | Redis database backend | `database.h`, `<hiredis.h>` |

**Connection pattern:** `servermap.h` → `database.h` (via `MapDatabase*`), `remoteplayer.h` → `database.h` (via `PlayerDatabase*`)

---

## 11. src/script/ — Scripting System

The C++ ↔ Lua bridge — the largest subsystem by file count (78 files).

### 11.1 Scripting Entry Points (src/script/)

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `scripting_server.h/cpp` | **Server scripting context** — the main mod API | `s_base.h`, `s_entity.h`, `s_env.h`, `s_inventory.h`, `s_modchannels.h`, `s_node.h`, `s_player.h`, `s_server.h`, `s_security.h`, `s_async.h` |
| `scripting_client.h/cpp` | Client scripting context (CSM) | `s_base.h`, `s_client.h`, `s_client_common.h`, `s_modchannels.h`, `s_security.h` |
| `scripting_mainmenu.h/cpp` | Main menu scripting | `s_base.h`, `s_mainmenu.h`, `s_security.h`, `s_async.h` |
| `scripting_emerge.h/cpp` | Emerge thread scripting | `s_base.h`, `s_mapgen.h`, `s_security.h` |
| `scripting_pause_menu.h/cpp` | Pause menu scripting | `s_base.h`, `s_client_common.h`, `s_pause_menu.h` |
| `scripting_sscsm.h/cpp` | SSCSM (server-sent CSM) scripting | `s_base.h`, `s_sscsm.h`, `s_security.h` |

### 11.2 Common Script Utilities (src/script/common/)

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `c_content.h/cpp` | Content push/pull from Lua — node/item/object tables | `nodedef.h`, `object_properties.h`, `itemdef.h`, `server.h` |
| `c_converter.h/cpp` | Lua↔C++ type conversion (v3f, aabb, etc.) | `irrlichttypes_bloated.h`, `<lua.h>` |
| `c_internal.h/cpp` | Internal script helpers (error handling, locking) | `<lua.h>`, `config.h` |
| `c_packer.h/cpp` | Script packer (SSCSM serialization) | `irrlichttypes.h`, `<lua.h>` |
| `c_types.h` | Script error types | `<lua.h>`, `exceptions.h` |
| `helper.h/cpp` | Lua helper utilities (stack inspection, etc.) | `<lua.h>` |

### 11.3 C++ Script API Bindings (src/script/cpp_api/)

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `s_base.h/cpp` | **Base scripting class** — Lua state management | `common/helper.h`, `common/c_internal.h`, `<lua.h>` |
| `s_async.h/cpp` | Async scripting (worker threads) | `s_base.h`, `threading/semaphore.h` |
| `s_client.h/cpp` | Client script API | `s_base.h`, `mapnode.h` |
| `s_client_common.h/cpp` | Common client script API | `s_base.h` |
| `s_entity.h/cpp` | Entity script API (register_entity) | `s_base.h`, `irr_v3d.h` |
| `s_env.h/cpp` | Environment script API (set_node, etc.) | `s_base.h`, `mapnode.h` |
| `s_inventory.h/cpp` | Inventory script API | `s_base.h` |
| `s_item.h/cpp` | Item callback script API | `s_base.h`, `irr_v3d.h` |
| `s_mainmenu.h/cpp` | Main menu script API | `s_base.h`, `gui/guiMainMenu.h` |
| `s_mapgen.h/cpp` | Mapgen script API | `s_base.h` |
| `s_modchannels.h/cpp` | Mod channel script API | `s_base.h` |
| `s_node.h/cpp` | Node script API | `s_base.h`, `s_nodemeta.h` |
| `s_nodemeta.h/cpp` | Node metadata script API | `s_base.h`, `s_item.h` |
| `s_pause_menu.h/cpp` | Pause menu script API | `s_base.h` |
| `s_player.h/cpp` | Player script API | `s_base.h`, `irr_v3d.h` |
| `s_security.h/cpp` | Security (sandbox) script API | `s_base.h` |
| `s_server.h/cpp` | Server script API | `s_base.h` |
| `s_sscsm.h/cpp` | SSCSM script API | `s_base.h` |
| `s_internal.h` | Script internal locking | `c_internal.h`, `threading/mutex_auto_lock.h` |

### 11.4 Lua API Bindings (src/script/lua_api/)

42 files implementing the `core.*` / `minetest.*` Lua API.

| File | Purpose | Key C++ Bridge |
|------|---------|---------------|
| `l_base.h/cpp` | **Base Lua API class** | `c_types.h`, `helper.h`, `gamedef.h` |
| `l_areastore.h/cpp` | AreaStore Lua API | → `util/areastore.h` |
| `l_async.h/cpp` | Async Lua API | → `script/cpp_api/s_async.h` |
| `l_auth.h/cpp` | Auth Lua API | → `server/ban.h` |
| `l_camera.h/cpp` | Camera Lua API | → `client/camera.h` |
| `l_client.h/cpp` | Client Lua API | → `client/client.h` |
| `l_client_common.h/cpp` | Common client Lua API | → `client/client.h` |
| `l_client_sound.h/cpp` | Client sound Lua API | → `client/sound.h` |
| `l_craft.h/cpp` | Crafting Lua API | → `craftdef.h` |
| `l_env.h/cpp` | Environment Lua API (get_node, add_entity, etc.) | → `serverenvironment.h`, `raycast.h` |
| `l_http.h/cpp` | HTTP Lua API | → `httpfetch.h` |
| `l_inventory.h/cpp` | Inventory Lua API | → `inventory.h` |
| `l_ipc.h/cpp` | IPC Lua API | → async workers |
| `l_item.h/cpp` | Item Lua API | → `inventory.h`, `itemdef.h` |
| `l_itemstackmeta.h/cpp` | ItemStack metadata Lua API | → `l_metadata.h`, `l_item.h` |
| `l_localplayer.h/cpp` | Local player Lua API | → `client/localplayer.h` |
| `l_mainmenu.h/cpp` | Main menu Lua API | → `gui/guiMainMenu.h` |
| `l_mainmenu_sound.h/cpp` | Main menu sound Lua API | → `client/sound.h` |
| `l_mapgen.h/cpp` | Mapgen Lua API | → `mapgen/mapgen.h` |
| `l_menu_common.h/cpp` | Menu common Lua API | → settings, paths |
| `l_metadata.h` | Metadata Lua API base | → `metadata.h` |
| `l_minimap.h/cpp` | Minimap Lua API | → `client/minimap.h` |
| `l_modchannels.h/cpp` | Mod channels Lua API | → `modchannels.h` |
| `l_nodemeta.h/cpp` | Node metadata Lua API | → `l_metadata.h` |
| `l_nodetimer.h/cpp` | Node timer Lua API | → `nodetimer.h` |
| `l_noise.h/cpp` | Noise Lua API | → `noise.h` |
| `l_object.h/cpp` | Object/entity Lua API | → `server/serveractiveobject.h` |
| `l_particleparams.h` | Particle parameter helpers | → `l_object.h`, `particles.h` |
| `l_particles.h/cpp` | Particles Lua API | → `particles.h` |
| `l_particles_local.h/cpp` | Local particles Lua API | → client particles |
| `l_pause_menu.h/cpp` | Pause menu Lua API | → pause menu system |
| `l_playermeta.h/cpp` | Player metadata Lua API | → `l_metadata.h`, `inventory.h` |
| `l_rollback.h/cpp` | Rollback Lua API | → `rollback_interface.h` |
| `l_server.h/cpp` | Server Lua API | → `server.h` |
| `l_settings.h/cpp` | Settings Lua API | → `settings.h`, `common/c_content.h` |
| `l_sscsm.h/cpp` | SSCSM Lua API | → SSCSM system |
| `l_storage.h/cpp` | Mod storage Lua API | → `l_metadata.h`, `content/mods.h` |
| `l_util.h/cpp` | Utility Lua API (log, serialize, etc.) | → `log.h`, `serialization.h` |
| `l_vmanip.h/cpp` | VoxelManip Lua API | → `voxel.h`, `util/basic_macros.h` |
| `l_internal.h` | Internal Lua API helpers | → `common/c_internal.h` |

### 11.5 SSCSM Internals (src/script/sscsm/)

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `sscsm_controller.h/cpp` | SSCSM controller — routes requests | `sscsm_irequest.h`, `sscsm_ievent.h` |
| `sscsm_environment.h/cpp` | SSCSM environment (runs client mods) | `client/client.h`, `threading/thread.h`, `sscsm_controller.h` |
| `sscsm_events.h` | SSCSM event types | `sscsm_ievent.h`, `sscsm_environment.h` |
| `sscsm_ievent.h` | SSCSM event interface | `<memory>`, `<type_traits>` |
| `sscsm_irequest.h` | SSCSM request interface | `exceptions.h` |
| `sscsm_requests.h` | SSCSM request types | `sscsm_irequest.h`, `mapnode.h`, `map.h` |
| `sscsm_stupid_channel.h` | SSCSM sync channel | `sscsm_irequest.h` |

---

## 12. src/gui/ — GUI System

30 files implementing the formspec-based UI system and all dialog widgets.

### 12.1 Core GUI

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `guiEngine.h/cpp` | Main menu GUI engine — drives the Lua main menu | `guiFormSpecMenu.h`, `clouds.h`, `sound.h`, `translation.h` |
| `guiFormSpecMenu.h/cpp` | **Formspec parser/renderer** — the central GUI class | `inventory.h`, `modalMenu.h`, `StyleSpec.h`, `guiInventoryList.h` |
| `modalMenu.h/cpp` | Modal menu base class | `<IGUIElement.h>`, `irr_ptr.h` |
| `mainmenumanager.h` | Main menu manager (modal stack) | `modalMenu.h`, `touchcontrols.h` |
| `StyleSpec.h` | Formspec style specification | `texturesource.h`, `fontengine.h`, `util/string.h` |
| `drawItemStack.h/cpp` | Item stack icon rendering | `<IVideoDriver.h>`, `irr_v3d.h` |

### 12.2 Widget Components

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `guiAnimatedImage.h/cpp` | Animated image widget | `<IGUIElement.h>` |
| `guiBackgroundImage.h/cpp` | Background image widget | `<IGUIElement.h>` |
| `guiBox.h/cpp` | Box drawing widget | `<IGUIElement.h>` |
| `guiButton.h/cpp` | Styled button widget | `StyleSpec.h`, `irrlicht_changes/static_text.h` |
| `guiButtonImage.h/cpp` | Image button widget | `guiButton.h`, `guiAnimatedImage.h` |
| `guiButtonItemImage.h/cpp` | Item image button widget | `guiButton.h` |
| `guiButtonKey.h/cpp` | Key binding button widget | `guiButton.h`, `keycode.h` |
| `guiChatConsole.h/cpp` | Chat console overlay | `modalMenu.h`, `chat.h` |
| `guiEditBoxWithScrollbar.h/cpp` | Scrollable edit box | `<CGUIEditBox.h>` |
| `guiHyperText.h/cpp` | Hypertext widget | `<IGUIElement.h>`, `irr_v3d.h` |
| `guiInventoryList.h/cpp` | Inventory list widget | `inventorymanager.h`, `<IGUIElement.h>` |
| `guiItemImage.h/cpp` | Item image widget | `<IGUIElement.h>` |
| `guiScene.h/cpp` | 3D scene widget | `StyleSpec.h`, `<AnimatedMeshSceneNode.h>` |
| `guiScrollBar.h/cpp` | Custom scroll bar | `<CGUIScrollBar.h>` |
| `guiScrollContainer.h/cpp` | Scroll container widget | `guiScrollBar.h` |
| `guiTable.h/cpp` | Table widget | `<IGUIElement.h>`, `<SColor.h>` |

### 12.3 Dialog Components

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `guiMainMenu.h` | Main menu parameters struct | `gameparams.h` |
| `guiOpenURL.h/cpp` | URL opener dialog | `modalMenu.h` |
| `guiPasswordChange.h/cpp` | Password change dialog | `modalMenu.h` |
| `guiPathSelectMenu.h/cpp` | File path selector dialog | `modalMenu.h` |
| `guiVolumeChange.h/cpp` | Volume change dialog | `modalMenu.h` |
| `profilergraph.h/cpp` | Profiler graph visualization | `<IGUIFont.h>`, `profiler.h` |
| `statusTextHelper.h/cpp` | Status text overlay helper | `<IGUIStaticText.h>` |

### 12.4 Touch Controls

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `touchcontrols.h/cpp` | Touch screen controls | `itemdef.h`, `touchscreenlayout.h`, `keycode.h` |
| `touchscreeneditor.h/cpp` | Touch layout editor | `touchscreenlayout.h`, `modalMenu.h` |
| `touchscreenlayout.h/cpp` | Touch screen button layout | `irr_ptr.h`, `util/enum_string.h` |

---

## 13. src/util/ — Utility Library

30 files — the universal leaf layer with no upward dependencies.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `areastore.h/cpp` | Spatial area store (R-tree) | `irr_v3d.h`, `container.h`, `numeric.h` |
| `auth.h/cpp` | SRP authentication | `base64.h`, `hashing.h`, `srp.h` |
| `base64.h/cpp` | Base64 encode/decode | `<string>` |
| `basic_macros.h` | Common macros (DISABLE_CLASS_COPY, etc.) | (none) |
| `bitmap.h` | Bitmap utilities | `irrlichttypes.h` |
| `colorize.h/cpp` | Terminal colorization | `<curl/urlapi.h>` |
| `container.h` | Thread-safe containers (MutexedQueue, etc.) | `threading/mutex_auto_lock.h`, `threading/semaphore.h` |
| `directiontables.h/cpp` | Direction vector lookup tables | `irrlichttypes.h`, `irr_v3d.h` |
| `enriched_string.h/cpp` | Rich text string (with colors) | `<SColor.h>`, `<string>` |
| `enum_string.h/cpp` | Enum↔string mapping | `<string_view>`, `<type_traits>` |
| `guid.h/cpp` | GUID generation | `irrlichttypes.h`, `base64.h` |
| `hashing.h/cpp` | SHA-1/SHA-256 hashing | `<openssl/sha.h>` |
| `hex.h` | Hex encode/decode | `<string>` |
| `ieee_float.h/cpp` | IEEE 754 float serialization | `irrlichttypes.h` |
| `k_d_tree.h` | K-D tree spatial index | `<algorithm>`, `<unordered_map>` |
| `metricsbackend.h/cpp` | Prometheus metrics backend | `util/thread.h`, `<prometheus/...>` |
| `numeric.h/cpp` | **Numeric utilities** (matrix, random, math) | `constants.h`, `irr_v2d.h`, `irr_v3d.h`, `irr_aabb3d.h` |
| `png.h/cpp` | PNG image writing | `<zlib.h>`, `serialize.h` |
| `pointabilities.h/cpp` | Pointing capability system | `itemgroup.h`, `irrlichttypes.h` |
| `pointedthing.h/cpp` | PointedThing (what player aims at) | `irr_v3d.h`, `pointabilities.h` |
| `pointer.h` | Buffer<T> smart pointer | `irrlichttypes.h` |
| `quicktune.h/cpp` | Quick-tune debug variables | `<string>` |
| `quicktune_shortcutter.h` | Quick-tune shortcut helper | `quicktune.h` |
| `screenshot.h/cpp` | Screenshot capture | `filesys.h`, `settings.h` |
| `serialize.h/cpp` | **Serialization utilities** — core binary I/O | `irrlichttypes_bloated.h`, `exceptions.h`, `ieee_float.h` |
| `sha1.h/cpp` | Pure SHA-1 implementation | `<cstdint>` |
| `srp.h/cpp` | SRP protocol implementation | `<cstddef>` |
| `stream.h` | Logging stream helpers | `<iostream>`, `<functional>` |
| `strfnd.h` | String finder/tokenizer | `<string>` |
| `string.h/cpp` | **String utilities** — wide, split, trim, wrap | `irrlichttypes_bloated.h`, `config.h` |
| `thread.h` | Thread utility wrappers | `threading/thread.h`, `container.h` |
| `timetaker.h/cpp` | Time measurement utility | `irrlichttypes.h` |
| `tracy_wrapper.h` | Tracy profiler wrapper | `<tracy/Tracy.hpp>` |

---

## 14. src/threading/ — Threading Primitives

7 files — very well-isolated, depends only on `util/basic_macros.h` and `debug.h`.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `thread.h/cpp` | Thread base class (wraps std::thread) | `util/basic_macros.h`, `<thread>` |
| `event.h/cpp` | Event (condition variable wrapper) | `<condition_variable>` |
| `mutex_auto_lock.h` | Mutex auto-lock guard (RAII) | `<mutex>` |
| `ordered_mutex.h` | Ordered mutex (priority-based) | `<condition_variable>` |
| `semaphore.h/cpp` | Semaphore (platform-specific) | `util/basic_macros.h`, platform headers |
| `lambda.h` | Lambda thread helper | `debug.h`, `threading/thread.h` |

---

## 15. src/content/ — Content/Mod Management

4 files for loading and resolving game content.

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `content.h/cpp` | Content type definitions (ModSpec, etc.) | `irrlichttypes.h` |
| `mods.h/cpp` | Mod loading & ModSpec — scans mod directories | `metadata.h`, `database/database.h` |
| `mod_configuration.h/cpp` | Mod configuration/dependency resolution | `mods.h` |
| `subgames.h/cpp` | Subgame (game) management — game.conf parsing | `<string>`, `<set>` |

---

## 16. src/benchmark/ & src/test/

### src/benchmark/ (8 files)

| File | Purpose | Key Dependencies |
|------|---------|-----------------|
| `benchmark.h/cpp` | Benchmark framework | `config.h`, `catch.h` |
| `benchmark_activeobjectmgr.cpp` | ActiveObjectMgr benchmark | `server/activeobjectmgr.h` |
| `benchmark_lighting.cpp` | Lighting benchmark | `voxelalgorithms.h`, `dummygamedef.h` |
| `benchmark_map.cpp` | Map benchmark | `dummygamedef.h`, `map.h` |
| `benchmark_mapblock.cpp` | MapBlock benchmark | `mapblock.h` |
| `benchmark_mapmodify.cpp` | Map modification benchmark | `util/container.h` |
| `benchmark_serialize.cpp` | Serialization benchmark | `util/serialize.h` |
| `benchmark_sha.cpp` | SHA benchmark | `util/hashing.h` |

### src/test/ (Small standalone tests, 6 files)

| File | Purpose |
|------|---------|
| `test.h/cpp` | Test framework setup |
| `test_irr_matrix4.cpp` | Matrix4 tests |
| `test_irr_rotation.cpp` | Rotation tests |
| `test_k_d_tree.cpp` | K-D tree tests |
| `test_nodedef.cpp` | Node definition tests |
| `test_serveractiveobjectmgr.cpp` | SAO manager tests |

### src/unittest/ (~55 files)

Full unit test suite covering: collision, craft, inventory, map, noise, serialization, settings, voxel, threading, Lua API, etc.

---

## 17. builtin/ — Lua Scripts

All Lua code that ships with the engine — implements core game logic, main menu, and specialized environments.

### 17.1 Master Entry Point

**`builtin/init.lua`** — loaded by C++ `ScriptApiBase::loadBuiltin`:

1. Sets up `core.error_handler`, `core.debug`, `print`, `math.randomseed`
2. Aliases `minetest = core`
3. Loads 6 common files shared by ALL environments
4. Dispatches based on `INIT` global variable

| `INIT` value | Sub-tree loaded | Purpose |
|---|---|---|
| `"game"` | `game/init.lua` | Server-side game environment |
| `"mainmenu"` | `mainmenu/init.lua` | Main menu GUI |
| `"async"` | `async/mainmenu.lua` | Async worker for main menu |
| `"async_game"` | `async/game.lua` | Async worker for game |
| `"client"` | `client/init.lua` | Client-side Lua environment |
| `"sscsm"` + client | `sscsm_client/init.lua` | Server-sent client-side mod env |
| `"sscsm"` + server | `sscsm_server/init.lua` | Server-side SSCSM handler |
| `"emerge"` | `emerge/init.lua` | Map generation environment |
| `"pause_menu"` | `pause_menu/init.lua` | In-game pause menu |

**Common files loaded unconditionally** (before dispatch):
```
common/math.lua → common/vector.lua → common/vector2.lua → common/strict.lua
→ common/serialize.lua → common/misc_helpers.lua
```

### 17.2 builtin/common/ (17 files + tests + settings)

| File | Purpose | Dependencies |
|---|---|---|
| `math.lua` | `math.hypot`, `math.sign`, `math.factorial`, `math.round` | — |
| `vector.lua` | 3D vector class with metatable, arithmetic, rotation | `core.set_read_vector`/`set_push_vector` (C++) |
| `vector2.lua` | 2D vector class | `core.set_read_vector2`/`set_push_vector2` (C++) |
| `strict.lua` | Global variable declaration checker | `core.log` |
| `serialize.lua` | `core.serialize` / `core.deserialize` | `loadstring`, `setfenv` |
| `misc_helpers.lua` | `dump`, `string.split`, `core.formspec_escape`, `core.colorize`, `core.translate`, `fgettext`, `table.copy` | `vector`, `core.*` |
| `item_s.lua` | Item helpers: `core.inventorycube`, `core.dir_to_facedir` | `builtin_shared` |
| `register.lua` | `core.run_callbacks`, `make_registration` — callback infrastructure | `builtin_shared` |
| `after.lua` | `core.after` — min-heap delayed execution | `core.register_globalstep` |
| `metatable.lua` | `core.register_portable_metatable` — cross-env metatable registry | `vector.metatable` |
| `mod_storage.lua` | `core.get_mod_storage` — per-mod persistent storage | `core.get_mod_storage` (C++) |
| `chatcommands.lua` | `core.register_chatcommand`, `/help` command | `core.get_translator` |
| `information_formspecs.lua` | Help/privs formspec GUIs | `core.registered_chatcommands` |
| `menu.lua` | Color constants, `core.are_keycodes_equal` | `core.normalize_keycode` |
| `filterlist.lua` | `filterlist` — generic filterable/sortable list | — |
| `settings/init.lua` | Settings dialog loader | `settingtypes.lua`, dialogs |

### 17.3 builtin/game/ (20 files + tests)

Server-side game logic — the largest and most complex sub-tree.

| File | Purpose | Dependencies |
|---|---|---|
| `init.lua` | Game env orchestrator — loads all game files in order | All game/*.lua and common/*.lua |
| `constants.lua` | `core.CONTENT_*`, `core.EMERGE_*`, `core.MAP_BLOCKSIZE` | — |
| `features.lua` | `core.features` table + `core.has_feature` — API versioning | — |
| `item.lua` | `core.item_place_node`, `core.item_drop`, `core.node_dig`, item defaults | `builtin_shared`, `vector` |
| `register.lua` | `core.register_item/node/craftitem/tool`, `core.register_abm/lbm/entity`, all `register_on_*` callbacks | `builtin_shared` |
| `item_entity.lua` | `__builtin:item` entity — dropped item behavior | `core.register_entity`, `core.serialize` |
| `deprecated.lua` | Backward-compat wrappers | `core.register_on_authplayer` |
| `misc_s.lua` | Standalone misc: `core.hash_node_position`, `core.get_item_group` | — |
| `misc.lua` | Server misc: `core.kick_player`, `core.check_player_privs` | C++ auth API |
| `privileges.lua` | `core.register_privilege`, built-in privileges | `core.string_to_privs` |
| `auth.lua` | `core.builtin_auth_handler`, authentication | `core.auth` (C++) |
| `chat.lua` | Chat commands: `/me`, `/teleport`, `/give`, `/time`, etc. | `core.register_on_chat_message` |
| `static_spawn.lua` | Static spawnpoint from settings | `core.register_on_newplayer` |
| `detached_inventory.lua` | `core.create_detached_inventory` | C++ raw API |
| `falling.lua` | `__builtin:falling_node` entity, `core.check_for_falling` | `builtin_shared` |
| `voxelarea.lua` | `VoxelArea` class for VoxelManip index calculations | `vector` |
| `forceloading.lua` | `core.forceload_block`/`forceload_free_block` | C++ forceload API |
| `hud.lua` | `core.hud_replace_builtin`, built-in HUD elements | `core.register_on_joinplayer` |
| `knockback.lua` | `core.calculate_knockback` | `core.register_on_punchplayer` |
| `async.lua` | `core.handle_async` — async job dispatch | `core.do_async_callback` |
| `death_screen.lua` | Death screen formspec + respawn | `core.register_on_dieplayer` |

### 17.4 builtin/mainmenu/ (20+ files)

Main menu GUI — runs in its own Lua environment.

| File | Purpose | Dependencies |
|---|---|---|
| `init.lua` | Main menu orchestrator | All mainmenu/*.lua, fstk/*.lua |
| `common.lua` | Shared menu globals, game/world helpers | `core.get_games`, `core.get_worlds` |
| `async_event.lua` | `core.handle_async` for main menu | `core.do_async_callback` |
| `serverlistmgr.lua` | Server list management | `core.get_serverlist`, `core.settings` |
| `game_theme.lua` | Game theme/background manager | `core.set_background` |
| `tab_local.lua` | "Local Game" tab | `filterlist`, `pkgmgr` |
| `tab_online.lua` | "Play Online" tab | `serverlistmgr` |
| `tab_content.lua` | "Content" tab | `pkgmgr`, `contentdb` |
| `tab_about.lua` | "About/Credits" tab | `credits.json` |
| `dlg_config_world.lua` | World configuration dialog | `pkgmgr` |
| `dlg_create_world.lua` | World creation dialog | `core.get_games` |
| `dlg_delete_world.lua` | World deletion dialog | — |
| `dlg_confirm_exit.lua` | Exit confirmation dialog | — |
| `dlg_register.lua` | Register account dialog | — |
| `content/init.lua` | Content sub-system loader | pkgmgr, contentdb, dialogs |
| `content/pkgmgr.lua` | Package manager | `core.get_modpath` |
| `content/contentdb.lua` | ContentDB API client | `core.http_fetch` |
| `content/update_detector.lua` | Package update detection | `core.http_fetch` |
| `content/screenshots.lua` | Screenshot download/cache | `core.http_fetch` |

### 17.5 builtin/fstk/ (4 files)

FSTK — "Formspec Toolkit" — UI framework for the main menu.

| File | Purpose | Dependencies |
|---|---|---|
| `ui.lua` | `ui` global — manages child UI elements, event callbacks | `core.update_formspec` |
| `dialog.lua` | `dialog_create`, `messagebox` — modal dialog system | `ui` |
| `tabview.lua` | `tabview_create` — tabbed interface system | `ui` |
| `buttonbar.lua` | `buttonbar_create` — scrollable button bar | `ui`, `core.settings` |

### 17.6 builtin/async/ (2 files)

| File | Purpose | Dependencies |
|---|---|---|
| `mainmenu.lua` | Main menu async worker | `core.deserialize`, `core.serialize` |
| `game.lua` | Game async worker — imports subset of game/ files | game/constants.lua, common/item_s.lua, game/misc_s.lua |

### 17.7 builtin/client/ (4 files)

| File | Purpose | Dependencies |
|---|---|---|
| `init.lua` | Client env orchestrator | common/register.lua, client/register.lua, common/after.lua |
| `register.lua` | Client callback registration | `builtin_shared.make_registration` |
| `chatcommands.lua` | Client chat commands (`.list_players`, `.disconnect`) | `core.display_chat_message` |
| `misc.lua` | Client misc: `core.setting_get_pos`, sound helpers | `core.settings` |

### 17.8 builtin/emerge/ (3 files)

| File | Purpose | Dependencies |
|---|---|---|
| `init.lua` | Emerge env orchestrator | game/constants.lua, emerge/register.lua, emerge/env.lua |
| `register.lua` | Emerge callbacks: `register_on_generated` | `builtin_shared.make_registration` |
| `env.lua` | Environment on VoxelManip: `set_node`, `get_node`, `bulk_set_node` | `core.vmanip` (C++) |

### 17.9 builtin/profiler/ (4 files)

| File | Purpose | Dependencies |
|---|---|---|
| `init.lua` | Profiler entry, registers `/profiler` chat command | sampling.lua, instrumentation.lua, reporter.lua |
| `sampling.lua` | Per-step measurement collection | `core.register_globalstep` |
| `instrumentation.lua` | Wraps callbacks with timing | `sampler`, `debug.getinfo` |
| `reporter.lua` | Output profiler data as txt/csv/lua/json | `core.write_json`, `core.serialize` |

### 17.10 builtin/sscsm_client/ (2 files), builtin/sscsm_server/ (1 file), builtin/pause_menu/ (2 files)

| File | Purpose | Dependencies |
|---|---|---|
| `sscsm_client/init.lua` | SSCSM client orchestrator | common/register.lua, sscsm_client/register.lua |
| `sscsm_client/register.lua` | `core.register_globalstep` only | `builtin_shared.make_registration` |
| `sscsm_server/init.lua` | Empty placeholder | — |
| `pause_menu/init.lua` | Pause menu orchestrator | common/register.lua, pause_menu/register.lua, common/settings/ |
| `pause_menu/register.lua` | `core.register_on_formspec_input` | `builtin_shared.make_registration` |

### 17.11 builtin/locale/ (~50 .tr files)

Translation files for built-in strings. One file per language code (e.g., `__builtin.fr.tr`, `__builtin.de.tr`).

---

## 18. client/ — Runtime Shaders

This directory contains **no C++ source files** — it holds runtime GLSL shader programs loaded by `src/client/shader.cpp`.

| Shader Directory | Files | Purpose | Connected C++ Code |
|---|---|---|---|
| `Irrlicht/` | `Solid.vsh/fsh`, `TransparentAlphaChannel.fsh`, etc. | Legacy Irrlicht fixed-pipeline shaders | `irr/src/COpenGLSLMaterialRenderer.cpp` |
| `nodes_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | **Primary terrain/node rendering** | `src/client/shader.cpp`, `src/client/content_mapblock.cpp` |
| `object_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | **Entity/object rendering** | `src/client/content_cao.cpp` |
| `shadow/pass1/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Shadow map (opaque) | `src/client/shadows/dynamicshadowsrender.cpp` |
| `shadow/pass1_trans/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Shadow map (translucent) | `src/client/shadows/dynamicshadowsrender.cpp` |
| `shadow/pass2/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Shadow map post-processing | `src/client/shadows/dynamicshadowsrender.cpp` |
| `second_stage/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | **Post-processing** (bloom, tone mapping) | `src/client/render/secondstage.cpp` |
| `extract_bloom/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Bloom extraction | `src/client/render/secondstage.cpp` |
| `bloom_downsample/` | `opengl_fragment.glsl` | Bloom downsample | `src/client/render/secondstage.cpp` |
| `bloom_upsample/` | `opengl_fragment.glsl` | Bloom upsample | `src/client/render/secondstage.cpp` |
| `blur_h/`, `blur_v/` | `opengl_fragment.glsl` | Bloom blur passes | `src/client/render/secondstage.cpp` |
| `fxaa/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | **FXAA anti-aliasing** | `src/client/render/plain.cpp` |
| `volumetric_light/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Volumetric light (god rays) | `src/client/render/secondstage.cpp` |
| `update_exposure/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Auto-exposure calculation | `src/client/render/secondstage.cpp` |
| `cloud_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Cloud rendering | `src/client/clouds.cpp` |
| `stars_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Star rendering | `src/client/sky.cpp` |
| `minimap_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Minimap rendering | `src/client/minimap.cpp` |
| `selection_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Selection halo | `src/client/game.cpp` |
| `inventory_shader/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | Inventory item rendering | `src/gui/drawItemStack.cpp` |
| `3d_interlaced_merge/` | `opengl_vertex.glsl`, `opengl_fragment.glsl` | 3D interlaced stereoscopic | `src/client/render/sidebyside.cpp` |

---

## 19. irr/ — IrrlichtMt Engine

A **heavily modified fork** of the Irrlicht 3D rendering engine (79 source files + ~90 public headers).

### 19.1 Key Components

| Category | Key Files | Purpose |
|---|---|---|
| **Device/OS** | `CIrrDeviceSDL.cpp/h`, `os.cpp/h`, `Irrlicht.cpp` | SDL2/SDL3 device, window management |
| **OpenGL Drivers** | `COpenGLDriver.cpp/h`, `COpenGLExtensionHandler.cpp/h` | Legacy OpenGL driver |
| **OpenGL3 Driver** | `OpenGL3/DriverGL3.cpp/h` | Modern OpenGL 3.2+ core profile |
| **Unified GL** | `OpenGL/Driver.cpp/h`, `OpenGL/BufferObject.cpp/h` | Shared GL3/GLES2 code |
| **GLES2 Driver** | `OpenGLES2/DriverGLES2.cpp/h` | OpenGL ES 2.0+ (mobile) |
| **Mesh Loaders** | `CB3DMeshFileLoader.cpp/h`, `CGLTFMeshFileLoader.cpp/h`, `CXMeshFileLoader.cpp/h` | B3D, glTF, X file formats |
| **Video/Images** | `CNullDriver.cpp/h`, `CImageLoaderPNG/JPG.cpp/h` | Video driver base, image I/O |
| **Scene Graph** | `CSceneManager.cpp/h`, `CCameraSceneNode.cpp/h` | Scene management, camera |
| **File I/O** | `CFileSystem.cpp/h`, `CReadFile.cpp/h`, `CZipReader.cpp/h` | Virtual filesystem |
| **GUI** | `CGUIButton/EditBox/Environment/Font/*.cpp/h` | Built-in GUI widgets |
| **Public API** | `include/irrlicht.h`, `IVideoDriver.h`, `ISceneManager.h` | ~90 public headers |

### 19.2 Connection to src/

The Luanti engine includes IrrlichtMt headers via:
- `irrlichttypes.h` → `<irrTypes.h>` (via include path)
- `src/client/renderingengine.h` → `<IrrlichtDevice.h>`, `<IVideoDriver.h>`
- `src/client/shader.h` → `<IMaterialRendererServices.h>`, `<IShaderConstantSetCallBack.h>`
- `src/gui/*` → `<IGUIElement.h>`, `<IGUIEnvironment.h>`, etc.
- All scene nodes → `<ISceneNode.h>`, `<ISceneManager.h>`

---

## 20. lib/ — Vendored Libraries

| Library | Files | Purpose | Used By |
|---|---|---|---|
| **lua/** | `src/*.c/h` (29 files) | Lua 5.x scripting engine | `src/script/` |
| **jsoncpp/** | `jsoncpp.cpp`, `json/json.h` | JSON parser/generator | `src/convert_json.h`, `src/settings.h` |
| **gmp/** | `mini-gmp.c`, `mini-gmp.h` | GNU MP (mini) — arbitrary precision | `src/util/auth.h` |
| **sha256/** | `sha256.c`, `my_sha256.h` | SHA-256 hash | `src/util/hashing.h` |
| **bitop/** | `bit.cpp`, `bit.h` | Bitwise ops for Lua | `src/script/` |
| **lstrpack/** | `lstrpack.c`, `lstrpack.h` | Lua struct packing | `src/script/` (network) |
| **catch2/** | `catch_amalgamated.cpp/.hpp` | C++ test framework | `src/unittest/`, `src/test/` |
| **tiniergltf/** | `tiniergltf.hpp` | Minimal glTF 2.0 loader | `irr/src/CGLTFMeshFileLoader.cpp` |

---

## 21. games/ — Game Packages

Contains **`devtest/`** — the built-in Development Test game with 24+ test mods.

| Key Path | Purpose |
|---|---|
| `devtest/game.conf` | Game metadata |
| `devtest/mods/testnodes/` | Exhaustive test nodes (~130 textures, 12 Lua files) |
| `devtest/mods/testentities/` | Test entities (sprites, meshes, cubes) |
| `devtest/mods/unittests/` | Lua-side unit tests |
| `devtest/mods/testformspec/` | Formspec UI testing |
| `devtest/mods/basetools/` | Basic tools (pickaxes, axes, swords) |
| `devtest/mods/basenodes/` | Fundamental node types |
| `devtest/mods/soundstuff/` | Sound testing |
| `devtest/mods/gltf/` | glTF model loading tests |

---

## 22. Other Directories

### textures/ — Base Texture Pack
- `base/pack/` — ~90 built-in textures (HUD hearts, bubbles, crosshair, joystick, server icons, minimap masks, etc.)
- Loaded by `src/client/texturesource.cpp` at runtime

### fonts/ — Bundled Fonts
| Font | Style | License |
|---|---|---|
| Arimo | Regular, Bold, Italic, BoldItalic | Apache |
| Cousine | Regular, Bold, Italic, BoldItalic | Apache |
| DroidSansFallbackFull | Regular (CJK) | Apache |

### clientmods/ — Client-Side Mod Examples
| File | Purpose |
|---|---|
| `preview/init.lua` | CSM demo mod — demonstrates HUD, raycast, minimap, chat commands |
| `preview/example.lua` | Loads example files |
| `preview/settingtypes.txt` | CSM settings UI definition |

### docs/ — Documentation
| File | Purpose |
|---|---|
| `lua_api.md` | **Main Lua API reference** for modding |
| `client_lua_api.md` | Client-side modding API |
| `menu_lua_api.md` | Main menu Lua API |
| `world_format.md` | World database format |
| `protocol.txt` | Network protocol specification |
| `sscsm_api.md` | SSCSM API & security |
| `compiling/` | Build instructions (Linux, Windows, macOS) |
| `developing/` | Dev guides (profiling, OS compat) |
| `ides/` | IDE setup (VSCode, Visual Studio, JetBrains) |

### po/ — Translations (73 Languages)
Each contains a `luanti.po` file: ar, be, bg, ca, cs, da, de, el, es, fi, fr, he, hi, hu, id, it, ja, ko, nl, pl, pt, pt_BR, ro, ru, sk, sv, th, tr, uk, vi, zh_CN, zh_TW, and 41 more.

### cmake/ — CMake Modules
Find modules for: Vorbis, ncursesw, JsonCpp, GMP, Lua/LuaJIT, SQLite3, Gettext, Zstd, CURL. Plus `GenerateVersion.cmake` and `AndroidLibs.cmake`.

### misc/ — Platform Files
Linux .desktop, AppStream metadata, icons (SVG/ICO/ICNS/PNG), Windows manifest & resource, NSIS installer, macOS Info.plist & entitlements.

### android/ — Android Build
Gradle build system, SDL Java bindings, custom Java code (MainActivity, GameActivity, UnzipService), Android resources for 30+ locales.

### util/ — Development Utilities
Shell scripts: `updatepo.sh`, `bump_version.sh`, `stress_mapgen.sh`, test scripts. Python: `gather_git_credits.py`, `reorder_translation_commits.py`. Wireshark dissector. CI scripts. Windows cross-compilation toolchain. Xcode helpers.

---

## 23. Cross-Subsystem Dependency Patterns

### 23.1 Server ↔ Script Coupling
`server.h` is included by virtually every `script/cpp_api/` and `script/lua_api/` file. The script system has deep two-way dependency with the server — C++ calls into Lua for callbacks, Lua calls into C++ for world manipulation.

### 23.2 Client ↔ Script Coupling
`client/client.h` is included by `script/lua_api/l_client*.cpp` and `script/cpp_api/s_client*.cpp`. The client script API is more restricted than the server API.

### 23.3 Map ↔ Mapgen
`map.h`, `nodedef.h`, and `voxel.h` form the core that all mapgen implementations depend on. Every mapgen variant includes `mapgen.h`, `voxel.h`, `noise.h`, `map.h`, `nodedef.h`.

### 23.4 Server ↔ Network
`server.h` → `network/connection.h` → `network/mtp/` → `network/socket.h`. The network subsystem is largely self-contained; only `serveropcodes.h` and `clientopcodes.h` cross the boundary.

### 23.5 Client ↔ GUI
Deep coupling between `client/client.h`, `gui/guiFormSpecMenu.h`, `gui/guiEngine.h`. Many GUI widgets need `client/client.h` for rendering items.

### 23.6 Database Independence
`database/` is well-isolated — depends only on `database.h`, `remoteplayer.h`, and `server/player_sao.h`. Different backends only differ by their external library (SQLite, PostgreSQL, LevelDB, Redis).

### 23.7 Util as Universal Leaf
`util/` has no upward dependencies — it only depends on `irrlichttypes.h`, `constants.h`, `threading/`, and `debug.h`. It is the true library layer.

### 23.8 Client/Server Mirror
Both `client/` and `server/` have `activeobjectmgr.h/cpp` that inherit from the root `activeobjectmgr.h`. Both environments inherit from `environment.h`.

### 23.9 Circular Dependencies (resolved via forward declarations)
- `server.h` ↔ `server/clientiface.h`
- `nodedef.h` ↔ `mapnode.h`
- `server.h` ↔ `scripting_server.h` (forward declarations in headers, includes only in .cpp)

---

## 24. C++ ↔ Lua Bridge

### 24.1 C++ → Lua (Engine calls into scripts)

The C++ engine invokes Lua functions at specific events:
- `core.run_callbacks` — dispatches registered callbacks (30+ types)
- `core.button_handler` / `core.event_handler` — main menu formspec events
- `core.async_event_handler` — async job completion
- `core.job_processor` — async worker entry point

### 24.2 Lua → C++ (Scripts call engine API)

Via the `core` global table (aliased as `minetest`), set by C++ before `init.lua` runs:

| API Category | Examples | C++ Implementation |
|---|---|---|
| **Registration** | `core.register_item_raw`, `core.register_alias_raw` | `l_item.cpp`, `l_register.cpp` |
| **Environment** | `core.get_node_raw`, `core.set_node`, `core.add_entity` | `l_env.cpp` |
| **Player** | `core.get_player_by_name`, `core.check_player_privs` | `l_player.cpp` |
| **Settings** | `core.settings`, `core.get_worldpath` | `l_settings.cpp` |
| **Async** | `core.do_async_callback`, `core.cancel_async_callback` | `l_async.cpp` |
| **Sound** | `core.sound_play`, `core.sound_stop` | `l_client_sound.cpp` |
| **Formspec** | `core.show_formspec`, `core.update_formspec` | Forms via `guiFormSpecMenu` |
| **Auth** | `core.auth` (read/create/save/delete) | `l_auth.cpp` |
| **HTTP** | `core.http_fetch` | `l_http.cpp` |
| **VoxelManip** | `core.vmanip` (set in emerge env) | `l_vmanip.cpp` |
| **Serialization** | `core.serialize`, `core.encode_png`, `core.write_json` | `l_util.cpp` |

### 24.3 Security: C++ Functions Removed After Use

The builtin scripts intentionally nullify sensitive C++ functions after capturing them into local variables:
- `core.register_item_raw` → captured, then `nil`
- `core.unregister_item_raw` → captured, then `nil`
- `core.register_alias_raw` → captured, then `nil`
- `core.create_detached_inventory_raw` → captured, then `nil`
- `core.auth` → captured as `core_auth`, then `nil`
- `core.forceload_block` → captured, then `nil`
- `core.set_http_api_lua` → used, then `nil`

### 24.4 `builtin_shared` Pattern

Several files are loaded via `assert(loadfile(path)(builtin_shared))` instead of plain `dofile`. This passes a shared table between files that is NOT exposed to mods:
- `common/register.lua` — provides `make_registration()`, `make_registration_reverse()`
- `common/item_s.lua` — provides `cache_content_ids()`, direction helpers
- `game/item.lua` — uses `builtin_shared.check_attached_node`
- `game/falling.lua` — provides `check_attached_node`, uses `builtin_shared`
- All environment `register.lua` files — use `make_registration`

---

## 25. Build System

### 25.1 CMake Structure

```
CMakeLists.txt (root)
├── irr/CMakeLists.txt          → IrrlichtMt static library
├── src/CMakeLists.txt          → Main engine (client + server)
├── lib/*/CMakeLists.txt        → Vendored libraries (lua, jsoncpp, etc.)
├── cmake/Modules/*.cmake       → FindXXX modules
└── android/app/build.gradle    → Android APK build
```

### 25.2 Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_CLIENT` | ON | Build the client binary |
| `BUILD_SERVER` | OFF | Build the server binary |
| `BUILD_UNITTESTS` | ON | Build unit tests (Catch2) |
| `BUILD_BENCHMARKS` | OFF | Build benchmarks |
| `BUILD_DOCUMENTATION` | ON | Build Doxygen docs |
| `ENABLE_LTO` | ON (Release) | Link-Time Optimization |
| `RUN_IN_PLACE` | OFF (WIN32=ON) | Run from build directory |
| `BUILD_WITH_TRACY` | OFF | Tracy profiler integration |
| `ENABLE_CURL` | ON | cURL for HTTP/server list |
| `ENABLE_GETTEXT` | ON (client) | i18n support |
| `ENABLE_SOUND` | ON | OpenAL + Vorbis audio |
| `ENABLE_CURSES` | ON (server) | ncurses terminal console |
| `ENABLE_POSTGRESQL` | ON | PostgreSQL database backend |
| `ENABLE_LEVELDB` | ON | LevelDB database backend |
| `ENABLE_REDIS` | ON | Redis database backend |
| `ENABLE_PROMETHEUS` | OFF | Prometheus metrics |
| `ENABLE_SPATIAL` | ON | SpatialIndex AreaStore |
| `ENABLE_OPENSSL` | ON | OpenSSL libcrypto for SHA |
| `PRECOMPILE_HEADERS` | OFF | Precompiled headers (CMake 3.16+) |

### 25.3 Compiler Requirements

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 7.5 | C++17 support required |
| Clang | 7.0.1 | C++17 support required |
| MSVC | VS2017+ | Windows builds |

Build types: `Release`, `Debug`, `SemiDebug`, `RelWithDebInfo`, `MinSizeRel`.

### 25.4 Required Dependencies

These are **hard requirements** — the build will fail without them:

| Library | CMake Find | Package (Debian/Ubuntu) | Package (Fedora) | Used By |
|---------|-----------|------------------------|-------------------|---------|
| zlib | `find_package(ZLIB REQUIRED)` | `zlib1g-dev` | `zlib-devel` | Client + Server |
| zstd | `find_package(Zstd REQUIRED)` | `libzstd-dev` | `libzstd-devel` | Client + Server |
| SQLite3 | `find_package(SQLite3 REQUIRED)` | `libsqlite3-dev` | `sqlite-devel` | Client + Server |
| GMP | `find_package(GMP REQUIRED)` | `libgmp-dev` | `gmp-devel` | Client + Server |
| JSONCpp | `find_package(Json 1.0.0 REQUIRED)` | `libjsoncpp-dev` | `jsoncpp-devel` | Client + Server |
| Lua/LuaJIT | `find_package(Lua REQUIRED)` | `libluajit-5.1-dev` | `luajit-devel` | Client + Server |
| Threads | `find_package(Threads REQUIRED)` | (system) | (system) | Client + Server |

### 25.5 Client-Only Required Dependencies

| Library | CMake Find | Package (Debian/Ubuntu) | Package (Fedora) | Used By |
|---------|-----------|------------------------|-------------------|---------|
| Freetype | `find_package(Freetype REQUIRED)` | `libfreetype6-dev` | `freetype-devel` | Client |
| OpenGL | (via IrrlichtMt) | `libgl1-mesa-dev` | `mesa-libGL-devel` | Client |
| SDL2 | (via IrrlichtMt/vcpkg) | `libsdl2-dev` | `SDL2-devel` | Client |

### 25.6 Optional Dependencies

| Library | CMake Option | Package (Debian/Ubuntu) | Package (Fedora) | Enables |
|---------|-------------|------------------------|-------------------|---------|
| cURL | `ENABLE_CURL` | `libcurl4-openssl-dev` | `libcurl-devel` | Server list, HTTP API, content browser, update checker |
| OpenAL | `ENABLE_SOUND` | `libopenal-dev` | `openal-soft-devel` | Audio playback |
| Vorbis | `ENABLE_SOUND` | `libvorbis-dev` | `libvorbis-devel` | OGG sound decoding |
| OGG | `ENABLE_SOUND` | `libogg-dev` | `libogg-devel` | Vorbis dependency |
| Gettext | `ENABLE_GETTEXT` | `gettext` | `gettext-devel` | Internationalization |
| ncursesw | `ENABLE_CURSES` | `libncurses5-dev` | `ncurses-devel` | Server terminal console |
| PostgreSQL | `ENABLE_POSTGRESQL` | `libpq-dev` | `postgresql-devel` | PostgreSQL database backend |
| LevelDB | `ENABLE_LEVELDB` | `libleveldb-dev` | `leveldb-devel` | LevelDB database backend |
| Redis | `ENABLE_REDIS` | `libhiredis-dev` | `hiredis-devel` | Redis database backend |
| OpenSSL | `ENABLE_OPENSSL` | `libssl-dev` (>=3.0) | `openssl-devel` | Faster SHA, crypto layer (WIP) |
| Prometheus | `ENABLE_PROMETHEUS` | (build from source) | (build from source) | Server metrics |
| SpatialIndex | `ENABLE_SPATIAL` | `libspatialindex-dev` | `spatialindex-devel` | AreaStore backend |
| libjpeg | (via IrrlichtMt) | `libjpeg-dev` | `libjpeg-turbo-devel` | JPEG texture support |
| libpng | (via IrrlichtMt) | `libpng-dev` | `libpng-devel` | PNG texture support |
| iconv | (auto-detect) | (system) | (system) | Character encoding |

### 25.7 Vendored Libraries (lib/)

These libraries are bundled in the source tree and do not need separate installation:

| Library | Path | Purpose |
|---------|------|---------|
| bitop | `lib/bitop/` | Lua bit operations (non-LuaJIT only) |
| lstrpack | `lib/lstrpack/` | Lua struct packing |
| sha256 | `lib/sha256/` | SHA-256 implementation |
| catch2 | `lib/catch2/` | Testing framework (unittests/benchmarks only) |
| tiniergltf | `lib/tiniergltf/` | glTF model loading |

### 25.8 CMake Find Modules (cmake/Modules/)

Custom `FindXXX.cmake` modules for dependency detection:

| Module | Finds | Notes |
|--------|-------|-------|
| `FindCURL.cmake` | libcurl | |
| `FindGMP.cmake` | libgmp | |
| `FindGettextLib.cmake` | gettext | |
| `FindJson.cmake` | jsoncpp | |
| `FindLua.cmake` | Lua/LuaJIT | |
| `FindLuaJIT.cmake` | LuaJIT specifically | |
| `FindNcursesw.cmake` | ncursesw | |
| `FindSQLite3.cmake` | SQLite 3 | |
| `FindVorbis.cmake` | libvorbis + libogg | |
| `FindZstd.cmake` | zstd | |
| `GenerateVersion.cmake` | Version string | Generates `cmake_config_githash.h` |

### 25.9 Build Targets

| Target | Binary | Description |
|--------|--------|-------------|
| `luanti` | `luanti` | Client executable (also includes server) |
| `luantiserer` | `luantiserver` | Server-only executable (no rendering deps) |
| `EngineCommon` | (object library) | Shared object code between client and server |

### 25.10 Typical Linux Build Commands

```bash
# Server-only (minimal dependencies)
cmake -B build -DBUILD_SERVER=TRUE -DBUILD_CLIENT=FALSE -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Full client + server
cmake -B build -DBUILD_SERVER=TRUE -DBUILD_CLIENT=TRUE -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# With vcpkg (Windows/macOS cross-platform)
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### 25.11 Key External Dependencies (from vcpkg.json)

From `vcpkg.json`: zlib, zstd, openssl, curl[ssl], openal-soft, libvorbis, libogg, libjpeg-turbo, sqlite3, freetype, luajit, gmp, jsoncpp, gettext[tools], sdl2

---

## 26. Git Branch Strategy & Project Workflow

### 26.1 Branch Overview

| Branch | Purpose | Base |
|--------|---------|------|
| `main` | Primary development branch with all modifications | `origin/main` |
| `v7-overlay-settings` | v7: Secure connection overlay + settings toggle (tagged v7.0.0) | `main` |
| `v8-security-info-tab` | v8: Security Info settings tab with technical details (current) | `v7-overlay-settings` |
| `crypto-wip` | Encrypted communications layer (WIP) | `main` |

### 26.2 Versioning Convention

- Each major project map update increments the version in the header: `Map Version: v1`, `v2`, etc.
- When saving a branch snapshot, the branch name includes the version: `v7-overlay-settings`, `v8-security-info-tab`, etc.
- The project map is a **living document** — it must be updated whenever significant changes are made to the codebase.
- Git tags mark release versions: `v7.0.0`, etc.

### 26.3 Version History

| Version | Branch | Tag | Key Features |
|---------|--------|-----|-------------|
| v1-v4 | (pre-branch) | — | Project map, crypto layer, initial overlay |
| v5-v6 | (pre-branch) | — | Overlay integration, build script, server/client scripts |
| v7 | `v7-overlay-settings` | `v7.0.0` | Secure connection overlay + settings checkbox for toggle |
| v8 | `v8-security-info-tab` | — | Security Info settings tab showing technical connection details (TDD) |

### 26.4 v8 Feature: Security Info Settings Tab

**Branch:** `v8-security-info-tab`

**Purpose:** Add a new "Connection Security" tab in the Settings dialog that shows read-only technical details about the security of the connection to the server, so users can verify that the security is real.

**Key changes:**
- `src/network/connection_security.h` — New `ConnectionSecurityInfo` struct with detailed fields (encryption algorithm, key exchange, authentication, cipher suite, certificate status, forward secrecy, replay protection, protocol version, server address/port)
- `src/client/client.h` — Uses `ConnectionSecurityInfo` instead of raw `ConnectionSecurity` enum
- `src/client/gameui.h/cpp` — Syncs `ConnectionSecurityInfo` from client, uses it for overlay logic
- `src/network/clientpackethandler.cpp` — Populates `ConnectionSecurityInfo` during handshake, writes runtime settings for Lua
- `src/network/serverpackethandler.cpp` — Expanded security flags (FORWARD_SECRECY, AUTHENTICATED, REPLAY_PROTECTED)
- `src/defaultsettings.cpp` — Security info runtime settings defaults
- `builtin/settingtypes.txt` — New `[**Connection Security]` section under Client
- `builtin/common/settings/security_info_component.lua` — Custom Lua component displaying read-only security info
- `builtin/common/settings/dlg_settings.lua` — Integrates security info component
- `src/unittest/test_connection_security_info.cpp` — 22 TDD tests for `ConnectionSecurityInfo`
- `src/unittest/test_gameui.cpp` — Updated tests for new `ConnectionSecurityInfo` integration

**Design decisions:**
- Security info written to runtime settings (e.g., `security_info_state`, `security_info_encryption`) so the Lua settings dialog can read them without new C++/Lua API
- Settings are transient — not persisted to disk, only reflect current connection state
- Custom Lua component (like `shadows_component`) renders read-only info with color-coded status indicators
- `connectionSecurityInfoFromFlags()` builds `ConnectionSecurityInfo` from the extended security flags bitfield

### 26.5 Active Projects

| Project | Branch | Status | Description |
|---------|--------|--------|-------------|
| Encrypted Comms | `crypto-wip` | WIP (paused) | X25519/P-256 ECDH + AES-256-GCM + ECDSA, 2 parallel API layers need reconciliation |
| Linux Build Script | `feature/linux-build-script` | Active | Fully automated Linux build script with TDD (53 tests pass) |
| Project Map | (all branches) | Always maintained | Living document tracking all files, connections, and subsystems |

### 26.4 Docker Build Reference

The `Dockerfile` provides an Alpine-based server-only build reference:
- Base image: `alpine:3.23`
- Build deps: `git build-base cmake curl-dev zlib-dev zstd-dev sqlite-dev postgresql-dev hiredis-dev leveldb-dev gmp-dev jsoncpp-dev ninja`
- External builds: prometheus-cpp, libspatialindex, LuaJIT (from source)
- Build command: `cmake -B build -DBUILD_SERVER=TRUE -DENABLE_PROMETHEUS=TRUE -DBUILD_UNITTESTS=FALSE -DBUILD_CLIENT=FALSE -GNinja`
- Runtime deps: `curl gmp libstdc++ libgcc libpq jsoncpp zstd-libs sqlite-libs postgresql hiredis leveldb`
