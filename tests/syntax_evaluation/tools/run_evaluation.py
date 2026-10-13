#!/usr/bin/env python3
"""Runs RFC-BPR-0081's harness over the materialized corpus.

For each tree-sitter-cpp pin the harness executable holds the shipped
refinement and the candidate side by side, so every run measures the baseline
in the same process. One process runs per pin and corpus class, so each
process's peak resident memory belongs to one class. Outputs go to --out:

  <pin>/<class>.metrics.jsonl   per file, engine and round; stop trials
  <pin>/<class>.spans.jsonl     grammar, baseline and candidate spans (round 0)
  <pin>/<class>.log             the harness's own output
  run.json                      the commands, the host and the elapsed times

Usage:
  python3 run_evaluation.py --build BUILD --corpus DIR --out DIR
      [--pins v0_23_4,master] [--classes ...] [--rounds 3] [--stop-trials 20]
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_CLASSES = ["contract", "first-party", "third-party", "gap", "fragments", "malformed", "composer"]
STOP_CLASSES = "L,pathological"


def host_facts() -> dict:
    facts = {"system": platform.system(), "release": platform.release(), "machine": platform.machine(),
             "python": platform.python_version()}
    if platform.system() == "Darwin":
        for key in ("machdep.cpu.brand_string", "hw.ncpu", "hw.memsize"):
            facts[key] = subprocess.run(["sysctl", "-n", key], capture_output=True, text=True).stdout.strip()
        facts["load_average"] = os.getloadavg()
    elif hasattr(os, "getloadavg"):
        facts["load_average"] = os.getloadavg()
    return facts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--pins", default="v0_23_4,master")
    parser.add_argument("--classes", default=",".join(DEFAULT_CLASSES))
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--stop-trials", type=int, default=20)
    parser.add_argument("--seed", type=int, default=1442)
    parser.add_argument("--maximum-spans", type=int, help="override the product's 262,144-span limit (a labelled diagnostic run)")
    arguments = parser.parse_args()

    suffix = ".exe" if os.name == "nt" else ""
    arguments.out.mkdir(parents=True, exist_ok=True)
    record = {"host": host_facts(), "build": str(arguments.build), "corpus": str(arguments.corpus), "runs": []}
    for pin in arguments.pins.split(","):
        harness = arguments.build / "tests" / "syntax_evaluation" / f"syntax_evaluation_{pin}{suffix}"
        if not harness.exists():
            print(f"missing harness {harness}", file=sys.stderr)
            return 2
        directory = arguments.out / pin
        directory.mkdir(parents=True, exist_ok=True)
        for corpus_class in arguments.classes.split(","):
            command = [str(harness), "--root", str(arguments.corpus), "--index", str(arguments.corpus / "index.json"),
                       "--classes", corpus_class, "--rounds", str(arguments.rounds),
                       "--metrics", str(directory / f"{corpus_class}.metrics.jsonl"),
                       "--spans", str(directory / f"{corpus_class}.spans.jsonl"),
                       "--stop-trials", str(arguments.stop_trials), "--stop-classes", STOP_CLASSES,
                       "--seed", str(arguments.seed)]
            if arguments.maximum_spans:
                command += ["--maximum-spans", str(arguments.maximum_spans)]
            started = time.time()
            environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
            with (directory / f"{corpus_class}.log").open("w", encoding="utf-8") as log:
                completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=environment)
            elapsed = time.time() - started
            record["runs"].append({"pin": pin, "class": corpus_class, "command": command,
                                   "exit": completed.returncode, "elapsed_s": round(elapsed, 1),
                                   "load_average_after": os.getloadavg() if hasattr(os, "getloadavg") else None})
            print(f"{pin} {corpus_class}: exit {completed.returncode} in {elapsed:.1f} s", flush=True)
    (arguments.out / "run.json").write_text(json.dumps(record, indent=1) + "\n", encoding="utf-8")
    return 0 if all(run["exit"] == 0 for run in record["runs"]) else 1


if __name__ == "__main__":
    sys.exit(main())
