#!/usr/bin/env python3
"""An image that enables Control Flow Guard is never linked incrementally.

MSVC's incremental linker can point a vtable at a new ILT thunk and leave that
thunk out of the image's Guard CF function table. The next virtual call through
the slot then fails fast (0xC0000409, subcode 0xA) in a correct program. That
was BPR-1027: a relink left fourteen deleting destructors out of one Debug
image's table, and deleting through any of them ended the process. CMake's
Debug and RelWithDebInfo defaults link `/INCREMENTAL`, so `cmake/Hardening.cmake`
passes `/INCREMENTAL:NO` next to `/guard:cf`. Target options come after the
configuration's linker flags, and the later option wins, in `link.exe` and in
CMake's `vs_link_exe` wrapper alike.

Three checks keep the two apart:

- Source: every `target_link_options` call in `cmake/Hardening.cmake` that
  passes `/guard:cf` ends its incremental choice on `/INCREMENTAL:NO`.
- Source: no other CMake source or preset enables CFG, incremental linking,
  or Edit and Continue (`/ZI`, which links incrementally).
- Build, when `--build-dir` names a generated tree: every Ninja link statement
  or Visual Studio project that links `/guard:cf` links completely. A Ninja
  link with `/debug` and no incremental option is incremental, because that is
  `link.exe`'s default.

Owned by BPR-1027.
"""

from __future__ import annotations

import os
import pathlib
import re
import sys
import unittest

from skip_report import SkipReportingTestRunner


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
HARDENING = pathlib.Path("cmake") / "Hardening.cmake"
GUARD_CF = re.compile(r"^[/-]guard:cf$", re.I)
INCREMENTAL = re.compile(r"^[/-]incremental(:(yes|no))?$", re.I)
# Case matters for cl: /Zi is a PDB, /ZI is Edit and Continue.
SOURCE_REFUSALS = (
    (re.compile(r"(?<![\w:])[/-]incremental\b(?!:no\b)", re.I), "enables incremental linking"),
    (re.compile(r"(?<![\w:])[/-]ZI\b"), "enables Edit and Continue, which links incrementally"),
    (re.compile(r"\bEditAndContinue\b"), "enables Edit and Continue, which links incrementally"),
    (re.compile(r"(?<![\w:])[/-]guard:cf\b(?!-)", re.I), "enables CFG outside cmake/Hardening.cmake"),
)
BUILD_DIRECTORY_NAMES = {"build", "out", "node_modules"}
BUILD_DIR: pathlib.Path | None = None


def without_cmake_comments(text: str) -> str:
    """Drop `#` line comments, which may name the options they explain."""
    kept = []
    for line in text.splitlines():
        match = re.search(r"(^|\s)#", line)
        kept.append(line[: match.start()] if match else line)
    return "\n".join(kept)


def calls(text: str, command: str) -> list[str]:
    """The argument text of every `command(...)` call, parentheses balanced."""
    found = []
    for match in re.finditer(rf"\b{command}\s*\(", text):
        depth = 1
        index = match.end()
        while index < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[index], 0)
            index += 1
        found.append(text[match.end() : index - 1])
    return found


def links_incrementally(tokens: list[str]) -> bool:
    """Whether `link.exe` given these options links incrementally."""
    choices = [token for token in tokens if INCREMENTAL.match(token)]
    if choices:
        return not choices[-1].lower().endswith(":no")
    return any(token.lower().startswith(("/debug", "-debug")) for token in tokens)


def hardening_findings(text: str) -> list[str]:
    findings = []
    guarded = 0
    for arguments in calls(without_cmake_comments(text), "target_link_options"):
        tokens = [token.strip('"') for token in arguments.split()]
        if not any(GUARD_CF.match(token) for token in tokens):
            continue
        guarded += 1
        choices = [token for token in tokens if INCREMENTAL.match(token)]
        if not choices or not choices[-1].lower().endswith(":no"):
            findings.append("target_link_options passes /guard:cf without ending on /INCREMENTAL:NO: " + " ".join(tokens))
    if not guarded:
        findings.append("no target_link_options call passes /guard:cf; the pairing has moved")
    return findings


def cmake_sources(root: pathlib.Path) -> list[pathlib.Path]:
    """Every CMake source and preset outside hidden and build directories."""
    sources = []
    for directory, subdirectories, files in os.walk(root):
        subdirectories[:] = sorted(name for name in subdirectories if not name.startswith(".") and name not in BUILD_DIRECTORY_NAMES)
        for name in sorted(files):
            if name in {"CMakeLists.txt", "CMakePresets.json"} or name.endswith(".cmake"):
                sources.append(pathlib.Path(directory) / name)
    return sources


def text_findings(relative: pathlib.PurePath, text: str) -> list[str]:
    """What one CMake source or preset enables that it must not."""
    findings = []
    if relative.suffix != ".json":
        text = without_cmake_comments(text)
    for pattern, reason in SOURCE_REFUSALS:
        if relative == HARDENING and "CFG" in reason:
            continue
        for match in pattern.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            findings.append(f"{relative.as_posix()}:{line}: {match.group(0)} {reason}")
    return findings


def source_findings(root: pathlib.Path) -> list[str]:
    findings = []
    for path in cmake_sources(root):
        findings += text_findings(path.relative_to(root), path.read_text(encoding="utf-8", errors="replace"))
    return findings


def ninja_findings(text: str) -> list[str]:
    findings = []
    output = None
    for line in text.splitlines():
        statement = re.match(r"build (.+?): (\w+_LINKER__\w+)", line)
        if statement:
            output = statement.group(1).split(" | ")[0].strip()
            continue
        if not line.startswith(" "):
            output = None
            continue
        if output and line.strip().startswith("LINK_FLAGS ="):
            tokens = line.split("=", 1)[1].split()
            if any(GUARD_CF.match(token) for token in tokens) and links_incrementally(tokens):
                findings.append(f"{output} links /guard:cf incrementally: {' '.join(tokens)}")
    return findings


def vcxproj_findings(name: str, text: str) -> list[str]:
    guarded = re.search(r"guard:cf|<(Link)?ControlFlowGuard>Guard", text, re.I)
    incremental = re.findall(r"<LinkIncremental\b[^>]*>\s*true\s*</LinkIncremental>", text, re.I)
    if guarded and incremental:
        return [f"{name} links /guard:cf incrementally ({len(incremental)} configuration(s))"]
    return []


def build_findings(build: pathlib.Path) -> tuple[str, list[str]]:
    ninja = build / "build.ninja"
    if ninja.is_file():
        return "ninja", ninja_findings(ninja.read_text(encoding="utf-8", errors="replace"))
    projects = sorted(build.rglob("*.vcxproj"))
    if projects:
        findings = []
        for project in projects:
            findings += vcxproj_findings(project.relative_to(build).as_posix(), project.read_text(encoding="utf-8", errors="replace"))
        return "visual-studio", findings
    return "none", []


class CfgCompleteLinkTests(unittest.TestCase):
    def test_hardening_ends_every_cfg_link_on_a_complete_link(self) -> None:
        findings = hardening_findings((REPOSITORY_ROOT / HARDENING).read_text(encoding="utf-8"))
        self.assertEqual(findings, [], "; ".join(findings))

    def test_no_other_cmake_source_enables_cfg_or_incremental_linking(self) -> None:
        findings = source_findings(REPOSITORY_ROOT)
        self.assertEqual(findings, [], "; ".join(findings))

    def test_generated_cfg_links_are_complete(self) -> None:
        if BUILD_DIR is None:
            self.skipTest("no --build-dir given")
        generator, findings = build_findings(BUILD_DIR)
        print(f"CFG_COMPLETE_LINK_BUILD generator={generator} findings={len(findings)}")
        self.assertEqual(findings, [], "; ".join(findings))


class RefusalTests(unittest.TestCase):
    """The checks refuse what they exist to refuse."""

    def test_hardening_without_a_complete_link_is_refused(self) -> None:
        self.assertEqual(hardening_findings('target_link_options("${t}" PRIVATE /guard:cf /INCREMENTAL:NO /NXCOMPAT)'), [])
        for text in (
            'target_link_options("${t}" PRIVATE /guard:cf /DYNAMICBASE)',
            'target_link_options("${t}" PRIVATE /guard:cf /INCREMENTAL:NO /INCREMENTAL)',
            '# /guard:cf /INCREMENTAL:NO\ntarget_link_options("${t}" PRIVATE /guard:cf)',
        ):
            self.assertEqual(len(hardening_findings(text)), 1, text)
        self.assertEqual(hardening_findings('target_link_options("${t}" PRIVATE /NXCOMPAT)'),
                         ["no target_link_options call passes /guard:cf; the pairing has moved"])

    def test_sources_that_enable_incremental_linking_are_refused(self) -> None:
        cases = {
            "target_link_options(x PRIVATE /INCREMENTAL)": 1,
            'set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/debug /incremental:yes")': 1,
            "target_compile_options(x PRIVATE /ZI)": 1,
            'set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "EditAndContinue")': 1,
            "target_link_options(x PRIVATE /guard:cf)": 1,
            "target_link_options(x PRIVATE /INCREMENTAL:NO)": 0,
            "target_compile_options(x PRIVATE /Zi)": 0,
            "# CMake's Debug default links /INCREMENTAL": 0,
            "target_link_options(x PRIVATE /guard:cf-)": 0,
        }
        # In memory: a contained Windows run cannot write into the directory
        # tempfile creates for it.
        for text, expected in cases.items():
            self.assertEqual(len(text_findings(pathlib.PurePath("CMakeLists.txt"), text + "\n")), expected, text)
        self.assertEqual(text_findings(HARDENING, "target_link_options(t PRIVATE /guard:cf /INCREMENTAL:NO)\n"), [])
        self.assertEqual(len(text_findings(pathlib.PurePath("CMakePresets.json"), '{"CMAKE_EXE_LINKER_FLAGS": "/INCREMENTAL"}')), 1)

    def test_ninja_links_that_keep_incremental_linking_are_refused(self) -> None:
        def statement(flags: str) -> str:
            return f"build src\\app\\a.exe: CXX_EXECUTABLE_LINKER__a_Debug main.cpp.obj | x.lib\n  LINK_FLAGS = {flags}\n\n"
        self.assertEqual(ninja_findings(statement("/debug /INCREMENTAL /guard:cf /INCREMENTAL:NO /MAP")), [])
        self.assertEqual(ninja_findings(statement("/debug /INCREMENTAL /subsystem:console")), [])
        self.assertEqual(ninja_findings(statement("/INCREMENTAL:NO")), [])
        for flags in ("/debug /INCREMENTAL /guard:cf", "/debug /guard:cf", "/guard:cf /INCREMENTAL:NO /incremental:yes"):
            self.assertEqual(len(ninja_findings(statement(flags))), 1, flags)
        self.assertIn("src\\app\\a.exe", ninja_findings(statement("/debug /guard:cf"))[0])

    def test_visual_studio_projects_that_keep_incremental_linking_are_refused(self) -> None:
        guarded = "<AdditionalOptions>%(AdditionalOptions) /guard:cf</AdditionalOptions>"
        on = "<LinkIncremental Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">true</LinkIncremental>"
        off = on.replace(">true<", ">false<")
        self.assertEqual(len(vcxproj_findings("a.vcxproj", guarded + on)), 1)
        self.assertEqual(vcxproj_findings("a.vcxproj", guarded + off), [])
        self.assertEqual(vcxproj_findings("a.vcxproj", on), [])


def _test_arguments(argv: list[str]) -> list[str]:
    """Take `--build-dir PATH` from the command line and leave the rest to unittest."""
    global BUILD_DIR
    unittest_argv = [argv[0]]
    index = 1
    while index < len(argv):
        if argv[index] == "--build-dir":
            if index + 1 >= len(argv):
                raise SystemExit("--build-dir requires a path")
            BUILD_DIR = pathlib.Path(argv[index + 1])
            index += 2
            continue
        unittest_argv.append(argv[index])
        index += 1
    return unittest_argv


if __name__ == "__main__":
    unittest.main(
        argv=_test_arguments(sys.argv), testRunner=SkipReportingTestRunner
    )
