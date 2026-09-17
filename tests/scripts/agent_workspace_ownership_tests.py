"""Native journal crash/restart and refusal evidence, entirely fixture-local."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

if os.name == "posix":
    import fcntl
else:
    import ctypes
    from ctypes import wintypes

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

from process_kill_support import kill_at_checkpoint, run


def make_private(path: Path) -> None:
    """A fixture directory as private as the product creates them.

    POSIX: mode 0700. Windows ([[BPR-0622]]): a protected DACL with one
    full-access, inheritable grant for the current user; the directory is
    created by this user, so the user owns it.
    """
    if os.name == "posix":
        path.chmod(0o700)
        return
    user = os.environ["USERDOMAIN"] + "\\" + os.environ["USERNAME"]
    # Python's mkdir(mode=0o700) on Windows writes explicit SYSTEM,
    # Administrators and OWNER RIGHTS grants; a private workspace object has
    # only the user's.
    subprocess.run(["icacls", str(path), "/inheritance:r", "/grant:r", f"{user}:(OI)(CI)F",
                    "/remove:g", "*S-1-5-18", "*S-1-5-32-544", "*S-1-3-4"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def create_root(parent: Path, name: str) -> Path:
    root = parent / name
    root.mkdir(mode=0o700)
    make_private(root)
    (root / "neighbourhood").mkdir(mode=0o700)
    make_private(root / "neighbourhood")
    return root


if os.name != "posix":
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    GUARD_LOCK_OFFSET = 0x4000_0000_0000_0000

    class OVERLAPPED(ctypes.Structure):
        _fields_ = [("Internal", ctypes.c_void_p), ("InternalHigh", ctypes.c_void_p),
                    ("Offset", wintypes.DWORD), ("OffsetHigh", wintypes.DWORD), ("hEvent", wintypes.HANDLE)]

    kernel32.CreateFileW.restype = wintypes.HANDLE
    kernel32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                     wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel32.LockFileEx.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                                    wintypes.DWORD, ctypes.POINTER(OVERLAPPED)]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]

    class SharedGuard:
        """An independent live owner: this process's own shared guard lock."""

        def __init__(self, path: Path) -> None:
            self.handle = kernel32.CreateFileW(str(path), 0x80000000, 0x1 | 0x2 | 0x4, None, 3, 0x00200000, None)
            if self.handle in (None, wintypes.HANDLE(-1).value):
                raise RuntimeError(f"cannot open journal guard ({ctypes.get_last_error()})")
            overlapped = OVERLAPPED()
            overlapped.Offset = GUARD_LOCK_OFFSET & 0xFFFFFFFF
            overlapped.OffsetHigh = GUARD_LOCK_OFFSET >> 32
            if not kernel32.LockFileEx(self.handle, 0x1, 0, 1, 0, ctypes.byref(overlapped)):
                kernel32.CloseHandle(self.handle)
                raise RuntimeError(f"cannot take shared journal guard ({ctypes.get_last_error()})")

        def close(self) -> None:
            if self.handle:
                kernel32.CloseHandle(self.handle)
                self.handle = None


def hold_shared(path: Path):
    if os.name == "posix":
        guard = path.open("rb")
        fcntl.flock(guard, fcntl.LOCK_SH | fcntl.LOCK_NB)
        return guard
    return SharedGuard(path)


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
    with contained_temporary_directory(prefix="bpr-workspace-ownership-") as temporary:
        parent = Path(temporary).resolve()
        # Real forced death (SIGKILL, TerminateProcess) between committed native
        # phases, including both renames.
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
                    make_private(target)
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
        if os.name != "posix":
            # [[BPR-0622]]. Windows privacy is the descriptor, not the mode: a
            # journal readable by Everyone is refused and left exactly as found.
            root = create_root(parent, "negative-acl")
            check(exe, "ready-abandon", root, "ready")
            record = journal(root)
            subprocess.run(["icacls", str(record), "/grant", "*S-1-1-0:(R)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            before = {str(p.relative_to(root)): (p.lstat().st_ino, p.read_bytes() if p.is_file() and not p.is_symlink() else None) for p in root.rglob("*")}
            check(exe, "recover", root, "failure=1")
            check(exe, "abandon", root, "failure=1")
            after = {str(p.relative_to(root)): (p.lstat().st_ino, p.read_bytes() if p.is_file() and not p.is_symlink() else None) for p in root.rglob("*")}
            if before != after:
                raise RuntimeError("acl: refusal altered original artifacts")
            # A neighbourhood is never created under a parent another principal can write.
            root = parent / "untrusted-parent"
            root.mkdir()
            make_private(root)
            subprocess.run(["icacls", str(root), "/grant", "*S-1-1-0:(OI)(CI)(M)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check(exe, "abandon", root, "failure=1")
            if (root / "neighbourhood").exists():
                raise RuntimeError("neighbourhood created under a foreign-writable parent")
        # A replaced quarantine is no more authoritative than a replaced source.
        for point in ("child-quarantined", "parent-quarantined"):
            root = create_root(parent, "replaced-" + point)
            kill_at_checkpoint(exe, point, root)
            trash = workspace(root).parent / ".bpr-cleanup-workspace" if point == "child-quarantined" else root / "neighbourhood" / (".bpr-cleanup-" + journal(root).stem)
            trash.rename(root / "original")
            trash.mkdir(mode=0o700)
            make_private(trash)
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
                guards.append(hold_shared(added.pop()))
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
