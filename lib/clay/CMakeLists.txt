# Clay is a single-header C layout library with zero dependencies.
# The implementation is compiled as C99 (required for designated initializers)
# while the rest of Luantis uses C++17.

# Compile clay_impl.c as C99 — it contains #define CLAY_IMPLEMENTATION + #include "clay.h"
add_library(clay_impl STATIC clay_impl.c)
target_include_directories(clay_impl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Also create an INTERFACE library for header-only usage from C++ code
add_library(clay INTERFACE)
target_link_libraries(clay INTERFACE clay_impl)
target_include_directories(clay INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
