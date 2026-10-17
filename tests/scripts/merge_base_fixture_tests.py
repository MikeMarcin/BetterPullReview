#!/usr/bin/env python3
"""[[BPR-1560]]. merge-base-v1 is exactly what its builder writes.

The builder runs into a scratch root and every file it writes must equal the
committed one byte for byte. The composed fixture's descriptor names the same
three commits: the recorded base, the head and the merge base its compare
answer serves.
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

TREES = ("tests/fixtures/github/pull-requests/merge-base-v1", "tests/fixtures/github/contracts/v1/merge-base-v1")
GENERATED = {"construction.json", "objects.pack"}


def files(root: Path, generated_only: bool = False) -> dict[str, bytes]:
    return {path.relative_to(root).as_posix(): path.read_bytes() for path in sorted(root.rglob("*"))
            if path.is_file() and (not generated_only or path.name in GENERATED or "contracts" in root.as_posix())}


class MergeBaseFixtureTests(unittest.TestCase):
    def test_builder_regenerates_the_committed_tree(self) -> None:
        with contained_temporary_directory(prefix="bpr-merge-base-fixture-") as root:
            scratch = Path(root)
            result = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/build_merge_base_fixture.py"), "--root", str(scratch)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("MERGE_BASE_FIXTURE_OK:", result.stdout)
            for relative in TREES:
                with self.subTest(tree=relative):
                    committed = {name: data for name, data in files(ROOT / relative).items() if name != "README.md"}
                    self.assertEqual(files(scratch / relative), committed)

    def test_descriptor_names_the_built_commits(self) -> None:
        construction = json.loads((ROOT / TREES[0] / "construction.json").read_text(encoding="utf-8"))
        source = (ROOT / "src/app/src/github_public_path_fixture.cpp").read_text(encoding="utf-8")
        block = source[source.index("case github_public_path_fixture::merge_base_v1:"):]
        block = block[:block.index("};")]
        named = dict(re.findall(r"\.(BaseOid|HeadOneOid|MergeBaseOid)\s*=\s*\"([0-9a-f]{40})\"", block))
        self.assertEqual(named.get("BaseOid"), construction["pull_request"]["base"]["sha"])
        self.assertEqual(named.get("HeadOneOid"), construction["pull_request"]["head"]["sha"])
        self.assertEqual(named.get("MergeBaseOid"), construction["merge_base"])
        self.assertNotEqual(construction["merge_base"], construction["pull_request"]["base"]["sha"])


if __name__ == "__main__":
    unittest.main()
