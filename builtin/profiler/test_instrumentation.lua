-- Test for path canonicalization in profiler instrumentation
-- TDD: Test first (fails), then implement fix

-- Mock core functions for testing
local mock_modpaths = {}
local mock_builtin_path = "/usr/share/luanti/builtin/"
local mock_worldpath = "/home/user/.minetest/worlds/test/"
local mock_user_path = "/home/user/.minetest/"

-- Function to canonicalize paths (resolve .. and .)
local function canonicalize_path(path)
	-- Split path into components
	local parts = {}
	local is_absolute = path:sub(1, 1) == "/"
	
	-- Split by "/" and process each component
	for part in path:gmatch("[^/]+") do
		if part == ".." then
			-- Go up one level if possible
			if #parts > 0 and parts[#parts] ~= ".." then
				table.remove(parts, #parts)
			elseif not is_absolute then
				-- For relative paths, keep ".." if we can't go up
				table.insert(parts, "..")
			end
		elseif part ~= "." and part ~= "" then
			-- Skip "." and empty components
			table.insert(parts, part)
		end
	end
	
	-- Rebuild the path
	local result = table.concat(parts, "/")
	if is_absolute then
		result = "/" .. result
	end
	if result == "" then
		result = is_absolute and "/" or "."
	end
	return result
end

-- Test cases for canonicalize_path
local tests = {
	{ input = "/usr/share/luanti/bin/../builtin", expected = "/usr/share/luanti/builtin" },
	{ input = "/home/user/./.minetest", expected = "/home/user/.minetest" },
	{ input = "/a/b/c/../../d", expected = "/a/d" },
	{ input = "/a/b/../c/../d", expected = "/a/d" },
	{ input = "/a/b/c", expected = "/a/b/c" },  -- No change needed
	{ input = "relative/path/../to/file", expected = "relative/to/file" },
}

local failures = 0
for i, test in ipairs(tests) do
	local result = canonicalize_path(test.input)
	if result ~= test.expected then
		print(string.format("FAIL test %d: input='%s', expected='%s', got='%s'",
			i, test.input, test.expected, result))
		failures = failures + 1
	else
		print(string.format("PASS test %d: '%s' -> '%s'", i, test.input, result))
	end
end

if failures > 0 then
	print(string.format("\n%d tests failed!", failures))
	os.exit(1)
else
	print("\nAll tests passed!")
	os.exit(0)
end
