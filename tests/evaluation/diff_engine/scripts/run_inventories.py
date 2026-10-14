#!/usr/bin/env python3
"""List every changed expectation of the stopping suites ([[RFC-BPR-0082]]).

core_diff_compute and application_diff_host stop at their first failed
expectation. The evaluation build generates copies that count failures
instead (tool_diff_compute_inventory, tool_diff_host_inventory). This runs
each against every candidate and appends one JSON line per candidate and
suite with every FAILED message, the failure count and the exit status, so a
candidate's failures can be compared with the baseline's one by one.

Usage:
  run_inventories.py --build BUILD/eval --out results/inventories.jsonl --candidates a,b
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys

TOOLS = {"core_diff_compute": "diff_compute_inventory", "application_diff_host": "diff_host_inventory"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--timeout", type=int, default=1800)
    arguments = parser.parse_args()
    directory = arguments.build.resolve() / "tests/evaluation/diff_engine"
    with arguments.out.open("a") as stream:
        for candidate in [name for name in arguments.candidates.split(",") if name]:
            for suite, tool in TOOLS.items():
                environment = dict(os.environ, BPR_DIFF_EVALUATION_CANDIDATE=candidate, QT_QPA_PLATFORM="offscreen")
                try:
                    completed = subprocess.run([str(directory / tool)], capture_output=True, text=True, env=environment, timeout=arguments.timeout)
                    status = completed.returncode
                    output = completed.stdout + completed.stderr
                except subprocess.TimeoutExpired as expired:
                    status = "timeout"
                    output = (expired.stdout or b"").decode(errors="replace") + (expired.stderr or b"").decode(errors="replace")
                failed = [line[len("FAILED: "):] for line in output.splitlines() if line.startswith("FAILED: ")]
                count = re.search(r"INVENTORY_FAILURES (\d+)", output)
                record = {"candidate": candidate, "suite": suite, "status": status, "completed": count is not None,
                          "failures": int(count.group(1)) if count else None, "failed": failed, "tail": "" if count else output[-1500:]}
                stream.write(json.dumps(record, sort_keys=True) + "\n")
                stream.flush()
                print(f"{candidate} {suite}: status {status}, {len(failed)} failed{'' if count else ' (did not finish)'}")
                sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
