#!/bin/bash
# test.sh - Verify that the generated Verilog contains correctly extended literal values.
#
# Usage: ./test.sh [path/to/jz-hdl]
#   If no binary is given, defaults to ../../build/jz-hdl relative to this script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
JZ_HDL="${1:-${SCRIPT_DIR}/../../build/jz-hdl}"

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

echo "Checking literal extension in generated Verilog..."

# Binary zero-extension: 8'b1010 -> 8'b00001010
check "binary zero-extension (8'b1010 -> 8'b00001010)" "8'b00001010"

# Hex zero-extension: 8'hF -> 8'b00001111
check "hex zero-extension (8'hF -> 8'b00001111)" "8'b00001111"

# Decimal zero-extension: 8'd13 -> 8'b00001101
check "decimal zero-extension (8'd13 -> 8'b00001101)" "8'b00001101"

# X-extension: 8'bx -> 8'b00000000 (x collapses to 0 in synthesis)
check "x-extension (8'bx -> 8'b00000000)" "8'b00000000"

# Z-extension: 8'bz -> 8'bzzzzzzzz
check "z-extension (8'bz -> 8'bzzzzzzzz)" "8'bzzzzzzzz"

# Z-extension false branch: 8'b1 -> 8'b00000001
check "z-extension false branch (8'b1 -> 8'b00000001)" "8'b00000001"

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
