#!/usr/bin/env bash
# Source this file before running build_linux.sh
# This sets up the environment for building with locally-installed dependencies
#
# Usage:
#   source build_env.sh
#   ./build_linux.sh --non-interactive --no-deps --client --run-in-place

LOCAL_PREFIX="/home/z/my-project/local-prefix"

export PKG_CONFIG_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:$LOCAL_PREFIX/usr/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig"
export CMAKE_PREFIX_PATH="$LOCAL_PREFIX/usr:$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu/cmake:/usr"
export C_INCLUDE_PATH="$LOCAL_PREFIX/usr/include:$LOCAL_PREFIX/usr/include/x86_64-linux-gnu:/usr/include:/usr/include/x86_64-linux-gnu"
export CPLUS_INCLUDE_PATH="$LOCAL_PREFIX/usr/include:$LOCAL_PREFIX/usr/include/x86_64-linux-gnu:/usr/include:/usr/include/x86_64-linux-gnu"
export LIBRARY_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"

# CMake flags for proper include and library path resolution
export CMAKE_EXTRA_FLAGS="-DCMAKE_C_FLAGS='-isystem $LOCAL_PREFIX/usr/include -isystem $LOCAL_PREFIX/usr/include/x86_64-linux-gnu' -DCMAKE_CXX_FLAGS='-isystem $LOCAL_PREFIX/usr/include -isystem $LOCAL_PREFIX/usr/include/x86_64-linux-gnu' -DCMAKE_EXE_LINKER_FLAGS='-L$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu -Wl,-rpath,$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu' -DCMAKE_SHARED_LINKER_FLAGS='-L$LOCAL_PREFIX/usr/lib/x86_64-linux-gnu'"

echo "Build environment configured with local prefix: $LOCAL_PREFIX"
echo "You can now run: ./build_linux.sh --non-interactive --no-deps --client --run-in-place"
