#!/usr/bin/env python3
"""The format gate refuses a clang-format whose verdict this tree cannot trust.

`.clang-format` is rendered by clang-format 22 and later. Release 21 and earlier
emit `{}` where this tree holds `{ }`, and no setting restores the space, so an
older tool reports several hundred false violations and `format.py` would
rewrite every one of them. A gate whose verdict depends on an unrecorded tool
version is not reproducible; these cases prove the version is now part of the
verdict rather than an assumption about the host.

The stubs here answer `--version` and record any later invocation, so a refusal
is proven by the *absence* of a formatting call rather than by the exit status
alone.

See `docs/issues/*/BPR-0367-format-gate-fails-under-clang-format-22.md`.
"""

from __future__ import annotations

import io
import os
import subprocess
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))
sys.path.insert(0, str(REPOSITORY_ROOT / "_coding-standard" / "scripts"))

from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

import _coding_standard as CODING_STANDARD  # noqa: E402


STANDARD_SCRIPTS = REPOSITORY_ROOT / "_coding-standard" / "scripts"
FORMAT_CHECK_SCRIPT = STANDARD_SCRIPTS / "format_check.py"
FORMAT_SCRIPT = STANDARD_SCRIPTS / "format.py"

# One version below the minimum and one at it. Written as text rather than
# derived from the constant so that raising the minimum has to be a deliberate
# edit here too.
UNSUPPORTED_BANNER = "Ubuntu clang-format version 21.1.8"
SUPPORTED_BANNER = "Homebrew clang-format version 22.1.8"

SOURCE_TEXT = "int main() {\n\treturn 0;\n}\n"


def write_stub(aDirectory: Path, aBanner: str, aMarker: Path) -> Path:
    """Write an executable clang-format stub that reports `aBanner`.

    Any invocation that is not `--version` or `--help` appends to `aMarker`, so
    a test can distinguish "the gate refused" from "the gate ran and happened to
    pass".
    """
    if os.name == "nt":
        stub = aDirectory / "clang-format-stub.bat"
        stub.write_text(
            "@echo off\r\n"
            'if "%~1"=="--version" (\r\n'
            f"  echo {aBanner}\r\n"
            "  exit /b 0\r\n"
            ")\r\n"
            'if "%~1"=="--help" (\r\n'
            "  echo   --dry-run\r\n"
            "  exit /b 0\r\n"
            ")\r\n"
            f'>>"{aMarker}" echo formatted\r\n'
            "exit /b 0\r\n",
            encoding="ascii",
            newline="",
        )
        return stub

    stub = aDirectory / "clang-format-stub"
    stub.write_text(
        "#!/bin/sh\n"
        'if [ "$1" = "--version" ]; then\n'
        f'  echo "{aBanner}"\n'
        "  exit 0\n"
        "fi\n"
        'if [ "$1" = "--help" ]; then\n'
        '  echo "  --dry-run"\n'
        "  exit 0\n"
        "fi\n"
        f'echo formatted >> "{aMarker}"\n'
        "exit 0\n",
        encoding="ascii",
    )
    stub.chmod(0o755)
    return stub


class clang_format_version_parsing(unittest.TestCase):
    def test_the_project_minimum_is_the_release_that_renders_this_tree(self) -> None:
        self.assertEqual(CODING_STANDARD.MINIMUM_CLANG_FORMAT_VERSION, (22,))

    def test_every_distribution_banner_parses_to_its_release(self) -> None:
        with ContainedTemporaryDirectory() as scratch:
            root = Path(scratch)
            marker = root / "invoked.log"
            for index, (banner, expected) in enumerate(
                (
                    ("Homebrew clang-format version 22.1.8", (22, 1, 8)),
                    ("Ubuntu clang-format version 21.1.8", (21, 1, 8)),
                    ("Apple clang-format version 16.0.0 (clang-1600.0.26.6)", (16, 0, 0)),
                    ("clang-format version 22.1.2", (22, 1, 2)),
                )
            ):
                stub_root = root / f"banner-{index}"
                stub_root.mkdir(parents=True, exist_ok=True)
                stub = write_stub(stub_root, banner, marker)
                self.assertEqual(CODING_STANDARD.clang_format_version(str(stub)), expected, banner)

    def test_an_unreadable_version_is_not_silently_treated_as_supported(self) -> None:
        diagnostic = io.StringIO()
        usable, version = CODING_STANDARD.check_clang_format_version(
            str(REPOSITORY_ROOT / "no-such-clang-format"),
            stream=diagnostic,
        )
        self.assertIsNone(version)
        self.assertFalse(usable)
        self.assertIn("unsupported clang-format", diagnostic.getvalue())


class format_gate_refuses_an_unsupported_tool(unittest.TestCase):
    def run_gate(self, aScript: Path, aBanner: str, *aExtra: str):
        with ContainedTemporaryDirectory() as scratch:
            root = Path(scratch)
            marker = root / "invoked.log"
            source = root / "probe.cpp"
            source.write_text(SOURCE_TEXT, encoding="ascii")
            stub = write_stub(root, aBanner, marker)

            completed = subprocess.run(
                [
                    sys.executable,
                    str(aScript),
                    "--root",
                    str(root),
                    "--clang-format",
                    str(stub),
                    *aExtra,
                    str(source),
                ],
                cwd=str(REPOSITORY_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            return completed, marker.exists()

    def test_format_check_refuses_before_it_checks_anything(self) -> None:
        completed, formatted = self.run_gate(FORMAT_CHECK_SCRIPT, UNSUPPORTED_BANNER)
        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertIn("unsupported clang-format", completed.stderr)
        self.assertIn("21.1.8", completed.stderr)
        self.assertIn("22", completed.stderr)
        self.assertFalse(formatted, "the gate reached clang-format despite refusing the version")

    def test_format_refuses_before_it_rewrites_anything(self) -> None:
        completed, formatted = self.run_gate(FORMAT_SCRIPT, UNSUPPORTED_BANNER)
        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertIn("unsupported clang-format", completed.stderr)
        self.assertIn("rewrite", completed.stderr)
        self.assertFalse(formatted, "an unsupported clang-format was allowed to rewrite files")

    def test_a_supported_tool_is_accepted_and_its_version_is_reported(self) -> None:
        completed, formatted = self.run_gate(FORMAT_CHECK_SCRIPT, SUPPORTED_BANNER)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("22.1.8", completed.stdout)
        self.assertIn("Formatting check passed.", completed.stdout)
        self.assertTrue(formatted, "a supported clang-format was never actually invoked")

    def test_the_override_runs_but_says_the_result_is_not_a_verdict(self) -> None:
        completed, formatted = self.run_gate(
            FORMAT_CHECK_SCRIPT,
            UNSUPPORTED_BANNER,
            CODING_STANDARD.UNSUPPORTED_CLANG_FORMAT_OVERRIDE,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("unsupported clang-format", completed.stderr)
        self.assertIn("not a gate verdict", completed.stderr)
        self.assertTrue(formatted, "the override refused instead of continuing")


class the_installed_tool_satisfies_the_recorded_minimum(unittest.TestCase):
    def test_this_host_can_produce_a_gate_verdict_or_says_why_not(self) -> None:
        try:
            tool = CODING_STANDARD.resolve_tool("clang-format")
        except FileNotFoundError:
            self.skipTest("clang-format is not on PATH on this host")

        version = CODING_STANDARD.clang_format_version(tool)
        self.assertIsNotNone(version, f"{tool} reports no parseable version")
        if version < CODING_STANDARD.MINIMUM_CLANG_FORMAT_VERSION:
            self.skipTest(
                f"{tool} is {CODING_STANDARD.version_text(version)}; this tree needs"
                f" {CODING_STANDARD.version_text(CODING_STANDARD.MINIMUM_CLANG_FORMAT_VERSION)}"
                " or newer, so the C/C++ format gate cannot return a verdict here"
            )


if __name__ == "__main__":
    unittest.main()
