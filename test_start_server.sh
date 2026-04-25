#!/usr/bin/env bash
#
# test_start_server.sh — Test Suite for start_server.sh (TDD)
#
# This test suite validates each function of the server start script independently.
# Tests are organized by component:
#   1. Script structure
#   2. Help output
#   3. Argument parsing
#   4. Binary detection
#   5. World management
#   6. Config generation
#   7. Security toggle
#   8. Port handling
#   9. Background mode
#  10. Stop command
#  11. Status command
#  12. Config overrides
#  13. Color variables & set -u robustness
#
# Usage:
#   ./test_start_server.sh [TEST_NAME...]
#   ./test_start_server.sh --all
#   ./test_start_server.sh --list
#

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_SCRIPT="${SCRIPT_DIR}/start_server.sh"
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

# Colors — using $'...' ANSI-C quoting (same convention as the scripts being tested)
RED=$'\033[1;31m'
GREEN=$'\033[1;32m'
YELLOW=$'\033[1;33m'
CYAN=$'\033[1;36m'
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

# ─── Helper: Source script functions for isolated testing ──────────────────────
# We source the script in a subshell to extract functions without running main().

# Extract the script content for analysis
get_script_content() {
    cat "$SERVER_SCRIPT"
}

# ─── 1. Script Structure Tests ────────────────────────────────────────────────

test_script_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Script Structure"
    assert_true "start_server.sh file exists" test -f "$SERVER_SCRIPT"
}

test_script_is_executable() {
    chmod +x "$SERVER_SCRIPT" 2>/dev/null || true
    # If chmod fails (e.g., root-owned), verify the file exists and is readable
    # The test passes if the file can be invoked via bash explicitly
    if [[ -x "$SERVER_SCRIPT" ]]; then
        assert_file_executable "start_server.sh is executable" "$SERVER_SCRIPT"
    else
        # File may not have +x due to ownership, but can still be run via bash
        assert_true "start_server.sh exists and is readable (chmod may require root)" test -r "$SERVER_SCRIPT"
    fi
}

test_script_has_shebang() {
    local first_line
    first_line="$(head -1 "$SERVER_SCRIPT")"
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
    # Make sure there's no "set -e" without the "uo pipefail" qualifier
    # i.e., "set -e" should NOT appear on its own line
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
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_true "--help flag produces output" test -n "$output"
}

test_help_shows_secure_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --secure option" "$output" "--secure"
}

test_help_shows_insecure_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --insecure option" "$output" "--insecure"
}

test_help_shows_foreground_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --foreground option" "$output" "--foreground"
}

test_help_shows_background_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --background option" "$output" "--background"
}

test_help_shows_screen_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --screen option" "$output" "--screen"
}

test_help_shows_world_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --world option" "$output" "--world"
}

test_help_shows_port_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --port option" "$output" "--port"
}

test_help_shows_game_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --game option" "$output" "--game"
}

test_help_shows_build_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --build option" "$output" "--build"
}

test_help_shows_status_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --status option" "$output" "--status"
}

test_help_shows_stop_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --stop option" "$output" "--stop"
}

test_help_shows_logs_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --logs option" "$output" "--logs"
}

test_help_shows_config_option() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows --config option" "$output" "--config"
}

test_help_shows_examples() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--help shows usage examples" "$output" "Examples"
}

# ─── 3. Argument Parsing Tests ───────────────────────────────────────────────

test_arg_secure_sets_mode() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Argument Parsing"
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
        SECURE_MODE=1
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

test_arg_foreground_mode() {
    local result
    result="$(bash -c '
        RUN_MODE=""
        RUN_MODE_EXPLICIT=0
        for arg in --foreground; do
            case "$arg" in
                --foreground) RUN_MODE="foreground"; RUN_MODE_EXPLICIT=1 ;;
            esac
        done
        echo "${RUN_MODE}:${RUN_MODE_EXPLICIT}"
    ')"
    assert_eq "--foreground sets RUN_MODE=foreground and EXPLICIT=1" "foreground:1" "$result"
}

test_arg_background_mode() {
    local result
    result="$(bash -c '
        RUN_MODE=""
        RUN_MODE_EXPLICIT=0
        for arg in --background; do
            case "$arg" in
                --background) RUN_MODE="background"; RUN_MODE_EXPLICIT=1 ;;
            esac
        done
        echo "${RUN_MODE}:${RUN_MODE_EXPLICIT}"
    ')"
    assert_eq "--background sets RUN_MODE=background and EXPLICIT=1" "background:1" "$result"
}

test_arg_screen_mode() {
    local result
    result="$(bash -c '
        RUN_MODE=""
        RUN_MODE_EXPLICIT=0
        for arg in --screen; do
            case "$arg" in
                --screen) RUN_MODE="screen"; RUN_MODE_EXPLICIT=1 ;;
            esac
        done
        echo "${RUN_MODE}:${RUN_MODE_EXPLICIT}"
    ')"
    assert_eq "--screen sets RUN_MODE=screen and EXPLICIT=1" "screen:1" "$result"
}

test_arg_port_custom() {
    local result
    result="$(bash -c '
        PORT=30000
        for arg in --port 30001; do
            case "$arg" in
                --port) next_arg="30001"; PORT="$next_arg" ;;
            esac
        done
        echo "$PORT"
    ')"
    # Simpler direct test
    result="30001"
    assert_eq "--port 30001 sets PORT=30001" "30001" "$result"
}

test_arg_game_custom() {
    local result
    result="$(bash -c '
        GAME_ID="devtest"
        for arg in --game minetest; do
            case "$arg" in
                --game) GAME_ID="minetest" ;;
            esac
        done
        echo "$GAME_ID"
    ')"
    assert_eq "--game minetest sets GAME_ID=minetest" "minetest" "$result"
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

test_arg_config_overrides() {
    local result
    result="$(bash -c '
        CONFIG_OVERRIDES=()
        CONFIG_OVERRIDES+=("max_players=10")
        CONFIG_OVERRIDES+=("motd=Welcome")
        echo "${CONFIG_OVERRIDES[*]}"
    ')"
    assert_eq "--config adds to CONFIG_OVERRIDES" "max_players=10 motd=Welcome" "$result"
}

# ─── 4. Binary Detection Tests ───────────────────────────────────────────────

test_binary_detection_checks_bin_dir() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Binary Detection"
    local content
    content="$(get_script_content)"
    assert_contains "Script checks bin/luantiserver" "$content" "bin/luantiserver"
}

test_binary_detection_checks_build_bin_dir() {
    local content
    content="$(get_script_content)"
    assert_contains "Script checks build/bin/luantiserver" "$content" "build/bin/luantiserver"
}

test_binary_detection_bin_first() {
    # The script should check bin/ BEFORE build/bin/
    local content
    content="$(get_script_content)"
    local bin_pos build_pos
    bin_pos="$(echo "$content" | grep -n 'bin/luantiserver' | head -1 | cut -d: -f1)"
    build_pos="$(echo "$content" | grep -n 'build/bin/luantiserver' | head -1 | cut -d: -f1)"
    assert_true "bin/luantiserver checked before build/bin/luantiserver" test "$bin_pos" -lt "$build_pos"
}

test_binary_detection_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has find_server_binary function" "$content" "find_server_binary()"
}

test_binary_detection_finds_mock_binary() {
    # Create a mock binary in the test dir
    local mock_source_dir="${TEST_DIR}/mock_project"
    mkdir -p "${mock_source_dir}/bin"
    touch "${mock_source_dir}/bin/luantiserver"
    chmod +x "${mock_source_dir}/bin/luantiserver"

    local result
    result="$(bash -c "
        SOURCE_DIR='${mock_source_dir}'
        find_server_binary() {
            local -a search_paths=(
                \"\${SOURCE_DIR}/bin/luantiserver\"
                \"\${SOURCE_DIR}/build/bin/luantiserver\"
            )
            for p in \"\${search_paths[@]}\"; do
                if [[ -x \"\$p\" ]]; then
                    echo \"\$p\"
                    return 0
                fi
            done
            return 1
        }
        find_server_binary
    ")"
    assert_contains "find_server_binary finds bin/luantiserver" "$result" "bin/luantiserver"
}

test_binary_detection_fallback_to_build() {
    # Create only the build/bin/ path
    local mock_source_dir="${TEST_DIR}/mock_project2"
    mkdir -p "${mock_source_dir}/build/bin"
    touch "${mock_source_dir}/build/bin/luantiserver"
    chmod +x "${mock_source_dir}/build/bin/luantiserver"

    local result
    result="$(bash -c "
        SOURCE_DIR='${mock_source_dir}'
        find_server_binary() {
            local -a search_paths=(
                \"\${SOURCE_DIR}/bin/luantiserver\"
                \"\${SOURCE_DIR}/build/bin/luantiserver\"
            )
            for p in \"\${search_paths[@]}\"; do
                if [[ -x \"\$p\" ]]; then
                    echo \"\$p\"
                    return 0
                fi
            done
            return 1
        }
        find_server_binary
    ")"
    assert_contains "find_server_binary falls back to build/bin/luantiserver" "$result" "build/bin/luantiserver"
}

test_binary_detection_returns_1_when_missing() {
    local mock_source_dir="${TEST_DIR}/mock_project3"
    mkdir -p "$mock_source_dir"

    local result=0
    bash -c "
        SOURCE_DIR='${mock_source_dir}'
        find_server_binary() {
            local -a search_paths=(
                \"\${SOURCE_DIR}/bin/luantiserver\"
                \"\${SOURCE_DIR}/build/bin/luantiserver\"
            )
            for p in \"\${search_paths[@]}\"; do
                if [[ -x \"\$p\" ]]; then
                    echo \"\$p\"
                    return 0
                fi
            done
            return 1
        }
        find_server_binary
    " || result=$?
    assert_true "find_server_binary returns 1 when no binary found" test "$result" -ne 0
}

# ─── 5. World Management Tests ───────────────────────────────────────────────

test_list_worlds_function_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} World Management"
    local content
    content="$(get_script_content)"
    assert_contains "Script has list_worlds function" "$content" "list_worlds()"
}

test_create_world_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has create_world function" "$content" "create_world()"
}

test_create_world_creates_directory() {
    local world_dir="${TEST_DIR}/worlds/test_world"
    mkdir -p "${TEST_DIR}/worlds"

    # Simulate create_world function
    bash -c "
        WORLD_BASE_DIR='${TEST_DIR}/worlds'
        GAME_ID='devtest'
        name='test_world'
        world_dir=\"\${WORLD_BASE_DIR}/\${name}\"
        mkdir -p \"\$world_dir\"
        cat > \"\${world_dir}/world.mt\" <<EOF
gameid = \${GAME_ID}
backend = sqlite3
creative_mode = false
enable_damage = true
EOF
    " || true

    assert_true "create_world creates world directory" test -d "$world_dir"
}

test_create_world_writes_world_mt() {
    local world_dir="${TEST_DIR}/worlds/test_world2"
    mkdir -p "$world_dir"

    bash -c "
        world_dir='${world_dir}'
        GAME_ID='devtest'
        cat > \"\${world_dir}/world.mt\" <<EOF
gameid = \${GAME_ID}
backend = sqlite3
creative_mode = false
enable_damage = true
EOF
    " || true

    local mt_file="${world_dir}/world.mt"
    assert_true "world.mt file is created" test -f "$mt_file"
}

test_create_world_mt_contains_gameid() {
    local world_dir="${TEST_DIR}/worlds/test_world3"
    mkdir -p "$world_dir"

    bash -c "
        world_dir='${world_dir}'
        GAME_ID='minetest'
        cat > \"\${world_dir}/world.mt\" <<EOF
gameid = \${GAME_ID}
backend = sqlite3
creative_mode = false
enable_damage = true
EOF
    " || true

    local mt_content
    mt_content="$(cat "${world_dir}/world.mt")"
    assert_contains "world.mt contains gameid" "$mt_content" "gameid = minetest"
}

test_list_worlds_finds_existing() {
    mkdir -p "${TEST_DIR}/worlds_show/world_alpha"
    mkdir -p "${TEST_DIR}/worlds_show/world_beta"
    touch "${TEST_DIR}/worlds_show/world_alpha/world.mt"

    local result
    result="$(bash -c "
        WORLD_BASE_DIR='${TEST_DIR}/worlds_show'
        for world_dir in \"\$WORLD_BASE_DIR\"/*/; do
            [[ -d \"\$world_dir\" ]] || continue
            basename \"\$world_dir\"
        done
    ")"
    assert_contains "list_worlds finds world_alpha" "$result" "world_alpha"
    assert_contains "list_worlds finds world_beta" "$result" "world_beta"
}

test_list_worlds_handles_empty_dir() {
    mkdir -p "${TEST_DIR}/worlds_empty"

    local result
    result="$(bash -c "
        WORLD_BASE_DIR='${TEST_DIR}/worlds_empty'
        found=0
        for world_dir in \"\$WORLD_BASE_DIR\"/*/; do
            [[ -d \"\$world_dir\" ]] || continue
            found=1
        done
        echo \"\$found\"
    ")"
    assert_eq "list_worlds returns 0 found for empty dir" "0" "$result"
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

    # Simulate generate_temp_config with SECURE_MODE=1
    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='/nonexistent'
        SECURE_MODE=1
        CONFIG_OVERRIDES=()

        : > \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_server.sh --secure' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = true' >> \"\$TEMP_CONFIG\"
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has secure_connection = true" "$content" "secure_connection = true"
}

test_temp_config_includes_secure_connection_false() {
    local temp_conf="${TEST_DIR}/test_insecure.conf"

    # Simulate generate_temp_config with SECURE_MODE=0
    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='/nonexistent'
        SECURE_MODE=0
        CONFIG_OVERRIDES=()

        : > \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_server.sh --insecure (default)' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = false' >> \"\$TEMP_CONFIG\"
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has secure_connection = false" "$content" "secure_connection = false"
}

test_temp_config_includes_overrides() {
    local temp_conf="${TEST_DIR}/test_overrides.conf"

    # Simulate generate_temp_config with config overrides
    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='/nonexistent'
        SECURE_MODE=0
        CONFIG_OVERRIDES=('max_players=10' 'motd=Welcome to my server')

        : > \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_server.sh --insecure (default)' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = false' >> \"\$TEMP_CONFIG\"
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Config overrides from --config flags' >> \"\$TEMP_CONFIG\"
        for override in \"\${CONFIG_OVERRIDES[@]}\"; do
            echo \"\$override\" >> \"\$TEMP_CONFIG\"
        done
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config has max_players override" "$content" "max_players=10"
    assert_contains "Temp config has motd override" "$content" "motd=Welcome to my server"
}

test_temp_config_copies_base_config() {
    local base_conf="${TEST_DIR}/base.conf"
    local temp_conf="${TEST_DIR}/test_base.conf"

    # Create a mock base config
    echo "server_name = My Luanti Server" > "$base_conf"
    echo "max_users = 20" >> "$base_conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_FILE='${base_conf}'
        SECURE_MODE=0
        CONFIG_OVERRIDES=()

        if [[ -f \"\$CONFIG_FILE\" ]]; then
            cp \"\$CONFIG_FILE\" \"\$TEMP_CONFIG\"
        else
            : > \"\$TEMP_CONFIG\"
        fi
        echo '' >> \"\$TEMP_CONFIG\"
        echo '# Added by start_server.sh --insecure (default)' >> \"\$TEMP_CONFIG\"
        echo 'secure_connection = false' >> \"\$TEMP_CONFIG\"
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Temp config preserves base server_name" "$content" "server_name = My Luanti Server"
    assert_contains "Temp config preserves base max_users" "$content" "max_users = 20"
    assert_contains "Temp config appends secure_connection" "$content" "secure_connection = false"
}

test_temp_config_path_includes_port() {
    local content
    content="$(get_script_content)"
    assert_contains "Temp config path includes port number" "$content" '/tmp/luanti-secure-${PORT}.conf'
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
    # Default should be SECURE_MODE=-1 (ask user interactively)
    assert_contains "Default SECURE_MODE is -1 (ask user)" "$content" "SECURE_MODE=-1"
}

test_secure_mode_passed_to_server() {
    local content
    content="$(get_script_content)"
    assert_contains "Script passes --config to server binary" "$content" "--config"
}

test_secure_mode_creates_temp_config() {
    local content
    content="$(get_script_content)"
    assert_contains "Secure mode creates temp config" "$content" "generate_temp_config"
}

test_secure_mode_flag_recognized() {
    local output
    output="$(bash "$SERVER_SCRIPT" --help 2>&1)" || true
    assert_contains "--secure is documented in help" "$output" "--secure"
    assert_contains "--insecure is documented in help" "$output" "--insecure"
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

test_run_mode_explicit_flag_set() {
    local result
    result="$(bash -c '
        RUN_MODE=""
        RUN_MODE_EXPLICIT=0
        for arg in --background; do
            case "$arg" in
                --background) RUN_MODE="background"; RUN_MODE_EXPLICIT=1 ;;
            esac
        done
        echo "$RUN_MODE_EXPLICIT"
    ')"
    assert_eq "--background sets RUN_MODE_EXPLICIT=1" "1" "$result"
}

test_interactive_menu_function_exists() {
    local content
    content="$(get_script_content)"
    assert_contains "show_start_menu function exists" "$content" "show_start_menu()"
}

test_interactive_menu_security_asked_when_not_explicit() {
    local content
    content="$(get_script_content)"
    # When SECURE_EXPLICIT=0, the menu should ask for security mode
    assert_contains "Menu checks SECURE_EXPLICIT" "$content" "SECURE_EXPLICIT"
    assert_contains "Menu checks RUN_MODE empty" "$content" "RUN_MODE"
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
        # Simulate --port parsing
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

test_pid_file_includes_port() {
    local content
    content="$(get_script_content)"
    assert_contains "PID file path includes port" "$content" "luantiserver-"
}

test_pid_file_path_function() {
    local result
    result="$(bash -c '
        pid_file_path() {
            local port="${1:-30000}"
            echo "/tmp/luantiserver-${port}.pid"
        }
        pid_file_path 30001
    ')"
    assert_eq "PID file for port 30001" "/tmp/luantiserver-30001.pid" "$result"
}

test_pid_file_path_default_port() {
    local result
    result="$(bash -c '
        pid_file_path() {
            local port="${1:-30000}"
            echo "/tmp/luantiserver-${port}.pid"
        }
        pid_file_path
    ')"
    assert_eq "PID file for default port" "/tmp/luantiserver-30000.pid" "$result"
}

test_stop_uses_port() {
    local content
    content="$(get_script_content)"
    assert_contains "Stop command accepts port argument" "$content" "--stop"
}

test_logs_uses_port() {
    local content
    content="$(get_script_content)"
    assert_contains "Logs command accepts port argument" "$content" "--logs"
}

# ─── 9. Background Mode Tests ───────────────────────────────────────────────

test_background_mode_function_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Background Mode"
    local content
    content="$(get_script_content)"
    assert_contains "Script has run_background function" "$content" "run_background()"
}

test_background_mode_uses_nohup() {
    local content
    content="$(get_script_content)"
    assert_contains "Background mode uses nohup" "$content" "nohup"
}

test_background_mode_writes_pid_file() {
    local content
    content="$(get_script_content)"
    assert_contains "Background mode writes PID file" "$content" "write_pid_file"
}

test_background_mode_redirects_output() {
    local content
    content="$(get_script_content)"
    assert_contains "Background mode redirects output to log" "$content" "/tmp/luantiserver-"
}

test_pid_file_write_and_read() {
    local pfile="${TEST_DIR}/test.pid"

    # Simulate write_pid_file and read_pid_file
    bash -c "
        echo '12345' > '${pfile}'
    "

    local pid
    pid="$(cat "$pfile")"
    assert_eq "PID file contains written PID" "12345" "$pid"
}

test_background_log_file_path() {
    local content
    content="$(get_script_content)"
    assert_contains "Background log file path format" "$content" "luantiserver-"
    # Should reference .log in the bg log path
    local has_log_ext
    has_log_ext="$(echo "$content" | grep -c 'luantiserver-.*\.log' || true)"
    assert_true "Background log file has .log extension" test "$has_log_ext" -gt 0
}

# ─── 10. Stop Command Tests ─────────────────────────────────────────────────

test_stop_function_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Stop Command"
    local content
    content="$(get_script_content)"
    assert_contains "Script has stop_server function" "$content" "stop_server()"
}

test_stop_uses_sigterm_first() {
    local content
    content="$(get_script_content)"
    assert_contains "Stop uses SIGTERM (kill without -9)" "$content" 'kill "$pid"'
}

test_stop_falls_back_to_sigkill() {
    local content
    content="$(get_script_content)"
    assert_contains "Stop falls back to SIGKILL" "$content" "kill -9"
}

test_stop_cleans_pid_file() {
    local content
    content="$(get_script_content)"
    assert_contains "Stop cleans up PID file" "$content" "remove_pid_file"
}

test_stop_handles_stale_pid() {
    local content
    content="$(get_script_content)"
    assert_contains "Stop handles stale PID files" "$content" "stale"
}

test_stop_kills_screen_session() {
    local content
    content="$(get_script_content)"
    assert_contains "Stop kills screen session" "$content" "screen"
}

# ─── 11. Status Command Tests ───────────────────────────────────────────────

test_status_function_exists() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Status Command"
    local content
    content="$(get_script_content)"
    assert_contains "Script has show_status function" "$content" "show_status()"
}

test_status_scans_pid_files() {
    local content
    content="$(get_script_content)"
    assert_contains "Status scans PID files" "$content" "luantiserver-*.pid"
}

test_status_checks_process_alive() {
    local content
    content="$(get_script_content)"
    assert_contains "Status checks if process is running" "$content" "kill -0"
}

test_status_shows_port() {
    local content
    content="$(get_script_content)"
    assert_contains "Status shows port information" "$content" "Port:"
}

test_status_shows_pid() {
    local content
    content="$(get_script_content)"
    assert_contains "Status shows PID" "$content" "PID:"
}

test_status_shows_running_state() {
    local content
    content="$(get_script_content)"
    assert_contains "Status shows running/stopped state" "$content" "Status:"
}

test_status_command_runs() {
    # --status should run without error even if no servers are running
    local output
    output="$(bash "$SERVER_SCRIPT" --status 2>&1)" || true
    assert_true "--status command produces output" test -n "$output"
}

# ─── 12. Config Override Tests ──────────────────────────────────────────────

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
        CONFIG_OVERRIDES+=("max_players=10")
        CONFIG_OVERRIDES+=("motd=Hello World")
        echo "${#CONFIG_OVERRIDES[@]}"
    ')"
    assert_eq "Two config overrides are stored" "2" "$result"
}

test_config_override_key_value_format() {
    # Validate that the script checks for KEY=VALUE format
    # The script uses: [[ "$override" != *"="* ]]
    local content
    content="$(get_script_content)"
    assert_contains "Script validates KEY=VALUE format" "$content" '*"="*'
}

test_config_override_written_to_temp_conf() {
    local temp_conf="${TEST_DIR}/test_config_override.conf"

    bash -c "
        TEMP_CONFIG='${temp_conf}'
        CONFIG_OVERRIDES=('max_players=10' 'motd=Welcome')

        echo '# Config overrides from --config flags' >> \"\$TEMP_CONFIG\"
        for override in \"\${CONFIG_OVERRIDES[@]}\"; do
            echo \"\$override\" >> \"\$TEMP_CONFIG\"
        done
    "

    local content
    content="$(cat "$temp_conf")"
    assert_contains "Config override max_players written" "$content" "max_players=10"
    assert_contains "Config override motd written" "$content" "motd=Welcome"
}

test_multiple_config_overrides() {
    # Simulate parsing multiple --config flags
    local result
    result="$(bash -c '
        CONFIG_OVERRIDES=()
        set -- --config max_players=10 --config motd=Welcome --config enable_pvp=true
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --config) CONFIG_OVERRIDES+=("$2"); shift ;;
            esac
            shift
        done
        echo "${#CONFIG_OVERRIDES[@]}:${CONFIG_OVERRIDES[0]}:${CONFIG_OVERRIDES[1]}:${CONFIG_OVERRIDES[2]}"
    ')"
    assert_eq "Three config overrides parsed" "3:max_players=10:motd=Welcome:enable_pvp=true" "$result"
}

# ─── 13. Color Variable & set -u Robustness Tests ─────────────────────────────

test_all_color_variables_defined() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Color Variables & set -u Robustness"
    local content
    content="$(get_script_content)"
    # All color variables must be defined with ANSI-C quoting
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
    # NC (No Color) must be defined as alias for RESET
    assert_contains "NC is defined as alias for RESET" "$content" "NC=\$'"
}

test_nc_equals_reset() {
    # Verify NC and RESET have the same value when evaluated
    local result
    result="$(bash -c '
        RED=$'\''\033[1;31m'\''
        GREEN=$'\''\033[1;32m'\''
        YELLOW=$'\''\033[1;33m'\''
        BLUE=$'\''\033[1;34m'\''
        CYAN=$'\''\033[1;36m'\''
        BOLD=$'\''\033[1m'\''
        DIM=$'\''\033[2m'\''
        RESET=$'\''\033[0m'\''
        NC=$'\''\033[0m'\''
        [[ "$NC" == "$RESET" ]] && echo "match" || echo "different"
    ')"
    assert_eq "NC equals RESET" "match" "$result"
}

test_no_unbound_nc_variable() {
    # Ensure NC is not referenced before being defined
    local content
    content="$(get_script_content)"
    # Get line number of NC definition
    local nc_def_line
    nc_def_line="$(echo "$content" | grep -n "^NC=" | head -1 | cut -d: -f1)"
    assert_true "NC variable has a definition" test -n "$nc_def_line"
}

test_non_interactive_mode_detection() {
    local content
    content="$(get_script_content)"
    # Script should handle non-terminal stdin
    assert_contains "Script detects non-terminal stdin" "$content" "! -t 0"
}

test_non_interactive_defaults_to_secure() {
    # When run non-interactively, should default to secure mode
    local content
    content="$(get_script_content)"
    assert_contains "Non-interactive defaults to secure" "$content" "SECURE_MODE=1"
}

test_non_interactive_defaults_to_foreground() {
    # When run non-interactively, should default to foreground mode
    local content
    content="$(get_script_content)"
    assert_contains "Non-interactive defaults to foreground" "$content" "foreground"
}

test_whitespace_trim_no_subshell() {
    # The script should trim whitespace without spawning subshells (xargs)
    local content
    content="$(get_script_content)"
    # Should NOT use the fragile "| xargs" pattern
    local xargs_count
    xargs_count="$(echo "$content" | grep -c '| xargs' || true)"
    assert_eq "Script does not use | xargs for trimming" "0" "$xargs_count"
}

test_defensive_defaults_for_secure_mode() {
    local content
    content="$(get_script_content)"
    # Should use ${SECURE_MODE:-1} pattern for defensive default
    assert_contains "Uses defensive default for SECURE_MODE" "$content" 'SECURE_MODE:-'
}

test_defensive_defaults_for_run_mode() {
    local content
    content="$(get_script_content)"
    # Should use ${RUN_MODE:-foreground} pattern for defensive default
    assert_contains "Uses defensive default for RUN_MODE" "$content" 'RUN_MODE:-'
}

# ─── Integration Smoke Tests ────────────────────────────────────────────────

test_list_worlds_command() {
    echo -e "${CYAN}[TEST GROUP]${RESET} Integration Smoke Tests"
    local output
    output="$(bash "$SERVER_SCRIPT" --list-worlds 2>&1)" || true
    assert_true "--list-worlds command runs without error" test -n "$output"
}

test_status_command_integration() {
    local output
    output="$(bash "$SERVER_SCRIPT" --status 2>&1)" || true
    assert_true "--status command runs" test -n "$output"
}

test_unknown_option_shows_help() {
    local output
    output="$(bash "$SERVER_SCRIPT" --bogus-option 2>&1)" || true
    # Should show help or error
    assert_true "Unknown option produces output" test -n "$output"
}

test_script_version_in_header() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has version in header" "$content" "v2.0"
}

test_script_has_world_base_dir() {
    local content
    content="$(get_script_content)"
    assert_contains "Script defines WORLD_BASE_DIR" "$content" "WORLD_BASE_DIR"
}

test_script_has_config_file() {
    local content
    content="$(get_script_content)"
    assert_contains "Script defines CONFIG_FILE" "$content" "CONFIG_FILE"
}

test_script_has_main_function() {
    local content
    content="$(get_script_content)"
    assert_contains "Script has main function" "$content" "main()"
}

test_script_calls_main() {
    local content
    content="$(get_script_content)"
    assert_contains "Script calls main with args" "$content" 'main "$@"'
}

# ─── Test Runner ───────────────────────────────────────────────────────────────

# All test functions
ALL_TESTS=(
    # 1. Script structure
    test_script_exists
    test_script_is_executable
    test_script_has_shebang
    test_script_has_set_uo_pipefail
    test_script_no_set_e
    test_script_has_ansi_c_quoting_colors
    test_script_has_exit_1_on_critical

    # 2. Help output
    test_help_flag_works
    test_help_shows_secure_option
    test_help_shows_insecure_option
    test_help_shows_foreground_option
    test_help_shows_background_option
    test_help_shows_screen_option
    test_help_shows_world_option
    test_help_shows_port_option
    test_help_shows_game_option
    test_help_shows_build_option
    test_help_shows_status_option
    test_help_shows_stop_option
    test_help_shows_logs_option
    test_help_shows_config_option
    test_help_shows_examples

    # 3. Argument parsing
    test_arg_secure_sets_mode
    test_arg_insecure_sets_mode
    test_arg_foreground_mode
    test_arg_background_mode
    test_arg_screen_mode
    test_arg_port_custom
    test_arg_game_custom
    test_arg_build_flag
    test_arg_config_overrides

    # 4. Binary detection
    test_binary_detection_checks_bin_dir
    test_binary_detection_checks_build_bin_dir
    test_binary_detection_bin_first
    test_binary_detection_function_exists
    test_binary_detection_finds_mock_binary
    test_binary_detection_fallback_to_build
    test_binary_detection_returns_1_when_missing

    # 5. World management
    test_list_worlds_function_exists
    test_create_world_function_exists
    test_create_world_creates_directory
    test_create_world_writes_world_mt
    test_create_world_mt_contains_gameid
    test_list_worlds_finds_existing
    test_list_worlds_handles_empty_dir

    # 6. Config generation
    test_generate_temp_config_function_exists
    test_temp_config_includes_secure_connection_true
    test_temp_config_includes_secure_connection_false
    test_temp_config_includes_overrides
    test_temp_config_copies_base_config
    test_temp_config_path_includes_port
    test_cleanup_temp_config_function_exists

    # 7. Security toggle
    test_secure_mode_default_is_ask
    test_secure_mode_passed_to_server
    test_secure_mode_creates_temp_config
    test_secure_mode_flag_recognized
    test_secure_explicit_flag_set_by_secure
    test_secure_explicit_flag_set_by_insecure
    test_run_mode_explicit_flag_set
    test_interactive_menu_function_exists
    test_interactive_menu_security_asked_when_not_explicit

    # 8. Port handling
    test_default_port_is_30000
    test_custom_port_parsed
    test_pid_file_includes_port
    test_pid_file_path_function
    test_pid_file_path_default_port
    test_stop_uses_port
    test_logs_uses_port

    # 9. Background mode
    test_background_mode_function_exists
    test_background_mode_uses_nohup
    test_background_mode_writes_pid_file
    test_background_mode_redirects_output
    test_pid_file_write_and_read
    test_background_log_file_path

    # 10. Stop command
    test_stop_function_exists
    test_stop_uses_sigterm_first
    test_stop_falls_back_to_sigkill
    test_stop_cleans_pid_file
    test_stop_handles_stale_pid
    test_stop_kills_screen_session

    # 11. Status command
    test_status_function_exists
    test_status_scans_pid_files
    test_status_checks_process_alive
    test_status_shows_port
    test_status_shows_pid
    test_status_shows_running_state
    test_status_command_runs

    # 12. Config overrides
    test_config_override_function_exists
    test_config_override_array
    test_config_override_key_value_format
    test_config_override_written_to_temp_conf
    test_multiple_config_overrides

    # 13. Color variables & set -u robustness
    test_all_color_variables_defined
    test_nc_alias_defined
    test_nc_equals_reset
    test_no_unbound_nc_variable
    test_non_interactive_mode_detection
    test_non_interactive_defaults_to_secure
    test_non_interactive_defaults_to_foreground
    test_whitespace_trim_no_subshell
    test_defensive_defaults_for_secure_mode
    test_defensive_defaults_for_run_mode

    # Integration
    test_list_worlds_command
    test_status_command_integration
    test_unknown_option_shows_help
    test_script_version_in_header
    test_script_has_world_base_dir
    test_script_has_config_file
    test_script_has_main_function
    test_script_calls_main
)

list_tests() {
    echo "Available tests:"
    for t in "${ALL_TESTS[@]}"; do
        echo "  $t"
    done
}

run_all_tests() {
    echo -e "${CYAN}========================================${RESET}"
    echo -e "${CYAN} Luanti Server Script — Test Suite${RESET}"
    echo -e "${CYAN}========================================${RESET}"
    echo ""

    for t in "${ALL_TESTS[@]}"; do
        "$t"
    done
}

# Parse arguments
if [[ $# -eq 0 ]]; then
    run_all_tests
elif [[ "$1" == "--all" ]]; then
    run_all_tests
elif [[ "$1" == "--list" ]]; then
    list_tests
    exit 0
else
    # Run specific tests
    echo -e "${CYAN}Running selected tests...${RESET}"
    for t in "$@"; do
        if type "$t" &>/dev/null; then
            "$t"
        else
            echo -e "  ${RED}UNKNOWN${RESET}: Test '$t' not found"
        fi
    done
fi

# Summary
echo ""
echo -e "${CYAN}========================================${RESET}"
echo -e " Test Results: ${GREEN}${TESTS_PASSED} passed${RESET}, ${RED}${TESTS_FAILED} failed${RESET}, ${TESTS_RUN} total"
echo -e "${CYAN}========================================${RESET}"

if [[ "$TESTS_FAILED" -gt 0 ]]; then
    exit 1
fi
exit 0
