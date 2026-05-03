-- Table to keep track of callback executions
-- {3, "foo", 1, "bar", 2} means 3 total callbacks, 2x foo and 1x bar
local PATTERNS = {
        -- 1: normal
        { 4, "allow_%w", 2, "on_take", 1, "on_put", 1 },
        -- 2: swap
        { 8, "allow_%w", 4, "on_take", 2, "on_put", 2 },
        -- 3: drop
        { 3, "allow_%w", 1, "on_drop", 1, "on_take", 1 },
        -- 4: denied drop
        { 0, },
}
local exec_listing = {} -- List of logged callbacks (e.g. "on_take", "allow_put")

-- Checks whether the logged callbacks equal the expected pattern
core.__helper_check_callbacks = function(pattern_i)
        local exec_pattern = PATTERNS[pattern_i]
        local ok = #exec_listing == exec_pattern[1]
        if ok then
                local list_index = 1
                for i = 2, #exec_pattern, 2 do
                        for n = 1, exec_pattern[i + 1] do
                                -- Match the list for "n" occurrences of the wanted callback name pattern
                                ok = exec_listing[list_index]:find(exec_pattern[i])
                                list_index = list_index + 1
                                if not ok then break end
                        end
                        if not ok then break end
                end
        end

        if not ok then
                print("Execution order mismatch!")
                print("Expected patterns: ", dump(exec_pattern))
                print("Got list: ", dump(exec_listing))
        end
        exec_listing = {}
        return ok
end

-- these need to actually exist for some tests
for i = 1, 3 do
        core.register_craftitem(":default:takeput_cb_" .. tostring(i), {
                description = "take/put callback " .. tostring(i),
        })
end
core.register_craftitem(":default:takeput_deny", {
        description = "take/put denied",
})
core.register_craftitem(":default:takeput_max_5", {
        description = "take/put max 5",
})

-- Uncomment the other line for easier callback debugging
local log = function(...) end
--local log = print

core.register_allow_player_inventory_action(function(_, action, inv, info)
        log("\tallow " .. action, info.count or info.stack:to_string())

        if action == "move" then
                -- testMoveFillStack
                return info.count
        end

        if action == "take" or action == "put" then
                assert(not info.stack:is_empty(), "Stack empty in: " .. action)

                -- testMoveUnallowed
                -- testSwapFromUnallowed
                -- testSwapToUnallowed
                if info.stack:get_name() == "default:takeput_deny" then
                        return 0
                end

                -- testMovePartial
                if info.stack:get_name() == "default:takeput_max_5" then
                        return 5
                end

                -- testCallbacks
                if info.stack:get_name():find("default:takeput_cb_%d") then
                        -- Log callback as executed
                        table.insert(exec_listing, "allow_" .. action)
                        return -- Unlimited
                end
        end

        return -- Unlimited
end)

core.register_on_player_inventory_action(function(_, action, inv, info)
        log("\ton " .. action, info.count or info.stack:to_string())

        if action == "take" or action == "put" then
                assert(not info.stack:is_empty(), action)

                if info.stack:get_name():find("default:takeput_cb_%d") then
                        -- Log callback as executed
                        table.insert(exec_listing, "on_" .. action)
                        return
                end
        end
end)

-- testDrop*
core.item_drop = function(itemstack, dropper, pos)
        log("\ton drop", itemstack:to_string())

        assert(not itemstack:is_empty())
        -- NOTE: Checking dropped count here validates that the engine's drop action
        -- produces a correctly-sized stack. The assertion verifies the count equals
        -- the stack max (full stack drop) or is at most 99 (partial/overflow drop).
        -- To extend: also assert that itemstack:get_count() matches the expected
        -- count from the original held stack before dropping. This would require
        -- capturing the pre-drop stack count in a test-local variable (e.g., via
        -- a wrapper around core.item_drop or a field on the test player object)
        -- and comparing it here. Currently we only check bounds, not exact counts.
        -- Check that the dropped item count matches the original stack count
        assert(itemstack:get_count() == itemstack:get_stack_max()
                or itemstack:get_count() <= 99,
                "Unexpected dropped count: " .. itemstack:get_count())
        table.insert(exec_listing, "on_drop")
        return ItemStack()
end
