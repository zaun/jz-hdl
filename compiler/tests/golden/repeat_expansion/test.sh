#!/bin/bash
# Golden test: verify @repeat expansion works in both @testbench and @simulation.
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

echo "Testing @repeat expansion..."

# Test 1: Testbench with @repeat — counter advances 3 cycles via @repeat
TB_OUTPUT=$("$JZ_HDL" --test "${SCRIPT_DIR}/test.jz" 2>&1) || true

if echo "$TB_OUTPUT" | grep -q "1 passed, 0 failed"; then
    echo "  PASS: testbench @repeat correctly expands 3 clock advances"
    PASS=$((PASS + 1))
else
    echo "  FAIL: testbench @repeat test"
    echo "        output: $TB_OUTPUT"
    FAIL=$((FAIL + 1))
fi

# Test 2: Simulation with @repeat runs without error
SIM_OUTPUT=$("$JZ_HDL" --simulate "${SCRIPT_DIR}/test_sim.jz" --vcd -o /tmp/repeat_golden.vcd 2>&1) || true

if echo "$SIM_OUTPUT" | grep -q "@simulation counter"; then
    echo "  PASS: simulation @repeat runs successfully"
    PASS=$((PASS + 1))
else
    echo "  FAIL: simulation @repeat run"
    echo "        output: $SIM_OUTPUT"
    FAIL=$((FAIL + 1))
fi

# Test 3: VCD file was produced (confirms @run directives expanded)
if [ -f /tmp/repeat_golden.vcd ]; then
    echo "  PASS: simulation @repeat produced VCD output"
    PASS=$((PASS + 1))
    rm -f /tmp/repeat_golden.vcd
else
    echo "  FAIL: no VCD file produced"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
