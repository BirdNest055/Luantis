local F = core.formspec_escape
local S = core.get_translator("__builtin")

-- NOTE: The death screen is re-shown when a player closes a non-death formspec
-- while dead, to prevent the "zombie state" where the player appears alive but
-- cannot interact (see issue #11523). The _reason parameter in show_death_screen
-- is currently unused but reserved for future death-screen customization (e.g.,
-- showing the damage type or killer name).
function core.show_death_screen(player, _reason)
        local fs = {
                "formspec_version[1]",
                "size[11,5.5,true]",
                "bgcolor[#320000b4;true]",
                "label[4.85,1.35;", F(S("You died")), "]",
                "button_exit[4,3;3,0.5;btn_respawn;", F(S("Respawn")), "]",
        }
        core.show_formspec(player:get_player_name(), "__builtin:death", table.concat(fs, ""))
end

core.register_on_dieplayer(function(player, reason)
        core.show_death_screen(player, reason)
end)

core.register_on_joinplayer(function(player)
        if player:get_hp() == 0 then
                core.show_death_screen(player, nil)
        end
end)

core.register_on_player_receive_fields(function(player, formname, fields)
        if formname == "__builtin:death" and fields.quit and player:get_hp() == 0 then
                player:respawn()
                core.log("action", player:get_player_name() .. " respawns at " ..
                                player:get_pos():to_string())
                return
        end

        -- If a non-death formspec was closed while the player is dead,
        -- re-show the death screen to prevent the "zombie state" (see #11523).
        if formname ~= "__builtin:death" and fields.quit and player:get_hp() == 0 then
                core.log("info", "Re-showing death screen to " .. player:get_player_name())
                core.show_death_screen(player, nil)
        end
end)
