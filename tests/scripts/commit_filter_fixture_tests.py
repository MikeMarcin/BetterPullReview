#!/usr/bin/env python3
"""RFC-BPR-0089. commit-filter-v1 is exactly what its builder writes.

The builder runs into a scratch root and every file it writes must equal the
committed one byte for byte. The composed fixture's descriptor names the same
merge base and head, and its commit list serves GitHub's parents along the
chain: the base and the head as the served placeholders, the others as built.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

TREES = ("tests/fixtures/github/pull-requests/commit-filter-v1", "tests/fixtures/github/contracts/v1/commit-filter-v1")
BASE_PLACEHOLDER = "1" * 40
HEAD_PLACEHOLDER = "2" * 40


def files(root: Path) -> dict[str, bytes]:
    return {path.relative_to(root).as_posix(): path.read_bytes() for path in sorted(root.rglob("*")) if path.is_file()}


class CommitFilterFixtureTests(unittest.TestCase):
    def test_builder_regenerates_the_committed_tree(self) -> None:
        with contained_temporary_directory(prefix="bpr-commit-filter-fixture-") as root:
            scratch = Path(root)
            result = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/build_commit_filter_fixture.py"), "--root", str(scratch)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("COMMIT_FILTER_FIXTURE_OK:", result.stdout)
            for relative in TREES:
                with self.subTest(tree=relative):
                    committed = {name: data for name, data in files(ROOT / relative).items() if name != "README.md"}
                    self.assertEqual(files(scratch / relative), committed)

    def test_descriptor_and_parents_name_the_chain(self) -> None:
        construction = json.loads((ROOT / TREES[0] / "construction.json").read_text(encoding="utf-8"))
        source = (ROOT / "src/app/src/github_public_path_fixture.cpp").read_text(encoding="utf-8")
        block = source[source.index("case github_public_path_fixture::commit_filter_v1:"):]
        block = block[:block.index("};")]
        named = dict(re.findall(r"\.(BaseOid|HeadOneOid)\s*=\s*\"([0-9a-f]{40})\"", block))
        self.assertEqual(named.get("BaseOid"), construction["merge_base"])
        self.assertEqual(named.get("HeadOneOid"), construction["pull_request"]["head"]["sha"])
        self.assertIn(f".Commits             = {len(construction['commits'])}U", block)
        commits = json.loads((ROOT / TREES[1] / "responses/commits.json").read_text(encoding="utf-8"))
        shas = [commit["sha"] for commit in commits]
        parents = [[parent["sha"] for parent in commit["parents"]] for commit in commits]
        self.assertEqual(len(commits), 3)
        self.assertEqual(shas[-1], HEAD_PLACEHOLDER)
        self.assertEqual(parents, [[BASE_PLACEHOLDER], [shas[0]], [shas[1]]])


if __name__ == "__main__":
    unittest.main()
