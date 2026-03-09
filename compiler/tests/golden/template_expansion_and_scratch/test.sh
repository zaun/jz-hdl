#!/bin/bash
# Golden test with scratch wire suffix normalization.
# Scratch wires have random 6-char suffixes (e.g., EXPAND__tmp__0_0_aB3xYz).
# This script strips those suffixes before comparing against expected output.
#
# Expected files are named expected.{ast,ir,v} (not test.*) to avoid the
# golden runner's automatic diff which can't handle random suffixes.
set -euo pipefail

JZ_HDL_BIN="$1"
DIR="$(cd "$(dirname "$0")" && pwd)"
FAIL=0

# Normalize: strip the trailing _XXXXXX from scratch wire names
# Pattern: <template>__<scratch>__<callsite>_<idx>_<6alphanum>
normalize() {
    sed -E 's/([A-Za-z]+__[a-zA-Z_]+__[0-9]+_[0-9]+)_[a-zA-Z0-9]{6}/\1/g'
}

for mode in ast ir verilog; do
    case "${mode}" in
        verilog) ext="v" ;;
        *)       ext="${mode}" ;;
    esac

    expected="${DIR}/expected.${ext}"
    if [[ ! -f "${expected}" ]]; then
        continue
    fi

    tmp_actual=$(mktemp)
    tmp_norm=$(mktemp)

    (cd "${DIR}" && "${JZ_HDL_BIN}" --${mode} -o "${tmp_actual}" test.jz) 2>/dev/null || true

    normalize < "${tmp_actual}" > "${tmp_norm}"

    tmp_expected_filtered=$(mktemp)
    tmp_norm_filtered=$(mktemp)
    grep -v 'jz-hdl version:' "${expected}" > "${tmp_expected_filtered}" 2>/dev/null || true
    grep -v 'jz-hdl version:' "${tmp_norm}" > "${tmp_norm_filtered}" 2>/dev/null || true

    if ! diff -u "${tmp_expected_filtered}" "${tmp_norm_filtered}" > /dev/null; then
        echo "FAIL test.jz (--${mode})"
        diff -u "${tmp_expected_filtered}" "${tmp_norm_filtered}" || true
        ((FAIL++))
    fi

    rm -f "${tmp_actual}" "${tmp_norm}" "${tmp_expected_filtered}" "${tmp_norm_filtered}"
done

if (( FAIL > 0 )); then
    exit 1
fi
exit 0
