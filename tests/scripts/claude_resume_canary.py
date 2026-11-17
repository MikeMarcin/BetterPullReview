#!/usr/bin/env python3
"""[[BPR-2138]], RFC-BPR-0101 AC12. Opt-in canary: a local draft's discussion
as BetterPullReview launches it (tool-free, no settings, no MCP, its session
kept), run twice against the installed Claude Code in one empty folder: the
first turn is told a word, the second resumes the first by the session it
reported and is asked for the word. Passes when the second turn knows it.
Makes two tiny real model calls on the person's own sign-in. Not part of ctest.

    python3 tests/scripts/claude_resume_canary.py \
        --launch-printer build/dev/tests/application/better_pull_review_agent_launch_policy_tests
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

if sys.version_info < (3, 10):
    sys.exit(f"Python 3.10 or newer is required; found {sys.version.split()[0]} at {sys.executable}")


def launch(printer: str, message: str, handle: str | None) -> tuple[list[str], bytes]:
    command = [printer, "--print-discussion-launch", message] + ([handle] if handle else [])
    printed = subprocess.run(command, capture_output=True, check=True).stdout
    count, _, rest = printed.partition(b"\n")
    arguments = []
    for _ in range(int(count)):
        size, _, rest = rest.partition(b"\n")
        arguments.append(rest[: int(size)].decode())
        rest = rest[int(size):]
    return arguments, rest


def result_of(stdout: bytes) -> dict | None:
    found = None
    for line in stdout.decode(errors="replace").splitlines():
        line = line.strip()
        if line.startswith("{"):
            frame = json.loads(line)
            if frame.get("type") == "result":
                found = frame
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--launch-printer", required=True, help="the policy test binary, which prints the launch")
    parser.add_argument("--claude", default=shutil.which("claude"), help="the Claude Code program (default: claude on PATH)")
    parser.add_argument("--model", default="haiku", help="a small model keeps the canary cheap")
    options = parser.parse_args()
    if not options.claude:
        print("CANARY_SKIPPED: no claude program", file=sys.stderr)
        return 2
    with contained_temporary_directory(prefix="bpr-resume-canary-") as folder:
        first_arguments, first_input = launch(options.launch_printer, "Remember the word KUMQUAT. Reply with: ok", None)
        if "--no-session-persistence" in first_arguments or "--resume" in first_arguments or "--disable-slash-commands" not in first_arguments:
            print("CANARY_FAILED: the first turn is not a tool-free launch that keeps its session", file=sys.stderr)
            return 1
        first = subprocess.run([options.claude, *first_arguments, "--model", options.model], input=first_input, cwd=folder, capture_output=True, timeout=180)
        opened = result_of(first.stdout)
        if opened is None or opened.get("is_error") or not opened.get("session_id"):
            print(f"CANARY_FAILED: the first turn did not answer (exit {first.returncode}): {first.stdout[-400:]!r} {first.stderr[-400:]!r}", file=sys.stderr)
            return 1
        session = opened["session_id"]
        second_arguments, second_input = launch(options.launch_printer, "Which word did I ask you to remember? Answer with the word alone.", session)
        if second_arguments[second_arguments.index("--resume") + 1] != session:
            print("CANARY_FAILED: the second turn does not resume the first", file=sys.stderr)
            return 1
        second = subprocess.run([options.claude, *second_arguments, "--model", options.model], input=second_input, cwd=folder, capture_output=True, timeout=180)
        resumed = result_of(second.stdout)
    if resumed is None or resumed.get("is_error"):
        print(f"CANARY_FAILED: the resumed turn did not answer (exit {second.returncode}): {second.stdout[-400:]!r} {second.stderr[-400:]!r}", file=sys.stderr)
        return 1
    answer = (resumed.get("result") or "").strip()
    if "KUMQUAT" not in answer.upper():
        print(f"CANARY_FAILED: the resumed turn did not know the word: {answer!r}", file=sys.stderr)
        return 1
    print(f"CANARY_PASS: {answer!r} same_session={resumed.get('session_id') == session} "
          f"cost={(opened.get('total_cost_usd') or 0) + (resumed.get('total_cost_usd') or 0)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
