#!/usr/bin/env bash

set -u

# Resolve repository root based on location of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR%/tests}"
VALIDATION_DIR="${ROOT_DIR}/tests/validation"
JZ_HDL_BIN="${JZ_HDL_BIN:-${ROOT_DIR}/build/jz-hdl}"

if [[ ! -x "${JZ_HDL_BIN}" ]]; then
  echo "error: jz-hdl binary not found or not executable at: ${JZ_HDL_BIN}" >&2
  echo "       Build it first, e.g.: cmake -S . -B build && cmake --build build" >&2
  exit 1
fi

shopt -s nullglob
validation_files=("${VALIDATION_DIR}"/*.jz)

if (( ${#validation_files[@]} == 0 )); then
  echo "No validation .jz files found in ${VALIDATION_DIR}"
  exit 0
fi

pass=0
fail=0
skip=0

tmp_out="$(mktemp)"
trap 'rm -f "${tmp_out}"' EXIT

echo "Running validation lint tests..."

for file in "${validation_files[@]}"; do
  rel_path="${file#${ROOT_DIR}/}"
  base_no_ext="${file%.jz}"
  expected_out="${base_no_ext}.out"

  if [[ ! -f "${expected_out}" ]]; then
    echo "SKIP ${rel_path} (no $(basename "${expected_out}"))"
    ((skip++))
    continue
  fi

  # Determine extra flags based on filename.
  extra_flags=()
  case "$(basename "${file}")" in
    *_GND_*) extra_flags+=(--tristate-default=GND) ;;
    *_VCC_*) extra_flags+=(--tristate-default=VCC) ;;
  esac

  # Run linter; capture both stdout and stderr. Many tests are expected to
  # produce diagnostics and/or non-zero exit codes, so we do not treat a
  # non-zero status as a test failure by itself.
  "${JZ_HDL_BIN}" --info --lint ${extra_flags[@]+"${extra_flags[@]}"} "${file}" >"${tmp_out}" 2>&1 || true

  if diff -u "${expected_out}" "${tmp_out}" > /dev/null; then
    echo "PASS ${rel_path}"
    ((pass++))
  else
    echo "FAIL ${rel_path}"
    diff -u "${expected_out}" "${tmp_out}" || true
    ((fail++))
    exit 1
  fi

done

echo
echo "Validation: PASS=${pass} FAIL=${fail} SKIP=${skip}"

# ---------------------------------------------------------------------------
# Golden tests
# ---------------------------------------------------------------------------
GOLDEN_DIR="${ROOT_DIR}/tests/golden"
golden_pass=0
golden_fail=0
golden_skip=0

if [[ -d "${GOLDEN_DIR}" ]]; then
  echo
  echo "Running golden output tests..."

  for dir in "${GOLDEN_DIR}"/*/; do
    golden_files=("${dir}"*.jz)
    if (( ${#golden_files[@]} == 0 )); then
      continue
    fi

    for file in "${golden_files[@]}"; do
      rel_path="${file#${ROOT_DIR}/}"
      base_no_ext="${file%.jz}"

      for mode in ast ir verilog rtlil; do
        # Map mode to file extension
        case "${mode}" in
          verilog) ext="v" ;;
          rtlil)   ext="il" ;;
          *)       ext="${mode}" ;;
        esac

        expected="${base_no_ext}.${ext}"
        if [[ ! -f "${expected}" ]]; then
          continue
        fi

        # Run from the test's directory so the compiler sees a relative
        # filename, matching what the golden files contain.  Use -o so
        # that only the actual output (IR/Verilog/AST) ends up in the
        # file; diagnostics stay on stderr and are not compared.
        (cd "$(dirname "${file}")" && "${JZ_HDL_BIN}" --${mode} -o "${tmp_out}" "$(basename "${file}")") 2>/dev/null || true

        # Filter out the version line before comparing so that golden
        # files are not invalidated by version string changes.
        grep -v 'jz-hdl version:' "${expected}" > "${tmp_out}.expected_filtered" 2>/dev/null || true
        grep -v 'jz-hdl version:' "${tmp_out}" > "${tmp_out}.actual_filtered" 2>/dev/null || true

        if diff -u "${tmp_out}.expected_filtered" "${tmp_out}.actual_filtered" > /dev/null; then
          echo "PASS ${rel_path} (--${mode})"
          ((golden_pass++))
        else
          echo "FAIL ${rel_path} (--${mode})"
          diff -u "${tmp_out}.expected_filtered" "${tmp_out}.actual_filtered" || true
          ((golden_fail++))
        fi
        rm -f "${tmp_out}.expected_filtered" "${tmp_out}.actual_filtered"
      done
    done

    # Run test.sh if present in the golden directory
    if [[ -x "${dir}test.sh" ]]; then
      rel_path="${dir#${ROOT_DIR}/}test.sh"
      if "${dir}test.sh" "${JZ_HDL_BIN}" >"${tmp_out}" 2>&1; then
        echo "PASS ${rel_path}"
        ((golden_pass++))
      else
        echo "FAIL ${rel_path}"
        cat "${tmp_out}"
        ((golden_fail++))
      fi
    fi
  done

  echo
  echo "Golden: PASS=${golden_pass} FAIL=${golden_fail}"
fi

# ---------------------------------------------------------------------------
# Yosys parse verification
#
# For each golden test that has a .v or .il file, verify that yosys can
# parse the output without errors.  This catches syntax issues that the
# golden diff alone would not detect.
# ---------------------------------------------------------------------------
YOSYS_BIN="$(command -v yosys 2>/dev/null || true)"
yosys_pass=0
yosys_fail=0
yosys_skip=0

if [[ -d "${GOLDEN_DIR}" ]] && [[ -n "${YOSYS_BIN}" ]]; then
  echo
  echo "Running yosys parse verification..."

  for dir in "${GOLDEN_DIR}"/*/; do
    golden_files=("${dir}"*.jz)
    if (( ${#golden_files[@]} == 0 )); then
      continue
    fi

    for file in "${golden_files[@]}"; do
      rel_path="${file#${ROOT_DIR}/}"
      base_no_ext="${file%.jz}"

      # Verify Verilog output
      verilog_file="${base_no_ext}.v"
      if [[ -f "${verilog_file}" ]]; then
        # Generate fresh Verilog output
        (cd "$(dirname "${file}")" && "${JZ_HDL_BIN}" --verilog -o "${tmp_out}" "$(basename "${file}")") 2>/dev/null || true
        if (cd "$(dirname "${file}")" && "${YOSYS_BIN}" -p "read_verilog ${tmp_out}") >/dev/null 2>&1; then
          echo "PASS ${rel_path} (yosys read_verilog)"
          ((yosys_pass++))
        else
          echo "FAIL ${rel_path} (yosys read_verilog)"
          (cd "$(dirname "${file}")" && "${YOSYS_BIN}" -p "read_verilog ${tmp_out}") 2>&1 | grep -i error || true
          ((yosys_fail++))
        fi
      fi

      # Verify RTLIL output
      rtlil_file="${base_no_ext}.il"
      if [[ -f "${rtlil_file}" ]]; then
        # Generate fresh RTLIL output
        (cd "$(dirname "${file}")" && "${JZ_HDL_BIN}" --rtlil -o "${tmp_out}" "$(basename "${file}")") 2>/dev/null || true
        if (cd "$(dirname "${file}")" && "${YOSYS_BIN}" -p "read_rtlil ${tmp_out}") >/dev/null 2>&1; then
          echo "PASS ${rel_path} (yosys read_rtlil)"
          ((yosys_pass++))
        else
          echo "FAIL ${rel_path} (yosys read_rtlil)"
          (cd "$(dirname "${file}")" && "${YOSYS_BIN}" -p "read_rtlil ${tmp_out}") 2>&1 | grep -i error || true
          ((yosys_fail++))
        fi
      fi
    done
  done

  echo
  echo "Yosys: PASS=${yosys_pass} FAIL=${yosys_fail}"
elif [[ -z "${YOSYS_BIN}" ]]; then
  echo
  echo "Yosys: SKIP (yosys not found in PATH)"
fi

# ---------------------------------------------------------------------------
# Yosys equivalence checking (Verilog vs RTLIL)
#
# For each golden test that has both a .v and .il file, verify that the
# Verilog and RTLIL backends produce functionally equivalent netlists
# using yosys equiv_make / equiv_simple / equiv_status.
# ---------------------------------------------------------------------------
equiv_pass=0
equiv_fail=0

if [[ -d "${GOLDEN_DIR}" ]] && [[ -n "${YOSYS_BIN}" ]]; then
  echo
  echo "Running yosys equivalence checking..."

  tmp_v=$(mktemp "${TMPDIR:-/tmp}/jzhdl_equiv_v.XXXXXX")
  tmp_il=$(mktemp "${TMPDIR:-/tmp}/jzhdl_equiv_il.XXXXXX")
  trap "rm -f '${tmp_out}' '${tmp_v}' '${tmp_il}'" EXIT

  for dir in "${GOLDEN_DIR}"/*/; do
    golden_files=("${dir}"*.jz)
    if (( ${#golden_files[@]} == 0 )); then
      continue
    fi

    for file in "${golden_files[@]}"; do
      rel_path="${file#${ROOT_DIR}/}"
      base_no_ext="${file%.jz}"

      verilog_file="${base_no_ext}.v"
      rtlil_file="${base_no_ext}.il"

      # Only run equiv check when both backends have golden files
      if [[ ! -f "${verilog_file}" ]] || [[ ! -f "${rtlil_file}" ]]; then
        continue
      fi

      # Generate fresh outputs
      (cd "$(dirname "${file}")" && "${JZ_HDL_BIN}" --verilog -o "${tmp_v}" "$(basename "${file}")") 2>/dev/null || true
      (cd "$(dirname "${file}")" && "${JZ_HDL_BIN}" --rtlil -o "${tmp_il}" "$(basename "${file}")") 2>/dev/null || true

      # Run equivalence check via yosys
      # Each backend is loaded, elaborated, and stashed separately so that
      # identically-named modules don't collide.
      equiv_out=$("${YOSYS_BIN}" -p "
        read_verilog ${tmp_v}
        hierarchy -auto-top
        proc
        rename -top gold
        design -stash gold_design

        read_rtlil ${tmp_il}
        hierarchy -auto-top
        proc
        rename -top gate
        design -stash gate_design

        design -copy-from gold_design -as gold gold
        design -copy-from gate_design -as gate gate

        equiv_make gold gate equiv
        hierarchy -top equiv
        async2sync
        equiv_simple
        equiv_induct
        equiv_status -assert
      " 2>&1) || true

      if echo "${equiv_out}" | grep -q "Equivalence successfully proven"; then
        echo "PASS ${rel_path} (equiv verilog<->rtlil)"
        ((equiv_pass++))
      else
        echo "FAIL ${rel_path} (equiv verilog<->rtlil)"
        echo "${equiv_out}" | grep -iE '(error|equiv|assert|fail)' || true
        ((equiv_fail++))
      fi
    done
  done

  echo
  echo "Equiv: PASS=${equiv_pass} FAIL=${equiv_fail}"
elif [[ -z "${YOSYS_BIN}" ]]; then
  echo
  echo "Equiv: SKIP (yosys not found in PATH)"
fi

# ---------------------------------------------------------------------------
# Testbench simulation tests
# ---------------------------------------------------------------------------
TESTBENCH_DIR="${ROOT_DIR}/tests/testbenches"
tb_pass=0
tb_fail=0

if [[ -d "${TESTBENCH_DIR}" ]] && [[ -x "${JZ_HDL_BIN}" ]]; then
  tb_files=("${TESTBENCH_DIR}"/*.jz)

  if (( ${#tb_files[@]} > 0 )); then
    echo
    echo "Running testbench simulation tests..."

    for file in "${tb_files[@]}"; do
      rel_path="${file#${ROOT_DIR}/}"

      if "${JZ_HDL_BIN}" --test "${file}" >"${tmp_out}" 2>&1; then
        echo "PASS ${rel_path}"
        ((tb_pass++))
      else
        echo "FAIL ${rel_path}"
        cat "${tmp_out}"
        ((tb_fail++))
      fi
    done

    echo
    echo "Testbench: PASS=${tb_pass} FAIL=${tb_fail}"
  fi
fi

# ---------------------------------------------------------------------------
# Simulation tests (--simulate mode)
# ---------------------------------------------------------------------------
SIMULATION_DIR="${ROOT_DIR}/tests/simulation"
sim_pass=0
sim_fail=0

if [[ -d "${SIMULATION_DIR}" ]] && [[ -x "${JZ_HDL_BIN}" ]]; then
  sim_files=("${SIMULATION_DIR}"/*.jz)

  if (( ${#sim_files[@]} > 0 )); then
    echo
    echo "Running simulation tests..."

    for file in "${sim_files[@]}"; do
      rel_path="${file#${ROOT_DIR}/}"

      if "${JZ_HDL_BIN}" --simulate "${file}" -o /dev/null >"${tmp_out}" 2>&1; then
        echo "PASS ${rel_path}"
        ((sim_pass++))
      else
        echo "FAIL ${rel_path}"
        cat "${tmp_out}"
        ((sim_fail++))
      fi
    done

    echo
    echo "Simulation: PASS=${sim_pass} FAIL=${sim_fail}"
  fi
fi

# ---------------------------------------------------------------------------
# Cross-mode rejection tests
# ---------------------------------------------------------------------------
cross_pass=0
cross_fail=0

echo
echo "Running cross-mode rejection tests..."

# @simulation files must be rejected by --test
sim_reject_files=("${SIMULATION_DIR}"/*.jz)
if (( ${#sim_reject_files[@]} > 0 )); then
  for file in "${sim_reject_files[@]}"; do
    rel_path="${file#${ROOT_DIR}/}"
    "${JZ_HDL_BIN}" --info --test "${file}" >"${tmp_out}" 2>&1 || true
    if grep -q "SIM_WRONG_TOOL" "${tmp_out}"; then
      echo "PASS ${rel_path} (rejected by --test)"
      ((cross_pass++))
    else
      echo "FAIL ${rel_path} (should be rejected by --test with SIM_WRONG_TOOL)"
      cat "${tmp_out}"
      ((cross_fail++))
    fi
  done
fi

echo
echo "Cross-mode: PASS=${cross_pass} FAIL=${cross_fail}"

total_pass=$((pass + golden_pass + yosys_pass + equiv_pass + tb_pass + sim_pass + cross_pass))
total_fail=$((fail + golden_fail + yosys_fail + equiv_fail + tb_fail + sim_fail + cross_fail))
total_skip=$((skip + golden_skip + yosys_skip))

echo
echo "==========================================="
echo "  Test Summary"
echo "==========================================="
printf "  %-16s %4s %4s %4s\n" "" "PASS" "FAIL" "SKIP"
echo "  -------------------------------------------"
printf "  %-16s %4d %4d %4d\n" "Validation" "${pass}" "${fail}" "${skip}"
printf "  %-16s %4d %4d %4d\n" "Golden" "${golden_pass}" "${golden_fail}" "${golden_skip}"
printf "  %-16s %4d %4d %4d\n" "Yosys parse" "${yosys_pass}" "${yosys_fail}" "${yosys_skip}"
printf "  %-16s %4d %4d %4d\n" "Equivalence" "${equiv_pass}" "${equiv_fail}" "0"
printf "  %-16s %4d %4d %4d\n" "Testbench" "${tb_pass}" "${tb_fail}" "0"
printf "  %-16s %4d %4d %4d\n" "Simulation" "${sim_pass}" "${sim_fail}" "0"
printf "  %-16s %4d %4d %4d\n" "Cross-mode" "${cross_pass}" "${cross_fail}" "0"
echo "  -------------------------------------------"
printf "  %-16s %4d %4d %4d\n" "Total" "${total_pass}" "${total_fail}" "${total_skip}"
echo "==========================================="

if (( total_fail > 0 )); then
  exit 1
fi

exit 0
