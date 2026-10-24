"""Actual processes; synthetic vault slots travel only in dedicated memory pipes."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bpr_narrow_owner_private import copy_private_root, make_root_private  # noqa: E402
from bpr_store_connection import store_connection  # noqa: E402
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def run(application, phase, root, vault=b""):
    generation = (1 if phase.endswith("seed") else
                  2 if phase == "auth-advance" or phase.startswith("anonymous") or phase == "capability" else
                  4 if phase in {"tabs-restore", "auth-close"} else 5 if phase == "auth-verify" else 3)
    result = subprocess.run([application, phase, str(root), str(generation)], input=vault,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=180, check=False)
    diagnostic = result.stderr.decode("utf-8", errors="replace")
    if result.returncode != 0 or f"profile-window-review-restoration:{phase}:pass" not in diagnostic:
        # stdout is vault material; never report it, including on process failure.
        raise SystemExit(f"{phase} failed ({result.returncode})\n{diagnostic}")
    # [[BPR-1050]]. Standard error is the phase's diagnostic, never vault
    # material, so a passing phase relays it too. A line the application prints
    # while the phase passes, such as AGENT_WORKSPACE_RECOVERY_REFUSED, then
    # reaches CTest and the error expressions every row carries.
    sys.stderr.write(diagnostic)
    if len(result.stdout) > 2048:
        raise SystemExit("fixture vault output exceeded bound")
    return result.stdout


def seed_auth(application, root):
    vault = run(application, "auth-seed", root)
    return run(application, "auth-advance", root, vault)


def database(root):
    matches = list(root.rglob("window-restoration.sqlite3"))
    if len(matches) != 1:
        raise SystemExit("expected one profile-private restoration store")
    return matches[0]


def records(root):
    with store_connection(database(root)) as connection:
        return connection.execute("SELECT window_id,mode,hostname,owner,repository,number FROM restoration_records ORDER BY window_id").fetchall()


def positions(root):
    with store_connection(database(root)) as connection:
        return connection.execute("SELECT window_id,position FROM restoration_records ORDER BY window_id").fetchall()


def recent(root):
    """[[BPR-0733]]. The per-profile recently viewed list, most recent first."""
    with store_connection(database(root)) as connection:
        return connection.execute("SELECT ordinal,number FROM recent_reviews ORDER BY ordinal").fetchall()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--only-orderly-close-control", action="store_true")
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-0709-restart-") as temporary:
        root = Path(temporary).resolve()
        # [[BPR-1050]]. Each phase's data root is a child of this directory, and
        # the agent workspace neighbourhood beside that root trusts only a
        # private parent. On Windows `os.chmod` does not narrow it, and the
        # application refused it; `make_root_private` writes the descriptor
        # the stores define there.
        os.chmod(root, 0o700)
        make_root_private(root)
        orderly = root / "orderly-close"
        make_root_private(orderly)
        orderly_vault = seed_auth(arguments.application, orderly)
        run(arguments.application, "auth-orderly-close-race", orderly, orderly_vault)
        if arguments.only_orderly_close_control:
            print("PROFILE_ORDERLY_CLOSE_CONTROL_OK")
            return
        early = root / "early-tab-reading"
        make_root_private(early)
        run(arguments.application, "auth-tab-gesture-seed", early)
        auth = root / "auth"
        make_root_private(auth)
        vault = seed_auth(arguments.application, auth)
        initial = records(auth)
        assert [row[-1] for row in initial] == ["17", "18"], initial
        assert recent(auth) == [(0, "18"), (1, "17")], recent(auth)
        initial_positions = positions(auth)
        assert len(initial_positions) == 2 and all(row[1] for row in initial_positions)
        assert initial_positions[0][1] != initial_positions[1][1], "independent readings collapsed"
        home = root / "home"
        copy_private_root(auth, home)
        run(arguments.application, "auth-home", home, vault)
        assert records(home) == initial, "Home cancellation lost retained destination"
        assert positions(home) == initial_positions, "Home cancellation lost accepted reading"
        for phase in ("auth-reading-gesture", "auth-reading-unchanged", "auth-reading-home"):
            course = root / phase
            make_root_private(course)
            course_vault = seed_auth(arguments.application, course)
            run(arguments.application, phase, course, course_vault)
        for phase in ("unavailable-anchor", "unavailable-file", "unavailable-semantic"):
            course = root / phase
            make_root_private(course)
            course_vault = seed_auth(arguments.application, course)
            course_positions = positions(course)
            run(arguments.application, phase, course, course_vault)
            assert records(course) == initial, "unavailable reading lost destination"
            assert positions(course)[1] == course_positions[1], "failure changed sibling reading"
        thin = root / "thin"
        copy_private_root(auth, thin)
        # Cache manifests bind the original directory identity. A copied cache
        # is deliberately unavailable, and is not positive restoration evidence.
        run(arguments.application, "unavailable-content", thin, vault)
        assert positions(thin) == initial_positions, "unavailable cache changed reading"
        stale = root / "stale"
        copy_private_root(auth, stale)
        run(arguments.application, "auth-stale", stale, vault)
        assert [row[-1] for row in records(stale)] == ["18", "18"], "late restored read replaced user navigation"
        for phase in ("missing-grant", "missing-server", "corrupt", "future", "failed-read"):
            course = root / phase
            copy_private_root(auth, course)
            if phase == "corrupt":
                database(course).write_bytes(b"not a sqlite database")
            elif phase != "missing-grant":
                with store_connection(database(course)) as connection:
                    if phase == "missing-server":
                        connection.execute("UPDATE restoration_records SET hostname='missing.example.com'")
                    elif phase == "future":
                        connection.execute("PRAGMA user_version=999")
                    else:
                        connection.execute("UPDATE restoration_records SET number='99'")
                        # A gesture on the list is what opens a review now, so
                        # the newest listed entry is the read that must fail.
                        connection.execute("UPDATE recent_reviews SET number='99' WHERE ordinal=0")
            before = database(course).read_bytes()
            expected = None if phase in ("corrupt", "future") else records(course)
            run(arguments.application, phase, course, b"" if phase == "missing-grant" else vault)
            if phase in ("corrupt", "future"):
                assert database(course).read_bytes() == before, "optional store failure rewrote data"
            else:
                assert records(course) == expected, "failed restoration lost retained destinations"
        vault = run(arguments.application, "auth-restore", auth, vault)
        assert [row[-1] for row in records(auth)] == ["18", "18"], "failed navigation replaced latest accepted record"
        vault = run(arguments.application, "auth-close", auth, vault)
        assert [(row[0], row[-1]) for row in records(auth)] == [("2", "18")], "Close erased wrong destination"
        # Closing a window retires its retained destination; the profile's list
        # of what was read keeps both entries.
        assert recent(auth) == [(0, "18"), (1, "17")], recent(auth)
        run(arguments.application, "auth-verify", auth, vault)
        assert [(row[0], row[-1]) for row in records(auth)] == [("2", "18")], "fresh window invented destination"
        anonymous = root / "anonymous"
        make_root_private(anonymous)
        run(arguments.application, "anonymous-seed", anonymous)
        capability = root / "capability"
        copy_private_root(anonymous, capability)
        with store_connection(database(capability)) as connection:
            connection.execute("UPDATE restoration_records SET hostname='ghe.example.invalid'")
        unavailable = records(capability)
        run(arguments.application, "capability", capability)
        assert records(capability) == unavailable, "unavailable capability lost destination"
        run(arguments.application, "anonymous-restore", anonymous)
    print("PROFILE_WINDOW_REVIEW_RESTORATION_OK")


if __name__ == "__main__":
    main()
