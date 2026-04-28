-- Luantis v9.39: Voice chat HUD and UX
-- This module provides:
--   1. HUD indicators showing who's talking
--   2. PTT status indicator
--   3. Voice chat overlay for groups and muting
--   4. Chat commands for voice chat management

local voice = {}

-- HUD state
voice.hud_ids = {}           -- peer_id -> { name_id, indicator_id }
voice.ptt_hud_id = nil       -- PTT status indicator
voice.self_talking_hud = nil -- "YOU ARE TALKING" indicator
voice.peer_states = {}       -- peer_id -> { name, talking, muted, enabled }
voice.is_local_talking = false
voice.server_allowed = false   -- v9.44: Server controls whether voice is ON
voice.receive_enabled = true   -- v9.44: Client can opt out of receiving
voice.hud_initialized = false -- track if HUD has been set up

-- Positioning constants
local HUD_MARGIN_TOP = 40
local HUD_MARGIN_RIGHT = 20
local HUD_LINE_HEIGHT = 20
local HUD_MAX_VISIBLE = 8

-- Color constants
local COLOR_TALKING = 0xFF44FF44   -- Green — player is talking
local COLOR_MUTED   = 0xFFFF4444   -- Red — player is muted
local COLOR_IDLE    = 0xFFAAAAAA   -- Gray — player is idle
local COLOR_PTT     = 0xFF44FF44   -- Green PTT indicator
local COLOR_OFF     = 0xFF888888   -- Gray when voice off

-- Initialize voice chat HUD
function voice.init()
        if not core.localplayer then return end

        -- Create PTT status indicator (top-right)
        voice.ptt_hud_id = core.localplayer:hud_add({
                hud_elem_type = "text",
                position = { x = 1, y = 0 },
                offset = { x = -200, y = HUD_MARGIN_TOP },
                text = "",
                number = COLOR_OFF,
                alignment = { x = -1, y = 1 },
                scale = { x = 100, y = 100 },
                z_index = 100,
        })

        -- Create "talking" indicator for local player
        voice.self_talking_hud = core.localplayer:hud_add({
                hud_elem_type = "text",
                position = { x = 1, y = 0 },
                offset = { x = -200, y = HUD_MARGIN_TOP + HUD_LINE_HEIGHT },
                text = "",
                number = COLOR_TALKING,
                alignment = { x = -1, y = 1 },
                scale = { x = 100, y = 100 },
                z_index = 100,
        })
end

-- Update HUD for all talking players
function voice.update_hud()
        if not core.localplayer then return end

        -- Remove old HUD elements for peers that no longer exist
        for peer_id, ids in pairs(voice.hud_ids) do
                if not voice.peer_states[peer_id] then
                        core.localplayer:hud_remove(ids.name_id)
                        core.localplayer:hud_remove(ids.indicator_id)
                        voice.hud_ids[peer_id] = nil
                end
        end

        -- Count visible talking peers
        local visible = {}
        for peer_id, state in pairs(voice.peer_states) do
                if state.talking and not state.muted then
                        table.insert(visible, { peer_id = peer_id, name = state.name })
                end
        end

        -- Sort by name for consistent ordering
        table.sort(visible, function(a, b) return a.name < b.name end)

        -- Limit visible entries
        local count = math.min(#visible, HUD_MAX_VISIBLE)

        -- Update or create HUD elements for visible peers
        for i = 1, count do
                local peer = visible[i]
                local y_offset = HUD_MARGIN_TOP + (i + 1) * HUD_LINE_HEIGHT

                if not voice.hud_ids[peer.peer_id] then
                        -- Create indicator dot
                        local indicator_id = core.localplayer:hud_add({
                                hud_elem_type = "text",
                                position = { x = 1, y = 0 },
                                offset = { x = -200, y = y_offset },
                                text = "🎤",
                                number = COLOR_TALKING,
                                alignment = { x = -1, y = 1 },
                                scale = { x = 100, y = 100 },
                                z_index = 100,
                        })

                        -- Create name text
                        local name_id = core.localplayer:hud_add({
                                hud_elem_type = "text",
                                position = { x = 1, y = 0 },
                                offset = { x = -180, y = y_offset },
                                text = peer.name,
                                number = COLOR_TALKING,
                                alignment = { x = -1, y = 1 },
                                scale = { x = 100, y = 100 },
                                z_index = 100,
                        })

                        voice.hud_ids[peer.peer_id] = {
                                name_id = name_id,
                                indicator_id = indicator_id,
                        }
                else
                        -- Update position
                        local ids = voice.hud_ids[peer.peer_id]
                        core.localplayer:hud_change(ids.indicator_id, "offset",
                                { x = -200, y = y_offset })
                        core.localplayer:hud_change(ids.name_id, "offset",
                                { x = -180, y = y_offset })
                        core.localplayer:hud_change(ids.name_id, "text", peer.name)
                        core.localplayer:hud_change(ids.name_id, "number", COLOR_TALKING)
                end
        end

        -- Remove HUD elements for peers who stopped talking
        for peer_id, ids in pairs(voice.hud_ids) do
                local state = voice.peer_states[peer_id]
                if not state or not state.talking or state.muted then
                        core.localplayer:hud_remove(ids.name_id)
                        core.localplayer:hud_remove(ids.indicator_id)
                        voice.hud_ids[peer_id] = nil
                end
        end

        -- Update PTT status text
        if voice.ptt_hud_id then
                local mode = core.settings:get("voice_chat_mode") or "ptt"
                if not voice.server_allowed then
                        core.localplayer:hud_change(voice.ptt_hud_id, "text",
                                "[Voice: SERVER OFF]")
                        core.localplayer:hud_change(voice.ptt_hud_id, "number", COLOR_MUTED)
                elseif not voice.receive_enabled then
                        core.localplayer:hud_change(voice.ptt_hud_id, "text",
                                "[Voice: MUTED]")
                        core.localplayer:hud_change(voice.ptt_hud_id, "number", COLOR_OFF)
                elseif mode == "ptt" then
                        core.localplayer:hud_change(voice.ptt_hud_id, "text",
                                "[Voice: PTT (~ key)]")
                        core.localplayer:hud_change(voice.ptt_hud_id, "number", COLOR_IDLE)
                else
                        core.localplayer:hud_change(voice.ptt_hud_id, "text",
                                "[Voice: TOGGLE]")
                        core.localplayer:hud_change(voice.ptt_hud_id, "number", COLOR_IDLE)
                end
        end

        -- Update "you are talking" indicator
        if voice.self_talking_hud then
                if voice.is_local_talking and voice.server_allowed and voice.receive_enabled then
                        core.localplayer:hud_change(voice.self_talking_hud, "text",
                                ">> YOU ARE TALKING <<")
                        core.localplayer:hud_change(voice.self_talking_hud, "number",
                                COLOR_TALKING)
                else
                        core.localplayer:hud_change(voice.self_talking_hud, "text", "")
                end
        end
end

-- Called when a peer starts/stops talking
function voice.peer_talking(peer_id, talking)
        if not voice.peer_states[peer_id] then
                voice.peer_states[peer_id] = {
                        name = "Player" .. peer_id,
                        talking = false,
                        muted = false,
                        enabled = true,
                }
        end
        voice.peer_states[peer_id].talking = talking
        voice.update_hud()
end

-- Called when local player starts/stops talking
function voice.set_local_talking(talking)
        voice.is_local_talking = talking
        voice.update_hud()
end

-- Mute/unmute a player
function voice.mute_player(peer_id, muted)
        if voice.peer_states[peer_id] then
                voice.peer_states[peer_id].muted = muted
                voice.update_hud()

                -- Notify via chat
                local name = voice.peer_states[peer_id].name or ("Player" .. peer_id)
                if muted then
                        core.display_chat_message("[Voice] Muted " .. name)
                else
                        core.display_chat_message("[Voice] Unmuted " .. name)
                end
        end
end

-- Toggle receive opt-out (mute all incoming voice)
-- v9.44: Server controls whether voice is ON; client can only opt out
function voice.toggle_receive()
        if not voice.server_allowed then
                core.display_chat_message("[Voice] Voice chat is disabled by the server")
                return
        end
        voice.receive_enabled = not voice.receive_enabled
        core.settings:set_bool("voice_chat_receive", voice.receive_enabled)
        voice.update_hud()
        if voice.receive_enabled then
                core.display_chat_message("[Voice] Voice unmuted (opted in)")
        else
                core.display_chat_message("[Voice] Voice muted (opted out)")
        end
end

-- Chat commands
core.register_chatcommand("voice", {
        description = "Voice chat commands: mute, unmute, mute <name>, unmute <name>, status",
        params = "<mute|unmute|status> [name]",
        func = function(param)
                local args = param:split(" ")
                local cmd = args[1] or ""

                if cmd == "mute" then
                        -- Global mute (opt out of receiving)
                        if not voice.server_allowed then
                                return false, "[Voice] Voice chat is disabled by the server"
                        end
                        voice.receive_enabled = false
                        core.settings:set_bool("voice_chat_receive", false)
                        voice.update_hud()
                        return true, "[Voice] All incoming voice muted (opted out)"

                elseif cmd == "unmute" then
                        -- Global unmute (opt back in)
                        if not voice.server_allowed then
                                return false, "[Voice] Voice chat is disabled by the server"
                        end
                        voice.receive_enabled = true
                        core.settings:set_bool("voice_chat_receive", true)
                        voice.update_hud()
                        return true, "[Voice] Voice unmuted (opted in)"

                elseif cmd == "mute_player" then
                        local name = args[2]
                        if not name or name == "" then
                                return false, "Usage: /voice mute_player <player_name>"
                        end
                        -- Find peer_id by name
                        for peer_id, state in pairs(voice.peer_states) do
                                if state.name:lower() == name:lower() then
                                        voice.mute_player(peer_id, true)
                                        return true, "[Voice] Muted " .. state.name
                                end
                        end
                        return false, "[Voice] Player '" .. name .. "' not found in voice chat"

                elseif cmd == "unmute_player" then
                        local name = args[2]
                        if not name or name == "" then
                                return false, "Usage: /voice unmute_player <player_name>"
                        end
                        for peer_id, state in pairs(voice.peer_states) do
                                if state.name:lower() == name:lower() then
                                        voice.mute_player(peer_id, false)
                                        return true, "[Voice] Unmuted " .. state.name
                                end
                        end
                        return false, "[Voice] Player '" .. name .. "' not found in voice chat"

                elseif cmd == "status" then
                        local lines = { "[Voice] Status:" }
                        lines[#lines + 1] = "  Voice chat: " ..
                                (voice.server_allowed and "SERVER ON" or "SERVER OFF")
                        lines[#lines + 1] = "  Receiving: " ..
                                (voice.receive_enabled and "YES" or "MUTED (opted out)")
                        lines[#lines + 1] = "  Mode: " ..
                                (core.settings:get("voice_chat_mode") or "ptt")
                        lines[#lines + 1] = "  E2EE: " ..
                                (core.settings:get_bool("voice_chat_e2ee") and "ON" or "OFF")

                        local talking_count = 0
                        for _, state in pairs(voice.peer_states) do
                                if state.talking then talking_count = talking_count + 1 end
                        end
                        lines[#lines + 1] = "  Peers talking: " .. talking_count

                        -- List voice-enabled peers
                        for peer_id, state in pairs(voice.peer_states) do
                                local status = ""
                                if state.talking then status = status .. " TALKING" end
                                if state.muted then status = status .. " MUTED" end
                                if state.enabled then status = status .. " enabled" end
                                lines[#lines + 1] = "  " .. state.name .. ": " .. status
                        end

                        return true, table.concat(lines, "\n")

                else
                        return false, "Usage: /voice <mute|unmute|mute_player|unmute_player|status> [name]"
                end
        end,
})

-- Periodic HUD update (check settings changes)
local function on_step(dtime)
        -- Keep voice.receive_enabled in sync with settings
        local setting_receive = core.settings:get_bool("voice_chat_receive")
        if setting_receive ~= voice.receive_enabled then
                voice.receive_enabled = setting_receive
                voice.update_hud()
        end
end

-- Register the step function
core.register_globalstep(function(dtime)
        -- Initialize HUD once localplayer becomes available (client-side)
        if not voice.hud_initialized and core.localplayer then
                voice.receive_enabled = core.settings:get_bool("voice_chat_receive")
                voice.server_allowed = true  -- Will be updated by TOCLIENT_VOICE_STATE
                voice.init()
                voice.hud_initialized = true
        end
        if voice.hud_initialized then
                on_step(dtime)
        end
end)

-- Initialize voice state for joining players (server-side)
core.register_on_joinplayer(function(player)
        if player then
                local name = player:get_player_name()
                -- Track the player in peer states
                voice.peer_states[name] = {
                        name = name,
                        talking = false,
                        muted = false,
                        enabled = true,
                }
        end
end)

-- Clean up when players leave
core.register_on_leaveplayer(function(player)
        if player then
                local name = player:get_player_name()
                voice.peer_states[name] = nil
                -- Remove HUD elements for departed player
                if voice.hud_ids[name] then
                        core.localplayer:hud_remove(voice.hud_ids[name].name_id)
                        core.localplayer:hud_remove(voice.hud_ids[name].indicator_id)
                        voice.hud_ids[name] = nil
                end
        end
end)

return voice
