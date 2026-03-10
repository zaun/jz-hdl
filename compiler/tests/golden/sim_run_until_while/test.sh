#!/bin/bash
# Golden test: verify @run_until and @run_while directives.
#
# Usage: ./test.sh <path/to/jz-hdl>
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
JZ_HDL="${1:-${SCRIPT_DIR}/../../../build/jz-hdl}"

if [ ! -x "$JZ_HDL" ]; then
    echo "FAIL: jz-hdl binary not found at $JZ_HDL"
    exit 1
fi

PASS=0
FAIL=0

echo "Testing @run_until and @run_while..."

# Test 1: Successful @run_until and @run_while
SIM_OUTPUT=$("$JZ_HDL" --simulate "${SCRIPT_DIR}/test.jz" --vcd -o /tmp/run_cond_golden.vcd 2>&1) || true

if echo "$SIM_OUTPUT" | grep -q "VCD written to"; then
    if echo "$SIM_OUTPUT" | grep -q "RUNTIME ERROR"; then
        echo "  FAIL: @run_until/@run_while should not produce runtime error"
        echo "        output: $SIM_OUTPUT"
        FAIL=$((FAIL + 1))
    else
        echo "  PASS: @run_until and @run_while complete without error"
        PASS=$((PASS + 1))
    fi
else
    echo "  FAIL: simulation did not produce VCD"
    echo "        output: $SIM_OUTPUT"
    FAIL=$((FAIL + 1))
fi
rm -f /tmp/run_cond_golden.vcd

# Test 2: Timeout produces runtime error
TIMEOUT_OUTPUT=$("$JZ_HDL" --simulate "${SCRIPT_DIR}/test_timeout.jz" --vcd -o /tmp/run_timeout_golden.vcd 2>&1) || true

if echo "$TIMEOUT_OUTPUT" | grep -q "TIMEOUT.*run_until"; then
    echo "  PASS: @run_until timeout produces TIMEOUT error"
    PASS=$((PASS + 1))
else
    echo "  FAIL: expected TIMEOUT error for @run_until with insufficient timeout"
    echo "        output: $TIMEOUT_OUTPUT"
    FAIL=$((FAIL + 1))
fi
rm -f /tmp/run_timeout_golden.vcd

# Test 3: Verbose output shows condition info
VERBOSE_OUTPUT=$("$JZ_HDL" --simulate "${SCRIPT_DIR}/test.jz" --vcd -o /tmp/run_cond_verbose.vcd --verbose 2>&1) || true

if echo "$VERBOSE_OUTPUT" | grep -q "@run_until(count =="; then
    echo "  PASS: verbose output shows @run_until condition"
    PASS=$((PASS + 1))
else
    echo "  FAIL: verbose output missing @run_until info"
    echo "        output: $VERBOSE_OUTPUT"
    FAIL=$((FAIL + 1))
fi

if echo "$VERBOSE_OUTPUT" | grep -q "@run_while(count !="; then
    echo "  PASS: verbose output shows @run_while condition"
    PASS=$((PASS + 1))
else
    echo "  FAIL: verbose output missing @run_while info"
    echo "        output: $VERBOSE_OUTPUT"
    FAIL=$((FAIL + 1))
fi
rm -f /tmp/run_cond_verbose.vcd

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
