#!/bin/bash
# test.sh - Verify that register reset type, polarity, and clock edge produce
#           correct Verilog sensitivity lists and reset guards.
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

check_absent() {
    local description="$1"
    local pattern="$2"

    if echo "$VERILOG" | grep -qF "$pattern"; then
        echo "  FAIL: $description"
        echo "        should NOT find: $pattern"
        FAIL=$((FAIL + 1))
    else
        echo "  PASS: $description"
        PASS=$((PASS + 1))
    fi
}

echo "Checking reset and sync hold logic in generated Verilog..."

# --- Test 1: Immediate reset (async) on posedge clk1 ---
# RESET_TYPE=Immediate with synchronizer: raw rst is NOT in the sensitivity list
# because the body uses the synchronized signal (rst_sync_0), not the raw rst.
check "immediate reset: sensitivity list is posedge clk1 only" \
      "always @(posedge clk1)"

check_absent "immediate reset: no raw rst in clk1 sensitivity list" \
             "always @(posedge clk1 or"

# Reset condition uses synchronized reset signal
check "immediate reset: synchronized reset guard" \
      "if (rst_sync_0) begin"

# The reset assignment zeros the register
check "immediate reset: reg_immediate reset to zero" \
      "reg_immediate <= 8'b00000000"

# --- Test 2: Clocked reset, active high, rising edge on clk2 ---
# RESET_TYPE=Clocked must NOT put rst in the sensitivity list
check "clocked reset: sensitivity list is posedge clk2 only" \
      "always @(posedge clk2)"

check_absent "clocked reset: no rst in clk2 sensitivity list" \
             "always @(posedge clk2 or"

check "clocked reset: reg_clocked reset to zero" \
      "reg_clocked <= 8'b00000000"

# --- Test 3: Clocked reset, active low, rising edge on clk3 ---
check "active-low reset: sensitivity list is posedge clk3 only" \
      "always @(posedge clk3)"

# Active-low reset checks !rst
check "active-low reset: inverted reset guard" \
      "if (!rst) begin"

check "active-low reset: reg_low reset to zero" \
      "reg_low <= 8'b00000000"

# --- Test 4: Clocked reset, active low, falling edge on clk4 ---
check "falling edge: sensitivity list uses negedge clk4" \
      "always @(negedge clk4)"

check "falling edge: reg_falling reset to zero" \
      "reg_falling <= 8'b00000000"

# --- Sync hold: en-gated update with implicit hold ---
check "sync hold: reg_immediate gated by en" \
      "reg_immediate <= d_in"

check "sync hold: reg_clocked gated by en" \
      "reg_clocked <= d_in"

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
