core.detached_inventories = {}

-- NOTE: The raw C++ functions are captured and nilled to prevent bypassing the
-- Lua wrapper, which tracks inventory metadata (callbacks, mod_origin) in the
-- core.detached_inventories table. The C++ side only manages the inventory data;
-- the Lua side is responsible for callback dispatch and ownership tracking.
-- If create_detached_inventory_raw were called directly, the inventory would exist
-- in C++ but be invisible to core.detached_inventories, breaking callback lookups.
local create_detached_inventory_raw = core.create_detached_inventory_raw
core.create_detached_inventory_raw = nil

function core.create_detached_inventory(name, callbacks, player_name)
        local stuff = {}
        stuff.name = name
        if callbacks then
                stuff.allow_move = callbacks.allow_move
                stuff.allow_put = callbacks.allow_put
                stuff.allow_take = callbacks.allow_take
                stuff.on_move = callbacks.on_move
                stuff.on_put = callbacks.on_put
                stuff.on_take = callbacks.on_take
        end
        stuff.mod_origin = core.get_current_modname() or "??"
        core.detached_inventories[name] = stuff
        return create_detached_inventory_raw(name, player_name)
end

local remove_detached_inventory_raw = core.remove_detached_inventory_raw
core.remove_detached_inventory_raw = nil

function core.remove_detached_inventory(name)
        core.detached_inventories[name] = nil
        return remove_detached_inventory_raw(name)
end
