#!/usr/bin/env bash
#
# run_examples.sh — build every example × board combination and produce
# a Markdown summary table with aligned columns.
#
# Usage:  ./scripts/run_examples.sh <examples-dir> <output-file>
#   e.g.  ./scripts/run_examples.sh examples status.md

set -euo pipefail

EXAMPLES_DIR="${1:?Usage: $0 <examples-dir> <output-file>}"
OUTPUT_FILE="${2:?Usage: $0 <examples-dir> <output-file>}"

# Collect example directories (sorted, skip cpu/soc)
examples=()
for d in "$EXAMPLES_DIR"/*/; do
    name=$(basename "$d")
    case "$name" in cpu|soc) continue ;; esac
    [ -f "$d/Makefile" ] || continue
    examples+=("$name")
done

# Discover boards: extract BOARD_GOAL values from each Makefile,
# keep only those that have a matching src/<board>.jz file,
# then build the union across all examples (preserving order of first appearance).
all_boards=""

for example in "${examples[@]}"; do
    makefile="$EXAMPLES_DIR/$example/Makefile"
    for board in $(grep 'ifeq ($(BOARD_GOAL)' "$makefile" | sed 's/.*,//;s/)//'); do
        src="$EXAMPLES_DIR/$example/src/${board}.jz"
        [ -f "$src" ] || continue
        case " $all_boards " in
            *" $board "*) ;;
            *) all_boards="$all_boards $board" ;;
        esac
    done
done
read -ra BOARDS <<< "$all_boards"

# Compute column widths: each column is at least as wide as its header,
# and at least 4 chars (enough for "PASS"/"FAIL"/"    ").
# First column width = max example name length, minimum "Example".
name_width=7  # length of "Example"
for example in "${examples[@]}"; do
    len=${#example}
    [ "$len" -gt "$name_width" ] && name_width=$len
done

board_widths=()
for board in "${BOARDS[@]}"; do
    w=${#board}
    [ "$w" -lt 4 ] && w=4
    board_widths+=("$w")
done

# Helper: pad a string to a given width, centered
pad_center() {
    local str="$1" width="$2"
    local len=${#str}
    local total_pad=$((width - len))
    local left_pad=$((total_pad / 2))
    local right_pad=$((total_pad - left_pad))
    printf "%*s%s%*s" "$left_pad" "" "$str" "$right_pad" ""
}

# Helper: pad a string to a given width, left-aligned
pad_left() {
    local str="$1" width="$2"
    printf "%-*s" "$width" "$str"
}

# Build results grid (example × board)
# Values: "PASS", "FAIL", or "" (no board support)
results=()
for example in "${examples[@]}"; do
    for i in "${!BOARDS[@]}"; do
        board="${BOARDS[$i]}"
        src="$EXAMPLES_DIR/$example/src/${board}.jz"
        if [ ! -f "$src" ]; then
            results+=("")
        else
            echo "  Building $example / $board ..."
            output=$(make -C "$EXAMPLES_DIR/$example" "$board" synthesis 2>&1) || true
            if echo "$output" | grep -q "Info: Program finished normally."; then
                results+=("PASS")
                echo "    PASS"
            else
                results+=("FAIL")
                echo "    FAIL"
            fi
            # Clean build artifacts between runs
            make -C "$EXAMPLES_DIR/$example" clean >/dev/null 2>&1 || true
        fi
    done
done

# Write table
{
    echo "# Example Synthesis Status"
    echo ""

    # Header row
    printf "| %s |" "$(pad_left "Example" "$name_width")"
    for i in "${!BOARDS[@]}"; do
        printf " %s |" "$(pad_center "${BOARDS[$i]}" "${board_widths[$i]}")"
    done
    echo ""

    # Separator row
    printf "|-%s-|" "$(printf '%*s' "$name_width" '' | tr ' ' '-')"
    for i in "${!BOARDS[@]}"; do
        printf ":%s:|" "$(printf '%*s' "${board_widths[$i]}" '' | tr ' ' '-')"
    done
    echo ""

    # Data rows
    idx=0
    for example in "${examples[@]}"; do
        printf "| %s |" "$(pad_left "$example" "$name_width")"
        for i in "${!BOARDS[@]}"; do
            val="${results[$idx]}"
            printf " %s |" "$(pad_center "$val" "${board_widths[$i]}")"
            idx=$((idx + 1))
        done
        echo ""
    done

    echo ""
    echo "_Generated on $(date -u '+%Y-%m-%d %H:%M UTC')_"
} > "$OUTPUT_FILE"

echo "Wrote $OUTPUT_FILE"
