#!/usr/bin/env python3
"""Parallel tidy keeps every verdict; shared-header fixes remain serial."""

import contextlib
import io
from pathlib import Path
import subprocess
import sys
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "_coding-standard/scripts"))
import tidy


class tidy_gate(unittest.TestCase):
    def test_parallel_failure_does_not_skip_later_files_or_mix_diagnostics(self):
        files = [ROOT / "src" / name for name in ("first.cpp", "second.cpp", "third.cpp")]
        rendezvous = threading.Barrier(2, timeout=5)
        called = []

        def analyse(command, **kwargs):
            path = Path(command[1])
            called.append(path)
            if path in files[:2]:
                rendezvous.wait()
            return subprocess.CompletedProcess(command, int(path == files[0]),
                                               f"begin {path.name}\nend {path.name}\n")

        output, errors = io.StringIO(), io.StringIO()
        with contextlib.ExitStack() as stack:
            for name, result in {
                "resolve_tidy_tool": "stub-tidy",
                "find_compile_commands": ROOT / "build/dev/compile_commands.json",
                "iter_source_files": files,
                "compile_command_files": set(files),
                "filtered_compilation_database": None,
                "host_extra_args": [],
            }.items():
                stack.enter_context(patch.object(tidy, name, return_value=result))
            stack.enter_context(patch.object(tidy.subprocess, "run", side_effect=analyse))
            stack.enter_context(contextlib.redirect_stdout(output))
            stack.enter_context(contextlib.redirect_stderr(errors))
            result = tidy.main(["--root", str(ROOT), "--jobs", "2", "--warnings-as-errors"])
        self.assertEqual(result, 1)
        self.assertCountEqual(called, files)
        for path in files:
            self.assertIn(f"begin {path.name}\nend {path.name}", output.getvalue())
        self.assertIn("src/first.cpp", errors.getvalue())
        self.assertNotIn("src/second.cpp", errors.getvalue())
        self.assertNotIn("clang-tidy passed", output.getvalue())

    def test_parallel_fix_and_invalid_counts_are_refused_before_analysis(self):
        for args in (["--jobs", "2", "--fix"], ["--jobs", "0"], ["--jobs", "-1"]):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as refused:
                    tidy.parse_args(args)
                self.assertEqual(refused.exception.code, 2)

    def test_default_discovery_prefers_22_over_path_23(self):
        with patch.object(tidy, "PINNED_CLANG_FORMAT_DIRECTORIES", ()), \
             patch.object(tidy.shutil, "which", side_effect=lambda name: {
                 "clang-tidy-22": "/pinned/tidy", "clang-tidy": "/default/tidy"}[name]), \
             patch.object(tidy.subprocess, "run", return_value=subprocess.CompletedProcess(
                 [], 0, "Homebrew LLVM version 22.1.8")) as invocation:
            self.assertEqual(tidy.resolve_tidy_tool(None), "/pinned/tidy")
            self.assertEqual(invocation.call_args.args[0], ["/pinned/tidy", "--version"])

    def test_default_discovery_refuses_a_different_major(self):
        with patch.object(tidy, "PINNED_CLANG_FORMAT_DIRECTORIES", ()), \
             patch.object(tidy.shutil, "which", return_value="/default/tidy"), \
             patch.object(tidy.subprocess, "run", return_value=subprocess.CompletedProcess(
                 [], 0, "Homebrew LLVM version 23.1.1")):
            with self.assertRaisesRegex(FileNotFoundError, "clang-tidy 22"):
                tidy.resolve_tidy_tool(None)


if __name__ == "__main__":
    unittest.main()
