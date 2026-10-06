#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import sys
import subprocess
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path


# Mirrors the guard in scripts/verify_documentation.py, which this imports.
# Without it the failure surfaces as an import-time TypeError from inside that
# module rather than as a wrong-interpreter message.
MINIMUM_PYTHON = (3, 10)

if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "verify_documentation_tests.py requires Python {}.{} or newer; this is "
        "{}.{}.{} at {}.\n".format(
            MINIMUM_PYTHON[0],
            MINIMUM_PYTHON[1],
            sys.version_info[0],
            sys.version_info[1],
            sys.version_info[2],
            sys.executable,
        )
    )
    raise SystemExit(2)


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

from scripts.verify_documentation import (  # noqa: E402
    front_matter,
    relocated_review_card_link,
    verify_issue_board,
    verify_operator_packet_records,
    verify_pending_packet_manifests,
    verify_no_conflict_markers,
    verify_rfc_index,
    verify_terminal_closeout_cards,
)


class HistoricalReviewCardLinkTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.review = self.root / "docs/verification/reviews/attended-v001/operator-packet.md"
        self.review.parent.mkdir(parents=True)
        self.board = self.root / "docs/issues"
        self.old = self.board / "20-ready/BPR-0743-fixture.md"
        self.current = self.board / "30-in-progress/BPR-0743-fixture.md"
        self.current.parent.mkdir(parents=True)
        self.current.write_text("current card\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_unique_relocated_card_keeps_historical_packet_valid(self) -> None:
        self.assertTrue(relocated_review_card_link(self.root, self.review, self.old))

    def test_requires_review_packet_and_unique_canonical_card(self) -> None:
        self.assertFalse(relocated_review_card_link(self.root, self.root / "docs/README.md", self.old))
        self.assertFalse(relocated_review_card_link(self.root, self.review, self.old.with_name("other.md")))
        duplicate = self.board / "60-done/BPR-0743-fixture.md"
        duplicate.parent.mkdir(parents=True)
        duplicate.write_text("duplicate\n", encoding="utf-8")
        self.assertFalse(relocated_review_card_link(self.root, self.review, self.old))

    def test_file_pinned_by_a_review_manifest_keeps_its_card_link(self) -> None:
        pinned = self.root / "docs/verification/BPR-0653-evidence/index.md"
        pinned.parent.mkdir(parents=True)
        pinned.write_text("evidence\n", encoding="utf-8")
        self.assertFalse(relocated_review_card_link(self.root, pinned, self.old))
        manifest = self.review.parent / "source-manifest.txt"
        manifest.write_text(
            "docs/verification/BPR-0653-evidence/index.md=" + "0" * 64 + "\n",
            encoding="utf-8",
        )
        self.assertTrue(relocated_review_card_link(self.root, pinned, self.old))


class ConflictMarkerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.card = self.root / "docs/issues/60-done/BPR-0001-fixture.md"
        self.card.parent.mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_a_setext_underline_is_not_a_marker(self) -> None:
        self.card.write_text("Title\n=======\n", encoding="utf-8")
        verify_no_conflict_markers(self.root)

    def test_a_diff3_base_line_is_refused(self) -> None:
        self.card.write_text("text\n||||||| parent of abc\nmore\n", encoding="utf-8")
        with self.assertRaises(SystemExit):
            verify_no_conflict_markers(self.root)


class RfcIndexTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.rfc_root = self.root / "docs/rfcs"
        self.rfc_root.mkdir(parents=True)
        self.rows: list[str] = []
        for number in range(1, 15):
            self.write_rfc(number, "Auto-Verified")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_rfc(self, number: int, status: str) -> Path:
        rfc_id = f"RFC-BPR-{number:04d}"
        path = self.rfc_root / f"{rfc_id}-fixture.md"
        path.write_text(f"---\nid: {rfc_id}\nstatus: {status}\n---\n", encoding="utf-8")
        self.rows.append(f"| [{rfc_id}]({path.name}) | Fixture | {status} | — |")
        return path

    def verify(self) -> None:
        (self.rfc_root / "README.md").write_text("\n".join(self.rows) + "\n", encoding="utf-8")
        verify_rfc_index(self.root)

    def test_accepts_original_pack_and_additional_statuses_together(self) -> None:
        self.write_rfc(18, "Deferred")
        self.write_rfc(24, "Proposed")
        self.write_rfc(25, "Auto-Verified")
        self.verify()

    def test_rejects_missing_original_rfc_even_with_fourteen_files(self) -> None:
        (self.rfc_root / "RFC-BPR-0007-fixture.md").unlink()
        self.write_rfc(24, "Deferred")
        with self.assertRaises(SystemExit):
            self.verify()

    def test_rejects_duplicate_id(self) -> None:
        path = self.write_rfc(24, "Deferred")
        path.with_name("RFC-BPR-0024-duplicate.md").write_text(path.read_text(), encoding="utf-8")
        self.rows.append(
            "| [RFC-BPR-0024](RFC-BPR-0024-duplicate.md) | Duplicate | Deferred | — |"
        )
        errors = StringIO()
        with redirect_stderr(errors), self.assertRaises(SystemExit):
            self.verify()
        self.assertIn("duplicate canonical RFC id", errors.getvalue())

    def test_rejects_additional_rfc_missing_from_index(self) -> None:
        self.write_rfc(24, "Deferred")
        self.rows.pop()
        with self.assertRaises(SystemExit):
            self.verify()

    def test_rejects_additional_rfc_status_mismatch(self) -> None:
        self.write_rfc(24, "Deferred")
        self.rows[-1] = self.rows[-1].replace("Deferred", "Auto-Verified")
        with self.assertRaises(SystemExit):
            self.verify()

    def test_rejects_index_link_to_a_different_rfc(self) -> None:
        self.write_rfc(24, "Deferred")
        self.rows[-1] = self.rows[-1].replace(
            "(RFC-BPR-0024-fixture.md)", "(RFC-BPR-0001-fixture.md)"
        )
        with self.assertRaises(SystemExit):
            self.verify()

    def test_rejects_filename_id_mismatch(self) -> None:
        path = self.write_rfc(24, "Deferred")
        path.rename(self.rfc_root / "RFC-BPR-0025-fixture.md")
        with self.assertRaises(SystemExit):
            self.verify()

    def test_rejects_reintroduced_future_folder(self) -> None:
        (self.rfc_root / "future").mkdir()
        with self.assertRaises(SystemExit):
            self.verify()


class IssueGraphTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.issue_root = self.root / "docs/issues"
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        context = self.root / ".mask/skill-config/implement-rfc"
        context.mkdir(parents=True)
        (context / "project.json").write_bytes(
            (REPOSITORY_ROOT / ".mask/skill-config/implement-rfc/project.json").read_bytes()
        )
        (context / "project.md").write_text("Fixture board context.\n", encoding="utf-8")
        lanes = (
            "10-backlog",
            "50-blocked",
            "40-parked",
            "20-ready",
            "00-inbox",
            "30-in-progress",
            "60-done",
        )
        for lane in lanes:
            (self.issue_root / lane).mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_card(
        self,
        issue_id: str,
        lane: str,
        status: str,
        dependencies: str,
        blocked_by: str | None = None,
        parked_on: str | None = None,
    ) -> None:
        blocked_line = f"blocked_by: {blocked_by}\n" if blocked_by else ""
        parked_line = f"parked_on: {parked_on}\n" if parked_on else ""
        (self.issue_root / lane / f"{issue_id}-fixture.md").write_text(
            "---\n"
            f"id: {issue_id}\n"
            "title: Fixture\n"
            f"status: {status}\n"
            "schema: implement-rfc.card.v1\n"
            "rfcs: []\n"
            f"depends_on: {json.dumps(dependencies.split(', ') if dependencies else [])}\n"
            f"{blocked_line}"
            f"{parked_line}"
            "---\n",
            encoding="utf-8",
        )

    def verify(self, next_id: str) -> None:
        (self.issue_root / "NEXT_ID").write_text(
            f"{next_id}\n",
            encoding="utf-8",
        )
        verify_issue_board(self.root)

    def test_accepts_completed_dependencies_and_backlog_frontier(self) -> None:
        self.write_card("BPR-0001", "60-done", "Done", "")
        self.write_card("BPR-0002", "20-ready", "Ready", "BPR-0001")
        self.write_card("BPR-0003", "10-backlog", "Backlog", "BPR-0002")

        self.verify("BPR-0004")

    def test_rejects_missing_dependency(self) -> None:
        self.write_card("BPR-0001", "10-backlog", "Backlog", "BPR-0099")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_ready_card_with_open_dependency(self) -> None:
        self.write_card("BPR-0001", "10-backlog", "Backlog", "")
        self.write_card("BPR-0002", "20-ready", "Ready", "BPR-0001")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")

    def test_rejects_dependency_cycle(self) -> None:
        self.write_card("BPR-0001", "10-backlog", "Backlog", "BPR-0002")
        self.write_card("BPR-0002", "10-backlog", "Backlog", "BPR-0001")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")

    def test_accepts_blocked_card_with_open_dependency(self) -> None:
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Blocked",
            "",
            blocked_by="upstream-toolchain-defect",
        )
        self.write_card(
            "BPR-0002",
            "50-blocked",
            "Blocked",
            "BPR-0001",
            blocked_by="upstream-toolchain-defect",
        )

        self.verify("BPR-0003")

    def test_platform_eligibility_does_not_waive_open_dependencies(self) -> None:
        self.write_card("BPR-0001", "20-ready", "Ready", "")
        self.write_card("BPR-0002", "20-ready", "Ready", "BPR-0001")
        path = self.issue_root / "20-ready/BPR-0002-fixture.md"
        path.write_text(path.read_text().replace("rfcs: []", 'rfcs: []\nplatforms: ["win"]'), encoding="utf-8")
        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")

    def test_rejects_platform_ready_folder(self) -> None:
        (self.issue_root / "21-ready-win").mkdir()
        self.write_card("BPR-0001", "21-ready-win", "Ready", "")
        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_blocked_card_without_blocked_by(self) -> None:
        self.write_card("BPR-0001", "50-blocked", "Blocked", "")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_accepts_a_real_blocker_reason(self) -> None:
        """`50-blocked/` is for something genuinely unresolved.

        The value is a free token rather than a closed set, because the
        interesting blockers are the ones nobody enumerated in advance.
        """
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Blocked",
            "",
            blocked_by="waiting-on-an-upstream-fix",
        )

        self.verify("BPR-0002")

    def test_rejects_a_platform_as_a_blocker(self) -> None:
        """A platform is a lane, not a blocker.

        Naming one here would hide the card from the session that could
        actually run it, which is the failure the platform lanes exist to fix.
        """
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Blocked",
            "",
            blocked_by="platform-windows",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_malformed_blocked_by_value(self) -> None:
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Blocked",
            "",
            blocked_by="Waiting On Something",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_blocked_by_outside_the_blocked_lane(self) -> None:
        self.write_card(
            "BPR-0001",
            "10-backlog",
            "Backlog",
            "",
            blocked_by="platform-windows",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_blocked_lane_status_mismatch(self) -> None:
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Backlog",
            "",
            blocked_by="platform-windows",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_ready_card_depending_on_a_blocked_card(self) -> None:
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Blocked",
            "",
            blocked_by="platform-linux",
        )
        self.write_card("BPR-0002", "20-ready", "Ready", "BPR-0001")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")

    def test_accepts_parked_card_with_completed_dependencies(self) -> None:
        self.write_card("BPR-0001", "60-done", "Done", "")
        self.write_card(
            "BPR-0002",
            "40-parked",
            "Parked",
            "BPR-0001",
            parked_on="human-review",
        )

        self.verify("BPR-0003")

    def test_rejects_parked_card_without_parked_on(self) -> None:
        self.write_card("BPR-0001", "40-parked", "Parked", "")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_unknown_parked_on_value(self) -> None:
        self.write_card(
            "BPR-0001",
            "40-parked",
            "Parked",
            "",
            parked_on="awaiting-hardware",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_parked_on_outside_the_parked_lane(self) -> None:
        self.write_card(
            "BPR-0001",
            "10-backlog",
            "Backlog",
            "",
            parked_on="awaiting-owner-verdict",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_parked_lane_status_mismatch(self) -> None:
        self.write_card(
            "BPR-0001",
            "40-parked",
            "Backlog",
            "",
            parked_on="awaiting-owner-verdict",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_parked_card_with_open_dependency(self) -> None:
        self.write_card("BPR-0001", "10-backlog", "Backlog", "")
        self.write_card(
            "BPR-0002",
            "40-parked",
            "Parked",
            "BPR-0001",
            parked_on="awaiting-owner-verdict",
        )

        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")

    def test_accepts_backlog_card_depending_on_a_parked_card(self) -> None:
        self.write_card(
            "BPR-0001",
            "40-parked",
            "Parked",
            "",
            parked_on="awaiting-live-github",
        )
        self.write_card("BPR-0002", "10-backlog", "Backlog", "BPR-0001")

        self.verify("BPR-0003")


class CloseoutContractTests(unittest.TestCase):
    """The light closeout contract: RFC base plus one row per criterion.

    Retired on 2026-09-01: the eight-column schema, gate-class and live-blocker
    tokens, anchored ledger links, per-row procedure links, placeholder and
    length policing, and the final-replay statement. They policed the record's
    shape rather than the acceptance and made every closeout cost a replay.
    """

    LIGHT_HEADER = "| Criterion | Check | Result | Commit |\n|---|---|---|---|\n"

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)
        for lane in ("10-backlog", "20-ready", "30-in-progress", "60-done"):
            (self.root / "docs/issues" / lane).mkdir(parents=True)
        (self.root / "docs/rfcs").mkdir(parents=True)
        self.rfc_id = "RFC-BPR-0999"
        self.base = "a" * 40
        self.card_path = (
            self.root / "docs/issues/10-backlog/BPR-0999-close-rfc-0999.md"
        )
        self.write_rfc()
        self.write_card()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_rfc(self) -> None:
        (self.root / "docs/rfcs/RFC-BPR-0999-fixture.md").write_text(
            "---\nid: RFC-BPR-0999\nstatus: Proposed\n---\n"
            "## 12. Acceptance criteria\n\n"
            "- First canonical criterion.\n"
            "- [ ] Second canonical criterion with\n"
            "  a multiline continuation.\n\n"
            "## Next section\n",
            encoding="utf-8",
        )

    def table(self, rows: list[list[str]] | None = None) -> str:
        effective_rows = rows or [
            [
                "First canonical criterion.",
                "`ctest --preset dev -R '^fixture$' --output-on-failure`",
                "Pass, 1/1",
                "`abc1234`",
            ],
            [
                "Second canonical criterion with a multiline continuation.",
                "Attended fixture interaction, recorded verdict",
                "Pass",
                "`abc1234`",
            ],
        ]
        return self.LIGHT_HEADER + "".join(
            "| " + " | ".join(row) + " |\n" for row in effective_rows
        )

    def write_card(
        self,
        *,
        base: str | None = None,
        table: str | None = None,
        heading: str = "## Closeout checklist",
    ) -> None:
        self.card_path.write_text(
            "---\n"
            "id: BPR-0999\n"
            "title: Verify and close RFC-BPR-0999\n"
            "status: Backlog\n"
            'schema: implement-rfc.card.v1\n'
            'rfcs: ["RFC-BPR-0999"]\n'
            'closeout_for: ["RFC-BPR-0999"]\n'
            "depends_on: []\n"
            "---\n\n"
            "# Verify and close RFC-BPR-0999\n\n"
            f"RFC base: `{base or self.base}`.\n\n"
            f"{heading}\n\n"
            f"{table if table is not None else self.table()}",
            encoding="utf-8",
        )

    def accept(self) -> None:
        verify_terminal_closeout_cards(self.root)

    def reject(self) -> None:
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            verify_terminal_closeout_cards(self.root)

    def test_accepts_light_contract(self) -> None:
        self.accept()

    def test_accepts_reordered_and_extra_rows(self) -> None:
        rows = [
            ["Second canonical criterion with a multiline continuation.", "check", "Pass", "`abc1234`"],
            ["An extra row the RFC did not name.", "check", "Pass", "`abc1234`"],
            ["First canonical criterion.", "check", "Pass", "`abc1234`"],
        ]
        self.write_card(table=self.table(rows))
        self.accept()

    def test_accepts_the_retired_eight_column_shape(self) -> None:
        header = (
            "| State | Criterion | Direct verification surface | Command or attended "
            "interaction | Expected result | Durable evidence location | Gate class | "
            "Live blocker |\n|---|---|---|---|---|---|---|---|\n"
        )
        rows = "".join(
            f"| [ ] | {criterion} | surface | command | result | evidence | automated | none |\n"
            for criterion in (
                "First canonical criterion.",
                "Second canonical criterion with a multiline continuation.",
            )
        )
        self.write_card(table=header + rows)
        self.accept()

    def test_accepts_markdown_code_normalization_in_criterion(self) -> None:
        rows = [
            ["`First` canonical criterion.", "check", "Pass", "`abc1234`"],
            ["Second canonical criterion with a multiline continuation.", "check", "Pass", "`abc1234`"],
        ]
        self.write_card(table=self.table(rows))
        self.accept()

    def test_rejects_missing_base(self) -> None:
        self.card_path.write_text(
            self.card_path.read_text(encoding="utf-8").replace(
                f"RFC base: `{self.base}`.\n\n", ""
            ),
            encoding="utf-8",
        )
        self.reject()

    def test_rejects_missing_checklist(self) -> None:
        self.write_card(heading="## Something else")
        self.reject()

    def test_rejects_table_without_criterion_column(self) -> None:
        self.write_card(
            table="| Claim | Result |\n|---|---|\n| First canonical criterion. | Pass |\n"
        )
        self.reject()

    def test_rejects_empty_cell(self) -> None:
        rows = [
            ["First canonical criterion.", "", "Pass", "`abc1234`"],
            ["Second canonical criterion with a multiline continuation.", "check", "Pass", "`abc1234`"],
        ]
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_omitted_criterion(self) -> None:
        rows = [["First canonical criterion.", "check", "Pass", "`abc1234`"]]
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_altered_criterion(self) -> None:
        rows = [
            ["First canonical criterion, reworded.", "check", "Pass", "`abc1234`"],
            ["Second canonical criterion with a multiline continuation.", "check", "Pass", "`abc1234`"],
        ]
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_future_terminal_card_without_schema(self) -> None:
        self.card_path.write_text(
            "---\n"
            "id: BPR-0999\n"
            "title: Close RFC-BPR-0999\n"
            "status: Backlog\n"
            'schema: implement-rfc.card.v1\n'
            'rfcs: ["RFC-BPR-0999"]\n'
            'closeout_for: ["RFC-BPR-0999"]\n'
            "depends_on: []\n"
            "---\n\nNo terminal schema.\n",
            encoding="utf-8",
        )
        self.reject()

    def test_rejects_terminal_title_with_noncanonical_filename(self) -> None:
        noncanonical_path = self.card_path.with_name("BPR-0999-terminal.md")
        noncanonical_path.write_text(
            self.card_path.read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        self.card_path.unlink()
        self.reject()

    def test_rejects_closeout_rfc_metadata_mismatch(self) -> None:
        self.card_path.write_text(
            self.card_path.read_text(encoding="utf-8").replace(
                'rfcs: ["RFC-BPR-0999"]',
                'rfcs: ["RFC-BPR-0001"]',
            ),
            encoding="utf-8",
        )
        self.reject()


class OperatorPacketRecordTests(unittest.TestCase):
    """A superseded packet must stop calling itself pending, and an answered one
    must never be edited at all."""

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.review_root = self.root / "docs/verification/reviews"
        self.review_root.mkdir(parents=True)
        for lane in (
            "10-backlog",
            "50-blocked",
            "40-parked",
            "20-ready",
            "00-inbox",
            "30-in-progress",
        ):
            (self.root / "docs/issues" / lane).mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_packet(self, gate: str, status: str) -> Path:
        directory = self.review_root / gate
        directory.mkdir(exist_ok=True)
        packet = directory / "operator-packet.md"
        packet.write_text(
            f"# {gate} operator packet\n\nStatus: **{status}**\n\nBody.\n",
            encoding="utf-8",
        )
        return packet

    def write_card(self, issue_id: str, lane: str, body: str) -> None:
        (self.root / "docs/issues" / lane / f"{issue_id}-fixture.md").write_text(
            "---\n"
            f"id: {issue_id}\n"
            "title: Fixture\n"
            "status: Parked\n"
            "schema: implement-rfc.card.v1\n"
            "rfcs: []\n"
            "depends_on: []\n"
            f"---\n\n{body}\n",
            encoding="utf-8",
        )

    def bind_packet(self, gate: str) -> None:
        """Record the packet's exact bytes the way an answered gate does."""
        packet = self.review_root / gate / "operator-packet.md"
        content = packet.read_bytes()
        (self.review_root / gate / "validation-record.json").write_text(
            json.dumps(
                {
                    "gate": gate,
                    "reviewed_packet": {
                        "path": f"docs/verification/reviews/{gate}/operator-packet.md",
                        "sha256": f"sha256:{hashlib.sha256(content).hexdigest()}",
                        "byte_length": len(content),
                    },
                }
            ),
            encoding="utf-8",
        )

    def bind_packet_response(self, gate: str, digest: str | None = None) -> None:
        """Record an exact sibling response binding without a validation record."""
        packet = self.review_root / gate / "operator-packet.md"
        packet_digest = digest or hashlib.sha256(packet.read_bytes()).hexdigest()
        (self.review_root / gate / "operator-response.md").write_text(
            "# operator response\n\n"
            f"Packet SHA-256: `{packet_digest}`\n\n"
            "## Exact operator decision\n\n> approved\n",
            encoding="utf-8",
        )

    def reject(self) -> None:
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            verify_operator_packet_records(self.root)

    # ---- BPR-0411: a pending packet's manifest must describe the tree ----

    def write_manifest(self, gate: str, entries: dict[str, bytes]) -> None:
        """Write source files and a manifest naming them, the way a packet does."""
        lines = []
        for relative, content in entries.items():
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            lines.append(f"{relative}={hashlib.sha256(content).hexdigest()}")
        (self.review_root / gate / "source-manifest.txt").write_text(
            "\n".join(lines) + "\n", encoding="utf-8")

    def test_accepts_a_pending_manifest_that_matches_the_tree(self) -> None:
        self.write_packet("gate-v001", "pending operator decision")
        self.write_manifest("gate-v001", {"src/thing.txt": b"exact bytes\n"})

        verify_pending_packet_manifests(self.root)

    def test_rejects_a_pending_manifest_that_disagrees_with_the_tree(self) -> None:
        """v002 shipped stale at birth and nothing caught it. This is that check."""
        self.write_packet("gate-v001", "pending operator decision")
        self.write_manifest("gate-v001", {"src/thing.txt": b"exact bytes\n"})
        (self.root / "src/thing.txt").write_bytes(b"different bytes\n")

        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            verify_pending_packet_manifests(self.root)

    def test_rejects_a_pending_manifest_naming_a_missing_file(self) -> None:
        self.write_packet("gate-v001", "pending operator decision")
        self.write_manifest("gate-v001", {"src/thing.txt": b"exact bytes\n"})
        (self.root / "src/thing.txt").unlink()

        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            verify_pending_packet_manifests(self.root)

    def test_ignores_a_retired_packets_manifest(self) -> None:
        """A superseded or invalidated packet describes a past tree on purpose.

        The first draft of this gate failed v001 for exactly that, because it
        keyed on `frozen` rather than on the status line — and a retired packet
        never earns a validation record either.
        """
        self.write_packet("gate-v001", "superseded by gate-v002; do not run")
        self.write_manifest("gate-v001", {"src/thing.txt": b"exact bytes\n"})
        (self.root / "src/thing.txt").write_bytes(b"the tree moved on\n")

        verify_pending_packet_manifests(self.root)

    def test_accepts_a_family_whose_only_pending_packet_is_the_live_one(self) -> None:
        self.write_packet("gate-v001", "superseded by gate-v002; do not run")
        self.write_packet("gate-v002", "pending operator run")

        verify_operator_packet_records(self.root)

    def test_rejects_a_superseded_packet_still_claiming_pending(self) -> None:
        self.write_packet("gate-v001", "pending operator run")
        self.write_packet("gate-v002", "pending operator run")

        self.reject()

    def test_rejects_a_packet_without_a_status_line(self) -> None:
        (self.review_root / "gate-v001").mkdir()
        (self.review_root / "gate-v001/operator-packet.md").write_text(
            "# gate-v001 operator packet\n\nNo status.\n",
            encoding="utf-8",
        )

        self.reject()

    def test_accepts_an_answered_superseded_packet_that_still_reads_pending(
        self,
    ) -> None:
        """A digest-bound packet is frozen evidence: it keeps the question as
        asked, and editing it would break the record of what the operator saw."""
        self.write_packet("gate-v001", "pending operator decision")
        self.bind_packet("gate-v001")
        self.write_packet("gate-v002", "pending operator decision")

        verify_operator_packet_records(self.root)

    def test_accepts_a_response_bound_superseded_packet_still_reading_pending(
        self,
    ) -> None:
        self.write_packet("gate-v001", "pending operator decision")
        self.bind_packet_response("gate-v001")
        self.write_packet("gate-v002", "pending operator decision")

        verify_operator_packet_records(self.root)

    def test_rejects_a_response_bound_to_different_packet_bytes(self) -> None:
        self.write_packet("gate-v001", "pending operator decision")
        self.bind_packet_response("gate-v001", "f" * 64)
        self.write_packet("gate-v002", "pending operator decision")

        self.reject()

    def test_rejects_an_edited_digest_bound_packet(self) -> None:
        self.write_packet("gate-v001", "pending operator decision")
        self.bind_packet("gate-v001")
        self.write_packet("gate-v001", "answered; approved")

        self.reject()

    def test_rejects_an_open_card_naming_only_a_superseded_packet(self) -> None:
        self.write_packet("gate-v001", "superseded by gate-v002; do not run")
        self.write_packet("gate-v002", "pending operator run")
        self.write_card("BPR-0001", "40-parked", "Run the procedure in gate-v001.")

        self.reject()

    def test_accepts_an_open_card_naming_the_live_packet_alongside_its_history(
        self,
    ) -> None:
        self.write_packet("gate-v001", "superseded by gate-v002; do not run")
        self.write_packet("gate-v002", "pending operator run")
        self.write_card(
            "BPR-0001",
            "40-parked",
            "gate-v001 was never run. The live procedure is gate-v002.",
        )

        verify_operator_packet_records(self.root)

    def test_ignores_a_reference_to_a_directory_holding_no_packet(self) -> None:
        """Not every review directory is an operator packet; log and capture
        directories are versioned the same way and carry no status line."""
        self.write_packet("gate-v001", "pending operator run")
        (self.review_root / "capture-v001").mkdir()
        (self.review_root / "capture-v001/run.log").write_text("x\n", encoding="utf-8")
        self.write_card("BPR-0001", "40-parked", "See capture-v001 and gate-v001.")

        verify_operator_packet_records(self.root)


class FrontMatterQuotingTests(unittest.TestCase):
    """The `---` fence says the block is YAML, so both readers must agree.

    This parser keeps everything after the first colon and reads
    `title: Core: buckets and reasons` correctly. A YAML reader rejects the
    whole block and returns no front matter at all, which is why a board
    inventory in the [[BPR-0467]] session reported a card as missing the `id`
    it plainly has. Refuse the inputs the two read differently.
    """

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write(self, body: str) -> Path:
        path = self.root / "card.md"
        path.write_text(f"---\n{body}---\n\n# Card\n", encoding="utf-8")
        return path

    def test_rejects_an_unquoted_colon(self) -> None:
        path = self.write(
            "id: BPR-0001\ntitle: Core: buckets and reasons\nstatus: Done\n"
        )

        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            front_matter(path)

    def test_accepts_a_quoted_colon(self) -> None:
        path = self.write(
            'id: BPR-0001\ntitle: "Core: buckets and reasons"\nstatus: Done\n'
        )

        self.assertEqual(
            front_matter(path)["title"], "Core: buckets and reasons"
        )

    def test_accepts_an_ordinary_value(self) -> None:
        path = self.write("id: BPR-0001\ntitle: Buckets and reasons\n")

        self.assertEqual(front_matter(path)["title"], "Buckets and reasons")

    def test_accepts_a_flow_sequence(self) -> None:
        path = self.write('id: BPR-0001\ndepends_on: ["BPR-0002", "BPR-0003"]\n')

        self.assertEqual(
            front_matter(path)["depends_on"], ["BPR-0002", "BPR-0003"]
        )

    def test_accepts_a_colon_without_following_space(self) -> None:
        # YAML accepts this too: only a colon followed by whitespace starts a
        # nested mapping. Refusing it would demand quoting that buys nothing.
        path = self.write("id: BPR-0001\nsource: https://example.invalid/x\n")

        self.assertEqual(
            front_matter(path)["source"], "https://example.invalid/x"
        )

    def test_rejects_a_trailing_colon(self) -> None:
        path = self.write("id: BPR-0001\ntitle: See the section below:\n")

        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            front_matter(path)


if __name__ == "__main__":
    unittest.main()
