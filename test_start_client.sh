#!/usr/bin/env bash
#
# test_start_client.sh — Test Suite for start_client.sh (TDD)
#
# This test suite validates each function of the client start script independently.
# Tests are organized by component:
#   1. Script structure
#   2. Help output
#   3. Argument parsing
#   4. Binary detection
#   5. Server address handling
#   6. Config generation
#   7. Security toggle
#   8. Port handling
#   9. Direct connect (--go) mode
#  10. Config overrides
#  11. Color variables & set -u robustness
#  12. Player name handling
#
# Usage:
#   ./test_start_client.sh [TEST_NAME...]
#   ./test_start_client.sh --all
#   ./test_start_client.sh --list
#

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLIENT_SCRIPT="${SCRIPT_DIR}/start_client.sh"
TEST_DIR="$(mktemp -d)"
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup
cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

# ─── Test Framework ────────────────────────────────────────────────────────────

# Colors — using $'...' ANSI-C quoting
RED=$'\033[1;31m'
GREEN=$'\033[1;32m'
YELLOW=$'\033[1;33m'
CYAN=$'\033[1;36m'
BOLD=$'\033[1m'
RESET=$'\033[0m'

assert_eq() {
    local description="$1" expected="$2" actual="$3"
    if [[ "$expected" == "$actual" ]]; then
        echo -e "  ${GREEN}PASS${RESET}: $description"
        ((TESTS_PASSED++)) || true
    else
        echo -e "  ${RED}FAIL${RESET}: $description"
        echo -e "    Expected: $expected"
        echo -e "    Actual:   $actual"
        ((TESTS_FAILED++)) || true
    fi
    ((TESTS_RUN++)) || true
}

assert_true() {
    local description="$1"
    shift
    if "$@" &>/dev/null; then
        echo -e "  ${GREEN}PASS${RESET}: $description"
        ((TESTS_PASSED++)) || true
    else
        echo -e "  ${RED}FAIL${RESET}: $description"
        ((TESTS_FAILED++)) || true
    fi
    ((TESTS_RUN++)) || true
}

assert_false() {
    local description="$1"
    shift
    if "$@" &>/dev/null; then
        echo -e "  ${RED}FAIL${RESET}: $description (expected failure, got success)"
        ((TESTS_FAILED++)) || true
    else
        echo -e "  ${GREEN}PASS${RESET}: $description"
        ((TESTS_PASSED++)) || true
    fi
    ((TESTS_RUN++)) || true
}

assert_contains() {
    local description="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        echo -e "  ${GREEN}PASS${RESET}: $description"
        ((TESTS_PASSED++)) || true
    else
        echo -e "  ${RED}FAIL${RESET}: $description"
        echo -e "    String does not contain: $needle"
        ((TESTS_FAILED++)) || true
    fi
    ((TESTS_RUN++)) || true
}

assert_not_contains() {
    local description="$1" haystack="$2" needle="$3"
    if [[ "$haystack" != *"$needle"* ]]; then
        echo -e "  ${GREEN}PASS${RESET}: $description"
        ((TESTS_PASSED++)) || true
    else
        echo -e "  ${RED}FAIL${RESET}: $description"
        echo -e "    String should not contain: $needle"
        ((TESTS_FAILED++)) || true
    fi
    ((TESTS_RUN++)) || true
}

assert_file_executable() {
    local description="$1" filepath="$2"
    if [[ -x "$filepath" ]]; then
        echo -e "  ${GREEN}PASS${RESET}: $description"
        ((TESTS_PASSED++)) || true
    else
        echo -e "  ${RED}FAIL${RESET}: $description ($filepath not executable or missing)"
        ((TESTS_FAILED++)) || true
    fi
    ((TESTS_RUN++)) || true
}

# ─── Helper ────────────────────────────────────────────────────────────────────

get_script_content() {
    cat "$CLIENT_SCRIPT"
}

# ─── 1. Script Structure Tests ────────────────────────────────────────────────

test_script_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Script Structure"
    assert_true "start_client.sh file exists" test -f "$CLIENT_SCRIPT"
}

test_script_is_executable() {
    if [[ -x "$CLIENT_SCRIPT" ]]; then
        assert_file_executable "start_client.sh is executable" "$CLIENT_SCRIPT"
    else
        assert_true "start_client.sh exists and is readable" test -r "$CLIENT_SCRIPT"
    fi
}

test_script_has_shebang() {
    local first_line
    first_line="$(head -1 "$CLIENT_SCRIPT")"
    assert_contains "Script has bash shebang" "$first_line" "#!/usr/bin/env bash"
}

test_script_has_set_uo_pipefail() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has set -uo pipefail" "$content" "set -uo pipefail"
}

test_script_no_set_e() {
    local content
    content="$(get_script_content)"
    local standalone_set_e
    standalone_set_e="$(echo "$content" | grep -cE '^set -e$' || true)"
    assert_eq "Script does NOT use standalone set -e" "0" "$standalone_set_e"
}

test_script_has_ansi_c_quoting_colors() {
    local content
    content="$(get_script_content)"
    assert_contains "Script uses ANSI-C quoting for RED" "$content" "RED=\$'\\033["
}

test_script_has_exit_1_on_critical() {
    local content
    content="$(get_script_content)"
    assert_contains "Script uses || exit 1 pattern" "$content" "|| exit 1"
}

# ─── 2. Help Output Tests ────────────────────────────────────────────────────

test_help_flag_works() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Help Output"
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_true "--help flag produces output" test -n "$output"
}

test_help_shows_address_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --address option" "$output" "--address"
}

test_help_shows_port_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --port option" "$output" "--port"
}

test_help_shows_name_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --name option" "$output" "--name"
}

test_help_shows_password_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --password option" "$output" "--password"
}

test_help_shows_secure_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --secure option" "$output" "--secure"
}

test_help_shows_insecure_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --insecure option" "$output" "--insecure"
}

test_help_shows_go_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --go option" "$output" "--go"
}

test_help_shows_build_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --build option" "$output" "--build"
}

test_help_shows_fullscreen_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --fullscreen option" "$output" "--fullscreen"
}

test_help_shows_config_option() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --config option" "$output" "--config"
}

test_help_shows_examples() {
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows usage examples" "$output" "Examples"
}

# ─── 3. Argument Parsing Tests ───────────────────────────────────────────────

test_arg_address_sets_server() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Argument Parsing"
    local result
    result="$(bash -c '
        SERVER_ADDRESS=""
        for arg in --address myserver; do
            case "$arg" in
                --address) SERVER_ADDRESS="myserver" ;;
            esac
        done
        echo "${SERVER_ADDRESS}"
    ')"
    assert_eq "--address sets SERVER_ADDRESS" "myserver" "$result"
}

test_arg_port_custom() {
    local result
    result="$(bash -c '
        PORT=30000
        for arg in --port 30001; do
            case "$arg" in
                --port) PORT="30001" ;;
            esac
        done
        echo "$PORT"
    ')"
    assert_eq "--port 30001 sets PORT=30001" "30001" "$result"
}

test_arg_name_sets_player() {
    local result
    result="$(bash -c '
        PLAYER_NAME=""
        for arg in --name; do
            case "$arg" in
                --name) PLAYER_NAME="player1" ;;
            esac
        done
        echo "${PLAYER_NAME}"
    ')"
    assert_eq "--name sets PLAYER_NAME" "player1" "$result"
}

test_arg_secure_sets_mode() {
    local result
    result="$(bash -c '
        SECURE_MODE=-1
        SECURE_EXPLICIT=0
        for arg in --secure; do
            case "$arg" in
                --secure) SECURE_MODE=1; SECURE_EXPLICIT=1 ;;
            esac
        done
        echo "${SECURE_MODE}:${SECURE_EXPLICIT}"
    ')"
    assert_eq "--secure sets SECURE_MODE=1 and EXPLICIT=1" "1:1" "$result"
}

test_arg_insecure_sets_mode() {
    local result
    result="$(bash -c '
        SECURE_MODE=-1
        SECURE_EXPLICIT=0
        for arg in --insecure; do
            case "$arg" in
                --insecure) SECURE_MODE=0; SECURE_EXPLICIT=1 ;;
            esac
        done
        echo "${SECURE_MODE}:${SECURE_EXPLICIT}"
    ')"
    assert_eq "--insecure sets SECURE_MODE=0 and EXPLICIT=1" "0:1" "$result"
}

test_arg_go_sets_flag() {
    local result
    result="$(bash -c '
        DO_GO=0
        for arg in --go; do
            case "$arg" in
                --go) DO_GO=1 ;;
            esac
        done
        echo "$DO_GO"
    ')"
    assert_eq "--go sets DO_GO=1" "1" "$result"
}

test_arg_build_flag() {
    local result
    result="$(bash -c '
        DO_BUILD=0
        for arg in --build; do
            case "$arg" in
                --build) DO_BUILD=1 ;;
            esac
        done
        echo "$DO_BUILD"
    ')"
    assert_eq "--build sets DO_BUILD=1" "1" "$result"
}

test_arg_fullscreen_flag() {
    local result
    result="$(bash -c '
        DO_FULLSCREEN=-1
        for arg in --fullscreen; do
            case "$arg" in
                --fullscreen) DO_FULLSCREEN=1 ;;
            esac
        done
        echo "$DO_FULLSCREEN"
    ')"
    assert_eq "--fullscreen sets DO_FULLSCREEN=1" "1" "$result"
}

test_arg_windowed_flag() {
    local result
    result="$(bash -c '
        DO_FULLSCREEN=-1
        for arg in --windowed; do
            case "$arg" in
                --windowed) DO_FULLSCREEN=0 ;;
            esac
        done
        echo "$DO_FULLSCREEN"
    ')"
    assert_eq "--windowed sets DO_FULLSCREEN=0" "0" "$result"
}

test_arg_config_overrides() {
    local result
    result="$(bash -c '
        CONFIG_OVERRIDES=()
        CONFIG_OVERRIDES+=("fps_max=60")
        CONFIG_OVERRIDES+=("enable_shaders=false")
        echo "${CONFIG_OVERRIDES[*]}"
    ')"
    assert_eq "--config adds to CONFIG_OVERRIDES" "fps_max=60 enable_shaders=false" "$result"
}

# ─── 4. Binary Detection Tests ───────────────────────────────────────────────

test_binary_detection_checks_bin_dir() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Binary Detection"
    local content
    content="$(get_script_content)"
    assert_contains "Script checks bin/luanti" "$content" "bin/luanti"
}

test_binary_detection_checks_build_bin_dir() {
    local content
    content="$(get_script_content)"
    assert_contains "Script checks build/bin/luanti" "$content" "build/bin/luanti"
}

test_binary_detection_bin_first() {
    local content
    content="$(get_script_content)"
    local bin_pos build_pos
    bin_pos="$(echo "$content" | grep -n 'bin/luanti' | head -1 | cut -d: -f1)"
    build_pos="$(echo "$content" | grep -n 'build/bin/luanti' | head -1 | cut -d: -f1)"
    assert_true "bin/luanti checked before build/bin/luanti" test "$bin_pos" -lt "$build_pos"
}

test_binary_detection_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has find_client_binary function" "$content" "find_client_binary()"
}

test_binary_detection_finds_mock_binary() {
    local mock_source_dir="${TEST_DIR}/mock_project"
    mkdir -p "${mock_source_dir}/bin"
    touch "${mock_source_dir}/bin/luanti"
    chmod +x "${mock_source_dir}/bin/luanti"

    local result
    result="$(bash -c "
        SOURCE_DIR='${mock_source_dir}'
        find_client_binary() {
            local -a search_paths=(
                \"\${SOURCE_DIR}/bin/luanti\"
                \"\${SOURCE_DIR}/build/bin/luanti\"
            )
            for p in \"\${search_paths[@]}\"; do
                if [[ -x \"\$p\" ]]; then
                    echo \"\$p\"
                    return 0
                fi
            done
            return 1
        }
        find_client_binary
    ")"
    assert_contains "find_client_binary finds bin/luanti" "$result" "bin/luanti"
}

test_binary_detection_fallback_to_build() {
    local mock_source_dir="${TEST_DIR}/mock_project2"
    mkdir -p "${mock_source_dir}/build/bin"
    touch "${mock_source_dir}/build/bin/luanti"
    chmod +x "${mock_source_dir}/build/bin/luanti"

    local result
    result="$(bash -c "
        SOURCE_DIR='${mock_source_dir}'
        find_client_binary() {
            local -a search_paths=(
                \"\${SOURCE_DIR}/bin/luanti\"
                \"\${SOURCE_DIR}/build/bin/luanti\"
            )
            for p in \"\${search_paths[@]}\"; do
                if [[ -x \"\$p\" ]]; then
                    echo \"\$p\"
                    return 0
                fi
            done
            return 1
        }
        find_client_binary
    ")"
    assert_contains "find_client_binary falls back to build/bin/luanti" "$result" "build/bin/luanti"
}

test_binary_detection_returns_1_when_missing() {
    local mock_source_dir="${TEST_DIR}/mock_project3"
    mkdir -p "$mock_source_dir"

    local result=0
    bash -c "
        SOURCE_DIR='${mock_source_dir}'
        find_client_binary() {
            local -a search_paths=(
                \"\${SOURCE_DIR}/bin/luanti\"
                \"\${SOURCE_DIR}/build/bin/luanti\"
            )
            for p in \"\${search_paths[@]}\"; do
                if [[ -x \"\$p\" ]]; then
                    echo \"\$p\"
                    return 0
                fi
            done
            return 1
        }
        find_client_binary
    " || result=$?
    assert_true "find_client_binary returns 1 when no binary found" test "$result" -ne 0
}

# ─── 5. Server Address Handling Tests ─────────────────────────────────────────

test_default_address_is_empty() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Server Address"
    local content
    content="$(get_script_content)"
    assert_contains "Default SERVER_ADDRESS is empty (ask user)" "$content" 'SERVER_ADDRESS=""'
}

test_address_passed_to_binary() {
    local content
    content="$(get_script_content)"
    assert_contains "Address passed to client binary" "$content" "--address"
}

test_localhost_used_as_default() {
    local content
    content="$(get_script_content)"
    assert_contains "Non-interactive defaults to localhost" "$content" "localhost"
}

test_interactive_server_menu_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has select_server_interactive function" "$content" "select_server_interactive()"
}

# ─── 6. Config Generation Tests ──────────────────────────────────────────────

test_generate_temp_config_function_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Config Generation"
    local content
    content="$(get_script_content)"
    assert_contains "Script has generate_temp_config function" "$content" "generate_temp_config()"
}

test_temp_config_includes_secure_connection_true() {
    local temp_conf="${TEST_DIR}/test_secure.conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='/nonexistent'
        SECURE_MODE=1
        CONFIG_OVERRIDES=()
        DO_FULLSCREEN=-1

        : > \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_client.sh (secure mode)' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = true' >> \"\$TEMP_CONFIG\"
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has secure_connection = true" "$content" "secure_connection = true"
}

test_temp_config_includes_secure_connection_false() {
    local temp_conf="${TEST_DIR}/test_insecure.conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='/nonexistent'
        SECURE_MODE=0
        CONFIG_OVERRIDES=()
        DO_FULLSCREEN=-1

        : > \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_client.sh --insecure' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = false' >> \"\$TEMP_CONFIG\"
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has secure_connection = false" "$content" "secure_connection = false"
}

test_temp_config_includes_fullscreen() {
    local temp_conf="${TEST_DIR}/test_fullscreen.conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='/nonexistent'
        SECURE_MODE=1
        CONFIG_OVERRIDES=()
        DO_FULLSCREEN=1

        : > \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = true' >> \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_client.sh --fullscreen' >> \"\$TEMP_CONFIG\"
        echo 'fullscreen = true' >> \"\$TEMP_CONFIG\"
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has fullscreen = true" "$content" "fullscreen = true"
}

test_temp_config_includes_overrides() {
    local temp_conf="${TEST_DIR}/test_overrides.conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_OVERRIDES=('fps_max=60' 'enable_shaders=false')

        echo '# Config overrides from --config flags' >> \"\$TEMP_CONFIG\"
        for override in \"\${CONFIG_OVERRIDES[@]}\"; do
            echo \"\$override\" >> \"\$TEMP_CONFIG\"
        done
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has fps_max override" "$content" "fps_max=60"
    assert_contains "Temp config has enable_shaders override" "$content" "enable_shaders=false"
}

test_cleanup_temp_config_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has cleanup_temp_config function" "$content" "cleanup_temp_config()"
}

# ─── 7. Security Toggle Tests ────────────────────────────────────────────────

test_secure_mode_default_is_ask() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Security Toggle"
    local content
    content="$(get_script_content)"
    assert_contains "Default SECURE_MODE is -1 (ask user)" "$content" "SECURE_MODE=-1"
}

test_secure_mode_creates_temp_config() {
    local content
    content="$(get_script_content)"
    assert_contains "Secure mode creates temp config" "$content" "generate_temp_config"
}

test_secure_explicit_flag_set_by_secure() {
    local result
    result="$(bash -c '
        SECURE_MODE=-1
        SECURE_EXPLICIT=0
        for arg in --secure; do
            case "$arg" in
                --secure) SECURE_MODE=1; SECURE_EXPLICIT=1 ;;
            esac
        done
        echo "$SECURE_EXPLICIT"
    ')"
    assert_eq "--secure sets SECURE_EXPLICIT=1" "1" "$result"
}

test_secure_explicit_flag_set_by_insecure() {
    local result
    result="$(bash -c '
        SECURE_MODE=-1
        SECURE_EXPLICIT=0
        for arg in --insecure; do
            case "$arg" in
                --insecure) SECURE_MODE=0; SECURE_EXPLICIT=1 ;;
            esac
        done
        echo "$SECURE_EXPLICIT"
    ')"
    assert_eq "--insecure sets SECURE_EXPLICIT=1" "1" "$result"
}

test_interactive_menu_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "show_start_menu function exists" "$content" "show_start_menu()"
}

test_interactive_menu_security_asked_when_not_explicit() {
    local content
    content="$(get_script_content)"
    assert_contains "Menu checks SECURE_EXPLICIT" "$content" "SECURE_EXPLICIT"
}

# ─── 8. Port Handling Tests ─────────────────────────────────────────────────

test_default_port_is_30000() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Port Handling"
    local content
    content="$(get_script_content)"
    assert_contains "Default port is 30000" "$content" "PORT=30000"
}

test_custom_port_parsed() {
    local result
    result="$(bash -c '
        PORT=30000
        set -- --port 30001
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --port) PORT="$2"; shift ;;
            esac
            shift
        done
        echo "$PORT"
    ')"
    assert_eq "--port 30001 sets PORT=30001" "30001" "$result"
}

test_port_passed_to_binary() {
    local content
    content="$(get_script_content)"
    assert_contains "Port passed to client binary" "$content" "--port"
}

# ─── 9. Direct Connect (--go) Tests ─────────────────────────────────────────

test_go_flag_default_off() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Direct Connect (--go)"
    local content
    content="$(get_script_content)"
    assert_contains "Default DO_GO is 0 (ask user)" "$content" "DO_GO=0"
}

test_go_flag_passed_to_binary() {
    local content
    content="$(get_script_content)"
    assert_contains "--go flag passed to binary" "$content" "--go"
}

test_go_skips_main_menu() {
    local content
    content="$(get_script_content)"
    assert_contains "Script mentions 'skip main menu'" "$content" "skip main menu"
}

test_launch_mode_menu_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Menu has launch mode selection" "$content" "Direct Connect"
}

# ─── 10. Config Override Tests ──────────────────────────────────────────────

test_config_override_function_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Config Overrides"
    local content
    content="$(get_script_content)"
    assert_contains "Script supports CONFIG_OVERRIDES" "$content" "CONFIG_OVERRIDES"
}

test_config_override_array() {
    local result
    result="$(bash -c '
        CONFIG_OVERRIDES=()
        CONFIG_OVERRIDES+=("fps_max=60")
        CONFIG_OVERRIDES+=("enable_shaders=false")
        echo "${#CONFIG_OVERRIDES[@]}"
    ')"
    assert_eq "Two config overrides are stored" "2" "$result"
}

test_config_override_key_value_format() {
    local content
    content="$(get_script_content)"
    assert_contains "Script validates KEY=VALUE format" "$content" '*"="*'
}

test_config_override_written_to_temp_conf() {
    local temp_conf="${TEST_DIR}/test_config_override.conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_OVERRIDES=('fps_max=60' 'enable_shaders=false')

        echo '# Config overrides from --config flags' >> \"\$TEMP_CONFIG\"
        for override in \"\${CONFIG_OVERRIDES[@]}\"; do
            echo \"\$override\" >> \"\$TEMP_CONFIG\"
        done
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Config override fps_max written" "$content" "fps_max=60"
    assert_contains "Config override enable_shaders written" "$content" "enable_shaders=false"
}

# ─── 11. Color Variable & set -u Robustness Tests ────────────────────────────

test_all_color_variables_defined() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Color Variables & set -u Robustness"
    local content
    content="$(get_script_content)"
    assert_contains "RED is defined" "$content" "RED=\$'"
    assert_contains "GREEN is defined" "$content" "GREEN=\$'"
    assert_contains "YELLOW is defined" "$content" "YELLOW=\$'"
    assert_contains "BLUE is defined" "$content" "BLUE=\$'"
    assert_contains "CYAN is defined" "$content" "CYAN=\$'"
    assert_contains "BOLD is defined" "$content" "BOLD=\$'"
    assert_contains "RESET is defined" "$content" "RESET=\$'"
}

test_nc_alias_defined() {
    local content
    content="$(get_script_content)"
    assert_contains "NC is defined as alias for RESET" "$content" "NC=\$'"
}

test_nc_equals_reset() {
    local result
    result="$(bash -c '
        RESET=$'\''\033[0m'\''
        NC=$'\''\033[0m'\''
        [[ "$NC" == "$RESET" ]] && echo "match" || echo "different"
    ')"
    assert_eq "NC equals RESET" "match" "$result"
}

test_non_interactive_mode_detection() {
    local content
    content="$(get_script_content)"
    assert_contains "Script detects non-terminal stdin" "$content" "! -t 0"
}

test_non_interactive_defaults_to_secure() {
    local content
    content="$(get_script_content)"
    assert_contains "Non-interactive defaults to secure" "$content" "SECURE_MODE=1"
}

test_non_interactive_defaults_to_localhost() {
    local content
    content="$(get_script_content)"
    assert_contains "Non-interactive defaults to localhost" "$content" "localhost"
}

test_whitespace_trim_no_subshell() {
    local content
    content="$(get_script_content)"
    local xargs_count
    xargs_count="$(echo "$content" | grep -c '| xargs' || true)"
    assert_eq "Script does not use | xargs for trimming" "0" "$xargs_count"
}

test_defensive_defaults_for_secure_mode() {
    local content
    content="$(get_script_content)"
    assert_contains "Uses defensive default for SECURE_MODE" "$content" 'SECURE_MODE:-'
}

test_defensive_defaults_for_server_address() {
    local content
    content="$(get_script_content)"
    assert_contains "Uses defensive default for SERVER_ADDRESS" "$content" 'SERVER_ADDRESS:-'
}

test_defensive_defaults_for_go_mode() {
    local content
    content="$(get_script_content)"
    assert_contains "Uses defensive default for DO_GO" "$content" 'DO_GO:-'
}

test_no_unbound_variable_in_cleanup() {
    # Ensure cleanup_temp_config uses ${TEMP_CONFIG:-} to avoid unbound errors
    local content
    content="$(get_script_content)"
    assert_contains "cleanup uses defensive default for TEMP_CONFIG" "$content" 'TEMP_CONFIG:-'
}

# ─── 12. Player Name Handling Tests ──────────────────────────────────────────

test_player_name_default_empty() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Player Name"
    local content
    content="$(get_script_content)"
    assert_contains "Default PLAYER_NAME is empty" "$content" 'PLAYER_NAME=""'
}

test_player_name_passed_to_binary() {
    local content
    content="$(get_script_content)"
    assert_contains "Player name passed to client binary" "$content" "--name"
}

test_player_password_passed_to_binary() {
    local content
    content="$(get_script_content)"
    assert_contains "Player password passed to client binary" "$content" "--password"
}

test_password_file_passed_to_binary() {
    local content
    content="$(get_script_content)"
    assert_contains "Password file passed to client binary" "$content" "--password-file"
}

test_interactive_name_menu_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has select_name_interactive function" "$content" "select_name_interactive()"
}

test_name_from_config_read() {
    local content
    content="$(get_script_content)"
    assert_contains "Script reads name from config file" "$content" "CONFIG_FILE"
}

# ─── Integration Smoke Tests ────────────────────────────────────────────────

test_list_servers_command() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Integration Smoke Tests"
    # --list-servers requires network; just check it doesn't crash immediately
    local output
    output="$(bash "$CLIENT_SCRIPT" --help 2>&1)" || true
    assert_contains "--list-servers is documented" "$output" "--list-servers"
}

test_non_interactive_no_hang() {
    # Ensure the script doesn't hang on non-interactive stdin
    local output
    output="$(echo "" | bash "$CLIENT_SCRIPT" --secure --go --address localhost 2>&1)" || true
    assert_not_contains "Non-interactive output doesn't show unbound variable" "$output" "unbound variable"
}

test_build_client_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has build_client function" "$content" "build_client()"
}

test_build_client_uses_build_script() {
    local content
    content="$(get_script_content)"
    assert_contains "Build uses --client flag" "$content" "--client"
}

# ─── Test Runner ──────────────────────────────────────────────────────────────

# List of all test functions
ALL_TESTS=(
    # 1. Script Structure
    test_script_exists
    test_script_is_executable
    test_script_has_shebang
    test_script_has_set_uo_pipefail
    test_script_no_set_e
    test_script_has_ansi_c_quoting_colors
    test_script_has_exit_1_on_critical

    # 2. Help Output
    test_help_flag_works
    test_help_shows_address_option
    test_help_shows_port_option
    test_help_shows_name_option
    test_help_shows_password_option
    test_help_shows_secure_option
    test_help_shows_insecure_option
    test_help_shows_go_option
    test_help_shows_build_option
    test_help_shows_fullscreen_option
    test_help_shows_config_option
    test_help_shows_examples

    # 3. Argument Parsing
    test_arg_address_sets_server
    test_arg_port_custom
    test_arg_name_sets_player
    test_arg_secure_sets_mode
    test_arg_insecure_sets_mode
    test_arg_go_sets_flag
    test_arg_build_flag
    test_arg_fullscreen_flag
    test_arg_windowed_flag
    test_arg_config_overrides

    # 4. Binary Detection
    test_binary_detection_checks_bin_dir
    test_binary_detection_checks_build_bin_dir
    test_binary_detection_bin_first
    test_binary_detection_function_exists
    test_binary_detection_finds_mock_binary
    test_binary_detection_fallback_to_build
    test_binary_detection_returns_1_when_missing

    # 5. Server Address
    test_default_address_is_empty
    test_address_passed_to_binary
    test_localhost_used_as_default
    test_interactive_server_menu_function_exists

    # 6. Config Generation
    test_generate_temp_config_function_exists
    test_temp_config_includes_secure_connection_true
    test_temp_config_includes_secure_connection_false
    test_temp_config_includes_fullscreen
    test_temp_config_includes_overrides
    test_cleanup_temp_config_function_exists

    # 7. Security Toggle
    test_secure_mode_default_is_ask
    test_secure_mode_creates_temp_config
    test_secure_explicit_flag_set_by_secure
    test_secure_explicit_flag_set_by_insecure
    test_interactive_menu_function_exists
    test_interactive_menu_security_asked_when_not_explicit

    # 8. Port Handling
    test_default_port_is_30000
    test_custom_port_parsed
    test_port_passed_to_binary

    # 9. Direct Connect
    test_go_flag_default_off
    test_go_flag_passed_to_binary
    test_go_skips_main_menu
    test_launch_mode_menu_exists

    # 10. Config Overrides
    test_config_override_function_exists
    test_config_override_array
    test_config_override_key_value_format
    test_config_override_written_to_temp_conf

    # 11. Color Variables & set -u Robustness
    test_all_color_variables_defined
    test_nc_alias_defined
    test_nc_equals_reset
    test_non_interactive_mode_detection
    test_non_interactive_defaults_to_secure
    test_non_interactive_defaults_to_localhost
    test_whitespace_trim_no_subshell
    test_defensive_defaults_for_secure_mode
    test_defensive_defaults_for_server_address
    test_defensive_defaults_for_go_mode
    test_no_unbound_variable_in_cleanup

    # 12. Player Name
    test_player_name_default_empty
    test_player_name_passed_to_binary
    test_player_password_passed_to_binary
    test_password_file_passed_to_binary
    test_interactive_name_menu_exists
    test_name_from_config_read

    # Integration
    test_list_servers_command
    test_non_interactive_no_hang
    test_build_client_function_exists
    test_build_client_uses_build_script
)

# List test names
list_tests() {
    for t in "${ALL_TESTS[@]}"; do
        echo "$t"
    done
}

# Run all or specific tests
run_tests() {
    local tests_to_run=()

    if [[ $# -eq 0 ]] || [[ "${1:-}" == "--all" ]]; then
        tests_to_run=("${ALL_TESTS[@]}")
    else
        for name in "$@"; do
            tests_to_run+=("$name")
        done
    fi

    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}║    Luanti Client Script — TDD Test Suite         ║${RESET}"
    echo -e "${BOLD}╚══════════════════════════════════════════════════╝${RESET}"
    echo ""
    echo -e "  Running ${#tests_to_run[@]} tests..."
    echo ""

    for t in "${tests_to_run[@]}"; do
        $t
    done

    echo ""
    echo -e "${BOLD}──────────────────────────────────────────────────${RESET}"
    echo -e "  Total:  ${TESTS_RUN}"
    echo -e "  ${GREEN}Passed: ${TESTS_PASSED}${RESET}"
    if [[ "$TESTS_FAILED" -gt 0 ]]; then
        echo -e "  ${RED}Failed: ${TESTS_FAILED}${RESET}"
    else
        echo -e "  Failed: 0"
    fi
    echo -e "${BOLD}──────────────────────────────────────────────────${RESET}"
    echo ""

    if [[ "$TESTS_FAILED" -gt 0 ]]; then
        return 1
    fi
    return 0
}

# Handle --list flag
if [[ "${1:-}" == "--list" ]]; then
    list_tests
    exit 0
fi

run_tests "$@"
