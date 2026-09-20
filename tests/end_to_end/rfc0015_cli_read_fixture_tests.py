#!/usr/bin/env python3
"""Verify external fixture framing, split routing, refusal, and object identity."""

import json
import os
import secrets
import shutil
import sys
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch
from contextlib import redirect_stderr
import io

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

import rfc0015_cli_read_fixture as fixture  # noqa: E402


class cli_read_fixture_tests(unittest.TestCase):
    def test_denied_optional_trace_preserves_empty_stderr(self):
        output = io.StringIO()
        with patch.object(Path, "open", side_effect=PermissionError), redirect_stderr(output):
            fixture.record_operation(Path("/unused"), "version")
        self.assertEqual(output.getvalue(), "")

    def test_closed_local_clone_and_canary(self):
        with contained_temporary_directory(prefix="bpr-cli-fixture-") as temporary:
            directory = Path(temporary).resolve() / "positive"
            fixture.prepare(directory, "positive")
            state = json.loads((directory / "fixture.json").read_text())
            root = (Path("/private/tmp") if sys.platform == "darwin" else Path("/tmp")) / ("bpr-cli-git-" + secrets.token_hex(8))
            root.mkdir(mode=0o700)
            self.addCleanup(shutil.rmtree, root)
            (root / "empty").mkdir()
            canary = secrets.token_hex(32)
            environment = dict(os.environ, GH_TOKEN=canary)
            outputs = []
            for index, oid in enumerate((state["base"], state["head"])):
                destination = root / f"clone-{index}.git"
                args = [str(directory / "bin/gh"), "repo", "clone",
                        "https://github.com/octo-org/review-repository.git",
                        str(destination), "--no-upstream", "--", "--bare", "--quiet",
                        "--revision=" + oid, "--template=" + str(root / "empty"),
                        "--config", "transfer.unpackLimit=0", "--config", "fetch.unpackLimit=0",
                        "--config", "core.hooksPath=/dev/null", "--config", "core.fsmonitor=false"]
                for position, replacement in ((3, "https://other.invalid/repo.git"),
                                               (9, "--revision=" + "0" * 40),
                                               (4, str(root / "escape.git")),
                                               (18, "core.fsmonitor=true")):
                    invalid = args.copy()
                    invalid[position] = replacement
                    result = subprocess.run(invalid, env=environment, capture_output=True, timeout=5)
                    self.assertEqual(result.returncode, 64)
                    self.assertFalse(destination.exists())
                    outputs.append(result.stdout + result.stderr)
                result = subprocess.run(args, env=environment, capture_output=True, timeout=15)
                self.assertEqual(result.returncode, 0)
                outputs.append(result.stdout + result.stderr)
                head = subprocess.check_output(["/usr/bin/git", "--git-dir", str(destination),
                                                "rev-parse", "HEAD"]).decode().strip()
                self.assertEqual(head, oid)
                subprocess.run(["/usr/bin/git", "--git-dir", str(destination),
                                "cat-file", "-e", oid + ":review.cpp"], check=True)
                self.assertEqual(subprocess.run(args, capture_output=True, timeout=5).returncode, 64)
            needle = canary.encode()
            for output in outputs:
                self.assertNotIn(needle, output)
            for tree in (directory, root):
                for path in tree.rglob("*"):
                    if path.is_file():
                        self.assertNotIn(needle, path.read_bytes())
            self.assertNotIn(needle, Path(fixture.__file__).read_bytes())

    def test_courses_and_exact_objects(self):
        with contained_temporary_directory(prefix="bpr-cli-fixture-") as temporary:
            for course in ("positive", "missing-process", "process-failure", "malformed-response"):
                directory = Path(temporary) / course
                fixture.prepare(directory, course)
                if course == "missing-process":
                    self.assertFalse((directory / "bin/gh").exists())
                    continue
                command = [str(directory / "bin/gh"), "api", "user", "--hostname",
                           "github.com", "--include", "--method", "GET"]
                version = subprocess.run([str(directory / "bin/gh"), "--version"],
                                         capture_output=True, timeout=5)
                self.assertEqual(version.returncode, 0)
                self.assertEqual(version.stdout,
                                 b"gh version 2.100.0 (synthetic local fixture)\n")
                discovery = subprocess.run([str(directory / "bin/gh"), "auth",
                                            "status", "--active", "--json", "hosts"],
                                           capture_output=True, timeout=5)
                self.assertEqual(discovery.returncode, 0)
                self.assertEqual(discovery.stderr, b"")
                self.assertEqual(json.loads(discovery.stdout), {"hosts": {
                    "github.com": [{"active": True, "host": "github.com",
                                    "login": "octo-reviewer", "state": "success"}]}})
                result = subprocess.run(command, capture_output=True, timeout=5)
                if course == "process-failure":
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(result.stdout, b"")
                    continue
                if course == "malformed-response":
                    self.assertEqual(result.stdout, b"invalid HTTP framing\n")
                    continue
                self.assertEqual(result.returncode, 0)
                headers, body = result.stdout.split(b"\r\n\r\n", 1)
                self.assertIn(b"X-RateLimit-Resource: core", headers)
                self.assertEqual(json.loads(body)["login"], "octo-reviewer")
                denied = command.copy()
                denied[2] = "repos/octo-org/inaccessible-repository"
                result = subprocess.run(denied, capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stderr, b"")
                self.assertTrue(result.stdout.startswith(b"HTTP/2.0 403 Forbidden\r\n"))
                search = command.copy()
                search[2] = "search/issues"
                result = subprocess.run(search, capture_output=True, timeout=5)
                self.assertIn(b"X-RateLimit-Resource: search", result.stdout)
                for part in range(9):
                    query = json.dumps({"query": f"query BetterPullReviewCapabilityProbeV1Part{part}{{__typename}}"})
                    result = subprocess.run([str(directory / "bin/gh"), "api", "graphql",
                                             "--hostname", "github.com", "--include",
                                             "--method", "POST", "--input", "-"],
                                            input=query.encode(), capture_output=True, timeout=5)
                    self.assertEqual(result.returncode, 0)
                    self.assertIn("data", json.loads(result.stdout.split(b"\r\n\r\n", 1)[1]))
                state = json.loads((directory / "fixture.json").read_text())
                for oid in (state["base"], state["head"]):
                    result = subprocess.run(["/usr/bin/git", "--git-dir",
                                             str(directory / "repository.git"), "cat-file",
                                             "-e", oid + ":review.cpp"], capture_output=True)
                    self.assertEqual(result.returncode, 0)
                result = subprocess.run([str(directory / "bin/gh"), "auth", "token"],
                                        capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 64)
                result = subprocess.run(command[:-1] + ["POST"], capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 64)


if __name__ == "__main__":
    unittest.main()
