#!/usr/bin/env python3
"""Kill the real epoch host and recover its installed-Git/SQLite state locally."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sqlite3
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from process_kill_support import kill_at_checkpoint, run  # noqa: E402


COMMIT = "826bb464b84352d60b3d7c56481a599a838d998d"


def inspect_recovered_state(git: Path, root: Path, *, preserved_empty_spools: frozenset[Path] = frozenset()) -> str:
    repositories = list((root / "git/repositories/v1").glob("*.git"))
    if len(repositories) != 1:
        raise RuntimeError("recovery did not retain one Git repository")
    environment = dict(os.environ)
    for name in tuple(environment):
        if name.startswith("GIT_"):
            del environment[name]
    environment.update(
        GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
        GIT_OPTIONAL_LOCKS="0", GIT_TERMINAL_PROMPT="0",
    )

    # [[BPR-0622]]. A cache ref under a contained temporary root is longer than
    # MAX_PATH; without core.longpaths Git for Windows silently lists no refs.
    long_paths = ("-c", "core.longpaths=true") if os.name == "nt" else ()

    def inspect(*arguments: str) -> str:
        completed = subprocess.run(
            [str(git), *long_paths, f"--git-dir={repositories[0]}", *arguments],
            env=environment, check=True, capture_output=True, text=True, timeout=15,
        )
        return completed.stdout

    inspect("fsck", "--full", "--strict", "--no-reflogs")
    refs = inspect("for-each-ref", "--format=%(refname) %(objectname)")
    rows = refs.splitlines()
    if len(rows) != 2 or any(not row.endswith(f" {COMMIT}") for row in rows):
        raise RuntimeError("recovery refs do not contain one exact base/head pair")
    for path in preserved_empty_spools:
        if not path.is_file() or path.stat().st_size != 0:
            raise RuntimeError("recovery changed a preserved unowned empty spool")
    if any(path not in preserved_empty_spools and path.is_file() and (path.name.endswith(".pack.receiving") or
                              path.name.endswith(".pack.part"))
           for path in (root / "git").rglob("*")):
        raise RuntimeError("recovery left an unfinished pack spool")
    databases = list((root / "catalog").rglob("*.sqlite3"))
    if not databases:
        raise RuntimeError("recovery left no catalog database")
    epoch_count = 0
    for database in databases:
        connection = sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)
        try:
            if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                raise RuntimeError("catalog integrity failed")
            if connection.execute("PRAGMA foreign_key_check").fetchall():
                raise RuntimeError("catalog foreign keys failed")
            if connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='epoch'"
            ).fetchall():
                epoch_count += connection.execute("SELECT count(*) FROM epoch").fetchone()[0]
        finally:
            connection.close()
    if epoch_count != 1:
        raise RuntimeError("recovery duplicated or lost the epoch")
    return refs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--git", type=Path, required=True)
    args = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-epoch-kill-") as raw:
        root = Path(raw)
        failures: list[str] = []
        for boundary in ("fetch", "spool", "prepared", "visible"):
            course = root / boundary
            course.mkdir(mode=0o700)
            try:
                kill_at_checkpoint(args.executable, f"--kill-{boundary}", course)
                if boundary == "fetch":
                    receiving = list((course / "git").rglob("*.pack.receiving"))
                    if len(receiving) != 1 or receiving[0].stat().st_size != 115:
                        raise RuntimeError("fetch kill did not interrupt a half-written pack")
                if "--recover-epoch:pass" not in run(args.executable, "--recover-epoch", course, timeout=30):
                    raise RuntimeError(f"{boundary}: missing recovered host result")
                first = inspect_recovered_state(args.git, course)
                if "--recover-epoch:pass" not in run(args.executable, "--recover-epoch", course, timeout=30):
                    raise RuntimeError(f"{boundary}: missing repeated recovery result")
                if inspect_recovered_state(args.git, course) != first:
                    raise RuntimeError(f"{boundary}: another restart changed retained refs")
                print(f"epoch-kill-recovery:{boundary}:pass")
            except (OSError, RuntimeError, sqlite3.Error, subprocess.SubprocessError) as error:
                failures.append(f"{boundary}: {error}")
        if failures:
            raise RuntimeError("\n".join(failures))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
