#!/usr/bin/env python3
"""[[BPR-2140]], RFC-BPR-0107 AC09. Opt-in canary: the exact Enhance launch
BetterPullReview builds, with a rule given only in its inline
`--append-system-prompt`, run against the installed Claude Code. Passes when
the answer follows the rule. Makes one tiny real model call on the person's
own sign-in, in an empty folder, with no tools, settings, MCP or saved session
(the launch's own restrictions). Not part of ctest.

    python3 tests/scripts/claude_system_prompt_canary.py \
        --launch-printer build/dev/tests/application/better_pull_review_agent_launch_policy_tests
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile

if sys.version_info < (3, 10):
    sys.exit(f"Python 3.10 or newer is required; found {sys.version.split()[0]} at {sys.executable}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--launch-printer", required=True, help="the policy test binary, which prints the launch")
    parser.add_argument("--claude", default=shutil.which("claude"), help="the Claude Code program (default: claude on PATH)")
    parser.add_argument("--model", default="haiku", help="a small model keeps the canary cheap")
    options = parser.parse_args()
    if not options.claude:
        print("CANARY_SKIPPED: no claude program", file=sys.stderr)
        return 2
    printed = subprocess.run([options.launch_printer, "--print-canary-launch"], capture_output=True, check=True).stdout
    head, _, framed = printed.partition(b"\0\0")
    arguments = [part.decode() for part in head.split(b"\0")]
    if "--append-system-prompt" not in arguments or any(a == "--append-system-prompt-file" for a in arguments):
        print("CANARY_FAILED: the launch does not carry the inline system prompt", file=sys.stderr)
        return 1
    if b"PINEAPPLE" in framed:
        print("CANARY_FAILED: the rule is in the message, so the canary proves nothing", file=sys.stderr)
        return 1
    arguments += ["--model", options.model]
    with tempfile.TemporaryDirectory(prefix="bpr-canary-") as folder:
        run = subprocess.run([options.claude, *arguments], input=framed, cwd=folder, capture_output=True, timeout=180)
    result = None
    for line in run.stdout.decode(errors="replace").splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        frame = json.loads(line)
        if frame.get("type") == "result":
            result = frame
    if result is None or result.get("is_error"):
        print(f"CANARY_FAILED: no answer (exit {run.returncode}): {run.stdout[-400:]!r} {run.stderr[-400:]!r}", file=sys.stderr)
        return 1
    answer = (result.get("result") or "").strip()
    if not answer.rstrip(".").endswith("PINEAPPLE"):
        print(f"CANARY_FAILED: the rule was not applied: {answer!r}", file=sys.stderr)
        return 1
    print(f"CANARY_PASS: {answer!r} turns={result.get('num_turns')} cost={result.get('total_cost_usd')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
