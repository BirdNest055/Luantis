-- Luanti-Secure
-- Copyright (C) 2026 Luanti-Secure contributors
-- SPDX-License-Identifier: LGPL-2.1-or-later
--
-- v9.37: Per-username keypair management dialog.
-- Each username gets its own Ed25519 keypair so that different
-- characters on the same server each have independent keys.

--------------------------------------------------------------------------------

local function format_date(iso_str)
	if not iso_str or iso_str == "" then
		return fgettext("Unknown")
	end
	-- Parse ISO 8601: "2026-04-28T10:00:00Z" → "2026-04-28 10:00"
	local date_part, time_part = iso_str:match("^(%d%d%d%d%-%d%d%-%d%d)T(%d%d:%d%d)")
	if date_part and time_part then
		return date_part .. " " .. time_part
	end
	return iso_str
end

--------------------------------------------------------------------------------

local function keypair_manager_formspec(dialogdata)
	local TOUCH_GUI = core.settings:get_bool("touch_gui")

	-- Gather keypair info
	local keypair_auth_enabled = core.settings:get_bool("keypair_auth")
	local keypairs = core.keypair_list_keypairs and core.keypair_list_keypairs() or {}
	local server_list = core.keypair_get_server_list and core.keypair_get_server_list() or {}
	local current_username = core.settings:get("name") or ""

	-- Find if the current user has a keypair
	local current_user_has_key = false
	local current_pubkey_b64 = ""
	for _, kp in ipairs(keypairs) do
		if kp.username == current_username then
			current_user_has_key = true
			current_pubkey_b64 = kp.public_key_base64 or ""
			break
		end
	end

	-- Truncate public key for display
	local pubkey_display = current_pubkey_b64
	if #pubkey_display > 28 then
		pubkey_display = current_pubkey_b64:sub(1, 10) .. "..." .. current_pubkey_b64:sub(#current_pubkey_b64 - 9)
	end

	-- Build keypairs table rows (2 columns: Username, Public Key)
	local keypair_table_rows = {}
	for i, kp in ipairs(keypairs) do
		local pk_display = kp.public_key_base64 or ""
		if #pk_display > 24 then
			pk_display = pk_display:sub(1, 8) .. "..." .. pk_display:sub(#pk_display - 7)
		end
		keypair_table_rows[#keypair_table_rows + 1] = core.formspec_escape(
			kp.username .. "," .. pk_display
		)
	end
	local keypair_table_data = table.concat(keypair_table_rows, ",")

	-- Build server list rows (4 columns: Server, Username, Created, Last Used)
	local server_table_rows = {}
	for i, entry in ipairs(server_list) do
		server_table_rows[#server_table_rows + 1] = core.formspec_escape(
			(entry.server or "") .. ","
			.. (entry.username or "") .. ","
			.. format_date(entry.created_at) .. ","
			.. format_date(entry.last_used_at)
		)
	end
	local server_table_data = table.concat(server_table_rows, ",")

	-- =============================================
	-- Layout: calculate Y positions top-to-bottom
	-- =============================================
	local y = 0.6          -- start after top padding
	local FORM_WIDTH = 12  -- total form width

	-- Section 1: Title
	local title_y = y
	y = y + 0.8

	-- Section 2: Status warnings (disabled / no keypair)
	local warning_y = y
	local has_warning = false

	if not keypair_auth_enabled then
		has_warning = true
	end
	if not current_user_has_key and keypair_auth_enabled and current_username ~= "" then
		has_warning = true
	end

	if has_warning then
		y = y + 1.2  -- box + label height
	end

	-- Section 3: Current user status bar
	local status_y = y
	y = y + 1.0

	-- Section 4: Public key display
	local pubkey_y = y
	y = y + 1.6

	-- Section 5: All keypairs table
	local keypairs_y = y
	local keypairs_section_height = 0
	if #keypairs > 0 then
		keypairs_section_height = 4.2  -- label + table + button + spacing
		y = y + keypairs_section_height
	end

	-- Section 6: Server list
	local servers_y = y
	local servers_section_height = 0
	if #server_list > 0 then
		servers_section_height = 6.2  -- label + table + button + spacing
		y = y + servers_section_height
	else
		servers_section_height = 0.6  -- just the "no servers" label
		y = y + servers_section_height
	end

	-- Section 7: Regenerate section
	local regenerate_y = y + 0.4
	y = regenerate_y + 1.8  -- separator + label + button row + spacing

	-- Warning / success messages extend below the regenerate section
	if dialogdata.show_regenerate_warning then
		y = y + 1.4
	elseif dialogdata.show_regenerate_success then
		y = y + 0.8
	end

	local total_height = y + 0.5  -- bottom padding

	-- =============================================
	-- Build formspec
	-- =============================================
	local formspec = {
		"formspec_version[4]",
		"size[", tostring(FORM_WIDTH), ",", tostring(total_height), "]",
		TOUCH_GUI and "padding[0.01,0.01]" or "",

		-- Title
		"label[0.375,", tostring(title_y), ";", fgettext("Keypair Manager"), "]",
	}

	-- Disabled warning
	if not keypair_auth_enabled then
		table.insert_all(formspec, {
			"box[0.375,", tostring(warning_y), ";11.25,0.6;#884400]",
			"label[0.625,", tostring(warning_y + 0.3), ";",
			core.formspec_escape(fgettext("Keypair authentication is disabled in settings.")),
			"]",
		})
	end

	-- No keypair warning
	if not current_user_has_key and keypair_auth_enabled and current_username ~= "" then
		local no_key_y = warning_y
		if not keypair_auth_enabled then
			no_key_y = warning_y + 0.8  -- below the disabled warning
		end
		table.insert_all(formspec, {
			"box[0.375,", tostring(no_key_y), ";11.25,0.6;#884400]",
			"label[0.625,", tostring(no_key_y + 0.3), ";",
			core.formspec_escape(fgettext("No keypair for '%s'. One will be generated on first connect.", current_username)),
			"]",
		})
	end

	-- Current user status bar
	table.insert_all(formspec, {
		"box[0.375,", tostring(status_y), ";11.25,0.6;#333333]",
		"label[0.625,", tostring(status_y + 0.3), ";",
		fgettext("Current user: $1 — Keypair: $2",
			current_username,
			current_user_has_key and fgettext("Active") or fgettext("Not generated")),
		"]",
	})

	-- Public key display
	table.insert_all(formspec, {
		"container[0.375,", tostring(pubkey_y), "]",
		"label[0,0;", fgettext("Public Key for '%s' (Ed25519)", current_username), "]",
		"box[0,0.4;11.25,0.6;#222222]",
		"label[0.2,0.65;", core.formspec_escape(pubkey_display), "]",
		"container_end[]",
	})

	-- All keypairs table
	if #keypairs > 0 then
		table.insert_all(formspec, {
			"container[0.375,", tostring(keypairs_y), "]",
			"label[0,0;", fgettext("All Keypairs"), "]",
			"tablecolumns[text,align=left,width=3;text,align=left,width=7]",
			"table[0,0.4;11.25,2.4;keypair_table;",
				"Username,Public Key,",
				keypair_table_data, "]",
			"button[0,3.0;5.5,0.8;btn_delete_keypair;", fgettext("Delete Selected Keypair"), "]",
			"tooltip[btn_delete_keypair;", fgettext("Delete the keypair for the selected username"), "]",
			"container_end[]",
		})
	end

	-- Server list section
	if #server_list > 0 then
		table.insert_all(formspec, {
			"container[0.375,", tostring(servers_y), "]",
			"label[0,0;", fgettext("Registered Servers"), "]",
			"tablecolumns[text,align=left,width=3.5;text,align=left,width=2;text,align=left,width=2.5;text,align=left,width=2.5]",
			"table[0,0.4;11.25,4.5;keypair_server_table;",
				"Server,Username,Created,Last Used,",
				server_table_data, "]",
			"button[0,5.2;5.5,0.8;btn_forget_server;", fgettext("Forget Selected Server"), "]",
			"tooltip[btn_forget_server;", fgettext("Remove the selected server registration"), "]",
			"container_end[]",
		})
	else
		table.insert_all(formspec, {
			"container[0.375,", tostring(servers_y), "]",
			"label[0,0;", fgettext("No registered servers yet. Connect to a server to register."), "]",
			"container_end[]",
		})
	end

	-- Separator line
	table.insert_all(formspec, {
		"box[0.375,", tostring(regenerate_y - 0.4), ";11.25,0.01;#444444]",
	})

	-- Regenerate section
	table.insert_all(formspec, {
		"container[0.375,", tostring(regenerate_y), "]",
		"label[0,0;", fgettext("Regenerate Keypair for '%s'", current_username), "]",
		"button[0,0.3;5.5,0.8;btn_regenerate_keypair;", fgettext("Regenerate Keypair"), "]",
		"tooltip[btn_regenerate_keypair;",
			fgettext("Generate a new Ed25519 keypair for '%s', replacing the current one. "
				.. "This will invalidate all existing server registrations for that username!", current_username),
		"]",

		-- Close button (right-aligned)
		"button[7.75,0.3;3.5,0.8;btn_close;", fgettext("Close"), "]",
		"container_end[]",
	})

	-- Warning message for regeneration (appears below regenerate button)
	if dialogdata.show_regenerate_warning then
		table.insert_all(formspec, {
			"box[0.375,", tostring(regenerate_y + 1.4), ";11.25,1.2;darkred]",
			"label[0.625,", tostring(regenerate_y + 1.7), ";",
			core.formspec_escape(fgettext("WARNING: Regenerating the keypair for '%s' will invalidate",
				current_username)),
			"]",
			"label[0.625,", tostring(regenerate_y + 2.1), ";",
			core.formspec_escape(fgettext("ALL server registrations for that username! You will need to re-register.")),
			"]",
		})
	end

	-- Success message after regeneration
	if dialogdata.show_regenerate_success then
		table.insert_all(formspec, {
			"box[0.375,", tostring(regenerate_y + 1.4), ";11.25,0.6;darkgreen]",
			"label[0.625,", tostring(regenerate_y + 1.7), ";",
			core.formspec_escape(fgettext("New keypair generated successfully for '%s'!", current_username)),
			"]",
		})
	end

	return table.concat(formspec, "")
end

--------------------------------------------------------------------------------

local function keypair_manager_buttonhandler(this, fields)
	-- Close button
	if fields.btn_close or fields.quit then
		this:delete()
		return true
	end

	-- Delete selected keypair
	if fields.btn_delete_keypair then
		local selected = core.get_table_index("keypair_table")
		if selected and selected > 0 then
			local keypairs = core.keypair_list_keypairs and core.keypair_list_keypairs() or {}
			if keypairs[selected] then
				local username = keypairs[selected].username
				if core.keypair_delete_keypair then
					core.keypair_delete_keypair(username)
				end
				this.data.show_regenerate_warning = nil
				this.data.show_regenerate_success = nil
			end
		end
		return true
	end

	-- Forget selected server
	if fields.btn_forget_server then
		local selected = core.get_table_index("keypair_server_table")
		if selected and selected > 0 then
			local server_list = core.keypair_get_server_list and core.keypair_get_server_list() or {}
			if server_list[selected] then
				local server_addr = server_list[selected].server
				if core.keypair_forget_server then
					core.keypair_forget_server(server_addr)
				end
				this.data.show_regenerate_warning = nil
				this.data.show_regenerate_success = nil
			end
		end
		return true
	end

	-- Regenerate keypair for current user
	if fields.btn_regenerate_keypair then
		local current_username = core.settings:get("name") or ""
		-- First click: show warning
		if not this.data.show_regenerate_warning then
			this.data.show_regenerate_warning = true
			this.data.show_regenerate_success = nil
			return true
		end
		-- Second click (confirm): regenerate
		if core.keypair_regenerate then
			local success = core.keypair_regenerate(current_username)
			this.data.show_regenerate_warning = nil
			this.data.show_regenerate_success = success
		end
		return true
	end

	return false
end

--------------------------------------------------------------------------------

function create_keypair_manager_dialog()
	local retval = dialog_create("dlg_keypair_manager",
		keypair_manager_formspec,
		keypair_manager_buttonhandler,
		nil)
	return retval
end
