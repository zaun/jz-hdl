#!/usr/bin/env bash
#
# update_golden.sh - Regenerate all golden test output files.
#
# Usage:
#   bash jz-hdl/tests/update_golden.sh          # update all golden files
#   bash jz-hdl/tests/update_golden.sh --dry-run # show what would change
#
# For each golden test directory, this script regenerates ALL output formats
# (.ast, .ir, .v, .il) by running the compiler in each mode.  Files are
# created if they don't exist yet and updated if the output has changed.
#

set -u

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" ]]; then
  DRY_RUN=1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR%/tests}"
GOLDEN_DIR="${ROOT_DIR}/tests/golden"
JZ_HDL_BIN="${ROOT_DIR}/build/jz-hdl"

if [[ ! -x "${JZ_HDL_BIN}" ]]; then
  echo "error: jz-hdl binary not found or not executable at: ${JZ_HDL_BIN}" >&2
  echo "       Build it first, e.g.: cmake -S jz-hdl -B jz-hdl/build && cmake --build jz-hdl/build" >&2
  exit 1
fi

if [[ ! -d "${GOLDEN_DIR}" ]]; then
  echo "error: golden test directory not found: ${GOLDEN_DIR}" >&2
  exit 1
fi

shopt -s nullglob

created=0
updated=0
unchanged=0
failed=0

tmp_out="$(mktemp)"
trap 'rm -f "${tmp_out}"' EXIT

for dir in "${GOLDEN_DIR}"/*/; do
  jz_files=("${dir}"*.jz)
  if (( ${#jz_files[@]} == 0 )); then
    continue
  fi

  # Skip directories with a test.sh — those tests manage their own golden
  # files (e.g., with scratch wire normalization).
  if [[ -x "${dir}test.sh" ]]; then
    continue
  fi

  for file in "${jz_files[@]}"; do
    rel_path="${file#${ROOT_DIR}/}"
    base_no_ext="${file%.jz}"

    for mode in ast ir verilog rtlil; do
      case "${mode}" in
        verilog) ext="v" ;;
        rtlil)   ext="il" ;;
        *)       ext="${mode}" ;;
      esac

      golden_file="${base_no_ext}.${ext}"

      # Run from the test's directory so the compiler sees a relative
      # filename, matching what the golden files contain.
      if ! (cd "$(dirname "${file}")" && "${JZ_HDL_BIN}" --${mode} -o "${tmp_out}" "$(basename "${file}")") 2>/dev/null; then
        echo "FAIL ${rel_path} (--${mode}) — compiler error"
        ((failed++))
        continue
      fi

      if [[ ! -f "${golden_file}" ]]; then
        # New file
        if (( DRY_RUN )); then
          echo "WOULD CREATE ${golden_file#${ROOT_DIR}/}"
        else
          cp "${tmp_out}" "${golden_file}"
          echo "CREATED ${golden_file#${ROOT_DIR}/}"
        fi
        ((created++))
      elif diff -q "${golden_file}" "${tmp_out}" > /dev/null 2>&1; then
        ((unchanged++))
      else
        if (( DRY_RUN )); then
          echo "WOULD UPDATE ${golden_file#${ROOT_DIR}/}"
          diff -u "${golden_file}" "${tmp_out}" | head -20
          echo "..."
        else
          cp "${tmp_out}" "${golden_file}"
          echo "UPDATED ${golden_file#${ROOT_DIR}/}"
        fi
        ((updated++))
      fi
    done
  done
done

echo
if (( DRY_RUN )); then
  echo "Dry run: ${created} would be created, ${updated} would be updated, ${unchanged} unchanged, ${failed} failed"
else
  echo "Done: ${created} created, ${updated} updated, ${unchanged} unchanged, ${failed} failed"
fi
