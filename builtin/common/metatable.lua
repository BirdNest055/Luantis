-- NOTE: Registered metatables enable the C++ serialization packer to preserve
-- metatable identity across serialize/deserialize and across async environment
-- boundaries. The known_metatables table serves as a bidirectional lookup:
--   name → mt (for deserialization: given a name, find the metatable)
--   mt → name (for serialization: given a metatable, find its name)
-- Without registration, serialized tables lose their metatables and deserialize
-- as plain tables. The assertion prevents accidental double-registration with
-- a different metatable object, which would break deserialization consistency.
local known_metatables = {}
function core.register_portable_metatable(name, mt)
        assert(type(name) == "string", ("attempt to use %s value as metatable name"):format(type(name)))
        assert(type(mt) == "table", ("attempt to register a %s value as metatable"):format(type(mt)))
        assert(known_metatables[name] == nil or known_metatables[name] == mt,
                        ("attempt to override metatable %s"):format(name))
        known_metatables[name] = mt
        known_metatables[mt] = name
end
core.known_metatables = known_metatables

function core.register_async_metatable(...)
        core.log("deprecated", "core.register_async_metatable is deprecated. " ..
                        "Use core.register_portable_metatable instead.")
        return core.register_portable_metatable(...)
end

core.register_portable_metatable("__builtin:vector", vector.metatable)
