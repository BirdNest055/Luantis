-- Luanti-Secure
-- Copyright (C) 2026 Luanti-Secure contributors
-- SPDX-License-Identifier: LGPL-2.1-or-later
--
-- v9.38: Extensible Server Info Overlay section system.
--
-- The C++ side renders the overlay when the SERVERINFO key (Tab) is held.
-- This Lua module provides a section registry so that mods can register
-- additional info sections that appear in the overlay.
--
-- Usage (from a mod):
--   server_info_overlay.register_section("my_section", {
--       title = "My Custom Info",
--       priority = 50,           -- lower = shown first; built-ins: 10, 20, 30
--       get_lines = function()
--           return { "Line 1", "Line 2" }
--       end,
--   })
--
-- Built-in sections:
--   "players"    (priority 10) — online player list
--   "server"     (priority 20) — server address, ping, protocol
--   "connection" (priority 30) — connection statistics

server_info_overlay = {}

--------------------------------------------------------------------------------
-- Section registry
--------------------------------------------------------------------------------

--- Registered sections table.
--- Each entry: { name = string, title = string, priority = number, get_lines = function }
local sections = {}

--- Register a new info section for the overlay.
--- @param name string    Unique section identifier (e.g. "my_mod:custom_stats")
--- @param def table      Section definition with:
---   title     string      Display title for the section
---   priority  number      Order priority (lower = shown first); default 100
---   get_lines function()  Returns an array of strings (one per line)
function server_info_overlay.register_section(name, def)
	if type(name) ~= "string" or name == "" then
		core.log("warning", "server_info_overlay: section name must be a non-empty string")
		return
	end
	if type(def) ~= "table" then
		core.log("warning", "server_info_overlay: section definition must be a table")
		return
	end
	if type(def.get_lines) ~= "function" then
		core.log("warning", "server_info_overlay: section '" .. name .. "' needs a get_lines function")
		return
	end

	sections[name] = {
		name = name,
		title = def.title or name,
		priority = def.priority or 100,
		get_lines = def.get_lines,
	}
end

--- Unregister a section (useful for mod disabling).
--- @param name string  The section identifier to remove
function server_info_overlay.unregister_section(name)
	sections[name] = nil
end

--- Get all registered sections, sorted by priority.
--- @return table  Array of section defs in display order
function server_info_overlay.get_sections()
	local sorted = {}
	for _, sec in pairs(sections) do
		sorted[#sorted + 1] = sec
	end
	table.sort(sorted, function(a, b) return a.priority < b.priority end)
	return sorted
end

--- Collect all section data as a nested table.
--- Returns: { {name=, title=, lines={}}, ... }
--- This can be called from C++ or Lua to get the current overlay content.
function server_info_overlay.collect_data()
	local data = {}
	local sorted = server_info_overlay.get_sections()
	for _, sec in ipairs(sorted) do
		local ok, lines = pcall(sec.get_lines)
		if ok and type(lines) == "table" then
			data[#data + 1] = {
				name = sec.name,
				title = sec.title,
				lines = lines,
			}
		elseif not ok then
			core.log("warning", "server_info_overlay: section '" .. sec.name .. "' get_lines error: " .. tostring(lines))
		end
	end
	return data
end

--- Get the full overlay content as a flat string (for debug/testing).
--- Each section is separated by a blank line, each line by newline.
function server_info_overlay.dump()
	local parts = {}
	local data = server_info_overlay.collect_data()
	for _, sec in ipairs(data) do
		parts[#parts + 1] = "== " .. sec.title .. " =="
		for _, line in ipairs(sec.lines) do
			parts[#parts + 1] = "  " .. line
		end
		parts[#parts + 1] = ""
	end
	return table.concat(parts, "\n")
end

--------------------------------------------------------------------------------
-- Built-in sections
--------------------------------------------------------------------------------

--- Section: Players (priority 10)
--- Shows the list of online players.
server_info_overlay.register_section("players", {
	title = "Players",
	priority = 10,
	get_lines = function()
		local names = {}
		if core.get_player_names then
			local plist = core.get_player_names()
			if plist then
				for _, name in ipairs(plist) do
					names[#names + 1] = name
				end
			end
		end
		table.sort(names)

		local lines = {}
		lines[1] = #names .. " online"
		for i, name in ipairs(names) do
			lines[#lines + 1] = "  " .. name
		end
		return lines
	end,
})

--- Section: Server (priority 20)
--- Shows server address, ping, and protocol version.
server_info_overlay.register_section("server", {
	title = "Server",
	priority = 20,
	get_lines = function()
		local lines = {}
		if core.get_server_info then
			local info = core.get_server_info()
			if info then
				lines[#lines + 1] = "Address: " .. (info.address or "?") .. ":" .. tostring(info.port or "?")
				lines[#lines + 1] = "Protocol: " .. tostring(info.protocol_version or "?")
			end
		end
		if core.get_connection_stats then
			local stats = core.get_connection_stats()
			if stats then
				local rtt = math.floor(stats.rtt_ms or 0)
				lines[#lines + 1] = "Ping: " .. rtt .. " ms"
			end
		end
		return lines
	end,
})

--- Section: Connection (priority 30)
--- Shows detailed connection statistics.
server_info_overlay.register_section("connection", {
	title = "Connection",
	priority = 30,
	get_lines = function()
		local lines = {}
		if core.get_connection_stats then
			local stats = core.get_connection_stats()
			if stats then
				local rtt = math.floor(stats.rtt_ms or 0)
				if rtt < 50 then
					lines[#lines + 1] = "Status: Excellent"
				elseif rtt < 150 then
					lines[#lines + 1] = "Status: Good"
				elseif rtt < 300 then
					lines[#lines + 1] = "Status: Fair"
				else
					lines[#lines + 1] = "Status: Poor"
				end
			end
		else
			lines[#lines + 1] = "Status: Connected"
		end
		return lines
	end,
})

--------------------------------------------------------------------------------
-- Chat command for testing the section system
--------------------------------------------------------------------------------

core.register_chatcommand("server_info", {
	description = "Show server info overlay section data (debug)",
	func = function(name)
		local data = server_info_overlay.collect_data()
		local parts = {}
		for _, sec in ipairs(data) do
			parts[#parts + 1] = "== " .. sec.title .. " =="
			for _, line in ipairs(sec.lines) do
				parts[#parts + 1] = "  " .. line
			end
		end
		if #parts == 0 then
			return true, "No server info sections registered."
		end
		return true, table.concat(parts, "\n")
	end,
})
