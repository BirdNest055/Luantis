// Luantis
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luantis Contributors

/*
 * Benchmark: Container performance comparison
 *
 * This benchmark measures the performance difference between std::map and
 * std::unordered_map for the key usage patterns found in the Luantis engine.
 * It directly tests the hot-path scenarios that the v9.62 optimization
 * targets:
 *
 * 1. v3s16-keyed maps (modified_blocks, emerge queue, draw list)
 * 2. u16-keyed sets (m_known_objects)
 * 3. string-keyed maps (profiler, item definitions)
 * 4. MutexedMap throughput
 *
 * Run with: ./luantis --run-benchmarks [benchmark_containers]
 */

#include "catch.h"
#include "irr_v3d.h"
#include "util/container.h"

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <random>
#include <vector>
#include <mutex>

// ============================================================================
// Helper: Generate random v3s16 positions (simulating block coordinates)
// ============================================================================
static std::vector<v3s16> generate_v3s16_positions(size_t count, unsigned seed = 42)
{
        std::mt19937 rng(seed);
        std::uniform_int_distribution<s16> dist(-50, 50);
        std::vector<v3s16> result;
        result.reserve(count);
        for (size_t i = 0; i < count; i++)
                result.emplace_back(dist(rng), dist(rng), dist(rng));
        return result;
}

// Helper: Generate random u16 IDs (simulating object IDs)
static std::vector<u16> generate_u16_ids(size_t count, unsigned seed = 42)
{
        std::mt19937 rng(seed);
        std::uniform_int_distribution<u16> dist(1, 60000);
        std::vector<u16> result;
        result.reserve(count);
        for (size_t i = 0; i < count; i++)
                result.push_back(dist(rng));
        return result;
}

// Helper: Generate random string keys (simulating profiler/setting names)
static std::vector<std::string> generate_string_keys(size_t count, unsigned seed = 42)
{
        std::mt19937 rng(seed);
        std::vector<std::string> result;
        result.reserve(count);
        for (size_t i = 0; i < count; i++) {
                // Simulate typical profiler key patterns
                result.push_back("key_" + std::to_string(i % 1000) + "_sub_" + std::to_string(i / 1000));
        }
        return result;
}

// ============================================================================
// BENCHMARK 1: v3s16-keyed maps (modified_blocks, emerge queue pattern)
// ============================================================================

TEST_CASE("benchmark_v3s16_map_insert_lookup")
{
        auto positions = generate_v3s16_positions(5000);

        BENCHMARK_ADVANCED("std_map_v3s16_insert_5000")(Catch::Benchmark::Chronometer meter) {
                std::map<v3s16, int> m;
                meter.measure([&] {
                        for (auto &p : positions)
                                m[p] = 42;
                        return m.size();
                });
        };

        BENCHMARK_ADVANCED("unordered_map_v3s16_insert_5000")(Catch::Benchmark::Chronometer meter) {
                std::unordered_map<v3s16, int> m;
                m.reserve(5000);
                meter.measure([&] {
                        for (auto &p : positions)
                                m[p] = 42;
                        return m.size();
                });
        };

        BENCHMARK_ADVANCED("std_map_v3s16_lookup_5000")(Catch::Benchmark::Chronometer meter) {
                std::map<v3s16, int> m;
                for (auto &p : positions)
                        m[p] = 42;
                meter.measure([&] {
                        int sum = 0;
                        for (auto &p : positions)
                                sum += m.count(p);
                        return sum;
                });
        };

        BENCHMARK_ADVANCED("unordered_map_v3s16_lookup_5000")(Catch::Benchmark::Chronometer meter) {
                std::unordered_map<v3s16, int> m;
                m.reserve(5000);
                for (auto &p : positions)
                        m[p] = 42;
                meter.measure([&] {
                        int sum = 0;
                        for (auto &p : positions)
                                sum += m.count(p);
                        return sum;
                });
        };

        BENCHMARK_ADVANCED("std_map_v3s16_clear_5000")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        std::map<v3s16, int> m;
                        for (auto &p : positions)
                                m[p] = 42;
                        m.clear();
                        return m.size();
                });
        };

        BENCHMARK_ADVANCED("unordered_map_v3s16_clear_5000")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        std::unordered_map<v3s16, int> m;
                        m.reserve(5000);
                        for (auto &p : positions)
                                m[p] = 42;
                        m.clear();
                        return m.size();
                });
        };
}

// ============================================================================
// BENCHMARK 2: v3s16-keyed sets (ActiveBlockList pattern)
// ============================================================================

TEST_CASE("benchmark_v3s16_set_operations")
{
        auto positions = generate_v3s16_positions(2000);

        BENCHMARK_ADVANCED("std_set_v3s16_insert_contains_2000")(Catch::Benchmark::Chronometer meter) {
                std::set<v3s16> s;
                meter.measure([&] {
                        for (auto &p : positions)
                                s.insert(p);
                        int found = 0;
                        for (auto &p : positions)
                                found += s.count(p);
                        return found;
                });
        };

        BENCHMARK_ADVANCED("unordered_set_v3s16_insert_contains_2000")(Catch::Benchmark::Chronometer meter) {
                std::unordered_set<v3s16> s;
                s.reserve(2000);
                meter.measure([&] {
                        for (auto &p : positions)
                                s.insert(p);
                        int found = 0;
                        for (auto &p : positions)
                                found += s.count(p);
                        return found;
                });
        };

        BENCHMARK_ADVANCED("std_set_v3s16_erase_2000")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        std::set<v3s16> s;
                        for (auto &p : positions)
                                s.insert(p);
                        for (auto &p : positions)
                                s.erase(p);
                        return s.size();
                });
        };

        BENCHMARK_ADVANCED("unordered_set_v3s16_erase_2000")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        std::unordered_set<v3s16> s;
                        s.reserve(2000);
                        for (auto &p : positions)
                                s.insert(p);
                        for (auto &p : positions)
                                s.erase(p);
                        return s.size();
                });
        };
}

// ============================================================================
// BENCHMARK 3: u16-keyed sets (m_known_objects pattern)
// ============================================================================

TEST_CASE("benchmark_u16_set_operations")
{
        auto ids = generate_u16_ids(500);

        BENCHMARK_ADVANCED("std_set_u16_insert_lookup_500")(Catch::Benchmark::Chronometer meter) {
                std::set<u16> s;
                meter.measure([&] {
                        for (auto id : ids)
                                s.insert(id);
                        int found = 0;
                        for (auto id : ids)
                                found += s.count(id);
                        return found;
                });
        };

        BENCHMARK_ADVANCED("unordered_set_u16_insert_lookup_500")(Catch::Benchmark::Chronometer meter) {
                std::unordered_set<u16> s;
                s.reserve(500);
                meter.measure([&] {
                        for (auto id : ids)
                                s.insert(id);
                        int found = 0;
                        for (auto id : ids)
                                found += s.count(id);
                        return found;
                });
        };
}

// ============================================================================
// BENCHMARK 4: String-keyed maps (profiler/ItemDef pattern)
// ============================================================================

TEST_CASE("benchmark_string_map_operations")
{
        auto keys = generate_string_keys(500);

        BENCHMARK_ADVANCED("std_map_string_insert_lookup_500")(Catch::Benchmark::Chronometer meter) {
                std::map<std::string, float> m;
                meter.measure([&] {
                        for (size_t i = 0; i < keys.size(); i++)
                                m[keys[i]] = static_cast<float>(i);
                        float sum = 0;
                        for (auto &k : keys)
                                sum += m[k];
                        return sum;
                });
        };

        BENCHMARK_ADVANCED("unordered_map_string_insert_lookup_500")(Catch::Benchmark::Chronometer meter) {
                std::unordered_map<std::string, float> m;
                m.reserve(500);
                meter.measure([&] {
                        for (size_t i = 0; i < keys.size(); i++)
                                m[keys[i]] = static_cast<float>(i);
                        float sum = 0;
                        for (auto &k : keys)
                                sum += m[k];
                        return sum;
                });
        };
}

// ============================================================================
// BENCHMARK 5: MutexedMap throughput
// ============================================================================

TEST_CASE("benchmark_mutexed_map")
{
        auto keys = generate_string_keys(200);

        BENCHMARK_ADVANCED("MutexedMap_set_get_200_keys")(Catch::Benchmark::Chronometer meter) {
                MutexedMap<std::string, int> m;
                meter.measure([&] {
                        for (size_t i = 0; i < keys.size(); i++)
                                m.set(keys[i], static_cast<int>(i));
                        int sum = 0;
                        int val;
                        for (auto &k : keys) {
                                m.get(k, &val);
                                sum += val;
                        }
                        return sum;
                });
        };
}

// ============================================================================
// BENCHMARK 6: Draw list rebuild pattern (ClientMap::updateDrawList)
// ============================================================================

TEST_CASE("benchmark_drawlist_rebuild")
{
        auto positions = generate_v3s16_positions(500);

        BENCHMARK_ADVANCED("std_map_rebuild_500_blocks")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        std::map<v3s16, int> m;
                        for (auto &p : positions)
                                m[p] = 1;
                        // Simulate iteration (rendering)
                        int sum = 0;
                        for (auto &pair : m)
                                sum += pair.second;
                        m.clear();
                        return sum;
                });
        };

        BENCHMARK_ADVANCED("unordered_map_rebuild_500_blocks")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        std::unordered_map<v3s16, int> m;
                        m.reserve(500);
                        for (auto &p : positions)
                                m[p] = 1;
                        // Simulate sorted iteration (rendering order)
                        std::vector<std::pair<v3s16, int>> sorted(m.begin(), m.end());
                        std::sort(sorted.begin(), sorted.end(),
                                [](const auto &a, const auto &b) {
                                        return a.first.X < b.first.X ||
                                                (a.first.X == b.first.X &&
                                                        (a.first.Y < b.first.Y ||
                                                                (a.first.Y == b.first.Y && a.first.Z < b.first.Z)));
                                });
                        int sum = 0;
                        for (auto &pair : sorted)
                                sum += pair.second;
                        m.clear();
                        return sum;
                });
        };
}

// ============================================================================
// BENCHMARK 7: Player lookup pattern (ServerEnvironment::getPlayer)
// ============================================================================

TEST_CASE("benchmark_player_lookup_linear")
{
        const int player_count = 50;
        std::vector<std::pair<u16, std::string>> player_data;
        for (int i = 0; i < player_count; i++)
                player_data.emplace_back(static_cast<u16>(i + 1), "player" + std::to_string(i));

        std::vector<u16> lookup_ids;
        for (int i = 0; i < 1000; i++)
                lookup_ids.push_back(static_cast<u16>((i % player_count) + 1));

        BENCHMARK_ADVANCED("linear_scan_50_players_1000_lookups")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        int found = 0;
                        for (u16 id : lookup_ids) {
                                for (auto &pd : player_data) {
                                        if (pd.first == id) {
                                                found++;
                                                break;
                                        }
                                }
                        }
                        return found;
                });
        };
}

TEST_CASE("benchmark_player_lookup_hash")
{
        const int player_count = 50;
        std::vector<std::pair<u16, std::string>> player_data;
        for (int i = 0; i < player_count; i++)
                player_data.emplace_back(static_cast<u16>(i + 1), "player" + std::to_string(i));

        std::unordered_map<u16, size_t> player_hashmap;
        for (size_t i = 0; i < player_data.size(); i++)
                player_hashmap[player_data[i].first] = i;

        std::vector<u16> lookup_ids;
        for (int i = 0; i < 1000; i++)
                lookup_ids.push_back(static_cast<u16>((i % player_count) + 1));

        BENCHMARK_ADVANCED("hash_map_50_players_1000_lookups")(Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                        int found = 0;
                        for (u16 id : lookup_ids)
                                found += static_cast<int>(player_hashmap.count(id));
                        return found;
                });
        };
}

// ============================================================================
// BENCHMARK 8: Profiler pattern — add/avg with mutex
// ============================================================================

TEST_CASE("benchmark_profiler_pattern")
{
        auto keys = generate_string_keys(100);

        BENCHMARK_ADVANCED("std_map_mutex_add_100_keys_1000_times")(Catch::Benchmark::Chronometer meter) {
                std::map<std::string, float> data;
                std::mutex mtx;
                meter.measure([&] {
                        for (int r = 0; r < 10; r++) {
                                for (auto &k : keys) {
                                        std::lock_guard<std::mutex> lock(mtx);
                                        auto it = data.find(k);
                                        if (it == data.end())
                                                data.emplace(k, 1.0f);
                                        else
                                                it->second += 1.0f;
                                }
                        }
                        return data.size();
                });
        };

        BENCHMARK_ADVANCED("unordered_map_mutex_add_100_keys_1000_times")(Catch::Benchmark::Chronometer meter) {
                std::unordered_map<std::string, float> data;
                data.reserve(100);
                std::mutex mtx;
                meter.measure([&] {
                        for (int r = 0; r < 10; r++) {
                                for (auto &k : keys) {
                                        std::lock_guard<std::mutex> lock(mtx);
                                        auto it = data.find(k);
                                        if (it == data.end())
                                                data.emplace(k, 1.0f);
                                        else
                                                it->second += 1.0f;
                                }
                        }
                        return data.size();
                });
        };
}
