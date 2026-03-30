#!/usr/bin/env python3
"""Run prompt 1.md to update all test plans using Claude Code CLI."""

import argparse
import os
import subprocess
import sys

PIPELINE_DIR = os.path.join(os.path.dirname(__file__), "..")
PROMPT_FILE = os.path.join(PIPELINE_DIR, "prompts", "tests", "1.md")


def load_prompt() -> str:
    """Load prompt 1.md."""
    with open(PROMPT_FILE, "r") as f:
        return f.read()


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
        print("[dry-run] would run: claude -p <prompt> --allowedTools ...")
        return 0

    result = subprocess.run(cmd, capture_output=False)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(
        description="Update all test plans by running prompt 1.md."
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be run without executing",
    )
    args = parser.parse_args()

    if not os.path.exists(PROMPT_FILE):
        print(f"Error: prompt file not found: {PROMPT_FILE}", file=sys.stderr)
        return 1

    print("Updating test plans with prompt 1.md...")
    print(f"  Prompt: {PROMPT_FILE}")
    print()

    prompt = load_prompt()
    rc = run_claude(prompt, dry_run=args.dry_run)

    if rc == 0:
        print("\nDone. Test plans updated successfully.")
    else:
        print(f"\nFailed (exit code {rc}).", file=sys.stderr)

    return rc


if __name__ == "__main__":
    sys.exit(main())
