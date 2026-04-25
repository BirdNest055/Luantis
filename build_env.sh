#!/usr/bin/env bash
# Source this file before running build_linux.sh
# This sets up the environment for building with locally-installed dependencies
#
# Usage:
#   source build_env.sh
#   ./build_linux.sh --non-interactive --no-deps --both --run-in-place
#
# Or configure CMake directly:
#   source build_env.sh
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=1 -DBUILD_SERVER=1 \
#       -DRUN_IN_PLACE=TRUE -DOPENAL_INCLUDE_DIR="$LOCAL_PREFIX/usr/include" .

LOCAL_PREFIX="/home/z/my-project/local-prefix"

# Add cmake (installed via pip) and local-prefix binaries to PATH
export PATH="/home/z/.local/bin:$LOCAL_PREFIX/usr/bin:$PATH"

# pkg-config paths so CMake can find libraries via pkg-config
export PKG_CONFIG_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig"

# CMake prefix path for find_package() and find_library()
export CMAKE_PREFIX_PATH="$LOCAL_PREFIX/usr:$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu/cmake:/usr"

# Library search paths for linking
export LIBRARY_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"

# Runtime library path for executing the built binaries
export LD_LIBRARY_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"

# NOTE: Do NOT set C_INCLUDE_PATH or CPLUS_INCLUDE_PATH as they can cause
# #include_next issues with system headers (e.g., <cmath> not finding <math.h>).
# Instead, rely on PKG_CONFIG_PATH and CMAKE_PREFIX_PATH which let CMake
# discover the correct include paths for each library individually.

# NOTE: Set OPENAL_INCLUDE_DIR to the parent of AL/ when configuring CMake,
# because CMake's FindOpenAL sets it to .../include/AL by default, but
# the code uses #include <AL/al.h> which needs .../include as the search path.

echo "Build environment configured with local prefix: $LOCAL_PREFIX"
echo ""
echo "To build with build_linux.sh:"
echo "  source build_env.sh"
echo "  ./build_linux.sh --non-interactive --no-deps --both --run-in-place --clean"
echo ""
echo "To configure CMake directly (more control):"
echo "  source build_env.sh"
echo "  rm -rf build"
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=1 -DBUILD_SERVER=1 \\"
echo "      -DRUN_IN_PLACE=TRUE -DOPENAL_INCLUDE_DIR=\"\$LOCAL_PREFIX/usr/include\" ."
echo "  cmake --build build -j\$(nproc)"
echo ""
echo "To run the built binaries:"
echo "  LD_LIBRARY_PATH=\"\$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu\" ./bin/luanti --version"
echo "  LD_LIBRARY_PATH=\"\$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu\" ./bin/luantiserver --version"
