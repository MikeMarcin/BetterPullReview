"""Native journal crash/restart and refusal evidence, entirely fixture-local."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import stat
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


def current_user_sid() -> str:
    """Windows only: the current user's SID, from System32's whoami.

    Named by path: Git for Windows' usr\\bin, on PATH for perl, carries a POSIX
    whoami that has no /user.
    """
    whoami = Path(os.environ["SystemRoot"]) / "System32" / "whoami.exe"
    return subprocess.run([str(whoami), "/user", "/fo", "csv", "/nh"], capture_output=True, text=True,
                          check=True).stdout.strip().split(",")[1].strip('"')


def make_private(path: Path) -> None:
    """A fixture directory as private as the product creates them.

    POSIX: mode 0700. Windows ([[BPR-0622]]): a protected DACL with one
    full-access, inheritable grant for the current user; the directory is
    created by this user, so the user owns it.
    """
    if os.name == "posix":
        path.chmod(0o700)
        return
    # [[BPR-1869]]. By SID, not USERDOMAIN\USERNAME: an OpenSSH logon on a
    # workgroup machine has USERDOMAIN=WORKGROUP, which names no account, and
    # icacls refused the grant with 1332.
    user = "*" + current_user_sid()
    # Python's mkdir(mode=0o700) on Windows writes explicit SYSTEM,
    # Administrators and OWNER RIGHTS grants; a private workspace object has
    # only the user's.
    subprocess.run(["icacls", str(path), "/inheritance:r", "/grant:r", f"{user}:(OI)(CI)F",
                    "/remove:g", "*S-1-5-18", "*S-1-5-32-544", "*S-1-3-4"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def grant_everyone_object_ace(path: Path) -> None:
    """Windows only: the current user's full access plus an object ACE for Everyone.

    icacls cannot write an object ACE, so the protected DACL is set from SDDL.
    """
    import ctypes
    from ctypes import wintypes
    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    advapi32.ConvertStringSecurityDescriptorToSecurityDescriptorW.argtypes = [
        wintypes.LPCWSTR, wintypes.DWORD, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.ULONG)]
    advapi32.SetFileSecurityW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, ctypes.c_void_p]
    kernel32.LocalFree.argtypes = [ctypes.c_void_p]
    user = current_user_sid()
    sddl = f"D:P(A;OICI;FA;;;{user})(OA;OICI;FA;bf967aba-0de6-11d0-a285-00aa003049e2;;WD)"
    descriptor = ctypes.c_void_p()
    if not advapi32.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None):
        raise RuntimeError(f"object ACE descriptor ({ctypes.get_last_error()})")
    try:
        # DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION
        if not advapi32.SetFileSecurityW(str(path), 0x00000004 | 0x80000000, descriptor):
            raise RuntimeError(f"object ACE set ({ctypes.get_last_error()})")
    finally:
        kernel32.LocalFree(descriptor)


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


def mode(path: Path) -> int:
    return stat.S_IMODE(path.lstat().st_mode)


def check_neighbourhood_parent_rule(exe: Path, parent: Path) -> None:
    """BPR-0817: a missing neighbourhood is created only below a private parent.

    A parent that already exists and that group or other can write to is
    refused as `repair_required`, is not repaired, and gains no entry. Below a
    private parent the neighbourhood is created exactly 0700 even under the
    user-private-group umask 0002.
    """
    for name, shared_mode in (("shared-parent-0775", 0o775), ("shared-parent-1777", 0o1777)):
        root = parent / name
        root.mkdir(mode=0o700)
        root.chmod(shared_mode)
        check(exe, "recover", root, "failure=1")
        # [[BPR-1857]]. The refusal names its check and the folder it refused.
        check(exe, "recover-check", root, "failure=1 check=neighbourhood-parent-shared folder=.")
        if list(root.iterdir()):
            raise RuntimeError(f"{name}: refused recovery created an entry")
        if mode(root) != shared_mode:
            raise RuntimeError(f"{name}: refused recovery changed the parent's mode")
    root = parent / "private-parent-umask-0002"
    root.mkdir(mode=0o700)
    previous = os.umask(0o002)
    try:
        check(exe, "recover", root, "recovered=0 live=0")
    finally:
        os.umask(previous)
    neighbourhood = root / "neighbourhood"
    if not stat.S_ISDIR(neighbourhood.lstat().st_mode) or mode(neighbourhood) != 0o700:
        raise RuntimeError("the neighbourhood was not created as an exactly 0700 directory under umask 0002")
    if mode(root / "registry") != 0o700:
        raise RuntimeError("the registry was not created as an exactly 0700 directory under umask 0002")


def check_own_objects_narrowed(exe: Path, parent: Path) -> None:
    """BPR-1870: the application's own workspace objects are narrowed, not refused.

    A neighbourhood, registry or record of this user's that is wider than
    private is set to 0700 or 0600 in place and recovery goes on, naming the
    check it repaired. A neighbourhood that is a link is still refused and its
    target keeps its mode; the folder that holds the neighbourhood is never
    changed (check_neighbourhood_parent_rule).
    """
    root = create_root(parent, "narrow-neighbourhood")
    (root / "neighbourhood").chmod(0o755)
    check(exe, "recover-check", root, "recovered=0 live=0 repaired=neighbourhood-not-private")
    if mode(root / "neighbourhood") != 0o700 or mode(root) != 0o700:
        raise RuntimeError("the neighbourhood was not narrowed to 0700, or its parent was changed")
    root = create_root(parent, "narrow-registry")
    (root / "registry").mkdir(mode=0o700)
    (root / "registry").chmod(0o755)
    check(exe, "recover-check", root, "recovered=0 live=0 repaired=registry-not-private")
    if mode(root / "registry") != 0o700:
        raise RuntimeError("the registry was not narrowed to 0700")
    root = create_root(parent, "narrow-record")
    check(exe, "ready-abandon", root, "ready")
    record = journal(root)
    record.chmod(0o644)
    check(exe, "recover-check", root, "recovered=1 live=0 repaired=record-not-private")
    assert_empty(root)
    root = create_root(parent, "linked-neighbourhood")
    target = root / "elsewhere"
    target.mkdir(mode=0o700)
    target.chmod(0o755)
    (root / "neighbourhood").rmdir()
    (root / "neighbourhood").symlink_to(target, target_is_directory=True)
    # The reported folder is relative to the root through the link, so only
    # the check is compared.
    linked = run(exe, "recover-check", root).strip()
    if not linked.startswith("failure=1 check=neighbourhood-not-private folder=") or "repaired=" in linked:
        raise RuntimeError(f"a linked neighbourhood was not refused as is: {linked!r}")
    if mode(target) != 0o755:
        raise RuntimeError("a linked neighbourhood's target was changed")


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


IDENTITY_FIELDS = ("Registry", "Neighbourhood", "File", "Parent", "Child")


def lines_of(record: Path) -> list[dict]:
    return [json.loads(line) for line in record.read_text().splitlines()]


def as_version_one(lines: list[dict], volume_of) -> list[dict]:
    """[[BPR-2118]]. The record as the previous release wrote it.

    Version 1 stored the mount's number where Version 2 stores the volume's
    UUID. On macOS each identity becomes `[kind, volume_of(field), 0, inode]`,
    the number a later mount may differ from; elsewhere both versions hold the
    same values, and only the version changes.
    """
    legacy = []
    for line in lines:
        line = dict(line, Version=1)
        if sys.platform == "darwin":
            for field in IDENTITY_FIELDS:
                identity = line[field]
                if any(identity):
                    line[field] = [identity[0], volume_of(field), 0, identity[3]]
        legacy.append(line)
    return legacy


def write_lines(record: Path, lines: list[dict]) -> None:
    # Glaze writes compact JSON; so does this, so a rewritten record is
    # byte-comparable with one the product wrote. Bytes, so Windows writes LF;
    # writing in place keeps the record's inode and descriptor.
    record.write_bytes("".join(json.dumps(item, separators=(",", ":")) + "\n" for item in lines).encode())


def check_remount_and_record_refusals(exe: Path, parent: Path) -> None:
    """[[BPR-2118]]. A remount is not a moved record; real mismatches are counted.

    The fixture stands in for a Mac restart that renumbered the Data volume by
    writing a retained record back as the previous release's Version 1, with
    every identity on one mount number that is not the live one, as the
    owner's `0x100000D` became `0x1000011`. Recovery keeps it live and
    rewrites it in place as the exact Version 2 record, inode and all. Two
    mount numbers in one record, or a changed inode, are still refused as
    `record-moved`, and a refusal counts every refused record and names the
    first.
    """
    root = create_root(parent, "remount-retained")
    check(exe, "retained-abandon", root, "retained")
    record = journal(root)
    mount = record.stat().st_dev + 4
    if mount in {os.stat(path).st_dev for path in (root / "registry", root / "neighbourhood", record)}:
        raise RuntimeError("the stand-in mount number is a live one")
    current = record.read_bytes()
    lines = lines_of(record)
    if any(line["Version"] != 2 for line in lines) or lines[-1]["Phase"] != 7:
        raise RuntimeError("a new retained record is not written as Version 2 at phase 7")
    if sys.platform == "darwin" and not all(line["File"][2] != 0 for line in lines):
        raise RuntimeError("a macOS record does not store the durable volume identity")
    inode = record.stat().st_ino
    write_lines(record, as_version_one(lines, lambda _field: mount))
    check(exe, "recover-check", root, "recovered=0 live=1")
    if record.read_bytes() != current or record.stat().st_ino != inode:
        raise RuntimeError("a remounted Version 1 record was not rewritten in place as the exact Version 2 record")
    # A Version 2 record round-trips unchanged.
    check(exe, "recover-check", root, "recovered=0 live=1")
    if record.read_bytes() != current or record.stat().st_ino != inode:
        raise RuntimeError("recovery changed a current Version 2 record")

    root = create_root(parent, "remount-two-volumes")
    check(exe, "retained-abandon", root, "retained")
    record = journal(root)
    lines = lines_of(record)
    if sys.platform == "darwin":
        legacy = as_version_one(lines, lambda field: mount + 4 if field == "Child" else mount)
    else:
        legacy = as_version_one(lines, None)
        legacy[-1]["Registry"] = [legacy[-1]["Registry"][0], legacy[-1]["Registry"][1] + 4,
                                  legacy[-1]["Registry"][2], legacy[-1]["Registry"][3]]
    write_lines(record, legacy)
    before = record.read_bytes()
    check(exe, "recover-check", root,
          f"failure=1 check=record-moved folder=registry/{record.name} records=1 finding=other-volume")
    if record.read_bytes() != before:
        raise RuntimeError("a refused record was rewritten")

    root = create_root(parent, "remount-inode")
    check(exe, "retained-abandon", root, "retained")
    record = journal(root)
    legacy = as_version_one(lines_of(record), lambda _field: mount)
    for line in legacy:
        line["Registry"] = [line["Registry"][0], line["Registry"][1], line["Registry"][2], line["Registry"][3] + 1]
    write_lines(record, legacy)
    before = record.read_bytes()
    check(exe, "recover-check", root,
          f"failure=1 check=record-moved folder=registry/{record.name} records=1 finding=replaced")
    if record.read_bytes() != before:
        raise RuntimeError("a refused record was rewritten")

    # Every record is checked before the refusal, which counts them all and
    # names the first one the scan reached.
    root = create_root(parent, "three-moved")
    # Retained, so each later preparation keeps the earlier records live.
    for _ in range(3):
        check(exe, "retained-abandon", root, "retained")
    records = sorted((root / "registry").glob("*.jsonl"))
    if len(records) != 3:
        raise RuntimeError("three retained records were not all kept")
    for record in records:
        lines = lines_of(record)
        for line in lines:
            line["Neighbourhood"] = [line["Neighbourhood"][0], line["Neighbourhood"][1], line["Neighbourhood"][2],
                                     line["Neighbourhood"][3] + 1]
        write_lines(record, lines)
    result = run(exe, "recover-check", root).strip()
    named = {f"failure=1 check=record-moved folder=registry/{record.name} records=3 finding=replaced" for record in records}
    if result not in named:
        raise RuntimeError(f"three refused records were not counted and the first named: {result!r}")
    if sorted((root / "registry").glob("*.jsonl")) != records:
        raise RuntimeError("a refusal removed a record")

    # A malformed record is its own check.
    root = create_root(parent, "malformed")
    check(exe, "ready-abandon", root, "ready")
    record = journal(root)
    record.write_bytes(record.read_bytes().replace(b'"Phase":0', b'"Phase":"zero"', 1))
    check(exe, "recover-check", root, f"failure=1 check=record-malformed folder=registry/{record.name} records=1")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    exe = args.executable.resolve(strict=True)
    with contained_temporary_directory(prefix="bpr-workspace-ownership-") as temporary:
        parent = Path(temporary).resolve()
        if os.name == "posix":
            # BPR-0817's mode-bit parent rule. Windows proves its own
            # untrusted-parent refusal through the DACL courses (BPR-0622).
            check_neighbourhood_parent_rule(exe, parent)
            check_own_objects_narrowed(exe, parent)
        check_remount_and_record_refusals(exe, parent)
        # [[BPR-1857]]. A workspace no record accounts for is refused by name,
        # and kept exactly as it was.
        root = create_root(parent, "unrecorded-leftover")
        check(exe, "recover", root, "recovered=0 live=0")
        leftover = root / "neighbourhood" / ("betterpullreview-agent-" + "0" * 32)
        leftover.mkdir(mode=0o700)
        make_private(leftover)
        (leftover / "keep").write_text("unowned")
        check(exe, "recover-check", root,
              "failure=1 check=unrecorded-leftover folder=neighbourhood/betterpullreview-agent-" + "0" * 32)
        if (leftover / "keep").read_text() != "unowned":
            raise RuntimeError("an unrecorded leftover was altered by its refusal")
        if os.name != "posix":
            # [[BPR-1857]]. An inherit-only entry grants nothing on the parent
            # itself, so it does not refuse the neighbourhood; the same grant
            # applying to the parent still does, and names the account.
            #
            # The split phases keep the registry in a private `data` folder, as
            # the product does, so the holding folder is judged only as the
            # neighbourhood's parent. In the plain layout that folder is also
            # the registry's data folder, whose exact descriptor the extra
            # entry rightly breaks, and the refusal says so.
            root = parent / "inherit-only-parent"
            root.mkdir()
            make_private(root)
            subprocess.run(["icacls", str(root), "/grant", "*S-1-1-0:(OI)(CI)(IO)(M)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check(exe, "recover-check", root, "failure=1 check=data-folder-not-private folder=.")
            (root / "data").mkdir()
            make_private(root / "data")
            check(exe, "recover-split", root, "recovered=0 live=0")
            root = parent / "shared-parent-named"
            root.mkdir()
            make_private(root)
            subprocess.run(["icacls", str(root), "/grant", "*S-1-1-0:(OI)(CI)(M)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            (root / "data").mkdir()
            make_private(root / "data")
            check(exe, "recover-check-split", root, "failure=1 check=neighbourhood-parent-shared folder=.")
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
        if os.name != "posix":
            # BPR-0825: the data root the SQLite adapter creates is admitted by
            # the Git adapter's workspace registry, through the one
            # owner-private descriptor both now take from the platform target.
            root = create_root(parent, "sqlite-root")
            check(exe, "sqlite-root", root, "cleaned")
            if list((root / "sqlite-neighbourhood").iterdir()) or list(
                    (root / "data" / "agent-workspace-ownership-v1").glob("*.jsonl")):
                raise RuntimeError("sqlite-root cleanup retained owned state")
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
            # [[BPR-0622]]. Windows privacy is the descriptor, not the mode.
            # [[BPR-1870]]. A journal of this user's readable by Everyone is
            # given the exact owner-private descriptor in place, and recovery
            # goes on and names the repair.
            root = create_root(parent, "negative-acl")
            check(exe, "ready-abandon", root, "ready")
            record = journal(root)
            subprocess.run(["icacls", str(record), "/grant", "*S-1-1-0:(R)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check(exe, "recover-check", root, "recovered=1 live=0 repaired=record-not-private")
            assert_empty(root)
            # The same for the application's own folders: a neighbourhood and
            # a registry of this user's that Everyone can read are given the
            # exact descriptor through their handles.
            root = create_root(parent, "negative-acl-neighbourhood")
            subprocess.run(["icacls", str(root / "neighbourhood"), "/grant", "*S-1-1-0:(OI)(CI)(R)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check(exe, "recover-check", root, "recovered=0 live=0 repaired=neighbourhood-not-private")
            check(exe, "recover-check", root, "recovered=0 live=0")
            root = create_root(parent, "negative-acl-registry")
            (root / "registry").mkdir()
            make_private(root / "registry")
            subprocess.run(["icacls", str(root / "registry"), "/grant", "*S-1-1-0:(OI)(CI)(R)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check(exe, "recover-check", root, "recovered=0 live=0 repaired=registry-not-private")
            check(exe, "recover-check", root, "recovered=0 live=0")
            # A neighbourhood is never created under a parent another principal can write.
            root = parent / "untrusted-parent"
            root.mkdir()
            make_private(root)
            subprocess.run(["icacls", str(root), "/grant", "*S-1-1-0:(OI)(CI)(M)"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            check(exe, "abandon", root, "failure=1")
            if (root / "neighbourhood").exists():
                raise RuntimeError("neighbourhood created under a foreign-writable parent")
            # [[BPR-0825]]. An allow entry that is not a plain ACCESS_ALLOWED_ACE
            # (here an object ACE granting Everyone full access) is refused too.
            root = parent / "untrusted-object-ace"
            root.mkdir()
            make_private(root)
            grant_everyone_object_ace(root)
            check(exe, "abandon", root, "failure=1")
            if (root / "neighbourhood").exists():
                raise RuntimeError("neighbourhood created under a parent with a foreign object ACE")
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
        if os.name != "posix":
            # [[BPR-0825]]. A directory junction, which needs no privilege, inside
            # the workspace being removed. Recovery must leave the external
            # directory exactly as it was, and either remove the junction itself
            # or refuse with the junction still in place.
            import _winapi
            root = create_root(parent, "external-junction")
            check(exe, "abandon", root, "prepared")
            external = root / "user-checkout"
            external.mkdir(mode=0o700)
            (external / "keep").write_text("user data")
            (external / "nested").mkdir()
            (external / "nested" / "deep").write_text("deep user data")
            owned = workspace(root)
            junction = owned / "junction"
            _winapi.CreateJunction(str(external), str(junction))
            before = sorted((str(p.relative_to(external)), p.read_bytes() if p.is_file() else None)
                            for p in external.rglob("*"))
            result = run(exe, "recover", root).strip()
            after = sorted((str(p.relative_to(external)), p.read_bytes() if p.is_file() else None)
                           for p in external.rglob("*"))
            if after != before:
                raise RuntimeError("recovery changed the directory a junction pointed at")
            if result == "recovered=1 live=0":
                if os.path.lexists(junction) or owned.exists():
                    raise RuntimeError("recovery reported removal but left the junction")
            elif result == "failure=1":
                if not os.path.lexists(junction):
                    raise RuntimeError("recovery refused but removed the junction")
            else:
                raise RuntimeError(f"junction recovery: unexpected result {result!r}")
            print(f"agent-workspace-ownership:external-junction:{result}", flush=True)
    print("AGENT_WORKSPACE_OWNERSHIP_OK")


if __name__ == "__main__":
    main()
