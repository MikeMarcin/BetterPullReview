#!/usr/bin/env python3
"""The format gate refuses a clang-format whose verdict this tree cannot trust.

The tree is rendered by clang-format 22, exactly ([[BPR-1352]]). Release 21 and
earlier emit `{}` where this tree holds `{ }`, and no setting restores the space,
so an older tool reports several hundred false violations and `format.py` would
rewrite every one of them. Release 23 sorts includes, aligns consecutive
assignments and indents designated-initializer continuations differently, so a
file formatted on a Mac with Homebrew's default 23 failed Linux's 22 and the
other way round. A gate whose verdict depends on an unrecorded tool version is
not reproducible; these cases prove the major release is part of the verdict
rather than an assumption about the host.

The stubs here answer `--version` and record any later invocation, so a refusal
is proven by the *absence* of a formatting call rather than by the exit status
alone.

See `docs/issues/*/BPR-0367-format-gate-fails-under-clang-format-22.md`.

The later cases cover what `coding_format_check` relies on ([[BPR-0865]]).
Configure's discovery prefers `clang-format-22` and the pinned install
directories, and passes over any other release that comes first on the search
path. The scripts' own discovery, used when no `--clang-format` is named, makes
the same choice. The row's driver fails on unformatted source, and it reports a
skip, not a failure, when there is no clang-format 22.
"""

from __future__ import annotations

import io
import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from typing import List, Optional, Tuple

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))
sys.path.insert(0, str(REPOSITORY_ROOT / "_coding-standard" / "scripts"))

from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402

import _coding_standard as CODING_STANDARD  # noqa: E402
from skip_report import SkipReportingTestRunner  # noqa: E402


STANDARD_SCRIPTS = REPOSITORY_ROOT / "_coding-standard" / "scripts"
FORMAT_CHECK_SCRIPT = STANDARD_SCRIPTS / "format_check.py"
FORMAT_SCRIPT = STANDARD_SCRIPTS / "format.py"
FORMAT_CHECK_OR_SKIP_SCRIPT = STANDARD_SCRIPTS / "format_check_or_skip.py"
CLANG_FORMAT_DISCOVERY_MODULE = (
    REPOSITORY_ROOT / "_coding-standard" / "cmake" / "CodingStandardClangFormat.cmake"
)

# The code `tests/CMakeLists.txt` gives `coding_format_check` as its
# SKIP_RETURN_CODE.
SKIP_EXIT_CODE = 77

# The release below the pinned one, the pinned one, and the one above it.
# Written as text rather than derived from the constant so that moving the pin
# has to be a deliberate edit here too.
UNSUPPORTED_BANNER = "Ubuntu clang-format version 21.1.8"
SUPPORTED_BANNER = "Homebrew clang-format version 22.1.8"
NEWER_BANNER = "Homebrew clang-format version 23.1.1"

SOURCE_TEXT = "int main() {\n\treturn 0;\n}\n"


# What clang-format 22 writes for a file `--dry-run --Werror` finds unformatted.
DIRTY_DIAGNOSTIC = "probe.cpp:1:1: error: code should be clang-formatted [-Wclang-format-violations]"


def write_stub(
    aDirectory: Path,
    aBanner: str,
    aMarker: Path,
    *,
    aName: str = "clang-format-stub",
    aDirty: bool = False,
) -> Path:
    """Write an executable clang-format stub that reports `aBanner`.

    Any invocation that is not `--version` or `--help` appends to `aMarker`, so
    a test can distinguish "the gate refused" from "the gate ran and happened to
    pass". With `aDirty` that invocation reports the file unformatted, as a real
    `--dry-run --Werror` does.
    """
    if os.name == "nt":
        # Inside a parenthesised `if` block cmd reads an unescaped `)` as the
        # end of the block, so a banner such as `Android (10552028, ...)` or
        # `Debian ... (tags/RELEASE)` was truncated and the stub reported no
        # version at all. `^` escapes both brackets.
        escaped = aBanner.replace("(", "^(").replace(")", "^)")
        verdict = (
            f">&2 echo {DIRTY_DIAGNOSTIC}\r\nexit /b 1\r\n" if aDirty else "exit /b 0\r\n"
        )
        stub = aDirectory / f"{aName}.bat"
        stub.write_text(
            "@echo off\r\n"
            'if "%~1"=="--version" (\r\n'
            f"  echo {escaped}\r\n"
            "  exit /b 0\r\n"
            ")\r\n"
            'if "%~1"=="--help" (\r\n'
            "  echo   --dry-run\r\n"
            "  exit /b 0\r\n"
            ")\r\n"
            f'>>"{aMarker}" echo formatted\r\n'
            f"{verdict}",
            encoding="ascii",
            newline="",
        )
        return stub

    verdict = f'echo "{DIRTY_DIAGNOSTIC}" >&2\nexit 1\n' if aDirty else "exit 0\n"
    stub = aDirectory / aName
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
        f"{verdict}",
        encoding="ascii",
    )
    stub.chmod(0o755)
    return stub


class clang_format_version_parsing(unittest.TestCase):
    def test_the_project_pins_the_release_that_renders_this_tree(self) -> None:
        self.assertEqual(CODING_STANDARD.REQUIRED_CLANG_FORMAT_MAJOR, 22)

    def test_only_major_release_22_is_the_required_tool(self) -> None:
        for version, required in (
            ((22,), True),
            ((22, 1, 8), True),
            ((22, 0, 0), True),
            ((21, 1, 8), False),
            ((23, 1, 1), False),
            ((24,), False),
            ((), False),
            (None, False),
        ):
            self.assertEqual(
                CODING_STANDARD.is_required_clang_format(version), required, version
            )

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
                    # A vendor prefix carrying a build number. An unanchored
                    # parse reads this as version 10552028, comfortably above
                    # any minimum -- and 17 cannot read `.clang-format` at all.
                    (
                        "Android (10552028, +pgo, +bolt, +lto, +mlgo, based on"
                        " r487747c) clang-format version 17.0.2",
                        (17, 0, 2),
                    ),
                    (
                        "Debian clang-format version 21.1.0-1~exp1 (tags/RELEASE)",
                        (21, 1, 0),
                    ),
                )
            ):
                stub_root = root / f"banner-{index}"
                stub_root.mkdir(parents=True, exist_ok=True)
                stub = write_stub(stub_root, banner, marker)
                self.assertEqual(CODING_STANDARD.clang_format_version(str(stub)), expected, banner)

    def test_a_banner_without_the_phrase_is_unreadable_rather_than_guessed(
        self,
    ) -> None:
        """No phrase, no version. Guessing from a stray number is how 17 passed."""
        with ContainedTemporaryDirectory() as scratch:
            root = Path(scratch)
            stub = write_stub(root, "some other tool 99.9", root / "invoked.log")
            self.assertIsNone(CODING_STANDARD.clang_format_version(str(stub)))

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

    def test_format_check_refuses_a_newer_release_and_says_how_to_get_22(self) -> None:
        """Homebrew's default llvm is 23; its verdict disagrees with Linux's 22."""
        completed, formatted = self.run_gate(FORMAT_CHECK_SCRIPT, NEWER_BANNER)
        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertIn("unsupported clang-format", completed.stderr)
        self.assertIn("clang-format-stub reports version 23.1.1", completed.stderr)
        self.assertIn("clang-format 22 exactly", completed.stderr)
        self.assertIn("brew install llvm@22", completed.stderr)
        self.assertIn("apt install clang-format-22", completed.stderr)
        self.assertFalse(formatted, "the gate reached a clang-format 23 despite refusing it")

    def test_format_refuses_a_newer_release_before_it_rewrites_anything(self) -> None:
        completed, formatted = self.run_gate(FORMAT_SCRIPT, NEWER_BANNER)
        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertIn("reports version 23.1.1", completed.stderr)
        self.assertIn("rewrite", completed.stderr)
        self.assertFalse(formatted, "a clang-format 23 was allowed to rewrite files")

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


class the_format_check_row_fails_on_rot_and_skips_without_a_tool(unittest.TestCase):
    """`coding_format_check` runs format_check_or_skip.py ([[BPR-0865]]).

    A missing or old tool must end in the skip code, and it must never ask that
    tool to format anything. Unformatted source must end in a failure, never in
    a skip.
    """

    def run_row(
        self, aBanner: Optional[str], *, aDirty: bool = False, aTool: Optional[str] = None
    ) -> Tuple[subprocess.CompletedProcess, bool]:
        with ContainedTemporaryDirectory() as scratch:
            root = Path(scratch)
            marker = root / "invoked.log"
            (root / "src").mkdir()
            (root / "src" / "probe.cpp").write_text(SOURCE_TEXT, encoding="ascii")
            arguments: List[str] = []
            if aBanner is not None:
                stub_root = root / "stub"
                stub_root.mkdir()
                stub = write_stub(stub_root, aBanner, marker, aDirty=aDirty)
                arguments = ["--clang-format", str(stub)]
            elif aTool is not None:
                arguments = ["--clang-format", aTool]

            completed = subprocess.run(
                [
                    sys.executable,
                    str(FORMAT_CHECK_OR_SKIP_SCRIPT),
                    "--root",
                    str(root),
                    "--skip-exit-code",
                    str(SKIP_EXIT_CODE),
                    *arguments,
                ],
                cwd=str(REPOSITORY_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            return completed, marker.exists()

    def test_formatted_source_passes_with_a_supported_tool(self) -> None:
        completed, formatted = self.run_row(SUPPORTED_BANNER)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("Formatting check passed.", completed.stdout)
        self.assertTrue(formatted, "the row passed without asking clang-format")

    def test_unformatted_source_fails_and_is_never_a_skip(self) -> None:
        completed, formatted = self.run_row(SUPPORTED_BANNER, aDirty=True)
        self.assertEqual(completed.returncode, 1, completed.stdout + completed.stderr)
        self.assertIn("Files needing formatting:", completed.stdout)
        self.assertIn("src/probe.cpp", completed.stdout)
        self.assertNotIn("SKIPPED", completed.stdout)
        self.assertTrue(formatted)

    def test_no_tool_from_configure_is_a_skip(self) -> None:
        completed, _ = self.run_row(None)
        self.assertEqual(completed.returncode, SKIP_EXIT_CODE, completed.stdout + completed.stderr)
        self.assertIn("SKIPPED: configure found no clang-format 22;", completed.stdout)
        self.assertIn("brew install llvm@22", completed.stdout)
        self.assertIn("not a formatting verdict", completed.stdout)

    def test_an_older_tool_is_a_skip_and_is_never_asked_to_format(self) -> None:
        completed, formatted = self.run_row(UNSUPPORTED_BANNER)
        self.assertEqual(completed.returncode, SKIP_EXIT_CODE, completed.stdout + completed.stderr)
        self.assertIn("reports version 21.1.8", completed.stdout)
        self.assertFalse(formatted, "an unsupported clang-format was asked for a verdict")

    def test_a_newer_tool_is_a_skip_and_is_never_asked_to_format(self) -> None:
        completed, formatted = self.run_row(NEWER_BANNER, aDirty=True)
        self.assertEqual(completed.returncode, SKIP_EXIT_CODE, completed.stdout + completed.stderr)
        self.assertIn("reports version 23.1.1", completed.stdout)
        self.assertIn("clang-format 22 exactly", completed.stdout)
        self.assertIn("-DBPR_CLANG_FORMAT_EXECUTABLE", completed.stdout)
        self.assertFalse(formatted, "a clang-format 23 was asked for a verdict")

    def test_an_older_tool_is_a_skip_even_when_the_source_is_unformatted(self) -> None:
        """A missing or old tool gives no verdict, whatever the tree holds."""
        completed, formatted = self.run_row(UNSUPPORTED_BANNER, aDirty=True)
        self.assertEqual(completed.returncode, SKIP_EXIT_CODE, completed.stdout + completed.stderr)
        self.assertFalse(formatted)

    def test_an_unreadable_version_is_a_skip(self) -> None:
        completed, formatted = self.run_row("some other tool 99.9")
        self.assertEqual(completed.returncode, SKIP_EXIT_CODE, completed.stdout + completed.stderr)
        self.assertIn("cannot read a version", completed.stdout)
        self.assertFalse(formatted)

    def test_a_configured_tool_that_has_gone_is_a_skip(self) -> None:
        completed, _ = self.run_row(None, aTool=str(REPOSITORY_ROOT / "no-such-clang-format"))
        self.assertEqual(completed.returncode, SKIP_EXIT_CODE, completed.stdout + completed.stderr)
        self.assertIn("reconfigure", completed.stdout)

    def test_the_skip_code_cannot_be_one_of_the_gate_verdicts(self) -> None:
        for gate_code in ("0", "1", "2"):
            completed = subprocess.run(
                [
                    sys.executable,
                    str(FORMAT_CHECK_OR_SKIP_SCRIPT),
                    "--root",
                    str(REPOSITORY_ROOT),
                    "--skip-exit-code",
                    gate_code,
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(completed.returncode, 2, gate_code)
            self.assertIn("must differ from the gate's own exit codes", completed.stderr)


@unittest.skipIf(
    os.name == "nt",
    "find_program on Windows looks for .com and .exe files, and these stubs are batch files",
)
class configure_finds_clang_format_22(unittest.TestCase):
    """CodingStandardClangFormat.cmake, run in script mode against stubs ([[BPR-0865]], [[BPR-1352]]).

    The Linux station's PATH gives clang-format 21.1.8 first and has 22.1.8 at
    /usr/bin/clang-format. The Mac's PATH gives Homebrew's 23.1.1 and has 22.1.8
    in the keg-only llvm@22. Stopping at the first one would skip the row, or on
    the Mac give a verdict the Linux station disagrees with.
    """

    def discover(
        self,
        aTools: List[Tuple[str, str]],
        *,
        aHints: Optional[List[int]] = None,
        aCached: Optional[int] = None,
    ) -> Tuple[Optional[int], str]:
        """Return the index of the stub discovery chose (None for NOTFOUND) and its output.

        `aTools` lists `(banner, name)`; each stub gets its own directory, and
        PATH holds them in order. `aHints` names the stubs whose directories are
        the pinned install directories; without it there are none, so the
        station's own llvm@22 cannot answer for a stub. `aCached` names a stub
        already in BPR_CLANG_FORMAT_EXECUTABLE, as a tree configured earlier
        holds. Compared by real path, so a temporary root reached through a
        symlink, as /var is on macOS, still matches.
        """
        cmake = CMAKE_COMMAND or shutil.which("cmake")
        if cmake is None:
            self.skipTest("no cmake to run the discovery module with")
        with ContainedTemporaryDirectory() as scratch:
            root = Path(scratch)
            directories: List[str] = []
            stubs: List[str] = []
            for index, (banner, name) in enumerate(aTools):
                directory = root / f"tool-{index}"
                directory.mkdir()
                stubs.append(
                    str(write_stub(directory, banner, root / "invoked.log", aName=name))
                )
                directories.append(str(directory))

            hints = ";".join(directories[index] for index in (aHints or []))
            definitions = [
                f"-DMODULE={CLANG_FORMAT_DISCOVERY_MODULE}",
                f"-DPYTHON={sys.executable}",
                f"-DCODING_STANDARD_CLANG_FORMAT_HINTS={hints}",
            ]
            if aCached is not None:
                definitions.append(f"-DBPR_CLANG_FORMAT_EXECUTABLE={stubs[aCached]}")

            script = root / "discover.cmake"
            script.write_text(
                'include("${MODULE}")\n'
                'coding_standard_find_clang_format("${PYTHON}")\n'
                'message(STATUS "discovered=[${BPR_CLANG_FORMAT_EXECUTABLE}]")\n',
                encoding="ascii",
            )
            environment = dict(os.environ)
            for variable in (
                "CMAKE_PREFIX_PATH",
                "CMAKE_PROGRAM_PATH",
                "CMAKE_APPBUNDLE_PATH",
                "CMAKE_FRAMEWORK_PATH",
            ):
                environment.pop(variable, None)
            environment["PATH"] = os.pathsep.join(directories)
            completed = subprocess.run(
                [cmake, *definitions, "-P", str(script)],
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            self.assertFalse(
                (root / "invoked.log").exists(), "discovery asked a candidate to format"
            )
            reported = [
                line.split("discovered=[", 1)[1].rsplit("]", 1)[0]
                for line in completed.stdout.splitlines()
                if "discovered=[" in line
            ]
            self.assertEqual(len(reported), 1, completed.stdout)
            found = reported[0]
            if found == "BPR_CLANG_FORMAT_EXECUTABLE-NOTFOUND":
                return None, completed.stdout
            for index, stub in enumerate(stubs):
                if os.path.realpath(found) == os.path.realpath(stub):
                    return index, completed.stdout
            self.fail(f"discovery chose {found}, which is none of the stubs: {completed.stdout}")

    def test_an_older_tool_first_on_the_path_is_passed_over(self) -> None:
        chosen, _ = self.discover(
            [(UNSUPPORTED_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format")]
        )
        self.assertEqual(chosen, 1)

    def test_a_newer_tool_first_on_the_path_is_passed_over(self) -> None:
        """The Mac's case: Homebrew's 23 on PATH ahead of a 22."""
        chosen, _ = self.discover(
            [(NEWER_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format")]
        )
        self.assertEqual(chosen, 1)

    def test_the_first_22_is_taken(self) -> None:
        chosen, _ = self.discover(
            [(SUPPORTED_BANNER, "clang-format"), (NEWER_BANNER, "clang-format")]
        )
        self.assertEqual(chosen, 0)

    def test_clang_format_22_by_name_comes_before_clang_format_earlier_on_the_path(
        self,
    ) -> None:
        """Debian's clang-format-22, even behind another directory's clang-format 22."""
        chosen, _ = self.discover(
            [(SUPPORTED_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format-22")]
        )
        self.assertEqual(chosen, 1)

    def test_a_pinned_install_directory_comes_before_the_path(self) -> None:
        """Homebrew's keg-only llvm@22 is never on PATH; it is a hint."""
        chosen, _ = self.discover(
            [(SUPPORTED_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format")],
            aHints=[1],
        )
        self.assertEqual(chosen, 1)

    def test_only_an_older_tool_is_not_found(self) -> None:
        chosen, _ = self.discover([(UNSUPPORTED_BANNER, "clang-format")])
        self.assertIsNone(chosen)

    def test_only_a_newer_tool_is_not_found(self) -> None:
        chosen, _ = self.discover([(NEWER_BANNER, "clang-format")])
        self.assertIsNone(chosen)

    def test_a_cached_newer_tool_is_reported_and_replaced_by_a_22(self) -> None:
        """A tree configured before the pin holds Homebrew's 23."""
        chosen, output = self.discover(
            [(NEWER_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format")],
            aCached=0,
        )
        self.assertEqual(chosen, 1, output)
        self.assertIn("needs clang-format 22 exactly", output)
        self.assertIn("reports version 23.1.1", output)

    def test_a_cached_22_is_kept(self) -> None:
        chosen, output = self.discover(
            [(SUPPORTED_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format")],
            aCached=1,
        )
        self.assertEqual(chosen, 1, output)
        self.assertNotIn("needs clang-format 22 exactly", output)


class the_scripts_find_clang_format_22_when_none_is_named(unittest.TestCase):
    """`format.py` and `format_check.py` without `--clang-format` ([[BPR-1352]]).

    The same order as configure: `clang-format-22` on PATH, then each pinned
    install directory, then `clang-format` on PATH.
    """

    def setUp(self) -> None:
        self.scratch = ContainedTemporaryDirectory()
        self.root = Path(self.scratch.__enter__())
        self.marker = self.root / "invoked.log"
        self.saved_path = os.environ.get("PATH")
        self.saved_directories = CODING_STANDARD.PINNED_CLANG_FORMAT_DIRECTORIES

    def tearDown(self) -> None:
        CODING_STANDARD.PINNED_CLANG_FORMAT_DIRECTORIES = self.saved_directories
        if self.saved_path is None:
            os.environ.pop("PATH", None)
        else:
            os.environ["PATH"] = self.saved_path
        self.scratch.__exit__(None, None, None)

    def station(
        self, aPath: List[Tuple[str, str]], aPinned: List[Tuple[str, str]]
    ) -> Tuple[List[str], List[str]]:
        """Lay out stubs on PATH and in pinned directories; return their paths."""
        on_path: List[str] = []
        pinned: List[str] = []
        path_directories: List[str] = []
        pinned_directories: List[str] = []
        for group, stubs, directories, label in (
            (aPath, on_path, path_directories, "path"),
            (aPinned, pinned, pinned_directories, "pinned"),
        ):
            for index, (banner, name) in enumerate(group):
                directory = self.root / f"{label}-{index}"
                directory.mkdir()
                stubs.append(str(write_stub(directory, banner, self.marker, aName=name)))
                directories.append(str(directory))
        os.environ["PATH"] = os.pathsep.join(path_directories)
        CODING_STANDARD.PINNED_CLANG_FORMAT_DIRECTORIES = tuple(pinned_directories)
        return on_path, pinned

    def assertSameTool(self, aFound: str, aExpected: str) -> None:
        self.assertEqual(os.path.realpath(aFound), os.path.realpath(aExpected))

    def test_clang_format_22_on_the_path_comes_first(self) -> None:
        on_path, _ = self.station(
            [(NEWER_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format-22")],
            [(SUPPORTED_BANNER, "clang-format")],
        )
        self.assertSameTool(CODING_STANDARD.find_clang_format(), on_path[1])

    @unittest.skipIf(os.name == "nt", "the pinned directories hold clang-format.exe on Windows")
    def test_a_pinned_22_comes_before_a_newer_clang_format_on_the_path(self) -> None:
        """The Mac: PATH gives Homebrew's 23, and llvm@22 holds the 22."""
        _, pinned = self.station(
            [(NEWER_BANNER, "clang-format")], [(SUPPORTED_BANNER, "clang-format")]
        )
        self.assertSameTool(CODING_STANDARD.find_clang_format(), pinned[0])

    @unittest.skipIf(os.name == "nt", "the pinned directories hold clang-format.exe on Windows")
    def test_a_pinned_directory_that_is_not_22_is_passed_over(self) -> None:
        on_path, _ = self.station(
            [(SUPPORTED_BANNER, "clang-format")], [(NEWER_BANNER, "clang-format")]
        )
        self.assertSameTool(CODING_STANDARD.find_clang_format(), on_path[0])

    def test_without_a_22_the_path_tool_is_returned_for_the_refusal_to_name(self) -> None:
        on_path, _ = self.station([(NEWER_BANNER, "clang-format")], [])
        found = CODING_STANDARD.find_clang_format()
        self.assertSameTool(found, on_path[0])
        diagnostic = io.StringIO()
        usable, version = CODING_STANDARD.check_clang_format_version(found, stream=diagnostic)
        self.assertFalse(usable)
        self.assertEqual(version, (23, 1, 1))
        self.assertIn(f"{found} reports version 23.1.1", diagnostic.getvalue())
        self.assertIn("brew install llvm@22", diagnostic.getvalue())

    def test_no_clang_format_at_all_says_how_to_install_22(self) -> None:
        self.station([], [])
        with self.assertRaises(FileNotFoundError) as raised:
            CODING_STANDARD.find_clang_format()
        self.assertIn("clang-format 22", str(raised.exception))
        self.assertIn("brew install llvm@22", str(raised.exception))
        self.assertIn("apt install clang-format-22", str(raised.exception))

    def test_format_check_without_a_named_tool_runs_the_22_it_finds(self) -> None:
        on_path, _ = self.station(
            [(NEWER_BANNER, "clang-format"), (SUPPORTED_BANNER, "clang-format-22")], []
        )
        source = self.root / "probe.cpp"
        source.write_text(SOURCE_TEXT, encoding="ascii")
        completed = subprocess.run(
            [sys.executable, str(FORMAT_CHECK_SCRIPT), "--root", str(self.root), str(source)],
            cwd=str(REPOSITORY_ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("(22.1.8)", completed.stdout)
        self.assertIn("clang-format-22", completed.stdout)
        self.assertTrue(self.marker.exists(), "the found clang-format 22 was never invoked")


class configure_and_the_scripts_search_the_same_places(unittest.TestCase):
    def test_the_cmake_lookup_names_every_pinned_name_and_directory(self) -> None:
        module = CLANG_FORMAT_DISCOVERY_MODULE.read_text(encoding="utf-8")
        names = " ".join(CODING_STANDARD.PINNED_CLANG_FORMAT_NAMES)
        self.assertIn(f"NAMES {names}\n", module)
        hints = module.split("set(\n        CODING_STANDARD_CLANG_FORMAT_HINTS\n", 1)[1]
        hints = hints.split(")", 1)[0].split()
        self.assertEqual(hints, list(CODING_STANDARD.PINNED_CLANG_FORMAT_DIRECTORIES))


class the_installed_tool_is_the_pinned_release(unittest.TestCase):
    def test_this_host_can_produce_a_gate_verdict_or_says_why_not(self) -> None:
        try:
            tool = CODING_STANDARD.find_clang_format()
        except FileNotFoundError as exc:
            self.skipTest(f"no clang-format on this host: {exc}")

        version = CODING_STANDARD.clang_format_version(tool)
        self.assertIsNotNone(version, f"{tool} reports no parseable version")
        if not CODING_STANDARD.is_required_clang_format(version):
            self.skipTest(
                f"{tool} is {CODING_STANDARD.version_text(version)}; this tree is rendered by"
                f" clang-format {CODING_STANDARD.REQUIRED_CLANG_FORMAT_MAJOR} exactly,"
                " so the C/C++ format gate cannot return a verdict here"
            )


def _test_arguments(aArgv: List[str]) -> Tuple[List[str], Optional[str]]:
    """Take `--cmake PATH` from the command line and leave the rest to unittest."""
    unittest_argv = [aArgv[0]]
    cmake: Optional[str] = None
    index = 1
    while index < len(aArgv):
        if aArgv[index] == "--cmake":
            if index + 1 >= len(aArgv):
                raise SystemExit("--cmake requires a path")
            cmake = aArgv[index + 1]
            index += 2
            continue
        unittest_argv.append(aArgv[index])
        index += 1
    return unittest_argv, cmake


UNITTEST_ARGV, CMAKE_COMMAND = _test_arguments(sys.argv)


if __name__ == "__main__":
    unittest.main(argv=UNITTEST_ARGV, testRunner=SkipReportingTestRunner)
