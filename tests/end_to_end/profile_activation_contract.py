"""Real desktop activation preserves owner identity and durable window routing."""
import argparse
import contextlib
import hashlib
import os
from pathlib import Path
import sqlite3
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import make_root_private  # noqa: E402
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

# [[BPR-0979]]. On Windows the rendezvous is a named pipe: it has no runtime
# directory, no filesystem entry and nothing to unlink, so the endpoint steps
# below are POSIX-only and the "unavailable" course (a removed endpoint while
# the owner lives) has no Windows equivalent. The routing, MRU, New Window,
# second-profile, owner death and closing-owner courses run on both.
POSIX = os.name == "posix"


@contextlib.contextmanager
def private_fixture_parent():
    """[[BPR-0847]]. The directory the course's data root is created in.

    An owner creates its agent workspace neighbourhood beside its data root, and
    only under a parent this user owns and nobody else can write. Otherwise
    workspace recovery refuses, and the orderly drain exits 1. The macOS runner's
    `BPR_TEST_SOCKET_ROOT` is such a directory. Linux's network sandbox sets
    none, and shared `/tmp` is not one, so the course makes its own there. `/tmp`
    keeps the data root short; since [[BPR-0881]] it no longer governs the
    rendezvous, which lives in a per-user runtime directory instead. On POSIX
    the contained support delegates to `tempfile.mkdtemp`, so the directory is
    still created mode 0700.
    """
    configured = os.environ.get("BPR_TEST_SOCKET_ROOT")
    if configured is not None:
        yield configured
        return
    if not POSIX:
        with contained_temporary_directory(prefix="bpr-activation-course-") as private:
            # [[BPR-0979]]. The contained support inherits %TEMP%'s entries on
            # purpose, and on a station whose %TEMP% grants another principal
            # Modify the neighbourhood beside the Default data root refuses
            # its parent, so the drain exits 1 naming repair-required. The
            # profile roots under `profiles/` never saw this: they inherit the
            # narrowed data root. Narrow the parent the way mode 0700 does.
            make_root_private(Path(private))
            yield private
        return
    with contained_temporary_directory(prefix="bpr-activation-course-", parent="/tmp") as private:
        yield private


def owner_private_directory(path, follow_links=True):
    """[[BPR-0881]]. Mirrors the product's own check on a runtime directory."""
    try:
        info = os.stat(path) if follow_links else os.lstat(path)
    except OSError:
        return False
    return (
        stat.S_ISDIR(info.st_mode)
        and info.st_uid == os.geteuid()
        and stat.S_IMODE(info.st_mode) == 0o700
    )


# The endpoint's name, `bpr-activate-<16 hex>.sock`, and Linux's `sun_path`,
# which the whole path and its terminator must fit.
ENDPOINT_NAME_SIZE = len("bpr-activate-") + 16 + len(".sock")
LINUX_SUN_PATH_SIZE = 108


def runtime_directory():
    """[[BPR-0881]]. The same candidate order `profile_activation.cpp` walks.

    On Linux an owner-private `$XDG_RUNTIME_DIR` first ([[BPR-0906]]), and it
    earns no child; one too long for the endpoint to fit is passed over
    ([[BPR-1113]]). Then Linux's `/run/user/<uid>`, then `$TMPDIR`, then `/tmp`;
    a candidate that is not itself owner-private earns one owner-private
    `bpr-<uid>` child, checked without following links so a planted symlink is
    refused.
    """
    identity = str(os.geteuid())
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if (
        sys.platform.startswith("linux")
        and runtime
        and runtime.startswith("/")
        and len(os.path.join(runtime, "n" * ENDPOINT_NAME_SIZE)) < LINUX_SUN_PATH_SIZE
        and owner_private_directory(runtime)
    ):
        return Path(runtime)
    configured = os.environ.get("TMPDIR")
    candidates = [Path("/run/user") / identity]
    if configured and configured.startswith("/"):
        candidates.append(Path(configured))
    candidates.append(Path("/tmp"))
    for candidate in candidates:
        if owner_private_directory(candidate):
            return candidate
        child = candidate / ("bpr-" + identity)
        try:
            child.mkdir(mode=0o700)
        except FileExistsError:
            pass
        except OSError:
            continue
        if owner_private_directory(child, follow_links=False):
            return child
    return None


def activation_endpoint(profile_root):
    """[[BPR-0881]]. The endpoint the product derives for this profile root.

    The digest is over the *lexically* normalised path, matching
    `endpoint_name`'s `lexically_normal()`: neither side resolves symlinks, so
    both sides must decline to resolve them.
    """
    directory = runtime_directory()
    assert directory is not None, "no owner-private per-user runtime directory"
    native = os.path.normpath(str(profile_root))
    digest = hashlib.sha256(os.fsencode(native)).hexdigest()[:16]
    return directory / f"bpr-activate-{digest}.sock"


def windows(root, profile):
    """The durable window rows of one profile, read through a closed connection.

    [[BPR-0980]]. `with sqlite3.connect(...)` commits or rolls back; it does not
    close. On Windows the course's own process then still holds the registry
    open when the contained temporary root is removed, and the row fails at
    cleanup with WinError 32 whenever garbage collection has not yet run.
    """
    with contextlib.closing(sqlite3.connect(root / "state/v1/profile-registry.sqlite3")) as connection:
        return connection.execute(
            "SELECT window_id,generation,ordinal FROM profile_registry_windows "
            "WHERE profile_id=? ORDER BY ordinal", (str(profile),)
        ).fetchall()


def tour_owner_course(tour_application, root, start, wait_marker, quit_owner):
    """[[BPR-1061]]. The shipped CLI reaches a real desktop owner through its tour channel."""
    import json
    owner = start("tour-owner", 1, "tourowner")
    wait_marker(owner, "OWNER_READY")
    seeded = [line for line in (root / "tourowner.log").read_text().splitlines() if line.startswith("TOUR_DRAFT ")]
    assert len(seeded) == 1, seeded
    _, draft, revision = seeded[0].split()
    def bpr(*arguments, data=None):
        return subprocess.run([tour_application, "tour", *arguments, "--profile", "1", "--draft", draft, "--json",
                               "--data-root", str(root)], input=data, capture_output=True, timeout=30)
    def field(result, name):
        return json.loads(result.stdout)["data"][name]
    status = bpr("status")
    assert status.returncode == 0 and field(status, "draft_id") == draft and field(status, "revision") == revision, status.stdout
    edited = bpr("edit", "problem", "--expected-revision", revision, "--request-id", "desktop-1", "--file", "-",
                 data="Forwarded to the real desktop α\n".encode())
    assert edited.returncode == 0, edited.stdout
    after = field(edited, "revision")
    assert after != revision
    assert json.loads(bpr("show", "problem").stdout)["data"]["text"] == "Forwarded to the real desktop α\n"
    duplicate = bpr("edit", "problem", "--expected-revision", revision, "--request-id", "desktop-1", "--file", "-",
                    data="Forwarded to the real desktop α\n".encode())
    assert duplicate.returncode == 0 and field(duplicate, "revision") == after, duplicate.stdout
    clash = bpr("edit", "problem", "--expected-revision", revision, "--request-id", "desktop-1", "--file", "-",
                data=b"another payload")
    assert clash.returncode != 0 and json.loads(clash.stdout)["error"]["code"] == "command_refused", clash.stdout
    recorded = bpr("status", "--request-id", "desktop-1")
    assert field(recorded, "operation_recorded") is True and field(recorded, "operation_revision") == after
    assert owner[0].poll() is None
    quit_owner(owner)
    # The owner drained: the committed text is durable and the next command runs standalone.
    standalone = bpr("show", "problem")
    assert standalone.returncode == 0 and json.loads(standalone.stdout)["data"]["text"] == "Forwarded to the real desktop α\n"
    print("PROFILE_TOUR_FORWARDING_OK")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--desktop-application", required=True)
    parser.add_argument("--tour-application")
    args = parser.parse_args()
    malformed = [
        ["--bpr-profile"], ["--bpr-profile", "0"],
        ["--bpr-profile", "-1"], ["--bpr-profile", "+1"],
        ["--bpr-profile", "1x"], ["--bpr-profile", " 1"],
        ["--bpr-profile", "18446744073709551616"],
        ["--bpr-profile", "1", "--bpr-profile", "2"],
        ["--new-window", "--new-window"], ["--new-window=yes"],
        ["--bpr-profile=1"], ["--bpr-profile", "--new-window"],
    ]
    for arguments in malformed:
        result = subprocess.run([args.desktop_application, *arguments],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode != 0 and "PROFILE_LAUNCH_ARGUMENTS_INVALID" in result.stdout, result
    for identity in ("1", "18446744073709551615"):
        result = subprocess.run([args.desktop_application, "--bpr-profile", identity,
                                 "--new-window", "--application-icon-probe"],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == 0 and "APPLICATION_ICON_OK" in result.stdout, result
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="act-", parent=parent) as temporary:
        root = Path(temporary).resolve()
        # [[BPR-0875]]. `os.chmod(root, 0o700)` says "only me" on POSIX; on
        # Windows the narrowing helper writes the one-grant descriptor the
        # profile registry admits.
        make_root_private(root)
        if POSIX:
            # The product's own test of the parent, so a shared one fails here by name.
            shared = root.parent.lstat()
            owner_only = (stat.S_ISDIR(shared.st_mode) and shared.st_uid == os.geteuid()
                          and shared.st_mode & (stat.S_IWGRP | stat.S_IWOTH) == 0)
            assert owner_only, f"{root.parent} is not owner-only, so an owner's workspace recovery would refuse"
        processes = []
        logs = []

        def start(mode, profile, name):
            log = (root / f"{name}.log").open("w+")
            logs.append(log)
            process = subprocess.Popen(
                [args.application, mode, str(root), str(profile), str(root / f"{name}.quit")],
                stdout=log, stderr=subprocess.STDOUT, text=True,
            )
            processes.append(process)
            return process, log, name

        def wait_marker(item, marker):
            process, log, _ = item
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                log.seek(0)
                output = log.read()
                if marker in output:
                    return
                if process.poll() is not None:
                    raise AssertionError(f"process exited {process.returncode}: {output}")
                time.sleep(0.02)
            raise AssertionError(f"missing {marker}: {output}")

        def quit_owner(item):
            process, log, name = item
            (root / f"{name}.quit").touch()
            code = process.wait(timeout=35)
            log.seek(0)
            output = log.read()
            assert code == 0 and "OWNER_RELEASED" in output, output

        try:
            subprocess.run([args.application, "seed", str(root), "1", str(root / "unused")], check=True, timeout=30)
            owner = start("owner", 1, "first")
            wait_marker(owner, "OWNER_READY")
            before = windows(root, 1)
            assert len(before) == 1
            sender = start("activate", 1, "activate")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            assert windows(root, 1) == before, "ordinary activation changed durable windows"
            sender = start("new", 1, "new")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            deadline = time.monotonic() + 30
            while len(windows(root, 1)) != 2 and time.monotonic() < deadline:
                time.sleep(0.02)
            assert len(windows(root, 1)) == 2, "owner did not admit new window"
            (root / "first.quit.focus").touch()
            wait_marker(owner, "MRU_FOCUSED")
            before = windows(root, 1)
            sender = start("activate", 1, "mru")
            assert sender[0].wait(timeout=35) == 0
            (root / "first.quit.check").touch()
            wait_marker(owner, "MRU_ACTIVATED")
            assert windows(root, 1) == before, "MRU activation changed windows"
            other = start("owner", 2, "other")
            wait_marker(other, "OWNER_READY")
            other_before = windows(root, 2)
            sender = start("activate", 2, "other-activate")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            assert windows(root, 2) == other_before
            assert windows(root, 1) == before, "other profile activation crossed identity"
            # [[BPR-0881]]. The rendezvous left the data root for a short
            # per-user runtime directory, so the course derives the default
            # owner's endpoint the way the product does instead of globbing the
            # root for it. A wrong derivation fails here naming both what it
            # expected and what is actually there, rather than silently finding
            # nothing to unlink.
            if POSIX:
                endpoint = activation_endpoint(root)
                assert endpoint.exists(), (
                    f"owner activation endpoint was not created at {endpoint}; "
                    f"the runtime directory holds "
                    f"{sorted(path.name for path in endpoint.parent.glob('bpr-activate-*.sock'))}"
                )
                endpoint.unlink()
                sender = start("unavailable", 1, "unavailable")
                assert sender[0].wait(timeout=35) == 0
                wait_marker(sender, "ACTIVATION_UNAVAILABLE")
                assert owner[0].poll() is None and windows(root, 1) == before
            quit_owner(other)
            quit_owner(owner)
            if args.tour_application and POSIX:
                tour_owner_course(args.tour_application, root, start, wait_marker, quit_owner)
            restarted = start("owner", 1, "restarted")
            wait_marker(restarted, "OWNER_READY")
            restarted[0].kill()
            restarted[0].wait(timeout=10)
            recovered = start("owner", 1, "recovered")
            wait_marker(recovered, "OWNER_READY")
            quit_owner(recovered)
            survivor = start("owner", 1, "survivor")
            wait_marker(survivor, "OWNER_READY")
            survivor_before = windows(root, 1)
            closing = start("closing", 2, "closing")
            wait_marker(closing, "CLOSING_READY")
            closing_before = windows(root, 2)
            sender = start("refused", 2, "closing-sender")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_REFUSED")
            assert windows(root, 2) == closing_before
            sender = start("activate", 1, "survivor-activate")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            assert windows(root, 1) == survivor_before
            quit_owner(closing)
            quit_owner(survivor)
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=10)
            for log in logs:
                log.close()
    print("PROFILE_ACTIVATION_OK")


if __name__ == "__main__":
    main()
