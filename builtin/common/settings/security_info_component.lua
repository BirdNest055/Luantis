-- Luanti
-- Copyright (C) 2026 Luanti contributors
-- SPDX-License-Identifier: LGPL-2.1-or-later

-- Custom component that displays read-only connection security information
-- in the Settings → Security Info tab. This shows technical details
-- about the security of the connection so users can verify the security
-- is real and understand what protections are active.
--
-- v8: Enhanced with security score, session data, server fingerprint,
--     TLS version, and a visual status banner at the top.

local security_info = {
        id = "security_info",
        full_width = true,

        get_formspec = function(self, avail_w)
                -- Read security info from runtime settings set by the C++ client
                -- These are set when the client establishes a connection and are
                -- only meaningful when connected to a server.
                local state = core.settings:get("security_info_state") or "Not Connected"
                local encryption = core.settings:get("security_info_encryption") or "N/A"
                local key_exchange = core.settings:get("security_info_key_exchange") or "N/A"
                local authentication = core.settings:get("security_info_authentication") or "N/A"
                local cipher_suite = core.settings:get("security_info_cipher_suite") or "N/A"
                local cert_status = core.settings:get("security_info_cert_status") or "N/A"
                local forward_secrecy = core.settings:get("security_info_forward_secrecy") or "N/A"
                local replay_protection = core.settings:get("security_info_replay_protection") or "N/A"
                local protocol_version = core.settings:get("security_info_protocol_version") or "N/A"
                local server_address = core.settings:get("security_info_server_address") or "N/A"
                local server_port = core.settings:get("security_info_server_port") or "N/A"
                -- v8: New fields
                local session_id = core.settings:get("security_info_session_id") or "N/A"
                local connected_since = core.settings:get("security_info_connected_since") or "N/A"
                local server_fingerprint = core.settings:get("security_info_server_fingerprint") or "N/A"
                local tls_version = core.settings:get("security_info_tls_version") or "N/A"
                local security_score = core.settings:get("security_info_security_score") or "0/100 (Insecure)"

                -- Determine colors based on connection state
                local state_color, bg_color, icon
                if state == "Encrypted" then
                        state_color = "#0f0"
                        bg_color = "#0408"
                        icon = "[+]"
                elseif state == "Local" then
                        state_color = "#0af"
                        bg_color = "#0448"
                        icon = "[~]"
                elseif state == "Insecure" then
                        state_color = "#f00"
                        bg_color = "#4008"
                        icon = "[!]"
                else
                        state_color = "#ff0"
                        bg_color = "#4408"
                        icon = "[?]"
                end

                -- Parse security score for coloring
                local score_num = tonumber(security_score:match("^(%d+)/")) or 0
                local score_color
                if score_num >= 80 then
                        score_color = "#0f0"
                elseif score_num >= 60 then
                        score_color = "#af0"
                elseif score_num >= 30 then
                        score_color = "#f80"
                else
                        score_color = "#f00"
                end

                local fs = {}
                local y = 0

                -- ============================================================
                -- Section 1: Visual Status Banner
                -- ============================================================
                fs[#fs + 1] = ("box[0,%f;%f,1.4;%s]"):format(y, avail_w, bg_color)
                fs[#fs + 1] = ("label[0.3,%f;%s]"):format(y + 0.2,
                        core.colorize(state_color, core.formspec_escape(icon .. " " .. state:upper() .. " CONNECTION")))
                fs[#fs + 1] = ("label[0.3,%f;%s]"):format(y + 0.65,
                        core.colorize(score_color, core.formspec_escape(
                                fgettext("Security Score:") .. " " .. security_score)))

                -- Status description line
                if state == "Encrypted" then
                        fs[#fs + 1] = ("label[0.3,%f;%s]"):format(y + 1.05,
                                core.colorize("#0c0", core.formspec_escape(
                                        fgettext("Game traffic is protected from eavesdropping."))))
                elseif state == "Local" then
                        fs[#fs + 1] = ("label[0.3,%f;%s]"):format(y + 1.05,
                                core.colorize("#0af", core.formspec_escape(
                                        fgettext("Local game — traffic does not leave this computer."))))
                elseif state == "Insecure" then
                        fs[#fs + 1] = ("label[0.3,%f;%s]"):format(y + 1.05,
                                core.colorize("#fa0", core.formspec_escape(
                                        fgettext("All game traffic is sent as plaintext!"))))
                else
                        fs[#fs + 1] = ("label[0.3,%f;%s]"):format(y + 1.05,
                                core.colorize("#cc0", core.formspec_escape(
                                        fgettext("Connect to a server to see security details."))))
                end
                y = y + 1.6

                -- ============================================================
                -- Section 2: Encryption & Authentication Details
                -- ============================================================
                fs[#fs + 1] = ("label[0,%f;%s]box[0,%f;%f,0.05;#ccc6]"):format(
                        y, core.formspec_escape(fgettext("Encryption & Authentication")),
                        y + 0.3, avail_w)
                y = y + 0.6

                local row_height = 0.4
                local label_x = 0
                local value_x = 3.8

                local function add_row(label, value, value_color)
                        value_color = value_color or "#fff"
                        fs[#fs + 1] = ("label[%f,%f;%s]"):format(
                                label_x, y, core.formspec_escape(label))
                        fs[#fs + 1] = ("label[%f,%f;%s]"):format(
                                value_x, y, core.colorize(value_color, core.formspec_escape(value)))
                        y = y + row_height
                end

                -- Encryption details
                add_row(fgettext("Connection State:"), state, state_color)
                add_row(fgettext("Encryption:"), encryption,
                        encryption ~= "None" and encryption ~= "N/A" and "#0f0" or "#f00")
                add_row(fgettext("Cipher Suite:"), cipher_suite,
                        cipher_suite ~= "None" and cipher_suite ~= "N/A" and "#0f0" or "#f00")
                add_row(fgettext("Key Exchange:"), key_exchange,
                        key_exchange ~= "None" and key_exchange ~= "N/A" and "#0f0" or nil)
                add_row(fgettext("Authentication:"), authentication,
                        authentication ~= "None" and authentication ~= "N/A" and "#0f0" or nil)
                add_row(fgettext("TLS Version:"), tls_version,
                        tls_version ~= "None" and tls_version ~= "N/A" and "#0f0" or "#f00")

                -- ============================================================
                -- Section 3: Certificate & Trust
                -- ============================================================
                y = y + 0.15
                fs[#fs + 1] = ("label[0,%f;%s]box[0,%f;%f,0.05;#ccc6]"):format(
                        y, core.formspec_escape(fgettext("Certificate & Trust")),
                        y + 0.3, avail_w)
                y = y + 0.6

                add_row(fgettext("Certificate:"), cert_status,
                        cert_status == "Verified" and "#0f0" or
                        (cert_status == "Self-Signed" and "#ff0" or "#f00"))
                add_row(fgettext("Server Fingerprint:"), server_fingerprint,
                        server_fingerprint ~= "N/A" and "#0af" or "#888")

                -- ============================================================
                -- Section 4: Security Properties
                -- ============================================================
                y = y + 0.15
                fs[#fs + 1] = ("label[0,%f;%s]box[0,%f;%f,0.05;#ccc6]"):format(
                        y, core.formspec_escape(fgettext("Security Properties")),
                        y + 0.3, avail_w)
                y = y + 0.6

                add_row(fgettext("Forward Secrecy:"), forward_secrecy,
                        forward_secrecy == "Yes" and "#0f0" or "#f00")
                add_row(fgettext("Replay Protection:"), replay_protection,
                        replay_protection == "Yes" and "#0f0" or "#f00")

                -- Explanation of key properties (compact tooltips-style)
                y = y + 0.1
                if forward_secrecy == "Yes" then
                        fs[#fs + 1] = ("box[0,%f;%f,0.5;#0404]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.1,
                                core.colorize("#0c0", core.formspec_escape(
                                        fgettext("Forward secrecy: Past sessions stay safe even if long-term keys are compromised."))))
                        y = y + 0.55
                elseif state == "Encrypted" then
                        fs[#fs + 1] = ("box[0,%f;%f,0.5;#4404]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.1,
                                core.colorize("#fa0", core.formspec_escape(
                                        fgettext("No forward secrecy: Compromised keys could reveal past session data."))))
                        y = y + 0.55
                end
                if replay_protection == "Yes" then
                        fs[#fs + 1] = ("box[0,%f;%f,0.5;#0404]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.1,
                                core.colorize("#0c0", core.formspec_escape(
                                        fgettext("Replay protection: Duplicate packets are detected and rejected."))))
                        y = y + 0.55
                end

                -- ============================================================
                -- Section 5: Session & Connection Details
                -- ============================================================
                y = y + 0.15
                fs[#fs + 1] = ("label[0,%f;%s]box[0,%f;%f,0.05;#ccc6]"):format(
                        y, core.formspec_escape(fgettext("Session & Connection")),
                        y + 0.3, avail_w)
                y = y + 0.6

                add_row(fgettext("Session ID:"), session_id,
                        session_id ~= "N/A" and "#0af" or "#888")

                -- Format the connected_since timestamp nicely
                local connected_since_display = connected_since
                if connected_since ~= "N/A" and tonumber(connected_since) then
                        local ts = tonumber(connected_since)
                        if ts > 0 then
                                connected_since_display = os.date("!%Y-%m-%d %H:%M:%S UTC", ts)
                        end
                end
                add_row(fgettext("Connected Since:"), connected_since_display,
                        connected_since ~= "N/A" and "#fff" or "#888")

                add_row(fgettext("Protocol Version:"), protocol_version)
                add_row(fgettext("Server Address:"), server_address)
                add_row(fgettext("Server Port:"), server_port)

                -- ============================================================
                -- Section 6: Context-sensitive warning/info box
                -- ============================================================
                y = y + 0.2
                if state == "Local" then
                        fs[#fs + 1] = ("box[0,%f;%f,1.2;#0448]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.15,
                                core.colorize("#0af", core.formspec_escape(
                                        fgettext("Singleplayer: This is a local game session."))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.55,
                                core.colorize("#09c", core.formspec_escape(
                                        fgettext("All game data stays on your computer. Network encryption"))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.85,
                                core.colorize("#09c", core.formspec_escape(
                                        fgettext("is not needed because no data is sent over the network."))))
                        y = y + 1.3
                elseif state == "Insecure" then
                        fs[#fs + 1] = ("box[0,%f;%f,1.5;#4008]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.15,
                                core.colorize("#f80", core.formspec_escape(
                                        fgettext("WARNING: Your connection is not encrypted!"))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.55,
                                core.colorize("#fa0", core.formspec_escape(
                                        fgettext("All game traffic is sent as plaintext and could be"))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.85,
                                core.colorize("#fa0", core.formspec_escape(
                                        fgettext("intercepted by third parties on your network."))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 1.2,
                                core.colorize("#fa0", core.formspec_escape(
                                        fgettext("Enable 'Secure connection' in server settings to fix this."))))
                        y = y + 1.6
                elseif state == "Encrypted" then
                        fs[#fs + 1] = ("box[0,%f;%f,0.8;#0408]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.15,
                                core.colorize("#0f0", core.formspec_escape(
                                        fgettext("Your connection is secured with encryption."))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.5,
                                core.colorize("#0c0", core.formspec_escape(
                                        fgettext("Game traffic is protected from eavesdropping."))))
                        y = y + 0.9
                else
                        fs[#fs + 1] = ("box[0,%f;%f,0.8;#4408]"):format(y, avail_w)
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.15,
                                core.colorize("#ff0", core.formspec_escape(
                                        fgettext("Not currently connected to a server."))))
                        fs[#fs + 1] = ("label[0.2,%f;%s]"):format(y + 0.5,
                                core.colorize("#cc0", core.formspec_escape(
                                        fgettext("Security details will appear when you join a server."))))
                        y = y + 0.9
                end

                return table.concat(fs, ""), y
        end,

        on_submit = function(self, fields)
                -- Read-only component, no submissions
                return false
        end,
}

return security_info
