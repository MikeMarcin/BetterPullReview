"""Public BPR wrapper integration; canonical policy is tested by AgentSkills."""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402
ALLOCATOR = ROOT / "scripts/allocate_issue_id.py"


@unittest.skipUnless(shutil.which("git"), "Git required")
class SharedBoardWrapperTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory(prefix="bpr-allocate-issue-id-")
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name).resolve()
        self.config = json.loads((ROOT / ".mask/skill-config/implement-rfc/project.json").read_text())
        self.config["workstreams"] = {"main": {"branch": "main"}}
        self.config["allocation"] = {"history_limit": 2000}
        ctx = self.repo / ".mask/skill-config/implement-rfc"
        ctx.mkdir(parents=True)
        (ctx / "project.json").write_text(json.dumps(self.config))
        (ctx / "project.md").write_text("Fixture repository, no product mutation.\n")
        source = ROOT / ".agents/skills/implement-rfc"
        target = self.repo / ".agents/skills/implement-rfc"
        for folder in ("scripts", "schemas"):
            shutil.copytree(source / folder, target / folder, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        board = self.repo / "docs/issues"
        for lane in self.config["lanes"].values():
            (board / lane).mkdir(parents=True, exist_ok=True)
        (board / "NEXT_ID").write_text("BPR-0002\n")
        (board / "_reservations").mkdir()
        (self.repo / "docs/rfcs").mkdir()
        fields = {"schema": "implement-rfc.card.v1", "id": "BPR-0001",
                  "title": "Existing fixture", "status": "Done", "rfcs": [], "depends_on": []}
        (board / "60-done/BPR-0001-fixture.md").write_text(
            "---\n" + "".join(f"{key}: {json.dumps(value)}\n" for key, value in fields.items()) + "---\n")
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.name", "Wrapper tests")
        self.git("config", "user.email", "tests@example.invalid")
        self.git("add", ".")
        self.git("commit", "-qm", "Fixture baseline")

    def git(self, *args: str) -> None:
        subprocess.run(["git", "-C", str(self.repo), *args], check=True, capture_output=True, text=True)

    def run_wrapper(self, tool: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run([sys.executable, str(tool), "--root", str(self.repo), *args],
                              cwd=self.temporary.name, capture_output=True, text=True)

    def test_plain_allocation_is_reserved_and_dry_run_does_not_write(self) -> None:
        counter = self.repo / "docs/issues/NEXT_ID"
        preview = self.run_wrapper(ALLOCATOR, "--count", "2", "--dry-run", "--json")
        self.assertEqual(preview.returncode, 0, preview.stderr + preview.stdout)
        self.assertEqual(counter.read_text(), "BPR-0002\n")
        result = self.run_wrapper(ALLOCATOR, "--count", "2", "--json")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        report = json.loads(result.stdout)
        self.assertEqual(report["ids"], ["BPR-0002", "BPR-0003"])
        self.assertEqual(report["schema"], "implement-rfc.allocation.v1")
        self.assertEqual(counter.read_text(), "BPR-0004\n")
        receipt_path = next(p for p in report["changed_paths"] if p.endswith(".json"))
        self.assertEqual(json.loads((self.repo / receipt_path).read_text())["reserve_for"], "local-main")

    def test_reservation_purpose_is_forwarded(self) -> None:
        result = self.run_wrapper(ALLOCATOR, "--reserve-for", "bpr-test-lane",
                                  "--purpose", "bounded test work", "--json")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        report = json.loads(result.stdout)
        receipt_path = next(p for p in report["changed_paths"] if p.endswith(".json"))
        self.assertEqual(json.loads((self.repo / receipt_path).read_text())["purpose"], "bounded test work")



if __name__ == "__main__":
    unittest.main()
