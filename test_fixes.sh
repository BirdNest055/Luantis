#!/bin/bash
# Test-driven validation of all v9.1 fixes:
# 1. SRP "SRPUser is NULL" fix
# 2. Settingtypes context annotations
# 3. start_server.sh gameid fix

PASS=0
FAIL=0

pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }
header() { echo ""; echo "=== $1 ==="; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ============================================================
# Test 1: start_server.sh uses correct gameid
# ============================================================
header "Test 1: start_server.sh gameid"

if grep -q '\-\-gameid devtest' "$SCRIPT_DIR/start_server.sh"; then
    pass "start_server.sh uses --gameid devtest"
else
    fail "start_server.sh does NOT use --gameid devtest"
fi

if grep -q '\-\-gameid minetest' "$SCRIPT_DIR/start_server.sh"; then
    fail "start_server.sh still uses --gameid minetest (will cause 'Game not found' error)"
else
    pass "start_server.sh does not use --gameid minetest"
fi

if [ -d "$SCRIPT_DIR/games/devtest" ]; then
    pass "games/devtest directory exists"
else
    fail "games/devtest directory does NOT exist"
fi

if [ -f "$SCRIPT_DIR/games/devtest/game.conf" ]; then
    pass "games/devtest/game.conf exists"
else
    fail "games/devtest/game.conf does NOT exist"
fi

# ============================================================
# Test 2: SRP NULL-check fix in client.cpp
# ============================================================
header "Test 2: SRP NULL-check fix (client.cpp)"

if grep -q 'if (!m_auth_data)' "$SCRIPT_DIR/src/client/client.cpp"; then
    pass "FIRST_SRP path: NULL check after srp_user_new() present"
else
    fail "FIRST_SRP path: NULL check after srp_user_new() MISSING"
fi

if grep -q 'srp_user_new() failed for SRP auth' "$SCRIPT_DIR/src/client/client.cpp"; then
    pass "SRP path: NULL check after srp_user_new() present"
else
    fail "SRP path: NULL check after srp_user_new() MISSING"
fi

if grep -q 'FATAL_ERROR.*srp_user_start_authentication' "$SCRIPT_DIR/src/client/client.cpp"; then
    fail "FATAL_ERROR still used for srp_user_start_authentication (should be graceful)"
else
    pass "No FATAL_ERROR for srp_user_start_authentication (graceful failure)"
fi

if grep -q 'TOSERVER_SRP_BYTES_A' "$SCRIPT_DIR/src/client/client.cpp"; then
    pass "FIRST_SRP path sends TOSERVER_SRP_BYTES_A (v9 fix present)"
else
    fail "FIRST_SRP path does NOT send TOSERVER_SRP_BYTES_A (v9 fix MISSING)"
fi

# ============================================================
# Test 3: Settingtypes context annotations
# ============================================================
header "Test 3: Settingtypes context annotations"

ENCRYPTION_SETTINGS=(
    "secure_connection"
    "show_security_overlay"
    "show_connection_info"
    "show_enc_session_overlay"
    "show_enc_packets_overlay"
    "show_enc_cipher_overlay"
    "show_enc_score_overlay"
    "show_enc_authfail_overlay"
    "show_enc_nonce_overlay"
    "show_enc_latency_overlay"
    "show_enc_timeline_overlay"
    "show_enc_pfs_overlay"
    "show_enc_trust_overlay"
    "show_enc_health_overlay"
    "show_enc_bandwidth_overlay"
)

for setting in "${ENCRYPTION_SETTINGS[@]}"; do
    if grep -q "^${setting} .* \[client\]" "$SCRIPT_DIR/builtin/settingtypes.txt"; then
        pass "$setting has [client] context annotation"
    else
        fail "$setting MISSING [client] context annotation"
    fi
done

# ============================================================
# Test 4: SRP unit tests exist
# ============================================================
header "Test 4: SRP unit tests exist"

if [ -f "$SCRIPT_DIR/src/unittest/test_srp.cpp" ]; then
    pass "test_srp.cpp exists"
    if grep -q 'testUserNewNullUsername\|testUserNewEmptyPassword' "$SCRIPT_DIR/src/unittest/test_srp.cpp"; then
        pass "test_srp.cpp has NULL/empty input tests"
    else
        fail "test_srp.cpp missing NULL/empty input tests"
    fi
else
    fail "test_srp.cpp does NOT exist"
fi

if [ -f "$SCRIPT_DIR/src/unittest/test_peer_encryption_state.cpp" ]; then
    pass "test_peer_encryption_state.cpp exists"
else
    fail "test_peer_encryption_state.cpp does NOT exist"
fi

if [ -f "$SCRIPT_DIR/src/unittest/test_ecdh_x25519.cpp" ]; then
    pass "test_ecdh_x25519.cpp exists"
else
    fail "test_ecdh_x25519.cpp does NOT exist"
fi

if [ -f "$SCRIPT_DIR/src/unittest/test_security_score_v91.cpp" ]; then
    pass "test_security_score_v91.cpp exists"
else
    fail "test_security_score_v91.cpp does NOT exist"
fi

# ============================================================
# Summary
# ============================================================
header "Test Summary"
TOTAL=$((PASS + FAIL))
echo "  Passed: $PASS / $TOTAL"
echo "  Failed: $FAIL / $TOTAL"

if [ "$FAIL" -eq 0 ]; then
    echo ""
    echo "  ALL TESTS PASSED!"
    exit 0
else
    echo ""
    echo "  SOME TESTS FAILED - review above"
    exit 1
fi
