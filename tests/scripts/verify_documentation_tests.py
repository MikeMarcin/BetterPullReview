#!/usr/bin/env python3
from __future__ import annotations

import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.verify_documentation import (  # noqa: E402
    CLOSEOUT_COLUMNS,
    CLOSEOUT_NO_LIVE_BLOCKER,
    CLOSEOUT_REPLAY_STATEMENT,
    verify_issue_board,
    verify_terminal_closeout_cards,
)


class IssueGraphTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.issue_root = self.root / "docs/issues"
        lanes = (
            "10-backlog",
            "50-blocked",
            "40-parked",
            "20-ready",
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
            "rfc: RFC-BPR-TEST\n"
            f"depends_on: [{dependencies}]\n"
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
            blocked_by="platform-windows",
        )
        self.write_card(
            "BPR-0002",
            "50-blocked",
            "Blocked",
            "BPR-0001",
            blocked_by="platform-windows",
        )

        self.verify("BPR-0003")

    def test_rejects_blocked_card_without_blocked_by(self) -> None:
        self.write_card("BPR-0001", "50-blocked", "Blocked", "")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_unknown_blocked_by_value(self) -> None:
        self.write_card(
            "BPR-0001",
            "50-blocked",
            "Blocked",
            "",
            blocked_by="waiting-on-the-product-owner",
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
            parked_on="awaiting-product-decision",
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
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        for lane in ("10-backlog", "20-ready", "30-in-progress", "60-done"):
            (self.root / "docs/issues" / lane).mkdir(parents=True)
        (self.root / "docs/rfcs").mkdir(parents=True)
        (self.root / "docs/verification").mkdir(parents=True)
        self.rfc_id = "RFC-BPR-0999"
        self.base = "a" * 40
        self.card_path = (
            self.root
            / "docs/issues/10-backlog/BPR-0999-close-rfc-0999.md"
        )
        self.blocker_path = (
            self.root / "docs/issues/10-backlog/BPR-0998-attended-blocker.md"
        )
        self.done_blocker_path = (
            self.root / "docs/issues/60-done/BPR-0997-done-blocker.md"
        )
        self.write_rfc()
        self.write_ledger()
        self.write_blocker(self.blocker_path, "Backlog")
        self.write_blocker(self.done_blocker_path, "Done")
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

    def write_ledger(
        self,
        base: str | None = None,
        headings: tuple[str, ...] = ("Claim matrix",),
    ) -> None:
        (self.root / "docs/verification/RFC-BPR-0999.md").write_text(
            f"# Ledger\n\nRFC base: `{base or self.base}`\n\n"
            + "".join(f"## {heading}\n" for heading in headings),
            encoding="utf-8",
        )

    def write_blocker(
        self,
        path: Path,
        status: str,
        *,
        procedure_heading: str = "Required direct evidence",
        procedure_body: str = (
            "Run the exact fixture interaction, compare the visible projection "
            "with the recorded server state, and retain the resulting verdict "
            "artifact for final-head replay."
        ),
    ) -> None:
        path.write_text(
            "---\n"
            f"id: {path.name[:8]}\n"
            "title: Attended blocker\n"
            f"status: {status}\n"
            "rfc: RFC-BPR-0999\n"
            "depends_on: []\n"
            "---\n\n"
            "# Attended blocker\n\n"
            f"## {procedure_heading}\n\n"
            f"{procedure_body}\n",
            encoding="utf-8",
        )

    def table(self, rows: list[list[str]] | None = None) -> str:
        effective_rows = rows or [
            [
                "[ ]",
                "First canonical criterion.",
                "Fixture executable command and projection assertion surface",
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "The focused fixture reports the first canonical projection "
                "result exactly once.",
                "First fixture row in the [RFC-BPR-0999 verification ledger]"
                "(../../verification/RFC-BPR-0999.md#claim-matrix); final replay "
                "record keyed by “First canonical criterion.”",
                "automated",
                CLOSEOUT_NO_LIVE_BLOCKER,
            ],
            [
                "[ ]",
                "Second canonical criterion with a multiline continuation.",
                "Attended fixture control and visible projection review surface",
                "Perform [BPR-0998](BPR-0998-attended-blocker.md"
                "#required-direct-evidence)'s attended fixture interaction and "
                "record its visible result",
                "The reviewer observes the complete multiline criterion result "
                "and records one verdict.",
                "Second fixture row in the [RFC-BPR-0999 verification ledger]"
                "(../../verification/RFC-BPR-0999.md#claim-matrix); final replay "
                "record keyed by “Second canonical criterion with a multiline "
                "continuation.”",
                "owner-attended",
                "[BPR-0998](BPR-0998-attended-blocker.md)",
            ],
        ]
        header = "| " + " | ".join(CLOSEOUT_COLUMNS) + " |\n"
        separator = "|" + "|".join("---" for _ in CLOSEOUT_COLUMNS) + "|\n"
        body = "".join("| " + " | ".join(row) + " |\n" for row in effective_rows)
        return header + separator + body

    def write_card(
        self,
        *,
        base: str | None = None,
        table: str | None = None,
        replay: str | None = None,
        dependencies: str = "BPR-0998",
    ) -> None:
        self.card_path.write_text(
            "---\n"
            "id: BPR-0999\n"
            "title: Verify and close RFC-BPR-0999\n"
            "status: Backlog\n"
            "rfc: RFC-BPR-0999\n"
            f"depends_on: [{dependencies}]\n"
            "---\n\n"
            "# Verify and close RFC-BPR-0999\n\n"
            f"RFC base: `{base or self.base}`.\n\n"
            "## Closeout checklist\n\n"
            f"{replay or CLOSEOUT_REPLAY_STATEMENT}\n\n"
            f"{table if table is not None else self.table()}",
            encoding="utf-8",
        )

    def reject(self) -> None:
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            verify_terminal_closeout_cards(self.root)

    def test_accepts_exact_numbered_checkbox_multiline_contract(self) -> None:
        verify_terminal_closeout_cards(self.root)

    def test_rejects_missing_base(self) -> None:
        self.card_path.write_text(
            self.card_path.read_text(encoding="utf-8").replace(
                f"RFC base: `{self.base}`.\n\n", ""
            ),
            encoding="utf-8",
        )
        self.reject()

    def test_rejects_mismatched_base(self) -> None:
        self.write_card(base="b" * 40)
        self.reject()

    def test_rejects_missing_checklist(self) -> None:
        self.card_path.write_text(
            self.card_path.read_text(encoding="utf-8").replace(
                "## Closeout checklist", "## Evidence"
            ),
            encoding="utf-8",
        )
        self.reject()

    def test_rejects_wrong_columns(self) -> None:
        self.write_card(table=self.table().replace("Expected result", "Outcome"))
        self.reject()

    def test_rejects_empty_cell(self) -> None:
        rows = [row.split(" | ") for row in self.table().splitlines()[2:]]
        rows[0] = [cell.strip("| ") for cell in rows[0]]
        rows[1] = [cell.strip("| ") for cell in rows[1]]
        rows[0][2] = ""
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_altered_criterion(self) -> None:
        self.write_card(table=self.table().replace("First canonical", "Altered"))
        self.reject()

    def test_rejects_omitted_criterion(self) -> None:
        rows = [
            [
                "[ ]", "First canonical criterion.", "Surface", "Command",
                "Expected", "Evidence", "automated", "No blocker",
            ]
        ]
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_duplicate_criterion(self) -> None:
        rows = [
            [
                "[ ]", "First canonical criterion.", "Surface", "Command",
                "Expected", "Evidence", "automated", "No blocker",
            ],
            [
                "[ ]", "First canonical criterion.", "Surface", "Command",
                "Expected", "Evidence", "automated", "No blocker",
            ],
        ]
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_extra_criterion(self) -> None:
        rows = [row.split(" | ") for row in self.table().splitlines()[2:]]
        rows = [[cell.strip("| ") for cell in row] for row in rows]
        rows.append(
            [
                "[ ]", "Extra criterion.", "Surface", "Command", "Expected",
                "Evidence", "automated", "No blocker",
            ]
        )
        self.write_card(table=self.table(rows))
        self.reject()

    def test_rejects_reordered_criteria(self) -> None:
        rows = [row.split(" | ") for row in self.table().splitlines()[2:]]
        rows = [[cell.strip("| ") for cell in row] for row in rows]
        self.write_card(table=self.table(list(reversed(rows))))
        self.reject()

    def test_accepts_escaped_pipe_in_canonical_criterion(self) -> None:
        rfc_path = self.root / "docs/rfcs/RFC-BPR-0999-fixture.md"
        rfc_path.write_text(
            rfc_path.read_text(encoding="utf-8").replace(
                "First canonical criterion.",
                "First canonical | criterion.",
            ),
            encoding="utf-8",
        )
        self.write_card(
            table=self.table().replace(
                "First canonical criterion.",
                r"First canonical \| criterion.",
            )
        )
        verify_terminal_closeout_cards(self.root)

    def test_accepts_markdown_code_normalization_in_evidence_key(self) -> None:
        rfc_path = self.root / "docs/rfcs/RFC-BPR-0999-fixture.md"
        rfc_path.write_text(
            rfc_path.read_text(encoding="utf-8").replace(
                "First canonical criterion.",
                "First `canonical` criterion.",
            ),
            encoding="utf-8",
        )
        self.write_card(
            table=self.table().replace(
                "First canonical criterion.",
                "First `canonical` criterion.",
                1,
            )
        )
        verify_terminal_closeout_cards(self.root)

    def test_rejects_checked_open_row(self) -> None:
        self.write_card(table=self.table().replace("| [ ] |", "| [x] |", 1))
        self.reject()

    def test_rejects_unknown_gate_class(self) -> None:
        self.write_card(table=self.table().replace("automated", "manual", 1))
        self.reject()

    def test_rejects_nonautomated_missing_blocker_link(self) -> None:
        self.write_card(
            table=self.table().replace(
                "[BPR-0998](BPR-0998-attended-blocker.md)", "Pending"
            )
        )
        self.reject()

    def test_rejects_nonautomated_done_blocker_link(self) -> None:
        self.write_card(
            table=self.table().replace(
                "[BPR-0998](BPR-0998-attended-blocker.md)",
                "[BPR-0997](../60-done/BPR-0997-done-blocker.md)",
            )
        )
        self.reject()

    def test_rejects_nonautomated_unrelated_open_blocker(self) -> None:
        unrelated_path = (
            self.root / "docs/issues/10-backlog/BPR-0996-unrelated-blocker.md"
        )
        self.write_blocker(unrelated_path, "Backlog")
        self.write_card(
            table=self.table().replace(
                "[BPR-0998](BPR-0998-attended-blocker.md)",
                "[BPR-0996](BPR-0996-unrelated-blocker.md)",
            )
        )
        self.reject()

    def test_rejects_one_character_direct_surface(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Fixture executable command and projection assertion surface",
                "x",
            )
        )
        self.reject()

    def test_rejects_one_character_command(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "x",
            )
        )
        self.reject()

    def test_rejects_one_character_expected_result(self) -> None:
        self.write_card(
            table=self.table().replace(
                "The focused fixture reports the first canonical projection result "
                "exactly once.",
                "x",
            )
        )
        self.reject()

    def test_rejects_one_character_evidence_location(self) -> None:
        rows = self.table().splitlines()
        first = rows[2].split(" | ")
        first[5] = "x"
        rows[2] = " | ".join(first)
        self.write_card(table="\n".join(rows) + "\n")
        self.reject()

    def test_rejects_one_character_live_blocker(self) -> None:
        self.write_card(
            table=self.table().replace(CLOSEOUT_NO_LIVE_BLOCKER, "x", 1)
        )
        self.reject()

    def test_rejects_wrong_ledger_link(self) -> None:
        self.write_card(
            table=self.table().replace(
                "../../verification/RFC-BPR-0999.md#claim-matrix",
                "../../verification/RFC-BPR-0001.md#claim-matrix",
                1,
            )
        )
        self.reject()

    def test_rejects_ledger_link_without_anchor(self) -> None:
        self.write_card(
            table=self.table().replace(
                "../../verification/RFC-BPR-0999.md#claim-matrix",
                "../../verification/RFC-BPR-0999.md",
                1,
            )
        )
        self.reject()

    def test_rejects_ledger_link_with_nonexistent_anchor(self) -> None:
        self.write_card(
            table=self.table().replace(
                "../../verification/RFC-BPR-0999.md#claim-matrix",
                "../../verification/RFC-BPR-0999.md#does-not-exist",
                1,
            )
        )
        self.reject()

    def test_rejects_ledger_heading_inside_html_comment(self) -> None:
        ledger_path = self.root / "docs/verification/RFC-BPR-0999.md"
        ledger_path.write_text(
            ledger_path.read_text(encoding="utf-8").replace(
                "## Claim matrix",
                "<!--\n## Claim matrix\n-->",
            ),
            encoding="utf-8",
        )
        self.reject()

    def test_accepts_direct_claim_matrix_anchor(self) -> None:
        self.write_ledger(headings=("Direct claim matrix",))
        self.write_card(
            table=self.table().replace("#claim-matrix", "#direct-claim-matrix")
        )
        verify_terminal_closeout_cards(self.root)

    def test_accepts_duplicate_heading_suffix_anchor(self) -> None:
        self.write_ledger(headings=("Claim matrix", "Claim matrix"))
        self.write_card(
            table=self.table().replace("#claim-matrix", "#claim-matrix-1")
        )
        verify_terminal_closeout_cards(self.root)

    def test_rejects_evidence_with_criterion_outside_wrong_key(self) -> None:
        self.write_card(
            table=self.table().replace(
                "record keyed by “First canonical criterion.”",
                "notes mention First canonical criterion.; final replay record "
                "keyed by “A different criterion.”",
                1,
            )
        )
        self.reject()

    def test_rejects_non_action_command(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "The fixture uses `python3 scripts/verify_documentation.py "
                "--root .` for deterministic projection output",
            )
        )
        self.reject()

    def test_rejects_action_without_command_or_procedure(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "Run alpha beta gamma delta epsilon operation",
            )
        )
        self.reject()

    def test_rejects_nonallowlisted_backticked_executable(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "`x` followed by filler words for a pretend repository operation",
            )
        )
        self.reject()

    def test_accepts_exact_ctest_command(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "`ctest --preset dev -R '^documentation_structure$' "
                "--output-on-failure`",
            )
        )
        verify_terminal_closeout_cards(self.root)

    def test_accepts_exact_documentation_command(self) -> None:
        verify_terminal_closeout_cards(self.root)

    def test_accepts_exact_format_and_diff_command(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "`cmake --build --preset dev --target coding-format-check "
                "coding-tidy && git diff --check`",
            )
        )
        verify_terminal_closeout_cards(self.root)

    def test_rejects_ctest_unknown_option(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "`ctest --preset dev -R '^documentation_structure$' "
                "--output-on-failure --verbose`",
            )
        )
        self.reject()

    def test_rejects_ctest_trailing_composition(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "`ctest --preset dev -R '^documentation_structure$' "
                "--output-on-failure && bogus`",
            )
        )
        self.reject()

    def test_accepts_linked_dependency_procedure(self) -> None:
        verify_terminal_closeout_cards(self.root)

    def test_rejects_plain_dependency_id_as_procedure(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Perform [BPR-0998](BPR-0998-attended-blocker.md"
                "#required-direct-evidence)'s attended fixture interaction and "
                "record its visible result",
                "Run BPR-0998 bananas through the attended fixture procedure",
            )
        )
        self.reject()

    def test_rejects_procedure_link_without_anchor(self) -> None:
        self.write_card(
            table=self.table().replace(
                "BPR-0998-attended-blocker.md#required-direct-evidence",
                "BPR-0998-attended-blocker.md",
                1,
            )
        )
        self.reject()

    def test_rejects_nonexistent_procedure_anchor(self) -> None:
        self.write_card(
            table=self.table().replace(
                "#required-direct-evidence",
                "#does-not-exist",
                1,
            )
        )
        self.reject()

    def test_rejects_procedure_heading_inside_html_comment(self) -> None:
        blocker_text = self.blocker_path.read_text(encoding="utf-8")
        self.blocker_path.write_text(
            blocker_text.replace(
                "## Required direct evidence",
                "<!--\n## Required direct evidence",
            ).rstrip()
            + "\n-->\n",
            encoding="utf-8",
        )
        self.reject()

    def test_rejects_nonaction_procedure_anchor(self) -> None:
        self.write_blocker(
            self.blocker_path,
            "Backlog",
            procedure_heading="Background",
        )
        self.write_card(
            table=self.table().replace(
                "#required-direct-evidence",
                "#background",
                1,
            )
        )
        self.reject()

    def test_rejects_done_owned_procedure(self) -> None:
        self.write_card(
            dependencies="BPR-0998, BPR-0997",
            table=self.table().replace(
                "[BPR-0998](BPR-0998-attended-blocker.md"
                "#required-direct-evidence)",
                "[BPR-0997](../60-done/BPR-0997-done-blocker.md"
                "#required-direct-evidence)",
                1,
            ),
        )
        self.reject()

    def test_rejects_unowned_procedure_reference(self) -> None:
        unrelated_path = (
            self.root / "docs/issues/10-backlog/BPR-0996-unrelated-blocker.md"
        )
        self.write_blocker(unrelated_path, "Backlog")
        self.write_card(
            table=self.table().replace(
                "[BPR-0998](BPR-0998-attended-blocker.md"
                "#required-direct-evidence)",
                "[BPR-0996](BPR-0996-unrelated-blocker.md"
                "#required-direct-evidence)",
                1,
            )
        )
        self.reject()

    def test_rejects_nonsubstantive_procedure_section(self) -> None:
        self.write_blocker(
            self.blocker_path,
            "Backlog",
            procedure_body="x",
        )
        self.reject()

    def test_accepts_automated_dependency_blocker(self) -> None:
        self.write_card(
            table=self.table().replace(
                CLOSEOUT_NO_LIVE_BLOCKER,
                "[BPR-0998](BPR-0998-attended-blocker.md) remains open.",
                1,
            )
        )
        verify_terminal_closeout_cards(self.root)

    def test_rejects_automated_unrelated_open_blocker(self) -> None:
        unrelated_path = (
            self.root / "docs/issues/10-backlog/BPR-0996-unrelated-blocker.md"
        )
        self.write_blocker(unrelated_path, "Backlog")
        self.write_card(
            table=self.table().replace(
                CLOSEOUT_NO_LIVE_BLOCKER,
                "[BPR-0996](BPR-0996-unrelated-blocker.md) remains open.",
                1,
            )
        )
        self.reject()

    def test_rejects_automated_done_blocker(self) -> None:
        self.write_card(
            table=self.table().replace(
                CLOSEOUT_NO_LIVE_BLOCKER,
                "[BPR-0997](../60-done/BPR-0997-done-blocker.md) remains open.",
                1,
            )
        )
        self.reject()

    def test_rejects_automated_arbitrary_blocker(self) -> None:
        self.write_card(
            table=self.table().replace(
                CLOSEOUT_NO_LIVE_BLOCKER,
                "An arbitrary future task remains pending.",
                1,
            )
        )
        self.reject()

    def test_rejects_automated_self_blocker(self) -> None:
        self.write_card(
            table=self.table().replace(
                CLOSEOUT_NO_LIVE_BLOCKER,
                "[BPR-0999](BPR-0999-close-rfc-0999.md) remains open.",
                1,
            )
        )
        self.reject()

    def test_rejects_generic_direct_surface_placeholder(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Fixture executable command and projection assertion surface",
                "RFC-BPR-0999 criterion 1 through the direct architecture, "
                "runtime, or authority boundary named by the criterion",
            )
        )
        self.reject()

    def test_rejects_generic_full_matrix_command_placeholder(self) -> None:
        self.write_card(
            table=self.table().replace(
                "Run `python3 scripts/verify_documentation.py --root .` and "
                "inspect its projection assertion",
                "`cmake --preset dev && cmake --build --preset dev && "
                "ctest --preset dev --output-on-failure`",
            )
        )
        self.reject()

    def test_rejects_generic_expected_result_placeholder(self) -> None:
        self.write_card(
            table=self.table().replace(
                "The focused fixture reports the first canonical projection result "
                "exactly once.",
                "The final-head replay demonstrates the criterion exactly as "
                "written, without historical-count or proxy substitution.",
            )
        )
        self.reject()

    def test_rejects_generic_evidence_location_placeholder(self) -> None:
        self.write_card(
            table=self.table().replace(
                "First fixture row in the [RFC-BPR-0999 verification ledger]"
                "(../../verification/RFC-BPR-0999.md#claim-matrix); final replay "
                "record keyed by “First canonical criterion.”",
                "Fixture ledger, criterion-specific final-head row",
            )
        )
        self.reject()

    def test_rejects_future_terminal_card_without_schema(self) -> None:
        self.card_path.write_text(
            "---\n"
            "id: BPR-0999\n"
            "title: Close RFC-BPR-0999\n"
            "status: Backlog\n"
            "rfc: RFC-BPR-0999\n"
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
                "rfc: RFC-BPR-0999",
                "rfc: RFC-BPR-0001",
            ),
            encoding="utf-8",
        )
        self.reject()


if __name__ == "__main__":
    unittest.main()
