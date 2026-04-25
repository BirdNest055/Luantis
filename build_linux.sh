#!/usr/bin/env bash
#
# build_linux.sh — Fully Automated Linux Build Script for Luanti
#
# Features:
#   - Interactive build target selection (client / server / both)
#   - Interactive dependency menu (ask before installing)
#   - Auto-detect missing deps and offer to install them
#   - Distro-aware package name mapping (Debian/Fedora/Arch/Alpine)
#   - Dependency removal option
#   - Ubuntu-safe package resolution (no broken deps)
#   - Auto-detect stale CMake cache and clean it
#   - Option to run the game after building
#   - Test-driven build verification
#
# Usage:
#   ./build_linux.sh [OPTIONS]
#
# Options:
#   --client           Build client only (skips target menu)
#   --server           Build server only (skips target menu)
#   --both             Build both client and server (skips target menu)
#   --release          Release build (default)
#   --debug            Debug build
#   --tests            Build and run unit tests
#   --install          Install after building
#   --run              Run the game after building
#   --deps-only        Only install dependencies, don't build
#   --no-deps          Skip initial dependency menu (still verifies and offers to install missing)
#   --remove-deps      Remove all installed build dependencies
#   --clean            Clean build directory before building
#   --jobs N           Number of parallel jobs (default: nproc)
#   --prefix PATH      Install prefix (default: /usr/local)
#   --local-prefix PATH  Path to locally-built dependencies (auto-detected)
#   --run-in-place     Run in place (no installation needed)
#   --enable-lto       Enable Link-Time Optimization
#   --enable-prometheus Enable Prometheus metrics (server only)
#   --enable-all-opts  Enable all optional features
#   --non-interactive  Skip all menus, use defaults (for CI; builds client only)
#   --verbose          Verbose output
#   --help             Show this help
#
# Examples:
#   ./build_linux.sh --both --tests           # Full build with tests
#   ./build_linux.sh --server --deps-only     # Install server deps only
#   ./build_linux.sh --client --run-in-place  # Dev build, run from source
#   ./build_linux.sh --run                    # Build and run the game
#   ./build_linux.sh --remove-deps            # Remove all build deps
#   ./build_linux.sh --non-interactive        # CI mode, no menus
#

set -uo pipefail

# ─── Configuration ─────────────────────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
LOG_FILE="${SCRIPT_DIR}/build_linux.log"
DEPS_MANIFEST="${SCRIPT_DIR}/.build_deps_installed"

# Build defaults
BUILD_CLIENT=1
BUILD_SERVER=0
BUILD_TARGET_EXPLICIT=0   # Track if user explicitly chose --client/--server/--both
BUILD_TYPE="Release"
RUN_TESTS=0
DO_INSTALL=0
DO_RUN=0
DEPS_ONLY=0
NO_DEPS=0
REMOVE_DEPS=0
DO_CLEAN=0
JOBS="$(nproc 2>/dev/null || echo 4)"
PREFIX="/usr/local"
LOCAL_PREFIX=""          # Path to locally-built dependencies (auto-detected if empty)
RUN_IN_PLACE=0
ENABLE_LTO=0
ENABLE_PROMETHEUS=0
ENABLE_ALL_OPTS=0
NON_INTERACTIVE=0
VERBOSE=0

# Colors — using $'...' ANSI-C quoting so escape chars are real bytes
# This makes them work with BOTH echo -e AND printf %s
RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
YELLOW=$'\033[1;33m'
BLUE=$'\033[0;34m'
CYAN=$'\033[0;36m'
BOLD=$'\033[1m'
DIM=$'\033[2m'
NC=$'\033[0m'

# ─── Logging ───────────────────────────────────────────────────────────────────

log()   { echo -e "${GREEN}[BUILD]${NC} $*" | tee -a "$LOG_FILE"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*" | tee -a "$LOG_FILE" >&2; }
error() { echo -e "${RED}[ERROR]${NC} $*" | tee -a "$LOG_FILE" >&2; }
info()  { echo -e "${BLUE}[INFO]${NC} $*" | tee -a "$LOG_FILE"; }
step()  { echo -e "${CYAN}[STEP]${NC} $*" | tee -a "$LOG_FILE"; }

# ─── Path Safety Check ────────────────────────────────────────────────────────
# Warn if the project path contains shell-special characters (e.g. parentheses)
# that break CMake's Unix Makefiles generator. Ninja is immune but Make is not.
# We check character-by-character to avoid regex parsing issues with special chars.
_path_has_special=false
for (( _i=0; _i<${#SCRIPT_DIR}; _i++ )); do
    _c="${SCRIPT_DIR:$_i:1}"
    case "$_c" in
        ' '|'('|')'|'&'|';'|'`'|'!'|'$'|'<'|'>'|'|'|'\'|"'"|'"') _path_has_special=true; break ;;
    esac
done
if [[ "$_path_has_special" == "true" ]]; then
    warn "Project path contains special characters:"
    warn "  ${SCRIPT_DIR}"
    warn "  This can break the build with the 'Unix Makefiles' generator."
    warn "  Consider moving the project to a path without spaces or special chars."
    warn "  Alternatively, install ninja-build for a more robust build generator."
    echo ""
fi

# ─── Argument Parsing ──────────────────────────────────────────────────────────

show_help() {
    sed -n '3,/^$/p' "$0" | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --client)          BUILD_CLIENT=1; BUILD_SERVER=0; BUILD_TARGET_EXPLICIT=1 ;;
        --server)          BUILD_CLIENT=0; BUILD_SERVER=1; BUILD_TARGET_EXPLICIT=1 ;;
        --both)            BUILD_CLIENT=1; BUILD_SERVER=1; BUILD_TARGET_EXPLICIT=1 ;;
        --release)         BUILD_TYPE="Release" ;;
        --debug)           BUILD_TYPE="Debug" ;;
        --tests)           RUN_TESTS=1 ;;
        --install)         DO_INSTALL=1 ;;
        --run)             DO_RUN=1 ;;
        --deps-only)       DEPS_ONLY=1 ;;
        --no-deps)         NO_DEPS=1 ;;
        --remove-deps)     REMOVE_DEPS=1 ;;
        --clean)           DO_CLEAN=1 ;;
        --jobs)            JOBS="$2"; shift ;;
        --prefix)          PREFIX="$2"; shift ;;
        --local-prefix)    LOCAL_PREFIX="$2"; shift ;;
        --run-in-place)    RUN_IN_PLACE=1 ;;
        --enable-lto)      ENABLE_LTO=1 ;;
        --enable-prometheus) ENABLE_PROMETHEUS=1 ;;
        --enable-all-opts) ENABLE_ALL_OPTS=1 ;;
        --non-interactive) NON_INTERACTIVE=1 ;;
        --verbose)         VERBOSE=1 ;;
        --help|-h)         show_help ;;
        *)                 error "Unknown option: $1"; show_help ;;
    esac
    shift
done

if [[ "$ENABLE_ALL_OPTS" -eq 1 ]]; then
    ENABLE_LTO=1
    ENABLE_PROMETHEUS=1
fi

# ─── Distro Detection ──────────────────────────────────────────────────────────

detect_distro() {
    if [[ -f /etc/os-release ]]; then
        # shellcheck disable=SC1091
        source /etc/os-release
        echo "${ID}"
    elif command -v lsb_release &>/dev/null; then
        lsb_release -is | tr '[:upper:]' '[:lower:]'
    elif [[ -f /etc/debian_version ]]; then
        echo "debian"
    elif [[ -f /etc/fedora-release ]]; then
        echo "fedora"
    elif [[ -f /etc/arch-release ]]; then
        echo "arch"
    elif [[ -f /etc/alpine-release ]]; then
        echo "alpine"
    elif [[ -f /etc/gentoo-release ]]; then
        echo "gentoo"
    else
        echo "unknown"
    fi
}

DISTRO="$(detect_distro)"
info "Detected distribution: ${DISTRO}"

# ─── Ubuntu-Safe Package Resolution ────────────────────────────────────────────

resolve_freetype_pkg() {
    if apt-cache showpkg libfreetype-dev 2>/dev/null | grep -q "Package: libfreetype-dev"; then
        echo "libfreetype-dev"
    else
        echo "libfreetype6-dev"
    fi
}

# ─── Package Install Status ────────────────────────────────────────────────────

pkg_is_installed() {
    local pkg="$1"
    case "$DISTRO" in
        debian|ubuntu|linuxmint|pop|elementary|kali)
            dpkg -s "$pkg" &>/dev/null ;;
        fedora|rhel|centos|rocky|alma|oracle)
            rpm -q "$pkg" &>/dev/null ;;
        arch|manjaro|endeavouros|garuda)
            pacman -Qi "$pkg" &>/dev/null ;;
        alpine)
            apk info -e "$pkg" &>/dev/null ;;
        *)
            return 1 ;;
    esac
}

# ─── Dependency Group Data ─────────────────────────────────────────────────────

NUM_GROUPS=0
GROUP_NAME=()
GROUP_DESC=()
GROUP_PKGS=()
GROUP_REQUIRED=()
GROUP_SELECTED=()
GROUP_ALREADY_INSTALLED=()
GROUP_TO_INSTALL=()

populate_groups() {
    NUM_GROUPS=0
    GROUP_NAME=()
    GROUP_DESC=()
    GROUP_PKGS=()
    GROUP_REQUIRED=()
    GROUP_SELECTED=()
    GROUP_ALREADY_INSTALLED=()
    GROUP_TO_INSTALL=()

    local freetype_pkg=""

    case "$DISTRO" in
        debian|ubuntu|linuxmint|pop|elementary|kali)
            freetype_pkg="$(resolve_freetype_pkg)"
            _add_group "core"          "Core build tools (gcc, cmake, pkg-config)"   "build-essential cmake pkg-config"                    1 "$BUILD_CLIENT"
            _add_group "common"        "Common libraries (zlib, zstd, sqlite...)"    "zlib1g-dev libzstd-dev libsqlite3-dev libgmp-dev libjsoncpp-dev libluajit-5.1-dev libssl-dev" 1 "$BUILD_CLIENT"
            _add_group "client_gl"     "Client: Graphics & windowing"                "${freetype_pkg} libgl1-mesa-dev libsdl2-dev"         0 "$BUILD_CLIENT"
            _add_group "client_audio"  "Client: Audio (openal, vorbis, ogg)"         "libopenal-dev libvorbis-dev libogg-dev"              0 "$BUILD_CLIENT"
            _add_group "client_media"  "Client: Images & i18n (jpeg, png, gettext)"  "libjpeg-dev libpng-dev gettext"                     0 "$BUILD_CLIENT"
            _add_group "client_net"    "Client: Networking (curl)"                    "libcurl4-openssl-dev"                                0 "$BUILD_CLIENT"
            _add_group "server"        "Server: Networking & terminal"                "libcurl4-openssl-dev libncurses-dev"                0 "$BUILD_SERVER"
            _add_group "optional_db"   "Optional: Database backends (pq, leveldb...)" "libpq-dev libleveldb-dev libhiredis-dev"            0 "$ENABLE_ALL_OPTS"
            _add_group "optional_spatial" "Optional: Spatial index"                    "libspatialindex-dev"                                 0 "$ENABLE_ALL_OPTS"
            ;;
        fedora|rhel|centos|rocky|alma|oracle)
            _add_group "core"          "Core build tools"                             "gcc-c++ cmake pkgconfig"                             1 "$BUILD_CLIENT"
            _add_group "common"        "Common libraries"                             "zlib-devel libzstd-devel sqlite-devel gmp-devel jsoncpp-devel luajit-devel openssl-devel" 1 "$BUILD_CLIENT"
            _add_group "client_gl"     "Client: Graphics & windowing"                 "freetype-devel mesa-libGL-devel SDL2-devel"          0 "$BUILD_CLIENT"
            _add_group "client_audio"  "Client: Audio"                                "openal-soft-devel libvorbis-devel libogg-devel"      0 "$BUILD_CLIENT"
            _add_group "client_media"  "Client: Images & i18n"                        "libjpeg-turbo-devel libpng-devel gettext-devel"      0 "$BUILD_CLIENT"
            _add_group "client_net"    "Client: Networking"                           "libcurl-devel"                                       0 "$BUILD_CLIENT"
            _add_group "server"        "Server: Networking & terminal"                "libcurl-devel ncurses-devel"                         0 "$BUILD_SERVER"
            _add_group "optional_db"   "Optional: Database backends"                  "postgresql-devel leveldb-devel hiredis-devel"        0 "$ENABLE_ALL_OPTS"
            _add_group "optional_spatial" "Optional: Spatial index"                    "spatialindex-devel"                                  0 "$ENABLE_ALL_OPTS"
            ;;
        arch|manjaro|endeavouros|garuda)
            _add_group "core"          "Core build tools"                             "base-devel cmake pkg-config"                         1 1
            _add_group "common"        "Common libraries"                             "zlib zstd sqlite gmp jsoncpp luajit openssl"         1 "$BUILD_CLIENT"
            _add_group "client_gl"     "Client: Graphics & windowing"                 "freetype2 mesa sdl2"                                 0 "$BUILD_CLIENT"
            _add_group "client_audio"  "Client: Audio"                                "openal libvorbis libogg"                             0 "$BUILD_CLIENT"
            _add_group "client_media"  "Client: Images & i18n"                        "libjpeg-turbo libpng gettext"                        0 "$BUILD_CLIENT"
            _add_group "client_net"    "Client: Networking"                           "curl"                                                0 "$BUILD_CLIENT"
            _add_group "server"        "Server: Networking & terminal"                "curl ncurses"                                        0 "$BUILD_SERVER"
            _add_group "optional_db"   "Optional: Database backends"                  "postgresql-libs leveldb hiredis"                     0 "$ENABLE_ALL_OPTS"
            _add_group "optional_spatial" "Optional: Spatial index"                    "spatialindex"                                        0 "$ENABLE_ALL_OPTS"
            ;;
        alpine)
            _add_group "core"          "Core build tools"                             "build-base cmake pkgconf"                            1 1
            _add_group "common"        "Common libraries"                             "zlib-dev zstd-dev sqlite-dev gmp-dev jsoncpp-dev luajit-dev openssl-dev" 1 "$BUILD_CLIENT"
            _add_group "client_gl"     "Client: Graphics & windowing"                 "freetype-dev mesa-dev sdl2-dev"                      0 "$BUILD_CLIENT"
            _add_group "client_audio"  "Client: Audio"                                "openal-soft-dev libvorbis-dev libogg-dev"            0 "$BUILD_CLIENT"
            _add_group "client_media"  "Client: Images & i18n"                        "libjpeg-turbo-dev libpng-dev gettext-dev"            0 "$BUILD_CLIENT"
            _add_group "client_net"    "Client: Networking"                           "curl-dev"                                            0 "$BUILD_CLIENT"
            _add_group "server"        "Server: Networking & terminal"                "curl-dev ncurses-dev"                                0 "$BUILD_SERVER"
            _add_group "optional_db"   "Optional: Database backends"                  "postgresql-dev leveldb-dev hiredis-dev"              0 "$ENABLE_ALL_OPTS"
            _add_group "optional_spatial" "Optional: Spatial index"                    "spatialindex-dev"                                    0 "$ENABLE_ALL_OPTS"
            ;;
        *)
            error "Unsupported distribution: ${DISTRO}"
            return 1
            ;;
    esac
}

_add_group() {
    local name="$1"
    local desc="$2"
    local pkgs="$3"
    local required="$4"
    local selected_default="$5"

    GROUP_NAME+=("$name")
    GROUP_DESC+=("$desc")
    GROUP_PKGS+=("$pkgs")
    GROUP_REQUIRED+=("$required")
    GROUP_SELECTED+=("${selected_default:-0}")
    GROUP_ALREADY_INSTALLED+=(0)
    GROUP_TO_INSTALL+=("")
    ((NUM_GROUPS++)) || true
}

compute_install_status() {
    for i in $(seq 0 $((NUM_GROUPS - 1))); do
        local already=0
        local to_install=""
        for pkg in ${GROUP_PKGS[$i]}; do
            if pkg_is_installed "$pkg"; then
                ((already++)) || true
            else
                if [[ -n "$to_install" ]]; then
                    to_install="${to_install} ${pkg}"
                else
                    to_install="${pkg}"
                fi
            fi
        done
        GROUP_ALREADY_INSTALLED[$i]="$already"
        GROUP_TO_INSTALL[$i]="$to_install"
    done
}

# ─── Build Target Menu ─────────────────────────────────────────────────────────

show_build_target_menu() {
    echo ""
    echo -e "${BOLD}============================================================${NC}"
    echo -e "${BOLD}       Luanti Build - Select Build Target${NC}"
    echo -e "${BOLD}============================================================${NC}"
    echo ""
    echo "  What would you like to build?"
    echo ""
    echo -e "  ${CYAN}1)${NC} Client only         ${DIM}(graphical game client)${NC}"
    echo -e "  ${CYAN}2)${NC} Server only         ${DIM}(headless dedicated server)${NC}"
    echo -e "  ${CYAN}3)${NC} Both client & server ${DIM}(full build)${NC}"
    echo ""

    while true; do
        read -rp "  Select [1-3] (default: 1): " answer
        answer="$(echo "$answer" | xargs)"

        case "$answer" in
            1|"")
                BUILD_CLIENT=1
                BUILD_SERVER=0
                log "Selected: Client only"
                break
                ;;
            2)
                BUILD_CLIENT=0
                BUILD_SERVER=1
                log "Selected: Server only"
                break
                ;;
            3)
                BUILD_CLIENT=1
                BUILD_SERVER=1
                log "Selected: Both client and server"
                break
                ;;
            q|quit)
                warn "Build cancelled by user."
                exit 0
                ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter 1, 2, or 3.${NC}"
                ;;
        esac
    done

    echo ""
}

# ─── Interactive Menu ──────────────────────────────────────────────────────────

show_dep_menu() {
    populate_groups || return 1
    compute_install_status

    while true; do
        echo ""
        echo -e "${BOLD}============================================================${NC}"
        echo -e "${BOLD}       Luanti Build - Dependency Selection Menu${NC}"
        echo -e "${BOLD}============================================================${NC}"
        echo ""
        echo "  Type a number to toggle a group on/off."
        echo -e "  Required groups ${RED}(R)${NC} cannot be toggled off."
        echo ""
        echo -e "  ${GREEN}[*]${NC} = selected (will install)    ${DIM}[ ]${NC} = skipped"
        echo -e "  ${GREEN}OK${NC}  = all packages already installed"
        echo -e "  ${YELLOW}2/4${NC} = some packages already installed"
        echo -e "  ${CYAN}NEW${NC} = packages need to be installed"
        echo ""
        echo -e "  ${DIM}--------------------------------------------------------${NC}"

        for i in $(seq 0 $((NUM_GROUPS - 1))); do
            local sel=" "
            local status=""
            local req_mark=""
            local total_pkgs
            total_pkgs=$(echo "${GROUP_PKGS[$i]}" | wc -w)
            local already="${GROUP_ALREADY_INSTALLED[$i]}"
            local to_install="${GROUP_TO_INSTALL[$i]}"

            if [[ "${GROUP_SELECTED[$i]}" -eq 1 ]]; then
                sel="*"
            fi

            # Status label
            if [[ "$already" -eq "$total_pkgs" ]] && [[ "$total_pkgs" -gt 0 ]]; then
                status="${GREEN}OK${NC}  "
            elif [[ "$already" -gt 0 ]]; then
                status="${YELLOW}${already}/${total_pkgs}${NC}  "
            elif [[ "${GROUP_SELECTED[$i]}" -eq 1 ]] && [[ -n "$to_install" ]]; then
                status="${CYAN}NEW${NC} "
            else
                status="${DIM}---${NC} "
            fi

            if [[ "${GROUP_REQUIRED[$i]}" -eq 1 ]]; then
                req_mark=" ${RED}(R)${NC}"
            fi

            printf "  %s%2d%s [%s]  %-18s %s%s\n" \
                "${BOLD}" "$((i + 1))" "${NC}" \
                "$sel" \
                "${GROUP_NAME[$i]}" \
                "$status" "$req_mark"

            # Description
            printf "         %s\n" "${DIM}${GROUP_DESC[$i]}${NC}"

            # Packages that need installing
            if [[ "${GROUP_SELECTED[$i]}" -eq 1 ]] && [[ -n "$to_install" ]]; then
                printf "         %s\n" "${CYAN}Install: ${to_install}${NC}"
            elif [[ "${GROUP_SELECTED[$i]}" -eq 1 ]] && [[ -z "$to_install" ]]; then
                printf "         %s\n" "${GREEN}(all already installed)${NC}"
            fi
        done

        echo -e "  ${DIM}--------------------------------------------------------${NC}"
        echo ""
        echo -e "  Commands:  ${BOLD}<number>${NC} Toggle  ${BOLD}I${NC} Install  ${BOLD}S${NC} Skip  ${BOLD}R${NC} Remove all deps  ${BOLD}Q${NC} Quit"
        echo ""

        read -rp "  > " answer
        answer="$(echo "$answer" | tr '[:upper:]' '[:lower:]' | xargs)"

        [[ -z "$answer" ]] && continue

        if [[ "$answer" == "q" ]] || [[ "$answer" == "quit" ]]; then
            warn "Build cancelled by user."
            exit 0
        fi

        if [[ "$answer" == "s" ]] || [[ "$answer" == "skip" ]]; then
            info "Skipping dependency installation."
            NO_DEPS=1
            return 0
        fi

        if [[ "$answer" == "r" ]] || [[ "$answer" == "remove" ]]; then
            remove_all_deps
            return $?
        fi

        if [[ "$answer" == "i" ]] || [[ "$answer" == "install" ]] || [[ "$answer" == "y" ]] || [[ "$answer" == "yes" ]]; then
            break
        fi

        # Toggle by number
        if [[ "$answer" =~ ^[0-9]+$ ]]; then
            local idx=$((answer - 1))
            if [[ "$idx" -ge 0 ]] && [[ "$idx" -lt "$NUM_GROUPS" ]]; then
                if [[ "${GROUP_REQUIRED[$idx]}" -eq 1 ]]; then
                    echo -e "  ${YELLOW}'${GROUP_NAME[$idx]}' is required and cannot be toggled off.${NC}"
                    sleep 1
                else
                    if [[ "${GROUP_SELECTED[$idx]}" -eq 1 ]]; then
                        GROUP_SELECTED[$idx]=0
                    else
                        GROUP_SELECTED[$idx]=1
                    fi
                fi
            else
                echo -e "  ${RED}Invalid number. Choose 1-${NUM_GROUPS}.${NC}"
                sleep 1
            fi
        fi
    done

    # Collect packages to install
    local -a all_pkgs=()
    for i in $(seq 0 $((NUM_GROUPS - 1))); do
        if [[ "${GROUP_SELECTED[$i]}" -eq 1 ]]; then
            for pkg in ${GROUP_TO_INSTALL[$i]}; do
                all_pkgs+=("$pkg")
            done
        fi
    done

    if [[ ${#all_pkgs[@]} -eq 0 ]]; then
        info "All selected packages are already installed. Nothing to do."
        return 0
    fi

    # Summary and confirm
    echo ""
    echo -e "${BOLD}The following packages will be installed:${NC}"
    echo ""
    local col=0
    for pkg in "${all_pkgs[@]}"; do
        printf "  ${GREEN}+${NC} %-26s" "$pkg"
        ((col++)) || true
        if [[ "$col" -ge 3 ]]; then
            echo ""
            col=0
        fi
    done
    echo ""
    echo ""
    printf "  Total: %s%s%s packages to install\n" "${BOLD}" "${#all_pkgs[@]}" "${NC}"
    echo ""

    read -rp "  Proceed with installation? [Y/n] " confirm
    confirm="$(echo "$confirm" | tr '[:upper:]' '[:lower:]' | xargs)"
    case "$confirm" in
        n|no)
            warn "Installation cancelled."
            return 1
            ;;
    esac

    install_packages "${all_pkgs[@]}"
    save_deps_manifest "${all_pkgs[@]}"

    if [[ "$ENABLE_PROMETHEUS" -eq 1 ]]; then
        if ! pkg-config --exists libprometheus-cpp-pull 2>/dev/null; then
            warn "prometheus-cpp not found. Building from source..."
            build_prometheus_cpp
        fi
    fi
}

# ─── Install / Remove Packages ─────────────────────────────────────────────────

install_packages() {
    local -a pkgs=("$@")
    step "Installing ${#pkgs[@]} packages..."

    # Check if sudo is available before attempting package operations
    if ! command -v sudo &>/dev/null; then
        error "sudo is not installed. Cannot install packages automatically."
        error "Please install the following packages manually:"
        error "  ${pkgs[*]}"
        return 1
    fi

    case "$DISTRO" in
        debian|ubuntu|linuxmint|pop|elementary|kali)
            if command -v add-apt-repository &>/dev/null; then
                sudo add-apt-repository -y universe 2>/dev/null || true
            fi
            sudo apt-get update -qq

            if sudo apt-get install -y "${pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"; then
                : # success
            else
                warn "Bulk install had issues. Trying packages one by one..."
                local -a failed=()
                for pkg in "${pkgs[@]}"; do
                    if ! sudo apt-get install -y "$pkg" 2>&1 | tee -a "$LOG_FILE"; then
                        warn "Failed: $pkg — skipping"
                        failed+=("$pkg")
                    fi
                done
                if [[ ${#failed[@]} -gt 0 ]]; then
                    warn "Could not install: ${failed[*]}"
                    warn "This may be OK if functionality is provided by an already-installed package."
                fi
            fi
            ;;
        fedora|rhel|centos|rocky|alma|oracle)
            sudo dnf install -y "${pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            ;;
        arch|manjaro|endeavouros|garuda)
            sudo pacman -S --needed --noconfirm "${pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            ;;
        alpine)
            sudo apk add "${pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            ;;
        *)
            error "Don't know how to install packages on ${DISTRO}"
            return 1
            ;;
    esac

    log "Package installation completed."
}

save_deps_manifest() {
    local -a pkgs=("$@")
    echo "# Luanti build dependencies installed by build_linux.sh" > "$DEPS_MANIFEST"
    echo "# Generated: $(date)" >> "$DEPS_MANIFEST"
    echo "# Distro: ${DISTRO}" >> "$DEPS_MANIFEST"
    for pkg in "${pkgs[@]}"; do
        echo "$pkg" >> "$DEPS_MANIFEST"
    done
    info "Dependency manifest saved to ${DEPS_MANIFEST}"
}

remove_all_deps() {
    echo ""
    echo -e "${BOLD}============================================================${NC}"
    echo -e "${BOLD}       Remove All Build Dependencies${NC}"
    echo -e "${BOLD}============================================================${NC}"
    echo ""

    populate_groups || return 1

    local -a all_possible_pkgs=()
    for i in $(seq 0 $((NUM_GROUPS - 1))); do
        for pkg in ${GROUP_PKGS[$i]}; do
            all_possible_pkgs+=("$pkg")
        done
    done

    # Also read manifest
    if [[ -f "$DEPS_MANIFEST" ]]; then
        while IFS= read -r line; do
            [[ "$line" =~ ^# ]] && continue
            [[ -z "$line" ]] && continue
            local found=0
            for p in "${all_possible_pkgs[@]}"; do
                if [[ "$p" == "$line" ]]; then found=1; break; fi
            done
            if [[ "$found" -eq 0 ]]; then
                all_possible_pkgs+=("$line")
            fi
        done < "$DEPS_MANIFEST"
    fi

    # Filter to actually installed
    local -a installed_pkgs=()
    for pkg in "${all_possible_pkgs[@]}"; do
        if pkg_is_installed "$pkg"; then
            installed_pkgs+=("$pkg")
        fi
    done

    if [[ ${#installed_pkgs[@]} -eq 0 ]]; then
        info "No build dependencies are currently installed. Nothing to remove."
        return 0
    fi

    echo -e "${YELLOW}The following build dependencies will be removed:${NC}"
    echo ""
    local col=0
    for pkg in "${installed_pkgs[@]}"; do
        printf "  ${RED}-${NC} %-26s" "$pkg"
        ((col++)) || true
        if [[ "$col" -ge 3 ]]; then
            echo ""
            col=0
        fi
    done
    echo ""
    echo ""
    printf "  Total: %s%s%s packages\n" "${BOLD}" "${#installed_pkgs[@]}" "${NC}"
    echo ""
    echo -e "${RED}WARNING: Some packages may be needed by other software on your system.${NC}"
    echo -e "${RED}This will also run 'apt autoremove' to clean up orphaned dependencies.${NC}"
    echo ""

    read -rp "  Remove these packages? [y/N] " confirm
    confirm="$(echo "$confirm" | tr '[:upper:]' '[:lower:]' | xargs)"
    case "$confirm" in
        y|yes) ;;
        *)
            info "Removal cancelled."
            return 0
            ;;
    esac

    step "Removing ${#installed_pkgs[@]} build dependencies..."

    case "$DISTRO" in
        debian|ubuntu|linuxmint|pop|elementary|kali)
            sudo apt-get remove -y "${installed_pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            sudo apt-get autoremove -y 2>&1 | tee -a "$LOG_FILE"
            ;;
        fedora|rhel|centos|rocky|alma|oracle)
            sudo dnf remove -y "${installed_pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            sudo dnf autoremove -y 2>&1 | tee -a "$LOG_FILE"
            ;;
        arch|manjaro|endeavouros|garuda)
            sudo pacman -Rns --noconfirm "${installed_pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            ;;
        alpine)
            sudo apk del "${installed_pkgs[@]}" 2>&1 | tee -a "$LOG_FILE"
            ;;
    esac

    rm -f "$DEPS_MANIFEST"
    log "Build dependencies removed successfully."
    info "You may also want to run: sudo apt autoremove"
}

# ─── Non-Interactive Dependency Install ────────────────────────────────────────

install_deps_noninteractive() {
    step "Installing build dependencies (non-interactive mode)..."

    populate_groups || return 1

    local -a pkgs_to_install=()
    for i in $(seq 0 $((NUM_GROUPS - 1))); do
        case "${GROUP_NAME[$i]}" in
            core|common) ;;
            client_*)  [[ "$BUILD_CLIENT" -eq 0 ]] && continue ;;
            server)    [[ "$BUILD_SERVER" -eq 0 ]] && continue ;;
            optional_*) [[ "$ENABLE_ALL_OPTS" -eq 0 ]] && continue ;;
        esac

        for pkg in ${GROUP_PKGS[$i]}; do
            if ! pkg_is_installed "$pkg"; then
                pkgs_to_install+=("$pkg")
            fi
        done
    done

    if [[ ${#pkgs_to_install[@]} -eq 0 ]]; then
        info "All required packages are already installed."
        return 0
    fi

    info "Packages to install: ${pkgs_to_install[*]}"
    install_packages "${pkgs_to_install[@]}"
    save_deps_manifest "${pkgs_to_install[@]}"

    if [[ "$ENABLE_PROMETHEUS" -eq 1 ]]; then
        if ! pkg-config --exists libprometheus-cpp-pull 2>/dev/null; then
            warn "prometheus-cpp not found. Building from source..."
            build_prometheus_cpp
        fi
    fi
}

# ─── Dependency Router ─────────────────────────────────────────────────────────

install_dependencies() {
    step "Preparing build dependencies..."

    if [[ "$NON_INTERACTIVE" -eq 1 ]]; then
        install_deps_noninteractive
    else
        show_dep_menu
    fi
}

# ─── Build prometheus-cpp from source ──────────────────────────────────────────

build_prometheus_cpp() {
    local tmpdir
    tmpdir="$(mktemp -d)"

    info "Building prometheus-cpp from source in ${tmpdir}..."
    git clone --depth 1 https://github.com/jupp0r/prometheus-cpp.git "$tmpdir/prometheus-cpp"
    pushd "$tmpdir/prometheus-cpp" > /dev/null
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_TESTING=0 \
        -DBUILD_SHARED_LIBS=ON
    cmake --build build -j"${JOBS}"
    sudo cmake --install build
    sudo ldconfig 2>/dev/null || true
    popd > /dev/null
    rm -rf "$tmpdir"
    log "prometheus-cpp built and installed."
}

# ─── CMake Cache Check ─────────────────────────────────────────────────────────

check_cmake_cache() {
    local cache_file="${BUILD_DIR}/CMakeCache.txt"
    if [[ ! -f "$cache_file" ]]; then
        return 0
    fi

    # CMakeCache.txt format: KEY:TYPE=VALUE
    # e.g. CMAKE_HOME_DIRECTORY:INTERNAL=/home/user/luanti
    # We need to extract the VALUE after the = sign

    # Check if the cached source dir matches current dir
    local cached_src
    cached_src="$(grep '^CMAKE_HOME_DIRECTORY:' "$cache_file" 2>/dev/null | sed 's/^[^=]*=//' | xargs)"
    if [[ -n "$cached_src" ]] && [[ "$cached_src" != "$SCRIPT_DIR" ]]; then
        warn "Stale CMake cache detected!"
        warn "  Cache was built from: ${cached_src}"
        warn "  Current source dir:   ${SCRIPT_DIR}"
        step "Auto-cleaning stale build directory..."
        rm -rf "$BUILD_DIR"
        log "Stale build directory cleaned."
        return 0
    fi

    # Check if the cached build dir matches
    local cached_build
    cached_build="$(grep '^CMAKE_BINARY_DIR:' "$cache_file" 2>/dev/null | sed 's/^[^=]*=//' | xargs)"
    if [[ -n "$cached_build" ]] && [[ "$cached_build" != "$BUILD_DIR" ]]; then
        warn "Stale CMake cache detected (build dir mismatch)!"
        step "Auto-cleaning stale build directory..."
        rm -rf "$BUILD_DIR"
        log "Stale build directory cleaned."
        return 0
    fi

    log "CMake cache is valid."
}

# ─── Map pkg-config / tool name → distro package name ─────────────────────────
# Given a logical name (e.g. "openssl", "cmake", "freetype2") return the
# actual package name for the current distribution.

lib_to_pkg() {
    local lib="$1"
    case "$DISTRO" in
        debian|ubuntu|linuxmint|pop|elementary|kali)
            case "$lib" in
                cmake)       echo "cmake" ;;
                g++)         echo "g++" ;;
                pkg-config)  echo "pkg-config" ;;
                zlib)        echo "zlib1g-dev" ;;
                libzstd)     echo "libzstd-dev" ;;
                sqlite3)     echo "libsqlite3-dev" ;;
                gmp)         echo "libgmp-dev" ;;
                openssl)     echo "libssl-dev" ;;
                jsoncpp)     echo "libjsoncpp-dev" ;;
                luajit)      echo "libluajit-5.1-dev" ;;
                freetype2)   echo "$(resolve_freetype_pkg)" ;;
                gl)          echo "libgl1-mesa-dev" ;;
                sdl2)        echo "libsdl2-dev" ;;
                openal)      echo "libopenal-dev" ;;
                vorbis)      echo "libvorbis-dev" ;;
                ogg)         echo "libogg-dev" ;;
                jpeg)        echo "libjpeg-dev" ;;
                png)         echo "libpng-dev" ;;
                curl)        echo "libcurl4-openssl-dev" ;;
                ncurses)     echo "libncurses-dev" ;;
                gettext)     echo "gettext" ;;
                spatialindex) echo "libspatialindex-dev" ;;
                pq)          echo "libpq-dev" ;;
                leveldb)     echo "libleveldb-dev" ;;
                hiredis)     echo "libhiredis-dev" ;;
                *)           echo "" ;;
            esac
            ;;
        fedora|rhel|centos|rocky|alma|oracle)
            case "$lib" in
                cmake)       echo "cmake" ;;
                g++)         echo "gcc-c++" ;;
                pkg-config)  echo "pkgconfig" ;;
                zlib)        echo "zlib-devel" ;;
                libzstd)     echo "libzstd-devel" ;;
                sqlite3)     echo "sqlite-devel" ;;
                gmp)         echo "gmp-devel" ;;
                openssl)     echo "openssl-devel" ;;
                jsoncpp)     echo "jsoncpp-devel" ;;
                luajit)      echo "luajit-devel" ;;
                freetype2)   echo "freetype-devel" ;;
                gl)          echo "mesa-libGL-devel" ;;
                sdl2)        echo "SDL2-devel" ;;
                openal)      echo "openal-soft-devel" ;;
                vorbis)      echo "libvorbis-devel" ;;
                ogg)         echo "libogg-devel" ;;
                jpeg)        echo "libjpeg-turbo-devel" ;;
                png)         echo "libpng-devel" ;;
                curl)        echo "libcurl-devel" ;;
                ncurses)     echo "ncurses-devel" ;;
                gettext)     echo "gettext-devel" ;;
                spatialindex) echo "spatialindex-devel" ;;
                pq)          echo "postgresql-devel" ;;
                leveldb)     echo "leveldb-devel" ;;
                hiredis)     echo "hiredis-devel" ;;
                *)           echo "" ;;
            esac
            ;;
        arch|manjaro|endeavouros|garuda)
            case "$lib" in
                cmake)       echo "cmake" ;;
                g++)         echo "gcc" ;;
                pkg-config)  echo "pkg-config" ;;
                zlib)        echo "zlib" ;;
                libzstd)     echo "zstd" ;;
                sqlite3)     echo "sqlite" ;;
                gmp)         echo "gmp" ;;
                openssl)     echo "openssl" ;;
                jsoncpp)     echo "jsoncpp" ;;
                luajit)      echo "luajit" ;;
                freetype2)   echo "freetype2" ;;
                gl)          echo "mesa" ;;
                sdl2)        echo "sdl2" ;;
                openal)      echo "openal" ;;
                vorbis)      echo "libvorbis" ;;
                ogg)         echo "libogg" ;;
                jpeg)        echo "libjpeg-turbo" ;;
                png)         echo "libpng" ;;
                curl)        echo "curl" ;;
                ncurses)     echo "ncurses" ;;
                gettext)     echo "gettext" ;;
                spatialindex) echo "spatialindex" ;;
                pq)          echo "postgresql-libs" ;;
                leveldb)     echo "leveldb" ;;
                hiredis)     echo "hiredis" ;;
                *)           echo "" ;;
            esac
            ;;
        alpine)
            case "$lib" in
                cmake)       echo "cmake" ;;
                g++)         echo "g++" ;;
                pkg-config)  echo "pkgconf" ;;
                zlib)        echo "zlib-dev" ;;
                libzstd)     echo "zstd-dev" ;;
                sqlite3)     echo "sqlite-dev" ;;
                gmp)         echo "gmp-dev" ;;
                openssl)     echo "openssl-dev" ;;
                jsoncpp)     echo "jsoncpp-dev" ;;
                luajit)      echo "luajit-dev" ;;
                freetype2)   echo "freetype-dev" ;;
                gl)          echo "mesa-dev" ;;
                sdl2)        echo "sdl2-dev" ;;
                openal)      echo "openal-soft-dev" ;;
                vorbis)      echo "libvorbis-dev" ;;
                ogg)         echo "libogg-dev" ;;
                jpeg)        echo "libjpeg-turbo-dev" ;;
                png)         echo "libpng-dev" ;;
                curl)        echo "curl-dev" ;;
                ncurses)     echo "ncurses-dev" ;;
                gettext)     echo "gettext-dev" ;;
                spatialindex) echo "spatialindex-dev" ;;
                pq)          echo "postgresql-dev" ;;
                leveldb)     echo "leveldb-dev" ;;
                hiredis)     echo "hiredis-dev" ;;
                *)           echo "" ;;
            esac
            ;;
        *)
            # Fallback: return the lib name as-is
            echo "$lib"
            ;;
    esac
}

# ─── Dependency Verification (with auto-install) ───────────────────────────────

verify_dependencies() {
    step "Verifying build dependencies..."

    local -a missing_required_libs=()   # logical lib names (e.g. "openssl")
    local -a missing_optional_libs=()   # logical lib names with bundled fallbacks
    local -a missing_client_libs=()     # logical lib names for client-only

    # ── Check build tools ──
    for tool in cmake g++ pkg-config; do
        if ! command -v "$tool" &>/dev/null; then
            missing_required_libs+=("$tool")
        fi
    done

    # ── Check required libraries (no bundled fallback) ──
    local required_libs=(zlib libzstd sqlite3 gmp openssl)
    for lib in "${required_libs[@]}"; do
        if ! pkg-config --exists "$lib" 2>/dev/null; then
            missing_required_libs+=("$lib")
        fi
    done

    # ── Check optional libraries (CMake has bundled fallbacks) ──
    # jsoncpp — always bundled in lib/jsoncpp/
    # luajit  — CMake falls back to bundled Lua if system LuaJIT not found
    local optional_libs=(jsoncpp luajit)
    for lib in "${optional_libs[@]}"; do
        if ! pkg-config --exists "$lib" 2>/dev/null; then
            missing_optional_libs+=("$lib")
        fi
    done

    if [[ ${#missing_optional_libs[@]} -gt 0 ]]; then
        info "Optional system libraries not found (will use bundled versions):"
        for lib in "${missing_optional_libs[@]}"; do
            local pkg_name
            pkg_name="$(lib_to_pkg "$lib")"
            info "  - ${pkg_name} (bundled fallback available)"
        done
    fi

    # ── Check client-only libraries ──
    if [[ "$BUILD_CLIENT" -eq 1 ]]; then
        local client_libs=(freetype2 gl sdl2)
        for lib in "${client_libs[@]}"; do
            if ! pkg-config --exists "$lib" 2>/dev/null; then
                missing_client_libs+=("$lib")
            fi
        done
    fi

    # ── Combine all missing libs that need packages ──
    local -a all_missing_libs=()
    all_missing_libs+=("${missing_required_libs[@]}")
    if [[ "$BUILD_CLIENT" -eq 1 ]]; then
        all_missing_libs+=("${missing_client_libs[@]}")
    fi

    # ── All good? ──
    if [[ ${#all_missing_libs[@]} -eq 0 ]]; then
        log "All required dependencies verified."
        return 0
    fi

    # ── Map missing libs to distro-specific package names ──
    local -a missing_pkgs=()
    local -a unresolvable=()
    for lib in "${all_missing_libs[@]}"; do
        local pkg_name
        pkg_name="$(lib_to_pkg "$lib")"
        if [[ -n "$pkg_name" ]]; then
            # Deduplicate (a lib might appear in both required + client lists)
            local dup=0
            for existing in "${missing_pkgs[@]}"; do
                if [[ "$existing" == "$pkg_name" ]]; then dup=1; break; fi
            done
            if [[ "$dup" -eq 0 ]]; then
                missing_pkgs+=("$pkg_name")
            fi
        else
            unresolvable+=("$lib")
        fi
    done

    # ── Also include optional missing libs in the install offer ──
    local -a optional_pkgs=()
    for lib in "${missing_optional_libs[@]}"; do
        local pkg_name
        pkg_name="$(lib_to_pkg "$lib")"
        if [[ -n "$pkg_name" ]]; then
            optional_pkgs+=("$pkg_name")
        fi
    done

    # ── Report ──
    echo ""
    echo -e "${BOLD}Missing dependencies detected:${NC}"
    echo ""
    echo -e "  ${RED}Required (build will fail without these):${NC}"
    for pkg in "${missing_pkgs[@]}"; do
        printf "    ${RED}✗${NC} %s\n" "$pkg"
    done
    if [[ ${#optional_pkgs[@]} -gt 0 ]]; then
        echo ""
        echo -e "  ${YELLOW}Optional (bundled fallback available):${NC}"
        for pkg in "${optional_pkgs[@]}"; do
            printf "    ${YELLOW}~${NC} %s\n" "$pkg"
        done
    fi
    echo ""

    if [[ ${#unresolvable[@]} -gt 0 ]]; then
        warn "Could not map these to packages: ${unresolvable[*]}"
        warn "You may need to install them manually."
        echo ""
    fi

    # ── Offer to install ──
    if [[ "$NON_INTERACTIVE" -eq 1 ]]; then
        # Non-interactive: auto-install required, skip optional
        if [[ ${#missing_pkgs[@]} -gt 0 ]]; then
            info "Non-interactive mode: auto-installing required packages..."
            install_packages "${missing_pkgs[@]}"
            save_deps_manifest "${missing_pkgs[@]}"

            # Re-verify after install
            step "Re-verifying dependencies after install..."
            local -a still_missing=()
            for lib in "${all_missing_libs[@]}"; do
                case "$lib" in
                    cmake|g++|pkg-config)
                        if ! command -v "$lib" &>/dev/null; then still_missing+=("$lib"); fi ;;
                    *)
                        if ! pkg-config --exists "$lib" 2>/dev/null; then still_missing+=("$lib"); fi ;;
                esac
            done
            if [[ ${#still_missing[@]} -gt 0 ]]; then
                error "These dependencies are still missing after install: ${still_missing[*]}"
                return 1
            fi
            log "All required dependencies now verified."
        fi
        return 0
    fi

    # Interactive: offer choices
    local -a install_pkgs=("${missing_pkgs[@]}")

    echo -e "  ${CYAN}Choose an action:${NC}"
    echo -e "    ${BOLD}I${NC}  Install required missing packages now"
    if [[ ${#optional_pkgs[@]} -gt 0 ]]; then
        echo -e "    ${BOLD}A${NC}  Install required + optional packages (recommended)"
    fi
    echo -e "    ${BOLD}S${NC}  Skip — continue without installing (build will likely fail)"
    echo -e "    ${BOLD}Q${NC}  Quit"
    echo ""

    while true; do
        read -rp "  > " answer
        answer="$(echo "$answer" | tr '[:upper:]' '[:lower:]' | xargs)"

        case "$answer" in
            i|install)
                step "Installing ${#install_pkgs[@]} required packages..."
                install_packages "${install_pkgs[@]}"
                save_deps_manifest "${install_pkgs[@]}"
                break
                ;;
            a|all)
                # Merge required + optional
                local -a combined=("${missing_pkgs[@]}")
                for opkg in "${optional_pkgs[@]}"; do
                    local dup=0
                    for existing in "${combined[@]}"; do
                        if [[ "$existing" == "$opkg" ]]; then dup=1; break; fi
                    done
                    if [[ "$dup" -eq 0 ]]; then
                        combined+=("$opkg")
                    fi
                done
                step "Installing ${#combined[@]} packages (required + optional)..."
                install_packages "${combined[@]}"
                save_deps_manifest "${combined[@]}"
                break
                ;;
            s|skip)
                warn "Skipping dependency installation. Build will likely fail."
                warn "You can re-run with './build_linux.sh' to install them later."
                return 1
                ;;
            q|quit)
                warn "Build cancelled by user."
                exit 0
                ;;
            *)
                echo -e "  ${RED}Invalid choice. Enter I, A, S, or Q.${NC}"
                ;;
        esac
    done

    # Re-verify after install
    step "Re-verifying dependencies after install..."
    local -a still_missing=()
    for lib in "${all_missing_libs[@]}"; do
        case "$lib" in
            cmake|g++|pkg-config)
                if ! command -v "$lib" &>/dev/null; then still_missing+=("$lib"); fi ;;
            *)
                if ! pkg-config --exists "$lib" 2>/dev/null; then still_missing+=("$lib"); fi ;;
        esac
    done

    if [[ ${#still_missing[@]} -gt 0 ]]; then
        error "These dependencies are still missing after install: ${still_missing[*]}"
        error "You may need to install them manually."
        return 1
    fi

    log "All required dependencies verified."
    return 0
}

# ─── Build ─────────────────────────────────────────────────────────────────────

configure_build() {
    step "Configuring build..."

    # Check for stale cache before configuring
    check_cmake_cache

    local -a cmake_args=(
        -B "$BUILD_DIR"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCMAKE_INSTALL_PREFIX="$PREFIX"
        -DBUILD_CLIENT="${BUILD_CLIENT}"
        -DBUILD_SERVER="${BUILD_SERVER}"
    )

    if [[ "$RUN_TESTS" -eq 1 ]]; then
        cmake_args+=(-DBUILD_UNITTESTS=TRUE)
    else
        cmake_args+=(-DBUILD_UNITTESTS=FALSE)
    fi

    if [[ "$ENABLE_LTO" -eq 1 ]]; then
        cmake_args+=(-DENABLE_LTO=TRUE)
    fi

    if [[ "$RUN_IN_PLACE" -eq 1 ]]; then
        cmake_args+=(-DRUN_IN_PLACE=TRUE)
    fi

    if [[ "$ENABLE_PROMETHEUS" -eq 1 ]]; then
        cmake_args+=(-DENABLE_PROMETHEUS=TRUE)
    fi

    # ── Auto-detect and apply LOCAL_PREFIX ───────────────────────────
    # If LOCAL_PREFIX is not set, try to find it next to the project
    if [[ -z "$LOCAL_PREFIX" ]]; then
        if [[ -d "${SCRIPT_DIR}/local-prefix" ]]; then
            LOCAL_PREFIX="${SCRIPT_DIR}/local-prefix"
        elif [[ -d "${SCRIPT_DIR}/../local-prefix" ]]; then
            LOCAL_PREFIX="$(cd "${SCRIPT_DIR}/../local-prefix" && pwd)"
        fi
    fi

    # If LOCAL_PREFIX exists, add it to CMake and runtime paths
    if [[ -n "$LOCAL_PREFIX" ]] && [[ -d "$LOCAL_PREFIX" ]]; then
        local _arch_suffix
        _arch_suffix="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || echo 'x86_64-linux-gnu')"
        cmake_args+=(-DCMAKE_PREFIX_PATH="${LOCAL_PREFIX}/usr;${LOCAL_PREFIX}/usr/lib/${_arch_suffix}/cmake")
        info "Using local prefix: ${LOCAL_PREFIX}"
        # Set runtime paths so binaries can find local libraries
        export LD_LIBRARY_PATH="${LOCAL_PREFIX}/usr/lib/${_arch_suffix}:${LD_LIBRARY_PATH:-}"
        export PKG_CONFIG_PATH="${LOCAL_PREFIX}/usr/lib/${_arch_suffix}/pkgconfig:${PKG_CONFIG_PATH:-}"
    fi

    if command -v ninja &>/dev/null; then
        cmake_args+=(-G Ninja)
    fi

    if [[ "$VERBOSE" -eq 1 ]]; then
        info "CMake arguments: ${cmake_args[*]}"
    fi

    if ! cmake "${cmake_args[@]}" "${SCRIPT_DIR}" 2>&1 | tee -a "$LOG_FILE"; then
        error "CMake configuration failed!"
        error "Try running with --clean to start fresh."
        return 1
    fi

    log "CMake configuration succeeded."
    return 0
}

run_build() {
    step "Building Luanti (${BUILD_TYPE}, jobs=${JOBS})..."

    local -a build_args=()
    if [[ "$VERBOSE" -eq 1 ]]; then
        build_args+=(--verbose)
    fi

    if ! cmake --build "$BUILD_DIR" -j"${JOBS}" "${build_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        error "Build failed!"
        error "Check the log at: ${LOG_FILE}"
        return 1
    fi

    log "Compilation succeeded."
    return 0
}

run_tests() {
    step "Running unit tests..."

    if command -v ctest &>/dev/null; then
        cd "$BUILD_DIR"
        if ! ctest --output-on-failure -j"${JOBS}"; then
            error "Some tests failed!"
            cd "${SCRIPT_DIR}"
            return 1
        fi
        cd "${SCRIPT_DIR}"
        log "Unit tests passed."
    else
        warn "CTest not found. Trying to run tests manually..."
        if ! cmake --build "$BUILD_DIR" --target test; then
            error "Tests failed!"
            return 1
        fi
    fi
    return 0
}

install_build() {
    step "Installing Luanti to ${PREFIX}..."

    if ! sudo cmake --install "$BUILD_DIR"; then
        error "Installation failed!"
        return 1
    fi
    sudo ldconfig 2>/dev/null || true

    log "Installation complete."
    return 0
}

# ─── Run The Game ──────────────────────────────────────────────────────────────

run_game() {
    step "Launching Luanti..."

    local game_bin=""

    # Luanti puts the binary in ${SOURCE_DIR}/bin/ when not cross-compiling,
    # and in ${BUILD_DIR}/bin/ when cross-compiling.
    # Search in order of likelihood.
    local -a search_paths=(
        "${SCRIPT_DIR}/bin/luanti"
        "${BUILD_DIR}/bin/luanti"
        "${PREFIX}/bin/luanti"
        "${PREFIX}/games/luanti"
        "/usr/local/bin/luanti"
        "/usr/bin/luanti"
    )

    for p in "${search_paths[@]}"; do
        if [[ -x "$p" ]]; then
            game_bin="$p"
            break
        fi
    done

    if [[ -z "$game_bin" ]]; then
        error "Could not find Luanti binary. Was the build successful?"
        return 1
    fi

    info "Running: ${game_bin}"
    echo ""
    "$game_bin"
}

# Find the game binary without launching
find_game_binary() {
    local -a search_paths=(
        "${SCRIPT_DIR}/bin/luanti"
        "${BUILD_DIR}/bin/luanti"
        "${PREFIX}/bin/luanti"
        "${PREFIX}/games/luanti"
        "/usr/local/bin/luanti"
        "/usr/bin/luanti"
    )

    for p in "${search_paths[@]}"; do
        if [[ -x "$p" ]]; then
            echo "$p"
            return 0
        fi
    done
    return 1
}

# ─── Main ──────────────────────────────────────────────────────────────────────

main() {
    # Initialize log
    echo "=== Luanti Build Script ===" > "$LOG_FILE"
    echo "Started: $(date)" >> "$LOG_FILE"
    echo "Distribution: ${DISTRO}" >> "$LOG_FILE"
    echo "Build: CLIENT=${BUILD_CLIENT} SERVER=${BUILD_SERVER} TYPE=${BUILD_TYPE}" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"

    # Handle --remove-deps standalone
    if [[ "$REMOVE_DEPS" -eq 1 ]]; then
        remove_all_deps
        exit $?
    fi

    # ── Build target selection ──
    # If the user didn't explicitly pass --client/--server/--both on the
    # command line and we're in interactive mode, ask what to build.
    if [[ "$BUILD_TARGET_EXPLICIT" -eq 0 ]] && [[ "$NON_INTERACTIVE" -eq 0 ]]; then
        show_build_target_menu
    fi

    # Summary
    info "Luanti Build Configuration:"
    info "  Distribution:    ${DISTRO}"
    info "  Build client:    $([ "$BUILD_CLIENT" -eq 1 ] && echo "yes" || echo "no")"
    info "  Build server:    $([ "$BUILD_SERVER" -eq 1 ] && echo "yes" || echo "no")"
    info "  Build type:      ${BUILD_TYPE}"
    info "  Run tests:       $([ "$RUN_TESTS" -eq 1 ] && echo "yes" || echo "no")"
    info "  Install:         $([ "$DO_INSTALL" -eq 1 ] && echo "yes" || echo "no")"
    info "  Run after build: $([ "$DO_RUN" -eq 1 ] && echo "yes" || echo "no")"
    info "  Prefix:          ${PREFIX}"
    info "  Jobs:            ${JOBS}"
    info "  Run in place:    $([ "$RUN_IN_PLACE" -eq 1 ] && echo "yes" || echo "no")"
    info "  LTO:             $([ "$ENABLE_LTO" -eq 1 ] && echo "yes" || echo "no")"
    info "  Prometheus:      $([ "$ENABLE_PROMETHEUS" -eq 1 ] && echo "yes" || echo "no")"
    info "  Interactive:     $([ "$NON_INTERACTIVE" -eq 1 ] && echo "no" || echo "yes")"
    echo ""

    # ── Fast path: --run with existing binary ──
    # If --run is set and the binary already exists, skip the build entirely.
    if [[ "$DO_RUN" -eq 1 ]] && [[ "$NO_DEPS" -eq 1 ]] && [[ "$DO_CLEAN" -ne 1 ]]; then
        local existing_bin=""
        existing_bin="$(find_game_binary)"
        if [[ -n "$existing_bin" ]]; then
            info "Found existing binary: ${existing_bin}"
            info "Skipping build (use --clean to force rebuild)."
            echo ""
            run_game || exit 1
            exit 0
        fi
    fi

    # Step 1: Install dependencies
    if [[ "$NO_DEPS" -ne 1 ]]; then
        install_dependencies || exit 1

        # If deps-only, stop here
        if [[ "$DEPS_ONLY" -eq 1 ]]; then
            log "Dependencies installed (--deps-only). Exiting."
            exit 0
        fi
    else
        info "Skipping initial dependency menu (--no-deps)."
        info "Dependencies will still be verified before build."
    fi

    # Step 2: Verify dependencies (with auto-install offer for missing ones)
    # This always runs, even with --no-deps. If deps are missing,
    # the user is offered to install them right here.
    verify_dependencies || exit 1

    # If deps-only and we got here (user used --no-deps + --deps-only),
    # the verify step already installed any missing deps.
    if [[ "$DEPS_ONLY" -eq 1 ]]; then
        log "Dependencies verified (--deps-only). Exiting."
        exit 0
    fi

    # Step 3: Clean if requested
    if [[ "$DO_CLEAN" -eq 1 ]] && [[ -d "$BUILD_DIR" ]]; then
        step "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        log "Build directory cleaned."
    fi

    # Step 4: Configure
    configure_build || exit 1

    # Step 5: Build
    run_build || exit 1

    # Step 6: Test
    if [[ "$RUN_TESTS" -eq 1 ]]; then
        run_tests || exit 1
    fi

    # Step 7: Install
    if [[ "$DO_INSTALL" -eq 1 ]]; then
        install_build || exit 1
    fi

    # Step 8: Run
    if [[ "$DO_RUN" -eq 1 ]]; then
        run_game || exit 1
    fi

    # Success
    echo ""
    log "Build completed successfully!"
    info "Build directory: ${BUILD_DIR}"

    if [[ "$BUILD_CLIENT" -eq 1 ]]; then
        local client_path=""
        client_path="$(find_game_binary)"
        if [[ -n "$client_path" ]]; then
            info "Client binary:  ${client_path}"
        fi
    fi

    if [[ "$BUILD_SERVER" -eq 1 ]]; then
        local server_path=""
        # Server binary is alongside the client
        local client_loc
        client_loc="$(find_game_binary)"
        if [[ -n "$client_loc" ]]; then
            local srv_dir
            srv_dir="$(dirname "$client_loc")"
            if [[ -x "${srv_dir}/luantiserver" ]]; then
                server_path="${srv_dir}/luantiserver"
            fi
        fi
        if [[ -n "$server_path" ]]; then
            info "Server binary:  ${server_path}"
        fi
    fi

    # Hint for running
    if [[ "$DO_RUN" -ne 1 ]]; then
        local hint_bin
        hint_bin="$(find_game_binary)"
        echo ""
        info "To run the game:"
        if [[ -n "$hint_bin" ]]; then
            info "  ./build_linux.sh --run --no-deps"
            info "  # or directly:"
            info "  ${hint_bin}"
        fi
    fi

    echo ""
    echo "Finished: $(date)" >> "$LOG_FILE"
}

main "$@"
