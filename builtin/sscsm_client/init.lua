local scriptpath = core.get_builtin_path()
local commonpath = scriptpath .. "common" .. DIR_DELIM
local mypath     = scriptpath .. "sscsm_client".. DIR_DELIM

-- Shared between builtin files, but
-- not exposed to outer context
local builtin_shared = {}

-- Content definitions storage for SSCM environment
-- Populated by the server when sending content defs
local content_id_to_name = {}
local content_name_to_id = {}

-- Register content definitions sent from server
function core.register_content_def(name, id)
        if type(name) ~= "string" or type(id) ~= "number" then
                core.log("error", "Invalid content def registration: " .. dump(name) .. " -> " .. dump(id))
                return false
        end
        content_name_to_id[name] = id
        content_id_to_name[id] = name
        return true
end

-- Get content ID from name (returns nil if not found)
function core.get_content_id(name)
        if type(name) ~= "string" then
                return nil
        end
        return content_name_to_id[name] or tonumber(name) -- Fallback: try to convert
end

-- Get content name from ID (returns nil if not found)  
function core.get_name_from_content_id(id)
        if type(id) ~= "number" then
                return nil
        end
        return content_id_to_name[id] or tostring(id) -- Fallback: convert to string
end

-- NOTE: Server should call register_content_def() for each content type during
-- SSCSM init. Currently, content_id_to_name and content_name_to_id remain empty
-- until mods manually register definitions, which means core.get_content_id() and
-- core.get_name_from_content_id() return nil for all server-defined content types.
-- Proposed fix:
--   1. During SSCSM initialization, the server serializes the full content ID map
--      (node names, item names, etc.) into the SSCSM startup data.
--   2. The client-side SSCSM loader iterates the map and calls
--      core.register_content_def(name, id) for each entry before loading mod files.
--   3. This ensures that content IDs are available to SSCSM code from the start,
--      matching the behavior of the main client environment where IDs are
--      populated during TOCLIENT_ITEMDEF / TOCLIENT_NODEDEF processing.
-- Blocked on: SSCSM startup data serialization (see sscsm_irequest.h).

assert(loadfile(commonpath .. "item_s.lua"))(builtin_shared)
assert(loadfile(commonpath .. "register.lua"))(builtin_shared)
assert(loadfile(mypath .. "register.lua"))(builtin_shared)

dofile(commonpath .. "after.lua")

-- unset, as promised in initializeSecuritySSCSM()
debug.getinfo = nil
