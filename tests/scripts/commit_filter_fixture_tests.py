#!/usr/bin/env python3
"""RFC-BPR-0089. commit-filter-v1, commit-filter-attributes-v1 and
commit-filter-split-v1 are exactly what their builder writes.

The builder runs into a scratch root and every file it writes must equal the
committed one byte for byte. Each composed fixture's descriptor names the same
merge base and head, and its commit list serves GitHub's parents along the
chain: the base and the head as the served placeholders, the others as built.
The attributes variant ([[BPR-1692]]) commits its `.gitattributes` in the
merge base, so its Files changed is the plain variant's 98 files. The split
variant (M3) adds src/version.txt, which all three commits change (99 files),
and serves C3 reworded on the same parent and tree as its second head.
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

VARIANTS = {
    "commit_filter_v1": ("commit-filter-v1", []),
    "commit_filter_attributes_v1": ("commit-filter-attributes-v1", ["--attributes"]),
    "commit_filter_split_v1": ("commit-filter-split-v1", ["--split"]),
}
FILES = {"commit-filter-v1": 98, "commit-filter-attributes-v1": 98, "commit-filter-split-v1": 99}
BASE_PLACEHOLDER = "1" * 40
HEAD_PLACEHOLDER = "2" * 40


def trees(name: str) -> tuple[str, str]:
    return f"tests/fixtures/github/pull-requests/{name}", f"tests/fixtures/github/contracts/v1/{name}"


def files(root: Path) -> dict[str, bytes]:
    return {path.relative_to(root).as_posix(): path.read_bytes() for path in sorted(root.rglob("*")) if path.is_file()}


class CommitFilterFixtureTests(unittest.TestCase):
    def test_builder_regenerates_the_committed_tree(self) -> None:
        for name, flags in VARIANTS.values():
            with self.subTest(fixture=name), contained_temporary_directory(prefix="bpr-commit-filter-fixture-") as root:
                scratch = Path(root)
                result = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/build_commit_filter_fixture.py"), *flags, "--root", str(scratch)],
                                        capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("COMMIT_FILTER_FIXTURE_OK:", result.stdout)
                for relative in trees(name):
                    committed = {entry: data for entry, data in files(ROOT / relative).items() if entry != "README.md"}
                    self.assertEqual(files(scratch / relative), committed)

    def test_descriptor_and_parents_name_the_chain(self) -> None:
        source = (ROOT / "src/app/src/github_public_path_fixture.cpp").read_text(encoding="utf-8")
        for enumerator, (name, _) in VARIANTS.items():
            with self.subTest(fixture=name):
                fixture, contract = trees(name)
                construction = json.loads((ROOT / fixture / "construction.json").read_text(encoding="utf-8"))
                block = source[source.index(f"case github_public_path_fixture::{enumerator}:"):]
                block = block[:block.index("};")]
                named = dict(re.findall(r"\.(BaseOid|HeadOneOid)\s*=\s*\"([0-9a-f]{40})\"", block))
                self.assertEqual(named.get("BaseOid"), construction["merge_base"])
                self.assertEqual(named.get("HeadOneOid"), construction["pull_request"]["head"]["sha"])
                self.assertIn(f".Commits             = {len(construction['commits'])}U", block)
                self.assertEqual(len(construction["files"]), FILES[name])
                commits = json.loads((ROOT / contract / "responses/commits.json").read_text(encoding="utf-8"))
                shas = [commit["sha"] for commit in commits]
                parents = [[parent["sha"] for parent in commit["parents"]] for commit in commits]
                self.assertEqual(len(commits), 3)
                self.assertEqual(shas[-1], HEAD_PLACEHOLDER)
                self.assertEqual(parents, [[BASE_PLACEHOLDER], [shas[0]], [shas[1]]])

    def test_split_variant_serves_a_reworded_second_head(self) -> None:
        source = (ROOT / "src/app/src/github_public_path_fixture.cpp").read_text(encoding="utf-8")
        block = source[source.index("case github_public_path_fixture::commit_filter_split_v1:"):]
        block = block[:block.index("};")]
        heads = dict(re.findall(r"\.(HeadOneOid|HeadTwoOid)\s*=\s*\"([0-9a-f]{40})\"", block))
        self.assertEqual(set(heads), {"HeadOneOid", "HeadTwoOid"})
        with contained_temporary_directory(prefix="bpr-commit-filter-split-") as root:
            pack = ROOT / "tests/fixtures/github/pull-requests/commit-filter-split-v1/objects.pack"
            subprocess.run(["git", "init", "-q", "--bare", root], check=True, capture_output=True)
            subprocess.run(["git", "-C", root, "index-pack", "--stdin"], input=pack.read_bytes(), check=True, capture_output=True)
            show = lambda revision: subprocess.run(["git", "-C", root, "rev-parse", revision], check=True, capture_output=True, text=True).stdout.strip()
            one, two = heads["HeadOneOid"], heads["HeadTwoOid"]
            # The force push carries exactly C3's change: the same parent and tree.
            self.assertEqual(show(f"{two}^"), show(f"{one}^"))
            self.assertEqual(show(f"{two}^{{tree}}"), show(f"{one}^{{tree}}"))
            self.assertNotEqual(one, two)


if __name__ == "__main__":
    unittest.main()
