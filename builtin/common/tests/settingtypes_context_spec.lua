-- Test for settingtypes.txt context annotations
-- Verifies that all encryption-related settings have proper context annotations
-- and that the parser correctly handles both explicit and inherited contexts.

_G.core = {
	log = function(level, msg) 
		if level == "error" then
			error("Settingtypes parse error: " .. msg)
		end
	end,
	get_builtin_path = function()
		return "builtin/"
	end,
}

-- Minimal string.trim implementation
local function trim(s)
	return s:match("^%s*(.-)%s*$")
end

-- Load the settingtypes module
dofile("builtin/common/settings/settingtypes.lua")

describe("settingtypes context annotations", function()
	
	local function parse_test_content(content, force_context)
		local settings = {}
		settings.current_comment = {}
		settings.current_hide_level = nil
		
		-- Simulate parse_single_file but with a string
		for line in content:gmatch("[^\r\n]+") do
			local error_msg = settingtypes._parse_setting_line(settings, line, true, 0, true, force_context)
			if error_msg then
				error("Parse error: " .. error_msg .. " on line: " .. line)
			end
		end
		
		settings.current_comment = nil
		settings.current_hide_level = nil
		return settings
	end

	it("accepts explicit [client] context on boolean settings", function()
		local settings = parse_test_content([[
[Security Info] [client]

secure_connection (Secure connection) [client] bool true
show_security_overlay (Show security overlay) [client] bool true
]])
		-- Find the settings
		local found_secure = false
		local found_overlay = false
		for _, s in ipairs(settings) do
			if s.name == "secure_connection" then
				found_secure = true
				assert.equals("client", s.context)
			end
			if s.name == "show_security_overlay" then
				found_overlay = true
				assert.equals("client", s.context)
			end
		end
		assert.is_true(found_secure)
		assert.is_true(found_overlay)
	end)

	it("inherits context from category when no explicit context", function()
		local settings = parse_test_content([[
[Security Info] [client]

secure_connection (Secure connection) bool true
]])
		local found = false
		for _, s in ipairs(settings) do
			if s.name == "secure_connection" then
				found = true
				assert.equals("client", s.context)
			end
		end
		assert.is_true(found)
	end)

	it("fails without context annotation when no category context", function()
		-- Settings outside a category context must have explicit context
		local settings = {}
		settings.current_comment = {}
		settings.current_hide_level = nil
		
		local line = "secure_connection (Secure connection) bool true"
		local error_msg = settingtypes._parse_setting_line(settings, line, true, 0, true, nil)
		assert.equals("Missing context annotation", error_msg)
	end)

	it("all 15 encryption settings parse without error with explicit [client]", function()
		local content = [[
[Security Info] [client]

secure_connection (Secure connection) [client] bool true
show_security_overlay (Show security overlay) [client] bool true
show_connection_info (Show connection info in debug) [client] bool false
show_enc_session_overlay (Show encryption session overlay) [client] bool false
show_enc_packets_overlay (Show encryption packets overlay) [client] bool false
show_enc_cipher_overlay (Show encryption cipher overlay) [client] bool false
show_enc_score_overlay (Show encryption score overlay) [client] bool false
show_enc_authfail_overlay (Show encryption auth failure overlay) [client] bool false
show_enc_nonce_overlay (Show encryption nonce overlay) [client] bool false
show_enc_latency_overlay (Show encryption latency overlay) [client] bool false
show_enc_timeline_overlay (Show encryption timeline overlay) [client] bool false
show_enc_pfs_overlay (Show encryption PFS overlay) [client] bool false
show_enc_trust_overlay (Show encryption trust overlay) [client] bool false
show_enc_health_overlay (Show encryption health overlay) [client] bool false
show_enc_bandwidth_overlay (Show encryption bandwidth overlay) [client] bool false
]]
		local settings = parse_test_content(content)
		
		local enc_settings = {}
		for _, s in ipairs(settings) do
			if s.type == "bool" and s.context then
				enc_settings[s.name] = s
			end
		end
		
		-- All 15 settings should be parsed with context "client"
		local expected = {
			"secure_connection", "show_security_overlay", "show_connection_info",
			"show_enc_session_overlay", "show_enc_packets_overlay", "show_enc_cipher_overlay",
			"show_enc_score_overlay", "show_enc_authfail_overlay", "show_enc_nonce_overlay",
			"show_enc_latency_overlay", "show_enc_timeline_overlay", "show_enc_pfs_overlay",
			"show_enc_trust_overlay", "show_enc_health_overlay", "show_enc_bandwidth_overlay",
		}
		
		for _, name in ipairs(expected) do
			assert.is_not_nil(enc_settings[name], "Setting " .. name .. " not found")
			assert.equals("client", enc_settings[name].context, 
				"Setting " .. name .. " has wrong context")
		end
	end)
end)
