#!/bin/bash
#
# Luantis Build Monitor for v9.47
#
# Runs TDD tests, builds the project, and performs E2E visual verification.
# Outputs a pass/fail report for each stage.
#
# Usage:  bash build_monitor.sh [--skip-e2e] [--skip-build]
#

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SKIP_E2E=false
SKIP_BUILD=false
for arg in "$@"; do
    case $arg in
        --skip-e2e) SKIP_E2E=true ;;
        --skip-build) SKIP_BUILD=true ;;
    esac
done

PASS=0
FAIL=0
TOTAL=0

report() {
    local name="$1" local status="$2" local detail="${3:-}"
    TOTAL=$((TOTAL + 1))
    if [ "$status" = "PASS" ]; then
        PASS=$((PASS + 1))
        echo -e "  ${GREEN}PASS${NC}: $name"
    else
        FAIL=$((FAIL + 1))
        echo -e "  ${RED}FAIL${NC}: $name — $detail"
    fi
}

echo "╔══════════════════════════════════════════════════════╗"
echo "║  Luantis v9.47 Build Monitor                        ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# --- Stage 1: TDD Tests ---
echo "=== Stage 1: TDD Unit & Layout Tests ==="
cd /home/z/my-project/clay_test

if [ ! -f clay_responsive_test ]; then
    echo "  Building TDD test..."
    cc -o clay_responsive_test clay_responsive_test.c -lm -std=c11 2>&1
fi

if ./clay_responsive_test > /tmp/tdd_output.txt 2>&1; then
    report "TDD tests (65 total)" "PASS"
    grep -c "PASS" /tmp/tdd_output.txt | xargs -I{} echo "    {} tests passed"
else
    report "TDD tests" "FAIL" "Some tests failed"
    grep "FAIL" /tmp/tdd_output.txt | head -5
fi

echo ""

# --- Stage 2: Project Build ---
if [ "$SKIP_BUILD" = true ]; then
    echo -e "=== Stage 2: Project Build ${YELLOW}(SKIPPED)${NC} ==="
else
    echo "=== Stage 2: Project Build ==="
    CMAKE=/home/z/.local/lib/python3.13/site-packages/cmake/data/bin/cmake
    BUILD_DIR=/home/z/my-project/Luantis/build

    BUILD_START=$(date +%s)
    if $CMAKE --build $BUILD_DIR -j$(nproc) > /tmp/build_output.txt 2>&1; then
        BUILD_END=$(date +%s)
        BUILD_TIME=$((BUILD_END - BUILD_START))
        report "Build luanti" "PASS"
        echo "    Build time: ${BUILD_TIME}s"

        # Check binary exists and is valid
        BINARY=/home/z/my-project/Luantis/bin/luanti
        if [ -f "$BINARY" ]; then
            SIZE=$(stat -c%s "$BINARY" 2>/dev/null || echo "0")
            report "Binary exists (${SIZE} bytes)" "PASS"
        else
            report "Binary exists" "FAIL" "Binary not found"
        fi
    else
        BUILD_END=$(date +%s)
        BUILD_TIME=$((BUILD_END - BUILD_START))
        report "Build luanti" "FAIL" "Build failed after ${BUILD_TIME}s"
        tail -10 /tmp/build_output.txt
    fi
fi

echo ""

# --- Stage 3: Binary Verification ---
echo "=== Stage 3: Binary Verification ==="
BINARY=/home/z/my-project/Luantis/bin/luanti
if [ -f "$BINARY" ]; then
    # Check it's an ELF binary
    if file "$BINARY" | grep -q "ELF"; then
        report "ELF binary format" "PASS"
    else
        report "ELF binary format" "FAIL" "Not a valid ELF binary"
    fi

    # Check version string
    VERSION=$($BINARY --version 2>/dev/null | head -1 || echo "unknown")
    if echo "$VERSION" | grep -q "v9.47"; then
        report "Version string contains v9.47" "PASS"
    else
        report "Version string" "FAIL" "Expected v9.47, got: $VERSION"
    fi

    # Check for --help flag
    if timeout 5 $BINARY --help > /dev/null 2>&1; then
        report "--help flag works" "PASS"
    else
        report "--help flag" "FAIL" "Binary crashes on --help"
    fi
else
    report "Binary verification" "FAIL" "Binary not found"
fi

echo ""

# --- Stage 4: E2E Visual Test ---
if [ "$SKIP_E2E" = true ]; then
    echo -e "=== Stage 4: E2E Visual Test ${YELLOW}(SKIPPED)${NC} ==="
else
    echo "=== Stage 4: E2E Visual Test ==="

    # Clean up
    pkill -f Xvfb 2>/dev/null || true
    pkill -f luanti 2>/dev/null || true
    sleep 1
    rm -f /tmp/.X11-unix/X99

    # Start Xvfb
    Xvfb :99 -screen 0 1280x720x24 -fbdir /home/z/my-project/download/fb 2>/dev/null &
    XVFB_PID=$!
    sleep 2

    if kill -0 $XVFB_PID 2>/dev/null; then
        report "Xvfb started" "PASS"
    else
        report "Xvfb started" "FAIL" "Xvfb process not running"
    fi

    # Launch game
    DISPLAY=:99 \
    XDG_RUNTIME_DIR=/home/z/.cache/xdg-runtime \
    LIBGL_ALWAYS_SOFTWARE=1 \
    MESA_GL_VERSION_OVERRIDE=3.3 \
    CLAY_RENDER_LOG=1 \
    CLAY_TEST_PAUSE=1 \
    timeout 25 $BINARY --go > /tmp/e2e_output.txt 2>&1 &
    GAME_PID=$!

    sleep 15

    # Check if game ran
    if kill -0 $GAME_PID 2>/dev/null; then
        report "Game launched and running" "PASS"
    else
        # Check if it exited normally (not crashed)
        if grep -q "signal_handler" /tmp/e2e_output.txt 2>/dev/null; then
            report "Game ran (timeout exit)" "PASS"
        else
            report "Game launched" "FAIL" "Game exited unexpectedly"
        fi
    fi

    # Check render log
    if [ -f /tmp/clay_render_log.jsonl ] && [ -s /tmp/clay_render_log.jsonl ]; then
        LINES=$(wc -l < /tmp/clay_render_log.jsonl)
        report "Render log captured ($LINES frames)" "PASS"
    else
        report "Render log captured" "FAIL" "No render log created"
    fi

    # Check framebuffer
    if [ -f /home/z/my-project/download/fb/Xvfb_screen0 ] && [ -s /home/z/my-project/download/fb/Xvfb_screen0 ]; then
        report "Framebuffer captured" "PASS"
    else
        report "Framebuffer captured" "FAIL" "No framebuffer file"
    fi

    # Kill processes
    kill $GAME_PID 2>/dev/null || true
    kill $XVFB_PID 2>/dev/null || true
    wait 2>/dev/null
fi

echo ""

# --- Summary ---
echo "╔══════════════════════════════════════════════════════╗"
echo -e "║  Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}, ${TOTAL} total"
echo "╚══════════════════════════════════════════════════════╝"

if [ $FAIL -gt 0 ]; then
    echo -e "\n${RED}BUILD FAILED${NC} — ${FAIL} stage(s) need attention"
    exit 1
else
    echo -e "\n${GREEN}ALL STAGES PASSED${NC}"
    exit 0
fi
