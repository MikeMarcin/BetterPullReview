from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

try:
    from jsonschema import Draft202012Validator
except ModuleNotFoundError:  # pragma: no cover - optional dependency
    Draft202012Validator = None

PACK = Path(__file__).resolve().parent.parent
GUARD = PACK / "scripts" / "remote_station_guard.py"
LEASE = PACK / "scripts" / "station_lease.py"
PROJECT_CONFIG_MODULE = PACK / "scripts" / "project_config.py"
FIXTURE_PROJECT = {
    "schema": "implement-rfc.project.v1",
    "project": "fixture",
    "display_name": "Fixture",
    "session_prefix": "fx",
    "origin_repo_name": "fixture",
    "rfc_root": "docs/rfcs",
    "issue_root": "docs/issues",
    "next_id": {"mode": "single-file", "path": "docs/issues/NEXT_ID"},
    "reservations_dir": "docs/issues/_reservations",
    "allocate_id_command": "python3 scripts/allocate_issue_id.py",
    "lanes": {
        "backlog": "10-backlog",
        "ready": "20-ready",
        "platform_ready": {},
        "in_progress": "30-in-progress",
        "parked": "40-parked",
        "blocked": "50-blocked",
        "done": "60-done",
    },
    "lane_statuses": {
        "10-backlog": "Backlog",
        "20-ready": "Ready",
        "30-in-progress": "In Progress",
        "40-parked": "Parked",
        "50-blocked": "Blocked",
        "60-done": "Done",
    },
    "rfc_link_field": "rfc",
    "terminal_rfc_status": "Auto-Verified",
    "closeout_card": {"front_matter": {"closeout": "true"}},
    "structure_gate": [],
    "attended_review_skill": None,
    "station_registry_doc": "docs/development/remote-stations.md",
    "workstreams": {"main": {"branch": "main"}, "acta": {"branch": "codex/acta"}},
    "role_addenda": {},
}
SCHEMA = PACK / "schemas" / "remote-station.schema.json"


def _git(repo: Path, *arguments: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repo), *arguments],
        check=True,
        capture_output=True,
        text=True,
    )
    return completed.stdout.strip()


class RemoteStationGuardTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.repo = Path(self._temporary.name) / "repo"
        self.repo.mkdir()
        _git(self.repo, "init", "-b", "main")
        _git(self.repo, "config", "user.name", "station-test")
        _git(self.repo, "config", "user.email", "station@example.invalid")
        _git(self.repo, "remote", "add", "origin", "https://example.invalid/fixture.git")
        (self.repo / "seed.txt").write_text("seed\n", encoding="utf-8")
        _git(self.repo, "add", "seed.txt")
        _git(self.repo, "commit", "-m", "seed")
        self.head = _git(self.repo, "rev-parse", "HEAD")
        lease_target = (
            self.repo
            / ".agents"
            / "skills"
            / "implement-rfc"
            / "scripts"
            / "station_lease.py"
        )
        lease_target.parent.mkdir(parents=True)
        shutil.copy2(LEASE, lease_target)
        shutil.copy2(PROJECT_CONFIG_MODULE, lease_target.with_name("project_config.py"))
        config_dir = lease_target.parent.parent / "config"
        config_dir.mkdir()
        (config_dir / "project.json").write_text(
            json.dumps(FIXTURE_PROJECT, indent=2) + "\n", encoding="utf-8"
        )
        self.station = {
            "schema": "implement-rfc.remote-station.v2",
            "station_id": "stn-1234abcd",
            "station": "fx-main-mac",
            "project": "fixture",
            "workstream": "main",
            "environment": "macos",
            "branch": "main",
            "authority": "lane-commit",
            "codex": {
                "thread_id": "thr-station-1234",
                "thread_name": "fx-main-mac",
                "cli_version": "0.151.0",
            },
        }
        self._write_station()
        self.holder = subprocess.Popen(
            [
                sys.executable,
                str(lease_target),
                "run",
                "--role",
                "project-integrator",
                "--repo",
                str(self.repo),
                "--station",
                "fx-main-mac",
                "--",
                sys.executable,
                "-c",
                "import time; print('held', flush=True); time.sleep(30)",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        assert self.holder.stdout is not None
        self.assertEqual(self.holder.stdout.readline().strip(), "held")

    def tearDown(self) -> None:
        if self.holder.poll() is None:
            self.holder.kill()
            self.holder.wait()
        if self.holder.stdout is not None:
            self.holder.stdout.close()
        if self.holder.stderr is not None:
            self.holder.stderr.close()
        self._temporary.cleanup()

    def _write_station(self) -> None:
        (self.repo / ".remote-station.json").write_text(
            json.dumps(self.station, indent=2) + "\n",
            encoding="utf-8",
        )

    def _envelope(self, **overrides: str) -> str:
        values = {
            "station": "fx-main-mac",
            "workstream": "main",
            "branch": "main",
            "authority": "lane-commit",
            "base_commit": self.head,
        }
        values.update(overrides)
        return f"""```yaml
envelope: implement-rfc.remote-task.v1
task_id: task-20990101-test
project: fixture
station: {values["station"]}
workstream: {values["workstream"]}
branch: {values["branch"]}
checkout: {self.repo}
rfcs: [RFC-FX-0005]
cards: []
rfc_base_sha: {self.head}
base_commit: {values["base_commit"]}
authority: {values["authority"]}
id_reservations: []
expires_at: 2099-01-01T00:00:00Z
reply_to: fx-main-mac
done_when: test the station guard
```"""

    def _event(self, event_name: str, **values: object) -> dict:
        event = {
            "session_id": "thr-station-1234",
            "turn_id": "turn-test-1234",
            "cwd": str(self.repo),
            "hook_event_name": event_name,
            "permission_mode": "bypassPermissions",
        }
        event.update(values)
        return event

    def _run_guard(self, event: dict) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(GUARD)],
            input=json.dumps(event),
            capture_output=True,
            text=True,
            check=False,
        )

    def test_valid_turn_allows_writer_tools_until_stop(self) -> None:
        accepted = self._run_guard(
            self._event("UserPromptSubmit", prompt=self._envelope())
        )
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        output = json.loads(accepted.stdout)
        self.assertIn("accepted", output["hookSpecificOutput"]["additionalContext"])

        tool = self._run_guard(
            self._event(
                "PreToolUse",
                tool_name="apply_patch",
                tool_use_id="tool-test",
                tool_input={"command": "*** Begin Patch"},
            )
        )
        self.assertEqual(tool.stdout, "")

        stopped = self._run_guard(self._event("Stop"))
        self.assertEqual(stopped.stdout, "")
        denied = self._run_guard(
            self._event(
                "PreToolUse",
                tool_name="apply_patch",
                tool_use_id="tool-test-2",
                tool_input={"command": "*** Begin Patch"},
            )
        )
        self.assertEqual(
            json.loads(denied.stdout)["hookSpecificOutput"]["permissionDecision"],
            "deny",
        )

    def test_wrong_thread_is_blocked_before_prompt_submission(self) -> None:
        result = self._run_guard(
            self._event(
                "UserPromptSubmit",
                session_id="thr-other-1234",
                prompt=self._envelope(),
            )
        )

        output = json.loads(result.stdout)
        self.assertEqual(output["decision"], "block")
        self.assertIn("bound to Codex task", output["reason"])

    def test_station_mismatch_yields_remote_refusal_context(self) -> None:
        result = self._run_guard(
            self._event(
                "UserPromptSubmit",
                prompt=self._envelope(station="fx-acta-linux"),
            )
        )

        context = json.loads(result.stdout)["hookSpecificOutput"]["additionalContext"]
        self.assertIn("implement-rfc.remote-refusal.v1", context)
        self.assertIn("reason: station-mismatch", context)
        self.assertIn("correct_station: fx-main-mac", context)

    def test_branch_mismatch_yields_remote_refusal_context(self) -> None:
        result = self._run_guard(
            self._event(
                "UserPromptSubmit",
                prompt=self._envelope(branch="codex/wrong-branch"),
            )
        )

        context = json.loads(result.stdout)["hookSpecificOutput"]["additionalContext"]
        self.assertIn("reason: branch-mismatch", context)

    def test_non_ancestor_base_yields_remote_refusal_context(self) -> None:
        result = self._run_guard(
            self._event(
                "UserPromptSubmit",
                prompt=self._envelope(base_commit="0" * 40),
            )
        )

        context = json.loads(result.stdout)["hookSpecificOutput"]["additionalContext"]
        self.assertIn("reason: base-not-ancestor", context)

    def test_absent_supervisor_lease_refuses_the_turn(self) -> None:
        self.holder.kill()
        self.holder.wait()

        result = self._run_guard(
            self._event("UserPromptSubmit", prompt=self._envelope())
        )

        context = json.loads(result.stdout)["hookSpecificOutput"]["additionalContext"]
        self.assertIn("reason: authority-exceeded", context)
        self.assertIn("supervisor lease is not held", context)

    def test_read_only_authority_denies_repository_edits(self) -> None:
        self.station["authority"] = "read-only-proof"
        self._write_station()
        accepted = self._run_guard(
            self._event(
                "UserPromptSubmit",
                prompt=self._envelope(authority="read-only-proof"),
            )
        )
        self.assertIn("accepted", accepted.stdout)

        denied = self._run_guard(
            self._event(
                "PreToolUse",
                tool_name="apply_patch",
                tool_use_id="tool-read-only",
                tool_input={"command": "*** Begin Patch"},
            )
        )
        output = json.loads(denied.stdout)["hookSpecificOutput"]
        self.assertEqual(output["permissionDecision"], "deny")
        self.assertIn("read-only-proof", output["permissionDecisionReason"])

    def test_envelope_cannot_widen_read_only_station_authority(self) -> None:
        self.station["authority"] = "read-only-proof"
        self._write_station()

        result = self._run_guard(
            self._event("UserPromptSubmit", prompt=self._envelope())
        )

        context = json.loads(result.stdout)["hookSpecificOutput"]["additionalContext"]
        self.assertIn("reason: authority-exceeded", context)

    def test_v1_station_file_leaves_the_guard_inactive(self) -> None:
        self.station = {
            "schema": "implement-rfc.remote-station.v1",
            "station_id": "stn-1234abcd",
            "station": "fx-main-mac",
            "project": "fixture",
            "role": "main",
            "environment": "macos",
            "branch": "main",
            "authority": "lane-commit",
        }
        self._write_station()

        result = self._run_guard(
            self._event("UserPromptSubmit", prompt="ordinary work under a v1 file")
        )

        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertIn("guard inactive", result.stderr)

    def test_legacy_direct_codex_mode_does_not_apply_thread_hooks(self) -> None:
        self.station["codex"] = None
        self._write_station()

        result = self._run_guard(
            self._event(
                "UserPromptSubmit",
                session_id="thr-direct-session",
                prompt="ordinary direct Codex work",
            )
        )

        self.assertEqual(result.stdout, "")


@unittest.skipIf(Draft202012Validator is None, "jsonschema is required for schema validation")
class RemoteStationSchemaTests(unittest.TestCase):
    def test_v2_accepts_provisioned_and_legacy_direct_modes(self) -> None:
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        validator = Draft202012Validator(schema)
        station = {
            "schema": "implement-rfc.remote-station.v2",
            "station_id": "stn-1234abcd",
            "station": "fx-main-mac",
            "project": "fixture",
            "workstream": "main",
            "environment": "macos",
            "branch": "main",
            "authority": "lane-commit",
            "codex": {
                "thread_id": "thr-station-1234",
                "thread_name": "fx-main-mac",
                "cli_version": "0.151.0",
            },
        }
        validator.validate(station)
        station["claude"] = {"session_id": "session_Acta123"}
        validator.validate(station)
        station["claude"] = None
        station["codex"] = None
        validator.validate(station)

    def test_v1_is_rejected_after_migration(self) -> None:
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        validator = Draft202012Validator(schema)
        station = {
            "schema": "implement-rfc.remote-station.v1",
            "station_id": "stn-1234abcd",
            "station": "fx-main-mac",
            "project": "fixture",
            "workstream": "main",
            "environment": "macos",
            "branch": "main",
            "authority": "lane-commit",
            "codex": None,
        }
        self.assertTrue(list(validator.iter_errors(station)))


if __name__ == "__main__":
    unittest.main()
