"""Negative tests of the archive validator; these do not execute BPR."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_pack", ROOT / "scripts/verify_pack.py")
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class ValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="bpr-cc-pack-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "pack"
        shutil.copytree(ROOT, self.root, ignore=shutil.ignore_patterns("__pycache__"))

    def update_json(self, relative: str, update) -> None:
        path = self.root / relative
        data = json.loads(path.read_text(encoding="utf-8"))
        update(data)
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")

    def assert_error(self, substring: str, *, integrity: bool = False) -> None:
        errors = VERIFY.validate(self.root, check_integrity=integrity, exhaustive=False)
        self.assertTrue(any(substring in error for error in errors), errors)

    def replace_frontmatter(self, relative: str, key: str, value) -> None:
        path = self.root / relative
        lines = path.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            if line.startswith(key + ":"):
                lines[index] = key + ": " + json.dumps(value)
                path.write_text("\n".join(lines) + "\n", encoding="utf-8")
                return
        self.fail(f"missing frontmatter field {key}")

    def test_valid_archive(self) -> None:
        self.assertEqual([], VERIFY.validate(self.root, exhaustive=False))

    def test_missing_label(self) -> None:
        self.update_json("specs/LABEL-CATALOG.json", lambda data: data["labels"].pop())
        self.assert_error("exactly all twelve")

    def test_duplicate_label(self) -> None:
        def update(data):
            data["labels"][-1] = dict(data["labels"][0])
        self.update_json("specs/LABEL-CATALOG.json", update)
        self.assert_error("exactly all twelve")

    def test_disabled_first_class_label(self) -> None:
        self.update_json("specs/LABEL-CATALOG.json", lambda data: data["labels"][0].update(first_class=False))
        self.assert_error("first-class")

    def test_schema_label_drift(self) -> None:
        self.update_json("specs/feedback-annotation.schema.json", lambda data: data["properties"]["label"]["enum"].pop(0))
        self.assert_error("schema label vocabulary")

    def test_dangling_dependency(self) -> None:
        self.replace_frontmatter("cards/PACK-CC-02.md", "depends_on", ["PACK-CC-99"])
        self.assert_error("unknown dependency")

    def test_card_cycle(self) -> None:
        self.replace_frontmatter("cards/PACK-CC-01.md", "depends_on", ["PACK-CC-02"])
        self.assert_error("dependency cycle")

    def test_missing_milestone_work(self) -> None:
        self.update_json("milestone-plan.json", lambda data: data["milestones"][0]["work"].clear())
        self.assert_error("work coverage")

    def test_duplicate_milestone_work(self) -> None:
        self.update_json("milestone-plan.json", lambda data: data["milestones"][0]["work"].append({"issue": "PACK-CC-01"}))
        self.assert_error("duplicate milestone work")

    def test_missing_requirement(self) -> None:
        self.update_json("verification/REQUIREMENTS.json", lambda data: data["requirements"].pop())
        self.assert_error("R01 through R48")

    def test_false_product_result(self) -> None:
        self.update_json("verification/REQUIREMENTS.json", lambda data: data["requirements"][0].update(status="passed"))
        self.assert_error("unearned product result")

    def test_wrong_counter_expectation(self) -> None:
        self.update_json("fixtures/counter-cases.json", lambda data: data["cases"][0]["expected"].update(catchall=99))
        self.assert_error("counter fixture mismatch")

    def test_contradictory_duplicate_identity(self) -> None:
        self.update_json("fixtures/counter-cases.json", lambda data: data["units"][-1].update(resolution="resolved"))
        self.assert_error("contradictory duplicate normalized identity")

    def test_protected_suggestion_changed(self) -> None:
        self.update_json("fixtures/header-edit-cases.json", lambda data: data["cases"][0].update(after=data["cases"][0]["after"].replace("checked_count", "wrong_count")))
        self.assert_error("protected native suggestion suffix")

    def test_claimed_predecessor_validation(self) -> None:
        self.update_json("references/PREDECESSOR-PACKS.json", lambda data: data["import_order"][0].update(archive_bytes_verified_here=True))
        self.assert_error("unavailable predecessor bytes")

    def test_payload_tamper(self) -> None:
        with (self.root / "README.md").open("a", encoding="utf-8") as output:
            output.write("\nModified payload.\n")
        self.assert_error("checksum mismatch", integrity=True)

    def test_unlisted_payload(self) -> None:
        (self.root / "unexpected.txt").write_text("extra", encoding="utf-8")
        self.assert_error("unlisted payload", integrity=True)

    def test_nested_checksum_file_is_not_ignored(self) -> None:
        nested = self.root / "unexpected" / "SHA256SUMS"
        nested.parent.mkdir()
        nested.write_text("extra", encoding="utf-8")
        self.assert_error("unlisted payload", integrity=True)

    def test_escaping_reference(self) -> None:
        self.update_json("PACK-MANIFEST.json", lambda data: data["cards"][0].update(path="../outside.md"))
        self.assert_error("unsafe relative path")


if __name__ == "__main__":
    unittest.main()
