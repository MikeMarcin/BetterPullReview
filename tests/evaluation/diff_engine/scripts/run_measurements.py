#!/usr/bin/env python3
"""Run every harness mode on one station ([[RFC-BPR-0082]] AC02).

Records the station's environment, then runs the harness modes one after
another into a results directory: measure (interleaved rounds), phases,
intraline, stops, overhead, determinism and rss. Run it on an otherwise idle
station from an optimized evaluation build.

Usage:
  run_measurements.py --build BUILD/eval --corpus DIR --results DIR [--rounds 3 --reps 3] [--modes a,b]
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import pathlib
import platform
import subprocess
import sys
import time

ALL_MODES = ("overhead", "measure", "phases", "intraline", "stops", "determinism", "rss")
INTRALINE = "baseline,c-dmp,c-onp-intraline"
PAIRING = "baseline,c-rapidfuzz,c-dmp,c-onp-intraline"


def text(command: list[str]) -> str:
    try:
        return subprocess.run(command, capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def environment(build: pathlib.Path) -> dict:
    cache = (build / "CMakeCache.txt").read_text().splitlines() if (build / "CMakeCache.txt").is_file() else []
    values = {line.split("=", 1)[0].split(":", 1)[0]: line.split("=", 1)[1] for line in cache if "=" in line and not line.startswith(("//", "#"))}
    stage = build / "_bpr/diff-evaluation"
    result = {
        "recorded": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "os": f"{platform.system()} {platform.release()} ({platform.mac_ver()[0] or platform.version()})",
        "machine": platform.machine(),
        "cpu": text(["sysctl", "-n", "machdep.cpu.brand_string"]) or platform.processor(),
        "cpu_count": os.cpu_count(),
        "memory_bytes": text(["sysctl", "-n", "hw.memsize"]),
        "model": text(["sysctl", "-n", "hw.model"]),
        "compiler": values.get("CMAKE_CXX_COMPILER", ""),
        "compiler_version": text([values.get("CMAKE_CXX_COMPILER", "c++"), "--version"]).splitlines()[0] if values.get("CMAKE_CXX_COMPILER") else "",
        "build_type": values.get("CMAKE_BUILD_TYPE", ""),
        "git_2_39": text([str(stage / "bin-2.39.5/git"), "--version"]),
        "git_2_56": text([str(stage / "bin-2.56.0/git"), "--version"]),
        "git_installed": text(["git", "--version"]),
        "revision": text(["git", "-C", str(build.parent.parent), "rev-parse", "HEAD"]),
    }
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=pathlib.Path, required=True)
    parser.add_argument("--corpus", type=pathlib.Path, required=True)
    parser.add_argument("--results", type=pathlib.Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--reps", type=int, default=3)
    parser.add_argument("--modes", default=",".join(ALL_MODES))
    parser.add_argument("--stop-pairs", default="")
    arguments = parser.parse_args()
    harness = arguments.build / "tests/evaluation/diff_engine/better_pull_review_diff_engine_evaluation"
    results = arguments.results
    results.mkdir(parents=True, exist_ok=True)
    (results / "environment.json").write_text(json.dumps(environment(arguments.build), indent=1, sort_keys=True) + "\n")
    common = ["--corpus", str(arguments.corpus)]
    plan = {
        "overhead": ["overhead", "--out", str(results / "overhead.jsonl"), "--reps", "60"],
        "measure": ["measure", "--out", str(results / "measure.jsonl"), "--rounds", str(arguments.rounds), "--reps", str(arguments.reps)] + common,
        "phases": ["phases", "--out", str(results / "phases.jsonl"), "--rounds", str(arguments.rounds), "--candidates", PAIRING] + common,
        "intraline": ["intraline", "--out", str(results / "intraline.jsonl"), "--reps", "7", "--candidates", INTRALINE] + common,
        "stops": ["stops", "--out", str(results / "stops.jsonl"), "--reps", "2", "--classes", "L,pathological"] + common
        + (["--pairs", arguments.stop_pairs] if arguments.stop_pairs else []),
        "determinism": ["determinism", "--out", str(results / "determinism.jsonl")] + common,
        "rss": ["rss", "--out", str(results / "rss.jsonl")] + common,
    }
    for mode in [name for name in arguments.modes.split(",") if name]:
        target = results / f"{mode}.jsonl"
        if target.exists():
            target.unlink()
        started = time.monotonic()
        completed = subprocess.run([str(harness)] + plan[mode], capture_output=True, text=True)
        (results / f"{mode}.log").write_text(completed.stdout[-20000:] + completed.stderr[-20000:])
        print(f"{mode}: exit {completed.returncode} in {time.monotonic() - started:.0f}s")
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
