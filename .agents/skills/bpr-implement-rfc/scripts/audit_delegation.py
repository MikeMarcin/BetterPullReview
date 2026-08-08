#!/usr/bin/env python3
"""Count delegated-agent launches per session from local Claude Code transcripts.

Delegation that never fires saves nothing: this reports how often sessions
actually launch delegated agents, and flags substantive sessions that ran
entirely in the primary context.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

LAUNCH_RE = re.compile(r'"subagent_type"\s*:\s*"([A-Za-z0-9_-]+)"')


def transcripts_for_repo(repo_root: Path) -> Path:
    flattened = str(repo_root.resolve()).replace("/", "-")
    return Path.home() / ".claude" / "projects" / flattened


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--transcripts-dir", type=Path)
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument(
        "--min-bytes",
        type=int,
        default=200_000,
        help="Sessions at least this large are substantive and expected to delegate.",
    )
    args = parser.parse_args()

    directory = args.transcripts_dir
    if directory is None:
        if args.repo_root is None:
            print("error: pass --transcripts-dir or --repo-root", file=sys.stderr)
            return 2
        directory = transcripts_for_repo(args.repo_root)
    if not directory.is_dir():
        print(f"error: no transcript directory at {directory}", file=sys.stderr)
        return 2

    totals: Counter[str] = Counter()
    flagged: list[tuple[str, int]] = []
    sessions = sorted(directory.glob("*.jsonl"))
    if not sessions:
        print(f"no session transcripts in {directory}")
        return 0
    for transcript in sessions:
        text = transcript.read_text(encoding="utf-8", errors="replace")
        launches = Counter(LAUNCH_RE.findall(text))
        totals.update(launches)
        launched = sum(launches.values())
        size = transcript.stat().st_size
        marker = "" if launched else "  <- zero delegation"
        if not launched and size >= args.min_bytes:
            flagged.append((transcript.stem[:8], size))
        print(f"session {transcript.stem[:8]}: {launched} launches, {size // 1024} KiB{marker}")

    print("\nper-role totals:")
    if totals:
        for name, count in totals.most_common():
            print(f"  {name}: {count}")
    else:
        print("  (none)")
    if flagged:
        print(f"\n{len(flagged)} substantive session(s) with zero delegation:")
        for stem, size in flagged:
            print(f"  {stem} ({size // 1024} KiB)")
        print("Zero delegation in a substantive session is a bug against the delegate skill.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
