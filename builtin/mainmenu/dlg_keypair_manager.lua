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
--- This is the correct pattern for Luanti formspec tables:
--- commas between cells must NOT be escaped.
local function escape_row(cells)
        local escaped = {}
        for i, cell in ipairs(cells) do
                escaped[i] = core.formspec_escape(tostring(cell))
        end
        return table.concat(escaped, ",")
end

--- Given the server_list, compute the earliest created_at and
--- latest last_used_at for each username across all servers.
local function aggregate_keypair_dates(keypairs, server_list)
        local dates = {}
        for _, kp in ipairs(keypairs) do
                dates[kp.username] = { created = "", last_used = "" }
        end
        for _, entry in ipairs(server_list) do
                local u = entry.username
                if dates[u] then
                        -- Keep earliest created_at
                        if dates[u].created == "" or (entry.created_at ~= "" and entry.created_at < dates[u].created) then
                                dates[u].created = entry.created_at
                        end
                        -- Keep latest last_used_at
                        if dates[u].last_used == "" or (entry.last_used_at ~= "" and entry.last_used_at > dates[u].last_used) then
                                dates[u].last_used = entry.last_used_at
                        end
                end
        end
        return dates
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
        if #pubkey_display > 32 then
                pubkey_display = current_pubkey_b64:sub(1, 16) .. "..." .. current_pubkey_b64:sub(#current_pubkey_b64 - 11)
        end

        -- Aggregate dates from server entries for each keypair username
        local kp_dates = aggregate_keypair_dates(keypairs, server_list)

        -- Build keypairs table rows (4 columns: Username, Public Key, First Seen, Last Used)
        -- CRITICAL: escape each cell individually — do NOT wrap the whole row
        -- in formspec_escape or the commas between cells get escaped too,
        -- collapsing all columns into one cell.
        local keypair_table_rows = {}
        for i, kp in ipairs(keypairs) do
                local pk_display = kp.public_key_base64 or ""
                if #pk_display > 24 then
                        pk_display = pk_display:sub(1, 10) .. "..." .. pk_display:sub(#pk_display - 9)
                end
                local d = kp_dates[kp.username] or {}
                keypair_table_rows[#keypair_table_rows + 1] = escape_row({
                        kp.username,
                        pk_display,
                        format_date(d.created),
                        format_date(d.last_used),
                })
        end
        local keypair_table_data = table.concat(keypair_table_rows, ",")

        -- Build server list rows (4 columns: Server, Username, Created, Last Used)
        local server_table_rows = {}
        for i, entry in ipairs(server_list) do
                server_table_rows[#server_table_rows + 1] = escape_row({
                        entry.server,
                        entry.username,
                        format_date(entry.created_at),
                        format_date(entry.last_used_at),
                })
        end
        local server_table_data = table.concat(server_table_rows, ",")

        ------------------------------------------------------------------------
        -- Dynamic layout: compute Y positions top-to-bottom
        ------------------------------------------------------------------------
        local y = 0.5           -- starting Y

        -- Title
        local title_y = y
        y = y + 0.7

        -- Keypair auth disabled warning
        local disabled_y = 0
        if not keypair_auth_enabled then
                disabled_y = y
                y = y + 0.9
        end

        -- Current user status bar
        local status_y = y
        y = y + 0.8

        -- No keypair warning (if needed)
        local no_keypair_y = 0
        if not current_user_has_key and keypair_auth_enabled and current_username ~= "" then
                no_keypair_y = y
                y = y + 0.9
        end

        -- Public key display
        local pubkey_y = y
        y = y + 1.6

        -- All Keypairs table
        local keypair_table_y = 0
        local keypair_table_h = 0
        if #keypairs > 0 then
                keypair_table_y = y
                -- Height: 1.2 base + 0.6 per keypair, capped at 3.5
                keypair_table_h = math.min(1.2 + #keypairs * 0.6, 3.5)
                y = y + keypair_table_h + 1.4   -- table + button + margin
        end

        -- Registered Servers table
        local server_table_y = 0
        local server_table_h = 0
        if #server_list > 0 then
                server_table_y = y
                server_table_h = math.min(1.2 + #server_list * 0.6, 4.5)
                y = y + server_table_h + 1.4   -- table + button + margin
        end

        -- Divider + Regenerate section
        local divider_y = y
        y = y + 0.5
        local regenerate_y = y
        y = y + 1.3

        local total_height = y + 0.3

        ------------------------------------------------------------------------
        -- Warning / success messages for regeneration
        ------------------------------------------------------------------------
        local warning_text = ""
        if dialogdata.show_regenerate_warning then
                warning_text = table.concat({
                        "box[0.375,", tostring(regenerate_y + 1.0), ";11.25,1.2;darkred]",
                        "label[0.625,", tostring(regenerate_y + 1.3), ";",
                        core.formspec_escape(fgettext("WARNING: Regenerating the keypair for '%s' will invalidate", current_username)),
                        "]",
                        "label[0.625,", tostring(regenerate_y + 1.7), ";",
                        core.formspec_escape(fgettext("ALL server registrations for that username! You will need to re-register.")),
                        "]",
                })
        end

        local success_text = ""
        if dialogdata.show_regenerate_success then
                success_text = table.concat({
                        "box[0.375,", tostring(regenerate_y + 1.0), ";11.25,0.6;darkgreen]",
                        "label[0.625,", tostring(regenerate_y + 1.3), ";",
                        core.formspec_escape(fgettext("New keypair generated successfully for '%s'!", current_username)),
                        "]",
                })
        end

        ------------------------------------------------------------------------
        -- Build formspec
        ------------------------------------------------------------------------
        local formspec = {
                "formspec_version[4]",
                "size[12,", tostring(total_height), "]",
                TOUCH_GUI and "padding[0.01,0.01]" or "",

                -- Title
                "label[0.375,", tostring(title_y + 0.3), ";",
                        fgettext("Keypair Manager"), "]",
        }

        -- Keypair auth disabled warning
        if not keypair_auth_enabled then
                table.insert_all(formspec, {
                        "box[0.375,", tostring(disabled_y), ";11.25,0.6;#884400]",
                        "label[0.625,", tostring(disabled_y + 0.3), ";",
                        core.formspec_escape(fgettext("Keypair authentication is disabled in settings.")),
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

        -- No keypair warning
        if no_keypair_y > 0 then
                table.insert_all(formspec, {
                        "box[0.375,", tostring(no_keypair_y), ";11.25,0.6;#884400]",
                        "label[0.625,", tostring(no_keypair_y + 0.3), ";",
                        core.formspec_escape(fgettext("No keypair for '%s'. One will be generated when you connect.", current_username)),
                        "]",
                })
        end

        -- Public key display — cleaner UX label
        if current_user_has_key then
                table.insert_all(formspec, {
                        "container[0.375,", tostring(pubkey_y), "]",
                        "label[0,0;", fgettext("Your Public Key"), "]",
                        "box[0,0.4;11.25,0.6;#222222]",
                        "label[0.2,0.65;", core.formspec_escape(pubkey_display), "]",
                        "container_end[]",
                })
        else
                table.insert_all(formspec, {
                        "container[0.375,", tostring(pubkey_y), "]",
                        "label[0,0;", fgettext("Your Public Key"), "]",
                        "box[0,0.4;11.25,0.6;#222222]",
                        "label[0.2,0.65;", core.formspec_escape(fgettext("No key generated yet")), "]",
                        "container_end[]",
                })
        end

        -- All Keypairs table (4 columns: Username, Public Key, First Seen, Last Used)
        if #keypairs > 0 then
                table.insert_all(formspec, {
                        "container[0.375,", tostring(keypair_table_y), "]",
                        "label[0,0.3;", fgettext("All Keypairs"), "]",
                        "tablecolumns[",
                                "text,align=left,width=2.5;",     -- Username
                                "text,align=left,width=4;",        -- Public Key
                                "text,align=center,width=2.25;",   -- First Seen
                                "text,align=center,width=2.25",    -- Last Used
                        "]",
                        "table[0,0.6;11.25,", tostring(keypair_table_h), ";keypair_table;",
                                "Username,Public Key,First Seen,Last Used,",
                                keypair_table_data, "]",
                        "button[0,", tostring(keypair_table_h + 0.8), ";5.5,0.8;btn_delete_keypair;",
                                fgettext("Delete Selected Keypair"), "]",
                        "tooltip[btn_delete_keypair;",
                                fgettext("Delete the keypair for the selected username"), "]",
                        "container_end[]",
                })
        end

        -- Registered Servers table
        if #server_list > 0 then
                table.insert_all(formspec, {
                        "container[0.375,", tostring(server_table_y), "]",
                        "label[0,0.3;", fgettext("Registered Servers"), "]",
                        "tablecolumns[",
                                "text,align=left,width=3.5;",      -- Server
                                "text,align=left,width=2;",        -- Username
                                "text,align=center,width=2.5;",    -- Created
                                "text,align=center,width=2.5",     -- Last Used
                        "]",
                        "table[0,0.6;11.25,", tostring(server_table_h), ";keypair_server_table;",
                                "Server,Username,Created,Last Used,",
                                server_table_data, "]",
                        "button[0,", tostring(server_table_h + 0.8), ";3.5,0.8;btn_forget_server;",
                                fgettext("Forget Selected Server"), "]",
                        "tooltip[btn_forget_server;",
                                fgettext("Remove the selected server registration"), "]",
                        "button[3.7,", tostring(server_table_h + 0.8), ";4,0.8;btn_show_history;",
                                fgettext("View History"), "]",
                        "tooltip[btn_show_history;",
                                fgettext("Open the full keypair connection history window"), "]",
                        "container_end[]",
                })
        else
                table.insert_all(formspec, {
                        "container[0.375,", tostring(server_table_y > 0 and server_table_y or keypair_table_y + keypair_table_h + 1), "]",
                        "label[0,0.3;",
                        core.formspec_escape(fgettext("No registered servers yet. Connect to a server to register your keypair.")),
                        "]",
                        "container_end[]",
                })
        end

        -- Divider line
        table.insert_all(formspec, {
                "box[0.375,", tostring(divider_y), ";11.25,0.01;#444444]",
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

                -- Confirm/Cancel buttons for regeneration (hidden by default)
                warning_text,
                success_text,

                -- Close button
                "button[7.75,0.3;3.5,0.8;btn_close;", fgettext("Close"), "]",
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

        -- v9.41: Open full history dialog
        if fields.btn_show_history then
                local dlg = create_keypair_history_dialog()
                dlg:set_parent(this)
                this:hide()
                dlg:show()
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
