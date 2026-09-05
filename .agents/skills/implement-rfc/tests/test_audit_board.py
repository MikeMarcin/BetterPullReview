from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "audit_board.py"
SPEC = importlib.util.spec_from_file_location("audit_board", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"could not load board audit at {SCRIPT}")
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)

# A fixture project, deliberately not this repository's config, so the test is
# identical in every project that carries the skill.
CONFIG = {
    "schema": "implement-rfc.project.v1",
    "project": "fixture",
    "display_name": "Fixture",
    "session_prefix": "fx",
    "origin_repo_name": "Fixture",
    "rfc_root": "docs/rfcs",
    "issue_root": "docs/issues",
    "next_id": {"mode": "single-file", "path": "docs/issues/NEXT_ID"},
    "reservations_dir": "docs/issues/_reservations",
    "allocate_id_command": "python3 scripts/allocate_issue_id.py",
    "lanes": {
        "backlog": "10-backlog",
        "ready": "20-ready",
        "platform_ready": {"win": "21-ready-win"},
        "in_progress": "30-in-progress",
        "parked": "40-parked",
        "blocked": "50-blocked",
        "done": "60-done",
    },
    "lane_statuses": {
        "10-backlog": "Backlog",
        "20-ready": "Ready",
        "21-ready-win": "Ready",
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
    "workstreams": {"main": {"branch": "main"}},
    "role_addenda": {},
}
LANES = module.project_config.lane_names(CONFIG)


def write_card(
    root: Path,
    lane: str,
    card_id: str,
    slug: str,
    *,
    status: str | None = None,
    rfc: str = "none",
    deps: tuple[str, ...] = (),
    closeout: bool = False,
    anchor: bool = False,
) -> Path:
    depends_on = "[" + ", ".join(deps) + "]"
    lines = [
        "---",
        f"id: {card_id}",
        f"title: {slug}",
        f"status: {status or CONFIG['lane_statuses'][lane]}",
        f"rfc: {rfc}",
        f"depends_on: {depends_on}",
    ]
    if closeout:
        lines.append("closeout: true")
    lines += ["---", "", f"# {card_id}", ""]
    if anchor:
        lines.append("RFC base: `0123456789abcdef0123456789abcdef01234567`.")
    path = root / "docs/issues" / lane / f"{card_id}-{slug}.md"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def write_rfc(root: Path, rfc_id: str, status: str) -> Path:
    path = root / "docs/rfcs" / f"{rfc_id}-fixture.md"
    path.write_text(
        f"---\nid: {rfc_id}\nstatus: {status}\n---\n\n# {rfc_id}\n",
        encoding="utf-8",
    )
    return path


def write_next_id(root: Path, value: str) -> None:
    (root / "docs/issues/NEXT_ID").write_text(value + "\n", encoding="utf-8")


class BoardAuditTests(unittest.TestCase):
    def build_clean(self, root: Path) -> None:
        for lane in LANES:
            (root / "docs/issues" / lane).mkdir(parents=True, exist_ok=True)
        (root / "docs/issues/_reservations").mkdir()
        (root / "docs/rfcs").mkdir(parents=True)
        write_rfc(root, "RFC-AAA-0001", "Proposed")
        write_card(root, "60-done", "AAA-0001", "implementation")
        write_card(
            root,
            "20-ready",
            "AAA-0003",
            "close-rfc-0001",
            rfc="RFC-AAA-0001",
            closeout=True,
            anchor=True,
        )
        write_next_id(root, "AAA-0010")

    def test_clean_board_conforms(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            violations, warnings, _ = module.audit(root, CONFIG)
            self.assertEqual(violations, [])
            self.assertEqual(warnings, [])

    def test_missing_transcripts_dir_is_reported_not_silent(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            for transcripts in (None, root / "no-such-transcripts"):
                _, _, infos = module.audit(root, CONFIG, transcripts)
                self.assertTrue(
                    any("remote stations audit their own transcripts" in row for row in infos)
                )

    def test_public_cli_smoke(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            config_path = root / "project.json"
            config_path.write_text(json.dumps(CONFIG), encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--repo-root",
                    str(root),
                    "--transcripts-dir",
                    str(root / "missing-transcripts"),
                    "--project-config",
                    str(config_path),
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn("board conforms: no violations, no warnings", result.stdout)

    def test_detects_noncanonical_folder_and_status_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            (root / "docs/issues/70-done").mkdir()
            write_card(root, "30-in-progress", "AAA-0004", "wrong-status", status="Ready")
            violations, _, _ = module.audit(root, CONFIG)
            text = "\n".join(violations)
            self.assertIn("non-canonical board folder: 70-done", text)
            self.assertIn("status 'Ready' != 'In Progress'", text)

    def test_underscore_folders_are_not_lanes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            violations, _, _ = module.audit(root, CONFIG)
            self.assertNotIn("_reservations", "\n".join(violations))

    def test_done_status_mismatch_is_a_violation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            write_card(root, "60-done", "AAA-0005", "old-card", status="Parked")
            violations, warnings, _ = module.audit(root, CONFIG)
            self.assertIn("status 'Parked' != 'Done'", "\n".join(violations))

    def test_duplicate_card_id_is_a_violation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            write_card(root, "10-backlog", "AAA-0001", "twin")
            violations, _, _ = module.audit(root, CONFIG)
            self.assertIn("duplicate card id AAA-0001", "\n".join(violations))

    def test_detects_dangling_dependency(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            write_card(root, "20-ready", "AAA-0006", "dangling", deps=("AAA-0999",))
            violations, _, _ = module.audit(root, CONFIG)
            self.assertIn("depends_on AAA-0999 not found", "\n".join(violations))

    def test_detects_malformed_and_rewound_next_ids(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            write_next_id(root, "AAA-0001")
            violations, _, _ = module.audit(root, CONFIG)
            self.assertIn("next AAA-0001 <=", "\n".join(violations))
            write_next_id(root, "not-an-id")
            violations, _, _ = module.audit(root, CONFIG)
            self.assertIn("malformed value 'not-an-id'", "\n".join(violations))

    def test_next_id_below_git_history_is_a_violation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            env = {
                "GIT_AUTHOR_NAME": "t",
                "GIT_AUTHOR_EMAIL": "t@t",
                "GIT_COMMITTER_NAME": "t",
                "GIT_COMMITTER_EMAIL": "t@t",
                "PATH": subprocess.os.environ["PATH"],
                "HOME": str(root),
            }

            def git(*args: str) -> None:
                subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True, env=env)

            git("init", "--initial-branch=main")
            write_next_id(root, "AAA-0020")
            git("add", "docs/issues/NEXT_ID")
            git("commit", "-m", "advance counter")
            write_next_id(root, "AAA-0010")
            violations, _, _ = module.audit(root, CONFIG)
            self.assertIn("below AAA-0020, which the counter already held", "\n".join(violations))

    def test_missing_linked_rfc_is_a_violation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            write_card(root, "20-ready", "AAA-0007", "missing-rfc", rfc="RFC-BBB-0001")
            violations, warnings, _ = module.audit(root, CONFIG)
            self.assertIn("linked RFC-BBB-0001 has no RFC file", "\n".join(violations))

    def test_active_auto_verified_rfc_is_a_follow_up_warning(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            write_rfc(root, "RFC-BBB-0001", "Auto-Verified")
            write_card(root, "20-ready", "AAA-0008", "late-work", rfc="RFC-BBB-0001")
            violations, warnings, _ = module.audit(root, CONFIG)
            self.assertEqual(violations, [])
            self.assertIn("follow-up/deferred pattern", "\n".join(warnings))

    def test_active_rejected_or_superseded_rfc_is_a_violation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            for status in ("Rejected", "Superseded"):
                with self.subTest(status=status):
                    write_rfc(root, "RFC-BBB-0001", status)
                    card = write_card(root, "50-blocked", "AAA-0008", "terminal-rfc-work", rfc="RFC-BBB-0001")
                    violations, warnings, _ = module.audit(root, CONFIG)
                    self.assertIn(f"already {status}", "\n".join(violations))
                    self.assertNotIn("RFC-BBB-0001: no closeout", "\n".join(warnings))
                    card.unlink()

    def test_open_rfc_without_closeout_warns(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            (root / "docs/issues/20-ready/AAA-0003-close-rfc-0001.md").unlink()
            write_card(root, "20-ready", "AAA-0003", "implementation-remaining", rfc="RFC-AAA-0001")
            violations, warnings, _ = module.audit(root, CONFIG)
            self.assertEqual(violations, [])
            self.assertIn("no closeout card found", "\n".join(warnings))

    def test_filename_pattern_closeout_detection(self) -> None:
        config = dict(CONFIG, closeout_card={"filename_pattern": r"^AAA-\d{4}-close-rfc-\d{4}\.md$"})
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            violations, warnings, _ = module.audit(root, config)
            self.assertEqual(violations, [])
            self.assertEqual(warnings, [])

    def test_closeout_without_run_anchor_warns(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            closeout = root / "docs/issues/20-ready/AAA-0003-close-rfc-0001.md"
            closeout.write_text(
                "\n".join(line for line in closeout.read_text(encoding="utf-8").splitlines() if "RFC base:" not in line) + "\n",
                encoding="utf-8",
            )
            violations, warnings, _ = module.audit(root, CONFIG)
            self.assertEqual(violations, [])
            self.assertIn("lacks an RFC_BASE_SHA run anchor", "\n".join(warnings))

    def test_transcript_presence_does_not_create_delegation_findings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.build_clean(root)
            transcripts = root / "transcripts"
            transcripts.mkdir()
            (transcripts / "session.jsonl").write_text('{"skill":"implement-rfc"}\n', encoding="utf-8")
            _, warnings, infos = module.audit(root, CONFIG, transcripts)
            self.assertEqual(warnings, [])
            self.assertIn("delegation behavior is informational", "\n".join(infos))


if __name__ == "__main__":
    unittest.main()
