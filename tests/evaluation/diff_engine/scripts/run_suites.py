#!/usr/bin/env python3
"""Run the RFC's suite list against each candidate ([[RFC-BPR-0082]] AC01).

The evaluation build tree links the hook into every executable; the build
tree's selection file names the candidate the ordinary entry points use. For
each candidate this writes the selection, runs the suites through ctest and the
diff fuzz target's corpus through the plain replay driver, and appends one JSON
line per test to --out. The selection file is restored to "baseline" at the
end. Candidates run one after another: the selection is per build tree.

Usage:
  run_suites.py --build BUILD/eval --repo ROOT --out results/suites.jsonl \
      [--candidates a,b] [--jobs N] [--regex REGEX]
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ElementTree

SUITES = (
    r"^(core_diff_compute|core_diff_semantics|core_diff_transition|"
    r"core_review_comment_range|core_review_cycle_comparison|core_review_cycle_suggestion|"
    r"core_diff_compute_benchmark|application_diff_host|application_large_review_.*)$"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=pathlib.Path, required=True)
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--jobs", type=int, default=6)
    parser.add_argument("--regex", default=SUITES)
    parser.add_argument("--timeout", type=int, default=3600)
    arguments = parser.parse_args()
    arguments.out = arguments.out.resolve()
    arguments.build = arguments.build.resolve()
    selection = arguments.build / "diff-evaluation-candidate.txt"
    replay = arguments.build / "tests/evaluation/diff_engine/diff_fuzz_replay"
    corpus = arguments.repo / "tests/fuzz/corpora/diff"
    arguments.out.parent.mkdir(parents=True, exist_ok=True)
    try:
        for candidate in [name for name in arguments.candidates.split(",") if name]:
            selection.write_text(candidate + "\n")
            junit = arguments.out.parent / f"junit-{candidate}.xml"
            started = time.monotonic()
            completed = subprocess.run(
                ["ctest", "--test-dir", str(arguments.build), "-R", arguments.regex, "-j", str(arguments.jobs),
                 "--timeout", str(arguments.timeout), "--output-on-failure", "--output-junit", str(junit)],
                capture_output=True, text=True)
            elapsed = time.monotonic() - started
            records = []
            if junit.is_file():
                for case in ElementTree.parse(junit).getroot().iter("testcase"):
                    failure = case.find("failure")
                    status = case.get("status", "")
                    output = (case.findtext("system-out") or "")
                    records.append({
                        "candidate": candidate,
                        "test": case.get("name"),
                        "status": "passed" if status == "run" and failure is None else (status or "failed"),
                        "seconds": float(case.get("time", "0")),
                        "failure_tail": "" if failure is None and status == "run" else output[-1500:],
                    })
            fuzz = subprocess.run([str(replay), str(corpus)], capture_output=True, text=True, timeout=arguments.timeout)
            match = re.search(r"FUZZ_REPLAY_INPUTS (\d+)", fuzz.stdout)
            records.append({
                "candidate": candidate,
                "test": "fuzz_diff_corpus_replay",
                "status": "passed" if fuzz.returncode == 0 and match else "failed",
                "seconds": 0.0,
                "inputs": int(match.group(1)) if match else 0,
                "failure_tail": (fuzz.stdout + fuzz.stderr)[-1500:] if fuzz.returncode != 0 else "",
            })
            with arguments.out.open("a") as stream:
                for record in records:
                    stream.write(json.dumps(record, sort_keys=True) + "\n")
            failed = [record["test"] for record in records if record["status"] != "passed"]
            print(f"{candidate}: {len(records) - len(failed)}/{len(records)} passed in {elapsed:.0f}s; ctest exit {completed.returncode}; failed {failed}")
            sys.stdout.flush()
    finally:
        selection.write_text("baseline\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
