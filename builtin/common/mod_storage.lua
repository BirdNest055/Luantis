-- NOTE: This module replaces core.get_mod_storage with a version that automatically
-- resolves the current mod name, so mods can simply call core.get_mod_storage()
-- without passing their own name. The `storages` table uses weak values (__mode="v")
-- so that if a mod's StorageRef is no longer referenced anywhere, it can be
-- garbage-collected. A subsequent call will re-create it via old_get_mod_storage.
-- If called outside a mod context (e.g., from a standalone script), get_current_modname()
-- returns nil and get_mod_storage() returns nil.

local get_current_modname = core.get_current_modname

local old_get_mod_storage = core.get_mod_storage

local storages = setmetatable({}, {
        __mode = "v", -- values are weak references (can be garbage-collected)
        __index = function(self, modname)
                local storage = old_get_mod_storage(modname)
                self[modname] = storage
                return storage
        end,
})

function core.get_mod_storage()
        local modname = get_current_modname()
        return modname and storages[modname]
end
