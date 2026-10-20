#!/usr/bin/env python3
"""Run every test in a CTest preset and retain an honest timing report."""
from __future__ import annotations

import argparse
from collections import Counter
from contextlib import contextmanager
from datetime import datetime, timezone
import errno
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ROW = re.compile(r"\d+/\d+ Test\s+#\d+: (\S+)\s+\.{2,}(.*?)\s*([\d.]+) sec")


def read_log(path: Path) -> dict:
    """Read retained CTest console output, including failures without a space."""
    content = path.read_text(encoding="utf-8", errors="replace")
    rows = []
    for match in ROW.finditer(content):
        name, result, elapsed = match.groups()
        status = "skipped" if "Skipped" in result else "passed" if "Passed" in result else "failed"
        rows.append({"name": name, "status": status, "seconds": float(elapsed)})
    wall = re.search(r"Total Test time \(real\)\s*=\s*([\d.]+) sec", content)
    if not rows:
        raise ValueError(f"No completed CTest rows in {path}")
    return summarize(rows, float(wall.group(1)) if wall else None)


def read_junit(path: Path, wall: float) -> dict:
    rows = []
    for case in ET.parse(path).getroot().iter("testcase"):
        failed = case.find("failure") is not None or case.find("error") is not None
        skipped = case.find("skipped") is not None or case.get("status") in {"notrun", "disabled"}
        rows.append({"name": case.attrib["name"], "seconds": float(case.get("time", "0")),
                     "status": "failed" if failed else "skipped" if skipped else "passed"})
    return summarize(rows, wall)


def summarize(rows: list[dict], wall: float | None) -> dict:
    return {"schema": "bpr.test-timings.v1", "wall_seconds": wall,
            "summed_test_seconds": sum(row["seconds"] for row in rows),
            "counts": dict(Counter(row["status"] for row in rows)),
            "tests": sorted(rows, key=lambda row: (-row["seconds"], row["name"]))}


def print_report(report: dict) -> None:
    wall = report["wall_seconds"]
    print(f"\nCTest wall time: {wall / 60:.2f} min" if wall is not None else "\nCTest wall time: incomplete run")
    print(f"Summed test time: {report['summed_test_seconds'] / 60:.2f} min (overlapping tests count separately)")
    print("Results: " + ", ".join(f"{count} {status}" for status, count in sorted(report["counts"].items())))
    print("Slowest tests:")
    for row in report["tests"][:20]:
        print(f"  {row['seconds']:7.2f}s  {row['status']:7}  {row['name']}")
    for status in ("failed", "skipped"):
        names = [row["name"] for row in report["tests"] if row["status"] == status]
        if names:
            print(f"{status.capitalize()}: " + ", ".join(names))


def validate_schedule(manifest: dict) -> None:
    """Keep exclusive courses alone; admit only declared isolated correctness."""
    for row in manifest["tests"]:
        props = {prop["name"]: prop["value"] for prop in row.get("properties", [])}
        directory = props.get("WORKING_DIRECTORY", "").replace("\\", "/")
        platforms = [item for item in props.get("ENVIRONMENT", []) if item.upper().startswith("QT_QPA_PLATFORM=")]
        labels = props.get("LABELS", [])
        course = "/tests/end_to_end" in directory or "/tests/adapters_qt" in directory
        declared = "bpr_isolated_correctness" in labels
        mode_labels = [label for label in labels if label.startswith("bpr_correctness_")]
        if mode_labels and not declared:
            raise ValueError(f"Invalid isolated correctness scheduling declaration: {row['name']}")
        if declared:
            locks = props.get("RESOURCE_LOCK", [])
            valid_platform = (
                mode_labels == ["bpr_correctness_headless"] and not platforms
                or mode_labels == ["bpr_correctness_offscreen"] and platforms == ["QT_QPA_PLATFORM=offscreen"]
            )
            valid_lock = any(re.fullmatch(r"bpr_isolated_correctness_[a-z0-9_]+", lock) for lock in locks)
            platform_override = any(item.upper().startswith("QT_QPA_PLATFORM=") for item in props.get("ENVIRONMENT_MODIFICATION", []))
            platform_argument = any(
                item in {"-platform", "--platform"} or item.startswith(("-platform=", "--platform="))
                for item in row.get("command", [])
            )
            exclusive = props.get("RUN_SERIAL") or "bpr_explicit_serial" in labels or "bpr_serial_gui" in labels
            if not valid_platform or not valid_lock or platform_override or platform_argument or exclusive:
                raise ValueError(f"Invalid isolated correctness scheduling declaration: {row['name']}")
        if (platforms or course or "bpr_serial_gui" in labels) and not props.get("RUN_SERIAL") and not declared:
            raise ValueError(f"Interactive/performance test is not serial: {row['name']}")


@contextmanager
def suite_lock(path: Path):
    """Keep two full-gate invocations from sharing persistent fixture roots."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as handle:
        try:
            if os.name == "nt":
                import msvcrt
                if handle.tell() == 0:
                    handle.write(b"\0")
                    handle.flush()
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            if error.errno not in {errno.EACCES, errno.EAGAIN, errno.EDEADLK}:
                raise
            raise RuntimeError(f"Another full-suite runner owns {path}; wait for it to finish") from error
        yield


def run_suite(preset: str, jobs: int, output: Path, ctest: str = "ctest", root: Path = ROOT) -> int:
    with suite_lock(root / "build/.test-suite.lock"):
        return run_locked_suite(preset, jobs, output, ctest, root)


def run_locked_suite(preset: str, jobs: int, output: Path, ctest: str, root: Path) -> int:
    output.mkdir(parents=True, exist_ok=False)
    manifest = subprocess.run([ctest, "--preset", preset, "--show-only=json-v1"], cwd=root,
                              check=True, text=True, capture_output=True)
    selected = json.loads(manifest.stdout)
    validate_schedule(selected)
    expected = [test["name"] for test in selected["tests"]]
    if not expected:
        raise ValueError("The selected CTest preset contains no tests")
    (output / "manifest.json").write_text(manifest.stdout, encoding="utf-8")
    command = [ctest, "--preset", preset, "--parallel", str(jobs), "--output-on-failure",
               "--no-tests=error", "--output-log", str(output / "ctest.log"),
               "--output-junit", str(output / "junit.xml")]
    started = time.monotonic()
    completed = subprocess.run(command, cwd=root, check=False)
    elapsed = time.monotonic() - started
    report = read_junit(output / "junit.xml", elapsed)
    actual = [row["name"] for row in report["tests"]]
    complete = Counter(actual) == Counter(expected)
    report.update(preset=preset, jobs=jobs, ctest_exit_code=completed.returncode,
                  complete=complete, missing=sorted(set(expected) - set(actual)),
                  unexpected=sorted(set(actual) - set(expected)))
    (output / "timings.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print_report(report)
    print(f"Timing report: {output / 'timings.json'}")
    if not complete:
        print("CTest result inventory differs from the selected preset", file=sys.stderr)
    return completed.returncode or (1 if not complete or report["counts"].get("failed", 0) else 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="dev")
    parser.add_argument("--jobs", type=int, default=4, help="Bounded CTest concurrency; exclusive courses stay serial; only audited correctness may overlap")
    parser.add_argument("--output-dir", type=Path, help="New directory for this run's log, JUnit and timing report")
    parser.add_argument("--report-log", type=Path, help="Only summarize an existing CTest console log")
    parser.add_argument("--json", action="store_true", help="Print a JSON report when reading --report-log")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.report_log:
        report = read_log(args.report_log)
        print(json.dumps(report, indent=2)) if args.json else print_report(report)
        return 0
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    output = args.output_dir or ROOT / "build/test-results" / args.preset / stamp
    return run_suite(args.preset, args.jobs, output.resolve())


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, KeyError, ET.ParseError, subprocess.CalledProcessError) as error:
        print(f"Test suite failed: {error}", file=sys.stderr)
        sys.exit(1)
