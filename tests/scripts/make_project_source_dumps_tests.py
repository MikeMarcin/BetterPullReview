#!/usr/bin/env python3
"""Contract tests for the BetterPullReview project-source dump generator."""

from __future__ import annotations

import importlib.util
import subprocess
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "make_project_source_dumps.py"
SPEC = importlib.util.spec_from_file_location("make_project_source_dumps", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load project-source generator: {SCRIPT}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ProjectSourceDumpTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.root = Path(self.temporary.name).resolve()
        self.write("README.md", "# Fixture\n")
        self.write("AGENTS.md", "# Instructions\n")
        self.write("CMakeLists.txt", "cmake_minimum_required(VERSION 3.25)\n")
        self.write("src/main.cpp", "int main() { return 0; }\n")
        self.write("docs/guide.md", "# Guide\n")
        self.write("tests/example.cpp", "void test() {}\n")
        self.write("build/generated.cpp", "generated\n")
        self.write("artifacts/previous.md", "previous dump\n")
        self.write(".git/config", "private repository metadata\n")
        self.write("src/image.png", "not really an image\n")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write(self, relative: str, text: str) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def discover(self, include_generated: bool = False) -> list[str]:
        paths = MODULE.discover_project_files(
            self.root,
            include_generated=include_generated,
            exclude_paths=set(),
        )
        return [MODULE.rel_path(path, self.root) for path in paths]

    def test_discovers_ordered_text_sources_and_skips_generated_content(self) -> None:
        paths = self.discover()

        self.assertEqual(paths[:2], ["README.md", "AGENTS.md"])
        self.assertIn("src/main.cpp", paths)
        self.assertIn("docs/guide.md", paths)
        self.assertIn("tests/example.cpp", paths)
        self.assertNotIn("build/generated.cpp", paths)
        self.assertNotIn("artifacts/previous.md", paths)
        self.assertNotIn(".git/config", paths)
        self.assertNotIn("src/image.png", paths)

        generated_paths = self.discover(include_generated=True)
        self.assertIn("build/generated.cpp", generated_paths)
        self.assertIn("artifacts/previous.md", generated_paths)
        self.assertNotIn(".git/config", generated_paths)

    def test_cli_writes_rerunnable_dump_with_safe_markdown_fences(self) -> None:
        self.write("src/fenced.cpp", "```\n")
        command = [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(self.root),
        ]

        first = subprocess.run(command, check=False, capture_output=True, text=True)
        second = subprocess.run(command, check=False, capture_output=True, text=True)

        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        output = self.root / "artifacts/better_pull_review_project_source.md"
        text = output.read_text(encoding="utf-8")
        self.assertIn("# BetterPullReview Project Source", text)
        self.assertIn("README.md [repository-context]", text)
        self.assertIn("src/main.cpp [source]", text)
        self.assertIn("````cpp\n```\n````", text)
        self.assertNotIn(
            "artifacts/better_pull_review_project_source.md [", text
        )


if __name__ == "__main__":
    unittest.main()
