#!/usr/bin/env python3
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.verify_documentation import verify_issue_board  # noqa: E402


class IssueGraphTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.issue_root = self.root / "docs/issues"
        for lane in ("backlog", "ready", "in-progress", "done"):
            (self.issue_root / lane).mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_card(
        self,
        issue_id: str,
        lane: str,
        status: str,
        dependencies: str,
    ) -> None:
        (self.issue_root / lane / f"{issue_id}-fixture.md").write_text(
            "---\n"
            f"id: {issue_id}\n"
            "title: Fixture\n"
            f"status: {status}\n"
            "rfc: RFC-BPR-TEST\n"
            f"depends_on: [{dependencies}]\n"
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
        self.write_card("BPR-0001", "done", "Done", "")
        self.write_card("BPR-0002", "ready", "Ready", "BPR-0001")
        self.write_card("BPR-0003", "backlog", "Backlog", "BPR-0002")

        self.verify("BPR-0004")

    def test_rejects_missing_dependency(self) -> None:
        self.write_card("BPR-0001", "backlog", "Backlog", "BPR-0099")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0002")

    def test_rejects_ready_card_with_open_dependency(self) -> None:
        self.write_card("BPR-0001", "backlog", "Backlog", "")
        self.write_card("BPR-0002", "ready", "Ready", "BPR-0001")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")

    def test_rejects_dependency_cycle(self) -> None:
        self.write_card("BPR-0001", "backlog", "Backlog", "BPR-0002")
        self.write_card("BPR-0002", "backlog", "Backlog", "BPR-0001")

        with self.assertRaises(SystemExit):
            self.verify("BPR-0003")


if __name__ == "__main__":
    unittest.main()
