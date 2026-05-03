# Luantis Rust Port Plan

> **Version:** v9.62-draft
> **Date:** 2026-05-04
> **Status:** Planning
> **Goal:** Port the Luantis game engine from C++ to Rust, module by module, sorted by easiness

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current Codebase Analysis](#2-current-codebase-analysis)
3. [Porting Philosophy](#3-porting-philosophy)
4. [Easiness Ranking — All Modules](#4-easiness-ranking--all-modules)
5. [Phase 1: Foundation Crates](#5-phase-1-foundation-crates)
6. [Phase 2: Core Engine](#6-phase-2-core-engine)
7. [Phase 3: Scripting & Lua](#7-phase-3-scripting--lua)
8. [Phase 4: Server & World](#8-phase-4-server--world)
9. [Phase 5: Client & Rendering](#9-phase-5-client--rendering)
10. [Rust Crate Architecture](#10-rust-crate-architecture)
11. [C++ ↔ Rust Interop Strategy](#11-c-rust-interop-strategy)
12. [Risk Register](#12-risk-register)
13. [Milestone Timeline](#13-milestone-timeline)
14. [Appendix: Module Reference](#14-appendix-module-reference)

---

## 1. Executive Summary

Luantis (fork of Minetest/Luanti) is a ~208,000-line C++17 voxel game engine with 373 header/implementation file pairs across 14 major modules. The codebase has deep coupling to IrrlichtMt (rendering) and Lua 5.1/LuaJIT (scripting), which are the two hardest porting challenges.

**Why Rust?**
- **Memory safety** — The C++ codebase has hundreds of raw pointer usages, manual lifetime management, and mutex patterns that Rust's borrow checker eliminates at compile time.
- **Fearless concurrency** — The server handles many concurrent tasks (network I/O, world generation, player management) that map naturally to Rust's async/await and `Send`/`Sync` guarantees.
- **Ecosystem** — Mature crates exist for every major dependency: `wgpu` for rendering, `mlua` for Lua bindings, `tokio` for async networking, `rusqlite` for databases, `glam` for math.
- **Performance** — Zero-cost abstractions, no garbage collection pauses, and LLVM backend optimization parity with C++.

**Strategy:** Port bottom-up by easiness — start with self-contained utility modules that have no internal dependencies, then work upward through the dependency graph. Each ported module gets a Rust crate with C FFI bindings so the C++ and Rust code can coexist during the transition. The last modules ported are the ones most tightly coupled to IrrlichtMt (rendering, GUI).

**Estimated total effort:** 18–24 months for a team of 2–3 developers, or 36–48 months solo.

---

## 2. Current Codebase Analysis

### 2.1 Codebase Size by Module

| Module | C++ Lines | % of Total | Files | Easiness |
|--------|-----------|-----------|-------|----------|
| `util/` | 10,089 | 4.8% | 55 | ★★★★★ Easiest |
| `threading/` | 940 | 0.5% | 9 | ★★★★★ Easiest |
| `database/` | 3,980 | 1.9% | 14 | ★★★★☆ Easy |
| `content/` | 1,504 | 0.7% | 8 | ★★★★☆ Easy |
| `network/` | 10,948 | 5.2% | 18 | ★★★☆☆ Moderate |
| `mapgen/` | 11,669 | 5.6% | 32 | ★★★☆☆ Moderate |
| `unittest/` | 12,107 | 5.8% | 58 | ★★★☆☆ Moderate |
| `server/` | 6,714 | 3.2% | 24 | ★★☆☆☆ Hard |
| Top-level `src/` | 49,717 | 23.8% | 155 | ★★☆☆☆ Hard |
| `script/` | 33,686 | 16.2% | 78+ | ★☆☆☆☆ Very Hard |
| `gui/` | 18,686 | 9.0% | 61 | ★☆☆☆☆ Very Hard |
| `client/` | 44,067 | 21.1% | 90 | ★☆☆☆☆ Very Hard |
| **Total** | **~208,479** | **100%** | **746** | |

### 2.2 Cross-Module Dependencies

```
                    ┌─────────────┐
                    │  Top-level  │  ← 49K lines, depends on everything
                    │  src/*.cpp  │
                    └──────┬──────┘
                           │
          ┌────────────────┼────────────────┐
          ▼                ▼                 ▼
    ┌──────────┐    ┌──────────┐      ┌──────────┐
    │  client  │───▶│  script  │◀─────│  server  │
    │ (44K)    │    │ (34K)    │      │ (7K)     │
    └────┬─────┘    └────┬─────┘      └────┬─────┘
         │               │                  │
         ▼               ▼                  ▼
    ┌──────────┐    ┌──────────┐      ┌──────────┐
    │   gui    │    │  mapgen  │      │ network  │
    │ (19K)    │    │ (12K)    │      │ (11K)    │
    └──────────┘    └──────────┘      └──────────┘
         │               │                  │
         └───────────────┼──────────────────┘
                         ▼
                    ┌──────────┐
                    │   util   │  ← Foundational, fewest dependencies
                    │ (10K)    │
                    └──────────┘
                         │
                    ┌──────────┐
                    │ database │  ← Clean abstraction layer
                    │ (4K)     │
                    └──────────┘
```

**Key insight:** `util/` and `threading/` have the fewest incoming dependencies and should be ported first. `script/` is the most deeply connected module (it touches every other module) and must be ported carefully to maintain Lua API compatibility.

### 2.3 Irrlicht Coupling

154 files depend on Irrlicht types. However, many of these are **type leaks** — even the server includes Irrlicht headers just for vector types (`v3f`, `v3s16`) and color types (`irr::video::SColor`). This is a design flaw that the Rust port will fix by replacing these with Rust-native types (`glam::Vec3`, etc.).

**Irrlicht dependency by severity:**
- **Expected (client/gui):** 57 files — rendering and GUI naturally depend on the rendering engine
- **Type leaks (util/server/network/database):** 30 files — only use vector/color types, easily decoupled
- **Deep coupling (client/render):** ~15 files — shader pipeline, mesh rendering, deeply intertwined

### 2.4 Lua Coupling

138 files call `lua_*` functions directly. 38 Lua API binding modules in `script/lua_api/` (20,852 lines). 22,654 lines of Lua runtime code in `builtin/` that must continue to work unchanged.

The `mlua` crate provides excellent Lua 5.1/LuaJIT compatibility and will be the primary bridging mechanism.

---

## 3. Porting Philosophy

### 3.1 Principles

1. **Incremental, never big-bang.** Each module is ported as a separate Rust crate with C FFI bindings so C++ and Rust coexist. The game remains buildable and runnable at every step.

2. **Bottom-up by easiness.** Start with leaf modules (no internal dependencies), work toward root modules (many dependencies). This ensures every ported module has a stable foundation.

3. **API compatibility first, idiomatic Rust second.** During the transition, Rust crates expose C-compatible APIs so C++ callers are unaffected. Once the C++ side is removed, the Rust API can be refactored idiomatically.

4. **Lua API compatibility is non-negotiable.** The entire `builtin/` Lua runtime and the mod ecosystem depend on the current Lua API surface. The `mlua` crate will maintain byte-compatible behavior.

5. **Test-driven porting.** Port the unit tests alongside each module. The existing C++ test suite in `unittest/` becomes the validation benchmark.

6. **Replace Irrlicht entirely.** IrrlichtMt is unmaintained and a porting dead-end. The Rust client will use `wgpu` for rendering, `winit` for windowing, and `egui` or a custom FormSpec renderer for UI.

### 3.2 What NOT to Port

These components will be replaced, not ported:

| Component | Lines | Replacement | Reason |
|-----------|-------|-------------|--------|
| `irrlicht_changes/` | 2,070 | wgpu/egui | Irrlicht patches — obsolete |
| Irrlicht type system | N/A | `glam`/`nalgebra` | Type decoupling |
| Bundled `lib/lua/` | ~4,000 | `mlua` crate | Use maintained crate |
| Bundled `lib/jsoncpp/` | ~2,500 | `serde_json` | Use maintained crate |
| Bundled `lib/sha256/` | ~200 | `sha2` crate | Use maintained crate |
| Bundled `lib/gmp/` | ~1,000 | `rug`/`num-bigint` | Use maintained crate |
| `build_linux.sh`, `build_env.sh` | ~1,500 | `cargo build` | Rust toolchain replaces CMake |

---

## 4. Easiness Ranking — All Modules

Sorted from easiest to hardest, with rationale:

| Rank | Module | Lines | Why This Easiness Level |
|------|--------|-------|------------------------|
| 1 | `threading/` | 940 | Trivial — Rust std::thread replaces custom Thread/Mutex/Semaphore/Event classes directly. No external deps. Pure utility. |
| 2 | `util/` | 10,089 | Straightforward — serialization, string ops, numeric helpers, PNG read. Most are pure functions. Need to replace Irrlicht vector types with `glam` first. |
| 3 | `database/` | 3,980 | Clean abstraction — `Database` abstract class maps to a Rust trait. 5 backends (SQLite3, PostgreSQL, LevelDB, Redis, Dummy). Each backend is independent. Port trait + SQLite3 first. |
| 4 | `content/` | 1,504 | Small & isolated — mod/content discovery and loading. Minimal dependencies (just `util/` and filesystem). |
| 5 | `network/` | 10,948 | Moderate — well-structured packet protocol. Some Irrlicht type leaks (v3f in packets). The MT protocol layer (`network/mtp/`) is self-contained. Async with `tokio` will simplify the threading model significantly. |
| 6 | `mapgen/` | 11,669 | Moderate — self-contained algorithms (7 mapgen variants, biome/ore/decoration/schematic systems). Depends on `util/` (noise, serialization) and `script/` (Lua callbacks). Port algorithm core first, script integration later. |
| 7 | `unittest/` | 12,107 | Moderate — port alongside each module. 58 test files. Each test file depends on the module it tests. Convert to `#[cfg(test)]` modules. |
| 8 | Top-level `src/` core | 49,717 | Hard — the "everything else" bucket. Contains map/voxel engine, server main loop, environment, inventory, node/item definitions, settings, collision detection. Highly interconnected. Must be decomposed and ported subsystem by subsystem. |
| 9 | `server/` | 6,714 | Hard — depends on network, scripting, map, database, environment. The server loop is a complex state machine. However, the server has no Irrlicht dependency (beyond type leaks), which makes it more feasible than the client. |
| 10 | `script/` | 33,686 | Very Hard — the single most complex module. 38 Lua API binding modules, deep C++/Lua interop, touches every subsystem. Must maintain byte-compatible Lua API. `mlua` helps but the sheer surface area is enormous. |
| 11 | `gui/` | 18,686 | Very Hard — 100% Irrlicht GUI toolkit dependency. FormSpec is a custom declarative UI format parsed and rendered by 29 Irrlicht-based widgets. Must build a new FormSpec renderer from scratch. |
| 12 | `client/` | 44,067 | Very Hard — the largest module. Tightly coupled to Irrlicht rendering. Contains the main game loop (game.cpp at 3,820 lines), mesh generation, shader pipeline, sound, camera, HUD. Requires a complete rendering rewrite using `wgpu`. |

---

## 5. Phase 1: Foundation Crates

**Timeline:** Months 1–4
**Goal:** Port all leaf modules that have no internal dependencies. Create the Rust workspace and crate structure.

### 5.1 `luantis-math` — Replace Irrlicht Types

**Priority:** ★★★★★ — Must be done FIRST because every other module uses vector/color types.

**Current state:** Irrlicht types (`irr::core::vector3d<f32>`, `irr::video::SColor`, `irr::core::aabbox3d`) are used as fundamental types across the entire codebase, including in modules that have no business depending on a rendering engine (server, network, database).

**Port plan:**
```
luantis-math/
├── src/
│   ├── lib.rs
│   ├── vector.rs      // v2f, v3f, v3s16, v3s32 → glam::Vec2, Vec3, IVec3
│   ├── color.rs       // irr::video::SColor → Color(r,g,b,a)
│   ├── aabb.rs        // irr::core::aabbox3d → AABB<f32>
│   ├── matrix.rs      // irr::core::matrix4 → glam::Mat4
│   ├── quaternion.rs  // irr::core::quaternion → glam::Quat
│   └── c_api.rs       // C FFI bindings for C++ interop
└── Cargo.toml
```

**Key decisions:**
- Use `glam` for SIMD-optimized vector math (already the Rust game industry standard)
- Define C-compatible type aliases so C++ code can call into Rust math
- Port `irr_v3d.h`, `irr_v2d.h`, `irr_aabb3d.h` type definitions

**Estimated effort:** 2 weeks

### 5.2 `luantis-threading`

**Priority:** ★★★★★

**Current state:** Custom `Thread`, `Mutex`, `Semaphore`, `Event` classes wrapping pthreads/C++ threads.

**Port plan:**
```
luantis-threading/
├── src/
│   ├── lib.rs
│   ├── thread.rs      // → std::thread
│   ├── mutex.rs       // → parking_lot::Mutex (smaller, faster than std)
│   ├── semaphore.rs   // → tokio::sync::Semaphore
│   └── event.rs       // → std::sync::Condvar + parking_lot::Mutex
└── Cargo.toml
```

This module is so simple it barely needs its own crate — but it establishes the pattern for all subsequent crates.

**Estimated effort:** 1 week

### 5.3 `luantis-serialization`

**Priority:** ★★★★★

**Current state:** Binary serialization system in `util/serialize.h` (header-only, ~1,200 lines). Custom `StreamReadBuffer`/`StreamWriteBuffer` classes. Network packet serialization.

**Port plan:**
```
luantis-serialization/
├── src/
│   ├── lib.rs
│   ├── stream.rs      // Read/Write stream traits
│   ├── binary.rs      // Binary serialization (little-endian)
│   ├── compress.rs    // zlib/zstd compression → flate2, zstd crates
│   ├── json.rs        // → serde_json
│   └── c_api.rs       // C FFI
└── Cargo.toml
```

**Key decisions:**
- Use `serde` for derive-based serialization where possible
- Maintain binary format compatibility for network protocol and save files
- Implement `Read`/`Write` traits instead of custom stream classes

**Estimated effort:** 2 weeks

### 5.4 `luantis-util`

**Priority:** ★★★★☆

**Current state:** 55 files of string utilities, numeric helpers, PNG read/write, container types, PRNG, logging, timers, etc.

**Port plan:**
```
luantis-util/
├── src/
│   ├── lib.rs
│   ├── string.rs      // wide_to_utf8, trim, split, etc.
│   ├── numeric.rs     // clamp, rangelim, myround, etc.
│   ├── container.rs   // LRUCache, MutexedVar, etc.
│   ├── png.rs         // → png crate
│   ├── prng.rs        // Pcg32 PRNG → rand crate
│   ├── logging.rs     // → tracing crate (structured logging)
│   ├── timer.rs       // → std::time::Instant
│   ├── traits.rs      // Shared traits
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Replace custom logging with `tracing` crate (structured, async-aware)
- Replace custom PRNG with `rand` crate
- Replace custom container types with standard Rust collections + `lru` crate

**Estimated effort:** 3 weeks

---

## 6. Phase 2: Core Engine

**Timeline:** Months 4–9
**Goal:** Port the core data layer — databases, network protocol, content definitions, and map generation.

### 6.1 `luantis-database`

**Priority:** ★★★★☆

**Current state:** Abstract `Database` class with 5 backends. The `MapDatabase` trait handles world storage. `PlayerDatabase` and `AuthDatabase` are separate.

**Port plan:**
```
luantis-database/
├── src/
│   ├── lib.rs
│   ├── traits.rs      // Database, MapDatabase, PlayerDatabase, AuthDatabase traits
│   ├── sqlite3.rs     // → rusqlite (priority — most common backend)
│   ├── postgresql.rs  // → tokio-postgres
│   ├── leveldb.rs     // → leveldb crate
│   ├── redis.rs       // → redis crate
│   ├── files.rs       // File-based backend
│   ├── dummy.rs       // In-memory test backend
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Define Rust traits that mirror the C++ abstract classes
- Port SQLite3 backend first (it's the default and most widely used)
- Use `rusqlite` for synchronous access, `tokio-postgres` for async PostgreSQL
- Maintain database format compatibility so existing worlds work

**Estimated effort:** 3 weeks (SQLite3 first), +2 weeks per additional backend

### 6.2 `luantis-network`

**Priority:** ★★★☆☆

**Current state:** Minetest Protocol (MTP) implementation with connection management, packet serialization, encryption, and multiple packet handlers. Split into `network/mtp/` (transport) and `network/` (protocol/packets).

**Port plan:**
```
luantis-network/
├── src/
│   ├── lib.rs
│   ├── protocol.rs     // NetworkProtocol — packet type definitions
│   ├── packet.rs       // NetworkPacket — serialization/deserialization
│   ├── connection.rs   // Connection state machine
│   ├── socket.rs       // → tokio::net::UdpSocket
│   ├── mtp/
│   │   ├── mod.rs
│   │   ├── impl.rs     // MTP implementation
│   │   ├── threads.rs  // Send/receive threads → tokio tasks
│   │   └── peer.rs     // Peer state management
│   ├── security.rs     // Encryption layer (AES-256-GCM, Ed25519)
│   ├── packethandler.rs // Packet dispatch
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Use `tokio` for async networking — eliminates the custom send/receive thread pool
- Use `ring` or `rustls` for crypto (AES-256-GCM, Ed25519, SHA-256)
- Maintain wire protocol compatibility so Rust servers can serve C++ clients and vice versa
- Replace `network/mtp/threads.cpp` custom threading with `tokio::spawn`

**Estimated effort:** 6 weeks

### 6.3 `luantis-content`

**Priority:** ★★★★☆

**Current state:** Mod/content discovery, loading, and management. Small and isolated module.

**Port plan:**
```
luantis-content/
├── src/
│   ├── lib.rs
│   ├── mod_spec.rs     // ModSpec — mod metadata
│   ├── subgames.rs     // GameSpec — game definition
│   ├── content.rs      // ContentPackage — generic content
│   ├── mods.rs         // Mod loading and sorting
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Use Rust's `std::fs` and `std::path` for filesystem operations
- Maintain mod.conf / game.conf format compatibility
- Support modpack archives (.zip)

**Estimated effort:** 2 weeks

### 6.4 `luantis-mapgen`

**Priority:** ★★★☆☆

**Current state:** 7 map generation algorithms (v5, v6, v7, flat, fractal, carpathian, valleys) plus biome/ore/decoration/schematic systems. Depends on noise functions and Lua callbacks.

**Port plan:**
```
luantis-mapgen/
├── src/
│   ├── lib.rs
│   ├── noise.rs        // Noise parameters and generation → noise crate
│   ├── mapgen.rs       // Mapgen trait + base class
│   ├── v5.rs           // MapgenV5
│   ├── v6.rs           // MapgenV6
│   ├── v7.rs           // MapgenV7 (default)
│   ├── flat.rs         // MapgenFlat
│   ├── fractal.rs      // MapgenFractal
│   ├── carpathian.rs   // MapgenCarpathian
│   ├── valleys.rs      // MapgenValleys
│   ├── biome.rs        // BiomeManager
│   ├── ore.rs          // OreManager
│   ├── decoration.rs   // DecorationManager
│   ├── schematic.rs    // SchematicManager
│   ├── cave.rs         // Cave generation
│   ├── dungeon.rs      // Dungeon generation
│   ├── tree.rs         // Tree generation
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Use the `noise` crate for Perlin/Simplex noise (or `simplex-noise-rs`)
- Port the core algorithms directly — they are well-tested mathematical code
- Lua callback integration is deferred to Phase 3
- MapgenV7 (the default) should be ported first for maximum coverage

**Estimated effort:** 6 weeks

### 6.5 `luantis-voxel`

**Priority:** ★★★☆☆

**Current state:** Voxel data structures (`VoxelManipulator`, `MapNode`, `MapBlock`, `MapSector`, `Map`, `ServerMap`). These are the core data structures that everything else builds on.

**Port plan:**
```
luantis-voxel/
├── src/
│   ├── lib.rs
│   ├── mapnode.rs      // MapNode — single voxel (content + param1 + param2)
│   ├── voxel.rs        // VoxelManipulator — bulk voxel editing
│   ├── mapblock.rs     // MapBlock — 16×16×16 chunk of voxels
│   ├── mapsector.rs    // MapSector — 16×16 grid of MapBlocks
│   ├── map.rs          // Map — the entire voxel world
│   ├── servermap.rs    // ServerMap — server-side world with save/load
│   ├── nodedef.rs      // NodeDefManager — content type definitions
│   ├── itemdef.rs      // ItemDefManager — item type definitions
│   ├── craftdef.rs     // Craft definitions
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- `MapNode` is just a `u32` with bitfields — trivial to port
- `MapBlock` is a 16³ array of MapNodes with metadata — use a fixed-size array
- `VoxelManipulator` is the workhorse for mapgen and lighting — port algorithm carefully
- Database integration via the `luantis-database` crate trait

**Estimated effort:** 5 weeks

---

## 7. Phase 3: Scripting & Lua

**Timeline:** Months 9–15
**Goal:** Port the Lua scripting engine. This is the highest-risk phase due to the massive API surface and deep interop requirements.

### 7.1 `luantis-script-core`

**Priority:** ★★☆☆☆ (Hard but foundational for server)

**Current state:** The scripting system has three layers:
- `script/common/` (5K lines) — type conversion, content serialization between C++ and Lua
- `script/cpp_api/` (6K lines) — C++ framework for calling into Lua (ScriptApiBase, ScriptApiEnv, etc.)
- `script/lua_api/` (21K lines) — 38 Lua API binding modules (the actual functions exposed to Lua)

**Port plan:**
```
luantis-script/
├── src/
│   ├── lib.rs
│   ├── engine.rs        // ScriptEngine — owns the Lua state
│   ├── types.rs         // Type conversion (Lua ↔ Rust)
│   ├── cpp_api/
│   │   ├── mod.rs
│   │   ├── base.rs      // ScriptApiBase
│   │   ├── env.rs       // ScriptApiEnv — environment callbacks
│   │   ├── script.rs    // ScriptApiScript — script lifecycle
│   │   └── s_async.rs   // Async scripting
│   ├── lua_api/
│   │   ├── mod.rs
│   │   ├── l_areastore.rs
│   │   ├── l_async.rs
│   │   ├── l_auth.rs
│   │   ├── l_base.rs
│   │   ├── l_camera.rs
│   │   ├── l_client.rs
│   │   ├── l_craft.rs
│   │   ├── l_env.rs     // ← largest, 1,621 lines
│   │   ├── l_http.rs
│   │   ├── l_inventory.rs
│   │   ├── l_item.rs
│   │   ├── l_mapgen.rs
│   │   ├── l_metadata.rs
│   │   ├── l_minimap.rs
│   │   ├── l_nodemeta.rs
│   │   ├── l_nodetimer.rs
│   │   ├── l_noise.rs
│   │   ├── l_object.rs  // ← largest, 3,001 lines
│   │   ├── l_particles.rs
│   │   ├── l_playermeta.rs
│   │   ├── l_rollback.rs
│   │   ├── l_server.rs
│   │   ├── l_settings.rs
│   │   ├── l_storage.rs
│   │   ├── l_util.rs
│   │   ├── l_vmanip.rs
│   │   └── ... (remaining bindings)
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Use `mlua` crate with `lua51` feature for Lua 5.1 compatibility
- Port `script/common/` type conversions first — they define the contract
- Port `script/cpp_api/` framework second — it defines how Rust calls Lua
- Port `script/lua_api/` bindings one by one, testing each against `builtin/` Lua scripts
- **Critical:** Every Lua API binding must be byte-compatible. The 22,654 lines of `builtin/` Lua code must work unchanged.

**Binding port order (by easiness, independent first):**

| Order | Binding | Lines | Dependencies | Notes |
|-------|---------|-------|-------------|-------|
| 1 | `l_util` | ~300 | None | Pure utility functions |
| 2 | `l_settings` | ~250 | Settings struct | Get/set config values |
| 3 | `l_noise` | ~400 | `luantis-mapgen` | Noise parameter objects |
| 4 | `l_areastore` | ~350 | None | Spatial area storage |
| 5 | `l_storage` | ~200 | `luantis-database` | Mod storage |
| 6 | `l_metadata` | ~250 | Node/player metadata | Metadata get/set |
| 7 | `l_auth` | ~200 | `luantis-database` | Auth database |
| 8 | `l_nodetimer` | ~150 | `luantis-voxel` | Node timers |
| 9 | `l_nodemeta` | ~250 | `luantis-voxel` | Node metadata |
| 10 | `l_craft` | ~500 | `luantis-voxel` | Crafting system |
| 11 | `l_item` | ~600 | `luantis-voxel` | Item definitions |
| 12 | `l_inventory` | ~700 | `luantis-voxel` | Inventory management |
| 13 | `l_mapgen` | ~2,100 | `luantis-mapgen` | World generation Lua API |
| 14 | `l_http` | ~300 | Network/HTTP | HTTP requests |
| 15 | `l_particles` | ~400 | Client/server | Particle spawning |
| 16 | `l_vmanip` | ~800 | `luantis-voxel` | Voxel manipulation |
| 17 | `l_object` | ~3,000 | Server active objects | **Largest binding** |
| 18 | `l_env` | ~1,600 | Server environment | **Second largest** |
| 19 | `l_server` | ~400 | Server | Server management |
| 20 | `l_client` | ~600 | Client | Client-side API |
| 21 | `l_camera` | ~300 | Client | Camera control |
| 22 | `l_mainmenu` | ~1,200 | GUI | Main menu API |

**Estimated effort:** 12–16 weeks

---

## 8. Phase 4: Server & World

**Timeline:** Months 12–18
**Goal:** Port the server executable. The server is more feasible than the client because it has no Irrlicht dependency.

### 8.1 `luantis-server`

**Priority:** ★★☆☆☆ (Hard but achievable)

**Current state:** The server is a complex state machine managing player connections, world simulation, active objects, environment, chat, and more. The main `server.cpp` is 4,534 lines.

**Port plan:**
```
luantis-server/
├── src/
│   ├── lib.rs
│   ├── main.rs          // Server entry point
│   ├── server.rs        // Server state machine (decompose from 4,534-line monolith)
│   ├── environment.rs   // ServerEnvironment — world simulation tick
│   ├── clientiface.rs   // ClientInterface — connected player management
│   ├── activeobject.rs  // ActiveObjectManager — entity management
│   ├── player_sao.rs    // PlayerSAO — server-side player object
│   ├── luaentity_sao.rs // LuaEntitySAO — scripted entity object
│   ├── ban.rs           // Ban manager
│   ├── rollback.rs      // Rollback system
│   ├── chat.rs          // Chat system
│   ├── console.rs       // Server console → crossterm
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Decompose `server.cpp` into focused modules — it's currently a 4,500-line monolith
- Use `tokio` for the server main loop instead of the custom thread pool
- The server console can use `crossterm` for terminal UI (replacing ncurses)
- Server is the first "executable" crate — it links all the library crates together

**Estimated effort:** 8 weeks

### 8.2 `luantis-inventory`

**Priority:** ★★★☆☆

**Current state:** Inventory system with items, stacks, lists, and actions. Used by both client and server.

**Port plan:**
```
luantis-inventory/
├── src/
│   ├── lib.rs
│   ├── item.rs          // ItemStack — item + count + wear + metadata
│   ├── list.rs          // InventoryList — list of ItemStacks
│   ├── inventory.rs     // Inventory — collection of InventoryLists
│   ├── action.rs        // InventoryAction — move, swap, drop
│   ├── loc.rs           // InventoryLocation — where an inventory lives
│   └── c_api.rs
└── Cargo.toml
```

**Estimated effort:** 3 weeks

### 8.3 `luantis-settings`

**Priority:** ★★★★☆

**Current state:** Settings system with type-safe get/set, config file parsing, and settings registry. Used everywhere.

**Port plan:**
```
luantis-settings/
├── src/
│   ├── lib.rs
│   ├── settings.rs      // Settings struct — key-value store with types
│   ├── settingtypes.rs  // Setting type definitions
│   ├── defaults.rs      // Default settings values
│   └── c_api.rs
└── Cargo.toml
```

**Estimated effort:** 2 weeks

---

## 9. Phase 5: Client & Rendering

**Timeline:** Months 15–24
**Goal:** Port the client. This requires building an entirely new rendering pipeline using `wgpu`, as IrrlichtMt is not being ported.

### 9.1 `luantis-render`

**Priority:** ★☆☆☆☆ (Very Hard — complete rewrite)

**Current state:** The rendering pipeline is deeply embedded in IrrlichtMt. Custom shaders, mesh rendering, shadow mapping, sky rendering, etc.

**Port plan (new implementation, not a direct port):**
```
luantis-render/
├── src/
│   ├── lib.rs
│   ├── renderer.rs      // Main renderer — owns wgpu device/queue
│   ├── pipeline.rs      // Render pipeline management
│   ├── vertex.rs        // Vertex formats and buffers
│   ├── texture.rs       // Texture loading and management
│   ├── shader.rs        // Shader compilation (WGSL)
│   ├── mesh.rs          // Mesh rendering
│   ├── sky.rs           // Sky rendering
│   ├── shadows.rs       // Shadow mapping
│   ├── hud.rs           // HUD rendering
│   ├── minimap.rs       // Minimap rendering
│   ├── particles.rs     // Particle rendering
│   └── camera.rs        // Camera and view/projection matrices
└── Cargo.toml
```

**Key decisions:**
- Use `wgpu` for cross-platform GPU access (Vulkan, Metal, DX12, WebGL)
- Rewrite all shaders in WGSL (from Irrlicht's shader format)
- Consider `rend3` or `bevy_render` as a higher-level abstraction on top of `wgpu`
- This is essentially a **new implementation** guided by the existing rendering behavior, not a line-by-line port

**Estimated effort:** 16–20 weeks

### 9.2 `luantis-gui`

**Priority:** ★☆☆☆☆ (Very Hard)

**Current state:** FormSpec is a custom declarative UI format with 29+ widget types, all rendered through Irrlicht's GUI system.

**Port plan:**
```
luantis-gui/
├── src/
│   ├── lib.rs
│   ├── formspec.rs      // FormSpec parser — parse the spec string into widgets
│   ├── theme.rs         // UI theme/style
│   ├── widgets/
│   │   ├── mod.rs
│   │   ├── button.rs
│   │   ├── label.rs
│   │   ├── image.rs
│   │   ├── field.rs     // Text input
│   │   ├── textarea.rs
│   │   ├── checkbox.rs
│   │   ├── dropdown.rs
│   │   ├── listbox.rs
│   │   ├── tabheader.rs
│   │   ├── scrollbar.rs
│   │   ├── table.rs
│   │   ├── inventory.rs // Inventory slots
│   │   └── ... (remaining widgets)
│   ├── renderer.rs      // Widget rendering using luantis-render
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Parse FormSpec strings into a widget tree (pure parsing, no rendering dependency)
- Render widgets using `luantis-render` (wgpu-based), not `egui`
- Consider `egui` for debug/developer UI only, not for FormSpec
- FormSpec format compatibility is essential — all existing mods use it

**Estimated effort:** 12–16 weeks

### 9.3 `luantis-sound`

**Priority:** ★★★☆☆ (Moderate — well-bounded)

**Current state:** OpenAL-based sound engine with 3D positional audio, sound groups, and occlusion.

**Port plan:**
```
luantis-sound/
├── src/
│   ├── lib.rs
│   ├── engine.rs        // SoundEngine — manages audio context
│   ├── buffer.rs        // SoundBuffer — decoded audio data
│   ├── source.rs        // SoundSource — positional audio source
│   ├── loader.rs        // Audio file loading → lewton (Ogg), symphonia (general)
│   ├── ogg.rs           // Ogg Vorbis decoding
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Use `alto` for OpenAL compatibility, or rewrite with `oddio` for a pure-Rust solution
- `lewton` for Ogg Vorbis decoding (pure Rust, no libvorbis dependency)
- Maintain sound definition format compatibility

**Estimated effort:** 3 weeks

### 9.4 `luantis-client`

**Priority:** ★☆☆☆☆ (Very Hard — depends on everything)

**Current state:** The client game loop (`game.cpp`, 3,820 lines), mesh generation, camera, input handling, network client, connection to server.

**Port plan:**
```
luantis-client/
├── src/
│   ├── lib.rs
│   ├── main.rs          // Client entry point
│   ├── game.rs          // Game loop (decompose from 3,820-line monolith)
│   ├── client.rs        // Network client — connection to server
│   ├── camera.rs        // Camera controller
│   ├── input.rs         // Input handling → winit
│   ├── meshgen.rs       // Client-side mesh generation from voxel data
│   ├── hud.rs           // HUD management
│   ├── minimap.rs       // Minimap
│   ├── particles.rs     // Client-side particles
│   ├── sky.rs           // Sky rendering management
│   ├── shadows.rs       // Shadow management
│   ├── media.rs         // Client media loading
│   └── c_api.rs
└── Cargo.toml
```

**Key decisions:**
- Use `winit` for window management and input (replaces SDL2/Irrlicht windowing)
- Decompose `game.cpp` into a proper game loop with state management
- The client is the final piece — it depends on all other crates

**Estimated effort:** 12–16 weeks

---

## 10. Rust Crate Architecture

### 10.1 Workspace Structure

```
luantis/
├── Cargo.toml                  # Workspace root
├── crates/
│   ├── luantis-math/           # Phase 1: Vector/color/matrix types
│   ├── luantis-threading/      # Phase 1: Threading primitives
│   ├── luantis-serialization/  # Phase 1: Binary/JSON serialization
│   ├── luantis-util/           # Phase 1: General utilities
│   ├── luantis-database/       # Phase 2: Database backends
│   ├── luantis-network/        # Phase 2: Network protocol
│   ├── luantis-content/        # Phase 2: Mod/content management
│   ├── luantis-mapgen/         # Phase 2: World generation
│   ├── luantis-voxel/          # Phase 2: Voxel data structures
│   ├── luantis-inventory/      # Phase 4: Inventory system
│   ├── luantis-settings/       # Phase 4: Settings management
│   ├── luantis-script/         # Phase 3: Lua scripting engine
│   ├── luantis-server/         # Phase 4: Server executable
│   ├── luantis-render/         # Phase 5: Rendering pipeline
│   ├── luantis-gui/            # Phase 5: FormSpec GUI
│   ├── luantis-sound/          # Phase 5: Audio engine
│   └── luantis-client/         # Phase 5: Client executable
├── src/                        # Original C++ code (kept during transition)
├── builtin/                    # Lua runtime (unchanged)
├── games/                      # Game data (unchanged)
└── docs/
    └── RUST_PORT_PLAN.md       # This document
```

### 10.2 Dependency Graph (Rust Crates)

```
luantis-math
    ↑
luantis-threading  luantis-serialization  luantis-util
    ↑                   ↑                     ↑
    └───────────────────┼─────────────────────┘
                        ↑
              luantis-database  luantis-content
                        ↑               ↑
                        └───────┬───────┘
                                ↑
                    luantis-network  luantis-voxel
                                ↑           ↑
                                └─────┬─────┘
                                      ↑
                    luantis-inventory  luantis-settings
                                      ↑
                              luantis-mapgen
                                      ↑
                              luantis-script
                                ↑     ↑
                 ┌──────────────┘     └──────────────┐
                 ↑                                    ↑
          luantis-server                       luantis-render
                                                    ↑
                                            luantis-sound
                                                 ↑
                                            luantis-gui
                                                 ↑
                                          luantis-client
```

### 10.3 Key Rust Crate Dependencies

| Crate | Rust Dependencies |
|-------|-------------------|
| `luantis-math` | `glam` |
| `luantis-threading` | `parking_lot` |
| `luantis-serialization` | `serde`, `serde_json`, `flate2`, `zstd`, `byteorder` |
| `luantis-util` | `luantis-math`, `luantis-serialization`, `tracing`, `rand`, `png` |
| `luantis-database` | `luantis-util`, `rusqlite`, `tokio-postgres`, `leveldb`, `redis` |
| `luantis-network` | `luantis-util`, `luantis-math`, `tokio`, `ring`, `aes-gcm` |
| `luantis-content` | `luantis-util` |
| `luantis-mapgen` | `luantis-util`, `luantis-voxel`, `noise` |
| `luantis-voxel` | `luantis-math`, `luantis-util`, `luantis-database`, `luantis-serialization` |
| `luantis-inventory` | `luantis-util`, `luantis-voxel` |
| `luantis-settings` | `luantis-util`, `luantis-serialization` |
| `luantis-script` | `luantis-util`, `luantis-voxel`, `luantis-mapgen`, `luantis-inventory`, `mlua` |
| `luantis-server` | All above + `tokio`, `crossterm` |
| `luantis-render` | `luantis-math`, `luantis-util`, `wgpu`, `rend3`, `image` |
| `luantis-sound` | `luantis-util`, `alto` or `oddio`, `lewton` |
| `luantis-gui` | `luantis-util`, `luantis-render`, `luantis-inventory` |
| `luantis-client` | All above + `winit` |

---

## 11. C++ ↔ Rust Interop Strategy

### 11.1 Coexistence During Transition

During the porting process, C++ and Rust code must coexist. The strategy:

1. **Each Rust crate exposes a C API** via `#[no_mangle] extern "C"` functions
2. **C++ code calls Rust via `extern "C"` declarations** in header files
3. **Rust code calls C++ via FFI bindings** generated by `cbindgen` (Rust→C++ headers) and manual `extern "C"` blocks (C++→Rust)
4. **Data passes through C-compatible types** — `#[repr(C)]` structs, raw pointers, C strings

### 11.2 Example: Porting `util/strftime`

**Before (C++):**
```cpp
// src/util/strftime.h
std::string strftime(const std::string &format, time_t time);
```

**After (Rust with C API):**
```rust
// crates/luantis-util/src/string.rs
pub fn strftime(format: &str, time: i64) -> String {
    // Rust implementation
}

// crates/luantis-util/src/c_api.rs
#[no_mangle]
pub extern "C" fn luantis_strerror(format: *const c_char, time: i64) -> *mut c_char {
    // C-compatible wrapper
}
```

**C++ calls Rust:**
```cpp
// src/util/strftime.h (modified)
extern "C" char* luantis_strerror(const char* format, int64_t time);

// Delegating implementation
std::string strftime(const std::string &format, time_t time) {
    char* result = luantis_strerror(format.c_str(), time);
    std::string s(result);
    free(result);
    return s;
}
```

### 11.3 Transition Checklist Per Module

For each module being ported:

- [ ] Create Rust crate with matching functionality
- [ ] Add C FFI layer (`c_api.rs`)
- [ ] Generate C headers with `cbindgen`
- [ ] Modify C++ code to call Rust via FFI (keep C++ wrappers for compatibility)
- [ ] Port unit tests and validate against C++ test results
- [ ] Remove C++ implementation (keep header for API compatibility)
- [ ] Once all C++ callers are ported to Rust, remove FFI layer and use idiomatic Rust APIs

---

## 12. Risk Register

| # | Risk | Impact | Likelihood | Mitigation |
|---|------|--------|------------|------------|
| 1 | **Lua API incompatibility** — Lua mods break due to subtle behavior differences | Critical | Medium | Test every binding against `builtin/` scripts. Use `mlua` which mirrors Lua C API. Run the full `builtin/` test suite after each binding port. |
| 2 | **Rendering regression** — New wgpu renderer looks different or performs worse | High | Medium | Build screenshot comparison tests. Profile with `tracy`. Incrementally replace Irrlicht render passes. |
| 3 | **Network protocol incompatibility** — Rust server can't serve C++ clients | High | Low | Maintain wire format compatibility. Test against C++ client binary. Use packet capture regression tests. |
| 4 | **Performance regression** — Rust code is slower than C++ in hot paths | Medium | Low | Benchmark every ported module. Use `criterion` for microbenchmarks. Profile with `perf` and `flamegraph`. |
| 5 | **Scope creep** — Porting takes much longer than estimated | High | High | Strict phase gates. Each phase must be complete and tested before starting the next. No skipping ahead. |
| 6 | **Irrlicht type decoupling breaks things** — Replacing vector types causes cascading changes | Medium | Medium | Port `luantis-math` first. Use type aliases during transition. Incrementally remove Irrlicht includes. |
| 7 | **FormSpec rendering differences** — UI looks different in new renderer | Medium | Medium | Pixel-comparison test every FormSpec widget. Maintain widget-by-widget compatibility. |
| 8 | **Database format incompatibility** — Existing worlds can't be loaded | Critical | Low | Maintain exact SQLite3 schema. Test with real world databases. Migration tool if needed. |
| 9 | **Monolithic files resist decomposition** — server.cpp/game.cpp are hard to split | Medium | High | Decompose incrementally. First extract helper functions, then extract classes, then move to Rust modules. |
| 10 | **Loss of debuggability** — Rust stack traces are less familiar | Low | Medium | Use `backtrace` crate. Integrate with `tracy` for profiling. Set up `rust-lldb`/`rust-gdb`. |

---

## 13. Milestone Timeline

### Milestone 1: Foundation (Months 1–4)

| Week | Deliverable |
|------|-------------|
| 1–2 | `luantis-math` crate (vector/color/matrix types, replace Irrlicht types) |
| 3 | `luantis-threading` crate (std::thread, parking_lot) |
| 4–5 | `luantis-serialization` crate (binary, JSON, compression) |
| 6–8 | `luantis-util` crate (string, numeric, PRNG, logging, PNG) |
| 9–12 | Integration testing — all Phase 1 crates pass C++ FFI validation |

**Exit criteria:** All Phase 1 crates compile, pass their C++ unit test equivalents, and are callable from C++ via FFI.

### Milestone 2: Core Engine (Months 4–9)

| Week | Deliverable |
|------|-------------|
| 13–15 | `luantis-database` crate (SQLite3 backend + trait) |
| 16–21 | `luantis-network` crate (MTP protocol, async networking) |
| 22–23 | `luantis-content` crate (mod loading) |
| 24–27 | `luantis-mapgen` crate (MapgenV7 + biome/ore/decoration) |
| 28–32 | `luantis-voxel` crate (MapNode, MapBlock, VoxelManipulator, Map) |
| 33–36 | Integration testing — Phase 2 crates pass validation |

**Exit criteria:** A Rust-based map generator can produce a world and save it to a SQLite3 database that a C++ server can load.

### Milestone 3: Scripting (Months 9–15)

| Week | Deliverable |
|------|-------------|
| 37–40 | `luantis-script` core framework (mlua integration, type conversion) |
| 41–44 | Server-side Lua API bindings (l_env, l_object, l_mapgen, l_inventory) |
| 45–48 | Remaining Lua API bindings (l_craft, l_item, l_metadata, l_util, etc.) |
| 49–52 | Client-side Lua API bindings (l_client, l_camera, l_mainmenu) |
| 53–56 | Full `builtin/` Lua test suite passes on Rust scripting engine |
| 57–60 | Integration testing — mods run correctly with Rust scripting |

**Exit criteria:** All `builtin/` Lua scripts and common mods work correctly with the Rust scripting engine, matching C++ behavior.

### Milestone 4: Server (Months 12–18, overlaps with Phase 3)

| Week | Deliverable |
|------|-------------|
| 45–48 | `luantis-inventory` crate |
| 49–50 | `luantis-settings` crate |
| 51–58 | `luantis-server` crate (decompose server.cpp, port state machine) |
| 59–64 | Server integration testing — Rust server can serve C++ clients |

**Exit criteria:** A pure-Rust server can accept connections from a C++ client, run mods, and maintain a world.

### Milestone 5: Client (Months 15–24)

| Week | Deliverable |
|------|-------------|
| 61–64 | `luantis-render` initial (wgpu setup, basic mesh rendering) |
| 65–68 | `luantis-sound` crate (OpenAL/oddio audio) |
| 69–76 | `luantis-render` advanced (shadows, sky, particles, HUD) |
| 77–84 | `luantis-gui` crate (FormSpec parser + widget rendering) |
| 85–92 | `luantis-client` crate (game loop, input, camera) |
| 93–96 | Full integration testing — Rust client connects to C++ server |

**Exit criteria:** A pure-Rust client can connect to a C++ or Rust server, render the world, and provide a playable game experience.

### Final: Pure Rust (Month 24+)

- Remove all C++ code and FFI layers
- Clean up Rust APIs (remove `#[repr(C)]` where no longer needed)
- Performance optimization pass
- Documentation and release

---

## 14. Appendix: Module Reference

### A. Largest Source Files (Top 30)

These are the monolithic files that will require the most careful decomposition:

| File | Lines | Module | Notes |
|------|-------|--------|-------|
| `guiFormSpecMenu.cpp` | 5,317 | gui | FormSpec widget manager — must decompose by widget |
| `server.cpp` | 4,534 | server | Server state machine — must decompose by subsystem |
| `game.cpp` | 3,820 | client | Client game loop — must decompose by system |
| `l_object.cpp` | 3,001 | script | Lua object API — largest Lua binding |
| `client.cpp` | 2,440 | client | Network client — moderate complexity |
| `l_mapgen.cpp` | 2,139 | script | Lua mapgen API — depends on mapgen crate |
| `l_mainmenu.cpp` | 1,198 | script | Main menu Lua API — depends on GUI |
| `guiEngine.cpp` | 1,131 | gui | GUI engine — depends on Irrlicht |
| `l_env.cpp` | 1,621 | script | Lua environment API — depends on server |
| `mapgen_v7.cpp` | 1,366 | mapgen | Default mapgen — first to port |
| `serverpackethandler.cpp` | 1,344 | server | Packet handlers — depends on network |
| `clientpackethandler.cpp` | 1,272 | client | Client packet handlers |
| `mapgen_carpathian.cpp` | 1,149 | mapgen | Carpathian mapgen |
| `content_cao.cpp` | 1,112 | client | Client active objects |
| `CIrrDeviceSDL.cpp` | 1,091 | irr | SDL device — will be replaced |
| `nodedef.cpp` | 1,065 | core | Node definitions — important data structure |
| `guiEditBoxWithScrollbar.cpp` | 1,044 | gui | Text edit widget |
| `CGUITTFont.cpp` | 1,030 | irr | Font rendering — will be replaced |
| `COpenGLDriver.cpp` | 1,019 | irr | OpenGL driver — will be replaced |
| `CSceneManager.cpp` | 978 | irr | Scene manager — will be replaced |
| `guiHyperText.cpp` | 964 | gui | HyperText widget |
| `clientmap.cpp` | 949 | client | Client-side map rendering |
| `CGUIEnvironment.cpp` | 936 | irr | GUI environment — will be replaced |
| `map.cpp` | 917 | core | Map implementation |
| `CGLTFMeshFileLoader.cpp` | 890 | irr | GLTF loader — will be replaced |
| `l_vmanip.cpp` | 800 | script | VoxelManip Lua API |
| `mapgen_fractal.cpp` | 787 | mapgen | Fractal mapgen |
| `serverenvironment.cpp` | 761 | server | Server environment |
| `player_sao.cpp` | 735 | server | Player server active object |
| `l_inventory.cpp` | 700 | script | Inventory Lua API |

### B. Third-Party Rust Crate Map

| C++ Library | Rust Crate | Maturity | Notes |
|-------------|-----------|----------|-------|
| IrrlichtMt | `wgpu` + custom | ★★★★★ | Industry standard GPU API |
| SDL2 | `winit` | ★★★★★ | Industry standard windowing |
| Lua 5.1/LuaJIT | `mlua` | ★★★★☆ | Excellent Lua bindings, supports Lua 5.1 |
| SQLite3 | `rusqlite` | ★★★★★ | Standard Rust SQLite binding |
| zlib | `flate2` | ★★★★★ | Standard Rust zlib binding |
| zstd | `zstd` | ★★★★★ | Official Rust binding |
| GMP | `rug` | ★★★★☆ | GMP bindings |
| JsonCpp | `serde_json` | ★★★★★ | Standard Rust JSON |
| OpenAL | `alto` / `oddio` | ★★★☆☆ | Alto is OpenAL wrapper, oddio is pure Rust |
| libcurl | `reqwest` | ★★★★★ | Standard Rust HTTP client |
| OpenSSL | `ring` / `rustls` | ★★★★★ | Pure Rust TLS/crypto |
| ncurses | `crossterm` | ★★★★★ | Modern terminal library |
| FreeType | `freetype-rs` | ★★★☆☆ | Freetype bindings |
| libpng | `png` | ★★★★★ | Pure Rust PNG |
| libjpeg | `jpeg-decoder` | ★★★★☆ | Pure Rust JPEG |
| libvorbis/ogg | `lewton` | ★★★★☆ | Pure Rust Ogg Vorbis |
| PostgreSQL | `tokio-postgres` | ★★★★★ | Async PostgreSQL |
| LevelDB | `leveldb` | ★★★☆☆ | LevelDB bindings |
| Redis | `redis` | ★★★★★ | Standard Redis client |
| Gettext | `gettext-rs` | ★★★☆☆ | Gettext bindings |
| Tracy | `tracy-client` | ★★★☆☆ | Tracy profiler bindings |
| Prometheus | `prometheus` | ★★★★☆ | Prometheus metrics |

### C. Quick Reference: Rust Tooling

| Purpose | Tool |
|---------|------|
| Build system | `cargo` (replaces CMake) |
| Linting | `clippy` |
| Formatting | `rustfmt` |
| Testing | `cargo test` + `criterion` (benchmarks) |
| FFI generation | `cbindgen` (Rust → C headers) |
| Documentation | `cargo doc` |
| Profiling | `tracy-client`, `flamegraph`, `perf` |
| Cross-compilation | `cargo build --target` |
| Dependency audit | `cargo audit` |

---

*This plan is a living document. As each phase is completed, update the status and adjust estimates based on actual experience.*
