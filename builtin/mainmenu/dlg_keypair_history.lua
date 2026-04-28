-- Luanti-Secure
-- Copyright (C) 2026 Luanti-Secure contributors
-- SPDX-License-Identifier: LGPL-2.1-or-later
--
-- v9.41: Keypair history dialog.
-- Shows a detailed log of all server connections with keypair authentication,
-- including server name, username, server address, creation date, and last visit.

--------------------------------------------------------------------------------
-- Module: dlg_keypair_history
--
-- This dialog is a modular, standalone component that displays the full
-- connection history from keypair_server_users.json. It cross-references
-- with the favorites list to resolve server names for entries that don't
-- have one stored yet (backward compat with pre-v9.41 data).
--------------------------------------------------------------------------------

local function format_date(iso_str)
	if not iso_str or iso_str == "" then
		return fgettext("N/A")
	end
	-- Parse ISO 8601: "2026-04-28T10:00:00Z" → "2026-04-28 10:00"
	local date_part, time_part = iso_str:match("^(%d%d%d%d%-%d%d%-%d%d)T(%d%d:%d%d)")
	if date_part and time_part then
		return date_part .. " " .. time_part
	end
	return iso_str
end

--- Escape each cell individually, then join with commas.
local function escape_row(cells)
	local escaped = {}
	for i, cell in ipairs(cells) do
		escaped[i] = core.formspec_escape(tostring(cell))
	end
	return table.concat(escaped, ",")
end

--- Build a lookup table from favorites: "address:port" → server name
--- Used to resolve server names for entries that don't have one stored.
local function build_favorites_lookup()
	local lookup = {}
	local favorites = serverlistmgr.get_favorites()
	if favorites then
		for _, fav in ipairs(favorites) do
			if fav.address and fav.port and fav.name and fav.name ~= "" then
				local key = fav.address:lower() .. ":" .. fav.port
				lookup[key] = fav.name
			end
		end
	end
	return lookup
end

--- Resolve the display name for a server entry.
--- Priority: stored server_name > favorites lookup > address
local function resolve_server_name(entry, fav_lookup)
	if entry.server_name and entry.server_name ~= "" then
		return entry.server_name
	end
	-- Try favorites lookup
	local key = entry.server:lower()
	if fav_lookup[key] then
		return fav_lookup[key]
	end
	-- Fall back to address (without port for display)
	local host = entry.server:match("^([^:]+)")
	return host or entry.server
end

--------------------------------------------------------------------------------

local function keypair_history_formspec(dialogdata)
	local TOUCH_GUI = core.settings:get_bool("touch_gui")

	-- Gather data
	local keypair_auth_enabled = core.settings:get_bool("keypair_auth")
	local server_list = core.keypair_get_server_list and core.keypair_get_server_list() or {}
	local fav_lookup = build_favorites_lookup()

	-- Sort by last_used_at descending (most recent first)
	table.sort(server_list, function(a, b)
		return (a.last_used_at or "") > (b.last_used_at or "")
	end)

	-- Build history table rows
	-- 5 columns: Server Name, Username, Server Address, Created, Last Visited
	local history_table_rows = {}
	for i, entry in ipairs(server_list) do
		local display_name = resolve_server_name(entry, fav_lookup)
		history_table_rows[#history_table_rows + 1] = escape_row({
			display_name,
			entry.username or "",
			entry.server or "",
			format_date(entry.created_at),
			format_date(entry.last_used_at),
		})
	end
	local history_table_data = table.concat(history_table_rows, ",")

	-- Layout
	local y = 0.5
	local title_y = y
	y = y + 0.7

	-- Auth disabled warning
	local disabled_y = 0
	if not keypair_auth_enabled then
		disabled_y = y
		y = y + 0.9
	end

	-- Stats bar
	local stats_y = y
	y = y + 0.8

	-- History table
	local table_y = y
	local table_h = math.max(3, math.min(1.2 + #server_list * 0.55, 7))
	y = y + table_h + 1.2

	local total_height = math.max(y + 0.3, 7)

	------------------------------------------------------------------------
	-- Build formspec
	------------------------------------------------------------------------
	local formspec = {
		"formspec_version[4]",
		"size[14,", tostring(total_height), "]",
		TOUCH_GUI and "padding[0.01,0.01]" or "",

		-- Title
		"label[0.375,", tostring(title_y + 0.3), ";",
			fgettext("Keypair Connection History"), "]",
	}

	-- Auth disabled warning
	if not keypair_auth_enabled then
		table.insert_all(formspec, {
			"box[0.375,", tostring(disabled_y), ";13.25,0.6;#884400]",
			"label[0.625,", tostring(disabled_y + 0.3), ";",
				core.formspec_escape(fgettext("Keypair authentication is disabled in settings.")),
			"]",
		})
	end

	-- Stats bar
	local unique_users = {}
	for _, entry in ipairs(server_list) do
		unique_users[entry.username] = true
	end
	local user_count = 0
	for _ in pairs(unique_users) do user_count = user_count + 1 end

	table.insert_all(formspec, {
		"box[0.375,", tostring(stats_y), ";13.25,0.6;#333333]",
		"label[0.625,", tostring(stats_y + 0.3), ";",
			fgettext("Servers: $1  |  Usernames: $2", #server_list, user_count),
		"]",
	})

	-- History table
	if #server_list > 0 then
		table.insert_all(formspec, {
			"container[0.375,", tostring(table_y), "]",
			"tablecolumns[",
				"text,align=left,width=3.5;",      -- Server Name
				"text,align=left,width=2;",         -- Username
				"text,align=left,width=3;",         -- Server Address
				"text,align=center,width=2.25;",    -- Created
				"text,align=center,width=2.25",     -- Last Visited
			"]",
			"table[0,0;13.25,", tostring(table_h), ";keypair_history_table;",
				"Server Name,Username,Server Address,Created,Last Visited,",
				history_table_data, "]",
			"button[0,", tostring(table_h + 0.3), ";4.5,0.8;btn_forget_history;",
				fgettext("Forget Selected"), "]",
			"tooltip[btn_forget_history;",
				fgettext("Remove the selected server from your keypair history"), "]",
			"button[4.7,", tostring(table_h + 0.3), ";4.5,0.8;btn_refresh;",
				fgettext("Refresh"), "]",
			"tooltip[btn_refresh;",
				fgettext("Refresh the history list"), "]",
			"button[9.4,", tostring(table_h + 0.3), ";3.85,0.8;btn_close;",
				fgettext("Close"), "]",
			"container_end[]",
		})
	else
		table.insert_all(formspec, {
			"container[0.375,", tostring(table_y), "]",
			"label[0,0.3;",
				core.formspec_escape(fgettext("No connection history yet. Connect to a server using keypair authentication to build your history.")),
			"]",
			"button[4.5,1.2;4.5,0.8;btn_close;",
				fgettext("Close"), "]",
			"container_end[]",
		})
	end

	return table.concat(formspec, "")
end

--------------------------------------------------------------------------------

local function keypair_history_buttonhandler(this, fields)
	-- Close button
	if fields.btn_close or fields.quit then
		this:delete()
		return true
	end

	-- Forget selected server
	if fields.btn_forget_history then
		local selected = core.get_table_index("keypair_history_table")
		if selected and selected > 0 then
			local server_list = core.keypair_get_server_list and core.keypair_get_server_list() or {}
			-- Sort the same way as the formspec
			table.sort(server_list, function(a, b)
				return (a.last_used_at or "") > (b.last_used_at or "")
			end)
			if server_list[selected] then
				local server_addr = server_list[selected].server
				if core.keypair_forget_server then
					core.keypair_forget_server(server_addr)
				end
			end
		end
		return true
	end

	-- Refresh button
	if fields.btn_refresh then
		return true  -- formspec will be regenerated on next update
	end

	return false
end

--------------------------------------------------------------------------------

function create_keypair_history_dialog()
	local retval = dialog_create("dlg_keypair_history",
		keypair_history_formspec,
		keypair_history_buttonhandler,
		nil)
	return retval
end
