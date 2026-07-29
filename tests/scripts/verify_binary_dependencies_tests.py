#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.verify_binary_dependencies import (  # noqa: E402
    windows_dependency_command,
)


class WindowsDependencyCommandTests(unittest.TestCase):
    def command_with_tools(self, *available: str) -> list[str]:
        paths = {tool: f"C:/tools/{tool}.exe" for tool in available}
        with mock.patch(
            "scripts.verify_binary_dependencies.shutil.which",
            side_effect=paths.get,
        ):
            return windows_dependency_command(Path("BetterPullReview.exe"))

    def test_prefers_dumpbin(self) -> None:
        self.assertEqual(
            self.command_with_tools("dumpbin", "llvm-readobj", "objdump"),
            [
                "C:/tools/dumpbin.exe",
                "/dependents",
                "BetterPullReview.exe",
            ],
        )

    def test_falls_back_to_llvm_readobj(self) -> None:
        self.assertEqual(
            self.command_with_tools("llvm-readobj", "objdump"),
            [
                "C:/tools/llvm-readobj.exe",
                "--coff-imports",
                "BetterPullReview.exe",
            ],
        )

    def test_falls_back_to_objdump(self) -> None:
        self.assertEqual(
            self.command_with_tools("objdump"),
            ["C:/tools/objdump.exe", "-p", "BetterPullReview.exe"],
        )

    def test_requires_a_supported_inspection_tool(self) -> None:
        with self.assertRaisesRegex(
            RuntimeError,
            "dumpbin, llvm-readobj, or objdump",
        ):
            self.command_with_tools()


if __name__ == "__main__":
    unittest.main()
