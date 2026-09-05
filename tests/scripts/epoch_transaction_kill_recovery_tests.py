#!/usr/bin/env python3
"""Force death inside the real epoch journal's uncommitted transactions."""

from __future__ import annotations

import argparse
from pathlib import Path
import sqlite3
import subprocess

from epoch_kill_recovery_tests import inspect_recovered_state
from process_kill_support import kill_at_checkpoint, run
from scripts.contained_temporary_support import contained_temporary_directory


def inspect_rollback(root: Path, boundary: str, with_history: bool) -> None:
    databases = list((root / "catalog").rglob("*.sqlite3"))
    observed = []
    for database in databases:
        connection = sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)
        try:
            if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                raise RuntimeError("killed transaction damaged catalog integrity")
            if connection.execute("PRAGMA foreign_key_check").fetchall():
                raise RuntimeError("killed transaction damaged catalog foreign keys")
            if not connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='append_operation'"
            ).fetchall():
                continue
            epochs = connection.execute("SELECT count(*) FROM epoch").fetchone()[0]
            counts = (epochs,) + tuple(connection.execute(
                f"SELECT count(*) FROM {table} JOIN operation USING (incarnation, operation) "
                "WHERE operation.terminal=0"
            ).fetchone()[0] for table in ("append_receiving", "append_receipt", "append_install"))
            journal = connection.execute(
                "SELECT phase, operation_revision FROM operation WHERE kind=1 AND terminal=0"
            ).fetchall()
            observed.append((counts, journal))
            if with_history:
                identities = [f"{scope.hex()}/{incarnation.hex()}/{ordinal}" for scope, incarnation, ordinal
                              in connection.execute("SELECT pr_scope, incarnation, ordinal FROM epoch")]
                if identities != [(root / "expected-visible").read_text()]:
                    raise RuntimeError("transaction kill replaced the prior committed epoch identity")
        finally:
            connection.close()
    epochs = int(with_history)
    expected = {
        "receiving": ((epochs, 0, 0, 0), [(1, 1)]),
        "spool": ((epochs, 1, 0, 0), [(1, 2)]),
        "install": ((epochs, 1, 1, 0), [(2, 3)]),
    }
    if observed != [expected[boundary]]:
        raise RuntimeError(f"{boundary}: uncommitted facts did not roll back: {observed}")
    pattern = "*.pack.receiving" if boundary == "receiving" else "*.pack.part"
    spools = list((root / "git").rglob(pattern))
    expected_bytes = 0 if boundary == "receiving" else 230
    if len(spools) != 1 or spools[0].stat().st_size != expected_bytes:
        raise RuntimeError(f"{boundary}: missing exact precommit spool state")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--git", type=Path, required=True)
    args = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-epoch-transaction-kill-") as raw:
        root = Path(raw)
        failures: list[str] = []
        for name in ("receiving", "spool", "install", "receiving-history", "spool-history", "install-history"):
            with_history = name.endswith("-history")
            boundary = name.removesuffix("-history")
            course = root / name
            course.mkdir(mode=0o700)
            try:
                kill_at_checkpoint(args.executable, f"--kill-transaction-{name}", course)
                inspect_rollback(course, boundary, with_history)
                print(f"epoch-transaction-kill:{name}:rollback-pass", flush=True)
                if "--recover-epoch:pass" not in run(args.executable, "--recover-epoch", course, timeout=30):
                    raise RuntimeError("missing recovered host result")
                first = inspect_recovered_state(args.git, course)
                if "--recover-epoch:pass" not in run(args.executable, "--recover-epoch", course, timeout=30):
                    raise RuntimeError("missing repeated recovery result")
                if inspect_recovered_state(args.git, course) != first:
                    raise RuntimeError("another restart changed retained refs")
                print(f"epoch-transaction-kill:{name}:pass", flush=True)
            except (OSError, RuntimeError, sqlite3.Error, subprocess.SubprocessError) as error:
                failures.append(f"{name}: {error}")
        if failures:
            raise RuntimeError("\n".join(failures))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
