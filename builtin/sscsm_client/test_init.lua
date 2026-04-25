-- Test for sscsm_client/init.lua content defs
-- TDD: Write test first (Red phase)

-- Mock the content defs table that should be sent from server
local content_defs = {
	["default:dirt"] = 1,
	["default:stone"] = 2,
	["default:wood"] = 3,
}

-- Test that get_content_id returns proper id for known items
local id = core.get_content_id("default:dirt")
if id ~= 1 then
	error("Expected content id 1 for 'default:dirt', got " .. tostring(id))
end

-- Test that get_name_from_content_id returns proper name
local name = core.get_name_from_content_id(2)
if name ~= "default:stone" then
	error("Expected 'default:stone' for id 2, got " .. tostring(name))
end

-- Test with unknown item (should return nil or 0)
local unknown_id = core.get_content_id("unknown:item")
if unknown_id ~= nil and unknown_id ~= 0 then
	error("Expected nil or 0 for unknown item, got " .. tostring(unknown_id))
end

print("All content def tests passed!")
