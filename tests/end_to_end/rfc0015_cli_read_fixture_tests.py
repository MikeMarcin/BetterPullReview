#!/usr/bin/env python3
"""Verify external fixture framing, split routing, refusal, and object identity."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import rfc0015_cli_read_fixture as fixture


class cli_read_fixture_tests(unittest.TestCase):
    def test_courses_and_exact_objects(self):
        with tempfile.TemporaryDirectory(prefix="bpr-cli-fixture-") as temporary:
            for course in ("positive", "missing-process", "process-failure", "malformed-response"):
                directory = Path(temporary) / course
                fixture.prepare(directory, course)
                if course == "missing-process":
                    self.assertFalse((directory / "bin/gh").exists())
                    continue
                command = [str(directory / "bin/gh"), "api", "user", "--hostname",
                           "github.com", "--include", "--method", "GET"]
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
