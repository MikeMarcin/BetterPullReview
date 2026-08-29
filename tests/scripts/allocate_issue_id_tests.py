#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

from scripts.allocate_issue_id import (  # noqa: E402
    AllocationError,
    allocate,
)

GIT_ENV = {
    **os.environ,
    "GIT_CONFIG_NOSYSTEM": "1",
    "GIT_CONFIG_GLOBAL": os.devnull,
    "GIT_AUTHOR_NAME": "Board Tests",
    "GIT_AUTHOR_EMAIL": "tests@example.invalid",
    "GIT_COMMITTER_NAME": "Board Tests",
    "GIT_COMMITTER_EMAIL": "tests@example.invalid",
}


class BoardFixture:
    """A throwaway tracker tree; the real `docs/issues` is never touched."""

    def __init__(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.repo_root = Path(self.temporary.name).resolve()
        self.issues_root = self.repo_root / "docs/issues"
        for lane in ("20-ready", "30-in-progress", "60-done"):
            (self.issues_root / lane).mkdir(parents=True)
        self.set_counter("BPR-0007")
        self.add_card("60-done", "BPR-0006")

    def cleanup(self) -> None:
        self.temporary.cleanup()

    def set_counter(self, value: str) -> None:
        (self.issues_root / "NEXT_ID").write_text(f"{value}\n", encoding="utf-8")

    def counter(self) -> str:
        return (self.issues_root / "NEXT_ID").read_text(encoding="utf-8").strip()

    def add_card(self, lane: str, card_id: str) -> None:
        path = self.issues_root / lane / f"{card_id}-fixture-card.md"
        path.write_text(f"# {card_id} fixture\n", encoding="utf-8")

    def git(self, *arguments: str) -> None:
        subprocess.run(
            ["git", "-C", str(self.repo_root), *arguments],
            check=True,
            capture_output=True,
            env=GIT_ENV,
        )


class AllocateIssueIdTests(unittest.TestCase):
    def setUp(self) -> None:
        self.board = BoardFixture()
        self.addCleanup(self.board.cleanup)

    def test_claims_one_id_and_advances_the_counter(self) -> None:
        allocation = allocate(self.board.repo_root, 1, None, "")
        self.assertEqual(allocation.ids, ["BPR-0007"])
        self.assertEqual(self.board.counter(), "BPR-0008")

    def test_reserve_for_writes_an_immutable_receipt(self) -> None:
        allocation = allocate(self.board.repo_root, 3, "bpr-win-jaegaw", "fixture")
        self.assertEqual(allocation.ids, ["BPR-0007", "BPR-0008", "BPR-0009"])
        self.assertIsNotNone(allocation.receipt_path)
        receipt_path = self.board.repo_root / allocation.receipt_path
        self.assertEqual(receipt_path.parent, self.board.issues_root / "_reservations")
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        self.assertEqual(receipt["schema"], "betterpullreview.issue-id-reservation.v1")
        self.assertEqual(receipt["range"], {"start": "BPR-0007", "end": "BPR-0009"})
        self.assertEqual(receipt["reserved_for"], "bpr-win-jaegaw")
        self.assertEqual(self.board.counter(), "BPR-0010")

    def test_refuses_a_counter_the_tree_has_already_passed(self) -> None:
        self.board.add_card("20-ready", "BPR-0011")
        with self.assertRaises(AllocationError):
            allocate(self.board.repo_root, 1, None, "")
        self.assertEqual(self.board.counter(), "BPR-0007")

    def test_refuses_a_counter_rewound_below_git_history(self) -> None:
        self.board.git("init", "-q", "-b", "main")
        self.board.git("add", "-A")
        self.board.git("commit", "-q", "-m", "seed")
        self.board.set_counter("BPR-0031")
        self.board.git("add", "-A")
        self.board.git("commit", "-q", "-m", "advance")
        self.board.set_counter("BPR-0007")
        with self.assertRaises(AllocationError) as caught:
            allocate(self.board.repo_root, 1, None, "")
        self.assertIn("already reached in Git history", str(caught.exception))

    def test_a_held_lock_refuses_a_second_allocation(self) -> None:
        lock_dir = self.board.repo_root / "build/tmp/issue-board"
        lock_dir.mkdir(parents=True)
        (lock_dir / "NEXT_ID.lock").write_text("pid 1\n", encoding="utf-8")
        with self.assertRaises(AllocationError):
            allocate(self.board.repo_root, 1, None, "")
        self.assertEqual(self.board.counter(), "BPR-0007")

    def test_reservations_directory_is_not_scanned_for_cards(self) -> None:
        allocate(self.board.repo_root, 1, "bpr-linux-jaegaw", "")
        allocation = allocate(self.board.repo_root, 1, None, "")
        self.assertEqual(allocation.ids, ["BPR-0008"])


if __name__ == "__main__":
    unittest.main()
