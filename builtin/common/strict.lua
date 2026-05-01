local debug_getinfo, rawget, rawset = debug.getinfo, rawget, rawset

function core.global_exists(name)
        if type(name) ~= "string" then
                error("core.global_exists: " .. tostring(name) .. " is not a string")
        end
        return rawget(_G, name) ~= nil
end


-- NOTE: This module implements "strict mode" for the Lua environment by installing
-- a metatable on the global table (_G). It warns on:
--   - Assignment to undeclared globals inside functions (but allows it at file scope)
--   - Access of undeclared globals inside functions
-- Globals are "declared" the first time they are assigned. The `warned` table
-- deduplicates warnings per (source, line, variable) to avoid log spam.
-- This is a development aid — it does not prevent access, only logs warnings.
-- To explicitly declare a global without assigning a value, use: rawset(_G, name, nil)
local meta = {}
local declared = {}
-- Key is source file, line, and variable name; separated by NULs
local warned = {}

function meta:__newindex(name, value)
        rawset(self, name, value)
        if declared[name] then
                return
        end
        declared[name] = true
        local info = debug_getinfo(2, "Sl")
        if info == nil or info.what == "C" or info.what == "main" then
                return
        end
        local warn_key = ("%s\0%d\0%s"):format(info.source, info.currentline, name)
        if not warned[warn_key] then
                warned[warn_key] = true
                core.log("warning", ("Assignment to undeclared global %q inside a function at %s:%d")
                                :format(name, info.short_src, info.currentline))
        end
end


function meta:__index(name)
        if declared[name] then
                return
        end
        local info = debug_getinfo(2, "Sl")
        if info == nil or info.what == "C" then
                return
        end
        local warn_key = ("%s\0%d\0%s"):format(info.source, info.currentline, name)
        if not warned[warn_key] then
                warned[warn_key] = true
                core.log("warning", ("Undeclared global variable %q accessed at %s:%d")
                                :format(name, info.short_src, info.currentline))
        end
end

setmetatable(_G, meta)
