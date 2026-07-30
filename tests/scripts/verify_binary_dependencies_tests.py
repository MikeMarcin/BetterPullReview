#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.verify_binary_dependencies import (  # noqa: E402
    dependency_references,
    forbidden_dependencies,
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


class DependencyReferenceTests(unittest.TestCase):
    def test_linux_ignores_randomized_load_addresses(self) -> None:
        output = """
linux-vdso.so.1 (0x00007ffcef123000)
libQt6Core.so.6 => /lib/libQt6Core.so.6 (0x00007f00cef00000)
/lib64/ld-linux-x86-64.so.2 (0x00007f0012340000)
"""
        self.assertEqual(
            forbidden_dependencies(output, "Linux"),
            (),
        )
        self.assertNotIn(
            "0x00007f00cef00000",
            dependency_references(output, "Linux"),
        )

    def test_linux_checks_names_and_resolved_paths(self) -> None:
        output = """
libQt6WebEngineCore.so.6 => /opt/qt/lib/libQt6WebEngineCore.so.6 (0x1234)
libmissing.so => not found
"""
        self.assertEqual(
            forbidden_dependencies(output, "Linux"),
            ("qt6webengine",),
        )
        self.assertIn(
            "libmissing.so",
            dependency_references(output, "Linux"),
        )

    def test_darwin_skips_executable_header_and_checks_install_names(self) -> None:
        output = """
/tmp/cef-looking-directory/BetterPullReview:
\t/opt/qt/QtWebEngine.framework/Versions/A/QtWebEngine (compatibility version 6.0.0, current version 6.11.1)
\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)
"""
        self.assertEqual(
            forbidden_dependencies(output, "Darwin"),
            ("qtwebengine",),
        )

    def test_windows_checks_imported_dll_names_only(self) -> None:
        output = """
File: C:\\cef-looking-directory\\BetterPullReview.exe
  ImageBase: 0x00000000cef00000
  DLL Name: KERNEL32.dll
  DLL Name: libcef.dll
"""
        self.assertEqual(
            forbidden_dependencies(output, "Windows"),
            ("cef",),
        )
        self.assertNotIn(
            "BetterPullReview.exe",
            dependency_references(output, "Windows"),
        )


if __name__ == "__main__":
    unittest.main()
