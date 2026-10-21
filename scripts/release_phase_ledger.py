#!/usr/bin/env python3
"""Append request/phase facts without scheduling, skipping or approving release work."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

from run_test_suite import suite_lock

SCHEMA = "bpr.release-phase-ledger.v1"


def utc() -> str:
    return datetime.now(timezone.utc).isoformat()


def instant(value: str) -> datetime:
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("Ledger instants require an explicit timezone")
    return result


def read(path: Path) -> list[dict]:
    rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    if not rows or rows[0].get("schema") != SCHEMA or rows[0].get("event") != "request":
        raise ValueError("Unknown or incomplete request ledger")
    if any(row.get("sequence") != index or row.get("request_id") != rows[0]["request_id"] for index, row in enumerate(rows)):
        raise ValueError("Ledger sequence/request identity changed")
    return rows


def append(path: Path, event: dict) -> dict:
    with suite_lock(path.with_suffix(path.suffix + ".lock")):
        rows = read(path)
        event = {**event, "sequence": len(rows), "request_id": rows[0]["request_id"], "recorded_utc": utc()}
        with path.open("a", encoding="utf-8") as output:
            output.write(json.dumps(event, sort_keys=True) + "\n")
            output.flush()
            os.fsync(output.fileno())
    return event


def initialize(path: Path, request_start: str, context: dict) -> None:
    if instant(request_start) > datetime.now(timezone.utc):
        raise ValueError("Request start cannot be in the future")
    path.parent.mkdir(parents=True, exist_ok=True)
    with suite_lock(path.with_suffix(path.suffix + ".lock")):
        with path.open("x", encoding="utf-8") as output:
            output.write(json.dumps(dict(schema=SCHEMA, event="request", sequence=0, request_id=uuid.uuid4().hex,
                                         request_start_utc=request_start, recorded_utc=utc(), context=context), sort_keys=True) + "\n")
            output.flush()
            os.fsync(output.fileno())


def begin(path: Path, phase: str, context: dict, command: list[str]) -> str:
    identity = uuid.uuid4().hex
    append(path, dict(event="start", phase_id=identity, phase=phase, started_utc=utc(), context=context,
                      command=command, wrapper_pid=os.getpid()))
    return identity


def finish(path: Path, identity: str, *, outcome: str, seconds: float, exit_code: int | None, detail: str = "") -> None:
    rows = read(path)
    if not any(row.get("event") == "start" and row.get("phase_id") == identity for row in rows):
        raise ValueError("Unknown phase identity")
    if any(row.get("event") == "end" and row.get("phase_id") == identity for row in rows):
        raise ValueError("A terminal phase cannot be rewritten")
    append(path, dict(event="end", phase_id=identity, ended_utc=utc(), seconds=seconds,
                      outcome=outcome, exit_code=exit_code, detail=detail))


def run(path: Path, phase: str, context: dict, command: list[str]) -> int:
    if not command:
        raise ValueError("A child command is required")
    identity = begin(path, phase, context, command)
    started = time.monotonic()
    try:
        child = subprocess.Popen(command)
        append(path, dict(event="child", phase_id=identity, child_pid=child.pid))
        code = child.wait()
    except OSError as error:
        finish(path, identity, outcome="launch-failed", seconds=time.monotonic() - started, exit_code=None, detail=str(error))
        return 1
    # An interrupted wrapper leaves an open phase. It cannot claim child completion.
    finish(path, identity, outcome="passed" if code == 0 else "failed", seconds=time.monotonic() - started, exit_code=code)
    return code


def summary(path: Path) -> dict:
    rows = read(path)
    ends = {row["phase_id"]: row for row in rows if row["event"] == "end"}
    phases = [{**row, "terminal": ends.get(row["phase_id"])} for row in rows if row["event"] == "start"]
    return dict(schema=SCHEMA, request=rows[0], observed_until_utc=utc(),
                request_elapsed_seconds=(datetime.now(timezone.utc) - instant(rows[0]["request_start_utc"])).total_seconds(),
                phases=phases, pending=[row["phase_id"] for row in phases if row["terminal"] is None],
                summed_phase_seconds=sum(row["seconds"] for row in ends.values()),
                note="Summed overlapping work is not critical-path elapsed time; this ledger confers no acceptance.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", type=Path, required=True)
    commands = parser.add_subparsers(dest="action", required=True)
    init = commands.add_parser("init")
    init.add_argument("--request-start", required=True)
    init.add_argument("--context", type=Path, required=True, help="Nonsecret JSON source/config/tools/cache/lease facts")
    execute = commands.add_parser("run")
    execute.add_argument("--phase", required=True)
    execute.add_argument("--context", type=Path, required=True)
    execute.add_argument("command", nargs=argparse.REMAINDER)
    commands.add_parser("summary")
    args = parser.parse_args()
    if args.action == "init":
        initialize(args.ledger, args.request_start, json.loads(args.context.read_text()))
        return 0
    if args.action == "run":
        command = args.command[1:] if args.command[:1] == ["--"] else args.command
        return run(args.ledger, args.phase, json.loads(args.context.read_text()), command)
    print(json.dumps(summary(args.ledger), indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f"Phase ledger refused: {error}", file=sys.stderr)
        sys.exit(1)
