"""Actual processes; synthetic vault slots travel only in dedicated memory pipes."""
import argparse
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import tempfile


def run(application, phase, root, vault=b""):
    result = subprocess.run([application, phase, str(root)], input=vault,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=60, check=False)
    diagnostic = result.stderr.decode("utf-8", errors="replace")
    if result.returncode != 0 or f"profile-window-review-restoration:{phase}:pass" not in diagnostic:
        # stdout is vault material; never report it, including on process failure.
        raise SystemExit(f"{phase} failed ({result.returncode})\n{diagnostic}")
    if len(result.stdout) > 2048:
        raise SystemExit("fixture vault output exceeded bound")
    return result.stdout


def database(root):
    matches = list(root.rglob("window-restoration.sqlite3"))
    if len(matches) != 1:
        raise SystemExit("expected one profile-private restoration store")
    return matches[0]


def records(root):
    with sqlite3.connect(database(root)) as connection:
        return connection.execute("SELECT window_id,mode,hostname,owner,repository,number FROM restoration_records ORDER BY window_id").fetchall()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bpr-0709-restart-") as temporary:
        root = Path(temporary).resolve()
        os.chmod(root, 0o700)
        auth = root / "auth"
        auth.mkdir(mode=0o700)
        vault = run(arguments.application, "auth-seed", auth)
        initial = records(auth)
        assert [row[-1] for row in initial] == ["17", "18"], initial
        home = root / "home"
        shutil.copytree(auth, home)
        run(arguments.application, "auth-home", home, vault)
        assert records(home) == initial, "Home cancellation lost retained destination"
        thin = root / "thin"
        shutil.copytree(auth, thin)
        run(arguments.application, "auth-restore", thin, vault)
        stale = root / "stale"
        shutil.copytree(auth, stale)
        run(arguments.application, "auth-stale", stale, vault)
        assert [row[-1] for row in records(stale)] == ["18", "18"], "late restored read replaced user navigation"
        for phase in ("missing-grant", "missing-server", "corrupt", "future", "failed-read"):
            course = root / phase
            shutil.copytree(auth, course)
            if phase == "corrupt":
                database(course).write_bytes(b"not a sqlite database")
            elif phase != "missing-grant":
                with sqlite3.connect(database(course)) as connection:
                    if phase == "missing-server":
                        connection.execute("UPDATE restoration_records SET hostname='missing.example.com'")
                    elif phase == "future":
                        connection.execute("PRAGMA user_version=999")
                    else:
                        connection.execute("UPDATE restoration_records SET number='99'")
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
        run(arguments.application, "auth-verify", auth, vault)
        assert [(row[0], row[-1]) for row in records(auth)] == [("2", "18")], "fresh window invented destination"
        anonymous = root / "anonymous"
        anonymous.mkdir(mode=0o700)
        run(arguments.application, "anonymous-seed", anonymous)
        capability = root / "capability"
        shutil.copytree(anonymous, capability)
        with sqlite3.connect(database(capability)) as connection:
            connection.execute("UPDATE restoration_records SET hostname='ghe.example.invalid'")
        unavailable = records(capability)
        run(arguments.application, "capability", capability)
        assert records(capability) == unavailable, "unavailable capability lost destination"
        run(arguments.application, "anonymous-restore", anonymous)
    print("PROFILE_WINDOW_REVIEW_RESTORATION_OK")


if __name__ == "__main__":
    main()
