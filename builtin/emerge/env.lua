-- Reimplementations of some environment function on vmanips, since this is
-- what the emerge environment operates on

-- core.vmanip = <VoxelManip> -- set by C++

function core.set_node(pos, node)
        return core.vmanip:set_node_at(pos, node)
end

function core.bulk_set_node(pos_list, node)
        local vm = core.vmanip
        local set_node_at = vm.set_node_at
        for _, pos in ipairs(pos_list) do
                if not set_node_at(vm, pos, node) then
                        return false
                end
        end
        return true
end

core.add_node = core.set_node

-- NOTE: swap_node is aliased to set_node in the emerge environment because
-- VoxelManip does not support metadata operations. In the main game environment,
-- swap_node preserves metadata while set_node replaces it, but during map
-- generation (emerge), there is no NodeMetadataDatabase to consult, so the
-- distinction is moot. If metadata-aware emerge operations become necessary,
-- a VoxelManip metadata API would need to be added first (see issue history).
core.swap_node = core.set_node

core.bulk_swap_node = core.bulk_set_node

function core.remove_node(pos)
        return core.vmanip:set_node_at(pos, {name="air"})
end

function core.get_node(pos)
        return core.vmanip:get_node_at(pos)
end

function core.get_value_noise(seed, octaves, persist, spread)
        local params
        if type(seed) == "table" then
                params = table.copy(seed)
        else
                assert(type(seed) == "number")
                params = {
                        seed = seed,
                        octaves = octaves,
                        persist = persist,
                        spread = {x=spread, y=spread, z=spread},
                }
        end
        params.seed = core.get_seed(params.seed) -- add mapgen seed
        return ValueNoise(params)
end

function core.get_value_noise_map(params, size)
        local params2 = table.copy(params)
        params2.seed = core.get_seed(params.seed) -- add mapgen seed
        return ValueNoiseMap(params2, size)
end

-- deprecated as of 5.12, as it was not Perlin noise
-- NOTE: These aliases exist for backward compatibility. PerlinNoise/PerlinNoiseMap
-- were historically mislabeled — they produce value noise, not Perlin noise. No
-- deprecation warning is logged yet to avoid breaking existing mods silently.
-- Migration plan:
--   1. Add a deprecation warning to core.get_perlin / core.get_perlin_map in the
--      next major version, pointing users to core.get_value_noise / core.get_value_noise_map.
--   2. Eventually remove the global PerlinNoise / PerlinNoiseMap globals.
--   3. The correct Perlin noise implementation could be added as core.get_perlin_noise
--      with proper gradient noise parameters if needed.
core.get_perlin = core.get_value_noise
core.get_perlin_map = core.get_value_noise_map
PerlinNoise = ValueNoise
PerlinNoiseMap = ValueNoiseMap
