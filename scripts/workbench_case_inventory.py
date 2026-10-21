#!/usr/bin/env python3
"""Validate ordered workbench case occurrences without crediting list-only runs as tests."""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def expected_rows(platform: str, root: Path = ROOT) -> dict:
    registry = json.loads((root / "tests/command_correctness_registry.json").read_text(encoding="utf-8"))
    family = next(family for family in registry["families"] if family["source"].endswith("/workbench_window_tests.cpp"))
    inventory = json.loads((root / "tests/workbench_case_inventory.json").read_text(encoding="utf-8"))
    if hashlib.sha256((root / family["source"]).read_bytes()).hexdigest() != inventory["source_sha256"]:
        raise ValueError("Workbench case inventory source identity is stale")
    rows = {}
    for row in family["rows"]:
        if platform not in row["platforms"]:
            continue
        if row["name"] in rows or row["selector"] not in inventory["selector_cases"]:
            raise ValueError("Unknown or duplicate workbench selection")
        cases = inventory["selector_cases"][row["selector"]]
        if not cases or len(cases) != len(set(cases)):
            raise ValueError("Empty or duplicate workbench case credit")
        rows[row["name"]] = {"selector": row["selector"], "cases": cases}
    if not rows:
        raise ValueError("No workbench rows for the selected platform")
    return rows


def validate_cases(expected: list[str], output: str) -> None:
    actual = [line.removeprefix("WORKBENCH_CASE:") for line in output.splitlines() if line.startswith("WORKBENCH_CASE:")]
    if actual != expected or len(actual) != len(set(actual)):
        raise ValueError(f"Workbench case occurrence mismatch: expected {len(expected)}, observed {len(actual)}")
    if ":pass" in output:
        raise ValueError("List-only inspection incorrectly claimed passing execution")


def validate_manifest(manifest: dict, platform: str, root: Path = ROOT) -> dict:
    expected = expected_rows(platform, root)
    rows = [row for row in manifest["tests"] if any(Path(arg).name.removesuffix(".exe") == "better_pull_review_qt_workbench_window_tests"
                                                    for arg in row.get("command", []))]
    names = [row["name"] for row in rows]
    if Counter(names) != Counter(expected.keys()):
        raise ValueError("Missing, duplicate or unknown workbench invocation")
    registry = json.loads((root / "tests/command_correctness_registry.json").read_text(encoding="utf-8"))
    declarations = {row["name"]: row for family in registry["families"] for row in family["rows"]}
    for row in rows:
        declared = declarations[row["name"]]
        arguments = declared["arguments"]
        actual_arguments = row["command"][-len(arguments):] if arguments else []
        if actual_arguments != arguments:
            raise ValueError("Workbench selector differs from its registered occurrence")
        properties = {prop["name"]: prop["value"] for prop in row["properties"]}
        directory = Path(properties["WORKING_DIRECTORY"])
        expected_environment = [item.replace("{binary_dir}", directory.as_posix()).replace("{build_dir}", directory.parents[1].as_posix()) for item in declared["environment"]]
        # A missing scale means CTest historically inherited the native 1.0 default.
        # The candidate makes that precondition explicit before permitting overlap.
        environment = properties.get("ENVIRONMENT", [])
        if any(item not in environment for item in expected_environment):
            raise ValueError("Workbench scale/output precondition differs from its registered occurrence")
    return {row["name"]: row for row in rows}


def environment_for(row: dict) -> dict[str, str]:
    properties = {prop["name"]: prop["value"] for prop in row["properties"]}
    environment = dict(os.environ)
    for entry in properties.get("ENVIRONMENT", []):
        name, value = entry.split("=", 1)
        environment[name] = value
    for entry in properties.get("ENVIRONMENT_MODIFICATION", []):
        name, action = entry.split("=", 1)
        operation, value = action.split(":", 1)
        if operation == "unset":
            environment.pop(name, None)
        elif operation == "set":
            environment[name] = value
        elif operation == "path_list_prepend":
            environment[name] = value + (os.pathsep + environment[name] if environment.get(name) else "")
        else:
            raise ValueError(f"Unimplemented CTest environment operation: {operation}")
    return environment


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--platform", required=True, choices=["Windows", "Darwin", "Linux"])
    parser.add_argument("--execute-lists", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    rows = validate_manifest(manifest, args.platform)
    expected = expected_rows(args.platform)
    result = {"schema": "bpr.workbench-occurrences.v1", "platform": args.platform,
              "inspection_only": True, "behavioral_pass_claim": False, "rows": []}
    for name, row in rows.items():
        record = {"name": name, "configuration": row.get("config"), **expected[name]}
        if args.execute_lists:
            properties = {prop["name"]: prop["value"] for prop in row["properties"]}
            completed = subprocess.run([*row["command"], "--list-cases"], cwd=properties["WORKING_DIRECTORY"],
                                       env=environment_for(row), capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30, check=True)
            validate_cases(record["cases"], completed.stdout)
            build = [line for line in completed.stdout.splitlines() if line.startswith("WORKBENCH_BUILD:")]
            if len(build) != 1 or "NDEBUG=0" not in build[0]:
                raise ValueError("Workbench binary does not have the inventoried Debug preconditions")
            if args.platform == "Windows" and "ITERATOR_DEBUG_LEVEL=2" not in build[0]:
                raise ValueError("Workbench binary does not preserve checked MSVC iterator diagnostics")
            record["build_preconditions"] = build[0]
            record["listed"] = True
        result["rows"].append(record)
    result["invocations"] = len(rows)
    result["case_occurrences"] = sum(len(row["cases"]) for row in result["rows"])
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Workbench inventory: {len(rows)} invocations, {result['case_occurrences']} ordered case occurrences; inspection only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
