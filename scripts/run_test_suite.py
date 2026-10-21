#!/usr/bin/env python3
"""Run every test in a CTest preset and retain an honest timing report."""
from __future__ import annotations

import argparse
from collections import Counter
from contextlib import contextmanager
from datetime import datetime, timezone
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ROW = re.compile(r"\d+/\d+ Test\s+#\d+: (\S+)\s+\.{2,}(.*?)\s*([\d.]+) sec")
TOOLING_PREFLIGHT = frozenset({
    "documentation_structure", "documentation_issue_graph_contract", "issue_id_allocation_contract",
})


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


def validate_command_owned_row(row: dict, props: dict, root: Path = ROOT) -> None:
    registry = json.loads((root / "tests/command_correctness_registry.json").read_text(encoding="utf-8"))
    if registry["schema"] != "bpr.command-correctness.v1":
        raise ValueError("Unknown command correctness registry schema")
    matches = [(family, candidate) for family in registry["families"] for candidate in family["rows"]
               if candidate["name"] == row["name"]]
    if len(matches) != 1:
        raise ValueError(f"Unknown or duplicate command ownership: {row['name']}")
    family, candidate = matches[0]
    if family["configurations"] not in (["Debug"], ["Debug", "Release"]) or row.get("config", "Debug") not in family["configurations"]:
        raise ValueError("Unaudited command correctness configuration")
    source = root / family["source"]
    directory = Path(props["WORKING_DIRECTORY"])
    replacements = {"{binary_dir}": directory.as_posix(), "{build_dir}": directory.parents[1].as_posix(),
                    "{source_dir}": root.as_posix(), "{configuration}": row.get("config", "Debug")}
    expected_arguments = []
    for argument in candidate["arguments"]:
        for token, value in replacements.items():
            argument = argument.replace(token, value)
        expected_arguments.append(argument)
    command = row.get("command", [])
    tail = len(expected_arguments) + 1
    actual = command[-tail:]
    valid_command = (len(actual) == tail
                     and Path(actual[0]).name.removesuffix(".exe") == candidate["executable"]
                     and actual[1:] == expected_arguments)
    expected_environment = [item.replace("{binary_dir}", directory.as_posix()).replace("{build_dir}", directory.parents[1].as_posix())
                            for item in candidate["environment"]]
    environment = props.get("ENVIRONMENT", [])
    valid_environment = all(item in environment for item in expected_environment)
    modifications = props.get("ENVIRONMENT_MODIFICATION", [])
    if family["source"].endswith("/workbench_window_tests.cpp") and not any(item.startswith("BPR_QT_RENDER_PATH=") for item in expected_environment):
        valid_environment = valid_environment and modifications.count("BPR_QT_RENDER_PATH=unset:") == 1
    for item in expected_environment:
        variable = item.split("=", 1)[0].upper()
        valid_environment = (valid_environment
                             and sum(entry.split("=", 1)[0].upper() == variable for entry in environment) == 1
                             and not any(entry.split("=", 1)[0].upper() == variable for entry in modifications))
    valid_unsets = all(modifications.count(f"{variable}=unset:") == 1
                       and sum(item.split("=", 1)[0].upper() == variable.upper() for item in modifications) == 1
                       for variable in family["unset_environment"])
    valid_inputs = all(hashlib.sha256((root / item["path"]).read_bytes()).hexdigest() == item["sha256"]
                       for item in family["inputs"])
    if (candidate["scheduling"] != "correctness" or not valid_command or not valid_unsets or not valid_environment or not valid_inputs
            or hashlib.sha256(source.read_bytes()).hexdigest() != family["sha256"]):
        raise ValueError(f"Invalid command ownership declaration: {row['name']}")


def validate_schedule(manifest: dict) -> None:
    """Keep exclusive courses alone; admit only declared isolated correctness."""
    for row in manifest["tests"]:
        props = {prop["name"]: prop["value"] for prop in row.get("properties", [])}
        directory = props.get("WORKING_DIRECTORY", "").replace("\\", "/")
        platforms = [item for item in props.get("ENVIRONMENT", []) if item.upper().startswith("QT_QPA_PLATFORM=")]
        labels = props.get("LABELS", [])
        course = "/tests/end_to_end" in directory or "/tests/adapters_qt" in directory
        declared = "bpr_isolated_correctness" in labels
        config_labels = [label for label in labels if label.startswith("bpr_correctness_config_")]
        mode_labels = [label for label in labels if label.startswith("bpr_correctness_") and label not in config_labels]
        if (mode_labels or config_labels) and not declared:
            raise ValueError(f"Invalid isolated correctness scheduling declaration: {row['name']}")
        if declared:
            locks = props.get("RESOURCE_LOCK", [])
            valid_platform = (
                mode_labels == ["bpr_correctness_headless"] and not platforms
                or mode_labels == ["bpr_correctness_offscreen"] and platforms == ["QT_QPA_PLATFORM=offscreen"]
            )
            configuration = row.get("config", "Debug")
            valid_configuration = (configuration in {"Debug", "Release"}
                                   and config_labels == [f"bpr_correctness_config_{configuration}"]
                                   and (configuration == "Debug" or "bpr_command_owned" in labels))
            valid_lock = any(re.fullmatch(r"bpr_isolated_correctness_[a-z0-9_]+", lock) for lock in locks)
            platform_override = any(item.upper().startswith("QT_QPA_PLATFORM=") for item in props.get("ENVIRONMENT_MODIFICATION", []))
            platform_argument = any(
                item in {"-platform", "--platform"} or item.startswith(("-platform=", "--platform="))
                for item in row.get("command", [])
            )
            exclusive = props.get("RUN_SERIAL") or "bpr_explicit_serial" in labels or "bpr_serial_gui" in labels
            if not valid_platform or not valid_lock or not valid_configuration or platform_override or platform_argument or exclusive:
                raise ValueError(f"Invalid isolated correctness scheduling declaration: {row['name']}")
        if "bpr_command_owned" in labels:
            if not declared:
                raise ValueError(f"Invalid command ownership declaration: {row['name']}")
            validate_command_owned_row(row, props)
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


def run_suite(preset: str, jobs: int, output: Path, ctest: str = "ctest", root: Path = ROOT, tests_regex: str | None = None) -> int:
    with suite_lock(root / "build/.test-suite.lock"):
        return run_locked_suite(preset, jobs, output, ctest, root, tests_regex)


def committed_tooling_pin(root: Path) -> dict:
    """Bind the declared test dependency; MASK owns deployment verification."""
    locked = subprocess.run(["git", "show", "HEAD:.mask.lock.json"], cwd=root,
                            check=True, capture_output=True).stdout
    value = json.loads(locked)
    if (not isinstance(value, dict) or not isinstance(value.get("packages"), list)
            or not all(isinstance(row, dict) for row in value["packages"])):
        raise ValueError("Committed MASK lock has an invalid package inventory")
    rows = [row for row in value["packages"]
            if row.get("source") == "agentskills" and row.get("id") == "implement-rfc"]
    if len(rows) != 1:
        raise ValueError("Committed MASK lock must select exactly one implement-rfc test dependency")
    row = rows[0]
    if (not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", row.get("commit", ""))
            or not re.fullmatch(r"sha256:[0-9a-f]{64}", row.get("payload_digest", ""))):
        raise ValueError("Committed implement-rfc test dependency has an invalid pin")
    return {"lock_source": "HEAD:.mask.lock.json", "lock_sha256": hashlib.sha256(locked).hexdigest(),
            "source": row["source"], "id": row["id"], "commit": row["commit"],
            "payload_digest": row["payload_digest"]}


def tooling_preflight(expected: list[str], preset: str, jobs: int, output: Path, ctest: str, root: Path) -> dict | None:
    names = [name for name in expected if name in TOOLING_PREFLIGHT]
    if not names:
        return None
    report = {"schema": "bpr.test-preflight.v1", "expected": names, "passed": False}
    started = time.monotonic()
    try:
        report["declared_dependency"] = committed_tooling_pin(root)
        expression = "^(" + "|".join(re.escape(name) for name in names) + ")$"
        command = [ctest, "--preset", preset, "--parallel", str(jobs), "--output-on-failure",
                   "--no-tests=error", "--tests-regex", expression,
                   "--output-log", str(output / "preflight.log"),
                   "--output-junit", str(output / "preflight-junit.xml")]
        report["command"] = command
        completed = subprocess.run(command, cwd=root, check=False)
        report["ctest_exit_code"] = completed.returncode
        results = read_junit(output / "preflight-junit.xml", time.monotonic() - started)
        report["results"] = results
        actual = [row["name"] for row in results["tests"]]
        report["complete"] = Counter(actual) == Counter(names)
        report["passed"] = (report["complete"] and completed.returncode == 0
                            and results["counts"].get("passed", 0) == len(names))
    except (OSError, ValueError, KeyError, TypeError, ET.ParseError, subprocess.CalledProcessError) as error:
        report["error"] = str(error)
    report["wall_seconds"] = time.monotonic() - started
    (output / "preflight.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def bootstrap_inputs(root: Path, manifest: dict, ctest: str) -> str:
    """Seal same-invocation source/bootstrap inputs; never authorize native reuse."""
    names = subprocess.check_output(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=root).decode().split("\0")
    files = {root / name for name in names if name}
    # The three source gates import the actual pinned deployment, outside Git.
    helper = root / ".agents/skills/implement-rfc"
    if not helper.is_dir():
        raise ValueError("Pinned bootstrap deployment is unavailable")
    files.update(path for path in helper.resolve().rglob("*")
                 if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc")
    for tool in (ctest, "cmake", "git", sys.executable):
        found = shutil.which(tool)
        if found is None:
            raise ValueError(f"Unknown bootstrap tool: {tool}")
        files.add(Path(found).resolve())
    digest = hashlib.sha256()
    for path in sorted(files, key=str):
        if not path.is_file():
            raise ValueError("Unknown bootstrap input type")
        digest.update(str(path).encode())
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    digest.update(json.dumps(manifest, sort_keys=True).encode())
    # Values stay private; their digest detects changed tool/native path options.
    digest.update(json.dumps(dict(os.environ), sort_keys=True).encode())
    digest.update(repr((sys.version, sys.flags, sys.platform, os.name)).encode())
    return digest.hexdigest()


def reusable_bootstrap(report: dict | None, invocation: str, identity: str | None) -> list[str]:
    """Only complete passing proof produced by this live invocation can be reused."""
    if not isinstance(report, dict) or identity is None:
        return []
    names = report.get("expected", [])
    results = report.get("results", {})
    if not isinstance(names, list) or not all(isinstance(name, str) for name in names) or not isinstance(results, dict):
        return []
    rows = results.get("tests", [])
    if not isinstance(rows, list) or not all(isinstance(row, dict) and isinstance(row.get("name"), str) and row.get("status") == "passed" for row in rows):
        return []
    if (report.get("invocation_id") != invocation or report.get("input_identity") != identity
            or not report.get("passed") or not report.get("complete")
            or report.get("ctest_exit_code") != 0 or set(names) - TOOLING_PREFLIGHT
            or len(names) != len(set(names)) or not names
            or results.get("counts") != {"passed": len(names)}
            or Counter(row["name"] for row in rows) != Counter(names)):
        return []
    return names


def run_locked_suite(preset: str, jobs: int, output: Path, ctest: str, root: Path, tests_regex: str | None = None) -> int:
    if sys.flags.optimize or os.environ.get("PYTHONOPTIMIZE"):
        raise ValueError("Qualification requires Python assertions enabled (no -O/PYTHONOPTIMIZE)")
    runner_started = time.monotonic()
    output.mkdir(parents=True, exist_ok=False)
    manifest = subprocess.run([ctest, "--preset", preset, "--show-only=json-v1"], cwd=root,
                              check=True, text=True, capture_output=True)
    selected = json.loads(manifest.stdout)
    validate_schedule(selected)
    all_names = [test["name"] for test in selected["tests"]]
    expected = [name for name in all_names if tests_regex is None or re.search(tests_regex, name)]
    if not expected:
        raise ValueError("The selected CTest preset contains no tests")
    (output / "manifest.json").write_text(manifest.stdout, encoding="utf-8")
    invocation = uuid.uuid4().hex
    identity = None
    identity_error = None
    if TOOLING_PREFLIGHT.intersection(all_names):
        try:
            identity = bootstrap_inputs(root, selected, ctest)
        except (OSError, ValueError, subprocess.CalledProcessError) as error:
            identity_error = str(error)
    preflight = tooling_preflight(all_names, preset, jobs, output, ctest, root)
    if preflight is not None:
        preflight.update(invocation_id=invocation, input_identity=identity, input_error=identity_error,
                         scope="same-invocation-bootstrap; no cross-request or native qualification reuse")
        (output / "preflight.json").write_text(json.dumps(preflight, indent=2) + "\n", encoding="utf-8")
    if preflight is not None and not preflight["passed"]:
        report = summarize([], 0.0)
        report.update(preset=preset, jobs=jobs, full_graph_started=False, complete=False,
                      missing=expected, unexpected=[], not_run=expected,
                      preflight_wall_seconds=preflight["wall_seconds"],
                      total_runner_wall_seconds=time.monotonic() - runner_started,
                      reason="tooling prerequisite preflight failed")
        (output / "timings.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Full graph not started: tooling prerequisite preflight failed; {len(expected)} tests not run", file=sys.stderr)
        print(f"Preflight report: {output / 'preflight.json'}", file=sys.stderr)
        return preflight.get("ctest_exit_code", 1) or 1
    current_identity = None
    if identity is not None:
        try:
            current_identity = bootstrap_inputs(root, selected, ctest)
        except (OSError, ValueError, subprocess.CalledProcessError):
            pass
    reused = sorted(set(reusable_bootstrap(preflight, invocation, current_identity)).intersection(expected))
    fresh = [name for name in expected if name not in reused]
    command = [ctest, "--preset", preset, "--parallel", str(jobs), "--output-on-failure",
               "--no-tests=error", "--output-log", str(output / "ctest.log"),
               "--output-junit", str(output / "junit.xml")]
    if tests_regex is not None:
        command.extend(["--tests-regex", tests_regex])
    if reused:
        command.extend(["--exclude-regex", "^(" + "|".join(re.escape(name) for name in reused) + ")$"])
    started = time.monotonic()
    if fresh:
        completed = subprocess.run(command, cwd=root, check=False)
        report = read_junit(output / "junit.xml", time.monotonic() - started)
    else:
        completed = subprocess.CompletedProcess(command, 0)
        report = summarize([], 0.0)
    elapsed = time.monotonic() - started
    if reused:
        proof_rows = [row for row in preflight["results"]["tests"] if row["name"] in reused]
        report = summarize(report["tests"] + [{**row, "evidence": "same-invocation-preflight"} for row in proof_rows], elapsed)
    actual = [row["name"] for row in report["tests"]]
    complete = Counter(actual) == Counter(expected)
    final_identity = None
    if identity is not None:
        try:
            final_identity = bootstrap_inputs(root, selected, ctest)
        except (OSError, ValueError, subprocess.CalledProcessError):
            pass
        complete = complete and identity == current_identity == final_identity
    report.update(preset=preset, jobs=jobs, ctest_exit_code=completed.returncode,
                  qualification_scope={"fresh": fresh, "reused": reused,
                      "reuse_scope": "same-invocation-bootstrap", "cross_request_reuse": "unready", "native_reuse": False,
                      "input_identity": identity, "input_identity_after_preflight": current_identity, "input_identity_after_execution": final_identity,
                      "release_gate_substitution": False, "required_debug": "complete affected-native Debug at every release candidate"},
                  complete=complete, full_graph_started=True, full_graph_selected=tests_regex is None, selection_regex=tests_regex,
                  preflight_wall_seconds=preflight["wall_seconds"] if preflight else 0.0,
                  total_runner_wall_seconds=time.monotonic() - runner_started,
                  missing=sorted(set(expected) - set(actual)),
                  unexpected=sorted(set(actual) - set(expected)))
    (output / "timings.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print_report(report)
    if preflight is not None:
        print(f"Prerequisite preflight: {preflight['wall_seconds']:.2f}s; total runner wall: {report['total_runner_wall_seconds'] / 60:.2f} min")
    print(f"Timing report: {output / 'timings.json'}")
    if not complete:
        print("CTest result inventory differs from the selected preset", file=sys.stderr)
    return completed.returncode or (1 if not complete or report["counts"].get("failed", 0) else 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="dev")
    parser.add_argument("--jobs", type=int, default=4, help="Bounded CTest concurrency; exclusive courses stay serial; only audited correctness may overlap")
    parser.add_argument("--output-dir", type=Path, help="New directory for this run's log, JUnit and timing report")
    parser.add_argument("--tests-regex", help="Bounded selection from the validated preset; the existing tooling preflight still runs")
    parser.add_argument("--phase-ledger", type=Path, help="Append this attempt to an existing request ledger, including refusal/failure")
    parser.add_argument("--phase-context", type=Path, help="Nonsecret JSON source/config/tools/cache/lease facts for --phase-ledger")
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
    if bool(args.phase_ledger) != bool(args.phase_context):
        parser.error("--phase-ledger and --phase-context are required together")
    if not args.phase_ledger:
        return run_suite(args.preset, args.jobs, output.resolve(), tests_regex=args.tests_regex)
    import release_phase_ledger as ledger
    context = json.loads(args.phase_context.read_text(encoding="utf-8"))
    context.update(preset=args.preset, jobs=args.jobs, output=str(output.resolve()), selection_regex=args.tests_regex)
    identity = ledger.begin(args.phase_ledger, "qualification", context, sys.argv)
    started = time.monotonic()
    try:
        code = run_suite(args.preset, args.jobs, output.resolve(), tests_regex=args.tests_regex)
    except Exception as error:
        ledger.finish(args.phase_ledger, identity, outcome="refused", seconds=time.monotonic() - started, exit_code=None, detail=str(error))
        raise
    ledger.finish(args.phase_ledger, identity, outcome="passed" if code == 0 else "failed", seconds=time.monotonic() - started, exit_code=code)
    return code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, KeyError, ET.ParseError, subprocess.CalledProcessError) as error:
        print(f"Test suite failed: {error}", file=sys.stderr)
        sys.exit(1)
