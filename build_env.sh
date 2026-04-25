#!/usr/bin/env bash
# Source this file before running build_linux.sh
# This sets up the environment for building with locally-installed dependencies
#
# Usage:
#   source build_env.sh                                    # auto-detect local-prefix
#   source build_env.sh /path/to/local-prefix              # explicit local prefix
#   LOCAL_PREFIX=/opt/my-prefix source build_env.sh        # override via env var
#
# Then build:
#   ./build_linux.sh --non-interactive --no-deps --both --run-in-place
#
# Or configure CMake directly:
#   source build_env.sh
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=1 -DBUILD_SERVER=1 \
#       -DRUN_IN_PLACE=TRUE .
#

# ── Auto-detect script directory ─────────────────────────────────────────────
_BUILD_ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Determine LOCAL_PREFIX ───────────────────────────────────────────────────
# Priority:
#   1. Already set in environment (LOCAL_PREFIX=/opt/foo source build_env.sh)
#   2. Passed as first argument (source build_env.sh /opt/foo)
#   3. Auto-detect: look for local-prefix/ next to this script
#   4. Auto-detect: look for local-prefix/ one directory up (parent of project)
#   5. Empty string (system paths only — for CI or machines with system deps)

if [[ -n "${LOCAL_PREFIX:-}" ]]; then
    : # already set via environment
elif [[ -n "${1:-}" ]] && [[ -d "$1" ]]; then
    LOCAL_PREFIX="$1"
elif [[ -d "${_BUILD_ENV_DIR}/local-prefix" ]]; then
    LOCAL_PREFIX="${_BUILD_ENV_DIR}/local-prefix"
elif [[ -d "${_BUILD_ENV_DIR}/../local-prefix" ]]; then
    LOCAL_PREFIX="$(cd "${_BUILD_ENV_DIR}/../local-prefix" && pwd)"
else
    LOCAL_PREFIX=""
fi

# ── Detect architecture suffix ────────────────────────────────────────────────
_ARCH_SUFFIX="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || echo 'x86_64-linux-gnu')"

# ── Set up environment ───────────────────────────────────────────────────────
if [[ -n "$LOCAL_PREFIX" ]]; then
    # Add local-prefix binaries to PATH (e.g. cmake installed via pip)
    export PATH="${LOCAL_PREFIX}/usr/bin:${PATH}"

    # pkg-config paths so CMake can find libraries via pkg-config
    export PKG_CONFIG_PATH="${LOCAL_PREFIX}/usr/lib/${_ARCH_SUFFIX}/pkgconfig:${LOCAL_PREFIX}/usr/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

    # CMake prefix path for find_package() and find_library()
    export CMAKE_PREFIX_PATH="${LOCAL_PREFIX}/usr:${LOCAL_PREFIX}/usr/lib/${_ARCH_SUFFIX}/cmake:${CMAKE_PREFIX_PATH:-}"

    # Library search paths for linking
    export LIBRARY_PATH="${LOCAL_PREFIX}/usr/lib/${_ARCH_SUFFIX}:${LIBRARY_PATH:-}"

    # Runtime library path for executing the built binaries
    export LD_LIBRARY_PATH="${LOCAL_PREFIX}/usr/lib/${_ARCH_SUFFIX}:${LD_LIBRARY_PATH:-}"
fi

# Also add user's pip bin to PATH (for cmake installed via pip)
export PATH="${HOME}/.local/bin:${PATH}"

# ── NOTE about include paths ─────────────────────────────────────────────────
# Do NOT set C_INCLUDE_PATH or CPLUS_INCLUDE_PATH as they can cause
# #include_next issues with system headers (e.g., <cmath> not finding <math.h>).
# Instead, rely on PKG_CONFIG_PATH and CMAKE_PREFIX_PATH which let CMake
# discover the correct include paths for each library individually.
#
# For the SDL2 version check in irr/src/CMakeLists.txt, the build system
# now auto-discovers extra include dirs from CMAKE_PREFIX_PATH, so no
# manual OPENAL_INCLUDE_DIR override should be needed in most cases.

# ── Print configuration ──────────────────────────────────────────────────────
echo "Build environment configured."
echo ""
if [[ -n "$LOCAL_PREFIX" ]]; then
    echo "  Local prefix:  $LOCAL_PREFIX"
    echo "  Arch suffix:   $_ARCH_SUFFIX"
else
    echo "  Local prefix:  (none — using system libraries only)"
fi
echo "  CMAKE_PREFIX_PATH:  ${CMAKE_PREFIX_PATH:-<not set>}"
echo "  PKG_CONFIG_PATH:    ${PKG_CONFIG_PATH:-<not set>}"
echo ""
echo "To build with build_linux.sh:"
echo "  ./build_linux.sh --non-interactive --no-deps --both --run-in-place --clean"
echo ""
echo "To configure CMake directly:"
echo "  rm -rf build"
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=1 -DBUILD_SERVER=1 \\"
echo "      -DRUN_IN_PLACE=TRUE ."
echo "  cmake --build build -j\$(nproc)"
if [[ -n "$LOCAL_PREFIX" ]]; then
    echo ""
    echo "To run the built binaries:"
    echo "  LD_LIBRARY_PATH=\"${LOCAL_PREFIX}/usr/lib/${_ARCH_SUFFIX}\" ./bin/luanti --version"
    echo "  LD_LIBRARY_PATH=\"${LOCAL_PREFIX}/usr/lib/${_ARCH_SUFFIX}\" ./bin/luantiserver --version"
fi
