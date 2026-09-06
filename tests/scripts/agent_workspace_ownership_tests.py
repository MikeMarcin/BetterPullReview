"""Native journal crash/restart and refusal evidence, entirely fixture-local."""
from __future__ import annotations

import argparse
import fcntl
import json
from pathlib import Path
import tempfile

from process_kill_support import kill_at_checkpoint, run


def create_root(parent: Path, name: str) -> Path:
    root = parent / name
    root.mkdir(mode=0o700)
    (root / "neighbourhood").mkdir(mode=0o700)
    return root


def check(exe: Path, phase: str, root: Path, expected: str) -> None:
    actual = run(exe, phase, root).strip()
    if actual != expected:
        raise RuntimeError(f"{phase}: expected {expected!r}, got {actual!r}")


def journal(root: Path) -> Path:
    records = list((root / "registry").glob("*.jsonl"))
    if len(records) != 1:
        raise RuntimeError("expected exactly one ownership journal")
    return records[0]


def workspace(root: Path) -> Path:
    return root / "neighbourhood" / ("betterpullreview-agent-" + journal(root).stem) / "workspace"


def assert_empty(root: Path) -> None:
    if list((root / "neighbourhood").iterdir()):
        raise RuntimeError("recovery retained owned directories")
    if list((root / "registry").glob("*.jsonl")):
        raise RuntimeError("recovery retained ownership journal")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    exe = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="bpr-workspace-ownership-") as temporary:
        parent = Path(temporary).resolve()
        # Real SIGKILL between committed native phases, including both renames.
        for point in [*(f"journal-{i}" for i in range(7)), "child-quarantined", "parent-quarantined"]:
            root = create_root(parent, point)
            kill_at_checkpoint(exe, point, root)
            check(exe, "recover", root, "recovered=1 live=0")
            check(exe, "recover", root, "recovered=0 live=0")
            assert_empty(root)
        # A mkdir without its original identity receipt is deliberately ambiguous.
        for point in ("parent-created", "child-created"):
            root = create_root(parent, point)
            kill_at_checkpoint(exe, point, root)
            before = sorted(str(p.relative_to(root)) for p in root.rglob("*"))
            check(exe, "recover", root, "failure=1")
            check(exe, "abandon", root, "failure=1")
            if before != sorted(str(p.relative_to(root)) for p in root.rglob("*")):
                raise RuntimeError("ambiguous preparation altered artifacts or admitted another workspace")
        root = create_root(parent, "live")
        kill_at_checkpoint(exe, "live", root, before_kill=lambda: check(exe, "recover", root, "recovered=0 live=1"))
        check(exe, "recover", root, "recovered=1 live=0")
        assert_empty(root)
        root = create_root(parent, "normal")
        check(exe, "normal", root, "cleaned")
        check(exe, "recover", root, "recovered=0 live=0")
        assert_empty(root)
        root = create_root(parent, "busy-cleanup")
        check(exe, "busy-cleanup", root, "cleaned")
        assert_empty(root)
        root = create_root(parent, "cancelled")
        check(exe, "cancelled", root, "failure=3")
        if (root / "registry").exists():
            raise RuntimeError("cancelled recovery created registry")
        # Partial append never authorizes an uncommitted transition.
        root = create_root(parent, "partial-tail")
        check(exe, "abandon", root, "prepared")
        with journal(root).open("ab") as stream:
            stream.write(b'{"Version":1,"Phase":4')
        check(exe, "recover", root, "recovered=1 live=0")
        assert_empty(root)
        # Substitution, symlink and journal corruption preserve the exact fixture.
        for kind in ("child", "parent", "neighbourhood", "journal", "missing-journal", "symlink", "marker", "phase", "scope", "authority", "missing-key", "trailing", "oversize", "unknown", "hardlink"):
            root = create_root(parent, "negative-" + kind)
            check(exe, "ready-abandon", root, "ready")
            record = journal(root)
            child = workspace(root)
            if kind in ("child", "parent", "neighbourhood", "symlink"):
                target = root / "neighbourhood" if kind == "neighbourhood" else child.parent if kind == "parent" else child
                target.rename(root / "original")
                if kind == "symlink":
                    target.symlink_to(root / "original", target_is_directory=True)
                else:
                    target.mkdir(mode=0o700)
                    (target / "unowned").write_text("preserve")
            elif kind == "missing-journal":
                record.rename(root / "original.jsonl")
            elif kind == "journal":
                data = record.read_bytes()
                record.rename(root / "original.jsonl")
                record.write_bytes(data)
                record.chmod(0o600)
            elif kind == "marker":
                (child / ".git" / "bpr-owned-workspace").write_text("untrusted")
            elif kind == "hardlink":
                (root / "journal-link").hardlink_to(record)
            elif kind == "trailing":
                record.write_text(record.read_text().replace("\n", " false\n", 1))
            elif kind == "oversize":
                with record.open("ab") as stream:
                    stream.write(b"x" * 65537)
            elif kind == "unknown":
                (root / "registry" / "unknown").write_text("preserve")
            else:
                records = [json.loads(line) for line in record.read_text().splitlines()]
                if kind == "phase":
                    records[-1]["Phase"] = 6
                elif kind == "scope":
                    records[-1]["Scope"][0] += 1
                elif kind == "authority":
                    records[-1]["AuthorityFingerprints"][0][0] += 1
                else:
                    del records[-1]["Parent"]
                record.write_text("".join(json.dumps(item) + "\n" for item in records))
            before = {str(p.relative_to(root)): (p.lstat().st_ino, p.read_bytes() if p.is_file() and not p.is_symlink() else None) for p in root.rglob("*")}
            check(exe, "recover", root, "failure=1")
            check(exe, "abandon", root, "failure=1")
            after = {str(p.relative_to(root)): (p.lstat().st_ino, p.read_bytes() if p.is_file() and not p.is_symlink() else None) for p in root.rglob("*")}
            if before != after:
                raise RuntimeError(f"{kind}: refusal altered original or substitute artifacts")
        # A replaced quarantine is no more authoritative than a replaced source.
        for point in ("child-quarantined", "parent-quarantined"):
            root = create_root(parent, "replaced-" + point)
            kill_at_checkpoint(exe, point, root)
            trash = workspace(root).parent / ".bpr-cleanup-workspace" if point == "child-quarantined" else root / "neighbourhood" / (".bpr-cleanup-" + journal(root).stem)
            trash.rename(root / "original")
            trash.mkdir(mode=0o700)
            (trash / "keep").write_text("unowned")
            check(exe, "recover", root, "failure=1")
            if (trash / "keep").read_text() != "unowned" or not (root / "original").is_dir():
                raise RuntimeError("replaced quarantine was removed")
        root = create_root(parent, "inventory-bound")
        for index in range(4097):
            (root / "neighbourhood" / f"unrelated-{index}").touch()
        check(exe, "abandon", root, "failure=2")
        if len(list((root / "neighbourhood").iterdir())) != 4097:
            raise RuntimeError("bounded inventory altered unrelated data")
        # Each live journal consumes one admission slot before any new mkdir.
        root = create_root(parent, "capacity")
        guards = []
        try:
            for _ in range(64):
                prior = set((root / "registry").glob("*.jsonl"))
                check(exe, "abandon", root, "prepared")
                added = set((root / "registry").glob("*.jsonl")) - prior
                if len(added) != 1:
                    raise RuntimeError("admission did not add exactly one journal")
                guard = added.pop().open("rb")
                fcntl.flock(guard, fcntl.LOCK_SH | fcntl.LOCK_NB)
                guards.append(guard)
            check(exe, "recover", root, "recovered=0 live=64")
            check(exe, "abandon", root, "failure=2")
            if len(list((root / "neighbourhood").iterdir())) != 64:
                raise RuntimeError("capacity refusal created another directory")
            guards.pop().close()
            check(exe, "normal", root, "cleaned")
            check(exe, "recover", root, "recovered=0 live=63")
        finally:
            for guard in guards:
                guard.close()
        check(exe, "recover", root, "recovered=63 live=0")
        assert_empty(root)
        # Recursive removal does not follow symlinks into an external checkout.
        root = create_root(parent, "external-symlink")
        check(exe, "abandon", root, "prepared")
        external = root / "user-checkout"
        external.mkdir(mode=0o700)
        (external / "keep").write_text("user data")
        (workspace(root) / "link").symlink_to(external, target_is_directory=True)
        check(exe, "recover", root, "recovered=1 live=0")
        if (external / "keep").read_text() != "user data":
            raise RuntimeError("recovery changed user checkout")
    print("AGENT_WORKSPACE_OWNERSHIP_OK")


if __name__ == "__main__":
    main()
