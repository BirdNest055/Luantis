// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luantis Contributors

/*
 * Benchmark: Client-side performance optimizations
 *
 * This benchmark measures the performance difference for the client-side
 * FPS optimizations in v9.62:
 *
 * 1. MeshCollector::findBuffer() linear scan vs hash lookup
 * 2. Draw list rebuild: std::map vs unordered_map + sort-on-demand
 * 3. Distance check: getDistanceFrom (sqrt) vs getDistanceFromSQ (no sqrt)
 * 4. Animation info: std::map vs unordered_map per-frame lookup
 *
 * Run with: ./luantis --run-benchmarks [benchmark_client_perf]
 */

#include "catch.h"
#include "irr_v3d.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>

// ============================================================================
// Helper: Generate TileLayer-like keys (simulating mesh material lookups)
// ============================================================================

// Simplified version of TileLayer for benchmarking the hash lookup pattern.
// The real TileLayer uses texture_id, shader_id, and material_flags for hashing.
struct SimTileLayer {
	u32 texture_id;
	u32 shader_id;
	u32 material_flags;

	bool operator==(const SimTileLayer &o) const {
		return texture_id == o.texture_id &&
			shader_id == o.shader_id &&
			material_flags == o.material_flags;
	}
};

struct SimTileLayerHash {
	size_t operator()(const SimTileLayer &l) const noexcept {
		size_t ret = 0;
		for (auto h : { l.texture_id, l.shader_id, l.material_flags }) {
			ret += h;
			ret ^= (ret << 6) + (ret >> 2);
		}
		return ret;
	}
};

static std::vector<SimTileLayer> generate_tile_layers(size_t unique_count, size_t total, unsigned seed = 42)
{
	std::mt19937 rng(seed);
	std::uniform_int_distribution<u32> tex_dist(1, 200);
	std::uniform_int_distribution<u32> shader_dist(1, 50);
	std::uniform_int_distribution<u32> flag_dist(0, 255);

	// Create unique layers
	std::vector<SimTileLayer> unique_layers;
	unique_layers.reserve(unique_count);
	for (size_t i = 0; i < unique_count; i++) {
		unique_layers.push_back({tex_dist(rng), shader_dist(rng), flag_dist(rng)});
	}

	// Now randomly select from unique layers to create the access pattern
	std::uniform_int_distribution<size_t> idx_dist(0, unique_count - 1);
	std::vector<SimTileLayer> result;
	result.reserve(total);
	for (size_t i = 0; i < total; i++) {
		result.push_back(unique_layers[idx_dist(rng)]);
	}
	return result;
}

// ============================================================================
// BENCHMARK 1: MeshCollector::findBuffer() — Linear scan vs Hash lookup
// ============================================================================

TEST_CASE("benchmark_mesh_collector_find_buffer")
{
	// Simulate a typical MapBlock with 30 unique materials and 24000 face lookups
	const size_t unique_materials = 30;
	const size_t total_lookups = 24000;
	auto layers = generate_tile_layers(unique_materials, total_lookups);

	BENCHMARK_ADVANCED("linear_scan_30_materials_24k_lookups")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			// Simulate the old findBuffer: linear scan through vector
			std::vector<SimTileLayer> buffers;
			int found = 0;
			for (auto &layer : layers) {
				bool match = false;
				for (auto &b : buffers) {
					if (b == layer) {
						match = true;
						break;
					}
				}
				if (!match)
					buffers.push_back(layer);
				found++;
			}
			return found;
		});
	};

	BENCHMARK_ADVANCED("hash_lookup_30_materials_24k_lookups")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			// Simulate the new findBuffer: hash index lookup
			std::vector<SimTileLayer> buffers;
			std::unordered_map<SimTileLayer, size_t, SimTileLayerHash> index;
			index.reserve(unique_materials);
			int found = 0;
			for (auto &layer : layers) {
				auto it = index.find(layer);
				if (it == index.end()) {
					buffers.push_back(layer);
					index[layer] = buffers.size() - 1;
				}
				found++;
			}
			return found;
		});
	};

	// Also test with more materials (heavy mod scene)
	const size_t unique_materials_heavy = 80;
	auto layers_heavy = generate_tile_layers(unique_materials_heavy, total_lookups);

	BENCHMARK_ADVANCED("linear_scan_80_materials_24k_lookups")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::vector<SimTileLayer> buffers;
			int found = 0;
			for (auto &layer : layers_heavy) {
				bool match = false;
				for (auto &b : buffers) {
					if (b == layer) {
						match = true;
						break;
					}
				}
				if (!match)
					buffers.push_back(layer);
				found++;
			}
			return found;
		});
	};

	BENCHMARK_ADVANCED("hash_lookup_80_materials_24k_lookups")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::vector<SimTileLayer> buffers;
			std::unordered_map<SimTileLayer, size_t, SimTileLayerHash> index;
			index.reserve(unique_materials_heavy);
			int found = 0;
			for (auto &layer : layers_heavy) {
				auto it = index.find(layer);
				if (it == index.end()) {
					buffers.push_back(layer);
					index[layer] = buffers.size() - 1;
				}
				found++;
			}
			return found;
		});
	};
}

// ============================================================================
// BENCHMARK 2: Draw list rebuild — std::map vs unordered_map + sort
// ============================================================================

TEST_CASE("benchmark_drawlist_rebuild_with_sort")
{
	// Generate block positions as before
	std::mt19937 rng(42);
	std::uniform_int_distribution<s16> dist(-30, 30);
	std::vector<v3s16> positions;
	positions.reserve(1000);
	for (int i = 0; i < 1000; i++)
		positions.emplace_back(dist(rng), dist(rng), dist(rng));

	v3s16 camera_block(0, 0, 0);

	BENCHMARK_ADVANCED("std_map_insert_ordered_1000")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			// Old approach: std::map with comparator auto-sorts
			std::map<v3s16, int> m;
			for (auto &p : positions)
				m[p] = 1;
			// Iteration is already sorted
			int sum = 0;
			for (auto &pair : m)
				sum += pair.second;
			m.clear();
			return sum;
		});
	};

	BENCHMARK_ADVANCED("unordered_map_sort_on_demand_1000")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			// New approach: unordered_map + sort only when needed
			std::unordered_map<v3s16, int> m;
			m.reserve(1000);
			for (auto &p : positions)
				m[p] = 1;
			// Sort on demand for transparent pass
			std::vector<std::pair<v3s16, int>> sorted(m.begin(), m.end());
			std::sort(sorted.begin(), sorted.end(),
				[&camera_block](const auto &a, const auto &b) {
					auto da = a.first.getDistanceFromSQ(camera_block);
					auto db = b.first.getDistanceFromSQ(camera_block);
					return da > db || (da == db && a.first > b.first);
				});
			int sum = 0;
			for (auto &pair : sorted)
				sum += pair.second;
			m.clear();
			return sum;
		});
	};

	// For the solid pass, no sort needed
	BENCHMARK_ADVANCED("unordered_map_no_sort_1000")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::unordered_map<v3s16, int> m;
			m.reserve(1000);
			for (auto &p : positions)
				m[p] = 1;
			// No sort needed for solid pass
			int sum = 0;
			for (auto &pair : m)
				sum += pair.second;
			m.clear();
			return sum;
		});
	};
}

// ============================================================================
// BENCHMARK 3: Distance check — sqrt vs no sqrt
// ============================================================================

TEST_CASE("benchmark_distance_check")
{
	// Simulate checking 2000 blocks for distance
	std::mt19937 rng(42);
	std::uniform_int_distribution<s16> dist(-100, 100);
	std::vector<v3f> positions;
	positions.reserve(2000);
	for (int i = 0; i < 2000; i++)
		positions.emplace_back(dist(rng) * 10.0f, dist(rng) * 10.0f, dist(rng) * 10.0f);

	v3f camera_pos(0, 0, 0);
	f32 range = 1000.0f;
	f32 radius = 50.0f;

	BENCHMARK_ADVANCED("getDistanceFrom_sqrt_2000")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			int in_range = 0;
			for (auto &p : positions) {
				if (p.getDistanceFrom(camera_pos) <= range + radius)
					in_range++;
			}
			return in_range;
		});
	};

	BENCHMARK_ADVANCED("getDistanceFromSQ_no_sqrt_2000")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			int in_range = 0;
			f32 range_sq = (range + radius) * (range + radius);
			for (auto &p : positions) {
				if (p.getDistanceFromSQ(camera_pos) <= range_sq)
					in_range++;
			}
			return in_range;
		});
	};
}

// ============================================================================
// BENCHMARK 4: Animation info — std::map vs unordered_map per-frame
// ============================================================================

TEST_CASE("benchmark_animation_info_lookup")
{
	// Simulate MapBlockMesh::animate() which iterates all animation entries
	// Typically 10-30 animated textures per block (water, leaves, etc.)
	using MeshIndex = std::pair<u8, u32>;
	struct MeshIndexHash {
		size_t operator()(const MeshIndex &k) const noexcept {
			return std::hash<u32>()(k.second) ^ (std::hash<unsigned>()(k.first) << 8);
		}
	};

	const int num_entries = 25;
	std::vector<MeshIndex> keys;
	for (int i = 0; i < num_entries; i++)
		keys.emplace_back(static_cast<u8>(i % 2), static_cast<u32>(i));

	// Simulate per-frame iteration (60 FPS = 60 iterations)
	const int frames = 600; // 10 seconds worth

	BENCHMARK_ADVANCED("std_map_animate_25_entries_600_frames")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::map<MeshIndex, int> anim_info;
			for (auto &k : keys)
				anim_info[k] = 0;
			for (int f = 0; f < frames; f++) {
				for (auto &it : anim_info)
					it.second++; // simulate updateTexture
			}
			return anim_info.size();
		});
	};

	BENCHMARK_ADVANCED("unordered_map_animate_25_entries_600_frames")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::unordered_map<MeshIndex, int, MeshIndexHash> anim_info;
			anim_info.reserve(num_entries);
			for (auto &k : keys)
				anim_info[k] = 0;
			for (int f = 0; f < frames; f++) {
				for (auto &it : anim_info)
					it.second++; // simulate updateTexture
			}
			return anim_info.size();
		});
	};

	// Test with more animated textures (heavily modded scene)
	const int num_entries_heavy = 80;
	std::vector<MeshIndex> keys_heavy;
	for (int i = 0; i < num_entries_heavy; i++)
		keys_heavy.emplace_back(static_cast<u8>(i % 2), static_cast<u32>(i));

	BENCHMARK_ADVANCED("std_map_animate_80_entries_600_frames")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::map<MeshIndex, int> anim_info;
			for (auto &k : keys_heavy)
				anim_info[k] = 0;
			for (int f = 0; f < frames; f++) {
				for (auto &it : anim_info)
					it.second++;
			}
			return anim_info.size();
		});
	};

	BENCHMARK_ADVANCED("unordered_map_animate_80_entries_600_frames")(Catch::Benchmark::Chronometer meter) {
		meter.measure([&] {
			std::unordered_map<MeshIndex, int, MeshIndexHash> anim_info;
			anim_info.reserve(num_entries_heavy);
			for (auto &k : keys_heavy)
				anim_info[k] = 0;
			for (int f = 0; f < frames; f++) {
				for (auto &it : anim_info)
					it.second++;
			}
			return anim_info.size();
		});
	};
}
