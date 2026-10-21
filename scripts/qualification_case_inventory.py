#!/usr/bin/env python3
"""Validate the bounded five-command qualification's actual ordered occurrences."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

from workbench_case_inventory import expected_rows

ROOT = Path(__file__).resolve().parents[1]
PILOT = {"qt_workbench_window_scale_100", "qt_composed_inline_drawer_scale_100",
         "core_diff_compute", "application_diff_host", "application_github_host"}


def validate(manifest: dict, log: str, configuration: str, platform: str, root: Path = ROOT) -> dict:
    if configuration not in {"Debug", "Release"}:
        raise ValueError("Unknown qualification configuration")
    inventory = json.loads((root / "tests/qualification_case_inventory.json").read_text(encoding="utf-8"))
    if inventory["schema"] != "bpr.qualification-cases.v1":
        raise ValueError("Unknown qualification case inventory")
    if hashlib.sha256((root / "tests/support/qualification_build.hpp").read_bytes()).hexdigest() != inventory["build_report_sha256"]:
        raise ValueError("Qualification build reporting input changed")
    expected = {}
    for row in inventory["rows"]:
        if hashlib.sha256((root / row["source"]).read_bytes()).hexdigest() != row["source_sha256"]:
            raise ValueError("Qualification source inventory is stale")
        if not row["cases"] or len(row["cases"]) != len(set(row["cases"])) or row["name"] in expected:
            raise ValueError("Duplicate or empty qualification cases")
        expected[row["name"]] = row["cases"]
    expected["qt_workbench_window_scale_100"] = expected_rows(platform, root)["qt_workbench_window_scale_100"]["cases"]
    if set(expected) != PILOT:
        raise ValueError("Incomplete qualification command inventory")
    parts = re.split(r"(?m)^\d+/\d+ Testing: ([^\r\n]+)\r?\n", log)
    blocks = {}
    for name, body in zip(parts[1::2], parts[2::2], strict=True):
        if name in blocks:
            raise ValueError("Duplicate executed qualification command")
        blocks[name] = body
    if set(blocks) != PILOT:
        raise ValueError("Missing or unknown executed qualification command")
    selected = [row for row in manifest["tests"] if row["name"] in PILOT]
    if len(selected) != len(PILOT) or any(row.get("config", configuration) != configuration for row in selected):
        raise ValueError("Wrong or duplicate qualification configuration")
    optimized = configuration == "Release"
    records = []
    for name in sorted(PILOT):
        body = blocks[name]
        prefix = "WORKBENCH" if name.startswith("qt_workbench") else "QUALIFICATION"
        began = re.findall(rf"(?m)^{prefix}_CASE_BEGIN:([^:\r\n]+)", body)
        ended = re.findall(rf"(?m)^{prefix}_CASE_END:([^:\r\n]+)", body)
        if began != expected[name] or ended != expected[name] or "Test Passed." not in body:
            raise ValueError(f"Incomplete or failed qualification occurrences: {name}")
        build = re.findall(rf"(?m)^{prefix}_BUILD:([^\r\n]+)", body)
        if len(build) != 1 or f"NDEBUG={int(optimized)}" not in build[0]:
            raise ValueError("Qualification binary has wrong assertion mode")
        if platform == "Windows" and f"ITERATOR_DEBUG_LEVEL={0 if optimized else 2}" not in build[0]:
            raise ValueError("Qualification binary has wrong STL mode")
        records.append({"name": name, "configuration": configuration, "cases": began, "build": build[0]})
    for name, marker, lines in [("core_diff_compute", "stop-lines", 40000 if optimized else 6000),
                                ("application_diff_host", "cancel-lines", 30000 if optimized else 6000)]:
        if f"QUALIFICATION_WORKLOAD:{marker}={lines}" not in blocks[name]:
            raise ValueError("Wrong seeded qualification workload")
    drawer = blocks["qt_composed_inline_drawer_scale_100"]
    limits = re.findall(r"INLINE_LATENCY_ASSERTION:([^;\r\n]+);limit-ms=(\d+)", drawer)
    required = [(name, "100") for name in ("open", "scroll", "tick-go-to", "markdown-open")]
    if limits != (required if optimized else []):
        raise ValueError("Wrong optimized latency assertion occurrence")
    sweeps = []
    if optimized or platform != "Windows":
        for label in ("throwing shutdown", "start_delete"):
            match = re.search(rf"BPR-1112 {label}: (\d+) allocations", blocks["application_github_host"])
            if match is None or int(match[1]) == 0:
                raise ValueError("Allocation sweep did not inject failures")
            sweeps.append({"label": label, "injected_allocations": int(match[1])})
    else:
        if blocks["application_github_host"].count("not run, because _ITERATOR_DEBUG_LEVEL 2") != 2:
            raise ValueError("Missing explicit checked-STL sweep exclusion")
    return {"schema": "bpr.qualification-occurrences.v1", "platform": platform, "configuration": configuration,
            "rows": records, "case_occurrences": sum(len(row["cases"]) for row in records),
            "latency_limits_ms": dict(limits), "allocation_sweeps": sweeps,
            "workload_lines": {"stop": 40000 if optimized else 6000, "cancel": 30000 if optimized else 6000}}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--configuration", choices=["Debug", "Release"], required=True)
    parser.add_argument("--platform", choices=["Windows", "Darwin", "Linux"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = validate(json.loads(args.manifest.read_text(encoding="utf-8")), args.log.read_text(encoding="utf-8", errors="replace"), args.configuration, args.platform)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"QUALIFICATION_OCCURRENCES_OK: {len(result['rows'])} commands, {result['case_occurrences']} completed cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
