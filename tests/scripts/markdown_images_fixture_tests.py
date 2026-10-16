#!/usr/bin/env python3
"""RFC-BPR-0084. markdown-images-v1 is exactly what its builder writes.

The builder is run against a scratch copy of its one input, the nested-folders
contract, and every file it writes must equal the committed one byte for byte.
The served tree must also agree with itself: each body node answers, each
served image exists and carries its signature, and only the image the course
expects to fail has no file.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = "tests/fixtures/github/contracts/v1/nested-folders-v1"
CONTRACT = "tests/fixtures/github/contracts/v1/markdown-images-v1"
IMAGES = "tests/fixtures/github/markdown-images/v1/markdown-images-v1"
SIGNATURES = {"image/png": b"\x89PNG\r\n\x1a\n", "image/gif": b"GIF89a"}


def files(root: Path) -> dict[str, bytes]:
    return {path.relative_to(root).as_posix(): path.read_bytes() for path in sorted(root.rglob("*")) if path.is_file()}


class MarkdownImagesFixtureTests(unittest.TestCase):
    def test_builder_regenerates_the_committed_tree(self) -> None:
        scratch = Path(tempfile.mkdtemp(prefix="bpr-markdown-images-"))
        try:
            shutil.copytree(ROOT / SOURCE, scratch / SOURCE)
            result = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/build_markdown_images_fixture.py"), str(scratch)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(result.stdout.startswith("MARKDOWN_IMAGES_FIXTURE_OK:"))
            for relative in (CONTRACT, IMAGES):
                with self.subTest(tree=relative):
                    self.assertEqual(files(scratch / relative), files(ROOT / relative))
        finally:
            shutil.rmtree(scratch)

    def test_served_tree_is_consistent(self) -> None:
        bodies = json.loads((ROOT / IMAGES / "bodies.json").read_text(encoding="utf-8"))
        pull = json.loads((ROOT / CONTRACT / "responses/pull_request.json").read_text(encoding="utf-8"))
        comments = json.loads((ROOT / CONTRACT / "responses/issue_comments.json").read_text(encoding="utf-8"))
        repository = json.loads((ROOT / CONTRACT / "responses/repository.json").read_text(encoding="utf-8"))
        self.assertEqual(bodies["repository"], repository["node_id"])
        self.assertEqual(bodies["pullRequest"], pull["node_id"])
        self.assertEqual(set(bodies["bodies"]), {pull["node_id"], *(comment["node_id"] for comment in comments)})
        manifest = json.loads((ROOT / IMAGES / "manifest.json").read_text(encoding="utf-8"))["images"]
        served = {entry["address"] for entry in manifest}
        for entry in manifest:
            with self.subTest(file=entry["file"]):
                data = (ROOT / IMAGES / entry["file"]).read_bytes()
                signature = SIGNATURES.get(entry["content_type"])
                if signature is not None:
                    self.assertTrue(data.startswith(signature))
                else:
                    self.assertTrue(entry["content_type"].startswith("image/svg+xml"))
        html = "".join(bodies["bodies"].values())
        missing = [address for address in served if address not in html]
        self.assertEqual(missing, [], "every served image is named by a body")
        self.assertIn("jwt=fixture", html)
        self.assertNotIn("jwt=ey", html, "no real signature is committed")


if __name__ == "__main__":
    unittest.main()
