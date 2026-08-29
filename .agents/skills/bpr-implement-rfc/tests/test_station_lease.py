import json
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "scripts" / "station_lease.py"


def _git(repo: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-C", str(repo), *args],
        check=True,
        capture_output=True,
        env={
            "GIT_AUTHOR_NAME": "t",
            "GIT_AUTHOR_EMAIL": "t@t",
            "GIT_COMMITTER_NAME": "t",
            "GIT_COMMITTER_EMAIL": "t@t",
            "PATH": subprocess.os.environ["PATH"],
            "HOME": str(repo),
        },
    )


def _lease(repo: Path, *args: str) -> subprocess.CompletedProcess:
    # --repo must precede any "--" remainder or argparse hands it to the
    # wrapped command instead of this script.
    return subprocess.run(
        [sys.executable, str(SCRIPT), args[0], "--repo", str(repo), *args[1:]],
        capture_output=True,
        text=True,
    )


def _holding_process(repo: Path, role: str) -> subprocess.Popen:
    proc = subprocess.Popen(
        [
            sys.executable,
            str(SCRIPT),
            "run",
            "--role",
            role,
            "--repo",
            str(repo),
            "--station",
            "bpr-test-station",
            "--",
            sys.executable,
            "-c",
            "import sys, time; print('held', flush=True); time.sleep(30)",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert proc.stdout is not None
    line = proc.stdout.readline()
    if "held" not in line:
        proc.kill()
        raise AssertionError(f"holder did not start: {line!r}")
    return proc


class StationLeaseTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.repo = Path(self._tmp.name) / "repo"
        self.repo.mkdir()
        _git(self.repo, "init", "-b", "main")
        (self.repo / "seed.txt").write_text("seed\n")
        _git(self.repo, "add", "seed.txt")
        _git(self.repo, "commit", "-m", "seed")

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_status_reports_free_on_fresh_repo(self) -> None:
        result = _lease(self.repo, "status", "--role", "station-writer")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("free", result.stdout)

    def test_second_writer_is_denied_while_lease_is_held(self) -> None:
        holder = _holding_process(self.repo, "station-writer")
        try:
            status = _lease(self.repo, "status", "--role", "station-writer")
            self.assertEqual(status.returncode, 10, status.stdout + status.stderr)
            record = json.loads(status.stdout.split("\n", 1)[1])
            self.assertEqual(record["role"], "station-writer")
            self.assertEqual(record["station"], "bpr-test-station")

            second = _lease(
                self.repo, "run", "--role", "station-writer", "--", "true"
            )
            self.assertEqual(second.returncode, 75, second.stderr)
            self.assertIn("held", second.stderr)
        finally:
            holder.kill()
            holder.wait()

    def test_lease_releases_when_the_wrapped_process_exits(self) -> None:
        holder = _holding_process(self.repo, "station-writer")
        holder.kill()
        holder.wait()
        deadline = time.time() + 5
        while time.time() < deadline:
            if _lease(self.repo, "status", "--role", "station-writer").returncode == 0:
                return
            time.sleep(0.1)
        self.fail("lease was not released after the holder died")

    def test_station_writer_scope_is_per_worktree_but_integrator_is_shared(self) -> None:
        worktree = Path(self._tmp.name) / "wt"
        _git(self.repo, "worktree", "add", "-b", "lane", str(worktree))
        holder = _holding_process(self.repo, "station-writer")
        integrator = _holding_process(self.repo, "project-integrator")
        try:
            in_worktree = _lease(
                worktree, "run", "--role", "station-writer", "--", "true"
            )
            self.assertEqual(in_worktree.returncode, 0, in_worktree.stderr)

            integrator_from_worktree = _lease(
                worktree, "run", "--role", "project-integrator", "--", "true"
            )
            self.assertEqual(integrator_from_worktree.returncode, 75)
        finally:
            holder.kill()
            holder.wait()
            integrator.kill()
            integrator.wait()


if __name__ == "__main__":
    unittest.main()
