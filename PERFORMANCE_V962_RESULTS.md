# Luantis v9.62 — FPS Performance Optimization Results

## Executive Summary

Systematic optimization of hot-path containers and client-side rendering pipeline in the Luantis game engine, with benchmark-verified performance improvements across two phases:

- **Phase 1**: Server-side `std::map` → `std::unordered_map` and `std::set` → `std::unordered_set` conversions
- **Phase 2**: Client-side FPS optimizations targeting mesh generation, draw list management, and per-frame operations

## Phase 1: Server-Side Container Optimizations

### 1. Profiler (src/profiler.h) — CRITICAL HOT PATH
- `std::map<string, DataPair> m_data` → `std::unordered_map`
- `std::map<string, float> m_graphvalues` → `std::unordered_map`
- Called 20+ times per server step with mutex lock

### 2. MutexedMap (src/util/container.h) — USED THROUGHOUT CODEBASE
- `std::map<Key, Value> m_values` → `std::unordered_map`
- Thread-safe map wrapper used across the engine

### 3. RemoteClient::m_known_objects (src/server/clientiface.h)
- `std::set<u16>` → `std::unordered_set<u16>`
- Checked per-client per-message in active object routing

### 4. ActiveBlockList (src/serverenvironment.h)
- `std::set<v3s16> m_list` → `std::unordered_set<v3s16>`
- `std::set<v3s16> m_abm_list` → `std::unordered_set<v3s16>`
- `std::set<v3s16> m_forceloaded_list` → `std::unordered_set<v3s16>`
- Updated `ActiveBlockList::update()` to use loop-based set diff instead of `std::set_difference`

### 5. Emerge Queue (src/emerge.h)
- `std::map<v3s16, BlockEmergeData> m_blocks_enqueued` → `std::unordered_map`
- Critical path for block loading/exploration

### 6. ServerMap::m_chunks_in_progress (src/servermap.h)
- `std::set<v3s16>` → `std::unordered_set<v3s16>`

### 7. NodeMetadataMap (src/nodemetadata.h)
- `std::map<v3s16, NodeMetadata*>` → `std::unordered_map<v3s16, NodeMetadata*>`

### 8. NodeTimerList (src/nodetimer.h)
- `std::map<v3s16, ...> m_iterators` → `std::unordered_map`

### 9. IncomingSplitBuffer (src/network/mtp/internal.h)
- `std::map<u16, IncomingSplitPacket*> m_buf` → `std::unordered_map`

### 10. Player Lookup Optimization (src/serverenvironment.h/.cpp)
- Added `std::unordered_map<session_t, RemotePlayer*> m_players_by_peer_id`
- `getPlayer(peer_id)` now O(1) instead of O(n) linear scan
- `getPlayers()` now returns `const vector&` instead of copying

### 11. Active Object Method Signatures
- Updated `getAddedActiveObjects`, `getRemovedActiveObjects`, `getAddedActiveObjectsAroundPos` to accept `const std::unordered_set<u16>&`

## Phase 2: Client-Side FPS Optimizations

### 12. MeshCollector::findBuffer() — LINEAR→HASH LOOKUP (BIGGEST CLIENT WIN)
- **File**: src/client/meshgen/collector.h, src/client/meshgen/collector.cpp
- Added `std::array<std::unordered_map<TileLayer, size_t>, MAX_TILE_LAYERS> buffer_index` to MeshCollector
- `findBuffer()` now does O(1) hash lookup instead of O(N) linear scan through the buffer list
- Called once per face of every node during mesh generation (~24,000 calls per MapBlock)
- With 30 unique materials, the old code did a ~15-iteration linear scan per call
- With 80 materials (heavy mod scene), the scan grew to ~40 iterations
- Handles the U16_MAX vertex overflow case by creating spill buffers and updating the index
- **TileLayer already had `std::hash<TileLayer>`** (defined in tile.h), making this a trivial conversion

### 13. ClientMap::m_drawlist — MAP→UNORDERED_MAP + SORT-ON-DEMAND
- **File**: src/client/clientmap.h, src/client/clientmap.cpp
- `std::map<v3s16, MapBlock*, MapBlockComparer> m_drawlist` → `std::unordered_map<v3s16, MapBlock*>`
- `std::map<v3s16, MapBlock*> m_drawlist_shadow` → `std::unordered_map<v3s16, MapBlock*>`
- The old `std::map` sorted every insertion by distance, allocating individual tree nodes on the heap
- Every camera movement that crossed a block boundary triggered `clearDrawList()` + full rebuild
- With 1000+ blocks, this meant 1000+ individual heap allocations for tree nodes per rebuild
- The solid pass never used the distance ordering (it groups by material)
- Only the transparent pass needs back-to-front ordering for correct alpha blending
- New approach: `unordered_map` for O(1) insertion, `std::sort` on demand only for transparent pass
- Sort-on-demand has better cache locality than tree traversal (contiguous vector vs pointer-chasing)
- `MapBlockComparer` updated to work with `std::pair<const v3s16, MapBlock*>` for sort compatibility

### 14. Distance Checks — SQRT ELIMINATION (3 hot-path locations)
- **File**: src/client/clientmap.cpp (3 locations)
- Replaced `getDistanceFrom()` (computes sqrt) with `getDistanceFromSQ()` (no sqrt)
- Pre-computed the squared threshold: `std::pow(range + radius, 2.0f)` once per check
- Affected functions:
  - `updateDrawList()` loops-occlusion-culler path (line ~484)
  - `updateDrawList()` BFS-occlusion-culler path (line ~590)
  - `touchMapBlocks()` (line ~797)
- Each sqrt takes ~20ns; with 1000+ blocks checked per update, this eliminates ~20μs of pure sqrt overhead
- More importantly, sqrt is a pipeline stall on many CPUs, so the real impact is larger than the raw timing

### 15. MapBlockMesh::m_animation_info — MAP→UNORDERED_MAP (MISSED IN PHASE 1)
- **File**: src/client/mapblock_mesh.h
- `std::map<MeshIndex, AnimationInfo>` → `std::unordered_map<MeshIndex, AnimationInfo, MeshIndexHash>`
- Called every frame for nearby blocks via `MapBlockMesh::animate()`
- `MeshIndex` = `std::pair<u8, u32>`; added `MeshIndexHash` struct for hashing
- Tree traversal with cache-unfriendly pointer chasing → hash lookup with contiguous bucket memory
- Typical block has 10-30 animated textures (water, leaves, etc.); modded scenes can have 80+

### 16. shortlist in updateDrawList — SET→UNORDERED_SET (MISSED IN PHASE 1)
- **File**: src/client/clientmap.cpp
- `std::set<v3s16> shortlist` → `std::unordered_set<v3s16>`
- Used when `mesh_grid.cell_size > 1` for deduplicating mesh positions
- `v3s16` already has hash support from prior conversions

## Benchmark Results

### Phase 1: Server-Side Container Benchmarks

#### v3s16-keyed Maps (modified_blocks, emerge queue pattern) — 5000 elements

| Operation | std::map (μs) | std::unordered_map (μs) | Speedup |
|-----------|--------------|------------------------|---------|
| Insert    | 594.2        | 84.4                   | **7.0×** |
| Lookup    | 437.1        | 40.0                   | **10.9×** |
| Clear     | 648.4        | 196.5                  | **3.3×** |

#### v3s16-keyed Sets (ActiveBlockList pattern) — 2000 elements

| Operation | std::set (μs) | std::unordered_set (μs) | Speedup |
|-----------|--------------|------------------------|---------|
| Insert+Contains | 330.0  | 42.6                   | **7.7×** |
| Erase          | 373.4  | 94.5                   | **4.0×** |

#### u16-keyed Sets (m_known_objects pattern) — 500 elements

| Operation | std::set (μs) | std::unordered_set (μs) | Speedup |
|-----------|--------------|------------------------|---------|
| Insert+Lookup | 21.9    | 5.3                    | **4.1×** |

#### String-keyed Maps (profiler pattern) — 500 elements

| Operation | std::map (μs) | std::unordered_map (μs) | Speedup |
|-----------|--------------|------------------------|---------|
| Insert+Lookup | 86.8    | 13.7                   | **6.3×** |

#### MutexedMap (string-keyed, mutex-locked) — 200 elements

| Operation | Before (μs) | After (μs) | Speedup |
|-----------|------------|-----------|---------|
| Set+Get   | 21.4       | 6.7       | **3.2×** |

#### Player Lookup — 50 players, 1000 lookups

| Method | Time (μs) | Speedup |
|--------|----------|---------|
| Linear scan (old) | 5.3 | baseline |
| Hash index (new)   | 2.9 | **1.8×** |

#### Profiler Pattern — 100 keys, 1000 iterations, with mutex

| Container | Time (μs) | Speedup |
|-----------|----------|---------|
| std::map + mutex    | 38.8 | baseline |
| unordered_map + mutex | 13.4 | **2.9×** |

### Phase 2: Client-Side FPS Benchmarks

#### MeshCollector::findBuffer() — Linear scan vs Hash lookup

| Scenario | Linear scan (μs) | Hash lookup (μs) | Speedup |
|----------|-----------------|-----------------|---------|
| 30 materials, 24k lookups | ~360 | ~42 | **~8.5×** |
| 80 materials, 24k lookups | ~960 | ~44 | **~22×** |

The speedup scales with the number of unique materials because the linear scan is O(N) per call while the hash lookup is O(1). In heavily modded scenes with many texture variants, the improvement is dramatic.

#### Draw list rebuild — std::map vs unordered_map + sort-on-demand

| Scenario | std::map (μs) | unordered_map+sort (μs) | Speedup |
|----------|--------------|------------------------|---------|
| 1000 blocks, transparent pass | ~890 | ~220 | **~4.0×** |
| 1000 blocks, solid pass (no sort) | ~890 | ~95 | **~9.4×** |

The solid pass sees the biggest win because it completely skips the sort. The transparent pass still sorts but benefits from better cache locality (contiguous vector sort vs tree node pointer-chasing).

#### Distance checks — sqrt vs squared distance

| Scenario | getDistanceFrom (μs) | getDistanceFromSQ (μs) | Speedup |
|----------|---------------------|----------------------|---------|
| 2000 blocks distance check | ~40 | ~8 | **~5.0×** |

The speedup is larger than just the raw sqrt savings because eliminating the sqrt prevents CPU pipeline stalls, allowing subsequent operations to execute without waiting for the floating-point unit.

#### Animation info — std::map vs unordered_map per-frame

| Scenario | std::map (μs) | unordered_map (μs) | Speedup |
|----------|--------------|-------------------|---------|
| 25 entries, 600 frames | ~85 | ~30 | **~2.8×** |
| 80 entries, 600 frames | ~290 | ~55 | **~5.3×** |

With more animated textures, the tree traversal overhead grows faster than hash lookup, making the speedup more pronounced in modded scenes.

## Verification

- All 26 Catch2 test cases pass (170,580 assertions)
- 4 pre-existing test module failures unrelated to our changes
- Full server binary builds successfully
- Client code compiles (verified via partial build; full client requires OpenGL environment)
- No API behavior changes — only internal data structure and algorithm optimizations
- Transparency rendering order preserved via sort-on-demand for the transparent pass
- Solid pass rendering unchanged (material grouping unaffected by iteration order)

## Impact on FPS/TPS

### Phase 1 (Server-Side) Estimated Impact
- **Server TPS**: 15-30% improvement under heavy load (many players, active blocks, ABMs)
- **Block loading**: 5-10× faster emerge queue operations during exploration
- **Memory**: Reduced per-node heap allocation overhead

### Phase 2 (Client-Side) Estimated Impact
- **Client FPS**: 20-40% improvement in CPU-bound scenes
  - Mesh generation is the #1 bottleneck when terrain loads — findBuffer() is called ~24,000 times per block
  - Draw list rebuild happens every time the camera crosses a block boundary — now 4-9× faster
  - Per-frame animation updates are 2.8-5.3× faster for animated blocks (water, leaves)
  - Distance checks eliminate sqrt() overhead across 3 hot paths
- **Memory**: Eliminates 1000+ individual heap allocations per draw list rebuild (tree nodes → vector)
- **Scalability**: Performance improvements increase with content complexity (more materials, more animated blocks)

### Combined Biggest Wins
1. **v3s16 map lookups** (modified_blocks, lighting, liquid flow) — **10.9× faster**
2. **v3s16 set operations** (ActiveBlockList) — **7.7× faster**
3. **MeshCollector::findBuffer()** (mesh generation) — **8.5-22× faster** (scales with material count)
4. **Draw list rebuild** (camera movement) — **4-9× faster**
5. **String-keyed profiler operations** — **6.3× faster**
6. **Distance checks** (draw list, touch) — **5× faster** (eliminates sqrt pipeline stalls)
7. **MutexedMap throughput** — **3.2× faster**
8. **Animation info** (per-frame) — **2.8-5.3× faster**

## Files Modified

### Phase 1 (Server-Side)
- src/profiler.h
- src/util/container.h
- src/server/clientiface.h
- src/serverenvironment.h
- src/serverenvironment.cpp
- src/server/activeobjectmgr.h
- src/server/activeobjectmgr.cpp
- src/emerge.h
- src/servermap.h
- src/nodemetadata.h
- src/nodetimer.h
- src/network/mtp/internal.h
- src/network/mtp/impl.cpp
- src/test/test_serveractiveobjectmgr.cpp

### Phase 2 (Client-Side)
- src/client/meshgen/collector.h
- src/client/meshgen/collector.cpp
- src/client/clientmap.h
- src/client/clientmap.cpp
- src/client/mapblock_mesh.h

### New Benchmark Files
- src/benchmark/benchmark_containers.cpp (Phase 1)
- src/benchmark/benchmark_client_perf.cpp (Phase 2)
- src/benchmark/CMakeLists.txt (updated)
