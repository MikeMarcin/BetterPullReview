#!/usr/bin/env python3
"""Exercise expectation/backstop failures and the four tightened Release limits."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess

from workbench_case_inventory import environment_for


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if not args.probe.is_absolute() or not args.output_dir.is_absolute():
        raise ValueError("Qualification paths must be absolute")
    args.output_dir.mkdir(exist_ok=False)
    rows = json.loads(args.manifest.read_text(encoding="utf-8"))["tests"]
    matches = [row for row in rows if row["name"] == "qt_composed_inline_drawer_scale_100"]
    if len(matches) != 1 or matches[0].get("config") != "Release":
        raise ValueError("Controls require the exact inventoried Release command")
    row = matches[0]
    props = {prop["name"]: prop["value"] for prop in row["properties"]}
    records = []

    def run(name: str, command: list[str], environment: dict | None, failing: bool, marker: str, cwd=None):
        completed = subprocess.run(command, env=environment, cwd=cwd, capture_output=True, timeout=120)
        (args.output_dir / f"{name}.stdout.log").write_bytes(completed.stdout)
        (args.output_dir / f"{name}.stderr.log").write_bytes(completed.stderr)
        text = (completed.stdout + completed.stderr).decode("utf-8", "replace")
        passed = ((completed.returncode != 0) == failing and marker in text
                  and (not failing or "FAILED:" in text and "inline-drawer:pass" not in text))
        records.append({"name": name, "command": command, "exit": completed.returncode, "passed": passed})
        (args.output_dir / "result.json").write_text(json.dumps({"complete": False, "records": records}, indent=2) + "\n")
        if not passed:
            raise ValueError(f"Negative control did not fail at its intended assertion: {name}")

    run("expect-pass", [str(args.probe), "pass"], None, False, "NDEBUG=1")
    run("expect-fail", [str(args.probe), "fail"], None, True, "qualification expectation control")
    run("expect-caught", [str(args.probe), "caught"], None, True, "caught before main")
    for name in ("open", "scroll", "tick-go-to", "markdown-open"):
        environment = environment_for(row)
        environment["BPR_QUALIFICATION_LATENCY_CONTROL"] = name
        run(f"latency-{name}", row["command"], environment, True,
            f"INLINE_LATENCY_ASSERTION:{name};limit-ms=0", props["WORKING_DIRECTORY"])
    (args.output_dir / "result.json").write_text(json.dumps({"complete": True, "passed": True, "records": records}, indent=2) + "\n")
    print("QUALIFICATION_ASSERTION_CONTROLS_OK: expectation, caught backstop and four latency limits")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
