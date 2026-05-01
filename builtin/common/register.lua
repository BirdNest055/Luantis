local builtin_shared = ...
local debug_getinfo = debug.getinfo

do
        local default = {mod = "??", name = "??"}
        core.callback_origins = setmetatable({}, {
                __index = function()
                        return default
                end
        })
end

-- NOTE: core.run_callbacks executes registered callbacks with different reduction
-- modes that control how return values are aggregated:
--   mode 0: Returns first callback's result (first-wins)
--   mode 1: Returns last callback's result (last-wins)
--   mode 2: Returns first non-nil/non-false result, or true if all return nil (OR)
--   mode 3: Returns first truthy result immediately (short-circuit OR), nil if none
--   mode 4: Returns first truthy result, or false if all return nil/false (AND-ish)
--   mode 5: Returns first truthy result immediately (short-circuit), nil if none
-- Empty callback list: modes 2/3 return true, modes 4/5 return false, others return nil.
function core.run_callbacks(callbacks, mode, ...)
        assert(type(callbacks) == "table")
        local cb_len = #callbacks
        if cb_len == 0 then
                if mode == 2 or mode == 3 then
                        return true
                elseif mode == 4 or mode == 5 then
                        return false
                end
        end
        local ret = nil
        for i = 1, cb_len do
                local origin = core.callback_origins[callbacks[i]]
                core.set_last_run_mod(origin.mod)
                local cb_ret = callbacks[i](...)

                if mode == 0 and i == 1 then
                        ret = cb_ret
                elseif mode == 1 and i == cb_len then
                        ret = cb_ret
                elseif mode == 2 then
                        if not cb_ret or i == 1 then
                                ret = cb_ret
                        end
                elseif mode == 3 then
                        if cb_ret then
                                return cb_ret
                        end
                        ret = cb_ret
                elseif mode == 4 then
                        if (cb_ret and not ret) or i == 1 then
                                ret = cb_ret
                        end
                elseif mode == 5 and cb_ret then
                        return cb_ret
                end
        end
        return ret
end

function builtin_shared.make_registration()
        local t = {}
        local registerfunc = function(func)
                t[#t + 1] = func
                core.callback_origins[func] = {
                        -- may be nil or return nil
                        mod = core.get_current_modname and core.get_current_modname() or "??",
                        name = debug_getinfo(1, "n").name or "??"
                }
        end
        return t, registerfunc
end

function builtin_shared.make_registration_reverse()
        local t = {}
        local registerfunc = function(func)
                table.insert(t, 1, func)
                core.callback_origins[func] = {
                        -- may be nil or return nil
                        mod = core.get_current_modname and core.get_current_modname() or "??",
                        name = debug_getinfo(1, "n").name or "??"
                }
        end
        return t, registerfunc
end
