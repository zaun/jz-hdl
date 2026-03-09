#!/bin/bash
# Golden test: BUS SOURCE array with shared parent wire.
# Verifies the tristate transform:
#   - Deduplicates alias-group PORT_OUT drivers on the same parent wire
#   - Builds OE-based mux for PORT_OUT conditional drivers (VALID)
#   - Merges pass-through PORT_INOUT drivers (DATA)
set -euo pipefail

JZ_HDL_BIN="$1"
DIR="$(cd "$(dirname "$0")" && pwd)"
FAIL=0

# 1. Check Verilog output with --tristate-default=GND
tmp_actual=$(mktemp)
tmp_diag=$(mktemp)
(cd "${DIR}" && "${JZ_HDL_BIN}" --verilog --tristate-default=GND --explain -o "${tmp_actual}" test.jz) > "${tmp_diag}" 2>&1 || true

tmp_expected_filtered=$(mktemp)
tmp_actual_filtered=$(mktemp)
grep -v 'jz-hdl version:' "${DIR}/expected.v" > "${tmp_expected_filtered}" 2>/dev/null || true
grep -v 'jz-hdl version:' "${tmp_actual}" > "${tmp_actual_filtered}" 2>/dev/null || true

if ! diff -u "${tmp_expected_filtered}" "${tmp_actual_filtered}" > /dev/null 2>&1; then
    echo "FAIL test.jz (--verilog --tristate-default=GND)"
    diff -u "${tmp_expected_filtered}" "${tmp_actual_filtered}" || true
    ((FAIL++))
fi
rm -f "${tmp_expected_filtered}" "${tmp_actual_filtered}"

# 2. Verify: VALID shared net transformed with 2 drivers
if ! grep -q "bus_a.*priority mux with 2 driver" "${tmp_diag}"; then
    echo "FAIL test.jz (expected VALID mux with 2 drivers)"
    grep "bus_a\|driver\|mux" "${tmp_diag}" || true
    ((FAIL++))
fi

# 3. Verify: DATA shared net with pass-through merge
if ! grep -q "bus_a.*priority mux with 2 real driver.*pass-through" "${tmp_diag}"; then
    echo "FAIL test.jz (expected DATA pass-through merge)"
    grep "bus_a\|driver\|pass" "${tmp_diag}" || true
    ((FAIL++))
fi

# 4. Verify: no MUTUAL_EXCLUSION_FAIL errors
if grep -q "TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL" "${tmp_diag}"; then
    echo "FAIL test.jz (unexpected MUTUAL_EXCLUSION_FAIL error)"
    grep "MUTUAL_EXCLUSION" "${tmp_diag}" || true
    ((FAIL++))
fi

rm -f "${tmp_actual}" "${tmp_diag}"

if (( FAIL > 0 )); then
    exit 1
fi
exit 0
