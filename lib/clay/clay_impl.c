/*
Luantis — Clay C Implementation Bridge
This file compiles Clay's implementation (clay.h) as C99 code,
since Clay requires C99/C++20 designated initializers which are
not available in C++17 (the standard Luantis uses).
*/

#define CLAY_IMPLEMENTATION
#include "clay.h"
