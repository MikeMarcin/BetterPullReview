import argparse
import contextlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parent.parent / "scripts" / "codex_lane.py"

PROJECT = json.loads(
    (Path(__file__).resolve().parent.parent / "config" / "project.json").read_text(encoding="utf-8")
)
LANE_LOG_NAME = f"{PROJECT['project']}-codex-lanes.jsonl"

FAKE_COMPANION = """\
import fs from "node:fs";
const args = process.argv.slice(2);
const promptIndex = args.indexOf("--prompt-file");
const prompt = promptIndex >= 0 ? fs.readFileSync(args[promptIndex + 1], "utf8") : "";
console.log(JSON.stringify({ args, prompt }));
"""

GIT_ENV = {
    "GIT_AUTHOR_NAME": "t",
    "GIT_AUTHOR_EMAIL": "t@t",
    "GIT_COMMITTER_NAME": "t",
    "GIT_COMMITTER_EMAIL": "t@t",
    "PATH": os.environ["PATH"],
}


def _git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        check=True,
        capture_output=True,
        text=True,
        env={**GIT_ENV, "HOME": str(repo)},
    )
    return result.stdout.strip()


@unittest.skipIf(shutil.which("node") is None, "node is required for the fake companion")
class CodexLaneTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.repo = self.root / "repo"
        self.repo.mkdir()
        _git(self.repo, "init", "--initial-branch=main")
        (self.repo / "seed.txt").write_text("seed\n", encoding="utf-8")
        _git(self.repo, "add", "seed.txt")
        _git(self.repo, "commit", "-m", "seed")
        self.companion = self.root / "fake-companion.mjs"
        self.companion.write_text(FAKE_COMPANION, encoding="utf-8")
        self.env = {
            **os.environ,
            "IMPLEMENT_RFC_CODEX_COMPANION": str(self.companion),
        }

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def _launch(self, *args: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--repo-root",
                str(self.repo),
                "launch",
                *args,
            ],
            capture_output=True,
            text=True,
            env=self.env,
            cwd=str(self.repo),
        )

    def _lane_log(self) -> list[dict]:
        log = self.repo / ".git" / LANE_LOG_NAME
        if not log.is_file():
            return []
        return [json.loads(line) for line in log.read_text(encoding="utf-8").splitlines()]

    def test_all_routes_are_forwarded_with_role_instructions_and_identity(self) -> None:
        module = _load_codex_lane()
        expected = {
            "rfc-utility": ("gpt-5.6-luna", "xhigh"),
            "rfc-test-runner": ("gpt-5.6-luna", "xhigh"),
            "rfc-scout": ("gpt-5.6-sol", "low"),
            "rfc-worker-routine": ("gpt-5.6-sol", "high"),
            "rfc-worker": ("gpt-6-astra", "high"),
            "rfc-reviewer": ("gpt-6-astra", "high"),
            "rfc-critical-reviewer": ("gpt-6-astra", "xhigh"),
        }
        for name, (model, effort) in expected.items():
            with self.subTest(role=name):
                lane_name = name.replace("-", "_") + "_focused_check"
                result = self._launch(
                    "--role", name, "--lane-name", lane_name,
                    "--task", "Bounded assignment.",
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotIn("downgrade", result.stderr)
                payload = json.loads(result.stdout)
                args = payload["args"]
                self.assertEqual(args[0], "task")
                self.assertEqual(args[args.index("--model") + 1], model)
                self.assertEqual(args[args.index("--effort") + 1], effort)
                self.assertNotIn("--write", args)
                role, route = module.load_role_route(name)
                self.assertIn(module.renderer.startup_banner(name, route), payload["prompt"])
                for instruction in module.renderer.project_instructions(role, PROJECT):
                    self.assertIn(instruction, payload["prompt"])
                addenda_roles = ("rfc-worker", name) if name == "rfc-worker-routine" else (name,)
                for addenda_role in addenda_roles:
                    for addendum in PROJECT["role_addenda"].get(addenda_role, []):
                        self.assertIn(addendum, payload["prompt"])
                self.assertIn(f"Session name: {lane_name}", payload["prompt"])
                self.assertIn("Bounded assignment.", payload["prompt"])
                record = self._lane_log()[-1]
                self.assertEqual(record["role"], name)
                self.assertEqual(record["lane_name"], lane_name)
                self.assertEqual(record["model"], model)
                self.assertEqual(record["effort_requested"], effort)
                self.assertEqual(record["effort"], effort)
                self.assertFalse(record["write"])
        self.assertEqual(len(self._lane_log()), len(expected))

    def test_effort_max_downgrades_to_xhigh(self) -> None:
        # Compatibility fallback is exercised without keeping a live route at max.
        module = _load_codex_lane()
        role, route = module.load_role_route("rfc-utility")
        route = {**route, "effort": "max"}
        args = argparse.Namespace(
            role="rfc-utility", task_file=None, task="Inventory changed files.",
            lane_name=None, write=False, allow_shared_write=False,
            cwd=str(self.repo), repo_root=self.repo, background=False,
        )
        stderr = io.StringIO()

        def capture_prompt(command):
            prompt_path = Path(command[command.index("--prompt-file") + 1])
            self.assertIn("Effort: xhigh", prompt_path.read_text())
            return subprocess.CompletedProcess(command, 0)

        with (
            mock.patch.object(module, "load_role_route", return_value=(role, route)),
            mock.patch.object(module, "find_companion", return_value=self.companion),
            mock.patch.object(module.subprocess, "run", side_effect=capture_prompt) as run,
            mock.patch.object(module, "record_launch") as record,
            contextlib.redirect_stderr(stderr),
        ):
            self.assertEqual(module.cmd_launch(args), 0)
        command = run.call_args.args[0]
        self.assertEqual(command[command.index("--effort") + 1], "xhigh")
        self.assertIn("downgrade", stderr.getvalue())
        self.assertEqual(record.call_args.args[1]["effort_requested"], "max")
        self.assertEqual(record.call_args.args[1]["effort"], "xhigh")
        self.assertFalse(Path(command[command.index("--prompt-file") + 1]).exists())

    def test_obsolete_role_is_rejected(self) -> None:
        result = self._launch("--role", "rfc-verifier", "--task", "Run tests.")
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown role", result.stderr)
        self.assertEqual(self._lane_log(), [])

    def test_write_refused_in_shared_checkout(self) -> None:
        for role in ("rfc-worker", "rfc-worker-routine", "rfc-test-runner"):
            with self.subTest(role=role):
                result = self._launch("--role", role, "--task", "Execute assignment.", "--write")
                self.assertEqual(result.returncode, 2)
                self.assertIn("own worktree", result.stderr)
                self.assertEqual(self._lane_log(), [])

    def test_write_allowed_in_lane_worktree(self) -> None:
        worktree = self.root / "wt-a"
        _git(self.repo, "worktree", "add", "-b", "lane/a", str(worktree), "HEAD")
        for role in ("rfc-worker", "rfc-worker-routine", "rfc-test-runner"):
            with self.subTest(role=role):
                result = self._launch(
                    "--role", role, "--task", "Execute the bounded assignment.",
                    "--write", "--cwd", str(worktree),
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                payload = json.loads(result.stdout)
                self.assertIn("--write", payload["args"])
                self.assertIn("--cwd", payload["args"])
                self.assertTrue(self._lane_log()[-1]["write"])
                if role == "rfc-test-runner":
                    self.assertIn("Do not edit source", payload["prompt"])
                    self.assertIn("Do not spawn another agent", payload["prompt"])

    def test_write_refused_for_read_only_role(self) -> None:
        result = self._launch("--role", "rfc-reviewer", "--task", "Review.", "--write")
        self.assertEqual(result.returncode, 2)
        self.assertIn("read-only", result.stderr)

    def test_lane_name_must_keep_role_prefix(self) -> None:
        result = self._launch(
            "--role", "rfc-scout", "--lane-name", "sync_path_scan", "--task", "Trace."
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("role prefix", result.stderr)
        self.assertEqual(self._lane_log(), [])


def _load_codex_lane():
    """Imports the launcher as a module.

    The other tests in this file invoke it as a subprocess, which is right for
    argument handling; prompt composition is a pure function and is cheaper and
    clearer to assert directly.
    """
    spec = importlib.util.spec_from_file_location("codex_lane_under_test", SCRIPT)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ComposePromptNamingTests(unittest.TestCase):
    """The first prompt line becomes the operator-visible job name."""

    def test_first_line_identifies_the_lane(self) -> None:
        # The companion labels a job by shortening the prompt's first line, so
        # a lane whose prompt led with the role instructions would be labelled
        # identically to every other lane of any role.
        role, route = _load_codex_lane().load_role_route("rfc-reviewer")
        prompt = _load_codex_lane().compose_prompt(
            "rfc-reviewer", role, route, "rfc_reviewer_snp_0042_closeout", "Do the thing."
        )
        first_line = prompt.splitlines()[0]
        self.assertIn("rfc_reviewer_snp_0042_closeout", first_line)
        self.assertIn("rfc-reviewer", first_line)

    def test_two_lanes_of_one_role_get_distinct_names(self) -> None:
        role, route = _load_codex_lane().load_role_route("rfc-reviewer")
        first, second = (
            _load_codex_lane().compose_prompt("rfc-reviewer", role, route, lane, "task").splitlines()[0]
            for lane in ("rfc_reviewer_alpha", "rfc_reviewer_beta")
        )
        self.assertNotEqual(first, second)

    def test_the_role_prompt_still_leads_the_instructions(self) -> None:
        # The banner is a label, not a replacement: the role's own instructions
        # must still be the first thing the agent is told to do.
        role, route = _load_codex_lane().load_role_route("rfc-reviewer")
        prompt = _load_codex_lane().compose_prompt("rfc-reviewer", role, route, "rfc_reviewer_x", "task")
        self.assertIn("emit exactly this startup line", prompt)
        self.assertLess(prompt.index(f"{PROJECT['display_name']} lane"), prompt.index("emit exactly this startup line"))


if __name__ == "__main__":
    unittest.main()
