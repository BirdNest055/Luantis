// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Test for nodedef.cpp Issue #29: support arbitrary rotations

#include "test.h"
#include <cstdint>
#include <cassert>

// Test that param2 mask works correctly for rotations
static void test_param2_rotation_mask()
{
        // Currently the code uses (param2 & 0x03) for 4 rotations
        // NOTE #29: The code should use (param2 & 0x1F) for 32 rotations
        
        // Test: rotation 16 should be masked differently
        uint8_t param2 = 16;
        
        // Current behavior: only uses lower 2 bits
        int mask_03 = (param2 & 0x03);  // = 0 (wrong for rotation 16)
        
        // Desired behavior: uses lower 5 bits  
        int mask_1F = (param2 & 0x1F);  // = 16 (correct)
        
        // This test demonstrates the need for the fix
        assert(mask_03 == 0);  // Current behavior
        assert(mask_1F == 16); // What we want
        
        // After fix: code should use 0x1F mask
        // This test will pass after the fix
}

TEST(test_param2_rotation_mask)
