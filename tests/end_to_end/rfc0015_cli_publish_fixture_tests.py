#!/usr/bin/env python3
"""Fixture contract evidence only; external UI verdicts are separate."""
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

import rfc0015_cli_publish_fixture as fixture


class PublishFixtureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bpr-cli-publish-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name).resolve() / "fixture"
        fixture.prepare(self.directory, "Synthetic review.")
        self.state = json.loads((self.directory / "fixture.json").read_text())

    def call(self, endpoint, payload=None):
        command = [str(self.directory / "bin/gh"), "api", endpoint.lstrip("/"),
                   "--hostname", "github.com", "--include", "--method",
                   "GET" if payload is None else "POST"]
        if payload is not None:
            command += ["--input", "-"]
        result = subprocess.run(command, input=None if payload is None else json.dumps(payload).encode(),
                                capture_output=True, timeout=5)
        self.assertEqual(result.stderr, b"")
        return result

    def payload(self, review=True):
        result = {"body": "Synthetic review.", "commit_id": self.state["head"]}
        result.update({"event": "COMMENT"} if review else
                      {"path": "review.cpp", "side": "RIGHT", "line": 1})
        return result

    def test_mutation_target_observation_identity(self):
        fixture.control(self.directory, "publication", "Synthetic review.")
        target = json.loads(self.call(fixture.PULL).stdout.split(b"\r\n\r\n", 1)[1])
        repository = self.state["routes"]["/repos/octo-org/review-repository"]
        self.assertEqual((target["id"], target["node_id"], target["number"], target["state"]),
                         (3001, "PR_fixture_17", 17, "open"))
        for side, oid, ref in (("base", self.state["base"], "main"),
                               ("head", self.state["head"], "fixture/deterministic-snapshots")):
            self.assertEqual(target[side]["sha"], oid)
            self.assertEqual(target[side]["ref"], ref)
            for field in ("id", "node_id", "full_name"):
                self.assertEqual(target[side]["repo"][field], repository[field])

    def test_initial_read_shape_and_empty_baselines(self):
        target = json.loads(self.call(fixture.PULL).stdout.split(b"\r\n\r\n", 1)[1])
        self.assertEqual(target, self.state["routes"][fixture.PULL])
        self.assertNotIn("node_id", target["base"]["repo"])
        for course in ("positive", "publication", "hold"):
            fixture.control(self.directory, course, "Synthetic review.")
            for endpoint in ("/reviews", "/comments"):
                body = self.call(fixture.PULL + endpoint).stdout.split(b"\r\n\r\n", 1)[1]
                self.assertEqual(json.loads(body), [])

    def test_receipts_and_no_child_writes(self):
        before = {p.relative_to(self.directory): p.read_bytes()
                  for p in self.directory.rglob("*") if p.is_file()}
        self.assertEqual(self.call("/user").returncode, 0)
        for review in (True, False):
            payload = self.payload(review)
            result = self.call(fixture.PULL + ("/reviews" if review else "/comments"), payload)
            self.assertEqual(result.returncode, 0)
            headers, body = result.stdout.split(b"\r\n\r\n", 1)
            self.assertIn(b"200" if review else b"201", headers)
            observed = json.loads(body)
            self.assertEqual(observed["body"], payload["body"])
            self.assertEqual(observed["commit_id"], payload["commit_id"])
            self.assertEqual(observed["user"]["login"], "octo-reviewer")
        after = {p.relative_to(self.directory): p.read_bytes()
                 for p in self.directory.rglob("*") if p.is_file()}
        self.assertEqual(before, after)

    def test_staged_post_then_comment_reconciliation(self):
        fixture.control(self.directory, "hold", "Synthetic review.", "secondary.cpp", 2)
        payload = dict(self.payload(False), path="secondary.cpp", line=2)
        command = [str(self.directory / "bin/gh"), "api", fixture.PULL.lstrip("/") + "/comments",
                   "--hostname", "github.com", "--include", "--method", "POST", "--input", "-"]
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            process.stdin.write(json.dumps(payload).encode())
            process.stdin.close()
            process.stdin = None
            time.sleep(0.2)
            self.assertIsNone(process.poll())
            fixture.control(self.directory, "published", "Synthetic review.", "secondary.cpp", 2)
            output, error = process.communicate(timeout=5)
            self.assertEqual((process.returncode, error), (0, b""))
            receipt = json.loads(output.split(b"\r\n\r\n", 1)[1])
            reconciled = json.loads(self.call(fixture.PULL + "/comments").stdout.split(b"\r\n\r\n", 1)[1])
            self.assertEqual(reconciled, [receipt])
            self.assertFalse((self.directory / "calls.jsonl").exists())
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    def test_closed_routes_and_shape(self):
        for endpoint, payload in ((fixture.PULL.replace("17", "18") + "/reviews", self.payload()),
                                  (fixture.PULL + "/reviews", dict(self.payload(), commit_id="f" * 40)),
                                  (fixture.PULL + "/comments", dict(self.payload(False), path="other.cpp")),
                                  (fixture.PULL + "/reviews", dict(self.payload(), extra="value"))):
            result = self.call(endpoint, payload)
            self.assertEqual(result.returncode, 64)
            self.assertEqual(result.stdout, b"")

    def test_mismatch_ambiguous_and_external_reconciliation(self):
        fixture.control(self.directory, "account-mismatch", "Synthetic review.")
        self.assertEqual(json.loads(self.call("/user").stdout.split(b"\r\n\r\n")[1])["login"], "other-reviewer")
        fixture.control(self.directory, "target-mismatch", "Synthetic review.")
        self.assertEqual(json.loads(self.call(fixture.PULL).stdout.split(b"\r\n\r\n")[1])["head"]["sha"], "f" * 40)
        fixture.control(self.directory, "ambiguous", "Synthetic review.")
        result = self.call(fixture.PULL + "/reviews", self.payload())
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b"")
        fixture.control(self.directory, "published", "Synthetic review.")
        reviews = json.loads(self.call(fixture.PULL + "/reviews").stdout.split(b"\r\n\r\n")[1])
        self.assertEqual(reviews[0]["body"], "Synthetic review.")
        self.assertEqual(reviews[0]["state"], "COMMENTED")


if __name__ == "__main__":
    unittest.main()
