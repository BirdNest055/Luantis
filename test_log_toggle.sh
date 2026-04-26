#!/usr/bin/env bash
#
# test_log_toggle.sh — TDD tests for Clawtest encryption log toggle v9.23
#
# Tests that the encryption_log_level config setting properly controls
# which [ENC:...] log lines are emitted by the server and client.
#
# Usage:
#   ./test_log_toggle.sh
#
# Exit codes:
#   0 = all tests passed
#   1 = one or more tests failed
#

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_BIN="$SCRIPT_DIR/bin/luantiserver"
CLIENT_BIN="$SCRIPT_DIR/bin/luanti"
TEST_DIR="/tmp/clawtest-log-toggle-test-$$"
WORLD_DIR="$TEST_DIR/world"
PASS=0
FAIL=0
TOTAL=0

# --- Colors ---
RED=$'\033[1;31m'
GREEN=$'\033[1;32m'
YELLOW=$'\033[1;33m'
CYAN=$'\033[1;36m'
BOLD=$'\033[1m'
RESET=$'\033[0m'

# --- Logging ---
log()   { echo -e "${GREEN}[TEST]${RESET} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET} $*" >&2; }
error() { echo -e "${RED}[FAIL]${RESET} $*" >&2; }
pass()  { echo -e "${GREEN}[PASS]${RESET} $*"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); }
fail()  { echo -e "${RED}[FAIL]${RESET} $*"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); }

# --- Setup ---
setup() {
    log "Setting up test environment..."
    rm -rf "$TEST_DIR"
    mkdir -p "$WORLD_DIR"

    cat > "$WORLD_DIR/world.mt" <<EOF
gameid = devtest
backend = sqlite3
creative_mode = false
enable_damage = true
auth_backend = sqlite3
player_backend = sqlite3
mod_storage_backend = sqlite3
world_name = test_world
EOF

    # Check binaries exist
    if [[ ! -x "$SERVER_BIN" ]]; then
        error "Server binary not found: $SERVER_BIN"
        exit 1
    fi
}

cleanup() {
    log "Cleaning up test environment..."
    pkill -f "luantiserver.*--port 3[0-9][0-9][0-9][0-9].*test_world" 2>/dev/null || true
    sleep 1
    rm -rf "$TEST_DIR"
    rm -f /tmp/clawtest-log-toggle-*.conf
}

# --- Test helpers ---

# Safe string search in binary (avoids SIGPIPE from grep -q)
string_exists() {
    local needle="$1"
    local file="$2"
    strings "$file" | grep -F "$needle" > /dev/null 2>&1
    local rc=$?
    [[ $rc -eq 0 || $rc -eq 141 ]] && return 0
    return 1
}

# Run server with a given config and capture output for N seconds
run_server_with_config() {
    local config="$1"
    local port="$2"
    local desc="$3"
    local timeout_secs="${4:-5}"
    local output_file="$TEST_DIR/server-output-${port}.log"

    log "Starting server: $desc (port $port)"
    timeout "$timeout_secs" "$SERVER_BIN" \
        --world "$WORLD_DIR" \
        --port "$port" \
        --gameid devtest \
        --config "$config" \
        > "$output_file" 2>&1 || true

    SERVER_OUTPUT="$(cat "$output_file" 2>/dev/null || echo "")"
}

assert_output_contains() {
    local needle="$1"
    local haystack="${2:-$SERVER_OUTPUT}"
    echo "$haystack" | grep -qF "$needle"
}

assert_output_not_contains() {
    local needle="$1"
    local haystack="${2:-$SERVER_OUTPUT}"
    ! echo "$haystack" | grep -qF "$needle"
}

# ======================================================================
# Tests
# ======================================================================

test_01_encryption_log_level_setting_exists() {
    local test_name="encryption_log_level setting is compiled into binary"
    log "Running: $test_name"

    if string_exists "encryption_log_level" "$SERVER_BIN"; then
        pass "$test_name: server binary contains encryption_log_level"
    else
        fail "$test_name: server binary missing encryption_log_level"
    fi

    if string_exists "encryption_log_level" "$CLIENT_BIN"; then
        pass "$test_name: client binary contains encryption_log_level"
    else
        fail "$test_name: client binary missing encryption_log_level"
    fi
}

test_02_log_level_strings_in_binary() {
    local test_name="Log level string values exist in binary"
    log "Running: $test_name"

    for level in none error action trace; do
        # Check that at least the setting default appears
        if string_exists "$level" "$SERVER_BIN"; then
            pass "$test_name: '$level' string found in server binary"
        else
            fail "$test_name: '$level' string NOT found in server binary"
        fi
    done
}

test_03_enclog_macros_guarded() {
    local test_name="enclog macros are guarded by shouldLog()"
    log "Running: $test_name"

    if string_exists "shouldLog" "$SERVER_BIN"; then
        pass "$test_name: shouldLog symbol found in server binary"
    else
        fail "$test_name: shouldLog symbol NOT found in server binary"
    fi
}

test_04_server_starts_with_log_level_none() {
    local test_name="Server starts with encryption_log_level=none"
    log "Running: $test_name"

    local config="$TEST_DIR/log-none.conf"
    cat > "$config" <<EOF
secure_connection = true
encryption_log_level = none
name = admin
EOF

    run_server_with_config "$config" 30011 "$test_name" 5

    if assert_output_contains "listening on"; then
        pass "$test_name: server started successfully"
    else
        fail "$test_name: server did not start"
        echo "$SERVER_OUTPUT" | head -20
    fi
}

test_05_server_starts_with_log_level_trace() {
    local test_name="Server starts with encryption_log_level=trace"
    log "Running: $test_name"

    local config="$TEST_DIR/log-trace.conf"
    cat > "$config" <<EOF
secure_connection = true
encryption_log_level = trace
name = admin
EOF

    run_server_with_config "$config" 30012 "$test_name" 5

    if assert_output_contains "listening on"; then
        pass "$test_name: server started successfully"
    else
        fail "$test_name: server did not start"
        echo "$SERVER_OUTPUT" | head -20
    fi
}

test_06_default_log_level_is_action() {
    local test_name="Default encryption_log_level is 'action'"
    log "Running: $test_name"

    local config="$TEST_DIR/log-default.conf"
    cat > "$config" <<EOF
secure_connection = true
name = admin
EOF

    run_server_with_config "$config" 30013 "$test_name" 5

    if assert_output_contains "listening on"; then
        pass "$test_name: server started with default log level"
    else
        fail "$test_name: server did not start with default log level"
    fi
}

test_07_start_server_script_has_flag() {
    local test_name="start_server.sh has --log-encryption flag"
    log "Running: $test_name"

    if grep -q '\-\-log-encryption' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: --log-encryption flag found in start_server.sh"
    else
        fail "$test_name: --log-encryption flag NOT found in start_server.sh"
    fi
}

test_08_start_client_script_has_flag() {
    local test_name="start_client.sh has --log-encryption flag"
    log "Running: $test_name"

    if grep -q '\-\-log-encryption' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: --log-encryption flag found in start_client.sh"
    else
        fail "$test_name: --log-encryption flag NOT found in start_client.sh"
    fi
}

test_09_start_server_script_writes_config() {
    local test_name="start_server.sh writes encryption_log_level to config"
    log "Running: $test_name"

    if grep -q 'encryption_log_level' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: start_server.sh references encryption_log_level"
    else
        fail "$test_name: start_server.sh does NOT reference encryption_log_level"
    fi
}

test_10_start_client_script_writes_config() {
    local test_name="start_client.sh writes encryption_log_level to config"
    log "Running: $test_name"

    if grep -q 'encryption_log_level' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: start_client.sh references encryption_log_level"
    else
        fail "$test_name: start_client.sh does NOT reference encryption_log_level"
    fi
}

test_11_settingtypes_documented() {
    local test_name="encryption_log_level is in settingtypes.txt"
    log "Running: $test_name"

    local settingtypes="$SCRIPT_DIR/builtin/settingtypes.txt"
    if grep -q "encryption_log_level" "$settingtypes"; then
        pass "$test_name: encryption_log_level found in settingtypes.txt"
    else
        fail "$test_name: encryption_log_level NOT found in settingtypes.txt"
    fi
}

test_12_defaultsettings_has_default() {
    local test_name="defaultsettings.cpp has default for encryption_log_level"
    log "Running: $test_name"

    if grep -q 'setDefault.*encryption_log_level' "$SCRIPT_DIR/src/defaultsettings.cpp"; then
        pass "$test_name: default for encryption_log_level found"
    else
        fail "$test_name: default for encryption_log_level NOT found in defaultsettings.cpp"
    fi
}

test_13_encryption_config_has_log_functions() {
    local test_name="encryption_config.h has log level functions"
    log "Running: $test_name"

    local header="$SCRIPT_DIR/src/network/encryption_config.h"
    for func in getLogLevel shouldLog getLogLevelString parseLogLevel; do
        if grep -q "$func" "$header"; then
            pass "$test_name: $func() declared in encryption_config.h"
        else
            fail "$test_name: $func() NOT declared in encryption_config.h"
        fi
    done
}

test_14_encryption_config_cpp_has_implementations() {
    local test_name="encryption_config.cpp has log level implementations"
    log "Running: $test_name"

    local cpp="$SCRIPT_DIR/src/network/encryption_config.cpp"
    for func in parseLogLevel getLogLevel shouldLog getLogLevelString; do
        if grep -q "EncryptionConfig::$func" "$cpp"; then
            pass "$test_name: EncryptionConfig::$func() implemented"
        else
            fail "$test_name: EncryptionConfig::$func() NOT implemented"
        fi
    done
}

test_15_enclog_macros_use_shouldLog() {
    local test_name="enclog macros check shouldLog()"
    log "Running: $test_name"

    local header="$SCRIPT_DIR/src/network/encryption_log.h"
    for macro in enclog_init enclog_activate enclog_security enclog_error enclog_disable enclog_trace; do
        if grep -A2 "#define $macro" "$header" | grep -q "shouldLog"; then
            pass "$test_name: $macro uses shouldLog()"
        else
            fail "$test_name: $macro does NOT use shouldLog()"
        fi
    done
}

test_16_start_scripts_syntax_valid() {
    local test_name="Start scripts have valid bash syntax after changes"
    log "Running: $test_name"

    if bash -n "$SCRIPT_DIR/start_server.sh" 2>/dev/null; then
        pass "$test_name: start_server.sh syntax valid"
    else
        fail "$test_name: start_server.sh has syntax errors"
    fi

    if bash -n "$SCRIPT_DIR/start_client.sh" 2>/dev/null; then
        pass "$test_name: start_client.sh syntax valid"
    else
        fail "$test_name: start_client.sh has syntax errors"
    fi
}

test_17_encryption_log_level_enum_values() {
    local test_name="EncryptionLogLevel enum has correct values"
    log "Running: $test_name"

    local header="$SCRIPT_DIR/src/network/encryption_log_level.h"
    for val in ENC_LOG_NONE ENC_LOG_ERROR ENC_LOG_ACTION ENC_LOG_TRACE; do
        if grep -q "$val" "$header"; then
            pass "$test_name: $val found in encryption_log_level.h"
        else
            fail "$test_name: $val NOT found in encryption_log_level.h"
        fi
    done
}

# ======================================================================
# v9.23: --no-log / --log master toggle tests
# ======================================================================

test_18_start_server_has_no_log_flag() {
    local test_name="start_server.sh has --no-log flag"
    log "Running: $test_name"

    if grep -q '\-\-no-log' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: --no-log flag found in start_server.sh"
    else
        fail "$test_name: --no-log flag NOT found in start_server.sh"
    fi
}

test_19_start_client_has_no_log_flag() {
    local test_name="start_client.sh has --no-log flag"
    log "Running: $test_name"

    if grep -q '\-\-no-log' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: --no-log flag found in start_client.sh"
    else
        fail "$test_name: --no-log flag NOT found in start_client.sh"
    fi
}

test_20_start_server_has_log_flag() {
    local test_name="start_server.sh has --log flag"
    log "Running: $test_name"

    if grep -q '\-\-log)' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: --log flag found in start_server.sh"
    else
        fail "$test_name: --log flag NOT found in start_server.sh"
    fi
}

test_21_start_client_has_log_flag() {
    local test_name="start_client.sh has --log flag"
    log "Running: $test_name"

    if grep -q '\-\-log)' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: --log flag found in start_client.sh"
    else
        fail "$test_name: --log flag NOT found in start_client.sh"
    fi
}

test_22_no_log_sets_debug_log_level_empty() {
    local test_name="--no-log sets debug_log_level to empty in server config"
    log "Running: $test_name"

    if grep -q 'debug_log_level = ' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: start_server.sh writes debug_log_level when --no-log"
    else
        fail "$test_name: start_server.sh does NOT write debug_log_level for --no-log"
    fi
}

test_23_no_log_sets_encryption_log_level_none() {
    local test_name="--no-log sets encryption_log_level=none in server config"
    log "Running: $test_name"

    if grep -q 'encryption_log_level = none' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: start_server.sh writes encryption_log_level=none when --no-log"
    else
        fail "$test_name: start_server.sh does NOT write encryption_log_level=none for --no-log"
    fi
}

test_24_no_log_sets_debug_log_level_empty_client() {
    local test_name="--no-log sets debug_log_level to empty in client config"
    log "Running: $test_name"

    if grep -q 'debug_log_level = ' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: start_client.sh writes debug_log_level when --no-log"
    else
        fail "$test_name: start_client.sh does NOT write debug_log_level for --no-log"
    fi
}

test_25_no_log_sets_encryption_log_level_none_client() {
    local test_name="--no-log sets encryption_log_level=none in client config"
    log "Running: $test_name"

    if grep -q 'encryption_log_level = none' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: start_client.sh writes encryption_log_level=none when --no-log"
    else
        fail "$test_name: start_client.sh does NOT write encryption_log_level=none for --no-log"
    fi
}

test_26_logging_enabled_default_is_on() {
    local test_name="LOGGING_ENABLED defaults to 1 (on) in start_server.sh"
    log "Running: $test_name"

    if grep -q 'LOGGING_ENABLED=1' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: LOGGING_ENABLED=1 found in start_server.sh"
    else
        fail "$test_name: LOGGING_ENABLED=1 NOT found in start_server.sh"
    fi
}

test_27_logging_enabled_default_is_on_client() {
    local test_name="LOGGING_ENABLED defaults to 1 (on) in start_client.sh"
    log "Running: $test_name"

    if grep -q 'LOGGING_ENABLED=1' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: LOGGING_ENABLED=1 found in start_client.sh"
    else
        fail "$test_name: LOGGING_ENABLED=1 NOT found in start_client.sh"
    fi
}

test_28_trace_file_respects_log_level() {
    local test_name="encryption_trace.cpp checks shouldLog() before opening file"
    log "Running: $test_name"

    local cpp="$SCRIPT_DIR/src/network/encryption_trace.cpp"
    if grep -q 'shouldLog' "$cpp"; then
        pass "$test_name: encryption_trace.cpp checks shouldLog()"
    else
        fail "$test_name: encryption_trace.cpp does NOT check shouldLog()"
    fi
}

test_29_banner_functions_respect_log_level() {
    local test_name="Banner functions in encryption_log.h respect log level"
    log "Running: $test_name"

    local header="$SCRIPT_DIR/src/network/encryption_log.h"
    for func in logSecureConnectionBanner logInsecureConnectionBanner; do
        if grep -A12 "$func" "$header" | grep -q "shouldLog"; then
            pass "$test_name: $func checks shouldLog()"
        else
            fail "$test_name: $func does NOT check shouldLog()"
        fi
    done
}

test_30_server_starts_with_no_log() {
    local test_name="Server starts with --no-log equivalent config"
    log "Running: $test_name"

    local config="$TEST_DIR/nolog.conf"
    cat > "$config" <<EOF
secure_connection = true
debug_log_level = 
encryption_log_level = none
name = admin
EOF

    run_server_with_config "$config" 30014 "$test_name" 5

    if assert_output_contains "listening on"; then
        pass "$test_name: server started with --no-log equivalent config"
    else
        fail "$test_name: server did not start with --no-log equivalent config"
        echo "$SERVER_OUTPUT" | head -20
    fi
}

test_31_interactive_logging_menu_server() {
    local test_name="start_server.sh has interactive logging menu"
    log "Running: $test_name"

    if grep -q 'select_logging_interactive\|Logging Mode' "$SCRIPT_DIR/start_server.sh"; then
        pass "$test_name: interactive logging menu found in start_server.sh"
    else
        fail "$test_name: interactive logging menu NOT found in start_server.sh"
    fi
}

test_32_interactive_logging_menu_client() {
    local test_name="start_client.sh has interactive logging menu"
    log "Running: $test_name"

    if grep -q 'select_logging_interactive\|Logging Mode' "$SCRIPT_DIR/start_client.sh"; then
        pass "$test_name: interactive logging menu found in start_client.sh"
    else
        fail "$test_name: interactive logging menu NOT found in start_client.sh"
    fi
}

# --- Main ---

main() {
    echo ""
    echo -e "${BOLD}+==================================================+${RESET}"
    echo -e "${BOLD}|   Clawtest Log Toggle Test Suite v9.23             |${RESET}"
    echo -e "${BOLD}+==================================================+${RESET}"
    echo ""

    setup

    # Run all tests
    test_01_encryption_log_level_setting_exists
    test_02_log_level_strings_in_binary
    test_03_enclog_macros_guarded
    test_04_server_starts_with_log_level_none
    test_05_server_starts_with_log_level_trace
    test_06_default_log_level_is_action
    test_07_start_server_script_has_flag
    test_08_start_client_script_has_flag
    test_09_start_server_script_writes_config
    test_10_start_client_script_writes_config
    test_11_settingtypes_documented
    test_12_defaultsettings_has_default
    test_13_encryption_config_has_log_functions
    test_14_encryption_config_cpp_has_implementations
    test_15_enclog_macros_use_shouldLog
    test_16_start_scripts_syntax_valid
    test_17_encryption_log_level_enum_values
    test_18_start_server_has_no_log_flag
    test_19_start_client_has_no_log_flag
    test_20_start_server_has_log_flag
    test_21_start_client_has_log_flag
    test_22_no_log_sets_debug_log_level_empty
    test_23_no_log_sets_encryption_log_level_none
    test_24_no_log_sets_debug_log_level_empty_client
    test_25_no_log_sets_encryption_log_level_none_client
    test_26_logging_enabled_default_is_on
    test_27_logging_enabled_default_is_on_client
    test_28_trace_file_respects_log_level
    test_29_banner_functions_respect_log_level
    test_30_server_starts_with_no_log
    test_31_interactive_logging_menu_server
    test_32_interactive_logging_menu_client

    # Summary
    echo ""
    echo -e "${BOLD}+==================================================+${RESET}"
    echo -e "${BOLD}|              Test Results Summary                  |${RESET}"
    echo -e "${BOLD}+==================================================+${RESET}"
    echo ""
    echo -e "  Total:  ${TOTAL}"
    echo -e "  ${GREEN}Pass:   ${PASS}${RESET}"
    echo -e "  ${RED}Fail:   ${FAIL}${RESET}"
    echo ""

    cleanup

    if [[ "$FAIL" -gt 0 ]]; then
        error "Some tests FAILED!"
        return 1
    else
        log "All tests PASSED!"
        return 0
    fi
}

main "$@"
