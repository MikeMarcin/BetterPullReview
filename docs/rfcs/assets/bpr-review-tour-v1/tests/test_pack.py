"""Offline artifact regression/negative tests; not application or service evidence."""
from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify_pack", ROOT / "scripts/verify_pack.py")
assert spec is not None and spec.loader is not None
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


def reseal(root: Path) -> None:
    """Test-only resealing makes negative tests exercise semantics, not just hashes."""
    (root / "SHA256SUMS").write_text("".join(
        hashlib.sha256((root / name).read_bytes()).hexdigest() + "  " + name + "\n"
        for name in pack.payload_names(root)), encoding="utf-8")


class PackTests(unittest.TestCase):
    def copied(self) -> Path:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name) / "pack"
        shutil.copytree(ROOT, root, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        return root

    def change_json(self, root: Path, name: str, action) -> None:
        path = root / name
        value = json.loads(path.read_text(encoding="utf-8"))
        action(value)
        path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
        reseal(root)

    def test_exact_pack(self):
        result = pack.verify(ROOT)
        self.assertEqual(result["status"], "passed")
        self.assertEqual((result["rfcs"], result["cards"], result["milestones"],
                          result["acceptance_requirements"]), (5, 26, 8, 43))
        self.assertFalse(result["product_runtime_tested"])
        self.assertFalse(result["live_github_tested"])

    def test_checksum_change_rejected(self):
        root = self.copied()
        with (root / "README.md").open("a", encoding="utf-8") as output:
            output.write("changed\n")
        with self.assertRaisesRegex(pack.PackError, "Checksum mismatch"):
            pack.verify(root)

    def test_unlisted_payload_rejected(self):
        root = self.copied()
        (root / "unlisted.txt").write_text("not part of the pack")
        with self.assertRaisesRegex(pack.PackError, "inventory"):
            pack.verify(root)

    def test_checksum_path_escape_rejected(self):
        root = self.copied()
        with (root / "SHA256SUMS").open("a", encoding="utf-8") as output:
            output.write("0" * 64 + "  ../outside.txt\n")
        with self.assertRaisesRegex(pack.PackError, "Unsafe payload"):
            pack.verify(root)

    def test_missing_card_even_after_reseal(self):
        root = self.copied()
        (root / "cards/PACK-TOUR-04.md").unlink()
        reseal(root)
        with self.assertRaisesRegex(pack.PackError, "Missing payload"):
            pack.verify(root)

    def test_duplicate_card_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "cards/CARD-INDEX.json",
                         lambda value: value["cards"].append(value["cards"][0]))
        with self.assertRaisesRegex(pack.PackError, "Duplicate card"):
            pack.verify(root)

    def test_plan_cycle_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "milestone-plan.json",
                         lambda value: value["milestones"][0].update(depends_on=["qualification-closeout"]))
        with self.assertRaisesRegex(pack.PackError, "Dependency cycle"):
            pack.verify(root)

    def test_missing_planned_card_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "milestone-plan.json",
                         lambda value: value["milestones"][0]["work"].clear())
        with self.assertRaisesRegex(pack.PackError, "Missing planned card"):
            pack.verify(root)

    def test_missing_requirement_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "verification/acceptance-matrix.json",
                         lambda value: value["requirements"].pop())
        with self.assertRaisesRegex(pack.PackError, "Acceptance coverage"):
            pack.verify(root)

    def test_wrong_requirement_owner_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "verification/acceptance-matrix.json",
                         lambda value: value["requirements"][0].update(owner_cards=["PACK-TOUR-01"]))
        with self.assertRaisesRegex(pack.PackError, "owner mismatch"):
            pack.verify(root)

    def test_fake_runtime_evidence_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "verification/acceptance-matrix.json",
                         lambda value: value["requirements"][0].update(implementation_status="passed"))
        with self.assertRaisesRegex(pack.PackError, "Invented runtime evidence"):
            pack.verify(root)

    def test_old_file_in_additive_overlay_rejected(self):
        root = self.copied()
        (root / "repository-overlay/docs/README.md").write_text("unsafe replacement")
        reseal(root)
        with self.assertRaisesRegex(pack.PackError, "Overlay may contain only"):
            pack.verify(root)

    def test_broken_local_link_even_after_reseal(self):
        root = self.copied()
        with (root / "README.md").open("a", encoding="utf-8") as output:
            output.write("\n[Missing document](supporting/missing.md)\n")
        reseal(root)
        with self.assertRaisesRegex(pack.PackError, "Broken local Markdown link"):
            pack.verify(root)

    def test_wrong_index_order_even_after_reseal(self):
        root = self.copied()
        self.change_json(root, "examples/github-route.json",
                         lambda value: value["stops"].reverse())
        with self.assertRaisesRegex(pack.PackError, "index order"):
            pack.verify(root)

    def test_missing_next_link_even_after_reseal(self):
        root = self.copied()
        path = root / "examples/manual-tour/stop-1.md"
        path.write_text(path.read_text().replace("#discussion_r102", "#discussion_r999"))
        reseal(root)
        with self.assertRaisesRegex(pack.PackError, "Missing next navigation"):
            pack.verify(root)

    def test_early_submission_in_create_rejected(self):
        root = self.copied()
        self.change_json(root, "examples/api/create-pending-review.json",
                         lambda value: value.update(event="COMMENT"))
        with self.assertRaisesRegex(pack.PackError, "omit event"):
            pack.verify(root)

    def test_approval_is_not_tour_publication(self):
        root = self.copied()
        self.change_json(root, "examples/api/submit-review.json",
                         lambda value: value.update(event="APPROVE"))
        with self.assertRaisesRegex(pack.PackError, "must be COMMENT"):
            pack.verify(root)

    def test_guessed_rest_batch_subject_field_rejected(self):
        root = self.copied()
        self.change_json(root, "examples/api/create-pending-review.json",
                         lambda value: value["comments"][0].update(subject_type="file"))
        with self.assertRaisesRegex(pack.PackError, "Undocumented field"):
            pack.verify(root)

    def test_staged_response_order_is_not_identity(self):
        root = self.copied()
        self.change_json(root, "examples/api/staged-comments-response.json", lambda value: value.reverse())
        self.assertEqual(pack.verify(root)["status"], "passed")

    def test_outline_cannot_include_publishable_bodies(self):
        value = pack.load(ROOT, "examples/ai/outline-result.json")
        value["stops"][0]["body"] = "A generated full comment."
        with self.assertRaisesRegex(pack.PackError, "Unexpected fields"):
            pack.validate_result(value)

    def test_outline_cannot_self_mark_ready(self):
        value = pack.load(ROOT, "examples/ai/outline-result.json")
        value["stops"][0]["ready"] = True
        with self.assertRaisesRegex(pack.PackError, "Unexpected fields"):
            pack.validate_result(value)

    def test_unknown_source_reference_rejected(self):
        value = pack.load(ROOT, "examples/ai/outline-result.json")
        refs = set(pack.load(ROOT, "examples/ai/outline-context.json")["supplied_source_refs"])
        value["stops"][0]["anchor_ref"] = "source:invented"
        with self.assertRaisesRegex(pack.PackError, "Unknown source ref"):
            pack.validate_result(value, refs)

    def test_model_cannot_choose_edit_target(self):
        value = pack.load(ROOT, "examples/ai/rewrite-result.json")
        value["target"] = "some-other-field"
        with self.assertRaisesRegex(pack.PackError, "Unexpected fields"):
            pack.validate_result(value)

    def test_replacement_ceiling_is_bytes_not_only_characters(self):
        value = pack.load(ROOT, "examples/ai/rewrite-result.json")
        value["replacement_text"] = "🐼" * 20000
        with self.assertRaisesRegex(pack.PackError, "byte ceiling"):
            pack.validate_result(value)

    def test_splice_does_not_split_utf8_code_points(self):
        with self.assertRaisesRegex(pack.PackError, "UTF-8 code point"):
            pack.splice_exact("A🐼B", 2, 4, "x")

    def test_splice_checks_bounds(self):
        for start, end in [(-1, 2), (3, 1), (0, 999), (True, 2)]:
            with self.subTest(start=start, end=end):
                with self.assertRaisesRegex(pack.PackError, "Invalid span"):
                    pack.splice_exact("abc", start, end, "x")

    def test_all_raw_text_transactions(self):
        for case in pack.load(ROOT, "examples/text-transactions.json")["cases"]:
            with self.subTest(case=case["name"]):
                result = pack.splice_exact(case["before"], case["start_utf8"], case["end_utf8"], case["replacement"])
                self.assertEqual(result, case["expected"])
                self.assertEqual(result != case["before"], case["creates_undo_entry"])

    def test_equal_rewrite_cannot_claim_an_undo_entry(self):
        root = self.copied()
        def break_noop(value):
            for case in value["cases"]:
                if case["name"] == "equal-noop":
                    case["creates_undo_entry"] = True
        self.change_json(root, "examples/text-transactions.json", break_noop)
        with self.assertRaisesRegex(pack.PackError, "No-op undo mismatch"):
            pack.verify(root)


if __name__ == "__main__":
    unittest.main()
