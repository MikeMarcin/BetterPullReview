#!/usr/bin/env python3
"""Every `std::getenv` in a Windows-built translation unit is spelled for MSVC.

MSVC's secure-CRT policy turns `std::getenv` into C4996, and the tree builds
warnings-as-errors, so a Qt-free file that reads the environment the portable
way compiles everywhere except on the Windows lane, which is the only place
the failure is seen (BPR-0519 twice in one day). The two accepted spellings
are the ones the tree already uses: a scoped `#pragma warning(disable : 4996)`
around exactly the read, or a `_dupenv_s` branch under `_MSC_VER` with the
`std::getenv` read in its `#else`. A Win32 `GetEnvironmentVariableA` branch
with the portable read in its `#else` is also accepted. A file that is never
built on Windows is allowlisted by name with the reason.

Owned by BPR-0519.
"""

from __future__ import annotations

import pathlib
import re
import sys
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
ROOTS = ("src", "tests")
CALL = re.compile(r"\bstd::getenv\s*\(")
LOOKBACK = 12
# POSIX-only suites that Windows never builds (their CMake registration is
# gated on the platform), so MSVC never sees their reads.
NOT_BUILT_ON_WINDOWS = frozenset(
    {
        "tests/github_contract/linux_secret_service_native_tests.cpp",
        "tests/github_contract/macos_keychain_native_tests.cpp",
        # The Linux automated-test launcher: `#error` off Linux, and Windows
        # builds `windows_network_sandbox.cpp` in its place ([[BPR-0906]]).
        "tests/network_isolation/linux_network_sandbox.cpp",
        # The Linux launcher's fault shim: `#error` off Linux, and registered
        # only inside the Linux branch of `tests/CMakeLists.txt` ([[BPR-1113]]).
        "tests/network_isolation/linux_launcher_fault_shim.cpp",
    }
)


def spelled_for_msvc(lines: list[str], index: int) -> bool:
    """Whether the read at `lines[index]` sits under either accepted spelling."""
    window = lines[max(0, index - LOOKBACK) : index]
    text = "\n".join(window)
    if re.search(r"disable\s*:\s*4996", text):
        return True
    # A `_dupenv_s` branch guarded on `_MSC_VER` whose `#else` holds this read.
    if "_MSC_VER" in text and "_dupenv_s" in text and re.search(r"^\s*#\s*else\b", text, re.M):
        return True
    # The probe uses Win32 directly; only its POSIX #else reads std::getenv.
    if ("#ifdef _WIN32" in text and "GetEnvironmentVariableA(" in text
            and re.search(r"^\s*#\s*else\b[^#]*$", text, re.M)):
        return True
    return False


def unspelled_reads(root: pathlib.Path) -> list[str]:
    findings: list[str] = []
    for directory in ROOTS:
        for path in sorted((root / directory).rglob("*")):
            if path.suffix not in {".cpp", ".hpp"}:
                continue
            relative = path.relative_to(root).as_posix()
            if relative in NOT_BUILT_ON_WINDOWS:
                continue
            lines = path.read_text(encoding="utf-8").splitlines()
            for index, line in enumerate(lines):
                stripped = line.lstrip()
                if stripped.startswith("//") or stripped.startswith("///"):
                    continue
                if CALL.search(line) and not spelled_for_msvc(lines, index):
                    findings.append(f"{relative}:{index + 1}")
    return findings


class MsvcSecureCrtTests(unittest.TestCase):
    def test_every_getenv_read_is_spelled_for_msvc(self) -> None:
        findings = unspelled_reads(REPOSITORY_ROOT)
        self.assertEqual(
            findings,
            [],
            "std::getenv without a scoped C4996 suppression or a _dupenv_s "
            "branch (MSVC refuses it under warnings-as-errors): "
            + ", ".join(findings),
        )

    def test_both_accepted_spellings_are_recognised(self) -> None:
        suppressed = [
            "#if defined(_MSC_VER)",
            "\t#pragma warning(push)",
            "\t#pragma warning(disable : 4996)",
            "#endif",
            "\tconst auto* value = std::getenv(name);",
        ]
        self.assertTrue(spelled_for_msvc(suppressed, 4))
        branched = [
            "#if defined(_MSC_VER)",
            "\tif (_dupenv_s(&buffer, &size, aName) != 0) {",
            "\t\treturn std::nullopt;",
            "\t}",
            "#else",
            "\tconst char* const value = std::getenv(aName);",
        ]
        self.assertTrue(spelled_for_msvc(branched, 5))
        win32 = ["#ifdef _WIN32", "GetEnvironmentVariableA(name, nullptr, 0);",
                 "#else", "const auto* value = std::getenv(name);"]
        self.assertTrue(spelled_for_msvc(win32, 3))
        self.assertFalse(spelled_for_msvc([*win32[:2], "#endif", win32[3]], 3))
        self.assertFalse(spelled_for_msvc([win32[0], win32[3]], 1))
        bare = ["\tconst auto* value = std::getenv(name);"]
        self.assertFalse(spelled_for_msvc(bare, 0))

    def test_the_allowlist_names_only_files_that_exist(self) -> None:
        for relative in NOT_BUILT_ON_WINDOWS:
            self.assertTrue((REPOSITORY_ROOT / relative).is_file(), relative)


if __name__ == "__main__":
    unittest.main()
