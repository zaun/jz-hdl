#!/bin/bash
# Golden test: Two INOUT ports from the same instance on the same parent wire.
# Verifies that the tristate transform deduplicates same-instance drivers.
set -euo pipefail

JZ_HDL_BIN="$1"
DIR="$(cd "$(dirname "$0")" && pwd)"
FAIL=0

# 1. Check Verilog output with --tristate-default=GND
tmp_actual=$(mktemp)
tmp_diag=$(mktemp)
(cd "${DIR}" && "${JZ_HDL_BIN}" --verilog --tristate-default=GND --explain -o "${tmp_actual}" test.jz) > "${tmp_diag}" 2>&1 || true

if ! diff -u "${DIR}/expected.v" "${tmp_actual}" > /dev/null 2>&1; then
    echo "FAIL test.jz (--verilog --tristate-default=GND)"
    diff -u "${DIR}/expected.v" "${tmp_actual}" || true
    ((FAIL++))
fi

# 2. Verify: the info message should show 3 drivers for 'shared'
if ! grep -q "shared.*priority mux with 3 driver" "${tmp_diag}"; then
    echo "FAIL test.jz (expected 3 drivers for 'shared')"
    grep "shared\|driver" "${tmp_diag}" || true
    ((FAIL++))
fi

rm -f "${tmp_actual}" "${tmp_diag}"

if (( FAIL > 0 )); then
    exit 1
fi
exit 0
