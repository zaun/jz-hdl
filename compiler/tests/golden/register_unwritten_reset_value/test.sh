#!/bin/bash
# test.sh - Verify that a register with a reset value that is never written
#           in a SYNCHRONOUS block still gets its reset assignment emitted.
#
# Usage: ./test.sh [path/to/jz-hdl]
#   If no binary is given, defaults to ../../build/jz-hdl relative to this script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
JZ_HDL="${1:-${SCRIPT_DIR}/../../../build/jz-hdl}"

if [ ! -x "$JZ_HDL" ]; then
    echo "FAIL: jz-hdl binary not found at $JZ_HDL"
    exit 1
fi

# Generate Verilog from the test source
VERILOG=$("$JZ_HDL" --verilog "${SCRIPT_DIR}/test.jz")

PASS=0
FAIL=0

check() {
    local description="$1"
    local pattern="$2"

    if echo "$VERILOG" | grep -qF "$pattern"; then
        echo "  PASS: $description"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $description"
        echo "        expected to find: $pattern"
        FAIL=$((FAIL + 1))
    fi
}

echo "Checking unwritten register reset in generated Verilog..."

# The 'factor' register is declared with reset value 8'hAB but never written
# in the SYNCHRONOUS block. It must still appear in the reset block.
check "unwritten register: factor declared" \
      "reg [7:0] factor"

check "unwritten register: factor reset to 0xAB" \
      "factor <= 8'b10101011"

# The 'data' register is written normally and should also be reset.
check "written register: data reset to zero" \
      "data <= 8'b00000000"

# factor should be used combinationally in the multiply
check "unwritten register: factor used in multiply" \
      "assign scaled = d_in * factor"

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
