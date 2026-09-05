from __future__ import annotations

import argparse
import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest import mock

SCRIPTS_ROOT = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS_ROOT))

import launch_remote_station as launcher  # noqa: E402


def arguments(**overrides: object) -> argparse.Namespace:
    values = {
        "workstream": "main",
        "machine": "",
        "authority": "lane-commit",
        "reattach": False,
        "new_claude_session": False,
        "fresh_station": False,
        "new_codex_session": False,
        "no_codex": False,
        "no_claude": False,
        "pair_codex": False,
        "no_server": False,
        "_supervise": False,
        "permission_mode": "bypassPermissions",
    }
    values.update(overrides)
    return argparse.Namespace(**values)


class FakeRemoteClient:
    def __init__(self, responses: dict[str, list[dict]], diagnostic: str = "") -> None:
        self.responses = {method: list(values) for method, values in responses.items()}
        self.calls: list[tuple[str, dict]] = []
        self._diagnostic = diagnostic
        self.closed = False

    def request(self, method: str, params: dict) -> dict:
        self.calls.append((method, params))
        values = self.responses.get(method)
        if not values:
            raise AssertionError(f"unexpected App Server request: {method}")
        return values.pop(0)

    def diagnostic(self) -> str:
        return self._diagnostic

    def close(self) -> None:
        self.closed = True


class LauncherTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        root = Path(self._temporary.name).resolve()
        self.context = launcher.StationContext(
            root=root,
            workstream="main",
            branch="main",
            name="fx-main-mac",
            environment="macos",
            authority="lane-commit",
            lease_role="project-integrator",
            station_file=root / ".remote-station.json",
        )

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def windows_context(self) -> launcher.StationContext:
        return replace(
            self.context,
            name="fx-main-win",
            environment="windows-native",
        )

    def test_station_document_is_v2_and_accepts_unprovisioned_legacy_mode(self) -> None:
        document = launcher.station_document(
            self.context,
            station_id="stn-1234abcd",
            codex=None,
        )

        self.assertEqual(document["schema"], launcher.STATION_SCHEMA)
        self.assertIsNone(document["codex"])
        self.assertIsNone(document["claude"])

    def test_reads_only_a_well_formed_claude_bridge_session(self) -> None:
        fake_home = self.context.root / "home"
        with mock.patch.object(launcher.pathlib.Path, "home", return_value=fake_home):
            pointer = launcher.claude_bridge_pointer(self.context.root)
            pointer.parent.mkdir(parents=True)
            pointer.write_text(
                json.dumps({"sessionId": "session_Acta123"}),
                encoding="utf-8",
            )

            self.assertEqual(
                launcher.read_claude_bridge_session_id(self.context.root),
                "session_Acta123",
            )
            pointer.write_text(json.dumps({"sessionId": "wrong"}), encoding="utf-8")
            self.assertIsNone(launcher.read_claude_bridge_session_id(self.context.root))

    def test_fresh_claude_isolates_cross_worktree_bridge_pointers(self) -> None:
        target = self.context.root / "target" / "bridge-pointer.json"
        sibling = self.context.root / "sibling" / "bridge-pointer.json"
        target.parent.mkdir()
        sibling.parent.mkdir()
        target.write_text("old-target", encoding="utf-8")
        sibling.write_text("old-sibling", encoding="utf-8")

        with mock.patch.object(
            launcher,
            "claude_worktree_bridge_pointers",
            return_value=(target, sibling),
        ):
            isolation = launcher.ClaudeBridgeIsolation.begin(self.context.root)

        self.assertFalse(target.exists())
        self.assertFalse(sibling.exists())
        target.write_text("new-target", encoding="utf-8")
        isolation.session_started()

        self.assertEqual(target.read_text(encoding="utf-8"), "new-target")
        self.assertEqual(sibling.read_text(encoding="utf-8"), "old-sibling")
        self.assertFalse(
            target.with_name(launcher.CLAUDE_BRIDGE_BACKUP_NAME).exists()
        )
        self.assertFalse(
            sibling.with_name(launcher.CLAUDE_BRIDGE_BACKUP_NAME).exists()
        )

    def test_failed_fresh_claude_restores_every_bridge_pointer(self) -> None:
        target = self.context.root / "target" / "bridge-pointer.json"
        sibling = self.context.root / "sibling" / "bridge-pointer.json"
        target.parent.mkdir()
        sibling.parent.mkdir()
        target.write_text("old-target", encoding="utf-8")
        sibling.write_text("old-sibling", encoding="utf-8")

        with mock.patch.object(
            launcher,
            "claude_worktree_bridge_pointers",
            return_value=(target, sibling),
        ):
            isolation = launcher.ClaudeBridgeIsolation.begin(self.context.root)
        isolation.close()

        self.assertEqual(target.read_text(encoding="utf-8"), "old-target")
        self.assertEqual(sibling.read_text(encoding="utf-8"), "old-sibling")

    def test_records_a_new_exact_claude_session_from_the_bridge(self) -> None:
        recorded: list[str] = []
        stop = launcher.threading.Event()
        with mock.patch.object(
            launcher,
            "read_claude_bridge_session_id",
            return_value="session_NewActa456",
        ):
            launcher.watch_for_claude_session(
                self.context,
                previous_session_id="session_OldMain123",
                record_session=recorded.append,
                stop=stop,
            )

        self.assertEqual(recorded, ["session_NewActa456"])

    def test_watch_outlives_the_grace_and_records_a_late_session(self) -> None:
        """`--spawn session` creates the session on first connection, not at launch."""
        recorded: list[str] = []
        released: list[str] = []
        stop = launcher.threading.Event()
        answers = iter([None, None, None, "session_LateActa789"])
        with mock.patch.object(
            launcher, "CLAUDE_BRIDGE_ISOLATION_GRACE_SECONDS", 0.0
        ), mock.patch.object(
            launcher, "CLAUDE_SESSION_RECORD_POLL_SECONDS", 0.001
        ), mock.patch.object(
            launcher,
            "read_claude_bridge_session_id",
            side_effect=lambda _root: next(answers),
        ):
            launcher.watch_for_claude_session(
                self.context,
                previous_session_id=None,
                record_session=recorded.append,
                stop=stop,
                grace_elapsed=lambda: released.append("released"),
            )

        self.assertEqual(recorded, ["session_LateActa789"])
        self.assertEqual(released, ["released"])

    def test_watch_ends_without_a_session_only_when_claude_stops(self) -> None:
        recorded: list[str] = []
        released: list[str] = []
        stop = launcher.threading.Event()
        polls: list[int] = []

        def never(_root: launcher.pathlib.Path) -> None:
            polls.append(1)
            if len(polls) == 5:
                stop.set()
            return None

        with mock.patch.object(
            launcher, "CLAUDE_BRIDGE_ISOLATION_GRACE_SECONDS", 0.0
        ), mock.patch.object(
            launcher, "CLAUDE_SESSION_RECORD_POLL_SECONDS", 0.001
        ), mock.patch.object(
            launcher, "read_claude_bridge_session_id", side_effect=never
        ):
            launcher.watch_for_claude_session(
                self.context,
                previous_session_id=None,
                record_session=recorded.append,
                stop=stop,
                grace_elapsed=lambda: released.append("released"),
            )

        self.assertEqual(recorded, [])
        self.assertEqual(released, ["released"])
        self.assertEqual(len(polls), 5)

    def test_releasing_siblings_keeps_the_stale_target_hidden(self) -> None:
        target = self.context.root / "target" / "bridge-pointer.json"
        sibling = self.context.root / "sibling" / "bridge-pointer.json"
        target.parent.mkdir()
        sibling.parent.mkdir()
        target.write_text("old-target", encoding="utf-8")
        sibling.write_text("old-sibling", encoding="utf-8")

        with mock.patch.object(
            launcher,
            "claude_worktree_bridge_pointers",
            return_value=(target, sibling),
        ), mock.patch.object(
            launcher, "claude_bridge_pointer", return_value=target
        ):
            isolation = launcher.ClaudeBridgeIsolation.begin(self.context.root)
        isolation.release_siblings()

        self.assertFalse(target.exists())
        self.assertTrue(target.with_name(launcher.CLAUDE_BRIDGE_BACKUP_NAME).exists())
        self.assertEqual(sibling.read_text(encoding="utf-8"), "old-sibling")
        self.assertFalse(
            sibling.with_name(launcher.CLAUDE_BRIDGE_BACKUP_NAME).exists()
        )

        target.write_text("new-target", encoding="utf-8")
        isolation.session_started()
        self.assertEqual(target.read_text(encoding="utf-8"), "new-target")
        self.assertFalse(
            target.with_name(launcher.CLAUDE_BRIDGE_BACKUP_NAME).exists()
        )

    def test_workstream_is_inferred_from_the_station_file(self) -> None:
        self.context.station_file.write_text(
            json.dumps({"schema": launcher.STATION_SCHEMA, "workstream": "linux"}),
            encoding="utf-8",
        )
        self.assertEqual(
            launcher.infer_workstream(self.context.station_file, "unrelated"),
            "linux",
        )

    def test_workstream_is_inferred_from_the_branch(self) -> None:
        for name, workstream in launcher.WORKSTREAMS.items():
            self.assertEqual(
                launcher.infer_workstream(
                    self.context.station_file, workstream["branch"]
                ),
                name,
            )

    def test_machine_suffix_is_inferred_from_the_station_file(self) -> None:
        """A relaunch keeps the recorded name without retyping `--machine`."""
        self.context.station_file.write_text(
            json.dumps(
                {"schema": launcher.STATION_SCHEMA, "station": "fx-win-jaegaw"}
            ),
            encoding="utf-8",
        )
        self.assertEqual(
            launcher.infer_machine(self.context.station_file, "fx-win"), "jaegaw"
        )

    def test_a_station_recorded_without_a_suffix_infers_none(self) -> None:
        self.context.station_file.write_text(
            json.dumps({"schema": launcher.STATION_SCHEMA, "station": "fx-win"}),
            encoding="utf-8",
        )
        self.assertEqual(launcher.infer_machine(self.context.station_file, "fx-win"), "")
        self.assertEqual(
            launcher.infer_machine(self.context.station_file, "fx-linux"), ""
        )

    def test_a_malformed_recorded_suffix_is_not_inferred(self) -> None:
        for recorded in ("fx-win-JaegAW", "fx-win-", "fx-win-a b", 17, None):
            with self.subTest(recorded=recorded):
                self.context.station_file.write_text(
                    json.dumps(
                        {"schema": launcher.STATION_SCHEMA, "station": recorded}
                    ),
                    encoding="utf-8",
                )
                self.assertEqual(
                    launcher.infer_machine(self.context.station_file, "fx-win"), ""
                )

    def test_no_station_file_infers_no_machine(self) -> None:
        self.assertEqual(
            launcher.infer_machine(self.context.station_file, "fx-win"), ""
        )

    def resolve(self, argv: list[str]) -> launcher.StationContext:
        """`resolve_context` for a checkout on the `win` workstream's branch."""
        workstream = launcher.WORKSTREAMS["win"]

        def fake_git(root: object, *command: str) -> str:
            if command[0] == "rev-parse":
                return str(self.context.root)
            if command[0] == "remote":
                return f"git@github.com:owner/{launcher.PROJECT['origin_repo_name']}.git"
            return workstream["branch"]

        with (
            mock.patch.object(launcher, "run_git", side_effect=fake_git),
            mock.patch.object(launcher, "host_platform", return_value=("win", "windows-native")),
        ):
            return launcher.resolve_context(launcher.parse_arguments(argv))

    def test_a_recorded_machine_suffix_survives_an_argument_free_relaunch(self) -> None:
        """The refusal this fixes: a relaunch recomputed a name its own task
        did not carry, kept the recorded task id, and then would not rename it."""
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": launcher.STATION_SCHEMA,
                    "workstream": "win",
                    "station": f"{launcher.PROJECT['session_prefix']}-win-jaegaw",
                }
            ),
            encoding="utf-8",
        )
        self.assertEqual(
            self.resolve([]).name,
            f"{launcher.PROJECT['session_prefix']}-win-jaegaw",
        )

    def test_fresh_station_takes_the_canonical_name_again(self) -> None:
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": launcher.STATION_SCHEMA,
                    "workstream": "win",
                    "station": f"{launcher.PROJECT['session_prefix']}-win-jaegaw",
                }
            ),
            encoding="utf-8",
        )
        self.assertEqual(
            self.resolve(["--fresh-station"]).name,
            f"{launcher.PROJECT['session_prefix']}-win",
        )

    def test_an_explicit_machine_that_contradicts_the_record_refuses(self) -> None:
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": launcher.STATION_SCHEMA,
                    "workstream": "win",
                    "station": f"{launcher.PROJECT['session_prefix']}-win-jaegaw",
                }
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(launcher.LaunchRefusal, "disagrees"):
            self.resolve(["--machine", "nuc"])
        # The same token the record already carries is not a contradiction.
        self.assertEqual(
            self.resolve(["--machine", "jaegaw"]).name,
            f"{launcher.PROJECT['session_prefix']}-win-jaegaw",
        )
        # `--fresh-station` is how a station is deliberately renamed.
        self.assertEqual(
            self.resolve(["--fresh-station", "--machine", "nuc"]).name,
            f"{launcher.PROJECT['session_prefix']}-win-nuc",
        )

    def test_the_resolved_machine_reaches_the_supervised_child(self) -> None:
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": launcher.STATION_SCHEMA,
                    "workstream": "win",
                    "station": f"{launcher.PROJECT['session_prefix']}-win-jaegaw",
                }
            ),
            encoding="utf-8",
        )
        parsed = launcher.parse_arguments([])
        workstream = launcher.WORKSTREAMS["win"]

        def fake_git(root: object, *command: str) -> str:
            if command[0] == "rev-parse":
                return str(self.context.root)
            if command[0] == "remote":
                return f"git@github.com:owner/{launcher.PROJECT['origin_repo_name']}.git"
            return workstream["branch"]

        with (
            mock.patch.object(launcher, "run_git", side_effect=fake_git),
            mock.patch.object(launcher, "host_platform", return_value=("win", "windows-native")),
        ):
            launcher.resolve_context(parsed)

        command = launcher.internal_supervisor_arguments(parsed)
        self.assertEqual(command[command.index("--machine") + 1], "jaegaw")

    def test_unknown_branch_without_a_station_file_refuses_to_guess(self) -> None:
        with self.assertRaisesRegex(launcher.LaunchRefusal, "cannot infer a workstream"):
            launcher.infer_workstream(self.context.station_file, "feature/x")

    def test_omitted_workstream_resolves_from_the_checkout(self) -> None:
        """`launch_remote_station.py` with no argument is the documented command."""
        parsed = launcher.parse_arguments([])
        self.assertIsNone(parsed.workstream)
        parsed = launcher.parse_arguments(["linux"])
        self.assertEqual(parsed.workstream, "linux")

    def test_supervisor_migrates_v1_and_preserves_station_id(self) -> None:
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": "implement-rfc.remote-station.v1",
                    "station_id": "stn-1234abcd",
                }
            ),
            encoding="utf-8",
        )
        completed = subprocess.CompletedProcess(["claude"], 0)
        codex = {
            "thread_id": "thr-station-1234",
            "thread_name": self.context.name,
            "cli_version": "0.151.0",
        }
        session = contextlib.nullcontext(codex)

        with (
            mock.patch.object(launcher, "provision_codex", return_value=session),
            mock.patch.object(launcher.subprocess, "run", return_value=completed) as run,
        ):
            result = launcher.supervise(arguments(), self.context)

        self.assertEqual(result, 0)
        migrated = json.loads(self.context.station_file.read_text(encoding="utf-8"))
        self.assertEqual(migrated["schema"], launcher.STATION_SCHEMA)
        self.assertEqual(migrated["station_id"], "stn-1234abcd")
        self.assertEqual(migrated["codex"], codex)
        self.assertIsNone(migrated["claude"])
        self.assertEqual(
            run.call_args.args[0][0:2],
            ["claude", "remote-control"],
        )

    def test_exact_session_id_is_forwarded_only_to_claude(self) -> None:
        command = launcher.claude_command(
            arguments(reattach=True),
            self.context,
            reattach=True,
            session_id="session_Acta123",
        )
        self.assertEqual(command[-2:], ["--session-id", "session_Acta123"])
        self.assertNotIn("--continue", command)
        self.assertNotIn("--name", command)
        self.assertNotIn("--spawn", command)

    def test_normal_start_reattaches_then_falls_back_to_fresh_claude(self) -> None:
        unavailable = subprocess.CompletedProcess(["claude"], 1)
        fresh = subprocess.CompletedProcess(["claude"], 0)
        with (
            mock.patch.object(
                launcher.subprocess,
                "run",
                side_effect=[unavailable, fresh],
            ) as run,
            mock.patch.object(launcher.time, "monotonic", return_value=10.0),
        ):
            result = launcher.run_claude(
                arguments(),
                self.context,
                session_id="session_Acta123",
            )

        self.assertEqual(result, 0)
        self.assertIn("--session-id", run.call_args_list[0].args[0])
        self.assertNotIn("--session-id", run.call_args_list[1].args[0])

    def test_continue_is_strict_and_does_not_fall_back(self) -> None:
        unavailable = subprocess.CompletedProcess(["claude"], 1)
        with mock.patch.object(
            launcher.subprocess,
            "run",
            return_value=unavailable,
        ) as run:
            result = launcher.run_claude(
                arguments(reattach=True),
                self.context,
                session_id="session_Acta123",
            )

        self.assertEqual(result, 1)
        self.assertEqual(run.call_count, 1)
        self.assertIn("--session-id", run.call_args.args[0])

    def test_continue_without_a_recorded_session_refuses(self) -> None:
        with self.assertRaisesRegex(launcher.LaunchRefusal, "no recorded Claude"):
            launcher.run_claude(arguments(reattach=True), self.context)

    def test_normal_start_without_a_recorded_session_starts_fresh(self) -> None:
        fresh = subprocess.CompletedProcess(["claude"], 0)
        with mock.patch.object(
            launcher.subprocess,
            "run",
            return_value=fresh,
        ) as run:
            result = launcher.run_claude(arguments(), self.context)

        self.assertEqual(result, 0)
        self.assertNotIn("--session-id", run.call_args.args[0])
        self.assertNotIn("--continue", run.call_args.args[0])
        self.assertEqual(run.call_args.args[0][:2], ["claude", "remote-control"])

    def test_explicit_new_claude_session_skips_reattachment(self) -> None:
        fresh = subprocess.CompletedProcess(["claude"], 0)
        with mock.patch.object(
            launcher.subprocess,
            "run",
            return_value=fresh,
        ) as run:
            result = launcher.run_claude(
                arguments(new_claude_session=True),
                self.context,
            )

        self.assertEqual(result, 0)
        self.assertNotIn("--continue", run.call_args.args[0])

    def test_running_claude_failure_does_not_create_a_new_session(self) -> None:
        stopped = subprocess.CompletedProcess(["claude"], 1)
        with (
            mock.patch.object(
                launcher.subprocess,
                "run",
                return_value=stopped,
            ) as run,
            mock.patch.object(launcher.time, "monotonic", side_effect=[10.0, 41.0]),
        ):
            result = launcher.run_claude(
                arguments(),
                self.context,
                session_id="session_Acta123",
            )

        self.assertEqual(result, 1)
        self.assertEqual(run.call_count, 1)

    def test_interrupted_reattachment_does_not_create_a_new_session(self) -> None:
        interrupted = subprocess.CompletedProcess(["claude"], -2)
        with (
            mock.patch.object(
                launcher.subprocess,
                "run",
                return_value=interrupted,
            ) as run,
            mock.patch.object(launcher.time, "monotonic", side_effect=[10.0, 10.1]),
        ):
            result = launcher.run_claude(
                arguments(),
                self.context,
                session_id="session_Acta123",
            )

        self.assertEqual(result, -2)
        self.assertEqual(run.call_count, 1)

    def test_outer_process_wraps_the_unified_supervisor_in_the_lease(self) -> None:
        completed = subprocess.CompletedProcess(["lease"], 0)
        with mock.patch.object(launcher.subprocess, "run", return_value=completed) as run:
            result = launcher.run_under_lease(
                arguments(reattach=True, pair_codex=True),
                self.context,
            )

        self.assertEqual(result, 0)
        command = run.call_args.args[0]
        self.assertIn("station_lease.py", command[1])
        self.assertIn("--_supervise", command)
        self.assertIn("--pair-codex", command)
        self.assertIn("--continue", command)
        self.assertEqual(command[command.index("--station") + 1], self.context.name)

    def test_new_claude_session_is_forwarded_to_the_supervisor(self) -> None:
        command = launcher.internal_supervisor_arguments(
            arguments(new_claude_session=True)
        )

        self.assertIn("--new-claude-session", command)
        self.assertNotIn("--continue", command)

    def test_fresh_station_is_forwarded_to_the_supervisor(self) -> None:
        command = launcher.internal_supervisor_arguments(
            arguments(fresh_station=True, new_claude_session=True)
        )

        self.assertIn("--fresh-station", command)
        self.assertNotIn("--continue", command)

    def test_fresh_station_archives_the_recorded_task_before_provisioning(self) -> None:
        """Starting over leaves nothing the new provisioning could adopt."""
        context = self.windows_context()
        client = FakeRemoteClient({})
        app_server = mock.MagicMock()
        app_server.return_value.__enter__.return_value = client

        with (
            mock.patch.object(launcher, "codex_cli_version", return_value="0.151.0"),
            mock.patch.object(launcher, "AppServerClient", app_server),
            mock.patch.object(launcher, "start_codex_desktop_app"),
            mock.patch.object(
                launcher,
                "release_named_threads",
                return_value=["thr-recorded archived"],
            ) as release,
            mock.patch.object(
                launcher,
                "provision_named_thread",
                return_value="thr-station-5678",
            ) as provision,
        ):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                with launcher.provision_codex(
                    context,
                    existing={"codex": {"thread_id": "thr-recorded"}},
                    pair=False,
                    fresh=True,
                ) as provisioned:
                    self.assertEqual(provisioned["thread_id"], "thr-station-5678")

        release.assert_called_once_with(
            client,
            cwd=context.root,
            name=context.name,
            recorded_thread_id="thr-recorded",
        )
        self.assertIn("thr-recorded archived", output.getvalue())
        # The recorded id is forgotten, so provisioning creates rather than adopts.
        provision.assert_called_once_with(
            client,
            cwd=context.root,
            name=context.name,
            expected_thread_id=None,
            new_session=False,
        )

    def test_a_normal_relaunch_archives_nothing(self) -> None:
        context = self.windows_context()
        client = FakeRemoteClient({})
        app_server = mock.MagicMock()
        app_server.return_value.__enter__.return_value = client

        with (
            mock.patch.object(launcher, "codex_cli_version", return_value="0.151.0"),
            mock.patch.object(launcher, "AppServerClient", app_server),
            mock.patch.object(launcher, "start_codex_desktop_app"),
            mock.patch.object(launcher, "release_named_threads") as release,
            mock.patch.object(
                launcher,
                "provision_named_thread",
                return_value="thr-recorded",
            ) as provision,
        ):
            with launcher.provision_codex(
                context,
                existing={"codex": {"thread_id": "thr-recorded"}},
                pair=False,
            ):
                pass

        release.assert_not_called()
        self.assertEqual(
            provision.call_args.kwargs["expected_thread_id"], "thr-recorded"
        )

    def test_fresh_station_keeps_neither_station_id_nor_claude_session(self) -> None:
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": launcher.STATION_SCHEMA,
                    "station_id": "stn-1234abcd",
                    "codex": {"thread_id": "thr-recorded"},
                    "claude": {"session_id": "session_Old123"},
                }
            ),
            encoding="utf-8",
        )
        codex = {
            "thread_id": "thr-station-5678",
            "thread_name": self.context.name,
            "cli_version": "0.151.0",
        }

        with (
            mock.patch.object(
                launcher,
                "provision_codex",
                return_value=contextlib.nullcontext(codex),
            ) as provision,
            mock.patch.object(
                launcher.subprocess,
                "run",
                return_value=subprocess.CompletedProcess(["claude"], 0),
            ) as run,
        ):
            result = launcher.supervise(
                arguments(fresh_station=True, new_claude_session=True), self.context
            )

        self.assertEqual(result, 0)
        # provision_codex still sees the recorded file, because it is what
        # tells it which task to archive.
        self.assertEqual(
            provision.call_args.kwargs["existing"]["station_id"], "stn-1234abcd"
        )
        self.assertIs(provision.call_args.kwargs["fresh"], True)
        written = json.loads(self.context.station_file.read_text(encoding="utf-8"))
        self.assertNotEqual(written["station_id"], "stn-1234abcd")
        self.assertEqual(written["codex"], codex)
        self.assertIsNone(written["claude"])
        self.assertEqual(
            run.call_args.args[0][0:2],
            ["claude", "remote-control"],
        )

    def test_pairing_is_explicit_and_follows_local_provisioning(self) -> None:
        client = FakeRemoteClient(
            {
                "remoteControl/status/read": [
                    {"status": "connecting"},
                    {"status": "connecting"},
                    {"status": "connected"},
                ],
                "remoteControl/pairing/start": [
                    {
                        "pairingCode": "do-not-print-this",
                        "manualPairingCode": "ABCD-EFGH",
                        "expiresAt": 1000,
                    }
                ],
                "remoteControl/pairing/status": [{"claimed": True}],
            }
        )
        with (
            mock.patch.object(launcher, "codex_cli_version", return_value="0.151.0"),
            mock.patch.object(
                launcher, "connect_codex_app_server", return_value=client
            ),
            mock.patch.object(
                launcher,
                "provision_named_thread",
                return_value="thr-station-1234",
            ),
            mock.patch.object(launcher.time, "time", return_value=700),
            mock.patch.object(launcher.time, "monotonic", return_value=0),
            mock.patch.object(launcher, "CODEX_REMOTE_CONNECT_WAIT_SECONDS", 0),
        ):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                with launcher.provision_codex(
                    self.context,
                    existing={},
                    pair=True,
                ) as provisioned:
                    self.assertEqual(provisioned["thread_id"], "thr-station-1234")

        self.assertEqual(
            [method for method, _ in client.calls],
            [
                "remoteControl/status/read",
                "remoteControl/status/read",
                "remoteControl/pairing/start",
                "remoteControl/pairing/status",
                "remoteControl/status/read",
            ],
        )
        self.assertEqual(client.calls[0][1], {})
        self.assertEqual(client.calls[2][1], {"manualCode": True})
        self.assertIn("ABCD-EFGH", output.getvalue())
        self.assertIn("different controller device", output.getvalue())
        self.assertNotIn("do-not-print-this", output.getvalue())
        self.assertTrue(client.closed)

    def test_pairing_refuses_an_already_errored_connection(self) -> None:
        client = FakeRemoteClient(
            {"remoteControl/status/read": [{"status": "errored"}]},
            diagnostic="HTTP 409: Remote app server already online",
        )
        with self.assertRaisesRegex(launcher.LaunchRefusal, "HTTP 409"):
            launcher.ensure_codex_remote(client, pair=True)

        self.assertEqual(len(client.calls), 1)

    def test_pairing_refuses_a_missing_manual_code(self) -> None:
        with self.assertRaisesRegex(launcher.LaunchRefusal, "manual pairing code"):
            launcher.codex_pairing_details(
                {"pairingCode": "machine-protocol-value"}
            )

    def test_unpaired_remote_is_not_reported_ready(self) -> None:
        client = FakeRemoteClient(
            {
                "remoteControl/status/read": [
                    {"status": "connecting"},
                    {"status": "connecting"},
                ],
            }
        )
        with (
            mock.patch.object(launcher.time, "monotonic", return_value=0),
            mock.patch.object(launcher, "CODEX_REMOTE_CONNECT_WAIT_SECONDS", 0),
        ):
            with self.assertRaisesRegex(launcher.LaunchRefusal, "--pair-codex"):
                launcher.ensure_codex_remote(client, pair=False)

    def test_malformed_remote_status_is_refused(self) -> None:
        client = FakeRemoteClient(
            {"remoteControl/status/read": [{"status": "unexpected"}]}
        )

        with self.assertRaisesRegex(launcher.LaunchRefusal, "malformed"):
            launcher.ensure_codex_remote(client, pair=False)

    def test_missing_remote_control_rpc_preserves_actionable_diagnostic(self) -> None:
        client = mock.MagicMock()
        client.request.side_effect = launcher.CodexStationError(
            "Codex App Server rejected remoteControl/status/read: method not found"
        )
        with (
            mock.patch.object(launcher, "codex_cli_version", return_value="0.151.0"),
            mock.patch.object(
                launcher, "connect_codex_app_server", return_value=client
            ),
        ):
            with self.assertRaisesRegex(launcher.LaunchRefusal, "method not found"):
                with launcher.provision_codex(
                    self.context,
                    existing={},
                    pair=False,
                ):
                    pass

    def test_native_windows_uses_short_lived_stdio_provisioning_only(self) -> None:
        context = self.windows_context()
        client = FakeRemoteClient({})
        app_server = mock.MagicMock()
        app_server.return_value.__enter__.return_value = client

        with (
            mock.patch.object(launcher, "codex_cli_version", return_value="0.151.0"),
            mock.patch.object(launcher, "AppServerClient", app_server),
            mock.patch.object(launcher, "start_codex_desktop_app") as start_app,
            mock.patch.object(launcher, "connect_codex_app_server") as connect,
            mock.patch.object(launcher, "ensure_codex_remote") as ensure_remote,
            mock.patch.object(
                launcher,
                "provision_named_thread",
                return_value="thr-station-1234",
            ) as provision,
        ):
            with launcher.provision_codex(
                context,
                existing={},
                pair=False,
            ) as provisioned:
                self.assertEqual(provisioned["thread_id"], "thr-station-1234")
                app_server.return_value.__exit__.assert_called_once()

        start_app.assert_called_once_with(context)
        app_server.assert_called_once_with(cwd=context.root)
        provision.assert_called_once_with(
            client,
            cwd=context.root,
            name=context.name,
            expected_thread_id=None,
            new_session=False,
        )
        connect.assert_not_called()
        ensure_remote.assert_not_called()
        self.assertEqual(client.calls, [])

    def test_native_windows_pairing_is_owned_by_desktop(self) -> None:
        context = self.windows_context()
        with (
            mock.patch.object(launcher, "codex_cli_version", return_value="0.151.0"),
            mock.patch.object(launcher, "start_codex_desktop_app") as start_app,
        ):
            with self.assertRaisesRegex(launcher.LaunchRefusal, "Control this PC"):
                with launcher.provision_codex(
                    context,
                    existing={},
                    pair=True,
                ):
                    pass

        start_app.assert_not_called()

    def test_native_windows_prints_the_desktop_sidebar_boundary(self) -> None:
        context = self.windows_context()
        codex = {
            "thread_id": "thr-station-1234",
            "thread_name": context.name,
            "cli_version": "0.151.0",
        }
        output = io.StringIO()
        with (
            mock.patch.object(
                launcher,
                "provision_codex",
                return_value=contextlib.nullcontext(codex),
            ),
            mock.patch.object(launcher, "run_claude", return_value=0),
            contextlib.redirect_stdout(output),
        ):
            result = launcher.supervise(arguments(), context)

        self.assertEqual(result, 0)
        self.assertIn(
            f"drag fx-main-win onto the {launcher.PROJECT['display_name']} project",
            output.getvalue(),
        )
        self.assertIn("no supported project-placement RPC", output.getvalue())

    def test_windows_has_no_supported_desktop_proxy_attachment(self) -> None:
        context = self.windows_context()
        with mock.patch.object(launcher, "start_codex_remote_daemon") as daemon:
            with self.assertRaisesRegex(
                launcher.LaunchRefusal, "no supported App Server proxy"
            ):
                launcher.connect_codex_app_server(context)

        daemon.assert_not_called()

    def test_start_codex_desktop_app_opens_the_station_checkout(self) -> None:
        completed = subprocess.CompletedProcess(["codex"], 0, "", "")
        with mock.patch.object(
            launcher.subprocess, "run", return_value=completed
        ) as run:
            launcher.start_codex_desktop_app(self.context)

        self.assertEqual(
            run.call_args.args[0],
            ["codex", "app", str(self.context.root)],
        )

    def test_daemon_start_rejects_malformed_json(self) -> None:
        completed = subprocess.CompletedProcess(
            ["codex"],
            0,
            "not-json\n",
            "",
        )
        with mock.patch.object(launcher.subprocess, "run", return_value=completed):
            with self.assertRaisesRegex(launcher.LaunchRefusal, "malformed JSON"):
                launcher.start_codex_remote_daemon(self.context)

    def test_supervisor_keeps_codex_process_alive_until_claude_stops(self) -> None:
        events: list[str] = []
        codex = {
            "thread_id": "thr-station-1234",
            "thread_name": self.context.name,
            "cli_version": "0.151.0",
        }

        @contextlib.contextmanager
        def session() -> object:
            events.append("codex-open")
            yield codex
            events.append("codex-closed")

        def run_claude(*_: object, **__: object) -> int:
            self.assertEqual(events, ["codex-open"])
            return 0

        with (
            mock.patch.object(launcher, "provision_codex", return_value=session()),
            mock.patch.object(launcher, "run_claude", side_effect=run_claude),
        ):
            result = launcher.supervise(arguments(), self.context)

        self.assertEqual(result, 0)
        self.assertEqual(events, ["codex-open", "codex-closed"])

    def test_minimum_codex_version_is_enforced(self) -> None:
        old = subprocess.CompletedProcess(["codex"], 0, "codex-cli 0.150.0\n", "")
        with mock.patch.object(launcher.subprocess, "run", return_value=old):
            with self.assertRaisesRegex(launcher.LaunchRefusal, "too old"):
                launcher.codex_cli_version(self.context)

    def test_no_server_marks_the_registry_as_direct_codex_mode(self) -> None:
        self.context.station_file.write_text(
            json.dumps(
                {
                    "schema": launcher.STATION_SCHEMA,
                    "station_id": "stn-1234abcd",
                    "codex": {
                        "thread_id": "thr-station-1234",
                        "thread_name": self.context.name,
                        "cli_version": "0.151.0",
                    },
                }
            ),
            encoding="utf-8",
        )

        result = launcher.run_legacy_no_server(self.context)

        self.assertEqual(result, 0)
        current = json.loads(self.context.station_file.read_text(encoding="utf-8"))
        self.assertEqual(current["station_id"], "stn-1234abcd")
        self.assertIsNone(current["codex"])

    def test_new_codex_flag_crosses_lease_boundary(self) -> None:
        parsed = launcher.parse_arguments(["main", "--new-codex-session"])
        self.assertIn("--new-codex-session", launcher.internal_supervisor_arguments(parsed))
        self.assertFalse(parsed.new_claude_session)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            launcher.parse_arguments(["main", "--new-codex-session", "--no-server"])

    def test_service_selection_and_fresh_apply_only_to_enabled_service(self) -> None:
        for flag in ("--no-codex", "--no-claude"):
            parsed = launcher.parse_arguments(["main", flag, "--fresh"])
            self.assertEqual(parsed.new_codex_session, flag == "--no-claude")
            self.assertEqual(parsed.new_claude_session, flag == "--no-codex")
            self.assertIn(flag, launcher.internal_supervisor_arguments(parsed))
        for flags in (
            ["--no-codex", "--no-claude"],
            ["--no-codex", "--pair-codex"],
            ["--no-codex", "--fresh", "codex"],
            ["--no-claude", "--continue"],
            ["--no-claude", "--fresh", "claude"],
            ["--no-claude", "--no-server"],
        ):
            with self.subTest(flags=flags):
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    launcher.parse_arguments(flags)

    def test_claude_only_skips_codex_and_preserves_its_binding(self) -> None:
        codex = {"thread_id": "thr-old", "thread_name": self.context.name}
        launcher.atomic_write_json(self.context.station_file, {"codex": codex})
        def run(*args, **kwargs):
            kwargs["record_session"]("session_New123")
            return 0
        with (
            mock.patch.object(launcher, "provision_codex", side_effect=AssertionError("Codex is down")),
            mock.patch.object(launcher, "run_claude", side_effect=run),
        ):
            self.assertEqual(launcher.supervise(arguments(no_codex=True), self.context), 0)
        saved = launcher.load_station_file(self.context.station_file)
        self.assertEqual(saved["codex"], codex)
        self.assertEqual(saved["claude"]["session_id"], "session_New123")

    def test_codex_only_holds_supervisor_open_without_starting_claude(self) -> None:
        claude = {"session_id": "session_Old123"}
        launcher.atomic_write_json(self.context.station_file, {"claude": claude})
        events = []
        @contextlib.contextmanager
        def provision(*args, **kwargs):
            events.append("open")
            yield {"thread_id": "thr-new", "cli_version": "0.151.0"}
            events.append("close")
        def wait():
            self.assertEqual(events, ["open"])
            return 0
        with (
            mock.patch.object(launcher, "provision_codex", side_effect=provision),
            mock.patch.object(launcher, "run_claude", side_effect=AssertionError("Claude is down")),
            mock.patch.object(launcher, "wait_for_station_stop", side_effect=wait),
        ):
            self.assertEqual(launcher.supervise(arguments(no_claude=True), self.context), 0)
        self.assertEqual(events, ["open", "close"])
        self.assertEqual(launcher.load_station_file(self.context.station_file)["claude"], claude)

    def test_codex_only_wait_stops_cleanly_on_ctrl_c(self) -> None:
        with mock.patch.object(launcher.time, "sleep", side_effect=KeyboardInterrupt):
            self.assertEqual(launcher.wait_for_station_stop(), 0)

    def test_fresh_options_and_legacy_flags_reach_the_same_supervisor(self) -> None:
        for flags, codex, claude in (
            ([], False, False),
            (["--fresh"], True, True),
            (["--fresh", "both"], True, True),
            (["--fresh", "codex"], True, False),
            (["--fresh", "claude"], False, True),
            (["--new-codex-session"], True, False),
            (["--new-claude-session"], False, True),
        ):
            with self.subTest(flags=flags):
                parsed = launcher.parse_arguments(["main", *flags])
                self.assertEqual(parsed.new_codex_session, codex)
                self.assertEqual(parsed.new_claude_session, claude)
                forwarded = launcher.internal_supervisor_arguments(parsed)
                self.assertEqual("--new-codex-session" in forwarded, codex)
                self.assertEqual("--new-claude-session" in forwarded, claude)

    def test_fresh_claude_conflicts_with_strict_resume(self) -> None:
        for flags in (
            ["--fresh", "--require-claude-resume"],
            ["--fresh", "claude", "--continue"],
            ["--fresh", "--no-server"],
        ):
            with self.subTest(flags=flags):
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    launcher.parse_arguments(flags)
        parsed = launcher.parse_arguments(["--fresh", "codex", "--require-claude-resume"])
        self.assertTrue(parsed.reattach)
        self.assertFalse(parsed.new_claude_session)

    def test_help_separates_everyday_and_advanced_options(self) -> None:
        for flag, advanced in (("--help", False), ("--help-all", True)):
            output = io.StringIO()
            with contextlib.redirect_stdout(output), self.assertRaises(SystemExit) as result:
                launcher.parse_arguments([flag])
            self.assertEqual(result.exception.code, 0)
            self.assertIn("--fresh", output.getvalue())
            self.assertEqual("--permission-mode" in output.getvalue(), advanced)
            self.assertEqual("--require-claude-resume" in output.getvalue(), advanced)

    def test_new_codex_session_updates_registry_and_preserves_claude(self) -> None:
        old = launcher.station_document(
            self.context, station_id="stn-existing",
            codex={"thread_id": "thr-old"},
            claude={"session_id": "session_Existing123"},
        )
        launcher.atomic_write_json(self.context.station_file, old)
        new = {"thread_id": "thr-new", "thread_name": self.context.name,
               "cli_version": "0.151.0"}
        with (
            mock.patch.object(launcher, "provision_codex",
                              return_value=contextlib.nullcontext(new)) as provision,
            mock.patch.object(launcher, "run_claude", return_value=0) as claude,
        ):
            self.assertEqual(launcher.supervise(
                arguments(new_codex_session=True), self.context,
            ), 0)
        self.assertTrue(provision.call_args.kwargs["new_session"])
        saved = launcher.load_station_file(self.context.station_file)
        self.assertEqual(saved["codex"], new)
        self.assertEqual(saved["claude"], old["claude"])
        self.assertEqual(saved["station_id"], old["station_id"])
        self.assertEqual(claude.call_args.kwargs["session_id"], "session_Existing123")

    def test_fresh_claude_session_has_the_station_name_and_spawn_mode(self) -> None:
        command = launcher.claude_command(
            arguments(),
            self.context,
            reattach=False,
        )

        self.assertEqual(command[:2], ["claude", "remote-control"])
        self.assertEqual(command[command.index("--name") + 1], self.context.name)
        self.assertIn("--spawn", command)


if __name__ == "__main__":
    unittest.main()
