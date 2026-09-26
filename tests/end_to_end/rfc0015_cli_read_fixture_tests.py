#!/usr/bin/env python3
"""Verify external fixture framing, split routing, refusal, and object identity."""

import json
import base64
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
    def test_modified_missing_left_is_absence_not_empty_blob(self):
        with contained_temporary_directory(prefix="bpr-cli-modified-fixture-") as temporary:
            directory = Path(temporary).resolve() / "modified"
            fixture.prepare(directory, "modified-missing-left")
            state = json.loads((directory / "fixture.json").read_text())
            selected = next(row for row in state["routes"]["/repos/octo-org/review-repository/pulls/17/files"]
                            if row["filename"] == "src/file_0000.cpp")
            self.assertEqual(selected["status"], "modified")
            self.assertNotIn("previous_filename", selected)
            git = ["/usr/bin/git", "--git-dir", str(directory / "repository.git")]
            self.assertEqual(subprocess.check_output([*git, "ls-tree", state["base"], "--", selected["filename"]]), b"")
            self.assertTrue(subprocess.check_output([*git, "ls-tree", state["head"], "--", selected["filename"]]).startswith(b"100644 blob "))
            self.assertTrue(subprocess.check_output([*git, "cat-file", "blob", state["head"] + ":" + selected["filename"]]))
            for revision in (state["base"], state["head"]):
                self.assertTrue(subprocess.check_output([*git, "cat-file", "blob", revision + ":src/file_0001.cpp"]))

    def test_large_course_requires_sparse_clone_and_exact_blob_read(self):
        with contained_temporary_directory(prefix="bpr-cli-large-fixture-") as temporary:
            directory = Path(temporary).resolve() / "large"
            fixture.prepare(directory, "large-positive")
            state = json.loads((directory / "fixture.json").read_text())
            manifest = json.loads((fixture.ROOT / "tests/fixtures/github/pull-requests/large-review-v1/manifest.json").read_text())
            root = (Path("/private/tmp") if sys.platform == "darwin" else Path("/tmp")) / ("bpr-cli-git-" + secrets.token_hex(8))
            root.mkdir(mode=0o700)
            self.addCleanup(shutil.rmtree, root)
            (root / "empty").mkdir()
            destination = root / "clone-0.git"
            command = [str(directory / "bin/gh"), "repo", "clone",
                       "https://github.com/octo-org/review-repository.git", str(destination),
                       "--no-upstream", "--", "--bare", "--quiet", "--revision=" + state["head"],
                       "--template=" + str(root / "empty"), "--config", "transfer.unpackLimit=0",
                       "--config", "fetch.unpackLimit=0", "--config", "core.hooksPath=/dev/null",
                       "--config", "core.fsmonitor=false"]
            self.assertEqual(subprocess.run(command, capture_output=True, timeout=15).returncode, 64)
            self.assertFalse(destination.exists())
            subprocess.run([*command, "--depth=1", "--filter=blob:none"], check=True, capture_output=True, timeout=15)
            for name in (".gitattributes", ".gitmodules"):
                oid = subprocess.check_output(["/usr/bin/git", "--git-dir", str(directory / "repository.git"),
                                               "rev-parse", state["head"] + ":" + name]).decode().strip()
                absent = subprocess.run(["/usr/bin/git", "--git-dir", str(destination), "cat-file", "-e", oid],
                                        env={**os.environ, "GIT_NO_LAZY_FETCH": "1", "GIT_ALLOW_PROTOCOL": ""}, capture_output=True)
                self.assertNotEqual(absent.returncode, 0, "special blobs must also remain absent in the sparse course")
            selected = next(item for item in manifest["files"] if item["path"] == "src/file_0000.cpp")
            missing = subprocess.run(["/usr/bin/git", "--git-dir", str(destination), "cat-file", "-e", selected["head"]["oid"]],
                                     env={**os.environ, "GIT_NO_LAZY_FETCH": "1", "GIT_ALLOW_PROTOCOL": ""}, capture_output=True)
            self.assertNotEqual(missing.returncode, 0, "sparse clone must not preload the selected blob")
            api = [str(directory / "bin/gh"), "api"]
            suffix = ["--hostname", "github.com", "--include", "--method", "GET"]
            blob_command = [*api, "repos/octo-org/review-repository/git/blobs/" + selected["head"]["oid"],
                            "--hostname", "github.com", "--method", "GET",
                            "--header", "Accept: application/vnd.github+json",
                            "--header", "X-GitHub-Api-Version: 2022-11-28"]
            blob = json.loads(subprocess.check_output(blob_command))
            self.assertEqual(blob["sha"], selected["head"]["oid"])
            self.assertEqual(len(base64.b64decode(blob["content"])), selected["head"]["bytes"])
            files = []
            for page in range(1, 7):
                wire = subprocess.check_output([*api, f"repos/octo-org/review-repository/pulls/17/files?page={page}&per_page=100", *suffix])
                headers, body = wire.split(b"\r\n\r\n", 1)
                self.assertEqual(b' rel="next"' in headers, page < 6)
                files.extend(json.loads(body))
            self.assertEqual(len(files), len(manifest["files"]))
            paths = []
            for start in range(0, len(files), 100):
                request = {"query": "query GetReviewFilesPage{files(first:100,after:$after){nodes{path}}}",
                           "variables": {"after": f"fixture-files-{start}"} if start else {}}
                wire = subprocess.check_output([*api, "graphql", "--hostname", "github.com",
                                                "--include", "--method", "POST", "--input", "-"],
                                               input=json.dumps(request).encode())
                pull = json.loads(wire.split(b"\r\n\r\n", 1)[1])["data"]["repository"]["pullRequest"]
                self.assertNotIn("reviewThreads", pull)
                paths.extend(item["path"] for item in pull["files"]["nodes"])
                self.assertEqual(pull["files"]["pageInfo"]["hasNextPage"], start + 100 < len(files))
            self.assertEqual(paths, [item["filename"] for item in files])

    def test_small_pull_request_in_large_repository_has_bounded_sparse_material(self):
        with contained_temporary_directory(prefix="bpr-cli-small-large-fixture-") as temporary:
            directory = Path(temporary).resolve() / "small"
            fixture.prepare(directory, "small-large-repository")
            state = json.loads((directory / "fixture.json").read_text())
            pull = state["routes"]["/repos/octo-org/review-repository/pulls/17"]
            self.assertLessEqual(pull["changed_files"], 100)
            self.assertLessEqual(pull["commits"], 100)
            git = ["/usr/bin/git", "--git-dir", str(directory / "repository.git")]
            ancestor = subprocess.check_output([*git, "rev-parse", state["base"] + "^"]).decode().strip()
            size = subprocess.check_output([*git, "cat-file", "-s", ancestor + ":unrelated.bin"])
            self.assertGreater(int(size), 32 * 1024 * 1024)
            self.assertEqual(subprocess.check_output([*git, "ls-tree", state["base"], "--", "unrelated.bin"]), b"")
            root = (Path("/private/tmp") if sys.platform == "darwin" else Path("/tmp")) / ("bpr-cli-git-" + secrets.token_hex(8))
            root.mkdir(mode=0o700)
            self.addCleanup(shutil.rmtree, root)
            (root / "empty").mkdir()
            destination = root / "clone-0.git"
            command = [str(directory / "bin/gh"), "repo", "clone",
                       "https://github.com/octo-org/review-repository.git", str(destination),
                       "--no-upstream", "--", "--bare", "--quiet", "--revision=" + state["base"],
                       "--template=" + str(root / "empty"), "--config", "transfer.unpackLimit=0",
                       "--config", "fetch.unpackLimit=0", "--config", "core.hooksPath=/dev/null",
                       "--config", "core.fsmonitor=false", "--depth=1", "--filter=blob:none"]
            subprocess.run(command, check=True, capture_output=True, timeout=20)
            self.assertLess(sum(p.stat().st_size for p in destination.rglob("*") if p.is_file()), 1024 * 1024)
            environment = dict(os.environ, GIT_NO_LAZY_FETCH="1", GIT_ALLOW_PROTOCOL="")
            types = subprocess.check_output(["/usr/bin/git", "--git-dir", str(destination),
                                             "cat-file", "--batch-all-objects", "--batch-check=%(objecttype)"], env=environment)
            self.assertNotIn(b"blob", types)
            self.assertEqual(types.count(b"commit"), 1)

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

    def test_missing_process_refuses_where_an_installed_gh_would_answer(self):
        with contained_temporary_directory(prefix="bpr-cli-fallback-") as temporary:
            empty = Path(temporary).resolve() / "empty"
            installed = Path(temporary).resolve() / "installed"
            empty.mkdir()
            installed.mkdir()
            gh = installed / "gh"
            gh.write_text("#!/bin/sh\nexit 99\n")
            gh.chmod(0o700)
            self.assertIsNone(fixture.missing_process_refusal("missing-process", (str(empty),)))
            self.assertIsNone(fixture.missing_process_refusal("positive", (str(installed),)))
            refusal = fixture.missing_process_refusal("missing-process", (str(empty), str(installed)))
            self.assertIsNotNone(refusal)
            self.assertIn(str(gh), refusal)

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
