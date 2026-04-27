-- Luanti-Secure
-- Copyright (C) 2026 Luanti-Secure contributors
-- SPDX-License-Identifier: LGPL-2.1-or-later
--
-- v9.31: GUI-based keypair management dialog.
-- Allows users to view their Ed25519 public key, regenerate their keypair,
-- and manage remembered server usernames.

--------------------------------------------------------------------------------

local function keypair_manager_formspec(dialogdata)
	local TOUCH_GUI = core.settings:get_bool("touch_gui")

	-- Gather keypair info
	local has_keypair = core.keypair_keypair_exists and core.keypair_keypair_exists() or false
	local pubkey_b64 = ""
	if has_keypair then
		pubkey_b64 = core.keypair_get_public_key_base64 and core.keypair_get_public_key_base64() or ""
	end
	local server_list = core.keypair_get_server_list and core.keypair_get_server_list() or {}
	local keypair_auth_enabled = core.settings:get_bool("keypair_auth")

	-- Truncate public key for display (show first/last 8 chars with ellipsis)
	local pubkey_display = pubkey_b64
	if #pubkey_display > 24 then
		pubkey_display = pubkey_b64:sub(1, 12) .. " ... " .. pubkey_b64:sub(#pubkey_b64 - 11)
	end

	-- Build server list rows for the table
	local server_table_rows = {}
	for i, entry in ipairs(server_list) do
		server_table_rows[#server_table_rows + 1] = core.formspec_escape(entry.server .. "," .. entry.username)
	end
	local server_table_data = table.concat(server_table_rows, ",")

	-- Determine if we need to show the "regenerate" section
	local regenerate_y = 5.2
	if #server_list > 0 then
		regenerate_y = 8.8
	end

	-- Warning message for regenerate
	local warning_text = ""
	if dialogdata.show_regenerate_warning then
		warning_text = table.concat({
			"box[0.375,", tostring(regenerate_y), ";9.25,1.2;darkred]",
			"label[0.625,", tostring(regenerate_y + 0.3), ";",
			core.formspec_escape(fgettext("WARNING: Regenerating your keypair will invalidate ALL your")),
			"]",
			"label[0.625,", tostring(regenerate_y + 0.7), ";",
			core.formspec_escape(fgettext("server registrations! You will need to re-register on each server.")),
			"]",
		})
	end

	-- Success message after regeneration
	local success_text = ""
	if dialogdata.show_regenerate_success then
		success_text = table.concat({
			"box[0.375,", tostring(regenerate_y), ";9.25,0.6;darkgreen]",
			"label[0.625,", tostring(regenerate_y + 0.3), ";",
			core.formspec_escape(fgettext("New keypair generated successfully!")),
			"]",
		})
	end

	-- No keypair warning
	local no_keypair_text = ""
	if not has_keypair and keypair_auth_enabled then
		no_keypair_text = table.concat({
			"box[0.375,2.2;9.25,0.6;#884400]",
			"label[0.625,2.5;",
			core.formspec_escape(fgettext("No keypair found. One will be generated when you first connect.")),
			"]",
		})
	end

	-- Keypair auth disabled warning
	local disabled_text = ""
	if not keypair_auth_enabled then
		disabled_text = table.concat({
			"box[0.375,0.8;9.25,0.6;#884400]",
			"label[0.625,1.1;",
			core.formspec_escape(fgettext("Keypair authentication is disabled in settings.")),
			"]",
		})
	end

	-- Base height
	local total_height = 7.2
	if #server_list > 0 then
		total_height = 10.8
	end
	if dialogdata.show_regenerate_warning then
		total_height = total_height + 1.4
	end
	if dialogdata.show_regenerate_success then
		total_height = total_height + 0.8
	end

	local formspec = {
		"formspec_version[4]",
		"size[10,", tostring(total_height), "]",
		TOUCH_GUI and "padding[0.01,0.01]" or "",

		-- Title
		"label[0.375,0.6;", fgettext("Keypair Manager"), "]",

		-- Disabled warning
		disabled_text,

		-- Status section
		"box[0.375,1.5;9.25,0.6;#333333]",
		"label[0.625,1.8;",
		fgettext("Keypair status: $1", has_keypair and fgettext("Active") or fgettext("Not generated")),
		"]",

		-- No keypair warning
		no_keypair_text,

		-- Public key display
		"container[0.375,3.0]",
		"label[0,0;", fgettext("Public Key (Ed25519)"), "]",
		"box[0,0.4;9.25,0.6;#222222]",
		"label[0.2,0.65;", core.formspec_escape(pubkey_display), "]",
		"container_end[]",
	}

	-- Server list section (only if there are remembered servers)
	if #server_list > 0 then
		table.insert_all(formspec, {
			"container[0.375,4.2]",
			"label[0,0;", fgettext("Remembered Servers"), "]",
			"tablecolumns[text,align=left;text,align=left]",
			"table[0,0.4;9.25,3.0;keypair_server_table;",
				"Server,Username,",
				server_table_data, "]",
			"button[0,3.6;4.5,0.8;btn_forget_server;", fgettext("Forget Selected Server"), "]",
			"tooltip[btn_forget_server;", fgettext("Remove the remembered username for the selected server"), "]",
			"container_end[]",
		})
	else
		table.insert_all(formspec, {
			"container[0.375,4.2]",
			"label[0,0;", fgettext("No remembered servers yet."), "]",
			"container_end[]",
		})
	end

	-- Regenerate section
	table.insert_all(formspec, {
		"container[0.375,", tostring(regenerate_y - 0.6), "]",
		"box[0,0;9.25,0.01;#444444]",
		"container_end[]",

		"container[0.375,", tostring(regenerate_y - 0.4), "]",
		"label[0,0;", fgettext("Regenerate Keypair"), "]",
		"button[0,0.3;4.5,0.8;btn_regenerate_keypair;", fgettext("Regenerate Keypair"), "]",
		"tooltip[btn_regenerate_keypair;",
			fgettext("Generate a new Ed25519 keypair, replacing the current one. "
				.. "This will invalidate all existing server registrations!"),
		"]",

		-- Confirm/Cancel buttons for regeneration (hidden by default)
		warning_text,
		success_text,

		-- Close button
		"button[6,0.3;3.25,0.8;btn_close;", fgettext("Close"), "]",
		"container_end[]",
	})

	return table.concat(formspec, "")
end

--------------------------------------------------------------------------------

local function keypair_manager_buttonhandler(this, fields)
	-- Close button
	if fields.btn_close or fields.quit then
		this:delete()
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

	-- Regenerate keypair
	if fields.btn_regenerate_keypair then
		-- First click: show warning
		if not this.data.show_regenerate_warning then
			this.data.show_regenerate_warning = true
			this.data.show_regenerate_success = nil
			return true
		end
		-- Second click (confirm): regenerate
		if core.keypair_regenerate then
			local success = core.keypair_regenerate()
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
