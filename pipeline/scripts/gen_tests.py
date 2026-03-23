#!/usr/bin/env python3
"""Run prompt 3.md against each test_*.md file using Claude Code CLI."""

import argparse
import glob
import os
import re
import subprocess
import sys

PIPELINE_DIR = os.path.join(os.path.dirname(__file__), "..")
PROMPT_FILE = os.path.join(PIPELINE_DIR, "prompts", "tests", "2.md")


def load_prompt(test_filename: str) -> str:
    """Load prompt 3.md and replace the first-line target with the given test file."""
    with open(PROMPT_FILE, "r") as f:
        prompt = f.read()

    # Replace the first line (e.g. "For test_12_4-path_security.md only:")
    # with the current test file target
    prompt = re.sub(
        r"^For test_[^\n]+\n",
        f"For {test_filename} only:\n",
        prompt,
        count=1,
    )
    return prompt


def get_test_files(filter_pattern: str | None = None, start_at: str | None = None) -> list[str]:
    """Find all test_*.md files in the pipeline directory."""
    pattern = os.path.join(PIPELINE_DIR, "test_*.md")
    files = sorted(glob.glob(pattern))
    if filter_pattern:
        files = [f for f in files if filter_pattern in os.path.basename(f)]
    if start_at:
        idx = next(
            (i for i, f in enumerate(files) if start_at in os.path.basename(f)),
            None,
        )
        if idx is None:
            print(f"Warning: --start-at '{start_at}' not found, running all files.", file=sys.stderr)
        else:
            files = files[idx:]
    return files


def run_claude(prompt: str, dry_run: bool = False) -> int:
    """Invoke claude -p with the given prompt."""
    cmd = [
        "claude",
        "-p",
        prompt,
        "--allowedTools",
        "Read,Edit,Write,Glob,Grep,Bash",
    ]

    if dry_run:
        print(f"  [dry-run] would run: claude -p <prompt> --allowedTools ...")
        return 0

    result = subprocess.run(cmd, capture_output=False)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(
        description="Generate validation tests by running prompt 3.md for each test plan."
    )
    parser.add_argument(
        "--filter",
        type=str,
        default=None,
        help="Only process test files matching this substring (e.g. 'test_4_1')",
    )
    parser.add_argument(
        "--start-at",
        type=str,
        default=None,
        help="Start from the first test matching this substring (e.g. 'test_4_10')",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be run without executing",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List matching test files and exit",
    )
    args = parser.parse_args()

    test_files = get_test_files(args.filter, args.start_at)

    if not test_files:
        print("No test_*.md files found.", file=sys.stderr)
        return 1

    if args.list:
        for f in test_files:
            print(os.path.basename(f))
        print(f"\n{len(test_files)} file(s)")
        return 0

    print(f"Found {len(test_files)} test plan(s) to process.\n")

    results = {"pass": [], "fail": []}

    for i, test_path in enumerate(test_files, 1):
        test_filename = os.path.basename(test_path)
        print(f"[{i}/{len(test_files)}] {test_filename}")

        prompt = load_prompt(test_filename)
        rc = run_claude(prompt, dry_run=args.dry_run)

        if rc == 0:
            results["pass"].append(test_filename)
            print(f"  -> OK\n")
        else:
            results["fail"].append(test_filename)
            print(f"  -> FAILED (exit code {rc})\n")

    # Summary
    print("=" * 60)
    print(f"Done. {len(results['pass'])} passed, {len(results['fail'])} failed.")
    if results["fail"]:
        print("\nFailed:")
        for f in results["fail"]:
            print(f"  - {f}")

    return 1 if results["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
