#!/usr/bin/env bash
#
# Luanti Server Start Script v3.0
# Supports: interactive start menu, security toggle, world management,
#           foreground/background/screen modes, game selection, admin config
#
# Usage:
#   ./start_server.sh [OPTIONS]
#
# Options:
#   --secure               Enable secure/encrypted connection (skips security menu)
#   --insecure             Disable secure connection (skips security menu)
#   --foreground           Run server in foreground (skips run mode menu)
#   --background           Run server as daemon (nohup, skips run mode menu)
#   --screen               Run server in a screen session (skips run mode menu)
#   --world NAME           Select or create a world by name
#   --list-worlds          List available worlds and exit
#   --create-world NAME    Create a new world and exit
#   --port PORT            Server port (default: 30000)
#   --game GAMEID          Game ID to use (default: devtest)
#   --admin NAME           Server admin player name
#   --motd MSG             Message of the day
#   --max-players N        Maximum number of players (default: 15)
#   --build                Build server binary first if it doesn't exist
#   --status               Show running Luanti server processes
#   --stop [PORT]          Stop a running server (default port: 30000)
#   --logs [PORT]          Tail server logs (default port: 30000)
#   --config KEY=VALUE     Override a server setting (can be used multiple times)
#   --help                 Show this help message
#
# Examples:
#   ./start_server.sh                                    # Interactive menu
#   ./start_server.sh --secure                           # Skip menu, start secure server
#   ./start_server.sh --insecure                         # Skip menu, start insecure server
#   ./start_server.sh --secure --port 30001              # Secure server on port 30001
#   ./start_server.sh --insecure --background            # Insecure server in background
#   ./start_server.sh --secure --screen --world myworld  # Secure server in screen session
#   ./start_server.sh --list-worlds                      # List available worlds
#   ./start_server.sh --create-world test_world          # Create a new world
#   ./start_server.sh --admin admin --max-players 20     # With admin and player limit
#   ./start_server.sh --status                           # Show running servers
#   ./start_server.sh --stop 30000                       # Stop server on port 30000
#

set -o pipefail

# --- Configuration ---
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
WORLD_BASE_DIR="$SOURCE_DIR/worlds"
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
SECURE_MODE=-1          # -1 = not set (ask user), 1 = secure, 0 = insecure
RUN_MODE=""             # "" = not set (ask user), foreground | background | screen
SECURE_EXPLICIT=0       # 1 = user passed --secure or --insecure on CLI
RUN_MODE_EXPLICIT=0     # 1 = user passed --foreground/--background/--screen on CLI
WORLD_NAME=""           # World name (empty = auto or prompt)
PORT=""                 # Empty = ask or default 30000
GAME_ID=""              # Empty = ask or default devtest
ADMIN_NAME=""           # Server admin player name
MOTD=""                 # Message of the day
MAX_PLAYERS=""          # Max players
DO_BUILD=0              # Whether to build before starting
DO_STATUS=0             # Show status and exit
DO_STOP=0               # Stop a running server
DO_LOGS=0               # Tail server logs
DO_LIST_WORLDS=0        # List worlds and exit
CREATE_WORLD=""         # World name to create (empty = don't create)
CONFIG_OVERRIDES=()     # Array of KEY=VALUE config overrides
STOP_PORT=""            # Port for --stop
LOGS_PORT=""            # Port for --logs

# Temp config file
TEMP_CONFIG=""

# Server command array
SERVER_CMD=()

# --- Logging Functions ---

log()   { echo -e "${GREEN}[SERVER]${RESET} $*"; }
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
            --secure)       SECURE_MODE=1; SECURE_EXPLICIT=1 ;;
            --insecure)     SECURE_MODE=0; SECURE_EXPLICIT=1 ;;
            --foreground)   RUN_MODE="foreground"; RUN_MODE_EXPLICIT=1 ;;
            --background)   RUN_MODE="background"; RUN_MODE_EXPLICIT=1 ;;
            --screen)       RUN_MODE="screen"; RUN_MODE_EXPLICIT=1 ;;
            --world)        WORLD_NAME="${2:?--world requires a name}"; shift ;;
            --list-worlds)  DO_LIST_WORLDS=1 ;;
            --create-world) CREATE_WORLD="${2:?--create-world requires a name}"; shift ;;
            --port)         PORT="${2:?--port requires a port number}"; shift ;;
            --game)         GAME_ID="${2:?--game requires a game ID}"; shift ;;
            --admin)        ADMIN_NAME="${2:?--admin requires a name}"; shift ;;
            --motd)         MOTD="${2:?--motd requires a message}"; shift ;;
            --max-players)  MAX_PLAYERS="${2:?--max-players requires a number}"; shift ;;
            --build)        DO_BUILD=1 ;;
            --status)       DO_STATUS=1 ;;
            --stop)         DO_STOP=1; STOP_PORT="${2:-}"; [[ "${STOP_PORT}" == --* ]] && STOP_PORT="" || { [[ -n "$STOP_PORT" ]] && shift; } ;;
            --logs)         DO_LOGS=1; LOGS_PORT="${2:-}"; [[ "${LOGS_PORT}" == --* ]] && LOGS_PORT="" || { [[ -n "$LOGS_PORT" ]] && shift; } ;;
            --config)       CONFIG_OVERRIDES+=("${2:?--config requires KEY=VALUE}"); shift ;;
            --help|-h)      show_help ;;
            *)              error "Unknown option: $1"; show_help ;;
        esac
        shift
    done

    STOP_PORT="${STOP_PORT:-${PORT:-30000}}"
    LOGS_PORT="${LOGS_PORT:-${PORT:-30000}}"
}

# --- Binary Detection ---

find_server_binary() {
    local -a search_paths=(
        "${SOURCE_DIR}/bin/luantiserver"
        "${SOURCE_DIR}/build/bin/luantiserver"
    )
    for p in "${search_paths[@]}"; do
        if [[ -x "$p" ]]; then
            echo "$p"
            return 0
        fi
    done
    return 1
}

# --- Available Games ---

list_available_games() {
    local games_dir="${SOURCE_DIR}/games"
    local -a games=()
    if [[ -d "$games_dir" ]]; then
        for g in "$games_dir"/*/; do
            [[ -d "$g" ]] || continue
            games+=("$(basename "$g")")
        done
    fi
    echo "${games[@]}"
}

# --- Build ---

build_server() {
    step "Building Luanti server binary..."
    if [[ ! -f "${SOURCE_DIR}/build_linux.sh" ]]; then
        error "build_linux.sh not found in ${SOURCE_DIR}"
        return 1
    fi
    bash "${SOURCE_DIR}/build_linux.sh" --server --run-in-place || {
        error "Build failed!"
        return 1
    }
    log "Build completed."
    return 0
}

# --- World Management ---

list_worlds() {
    step "Available worlds:"
    echo ""
    if [[ ! -d "$WORLD_BASE_DIR" ]]; then
        info "No worlds directory found at ${WORLD_BASE_DIR}"
        info "Create one with: $0 --create-world <NAME>"
        return 0
    fi
    local found=0
    for world_dir in "$WORLD_BASE_DIR"/*/; do
        [[ -d "$world_dir" ]] || continue
        local wname
        wname="$(basename "$world_dir")"
        local has_meta="no"
        [[ -f "${world_dir}/world.mt" ]] && has_meta="yes"
        printf "  ${GREEN}%-30s${RESET}  world.mt: %s\n" "$wname" "$has_meta"
        found=1
    done
    if [[ "$found" -eq 0 ]]; then
        info "No worlds found in ${WORLD_BASE_DIR}"
    fi
    echo ""
    return 0
}

create_world() {
    local name="${1:-}"
    if [[ -z "$name" ]]; then
        error "World name is required"
        return 1
    fi
    local world_dir="${WORLD_BASE_DIR}/${name}"
    if [[ -d "$world_dir" ]]; then
        warn "World '${name}' already exists at ${world_dir}"
        return 0
    fi
    step "Creating world '${name}'..."
    mkdir -p "$world_dir" || {
        error "Failed to create world directory: ${world_dir}"
        return 1
    }
    local gid="${GAME_ID:-devtest}"
    cat > "${world_dir}/world.mt" <<EOF
gameid = ${gid}
backend = sqlite3
creative_mode = false
enable_damage = true
auth_backend = sqlite3
player_backend = sqlite3
mod_storage_backend = sqlite3
world_name = ${name}
EOF
    log "World '${name}' created at ${world_dir}"
    return 0
}

select_world_interactive() {
    if [[ ! -d "$WORLD_BASE_DIR" ]]; then
        mkdir -p "$WORLD_BASE_DIR"
    fi

    local -a worlds=()
    for world_dir in "$WORLD_BASE_DIR"/*/; do
        [[ -d "$world_dir" ]] || continue
        worlds+=("$(basename "$world_dir")")
    done

    if [[ ${#worlds[@]} -eq 0 ]]; then
        info "No worlds found. Creating default world..."
        WORLD_NAME="world"
        create_world "$WORLD_NAME" || return 1
        return 0
    fi

    if [[ ${#worlds[@]} -eq 1 ]]; then
        WORLD_NAME="${worlds[0]}"
        info "Only one world found, using: ${WORLD_NAME}"
        return 0
    fi

    echo ""
    echo -e "${BOLD}  Available worlds:${RESET}"
    for i in "${!worlds[@]}"; do
        printf "    %s%d%s) %s\n" "${CYAN}" "$((i + 1))" "${RESET}" "${worlds[$i]}"
    done
    echo ""

    local choice=""
    read -rp "  Select world [1-${#worlds[@]}] (default: 1): " choice || true
    choice="${choice#"${choice%%[![:space:]]*}"}"
    choice="${choice%"${choice##*[![:space:]]}"}"

    if [[ -z "$choice" ]]; then
        WORLD_NAME="${worlds[0]}"
    elif [[ "$choice" =~ ^[0-9]+$ ]] && [[ "$choice" -ge 1 ]] && [[ "$choice" -le "${#worlds[@]}" ]]; then
        WORLD_NAME="${worlds[$((choice - 1))]}"
    else
        error "Invalid selection."
        return 1
    fi
    log "Selected world: ${WORLD_NAME}"
    return 0
}

# --- Config Generation ---

generate_temp_config() {
    TEMP_CONFIG="/tmp/luanti-server-${PORT:-30000}-$$.conf"

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
        echo "# Added by start_server.sh" >> "$TEMP_CONFIG"
        echo "secure_connection = true" >> "$TEMP_CONFIG"
    else
        echo "" >> "$TEMP_CONFIG"
        echo "# Added by start_server.sh" >> "$TEMP_CONFIG"
        echo "secure_connection = false" >> "$TEMP_CONFIG"
    fi

    # Admin name
    if [[ -n "$ADMIN_NAME" ]]; then
        echo "name = ${ADMIN_NAME}" >> "$TEMP_CONFIG"
    fi

    # MOTD
    if [[ -n "$MOTD" ]]; then
        echo "motd = ${MOTD}" >> "$TEMP_CONFIG"
    fi

    # Max players
    if [[ -n "$MAX_PLAYERS" ]]; then
        echo "max_users = ${MAX_PLAYERS}" >> "$TEMP_CONFIG"
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
    if [[ -n "$TEMP_CONFIG" ]] && [[ -f "$TEMP_CONFIG" ]]; then
        rm -f "$TEMP_CONFIG"
        TEMP_CONFIG=""
    fi
}

# --- PID File Management ---

pid_file_path() {
    local port="${1:-${PORT:-30000}}"
    echo "/tmp/luantiserver-${port}.pid"
}

write_pid_file() {
    local pid="${1:-}"
    local port="${2:-${PORT:-30000}}"
    echo "$pid" > "$(pid_file_path "$port")" || {
        error "Failed to write PID file"
        return 1
    }
}

read_pid_file() {
    local port="${1:-${PORT:-30000}}"
    local pfile
    pfile="$(pid_file_path "$port")"
    if [[ -f "$pfile" ]]; then
        cat "$pfile"
        return 0
    fi
    return 1
}

remove_pid_file() {
    local port="${1:-${PORT:-30000}}"
    rm -f "$(pid_file_path "$port")"
}

# --- Status Command ---

show_status() {
    step "Luanti Server Status"
    echo ""
    local found=0
    for pfile in /tmp/luantiserver-*.pid; do
        [[ -f "$pfile" ]] || continue
        found=1
        local pid
        pid="$(cat "$pfile" 2>/dev/null || echo "unknown")"
        local port
        port="$(basename "$pfile" .pid | sed 's/luantiserver-//')"
        local status="stopped"
        if [[ "$pid" != "unknown" ]] && kill -0 "$pid" 2>/dev/null; then
            status="running"
        fi
        local status_color="$GREEN"
        [[ "$status" == "stopped" ]] && status_color="$RED"
        printf "  Port: %s%s%s\n" "${CYAN}" "$port" "${RESET}"
        printf "  PID:  %s\n" "$pid"
        printf "  Status: %s%s%s\n" "$status_color" "$status" "${RESET}"
        echo ""
    done
    if [[ "$found" -eq 0 ]]; then
        info "No Luanti server processes found."
    fi
    return 0
}

# --- Stop Command ---

stop_server() {
    local port="${1:-$STOP_PORT}"
    local pid
    pid="$(read_pid_file "$port")" || true
    if [[ -z "$pid" ]]; then
        pid="$(pgrep -f "luantiserver.*--port ${port}" 2>/dev/null | head -1 || true)"
    fi
    if [[ -z "$pid" ]]; then
        warn "No server found on port ${port}"
        return 1
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        warn "Process ${pid} is not running. Cleaning up stale PID file."
        remove_pid_file "$port"
        return 0
    fi
    step "Stopping Luanti server on port ${port} (PID: ${pid})..."
    kill "$pid" 2>/dev/null || {
        error "Failed to send SIGTERM to PID ${pid}"
        return 1
    }
    local waited=0
    while kill -0 "$pid" 2>/dev/null && [[ "$waited" -lt 10 ]]; do
        sleep 1
        ((waited++)) || true
    done
    if kill -0 "$pid" 2>/dev/null; then
        warn "Server did not shut down gracefully, sending SIGKILL..."
        kill -9 "$pid" 2>/dev/null || true
    fi
    remove_pid_file "$port"
    log "Server on port ${port} stopped."
    return 0
}

# --- Logs Command ---

tail_logs() {
    local port="${1:-$LOGS_PORT}"
    local bg_log="/tmp/luantiserver-${port}.log"
    if [[ -f "$bg_log" ]]; then
        step "Tailing background log: ${bg_log}"
        tail -f "$bg_log"
        return 0
    fi
    local debug_log="$HOME/.local/share/luanti/debug.txt"
    if [[ -f "$debug_log" ]]; then
        step "Tailing server log: ${debug_log}"
        tail -f "$debug_log"
        return 0
    fi
    error "No log file found."
    return 1
}

# --- Build Server Command ---

build_server_cmd() {
    local binary="${1:-}"
    SERVER_CMD=("$binary")
    SERVER_CMD+=("--port" "${PORT:-30000}")
    SERVER_CMD+=("--gameid" "${GAME_ID:-devtest}")

    if [[ -n "$WORLD_NAME" ]]; then
        SERVER_CMD+=("--world" "${WORLD_BASE_DIR}/${WORLD_NAME}")
    fi

    if [[ -n "$TEMP_CONFIG" ]]; then
        SERVER_CMD+=("--config" "$TEMP_CONFIG")
    fi

    return 0
}

# --- Run Modes ---

run_foreground() {
    local -a cmd=("${SERVER_CMD[@]}")
    log "Starting Luanti server in foreground..."
    info "  Binary:  ${cmd[0]}"
    info "  Port:    ${PORT:-30000}"
    info "  Game:    ${GAME_ID:-devtest}"
    info "  World:   ${WORLD_NAME:-<auto>}"
    info "  Secure:  $([ "$SECURE_MODE" -eq 1 ] && echo "yes" || echo "no")"
    [[ -n "$ADMIN_NAME" ]] && info "  Admin:   ${ADMIN_NAME}"
    [[ -n "$MAX_PLAYERS" ]] && info "  Max players: ${MAX_PLAYERS}"
    echo ""
    info "Press Ctrl+C to stop the server."
    echo ""

    # Trap cleanup for foreground mode
    trap cleanup_temp_config EXIT INT TERM

    local exit_code=0
    "${cmd[@]}" || exit_code=$?

    echo ""
    if [[ "$exit_code" -eq 0 ]]; then
        log "Server shut down normally."
    else
        error "Server exited with error code ${exit_code}."
    fi

    cleanup_temp_config

    # Pause so the user can see the output before terminal closes
    if [[ -t 0 ]]; then
        echo ""
        read -rp "Press Enter to close this window..." _ || true
    fi

    return "$exit_code"
}

run_background() {
    local -a cmd=("${SERVER_CMD[@]}")
    local bg_log="/tmp/luantiserver-${PORT:-30000}.log"

    log "Starting Luanti server in background..."
    info "  Binary:   ${cmd[0]}"
    info "  Port:     ${PORT:-30000}"
    info "  Game:     ${GAME_ID:-devtest}"
    info "  World:    ${WORLD_NAME:-<auto>}"
    info "  Secure:   $([ "$SECURE_MODE" -eq 1 ] && echo "yes" || echo "no")"
    info "  Log:      ${bg_log}"
    info "  PID file: $(pid_file_path)"
    echo ""

    nohup "${cmd[@]}" > "$bg_log" 2>&1 &
    local bg_pid=$!
    sleep 1

    if ! kill -0 "$bg_pid" 2>/dev/null; then
        error "Server failed to start. Check log: ${bg_log}"
        cleanup_temp_config
        return 1
    fi

    write_pid_file "$bg_pid" || true
    log "Server started in background (PID: ${bg_pid})"
    info "  View logs:  $0 --logs ${PORT:-30000}"
    info "  Stop server: $0 --stop ${PORT:-30000}"
    return 0
}

run_screen() {
    local -a cmd=("${SERVER_CMD[@]}")
    local sname="luanti-${PORT:-30000}"

    if ! command -v screen &>/dev/null; then
        error "'screen' is not installed. Install it with: sudo apt install screen"
        return 1
    fi

    log "Starting Luanti server in screen session '${sname}'..."
    info "  Binary:  ${cmd[0]}"
    info "  Port:    ${PORT:-30000}"
    info "  Game:    ${GAME_ID:-devtest}"
    info "  World:   ${WORLD_NAME:-<auto>}"
    info "  Secure:  $([ "$SECURE_MODE" -eq 1 ] && echo "yes" || echo "no")"
    echo ""

    if screen -ls 2>/dev/null | grep -q "$sname"; then
        error "Screen session '${sname}' already exists."
        error "Attach to it with: screen -r ${sname}"
        return 1
    fi

    screen -dmS "$sname" "${cmd[@]}" || {
        error "Failed to start screen session."
        cleanup_temp_config
        return 1
    }

    sleep 1
    log "Server started in screen session '${sname}'"
    info "  Attach:     screen -r ${sname}"
    info "  Detach:     Ctrl+A, D"
    info "  Stop:       $0 --stop ${PORT:-30000}"
    return 0
}

# --- Interactive Start Menu ---

show_start_menu() {
    if [[ ! -t 0 ]]; then
        [[ "$SECURE_MODE" -eq -1 ]] && SECURE_MODE=1
        [[ -z "$RUN_MODE" ]] && RUN_MODE="foreground"
        [[ -z "$PORT" ]] && PORT=30000
        [[ -z "$GAME_ID" ]] && GAME_ID="devtest"
        log "Non-interactive mode: secure=${SECURE_MODE}, run=${RUN_MODE}"
        return 0
    fi

    echo ""
    echo -e "${BOLD}+==================================================+${RESET}"
    echo -e "${BOLD}|        Luanti Server - Start Configuration        |${RESET}"
    echo -e "${BOLD}+==================================================+${RESET}"
    echo ""

    # ── Security mode ──
    if [[ "$SECURE_EXPLICIT" -eq 0 ]]; then
        echo -e "  ${BOLD}1. Security Mode:${RESET}"
        echo ""
        echo -e "    ${CYAN}1)${RESET} ${GREEN}Secure${RESET}       - Encrypted connections (recommended)"
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
                    log "Security: ${GREEN}secure${RESET} (encrypted connections)"
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
    fi

    # ── Port ──
    if [[ -z "$PORT" ]]; then
        echo -e "  ${BOLD}2. Server Port:${RESET}"
        echo ""
        echo -e "    Default: ${CYAN}30000${RESET}"
        echo ""
        local answer=""
        read -rp "  Enter port (default: 30000): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"
        if [[ -n "$answer" ]] && [[ "$answer" =~ ^[0-9]+$ ]] && [[ "$answer" -ge 1 ]] && [[ "$answer" -le 65535 ]]; then
            PORT="$answer"
        else
            PORT=30000
        fi
        log "Port: ${PORT}"
        echo ""
    fi

    # ── Game selection ──
    if [[ -z "$GAME_ID" ]]; then
        local -a games=()
        read -ra games <<< "$(list_available_games)"

        echo -e "  ${BOLD}3. Game:${RESET}"
        echo ""
        for i in "${!games[@]}"; do
            printf "    %s%d%s) %s\n" "${CYAN}" "$((i + 1))" "${RESET}" "${games[$i]}"
        done
        if [[ ${#games[@]} -eq 0 ]]; then
            echo -e "    ${YELLOW}No games found in games/ directory${RESET}"
        fi
        echo ""

        local answer=""
        read -rp "  Select game [1-${#games[@]}] (default: 1): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"

        if [[ -z "$answer" ]] && [[ ${#games[@]} -gt 0 ]]; then
            GAME_ID="${games[0]}"
        elif [[ "$answer" =~ ^[0-9]+$ ]] && [[ "$answer" -ge 1 ]] && [[ "$answer" -le "${#games[@]}" ]]; then
            GAME_ID="${games[$((answer - 1))]}"
        elif [[ ${#games[@]} -gt 0 ]]; then
            GAME_ID="${games[0]}"
            warn "Invalid selection, using: ${GAME_ID}"
        else
            GAME_ID="devtest"
            warn "No games found, using: devtest"
        fi
        log "Game: ${GAME_ID}"
        echo ""
    fi

    # ── World selection ──
    if [[ -z "$WORLD_NAME" ]]; then
        echo -e "  ${BOLD}4. World:${RESET}"
        echo ""
        select_world_interactive || exit 1
        echo ""
    fi

    # ── Admin name ──
    if [[ -z "$ADMIN_NAME" ]]; then
        echo -e "  ${BOLD}5. Admin Player Name (optional):${RESET}"
        echo ""
        echo -e "    This player will have admin privileges on the server."
        echo -e "    Leave empty to skip."
        echo ""
        local answer=""
        read -rp "  Admin name (default: none): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"
        if [[ -n "$answer" ]]; then
            ADMIN_NAME="$answer"
            log "Admin: ${ADMIN_NAME}"
        else
            info "No admin player set"
        fi
        echo ""
    fi

    # ── Max players ──
    if [[ -z "$MAX_PLAYERS" ]]; then
        echo -e "  ${BOLD}6. Max Players:${RESET}"
        echo ""
        echo -e "    Default: ${CYAN}15${RESET}"
        echo ""
        local answer=""
        read -rp "  Max players (default: 15): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"
        if [[ -n "$answer" ]] && [[ "$answer" =~ ^[0-9]+$ ]] && [[ "$answer" -ge 1 ]]; then
            MAX_PLAYERS="$answer"
        else
            MAX_PLAYERS="15"
        fi
        log "Max players: ${MAX_PLAYERS}"
        echo ""
    fi

    # ── MOTD ──
    if [[ -z "$MOTD" ]]; then
        echo -e "  ${BOLD}7. Message of the Day (optional):${RESET}"
        echo ""
        echo -e "    Shown to players when they connect."
        echo -e "    Leave empty for no MOTD."
        echo ""
        local answer=""
        read -rp "  MOTD (default: none): " answer || true
        answer="${answer#"${answer%%[![:space:]]*}"}"
        answer="${answer%"${answer##*[![:space:]]}"}"
        if [[ -n "$answer" ]]; then
            MOTD="$answer"
            log "MOTD: ${MOTD}"
        else
            info "No MOTD set"
        fi
        echo ""
    fi

    # ── Run mode ──
    if [[ -z "$RUN_MODE" ]]; then
        echo -e "  ${BOLD}8. Run Mode:${RESET}"
        echo ""
        echo -e "    ${CYAN}1)${RESET} Foreground   - Run in terminal (Ctrl+C to stop)"
        echo -e "    ${CYAN}2)${RESET} Background   - Run as daemon (nohup)"
        echo -e "    ${CYAN}3)${RESET} Screen       - Run in screen session (attach/detach)"
        echo ""

        while true; do
            local answer=""
            read -rp "  Select run mode [1-3] (default: 1): " answer || true
            answer="${answer#"${answer%%[![:space:]]*}"}"
            answer="${answer%"${answer##*[![:space:]]}"}"

            case "$answer" in
                1|"")
                    RUN_MODE="foreground"
                    log "Run mode: foreground"
                    break
                    ;;
                2)
                    RUN_MODE="background"
                    log "Run mode: background (daemon)"
                    break
                    ;;
                3)
                    if ! command -v screen &>/dev/null; then
                        echo -e "  ${RED}'screen' is not installed. Install it first: sudo apt install screen${RESET}"
                        continue
                    fi
                    RUN_MODE="screen"
                    log "Run mode: screen session"
                    break
                    ;;
                q|quit)
                    info "Cancelled."; exit 0 ;;
                *)
                    echo -e "  ${RED}Invalid choice. Enter 1, 2, or 3.${RESET}" ;;
            esac
        done
        echo ""
    fi
}

# --- Main ---

main() {
    parse_args "$@"

    # Handle standalone commands
    if [[ "$DO_LIST_WORLDS" -eq 1 ]]; then
        list_worlds || exit 1
        exit 0
    fi

    if [[ -n "$CREATE_WORLD" ]]; then
        create_world "$CREATE_WORLD" || exit 1
        exit 0
    fi

    if [[ "$DO_STATUS" -eq 1 ]]; then
        show_status || exit 1
        exit 0
    fi

    if [[ "$DO_STOP" -eq 1 ]]; then
        stop_server "$STOP_PORT" || exit 1
        exit 0
    fi

    if [[ "$DO_LOGS" -eq 1 ]]; then
        tail_logs "$LOGS_PORT" || exit 1
        exit 0
    fi

    # Interactive start menu
    show_start_menu

    # Apply defaults
    SECURE_MODE="${SECURE_MODE:-1}"
    [[ "$SECURE_MODE" -eq -1 ]] && SECURE_MODE=1
    RUN_MODE="${RUN_MODE:-foreground}"
    [[ -z "$RUN_MODE" ]] && RUN_MODE="foreground"
    PORT="${PORT:-30000}"
    GAME_ID="${GAME_ID:-devtest}"

    # Find or build the server binary
    local server_binary=""
    server_binary="$(find_server_binary)" || true

    if [[ -z "$server_binary" ]]; then
        if [[ "$DO_BUILD" -eq 1 ]]; then
            build_server || exit 1
            server_binary="$(find_server_binary)" || {
                error "Server binary still not found after build."
                exit 1
            }
        else
            error "Luanti server binary not found."
            error "Searched: ${SOURCE_DIR}/bin/luantiserver"
            error "          ${SOURCE_DIR}/build/bin/luantiserver"
            error "Build it first with: ./build_linux.sh --server --run-in-place"
            error "Or use --build to auto-build."
            exit 1
        fi
    fi

    log "Server binary: ${server_binary}"

    # Ensure world base directory exists
    mkdir -p "$WORLD_BASE_DIR" || {
        error "Failed to create world directory: ${WORLD_BASE_DIR}"
        exit 1
    }

    # If a world was specified, ensure it exists
    if [[ -n "$WORLD_NAME" ]]; then
        if [[ ! -d "${WORLD_BASE_DIR}/${WORLD_NAME}" ]]; then
            info "World '${WORLD_NAME}' not found. Creating it..."
            create_world "$WORLD_NAME" || exit 1
        fi
    fi

    # Generate temp config
    generate_temp_config || exit 1

    # Build server command
    build_server_cmd "$server_binary" || exit 1

    # Display startup banner
    echo ""
    echo -e "${BOLD}+==================================================+${RESET}"
    echo -e "${BOLD}|          Luanti Server Start Script v3.0          |${RESET}"
    echo -e "${BOLD}+==================================================+${RESET}"
    echo ""
    echo -e "  ${CYAN}Binary:${RESET}     ${server_binary}"
    echo -e "  ${CYAN}Port:${RESET}       ${PORT}"
    echo -e "  ${CYAN}Game:${RESET}       ${GAME_ID}"
    echo -e "  ${CYAN}World:${RESET}      ${WORLD_NAME:-<auto>}"
    echo -e "  ${CYAN}Security:${RESET}   $([ "$SECURE_MODE" -eq 1 ] && echo "${GREEN}secure${RESET}" || echo "${YELLOW}insecure${RESET}")"
    echo -e "  ${CYAN}Run mode:${RESET}   ${RUN_MODE}"
    [[ -n "$ADMIN_NAME" ]] && echo -e "  ${CYAN}Admin:${RESET}      ${ADMIN_NAME}"
    [[ -n "$MAX_PLAYERS" ]] && echo -e "  ${CYAN}Max players:${RESET} ${MAX_PLAYERS}"
    [[ -n "$MOTD" ]] && echo -e "  ${CYAN}MOTD:${RESET}       ${MOTD}"
    [[ -n "$TEMP_CONFIG" ]] && echo -e "  ${CYAN}Conf file:${RESET}  ${TEMP_CONFIG}"
    echo ""

    # Run the server
    case "$RUN_MODE" in
        foreground)  run_foreground  || exit 1 ;;
        background)  run_background  || exit 1 ;;
        screen)      run_screen      || exit 1 ;;
        *)
            error "Unknown run mode: ${RUN_MODE}"
            exit 1
            ;;
    esac

    return 0
}

main "$@"
