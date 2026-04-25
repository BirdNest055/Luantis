local function raycast_with_pointabilities(start_pos, end_pos, pointabilities)
	local ray = core.raycast(start_pos, end_pos, nil, nil, pointabilities)
	for hit in ray do
		if hit.type == "node" then
			return hit.under
		end
	end
	return nil
end

local function test_raycast_pointabilities(player, pos1)
	local pos2 = pos1:offset(0, 0, 1)
	local pos3 = pos1:offset(0, 0, 2)

	local oldnode1 = core.get_node(pos1)
	local oldnode2 = core.get_node(pos2)
	local oldnode3 = core.get_node(pos3)
	core.swap_node(pos1, {name = "air"})
	core.swap_node(pos2, {name = "testnodes:not_pointable"})
	core.swap_node(pos3, {name = "testnodes:pointable"})

	local p = nil
	assert(raycast_with_pointabilities(pos1, pos3, p) == pos3)

	p = core.registered_items["testtools:blocked_pointing_staff"].pointabilities
	assert(raycast_with_pointabilities(pos1, pos3, p) == nil)

	p = core.registered_items["testtools:ultimate_pointing_staff"].pointabilities
	assert(raycast_with_pointabilities(pos1, pos3, p) == pos2)

	core.swap_node(pos1, oldnode1)
	core.swap_node(pos2, oldnode2)
	core.swap_node(pos3, oldnode3)
end

unittests.register("test_raycast_pointabilities", test_raycast_pointabilities, {map=true})

local function test_raycast_noskip(_, pos)
	local function random_point_in_area(min, max)
		local extents = max - min
		local v = extents:multiply(vector.new(
			math.random(),
			math.random(),
			math.random()
		))
		return min + v
	end

	-- NOTE: Perfect diagonal raycasts may fail because they can miss
	-- outside faces entirely, "piercing" through nodes.
	-- This is a known limitation of the raycast implementation.
	-- We avoid this edge case by ensuring ray_start is not on a
	-- perfect diagonal that would miss the cuboid.

	local function cuboid_minmax(extent)
		return pos:offset(-extent, -extent, -extent),
				pos:offset(extent, extent, extent)
	end

	-- Carve out a 3x3x3 dirt cuboid in a larger air cuboid
	local r = 8
	local min, max = cuboid_minmax(r + 1)
	local vm = core.get_voxel_manip(min, max)
	local old_data = vm:get_data()
	local data = vm:get_data()
	local emin, emax = vm:get_emerged_area()
	local va = VoxelArea:new({MinEdge = emin, MaxEdge = emax})
	for index in va:iterp(min, max) do
		data[index] = core.CONTENT_AIR
	end
	for index in va:iterp(cuboid_minmax(1)) do
		data[index] = core.get_content_id("basenodes:dirt")
	end
	vm:set_data(data)
	vm:write_to_map()

	-- Raycast many times from outside the cuboid
		for _ = 1, 100 do
			local ray_start
			repeat
				ray_start = random_point_in_area(cuboid_minmax(r))
			until not ray_start:in_area(cuboid_minmax(1.501))
			-- Pick a random position inside the dirt
			local ray_end = random_point_in_area(cuboid_minmax(1.499))
			-- The first pointed thing should have only air "in front" of it,
			-- or a dirt node got falsely skipped.
			-- NOTE: Skip perfect diagonals that might miss outside faces
			local dir = ray_end - ray_start
			local abs_dir = vector.new(math.abs(dir.x), math.abs(dir.y), math.abs(dir.z))
			local is_perfect_diagonal = (abs_dir.x == abs_dir.y and abs_dir.y == abs_dir.z)
			if not is_perfect_diagonal then
				local pt = core.raycast(ray_start, ray_end, false, false):next()
				if pt then
					assert(core.get_node(pt.above).name == "air")
				end
			end
		end

	vm:set_data(old_data)
	vm:write_to_map()
end

unittests.register("test_raycast_noskip", test_raycast_noskip, {map = true, random = true})
