#!/usr/bin/env python3
"""Forced death before/after every staged witness and applied server effect."""
from pathlib import Path
import argparse
import sqlite3
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
# [[BPR-1263]]. On Windows `mkdir(mode=0o700)` writes CPython's owner-only
# descriptor, which the staged journal refuses; this writes the stores' own.
from bpr_narrow_owner_private import make_root_private
from process_kill_support import kill_at_checkpoint, run

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    executable = parser.parse_args().executable
    with contained_temporary_directory(prefix="bpr-staged-kill-") as raw:
        for step in range(5):
            for boundary in ("prepared", "witnessed", "applied"):
                root = Path(raw) / f"{step}-{boundary}"
                make_root_private(root)
                kill_at_checkpoint(executable, f"--kill-{step}-{boundary}", root)
                trace = root / "server-writes"
                before = trace.read_bytes() if trace.exists() else b""
                run(executable, "--recover", root)
                run(executable, "--recover", root)
                after = trace.read_bytes() if trace.exists() else b""
                if after != before:
                    raise RuntimeError("Read-only restart emitted a mutation")
                if boundary != "witnessed":
                    run(executable, "--resume", root)
                    lines = trace.read_text().splitlines()
                    if len(lines) != 5:
                        raise RuntimeError("Resume duplicated or omitted a publication effect")
                connection = sqlite3.connect((root / "state/v1/review-mutation-recovery.sqlite3").as_uri()+"?mode=ro", uri=True)
                try:
                    if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)] or connection.execute("PRAGMA foreign_key_check").fetchall():
                        raise RuntimeError("Killed journal integrity failed")
                finally:
                    connection.close()
                print(f"staged-kill:{step}:{boundary}:pass")
        for step in range(3):
            for boundary in ("prepared", "witnessed", "applied"):
                root = Path(raw) / f"update-{step}-{boundary}"
                make_root_private(root)
                run(executable, "--init-update", root)
                kill_at_checkpoint(executable, f"--update-kill-{step}-{boundary}", root)
                trace = root / "server-writes"
                before = trace.read_bytes()
                run(executable, "--update-recover", root)
                if trace.read_bytes() != before:
                    raise RuntimeError("Maintenance recovery emitted a write")
                if boundary != "witnessed":
                    run(executable, "--update-resume", root)
                    if len(trace.read_text().splitlines()) != 8:
                        raise RuntimeError("Maintenance resume duplicated or omitted an edit")
                print(f"maintenance-kill:{step}:{boundary}:pass")
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
