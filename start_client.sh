#!/usr/bin/env bash
#
# Luanti Client Start Script v3.0
# Supports: interactive connection menu, security toggle, quick-connect,
#           player name/password, fullscreen/windowed, display settings
#
# Usage:
#   ./start_client.sh [OPTIONS]
#
# Options:
#   --address HOST         Server address to connect to (default: localhost)
#   --port PORT            Server port (default: 30000)
#   --name NAME            Player name
#   --password PASS        Player password (use --password-file for better security)
#   --password-file FILE   Read password from file
#   --secure               Enable secure/encrypted connection (skips security menu)
#   --insecure             Disable secure connection (skips security menu)
#   --log                  Enable full logging (default)
#   --no-log               Disable ALL logging — no debug.txt, no encryption trace
#                          Use this when logging is not needed to avoid large log files
#   --log-encryption LVL   Set encryption log level: none|error|action|trace
#   --go                   Skip main menu, connect directly to server
#   --build                Build client binary first if it doesn't exist
#   --fullscreen           Start in fullscreen mode
#   --windowed             Start in windowed mode
#   --resolution WxH       Window resolution (e.g. 1280x720, 1920x1080)
#   --config KEY=VALUE     Override a client setting (can be used multiple times)
#   --list-servers         List known servers from server list
#   --help                 Show this help message
#
# Examples:
#   ./start_client.sh                                    # Interactive menu
#   ./start_client.sh --go                               # Quick-connect to localhost:30000
#   ./start_client.sh --go --address 192.168.1.10        # Quick-connect to remote server
#   ./start_client.sh --secure --go                      # Secure direct connect
#   ./start_client.sh --insecure --go                    # Insecure direct connect
#   ./start_client.sh --name player1 --go                # With player name
#   ./start_client.sh --address localhost --port 30000 --name player1 --go --secure
#   ./start_client.sh --build --secure --go              # Build if needed, then connect
#   ./start_client.sh --go --fullscreen                  # Connect in fullscreen
#   ./start_client.sh --go --no-log                     # No logging (no debug.txt, no trace files)
#   ./start_client.sh --go --log-encryption trace       # Full encryption trace logging
#   ./start_client.sh --go --resolution 1920x1080        # Custom resolution
#

set -o pipefail

# --- Configuration ---
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_FILE="$HOME/.local/share/luanti/minetest.conf"

# --- Color definitions ---
RED=$'\033[1;31m'
GREEN=$'\033[1;32m'
YELLOW=$'\033[1;33m'
BLUE=$'\033[1;34m'
CYAN=$'\033[1;36m'
BOLD=$'\033[1m'
DIM=$'\033[2m'
RESET=$'\033[0m'

# --- Default Settings ---
SERVER_ADDRESS=""        # Empty = ask user or localhost
PORT=""                  # Empty = ask or default 30000
PLAYER_NAME=""           # Empty = ask user
PLAYER_PASSWORD=""       # Empty = no password
PASSWORD_FILE=""         # File to read password from
SECURE_MODE=-1           # -1 = not set (ask user), 1 = secure, 0 = insecure
SECURE_EXPLICIT=0        # 1 = user passed --secure or --insecure on CLI
ENC_LOG_LEVEL=""         # Empty = default (action), one of: none, error, action, trace
LOGGING_ENABLED=1        # 1 = full logging (default), 0 = no logging at all
DO_GO=-1                 # -1 = ask user, 1 = skip main menu, 0 = show main menu
DO_BUILD=0               # Whether to build before starting
DO_FULLSCREEN=-1         # -1 = ask user, 1 = fullscreen, 0 = windowed
RESOLUTION=""            # Empty = default
DO_LIST_SERVERS=0        # List known servers and exit
CONFIG_OVERRIDES=()      # Array of KEY=VALUE config overrides

# Temp config file
TEMP_CONFIG=""

# Client command array
CLIENT_CMD=()

# --- Logging Functions ---

log()   { echo -e "${GREEN}[CLIENT]${RESET} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET} $*" >&2; }
error() { echo -e "${RED}[ERROR]${RESET} $*" >&2; }
info()  { echo -e "${BLUE}[INFO]${RESET} $*"; }
step()  { echo -e "${CYAN}[STEP]${RESET} $*"; }

# --- Help ---

show_help() {
    sed -n '3,/^$/p' "$0" | sed 's/^# \?//'
    exit 0
}

# --- Argument Parsing ---

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --address)      SERVER_ADDRESS="${2:?--address requires a host}"; shift ;;
            --port)         PORT="${2:?--port requires a port number}"; shift ;;
            --name)         PLAYER_NAME="${2:?--name requires a player name}"; shift ;;
            --password)     PLAYER_PASSWORD="${2:?--password requires a value}"; shift ;;
            --password-file) PASSWORD_FILE="${2:?--password-file requires a file path}"; shift ;;
            --secure)       SECURE_MODE=1; SECURE_EXPLICIT=1 ;;
            --insecure)     SECURE_MODE=0; SECURE_EXPLICIT=1 ;;
            --log)          LOGGING_ENABLED=1 ;;
            --no-log)       LOGGING_ENABLED=0 ;;
            --log-encryption) ENC_LOG_LEVEL="${2:?--log-encryption requires none|error|action|trace}"; shift ;;
            --go)           DO_GO=1 ;;
            --build)        DO_BUILD=1 ;;
            --fullscreen)   DO_FULLSCREEN=1 ;;
            --windowed)     DO_FULLSCREEN=0 ;;
            --resolution)   RESOLUTION="${2:?--resolution requires WxH}"; shift ;;
            --config)       CONFIG_OVERRIDES+=("${2:?--config requires KEY=VALUE}"); shift ;;
            --list-servers) DO_LIST_SERVERS=1 ;;
            --help|-h)      show_help ;;
            *)              error "Unknown option: $1"; show_help ;;
        esac
        shift
    done
}

# --- Binary Detection ---

find_client_binary() {
    local -a search_paths=(
        "${SOURCE_DIR}/bin/luanti"
        "${SOURCE_DIR}/build/bin/luanti"
    )
    for p in "${search_paths[@]}"; do
        if [[ -x "$p" ]]; then
            echo "$p"
            return 0
        fi
    done
    return 1
}

# --- Build ---

build_client() {
    step "Building Luanti client binary..."
    if [[ ! -f "${SOURCE_DIR}/build_linux.sh" ]]; then
        error "build_linux.sh not found in ${SOURCE_DIR}"
        return 1
    fi
    bash "${SOURCE_DIR}/build_linux.sh" --client --run-in-place || {
        error "Build failed!"
        return 1
    }
    log "Build completed."
    return 0
}

# --- Server Discovery ---

list_servers() {
    step "Fetching public server list..."
    echo ""
    if ! command -v curl &>/dev/null; then
        error "'curl' is not installed. Install it to use --list-servers."
        return 1
    fi
    local json_data
    json_data="$(curl -s 'https://servers.luanti.org/list' 2>/dev/null)" || {
        error "Failed to fetch server list."
        return 1
    }
    if [[ -z "$json_data" ]]; then
        error "Empty response from server list API."
        return 1
    fi
    if command -v python3 &>/dev/null; then
        python3 -c "
import json, sys
try:
    data = json.loads('''${json_data}''')
    if isinstance(data, list):
        for i, srv in enumerate(data[:30], 1):
            addr = srv.get('address', '?')
            port = srv.get('port', 30000)
            name = srv.get('name', 'Unknown')
            clients = srv.get('clients', 0)
            max_c = srv.get('clients_max', '?')
            print(f'  {i:3d}) {name[:40]:<40s}  {addr}:{port}  [{clients}/{max_c}]')
    else:
        print('Unexpected data format')
except Exception as e:
    print(f'Parse error: {e}', file=sys.stderr)
    sys.exit(1)
" 2>/dev/null || {
            warn "Could not parse server list. Raw data: https://servers.luanti.org/list"
        }
    else
        info "Raw server list: https://servers.luanti.org/list"
    fi
    echo ""
    return 0
}

# --- Interactive Prompts ---

select_server_interactive() {
    echo -e "  ${BOLD}1. Server Address:${RESET}"
    echo ""
    echo -e "    ${CYAN}1)${RESET} localhost        - Connect to a local server on this machine"
    echo -e "    ${CYAN}2)${RESET} 127.0.0.1        - Same as localhost (explicit IP)"
    echo -e "    ${CYAN}3)${RESET} Custom address   - Enter a remote server address"
    echo ""

    while true; do
        local answer=""
        read -rp "  Select server address [1-3] (default: 1): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"

        case "$answer" in
            1|"")
                SERVER_ADDRESS="localhost"
                log "Server: localhost"
                break
                ;;
            2)
                SERVER_ADDRESS="127.0.0.1"
                log "Server: 127.0.0.1"
                break
                ;;
            3)
                read -rp "  Enter server address: " answer || true
                answer="${answer#"${answer%%[![:space:]]*}"}"
                answer="${answer%"${answer##*[![:space:]]}"}"
                if [[ -n "$answer" ]]; then
                    SERVER_ADDRESS="$answer"
                    log "Server: ${SERVER_ADDRESS}"
                    break
                else
                    echo -e "  ${RED}Address cannot be empty.${RESET}"
                fi
                ;;
            q|quit)
                info "Cancelled."; exit 0 ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter 1, 2, or 3.${RESET}" ;;
        esac
    done
    echo ""
}

select_port_interactive() {
    echo -e "  ${BOLD}2. Server Port:${RESET}"
    echo ""
    echo -e "    Default: ${CYAN}${PORT:-30000}${RESET}"
    echo ""

    local answer=""
    read -rp "  Enter port (default: ${PORT:-30000}): " answer || true
    answer="${answer#"${answer%%[![:space:]]*}"}"
    answer="${answer%"${answer##*[![:space:]]}"}"

    if [[ -n "$answer" ]] && [[ "$answer" =~ ^[0-9]+$ ]] && [[ "$answer" -ge 1 ]] && [[ "$answer" -le 65535 ]]; then
        PORT="$answer"
        log "Port: ${PORT}"
    elif [[ -z "$answer" ]]; then
        PORT="${PORT:-30000}"
        log "Port: ${PORT} (default)"
    else
        PORT="${PORT:-30000}"
        warn "Invalid port, using default: ${PORT}"
    fi
    echo ""
}

select_name_interactive() {
    # Check if name is already set in config
    local config_name=""
    if [[ -f "$CONFIG_FILE" ]]; then
        config_name="$(grep -E '^name\s*=' "$CONFIG_FILE" 2>/dev/null | head -1 | sed 's/^[^=]*=//' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//' || true)"
    fi

    echo -e "  ${BOLD}3. Player Name:${RESET}"
    echo ""
    if [[ -n "$config_name" ]]; then
        echo -e "    Default (from config): ${CYAN}${config_name}${RESET}"
    fi
    echo -e "    This is the name other players will see."
    echo ""

    local answer=""
    local prompt="  Enter player name"
    [[ -n "$config_name" ]] && prompt+=" (default: ${config_name})"
    prompt+=": "
    read -rp "$prompt" answer || true
    answer="${answer#"${answer%%[![:space:]]*}"}"
    answer="${answer%"${answer##*[![:space:]]}"}"

    if [[ -n "$answer" ]]; then
        PLAYER_NAME="$answer"
        log "Player name: ${PLAYER_NAME}"
    elif [[ -n "$config_name" ]]; then
        PLAYER_NAME="$config_name"
        log "Player name: ${PLAYER_NAME} (from config)"
    else
        PLAYER_NAME="player_${USER}"
        warn "No name specified, using: ${PLAYER_NAME}"
    fi
    echo ""
}

select_password_interactive() {
    echo -e "  ${BOLD}4. Password (optional):${RESET}"
    echo ""
    echo -e "    Required if the server has authentication enabled."
    echo -e "    Leave empty if the server doesn't require a password."
    echo ""

    local answer=""
    read -rp "  Enter password (leave empty for none): " answer || true
    # Don't trim password - it could have spaces
    if [[ -n "$answer" ]]; then
        PLAYER_PASSWORD="$answer"
        log "Password: ********"
    else
        info "No password set"
    fi
    echo ""
}

select_security_interactive() {
    echo -e "  ${BOLD}5. Security Mode:${RESET}"
    echo ""
    echo -e "    ${CYAN}1)${RESET} ${GREEN}Secure${RESET}       - Encrypted connection (recommended)"
    echo -e "    ${CYAN}2)${RESET} ${YELLOW}Insecure${RESET}     - Unencrypted (for testing / development)"
    echo ""

    while true; do
        local answer=""
        read -rp "  Select security mode [1-2] (default: 1): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"

        case "$answer" in
            1|"")
                SECURE_MODE=1
                log "Security: ${GREEN}secure${RESET} (encrypted connection)"
                break
                ;;
            2)
                SECURE_MODE=0
                warn "Security: ${YELLOW}insecure${RESET} (unencrypted - for testing only!)"
                break
                ;;
            q|quit)
                info "Cancelled."; exit 0 ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter 1 or 2.${RESET}" ;;
        esac
    done
    echo ""
}

select_logging_interactive() {
    echo -e "  ${BOLD}6. Logging Mode:${RESET}"
    echo ""
    echo -e "    ${CYAN}1)${RESET} ${GREEN}Logging ON${RESET}    - Full logging to debug.txt (default)"
    echo -e "    ${CYAN}2)${RESET} ${RED}Logging OFF${RESET}   - No debug.txt, no trace files (saves disk)"
    echo ""
    echo -e "    ${DIM}When OFF, no log data is generated at all. Use when logging is not needed.${RESET}"
    echo -e "    ${DIM}A previous session with logging ON generated 180MB+ of log data.${RESET}"
    echo ""

    while true; do
        local answer=""
        read -rp "  Select logging mode [1-2] (default: 1): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"

        case "$answer" in
            1|"")
                LOGGING_ENABLED=1
                log "Logging: ${GREEN}ON${RESET}"
                break
                ;;
            2)
                LOGGING_ENABLED=0
                log "Logging: ${RED}OFF${RESET} (no debug.txt, no trace files)"
                break
                ;;
            q|quit)
                info "Cancelled."; exit 0 ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter 1 or 2.${RESET}" ;;
        esac
    done
    echo ""
}

select_launch_mode_interactive() {
    echo -e "  ${BOLD}7. Launch Mode:${RESET}"
    echo ""
    echo -e "    ${CYAN}1)${RESET} Direct Connect  - Skip main menu, connect directly to server"
    echo -e "    ${CYAN}2)${RESET} Main Menu       - Open Luanti main menu first"
    echo ""

    while true; do
        local answer=""
        read -rp "  Select launch mode [1-2] (default: 1): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"

        case "$answer" in
            1|"")
                DO_GO=1
                log "Launch mode: direct connect"
                break
                ;;
            2)
                DO_GO=0
                log "Launch mode: main menu"
                break
                ;;
            q|quit)
                info "Cancelled."; exit 0 ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter 1 or 2.${RESET}" ;;
        esac
    done
    echo ""
}

select_display_interactive() {
    echo -e "  ${BOLD}8. Display Mode:${RESET}"
    echo ""
    echo -e "    ${CYAN}1)${RESET} Windowed      - Run in a window"
    echo -e "    ${CYAN}2)${RESET} Fullscreen    - Run in fullscreen"
    echo ""

    while true; do
        local answer=""
        read -rp "  Select display mode [1-2] (default: 1): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"

        case "$answer" in
            1|"")
                DO_FULLSCREEN=0
                log "Display: windowed"
                break
                ;;
            2)
                DO_FULLSCREEN=1
                log "Display: fullscreen"
                break
                ;;
            q|quit)
                info "Cancelled."; exit 0 ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter 1 or 2.${RESET}" ;;
        esac
    done
    echo ""

    # Resolution (only for windowed mode)
    if [[ "$DO_FULLSCREEN" -eq 0 ]] && [[ -z "$RESOLUTION" ]]; then
        echo -e "  ${BOLD}9. Window Resolution (optional):${RESET}"
        echo ""
        echo -e "    Common resolutions:"
        echo -e "      1280x720, 1366x768, 1600x900, 1920x1080, 2560x1440"
        echo -e "    Leave empty for default."
        echo ""

        local answer=""
        read -rp "  Resolution (default: auto): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"
        if [[ -n "$answer" ]]; then
            if [[ "$answer" =~ ^[0-9]+x[0-9]+$ ]]; then
                RESOLUTION="$answer"
                log "Resolution: ${RESOLUTION}"
            else
                warn "Invalid resolution format '${answer}'. Use WxH (e.g. 1920x1080). Using default."
            fi
        else
            info "Using default resolution"
        fi
        echo ""
    fi
}

# --- Config Generation ---

generate_temp_config() {
    TEMP_CONFIG="/tmp/luanti-client-$$.conf"

    # Start with the base config if it exists
    if [[ -f "$CONFIG_FILE" ]]; then
        cp "$CONFIG_FILE" "$TEMP_CONFIG" || {
            error "Failed to copy base config to temp file"
            return 1
        }
    else
        : > "$TEMP_CONFIG"
    fi

    # Secure connection
    if [[ "$SECURE_MODE" -eq 1 ]]; then
        echo "" >> "$TEMP_CONFIG"
        echo "# Added by start_client.sh" >> "$TEMP_CONFIG"
        echo "secure_connection = true" >> "$TEMP_CONFIG"
    elif [[ "$SECURE_MODE" -eq 0 ]]; then
        echo "" >> "$TEMP_CONFIG"
        echo "# Added by start_client.sh" >> "$TEMP_CONFIG"
        echo "secure_connection = false" >> "$TEMP_CONFIG"
    fi

    # v9.23: Master logging toggle
    if [[ "$LOGGING_ENABLED" -eq 0 ]]; then
        echo "" >> "$TEMP_CONFIG"
        echo "# v9.23: Logging disabled by --no-log" >> "$TEMP_CONFIG"
        echo "debug_log_level = " >> "$TEMP_CONFIG"
        echo "encryption_log_level = none" >> "$TEMP_CONFIG"
    else
        # v9.23: Encryption log level (only if explicitly set)
        if [[ -n "$ENC_LOG_LEVEL" ]]; then
            echo "encryption_log_level = ${ENC_LOG_LEVEL}" >> "$TEMP_CONFIG"
        fi
    fi

    # Player name
    if [[ -n "$PLAYER_NAME" ]]; then
        echo "name = ${PLAYER_NAME}" >> "$TEMP_CONFIG"
    fi

    # Fullscreen/windowed mode
    if [[ "$DO_FULLSCREEN" -eq 1 ]]; then
        echo "fullscreen = true" >> "$TEMP_CONFIG"
    elif [[ "$DO_FULLSCREEN" -eq 0 ]]; then
        echo "fullscreen = false" >> "$TEMP_CONFIG"
    fi

    # Resolution
    if [[ -n "$RESOLUTION" ]]; then
        local w h
        w="${RESOLUTION%%x*}"
        h="${RESOLUTION##*x}"
        echo "screen_w = ${w}" >> "$TEMP_CONFIG"
        echo "screen_h = ${h}" >> "$TEMP_CONFIG"
    fi

    # Config overrides
    if [[ ${#CONFIG_OVERRIDES[@]} -gt 0 ]]; then
        echo "" >> "$TEMP_CONFIG"
        echo "# Config overrides from --config flags" >> "$TEMP_CONFIG"
        for override in "${CONFIG_OVERRIDES[@]}"; do
            if [[ "$override" != *"="* ]]; then
                error "Invalid --config format: '${override}'. Expected KEY=VALUE"
                cleanup_temp_config
                return 1
            fi
            echo "$override" >> "$TEMP_CONFIG"
        done
    fi

    return 0
}

cleanup_temp_config() {
    if [[ -n "${TEMP_CONFIG:-}" ]] && [[ -f "${TEMP_CONFIG}" ]]; then
        rm -f "$TEMP_CONFIG"
        TEMP_CONFIG=""
    fi
}

# --- Build Client Command ---

build_client_cmd() {
    local binary="${1:-}"
    CLIENT_CMD=("$binary")

    # Server address
    if [[ -n "$SERVER_ADDRESS" ]]; then
        CLIENT_CMD+=("--address" "$SERVER_ADDRESS")
    fi

    # Port
    CLIENT_CMD+=("--port" "${PORT:-30000}")

    # Player name
    if [[ -n "$PLAYER_NAME" ]]; then
        CLIENT_CMD+=("--name" "$PLAYER_NAME")
    fi

    # Player password
    if [[ -n "$PLAYER_PASSWORD" ]]; then
        CLIENT_CMD+=("--password" "$PLAYER_PASSWORD")
    fi

    # Password file
    if [[ -n "$PASSWORD_FILE" ]]; then
        if [[ -f "$PASSWORD_FILE" ]]; then
            CLIENT_CMD+=("--password-file" "$PASSWORD_FILE")
        else
            warn "Password file not found: ${PASSWORD_FILE}"
        fi
    fi

    # Config file
    if [[ -n "${TEMP_CONFIG:-}" ]]; then
        CLIENT_CMD+=("--config" "$TEMP_CONFIG")
    fi

    # Skip main menu (go directly)
    if [[ "${DO_GO:-0}" -eq 1 ]]; then
        CLIENT_CMD+=("--go")
    fi

    return 0
}

# --- Interactive Start Menu ---

show_start_menu() {
    if [[ ! -t 0 ]]; then
        [[ "${SERVER_ADDRESS:-}" == "" ]] && SERVER_ADDRESS="localhost"
        [[ "$SECURE_MODE" -eq -1 ]] && SECURE_MODE=1
        [[ "$DO_GO" -eq -1 ]] && DO_GO=1
        [[ -z "$PORT" ]] && PORT=30000
        log "Non-interactive mode: address=${SERVER_ADDRESS}, secure=${SECURE_MODE}, go=1"
        return 0
    fi

    echo ""
    echo -e "${BOLD}+==================================================+${RESET}"
    echo -e "${BOLD}|        Luanti Client - Connection Setup           |${RESET}"
    echo -e "${BOLD}+==================================================+${RESET}"
    echo ""

    # ── Server address ──
    if [[ -z "$SERVER_ADDRESS" ]]; then
        select_server_interactive
    else
        log "Server address: ${SERVER_ADDRESS} (from command line)"
        echo ""
    fi

    # ── Port ──
    if [[ -z "$PORT" ]]; then
        select_port_interactive
    else
        log "Port: ${PORT} (from command line)"
        echo ""
    fi

    # ── Player name ──
    if [[ -z "$PLAYER_NAME" ]]; then
        select_name_interactive
    else
        log "Player name: ${PLAYER_NAME} (from command line)"
        echo ""
    fi

    # ── Password ──
    if [[ -z "$PLAYER_PASSWORD" ]] && [[ -z "$PASSWORD_FILE" ]]; then
        select_password_interactive
    fi

    # ── Security mode ──
    if [[ "$SECURE_EXPLICIT" -eq 0 ]]; then
        select_security_interactive
    else
        log "Security: $([ "$SECURE_MODE" -eq 1 ] && echo "secure" || echo "insecure") (from command line)"
        echo ""
    fi

    # ── Logging mode ──
    select_logging_interactive

    # ── Direct connect or main menu ──
    if [[ "$DO_GO" -eq -1 ]]; then
        select_launch_mode_interactive
    else
        log "Launch mode: $([ "$DO_GO" -eq 1 ] && echo "direct connect" || echo "main menu") (from command line)"
        echo ""
    fi

    # ── Display mode ──
    if [[ "$DO_FULLSCREEN" -eq -1 ]]; then
        select_display_interactive
    else
        log "Display: $([ "$DO_FULLSCREEN" -eq 1 ] && echo "fullscreen" || echo "windowed") (from command line)"
        echo ""
    fi
}

# --- Main ---

main() {
    parse_args "$@"

    # Handle standalone commands
    if [[ "$DO_LIST_SERVERS" -eq 1 ]]; then
        list_servers || exit 1
        exit 0
    fi

    # Interactive start menu
    show_start_menu

    # Apply defaults for any values still unset
    SERVER_ADDRESS="${SERVER_ADDRESS:-localhost}"
    SECURE_MODE="${SECURE_MODE:-1}"
    [[ "$SECURE_MODE" -eq -1 ]] && SECURE_MODE=1
    DO_GO="${DO_GO:-1}"
    [[ "$DO_GO" -eq -1 ]] && DO_GO=1
    PORT="${PORT:-30000}"
    DO_FULLSCREEN="${DO_FULLSCREEN:-0}"
    [[ "$DO_FULLSCREEN" -eq -1 ]] && DO_FULLSCREEN=0

    # Find or build the client binary
    local client_binary=""
    client_binary="$(find_client_binary)" || true

    if [[ -z "$client_binary" ]]; then
        if [[ "$DO_BUILD" -eq 1 ]]; then
            build_client || exit 1
            client_binary="$(find_client_binary)" || {
                error "Client binary still not found after build."
                exit 1
            }
        else
            error "Luanti client binary not found."
            error "Searched: ${SOURCE_DIR}/bin/luanti"
            error "          ${SOURCE_DIR}/build/bin/luanti"
            error "Build it first with: ./build_linux.sh --client --run-in-place"
            error "Or use --build to auto-build."
            exit 1
        fi
    fi

    log "Client binary: ${client_binary}"

    # Generate temp config
    generate_temp_config || exit 1

    # Build client command
    build_client_cmd "$client_binary" || exit 1

    # Display startup banner
    echo ""
    echo -e "${BOLD}+==================================================+${RESET}"
    echo -e "${BOLD}|         Luanti Client Start Script v3.0           |${RESET}"
    echo -e "${BOLD}+==================================================+${RESET}"
    echo ""
    echo -e "  ${CYAN}Binary:${RESET}     ${client_binary}"
    echo -e "  ${CYAN}Server:${RESET}     ${SERVER_ADDRESS}:${PORT}"
    echo -e "  ${CYAN}Player:${RESET}     ${PLAYER_NAME:-<not set>}"
    echo -e "  ${CYAN}Password:${RESET}   $([ -n "$PLAYER_PASSWORD" ] && echo "********" || echo "<none>")"
    echo -e "  ${CYAN}Security:${RESET}   $([ "$SECURE_MODE" -eq 1 ] && echo "${GREEN}secure${RESET}" || echo "${YELLOW}insecure${RESET}")"
    if [[ "$LOGGING_ENABLED" -eq 0 ]]; then
        echo -e "  ${CYAN}Logging:${RESET}    ${RED}OFF${RESET} (no debug.txt, no trace files)"
    else
        local enc_log_desc="${ENC_LOG_LEVEL:-action (default)}"
        echo -e "  ${CYAN}Logging:${RESET}    ${GREEN}ON${RESET} (enc level: ${enc_log_desc})"
    fi
    echo -e "  ${CYAN}Launch:${RESET}     $([ "$DO_GO" -eq 1 ] && echo "direct connect" || echo "main menu")"
    echo -e "  ${CYAN}Display:${RESET}    $([ "$DO_FULLSCREEN" -eq 1 ] && echo "fullscreen" || echo "windowed")"
    [[ -n "$RESOLUTION" ]] && echo -e "  ${CYAN}Resolution:${RESET}  ${RESOLUTION}"
    [[ -n "${TEMP_CONFIG:-}" ]] && echo -e "  ${CYAN}Conf file:${RESET}  ${TEMP_CONFIG}"
    echo ""

    # Run the client
    trap cleanup_temp_config EXIT INT TERM

    log "Starting Luanti client..."
    local exit_code=0
    "${CLIENT_CMD[@]}" || exit_code=$?

    echo ""
    if [[ "$exit_code" -ne 0 ]]; then
        error "Client exited with code ${exit_code}."
    fi

    cleanup_temp_config

    # Pause so the user can see the output before terminal closes
    if [[ -t 0 ]] && [[ "$exit_code" -ne 0 ]]; then
        echo ""
        read -rp "Press Enter to close this window..." _ || true
    fi

    return "$exit_code"
}

main "$@"
