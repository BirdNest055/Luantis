set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Include directories
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/include -I${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/include/x86_64-linux-gnu -I/usr/include -I/usr/include/x86_64-linux-gnu")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/include -I${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/include/x86_64-linux-gnu -I/usr/include -I/usr/include/x86_64-linux-gnu")

# Library directories
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/lib/x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/lib/x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu")

# Prefix path for CMake find_package
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/lib/x86_64-linux-gnu/cmake")

# PKG_CONFIG path
set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/lib/x86_64-linux-gnu/pkgconfig:${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig")

# RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_LIST_DIR}/../local-prefix/usr/lib/x86_64-linux-gnu")
