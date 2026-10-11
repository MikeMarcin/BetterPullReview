#!/usr/bin/env python3
from __future__ import annotations

import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"})
CONCRETE_SELECTION_PATTERNS = (
    re.compile(r"\bmake_github_provider\s*\("),
    re.compile(r"\bmake_macos_keychain_vault\s*\("),
    re.compile(r"\bmake_windows_credential_manager_vault\s*\("),
    re.compile(r"\bmake_linux_secret_service_vault\s*\("),
    re.compile(r"\bnative_github_wire_transport_factory\b"),
    re.compile(r"\bworker_executor\b"),
    # BPR-0142's factory, forbidden here before it exists. This list covers a
    # curated subset of src/, so it is not the whole guarantee: clause (C) of
    # `live_mutation_isolation` walks the entire tree with a two-path
    # allowlist. This entry is the part that fails in the same suite that
    # already owns concrete-selection policy.
    re.compile(r"\bmake_live_review_mutation_provider\s*\("),
    re.compile(r"\bmake_live_review_submission_provider\s*\("),
)
RELEASE_BINARY_FORBIDDEN_BYTES = (
    b"--github-fixture",
    b"--epoch-fixture-root",
    b"--epoch-fixture-phase",
    b"--native-appearance-probe",
    b"--changed-render-evidence",
    b"--diff-workbench-probe",
    b"--diff-workbench-shutdown-probe",
    b"--diff-interaction-evidence",
    b"EPOCH_FIXTURE_SEED_OK",
    b"EPOCH_FIXTURE_REUSE_OK",
    b"EPOCH_FIXTURE_SEED_FAILED",
    b"EPOCH_FIXTURE_REUSE_FAILED",
    b"GITHUB_FIXTURE_OK",
    b"GITHUB_FIXTURE_FAILED",
    b"GITHUB_FIXTURE_EVIDENCE",
    b"REVIEW_MUTATION_PROVIDER:fixture",
    b"NATIVE_APPEARANCE_",
    b"DIFF_WORKBENCH_",
    b"DIFF_INTERACTION_EVIDENCE_",
    b"AppleInterfaceStyle",
    b"CFPreferencesCopyValue",
    b"CFPreferencesSynchronize",
    b"Recorded native HTTPS",
    b"PRRT_fixture_1",
    b"github_application_fixture_access",
    b"fail_next_epoch_job_admission",
    b"clear_epoch_job_admission_failure",
    b"probe_private_descriptor",
    b"github_public_path_fixture",
    b"tests/fixtures/github/",
    b"tests\\fixtures\\github\\",
    b"tests/fixtures/github/contracts/v1",
    b"tests/fixtures/github/capabilities/v1",
    # [[BPR-0797]], [[BPR-0802]]. The automated-test boundary marker. A shipped
    # build has no boundary to be inside, so it must carry no reader that a
    # stray environment variable could reach; the composition reads the name
    # only in the tests-enabled graph.
    b"BPR_AUTOMATED_TEST_BOUNDARY",
)
# [[BPR-0982]]. The UTF-8 `gh sign-in` label went with the precomposed CLI
# rows ([[BPR-0909]]); the release self-test below now observes the Use
# GitHub CLI entry the shipped graph offers.
MAC_CLI_BINARY_MARKERS = (
    b"gh_cli_transport",
    b"github_cli_process_runner",
    b"validate_gh_cli_configuration",
    b"compile_gh_cli_process_request",
)


def parse_arguments() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument(
        "--cmake",
        default=shutil.which("cmake") or "cmake",
    )
    parser.add_argument(
        "--cxx",
        default=shutil.which("c++") or "c++",
    )
    parser.add_argument("--qt-root", default="")
    parser.add_argument("--qt-dir", default="")
    parser.add_argument("--cpm-source-cache", default="")
    parser.add_argument("--release-build-dir", type=Path)
    return parser.parse_known_args()


ARGUMENTS, UNITTEST_ARGUMENTS = parse_arguments()


# What a diagnostic must say for a hostile snippet to have been stopped by the
# seal it targets rather than by an unrelated mistake. `review_mutation_-
# execution_authority` is only forward-declared in the public header, so both
# GCC and Clang reject its construction for incompleteness before access
# control is ever consulted; the entity is unusable either way.
ACCESS_CONTROLLED = r"private"
# Each reason carries the Clang/GCC spelling and MSVC's: an incomplete type
# is "use of undefined type" there, and an unknown name is an "undeclared
# identifier" (BPR-0440).
UNCONSTRUCTIBLE = r"(private|incomplete type|undefined type)"
UNDECLARED = r"(no member named|not a member|undeclared identifier)"
REDEFINED = r"(private|incomplete type|undefined type|redefinition)"

SNIPPET_INCLUDE_ARGUMENTS = (
    f"-I{REPOSITORY_ROOT / 'src/application/include'}",
    f"-I{REPOSITORY_ROOT / 'src/core/include'}",
)


def msvc_compiler() -> bool:
    return Path(ARGUMENTS.cxx).name.lower() in {"cl", "cl.exe"}


def compile_snippet(
    temporary_root: Path,
    name: str,
    source: str,
) -> subprocess.CompletedProcess[str]:
    path = temporary_root / f"{name}.cpp"
    path.write_text(source, encoding="utf-8")
    if msvc_compiler():
        # cl has no -std or -fsyntax-only; /Zs is the syntax-only check, and
        # the language level is the one the build itself selects (BPR-0440).
        arguments = [
            ARGUMENTS.cxx,
            "/nologo",
            "/std:c++latest",
            "/permissive-",
            "/EHsc",
            "/utf-8",
            "/Zs",
        ]
    else:
        arguments = [
            ARGUMENTS.cxx,
            "-std=c++23",
            "-fsyntax-only",
            "-D_LIBCPP_ENABLE_EXPERIMENTAL",
        ]
    return subprocess.run(
        [
            *arguments,
            *SNIPPET_INCLUDE_ARGUMENTS,
            str(path),
        ],
        check=False,
        capture_output=True,
        text=True,
        timeout=30,
    )


def diagnostic_messages(
    completed: subprocess.CompletedProcess[str],
) -> list[str]:
    """The lowercased message of every reported error, without its location.

    Everything before `error:` on a diagnostic line is the source path, and a
    path is not evidence about the code. Keeping it lets a directory name
    satisfy an assertion about what the compiler said: the temporary root on
    macOS lands under `/private/var/folders`, so a search of the raw output for
    `private` succeeded there however the seal behaved. Compiling the same
    hostile snippet from a directory named `/private/var/folders/xy` on Linux
    reproduces that directly.
    """
    combined = completed.stdout + completed.stderr
    # Clang and GCC write `path:line:col: error: message`; cl writes
    # `path(line): error C2039: message`. Both end at the same message.
    return [
        match.group(1).strip().lower()
        for line in combined.splitlines()
        if (match := re.search(r"error(?: C\d+)?:(.*)$", line))
    ]


def read(relative_path: str) -> str:
    return (REPOSITORY_ROOT / relative_path).read_text(encoding="utf-8")


def strip_cxx_comments(source: str) -> str:
    """Blank out C++ comments, keeping string and character literals intact.

    A rule about what a translation unit compiles must not trip on prose that
    names the thing it forbids ([[BPR-0821]]). Newlines survive so a caller's
    line numbers and multi-line patterns keep their shape.
    """
    result: list[str] = []
    index = 0
    length = len(source)
    while index < length:
        character = source[index]
        following = source[index + 1] if index + 1 < length else ""
        if character == "/" and following == "/":
            end = source.find("\n", index)
            end = length if end == -1 else end
            result.append(" " * (end - index))
            index = end
        elif character == "/" and following == "*":
            end = source.find("*/", index + 2)
            end = length if end == -1 else end + 2
            result.append(re.sub(r"[^\n]", " ", source[index:end]))
            index = end
        elif character == '"' or (
            # A quote after a digit is a C++14 digit separator (`1'024U`),
            # not the start of a character literal.
            character == "'" and not (index > 0 and source[index - 1].isalnum())
        ):
            end = index + 1
            while end < length and source[end] != character:
                end += 2 if source[end] == "\\" else 1
            end = min(end + 1, length)
            result.append(source[index:end])
            index = end
        else:
            result.append(character)
            index += 1
    return "".join(result)


def ninja_build_edges(build_graph: str) -> list[str]:
    """Return the `build` statements that make or consume build products.

    The diagnostic build identity (`src/adapters/logging/CMakeLists.txt`)
    hashes every file under `src/`, so each one is a configure dependency: the
    generator names it in the `RERUN_CMAKE` statement and in the `phony`
    statement that declares those dependencies. Neither compiles, links or
    runs anything. Every other statement is kept, `$` line continuations
    joined, so a file that feeds a compile, link or custom command is found
    ([[BPR-1228]]).
    """
    statements: list[str] = []
    current = ""
    for line in build_graph.splitlines():
        if current:
            current += " " + line.strip()
        elif line.startswith("build "):
            current = line
        else:
            continue
        if current.endswith("$"):
            current = current[:-1]
            continue
        statements.append(current)
        current = ""
    edges: list[str] = []
    for statement in statements:
        rule_and_inputs = re.split(r"(?<!\$):", statement, maxsplit=1)
        if len(rule_and_inputs) != 2:
            edges.append(statement)
            continue
        words = rule_and_inputs[1].split()
        rule = words[0] if words else ""
        if rule == "RERUN_CMAKE" or (rule == "phony" and len(words) == 1):
            continue
        edges.append(statement)
    return edges


# [[BPR-0797]] moved both GitHub CLI consent gates from the preprocessor to
# one run-time question. What the contract protects did not move: a macOS
# graph that can reach the configured `gh` shows the reader's trust warning and
# the per-publication confirmation, and a run inside the automated test
# boundary cannot reach either ([[BPR-0821]]).
CLI_READ_OFFERED_DEFINITION = re.compile(
    r"auto\s+cli_read_offered\(\s*\[\[maybe_unused\]\]\s*const\s+bool\s+"
    r"aInsideAutomatedTestBoundary\s*\)\s*noexcept\s*->\s*bool\s*\{\s*"
    r"#\s*if\s+defined\(__APPLE__\)\s*"
    r"#\s*if\s+defined\(BPR_BUILD_TESTING\)\s*return\s+!aInsideAutomatedTestBoundary\s*;\s*"
    r"#\s*else\s*return\s+true\s*;\s*#\s*endif\s*"
    r"#\s*else\s*return\s+false\s*;\s*#\s*endif\s*\}"
)
# [[BPR-0982]]. [[BPR-0909]] let a tests-enabled macOS run that carries
# fixture-discovered CLI accounts offer the read, so its discovery courses
# meet the real read opt-in. That only widens where the question is asked;
# the accounts exist only with a fixture (checked below), non-Apple graphs
# still answer false, and the shipped graph still asks its one question.
CLI_READ_OFFERED_HERE_DEFINITION = re.compile(
    r"auto\s+cli_read_offered_here\(\s*\)\s*const\s+noexcept\s*->\s*bool\s*\{\s*"
    r"#\s*if\s+defined\(BPR_BUILD_TESTING\)\s*"
    r"#\s*if\s+defined\(__APPLE__\)\s*"
    r"return\s+GithubCliDiscoveryAccounts\.has_value\(\)\s*\|\|\s*"
    r"cli_read_offered\(\s*InsideAutomatedTestBoundary\s*\)\s*;\s*"
    r"#\s*else\s*return\s+false\s*;\s*#\s*endif\s*"
    r"#\s*else\s*return\s+cli_read_offered\(\s*false\s*\)\s*;\s*#\s*endif\s*\}"
)
CLI_DISCOVERY_ACCOUNTS_ASSIGNMENT = re.compile(
    r"GithubCliDiscoveryAccounts\s*=\s*aFixture\.has_value\(\)\s*"
    r"\?\s*aFixtureOptions\.GithubCliDiscoveryAccounts\s*:\s*std::nullopt\s*;"
)
# Each consent's title and the call that asks it. [[BPR-0933]] moved the read
# opt-in's layout into the Qt adapter, so it is asked through that adapter's
# question rather than a static warning box; the gate around it is unchanged.
CLI_CONSENT_QUESTIONS = (
    ("Confirm GitHub CLI publication", r"QMessageBox::warning"),
    ("Use configured GitHub CLI", r"qt::ask_github_cli_read_consent"),
)


def cli_consent_gate_findings(composition_source: str) -> list[str]:
    """Name every way the composition source loses a GitHub CLI consent gate."""
    code = strip_cxx_comments(composition_source)
    findings: list[str] = []
    if len(CLI_READ_OFFERED_DEFINITION.findall(code)) != 1:
        findings.append(
            "cli_read_offered must answer !inside-boundary in the tests-enabled "
            "macOS graph, true in the shipped macOS graph and false elsewhere"
        )
    if len(CLI_READ_OFFERED_HERE_DEFINITION.findall(code)) != 1:
        findings.append(
            "cli_read_offered_here must pass the run's boundary answer in the "
            "tests-enabled macOS graph, widened only by fixture-discovered "
            "accounts, and the unconditional answer in the shipped graph"
        )
    if (
        len(CLI_DISCOVERY_ACCOUNTS_ASSIGNMENT.findall(code)) != 1
        or len(re.findall(r"GithubCliDiscoveryAccounts\s*=[^=]", code)) != 1
    ):
        findings.append(
            "fixture-discovered CLI accounts must be assigned once, and only "
            "from a fixture"
        )
    for title, question in CLI_CONSENT_QUESTIONS:
        gate = re.compile(
            r"#\s*if\s+defined\(__APPLE__\)\s*"
            r"if\s*\(\s*Profile->cli_read_offered_here\(\s*\)\s*\)\s*\{\s*"
            r"const\s+auto\s+consent\s*=\s*" + question + r"\(\s*"
            r"Window\.get\(\)\s*,\s*QStringLiteral\(\s*\"" + re.escape(title) + r"\"\s*\)"
        )
        if code.count(f'"{title}"') != 1 or len(gate.findall(code)) != 1:
            findings.append(
                f"the {title!r} consent must appear once, compiled for macOS "
                "and asked whenever the run-time CLI offer is open"
            )
    return findings


# [[BPR-0802]]. The automated-test boundary has one reader, and it is the
# composition, where the browser and the GitHub CLI factory are chosen. While
# `src/app/main.cpp` read it and passed it in, the seam covered the application
# binary and no other composition root: a test composing `github_application`
# in process, with no fixture, was outside the boundary whatever its CTest row
# said. A second reader would bring that back for whichever root it missed.
#
# [[BPR-0983]]. The updater and the update lifetime had become second readers,
# in every graph: a stray variable in a person's shipped build stopped installs
# and the update lifetime, and the tests-disabled binary carried the name the
# Release scan forbids. The one reader is now a function the composition
# defines, `inside_automated_test_boundary`, which reads the marker only in the
# tests-enabled graph and answers false in a tests-disabled one; a profile's
# composition, the updater and the update lifetime all ask it.
AUTOMATED_TEST_BOUNDARY_LITERAL = '"BPR_AUTOMATED_TEST_BOUNDARY"'
AUTOMATED_TEST_BOUNDARY_READER_PATH = "src/app/src/github_application.cpp"
AUTOMATED_TEST_BOUNDARY_READER = re.compile(
    r"auto\s+inside_automated_test_boundary\(\s*\)\s*->\s*bool\s*\{\s*"
    r"#if\s+defined\(\s*BPR_BUILD_TESTING\s*\)\s*"
    r"return\s+!qEnvironmentVariableIsEmpty\(\s*"
    + re.escape(AUTOMATED_TEST_BOUNDARY_LITERAL)
    + r"\s*\)\s*;\s*#else\s*return\s+false\s*;\s*#endif\s*\}"
)
AUTOMATED_TEST_BOUNDARY_PROFILE_ANSWER = re.compile(
    r"InsideAutomatedTestBoundary\s*=\s*inside_automated_test_boundary\(\s*\)"
    r"\s*\|\|\s*aFixture\.has_value\(\s*\)\s*;"
)


def automated_test_boundary_reader_findings(sources: dict[str, str]) -> list[str]:
    """Name every way `src/` stops reading the boundary marker in one place."""
    counts = {
        path: strip_cxx_comments(text).count(AUTOMATED_TEST_BOUNDARY_LITERAL)
        for path, text in sources.items()
    }
    findings: list[str] = []
    elsewhere = sorted(
        path
        for path, count in counts.items()
        if count and path != AUTOMATED_TEST_BOUNDARY_READER_PATH
    )
    if elsewhere:
        findings.append(
            "the automated-test boundary marker is read outside the "
            f"composition: {elsewhere!r}"
        )
    composition = strip_cxx_comments(
        sources.get(AUTOMATED_TEST_BOUNDARY_READER_PATH, "")
    )
    if (
        counts.get(AUTOMATED_TEST_BOUNDARY_READER_PATH, 0) != 1
        or len(AUTOMATED_TEST_BOUNDARY_READER.findall(composition)) != 1
    ):
        findings.append(
            "the composition must read the marker exactly once, in "
            "inside_automated_test_boundary, and only in the tests-enabled graph"
        )
    if len(AUTOMATED_TEST_BOUNDARY_PROFILE_ANSWER.findall(composition)) != 1:
        findings.append(
            "a profile's composition must take InsideAutomatedTestBoundary "
            "from the one reader together with the fixture"
        )
    return findings


def source_files(relative_root: str) -> list[Path]:
    root = REPOSITORY_ROOT / relative_root
    # A named root that does not exist yields nothing from `rglob` without
    # raising, so a renamed directory turns this scan into a guard that reads
    # no files and reports success. `src/adapters/ai` was listed here for as
    # long as the directory had been called `agent`, and nothing said so.
    if not root.is_dir():
        raise AssertionError(
            f"{relative_root} is not a directory, so scanning it would examine "
            f"no files and pass. Repair the name rather than the expectation."
        )
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    )


class CompositionContractTests(unittest.TestCase):
    def assert_language_sealed(
        self,
        completed: subprocess.CompletedProcess[str],
        sealed: str | tuple[str, ...],
        reason: str,
    ) -> None:
        """Assert a hostile snippet was rejected by the seal it targets.

        Three things must hold together. The snippet must fail; some
        diagnostic must name the sealed entity; and that same diagnostic must
        give a reason the entity was unusable. A bare exit-status check would
        accept a snippet that failed over a typo, and naming without a reason
        would accept one that merely mentioned the entity on its way to an
        unrelated error.
        """
        names = (sealed,) if isinstance(sealed, str) else sealed
        rendered = completed.stdout + completed.stderr
        self.assertNotEqual(
            completed.returncode,
            0,
            f"the snippet compiled, so the seal is gone:\n{rendered}",
        )
        messages = diagnostic_messages(completed)
        self.assertTrue(
            messages,
            f"a rejected snippet must report an error:\n{rendered}",
        )
        naming = [
            message
            for message in messages
            if any(name in message for name in names)
        ]
        self.assertTrue(
            naming,
            f"no diagnostic names any of {names}, so the snippet may have "
            f"failed for an unrelated reason:\n{rendered}",
        )
        self.assertTrue(
            any(re.search(reason, message) for message in naming),
            f"no diagnostic naming {names} matches {reason!r}, so nothing "
            f"proves the entity was unusable:\n{rendered}",
        )

    def test_production_mutation_authority_issuers_are_language_sealed(self) -> None:
        with contained_temporary_directory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            production_baseline = compile_snippet(
                temporary_root,
                "production-baseline",
                (
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    '#include "bpr/application/epoch_application_host.hpp"\n'
                    "int main() { return 0; }\n"
                ),
            )
            self.assertEqual(
                production_baseline.returncode,
                0,
                production_baseline.stdout + production_baseline.stderr,
            )

            testing_baseline = compile_snippet(
                temporary_root,
                "testing-baseline",
                (
                    "#define BPR_APPLICATION_TESTING 1\n"
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    '#include "bpr/application/epoch_application_host.hpp"\n'
                    "void test_issuers_are_declared() {\n"
                    "  auto endpoint = "
                    "bpr::application::make_workbench_selected_diff_endpoint_for_testing(\n"
                    "    {}, bpr::core::diff_side::right, {}, {});\n"
                    "  auto grant = bpr::application::make_review_mutation_testing_grant(\n"
                    "    {}, {}, {}, {});\n"
                    "  auto one_shot = "
                    "bpr::application::make_review_mutation_one_shot_testing_grant(\n"
                    "    {}, std::chrono::steady_clock::now());\n"
                    "  static_cast<void>(endpoint);\n"
                    "  static_cast<void>(grant);\n"
                    "  static_cast<void>(one_shot);\n"
                    "}\n"
                ),
            )
            self.assertEqual(
                testing_baseline.returncode,
                0,
                testing_baseline.stdout + testing_baseline.stderr,
            )

            hostile_endpoint = compile_snippet(
                temporary_root,
                "hostile-endpoint",
                (
                    '#include "bpr/application/workbench_runtime_ports.hpp"\n'
                    "auto hostile_endpoint(\n"
                    "  bpr::application::workbench_selected_diff_request request) {\n"
                    "  return bpr::application::workbench_selected_diff_endpoint{\n"
                    "    request, bpr::core::diff_side::right, {}, {}};\n"
                    "}\n"
                ),
            )
            self.assert_language_sealed(
                hostile_endpoint,
                "workbench_selected_diff_endpoint",
                ACCESS_CONTROLLED,
            )

            hostile_grant = compile_snippet(
                temporary_root,
                "hostile-grant",
                (
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    "namespace bpr::application {\n"
                    "auto make_review_mutation_testing_grant(\n"
                    "  core::application_session_id, core::request_generation,\n"
                    "  core::review_mutation_target, core::github_account_id)\n"
                    "  -> review_mutation_execution_grant {\n"
                    "  return review_mutation_execution_grant{\n"
                    "    new review_mutation_execution_authority{\n"
                    "      review_mutation_execution_scope{}}};\n"
                    "}\n"
                    "}\n"
                ),
            )
            self.assert_language_sealed(
                hostile_grant,
                "review_mutation_execution_authority",
                UNCONSTRUCTIBLE,
            )

            hostile_grant_call = compile_snippet(
                temporary_root,
                "hostile-grant-call",
                (
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    "void hostile_grant_call() {\n"
                    "  auto grant = "
                    "bpr::application::make_review_mutation_testing_grant(\n"
                    "    {}, {}, {}, {});\n"
                    "  static_cast<void>(grant);\n"
                    "}\n"
                ),
            )
            self.assert_language_sealed(
                hostile_grant_call,
                "make_review_mutation_testing_grant",
                UNDECLARED,
            )

            hostile_attended_authority = compile_snippet(
                temporary_root,
                "hostile-attended-authority",
                (
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    "auto hostile_attended_authority() {\n"
                    "  return bpr::application::review_mutation_execution_grant{\n"
                    "    new bpr::application::review_mutation_execution_authority{\n"
                    "      std::chrono::steady_clock::now()}};\n"
                    "}\n"
                ),
            )
            self.assert_language_sealed(
                hostile_attended_authority,
                "review_mutation_execution_authority",
                UNCONSTRUCTIBLE,
            )

            hostile_bootstrap_definition = compile_snippet(
                temporary_root,
                "hostile-bootstrap-definition",
                (
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    "namespace bpr::application {\n"
                    "class designated_review_authority_bootstrap final {\n"
                    "public:\n"
                    "  static auto mint() {\n"
                    "    return review_mutation_execution_grant{\n"
                    "      new review_mutation_execution_authority{\n"
                    "        std::chrono::steady_clock::now()}};\n"
                    "  }\n"
                    "};\n"
                    "}\n"
                ),
            )
            self.assert_language_sealed(
                hostile_bootstrap_definition,
                (
                    "review_mutation_execution_authority",
                    "designated_review_authority_bootstrap",
                ),
                REDEFINED,
            )

            hostile_one_shot_factory_call = compile_snippet(
                temporary_root,
                "hostile-one-shot-factory-call",
                (
                    '#include "bpr/application/review_mutation_application_host.hpp"\n'
                    "void hostile_one_shot_factory_call() {\n"
                    "  auto grant = bpr::application::"
                    "make_review_mutation_one_shot_testing_grant(\n"
                    "    {}, std::chrono::steady_clock::now());\n"
                    "  static_cast<void>(grant);\n"
                    "}\n"
                ),
            )
            self.assert_language_sealed(
                hostile_one_shot_factory_call,
                "make_review_mutation_one_shot_testing_grant",
                UNDECLARED,
            )

    def test_whole_review_authority_is_distinct_and_test_factory_is_private(self) -> None:
        header = '#include "bpr/application/review_submission_application_host.hpp"\n'
        with contained_temporary_directory() as temporary_directory:
            root = Path(temporary_directory)
            baseline = compile_snippet(root, "whole-review-baseline", header +
                '#include "bpr/application/review_mutation_application_host.hpp"\n'
                '#include <type_traits>\n'
                'static_assert(!std::is_convertible_v<bpr::application::review_mutation_execution_grant, '
                'bpr::application::review_submission_execution_grant>);\n'
                'int main() { return 0; }\n')
            self.assertEqual(baseline.returncode, 0, baseline.stdout + baseline.stderr)
            # Both testing overloads, each named by its typed third argument: a
            # braced `{}` there is ambiguous between the request and the staged plan.
            factory_calls = (
                'auto fixture() { return bpr::application::make_review_submission_testing_grant('
                '{}, {}, bpr::application::review_submission_request{}); }\n'
                'auto staged() { return bpr::application::make_review_submission_testing_grant('
                '{}, {}, bpr::application::staged_review_plan{}); }\n')
            testing = compile_snippet(root, "whole-review-test-declaration",
                '#define BPR_APPLICATION_TESTING 1\n' + header + factory_calls)
            self.assertEqual(testing.returncode, 0, testing.stdout + testing.stderr)
            forbidden_factory = compile_snippet(root, "whole-review-forbidden-test-factory", header + factory_calls)
            self.assert_language_sealed(forbidden_factory, "make_review_submission_testing_grant", UNDECLARED)
            private_header = (REPOSITORY_ROOT / "src/application/src/review_submission_execution_authority.hpp").as_posix()
            construction = compile_snippet(root, "whole-review-forbidden-construction",
                f'#include "{private_header}"\n' +
                'auto fixture() { return new bpr::application::review_submission_execution_authority{'
                '{}, {}, {}, std::chrono::steady_clock::time_point{}}; }\n')
            self.assert_language_sealed(construction, "review_submission_execution_authority", ACCESS_CONTROLLED)

    def test_build_edges_keep_compiles_and_drop_only_regeneration(self) -> None:
        """A fixture named by a compile edge must still be found.

        Only the regeneration statements are dropped; a compile, link or
        custom command that names a file is an edge ([[BPR-1228]]).
        """
        graph = (
            "rule CXX_COMPILER\n"
            "build src/a.o: CXX_COMPILER /s/src/app/src/github_application.cpp || x $\n"
            "    y\n"
            "  FLAGS = -O2\n"
            "build src/f.o: CXX_COMPILER /s/src/app/src/github_public_path_fixture.cpp\n"
            "build lib.a: LINK src/a.o\n"
            "build build.ninja: RERUN_CMAKE | /s/src/app/src/github_public_path_fixture.cpp $\n"
            "    /s/CMakeLists.txt\n"
            "build /s/src/app/src/github_public_path_fixture.cpp /s/CMakeLists.txt: phony\n"
            "build all: phony lib.a\n"
        )
        edges = ninja_build_edges(graph)
        self.assertEqual(
            [edge.split(":", 1)[0] for edge in edges],
            ["build src/a.o", "build src/f.o", "build lib.a", "build all"],
        )
        self.assertIn("y", edges[0])
        self.assertTrue(
            any("github_public_path_fixture.cpp" in edge for edge in edges)
        )
        regeneration_only = "\n".join(
            line for line in graph.splitlines() if "src/f.o" not in line
        )
        self.assertFalse(
            [
                edge
                for edge in ninja_build_edges(regeneration_only)
                if "github_public_path_fixture.cpp" in edge
            ]
        )

    def test_language_seal_assertion_rejects_what_does_not_prove_a_seal(
        self,
    ) -> None:
        """The seal assertion must fail on both ways of not proving a seal.

        A snippet that compiles means the seal is gone, and a snippet that
        fails over a typo proves nothing about the entity it names. Both must
        make `assert_language_sealed` fail, or a removed seal could keep the
        contract green.
        """
        sealed = "review_mutation_execution_authority"
        header = '#include "bpr/application/review_mutation_application_host.hpp"\n'
        with contained_temporary_directory() as temporary_directory:
            temporary_root = Path(temporary_directory)

            compiles = compile_snippet(
                temporary_root,
                "seal-control-compiles",
                header + "int main() { return 0; }\n",
            )
            self.assertEqual(
                compiles.returncode,
                0,
                compiles.stdout + compiles.stderr,
            )
            with self.assertRaises(AssertionError):
                self.assert_language_sealed(compiles, sealed, UNCONSTRUCTIBLE)

            typo = compile_snippet(
                temporary_root,
                "seal-control-typo",
                header + "int main() { retrun 0; }\n",
            )
            self.assertNotEqual(
                typo.returncode,
                0,
                typo.stdout + typo.stderr,
            )
            with self.assertRaises(AssertionError):
                self.assert_language_sealed(typo, sealed, UNCONSTRUCTIBLE)

            # The reason must be checked against the diagnostic that names the
            # entity, not against the output as a whole.
            unrelated = compile_snippet(
                temporary_root,
                "seal-control-unrelated",
                header
                + "namespace bpr::application {\n"
                + "auto unrelated() -> review_mutation_execution_grant;\n"
                + "}\n"
                + "int main() { retrun 0; }\n",
            )
            self.assertNotEqual(
                unrelated.returncode,
                0,
                unrelated.stdout + unrelated.stderr,
            )
            with self.assertRaises(AssertionError):
                self.assert_language_sealed(unrelated, sealed, UNCONSTRUCTIBLE)

    def assert_release_self_test(
        self,
        executable: Path,
        runtime_environment: dict[str, str],
        expected_vault_selection: str,
        expected_cards: int,
        *extra_arguments: str,
    ) -> None:
        completed = subprocess.run(
            [str(executable), "--self-test", *extra_arguments],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
            env=runtime_environment,
        )
        self.assertEqual(
            completed.returncode,
            0,
            completed.stdout + completed.stderr,
        )
        self.assertIn("AUTOMATED_PROFILE_PRIVATE_OK", completed.stdout)
        # [[BPR-0982]]. The shipped macOS graph offers the Use GitHub CLI
        # entry and precomposes no CLI row; elsewhere the entry is absent.
        cli_marker = (
            "CLI_ENTRY_OFFERED_OK"
            if sys.platform == "darwin"
            else "CLI_ENTRY_ABSENT_OK"
        )
        self.assertRegex(
            completed.stdout,
            (
                r"PUBLIC_PATH_OK[\r\n]+"
                r"NATIVE_PLATFORM_VAULT_SELECTED:"
                + expected_vault_selection
                + rf"[\r\n]+{cli_marker}"
                + rf"[\r\n]+CLI_ENTRY_CARDS:{expected_cards}"
            ),
        )
        self.assertNotIn("GITHUB_FIXTURE", completed.stdout)

    def test_concrete_github_selection_is_owned_by_composition(self) -> None:
        composition_source = read("src/app/src/github_application.cpp")
        required_selection = (
            "make_github_provider(",
            "native_github_wire_transport_factory",
            "make_live_review_mutation_provider(",
            "make_platform_vault(",
        )
        for selection in required_selection:
            with self.subTest(selection=selection):
                self.assertIn(selection, composition_source)
        # [[BPR-0983]]. 65dd7dd4 moved the platform vault selection into its
        # own file in the composition root. The composition asks it for the
        # vault above, it names every platform's vault, and nothing outside
        # it and the adapter that defines them names one.
        vault_path = "src/app/src/platform_credential_vault.cpp"
        vault_source = strip_cxx_comments(read(vault_path))
        platform_vaults = (
            "make_macos_keychain_vault(",
            "make_windows_credential_manager_vault(",
            "make_linux_secret_service_vault(",
        )
        for selection in platform_vaults:
            with self.subTest(platform_vault=selection):
                self.assertIn(selection, vault_source)
        naming = sorted(
            path.relative_to(REPOSITORY_ROOT).as_posix()
            for path in source_files("src")
            if not path.relative_to(REPOSITORY_ROOT)
            .as_posix()
            .startswith("src/adapters/github/")
            and any(
                selection in strip_cxx_comments(path.read_text(encoding="utf-8"))
                for selection in platform_vaults
            )
        )
        self.assertEqual(naming, [vault_path])
        self.assertRegex(composition_source, r"worker_executor\s+ProviderEffects;")
        self.assertRegex(composition_source, r"worker_executor\s+VaultEffects;")
        self.assertIn("make_github_cli_process_runner", composition_source)
        self.assertEqual(cli_consent_gate_findings(composition_source), [])
        publication_gate = (
            "#if defined(__APPLE__)\n\t\t\tif (Profile->cli_read_offered_here()) {"
        )
        self.assertIn(publication_gate, composition_source)
        hostile_consent_sources = {
            # The pre-BPR-0797 compile-time gate: a person's tests-enabled
            # build composes the CLI transport with this confirmation absent.
            "publication-consent-compiled-out-of-tests-enabled-graphs": composition_source.replace(
                publication_gate,
                "#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)\n\t\t\tif (true) {",
                1,
            ),
            # A suite run whose CLI offer stays open can spend somebody's gh sign-in.
            "offer-open-inside-the-boundary": composition_source.replace(
                "return !aInsideAutomatedTestBoundary;", "return true;", 1
            ),
            "boundary-answer-dropped": composition_source.replace(
                "cli_read_offered(InsideAutomatedTestBoundary);",
                "cli_read_offered(false);",
                1,
            ),
            # [[BPR-0982]]. Discovered accounts must come only with a fixture.
            "discovery-accounts-without-a-fixture": composition_source.replace(
                "GithubCliDiscoveryAccounts = aFixture.has_value()",
                "GithubCliDiscoveryAccounts = true",
                1,
            ),
            "read-warning-removed": composition_source.replace(
                '"Use configured GitHub CLI"', '"Use GitHub CLI"', 1
            ),
            # [[BPR-0933]]. The shorter question must still be the answer.
            "read-question-answered-for-the-reviewer": composition_source.replace(
                "const auto consent = qt::ask_github_cli_read_consent(",
                "const auto consent = true || qt::ask_github_cli_read_consent(",
                1,
            ),
        }
        for name, hostile_source in hostile_consent_sources.items():
            with self.subTest(hostile_consent=name):
                self.assertNotEqual(hostile_source, composition_source)
                self.assertNotEqual(cli_consent_gate_findings(hostile_source), [])

        provider_source = read(
            "src/adapters/github/src/live_review_mutation_provider.cpp"
        )
        factory = re.search(
            r"auto make_live_review_mutation_provider\((?P<body>.*?)\n\t}\n",
            provider_source,
            re.DOTALL,
        )
        self.assertIsNotNone(factory, "live provider factory definition is required")
        factory_body = factory.group("body") if factory is not None else ""
        self.assertIn("detail::admit_review_mutation_provider(", factory_body)
        self.assertNotIn("dynamic_pointer_cast", factory_body)

    def test_the_automated_test_boundary_has_one_reader(self) -> None:
        sources = {
            path.relative_to(REPOSITORY_ROOT).as_posix(): path.read_text(
                encoding="utf-8"
            )
            for path in source_files("src")
        }
        self.assertEqual(automated_test_boundary_reader_findings(sources), [])
        composition = sources[AUTOMATED_TEST_BOUNDARY_READER_PATH]
        reader = AUTOMATED_TEST_BOUNDARY_READER.search(composition)
        self.assertIsNotNone(reader)
        reader_text = reader.group(0) if reader is not None else ""
        profile_answer = AUTOMATED_TEST_BOUNDARY_PROFILE_ANSWER.search(composition)
        self.assertIsNotNone(profile_answer)
        profile_answer_text = (
            profile_answer.group(0) if profile_answer is not None else ""
        )
        # [[BPR-0983]]. The updater and the update lifetime ask the reader.
        for asker in (
            "src/app/src/release_update_desktop.cpp",
            "src/app/src/profile_desktop.cpp",
        ):
            with self.subTest(asks_the_one_reader=asker):
                self.assertIn(
                    "inside_automated_test_boundary()",
                    strip_cxx_comments(sources[asker]),
                )
        read_everywhere = reader_text.replace(
            "return false;",
            "return !qEnvironmentVariableIsEmpty("
            + AUTOMATED_TEST_BOUNDARY_LITERAL
            + ");",
            1,
        )
        hostile_sources = {
            # The pre-BPR-0802 shape: main reads it and passes it in, so an
            # in-process composition root never hears the answer.
            "second-reader-in-main": {
                **sources,
                "src/app/main.cpp": sources["src/app/main.cpp"]
                + "\nconst auto inside = !qEnvironmentVariableIsEmpty("
                + AUTOMATED_TEST_BOUNDARY_LITERAL
                + ");\n",
            },
            # The pre-BPR-0983 shape: the updater reads it for itself.
            "updater-reads-it-for-itself": {
                **sources,
                "src/app/src/release_update_desktop.cpp": sources[
                    "src/app/src/release_update_desktop.cpp"
                ].replace(
                    "inside_automated_test_boundary()",
                    "qEnvironmentVariableIsSet("
                    + AUTOMATED_TEST_BOUNDARY_LITERAL
                    + ")",
                    1,
                ),
            },
            "reader-removed": {
                **sources,
                AUTOMATED_TEST_BOUNDARY_READER_PATH: composition.replace(
                    reader_text,
                    "auto inside_automated_test_boundary() -> bool { return false; }",
                    1,
                ),
            },
            "reader-left-only-in-a-comment": {
                **sources,
                AUTOMATED_TEST_BOUNDARY_READER_PATH: composition.replace(
                    reader_text,
                    "auto inside_automated_test_boundary() -> bool { return false; } /* "
                    + reader_text
                    + " */",
                    1,
                ),
            },
            # A shipped build would carry the name and answer to it.
            "reader-in-every-graph": {
                **sources,
                AUTOMATED_TEST_BOUNDARY_READER_PATH: composition.replace(
                    reader_text, read_everywhere, 1
                ),
            },
            "profile-ignores-the-reader": {
                **sources,
                AUTOMATED_TEST_BOUNDARY_READER_PATH: composition.replace(
                    profile_answer_text,
                    "InsideAutomatedTestBoundary = aFixture.has_value();",
                    1,
                ),
            },
        }
        for name, hostile in hostile_sources.items():
            with self.subTest(hostile_reader=name):
                self.assertNotEqual(hostile, sources)
                self.assertNotEqual(
                    automated_test_boundary_reader_findings(hostile), []
                )

    def test_async_mutation_host_inventory_rejects_every_direct_bypass(self) -> None:
        source = read("src/app/src/github_application.cpp")
        insertion_point = "\n\t\tvoid compose_review_mutation("
        self.assertEqual(source.count(insertion_point), 1)
        concrete_helper = (
            "\n#if !defined(BPR_BUILD_TESTING)\n"
            "\t\tvoid hostile_live_provider_use_for_contract()\n"
            "\t\t{\n"
            "\t\t\tstatic_cast<void>(\n"
            "\t\t\t\tLiveReviewMutationProvider->observe_target());\n"
            "\t\t}\n"
            "#endif\n"
        )
        provider_success = (
            "\t\t\tif (provider.has_value()) {\n"
            "\t\t\t\tInlineReviewBinding = *selection;\n"
            "\t\t\t\tauto erased         = "
            "std::unique_ptr<application::review_mutation_port>{"
        )
        self.assertEqual(source.count(provider_success), 1)
        hostile_helpers = {
            "concrete-provider": concrete_helper,
            "generic-port": (
                "\n\t\tvoid hostile_generic_port_for_contract(\n"
                "\t\t\tapplication::review_mutation_port& mutation)\n"
                "\t\t{\n"
                "\t\t\tstatic_cast<void>(mutation.submit({ }));\n"
                "\t\t}\n"
            ),
            "coordinator": (
                "\n\t\tapplication::review_mutation_coordinator*\n"
                "\t\t\tHostileCoordinatorForContract = nullptr;\n"
            ),
            "inferred-provider": (
                "\n\t\ttemplate <typename Mutation>\n"
                "\t\tvoid hostile_inferred_provider_for_contract(Mutation& mutation)\n"
                "\t\t{\n"
                "\t\t\tstatic_cast<void>(mutation.observe_target());\n"
                "\t\t}\n"
            ),
            "second-factory": (
                "\n\t\tvoid hostile_second_factory_for_contract()\n"
                "\t\t{\n"
                "\t\t\tstatic_cast<void>(make_live_review_mutation_provider(\n"
                "\t\t\t\t/* hostile inventory candidate */));\n"
                "\t\t}\n"
            ),
            "pointer-to-member": (
                "\n\t\tauto HostileSubmitMemberForContract\n"
                "\t\t\t= &application::review_mutation_port::submit;\n"
            ),
        }
        hostile_sources = {
            name: source.replace(insertion_point, helper + insertion_point, 1)
            for name, helper in hostile_helpers.items()
        }
        hostile_sources["existing-provider-indirection"] = source.replace(
            provider_success,
            "\t\t\tif (provider.has_value()) {\n"
            "\t\t\t\tstatic_cast<void>((*provider)->observe_target());\n"
            "\t\t\t\tauto erased = "
            "std::unique_ptr<application::review_mutation_port>{",
            1,
        )
        contract = (
            REPOSITORY_ROOT
            / "tests/end_to_end/application_review_round_trip_composition_contract.cmake"
        )

        with contained_temporary_directory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            baseline_path = temporary_root / "baseline-github-application.cpp"
            baseline_path.write_text(source, encoding="utf-8")

            def verify(
                candidate: Path,
                app_source_root: Path = REPOSITORY_ROOT / "src/app",
            ) -> subprocess.CompletedProcess[str]:
                return subprocess.run(
                    [
                        ARGUMENTS.cmake,
                        f"-DBPR_PROVIDER_INVENTORY_SOURCE_FILE={candidate}",
                        f"-DBPR_APP_SOURCE_ROOT={app_source_root}",
                        "-P",
                        str(contract),
                    ],
                    check=False,
                    capture_output=True,
                    text=True,
                    timeout=30,
                )

            baseline = verify(baseline_path)
            self.assertEqual(
                baseline.returncode,
                0,
                baseline.stdout + baseline.stderr,
            )
            self.assertIn(
                "ASYNC_MUTATION_HOST_INVENTORY_OK",
                baseline.stdout + baseline.stderr,
            )

            for name, hostile_source in hostile_sources.items():
                with self.subTest(hostile=name):
                    hostile_path = temporary_root / f"{name}.cpp"
                    hostile_path.write_text(hostile_source, encoding="utf-8")
                    hostile = verify(hostile_path)
                    self.assertNotEqual(
                        hostile.returncode,
                        0,
                        hostile.stdout + hostile.stderr,
                    )
                    self.assertRegex(
                        hostile.stdout + hostile.stderr,
                        r"(full LF-normalized|concrete provider bypasses|literal inventory)",
                    )

            app_tree_attacks = {
                "second-app-generic-port": (
                    "src/foundation_application.cpp",
                    "append",
                    (
                        "\nvoid hostile_second_app_tu_generic_port(\n"
                        "    bpr::application::review_mutation_port& mutation)\n"
                        "{\n"
                        "    static_cast<void>(mutation.submit({}));\n"
                        "}\n"
                    ),
                ),
                "foundation-spaced-member": (
                    "src/foundation_application.cpp",
                    "append",
                    (
                        "\ntemplate <typename Mutation>\n"
                        "constexpr auto HostileSpacedSubmitMember\n"
                        "    = &Mutation:: submit;\n"
                    ),
                ),
                "worker-commented-member": (
                    "src/worker_executor.cpp",
                    "append",
                    (
                        "\ntemplate <typename Mutation>\n"
                        "constexpr auto HostileCommentedSubmitMember\n"
                        "    = &Mutation:: /* literal-evasion */ submit;\n"
                    ),
                ),
                "worker-include-fragment": (
                    "src/worker_executor.cpp",
                    "prepend",
                    (
                        '#include "bpr/application/review_mutation_'
                        "\\\n"
                        'port.hpp"\n'
                    ),
                ),
            }
            self.assertNotIn(
                "::submit",
                app_tree_attacks["foundation-spaced-member"][2],
            )
            self.assertNotIn(
                "::submit",
                app_tree_attacks["worker-commented-member"][2],
            )
            self.assertNotIn(
                "review_mutation_port",
                app_tree_attacks["worker-include-fragment"][2],
            )
            for name, (
                relative_path,
                placement,
                fragment,
            ) in app_tree_attacks.items():
                with self.subTest(hostile=name):
                    hostile_app_root = temporary_root / name
                    shutil.copytree(REPOSITORY_ROOT / "src/app", hostile_app_root)
                    hostile_app_source = hostile_app_root / relative_path
                    original = hostile_app_source.read_text(encoding="utf-8")
                    hostile_app_source.write_text(
                        fragment + original
                        if placement == "prepend"
                        else original + fragment,
                        encoding="utf-8",
                    )
                    hostile_app = verify(baseline_path, hostile_app_root)
                    self.assertNotEqual(
                        hostile_app.returncode,
                        0,
                        hostile_app.stdout + hostile_app.stderr,
                    )
                    self.assertIn(
                        "production application source content digest drifted",
                        hostile_app.stdout + hostile_app.stderr,
                    )

        composition_cmake = read("src/app/CMakeLists.txt")
        target = re.search(
            r"add_library\s*\(\s*better_pull_review_composition"
            r"\s+STATIC(?P<body>.*?)\)",
            composition_cmake,
            re.DOTALL,
        )
        self.assertIsNotNone(target, "production composition target is required")
        target_body = target.group("body") if target is not None else ""
        self.assertIn("src/github_application.cpp", target_body)
        self.assertIn("src/worker_executor.cpp", target_body)
        self.assertNotIn("src/github_public_path_fixture.cpp", target_body)

        # CMake accumulates dependencies across calls (including the fixture's
        # separate private Glaze link); inspecting only the first loses the
        # production dependency declaration.
        links = re.findall(
            r"target_link_libraries\s*\(\s*"
            r"better_pull_review_composition\b(?P<body>.*?)\)",
            composition_cmake,
            re.DOTALL,
        )
        self.assertTrue(links, "composition dependencies are required")
        link_body = "\n".join(links)
        for dependency in (
            "BetterPullReview::Application",
            "BetterPullReview::AdapterGitHub",
            "BetterPullReview::AdapterQt",
            "Threads::Threads",
        ):
            with self.subTest(dependency=dependency):
                self.assertIn(dependency, link_body)

        violations: list[str] = []
        for relative_root in (
            "src/core",
            "src/application",
            "src/adapters/qt",
            "src/adapters/git",
            "src/adapters/sqlite",
            # `agent` replaced `ai`, and `syntax` was never listed. Both are
            # adapters that must not select a concrete provider themselves.
            # `src/adapters/github` and `src/app` stay absent deliberately:
            # the first is where the concrete provider legitimately lives and
            # the second is the composition root.
            "src/adapters/agent",
            "src/adapters/syntax",
        ):
            for path in source_files(relative_root):
                contents = path.read_text(encoding="utf-8")
                for pattern in CONCRETE_SELECTION_PATTERNS:
                    if pattern.search(contents):
                        violations.append(
                            f"{path.relative_to(REPOSITORY_ROOT)}:"
                            f"{pattern.pattern}"
                        )
        self.assertEqual(
            violations,
            [],
            "concrete provider, vault, transport, and worker selection "
            "belongs only to the composition root",
        )

    def test_tests_off_release_graph_omits_public_path_fixture(self) -> None:
        public_header = read("src/app/include/bpr/app/github_application.hpp")
        # [[BPR-0821]]. The header's layout must not depend on the
        # tests-enabled define; prose that names the define is not a use.
        self.assertNotIn("BPR_BUILD_TESTING", strip_cxx_comments(public_header))
        self.assertIn(
            "BPR_BUILD_TESTING",
            strip_cxx_comments(public_header + "\n#if defined(BPR_BUILD_TESTING)\n#endif\n"),
        )
        self.assertIn(
            "BPR_BUILD_TESTING",
            strip_cxx_comments(public_header + '\nconstexpr auto Name = "BPR_BUILD_TESTING";\n'),
        )

        # CTest owns a persistent build below its binary tree. Reconfigure and
        # rebuild it every time: only compiler outputs are reused, never a test
        # verdict or an unchecked prebuilt executable. Standalone callers keep
        # the disposable default.
        root = (nullcontext(ARGUMENTS.release_build_dir)
                if ARGUMENTS.release_build_dir else contained_temporary_directory())
        with root as temporary_directory:
            build_directory = Path(temporary_directory) / "release"
            command = [
                ARGUMENTS.cmake,
                "-S",
                str(REPOSITORY_ROOT),
                "-B",
                str(build_directory),
                "-G",
                "Ninja",
                "-DBPR_BUILD_QT=ON",
                "-DBUILD_TESTING=OFF",
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                f"-DCMAKE_CXX_COMPILER={ARGUMENTS.cxx}",
            ]
            if ARGUMENTS.qt_root:
                command.append(f"-DQt6_ROOT={ARGUMENTS.qt_root}")
            if ARGUMENTS.qt_dir:
                command.append(f"-DQt6_DIR={ARGUMENTS.qt_dir}")
            if ARGUMENTS.cpm_source_cache:
                command.append(
                    f"-DCPM_SOURCE_CACHE={ARGUMENTS.cpm_source_cache}"
                )

            configured = subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                configured.returncode,
                0,
                configured.stdout + configured.stderr,
            )

            cache = read_from(build_directory / "CMakeCache.txt")
            self.assertRegex(cache, r"(?m)^BUILD_TESTING:BOOL=OFF$")
            self.assertRegex(cache, r"(?m)^CMAKE_BUILD_TYPE:STRING=Release$")
            dependency_audit = read_from(
                build_directory / "_bpr/product-dependency-audit.cmake"
            )
            self.assertNotIn("Python3|", dependency_audit)
            self.assertNotIn("Git|", dependency_audit)
            compilation = json.loads(
                read_from(build_directory / "compile_commands.json")
            )
            compiled_files = {
                Path(entry["file"]).resolve() for entry in compilation
            }
            self.assertIn(
                (REPOSITORY_ROOT / "src/app/src/github_application.cpp")
                .resolve(),
                compiled_files,
            )
            self.assertIn(
                (REPOSITORY_ROOT / "src/app/src/worker_executor.cpp").resolve(),
                compiled_files,
            )
            self.assertNotIn(
                (
                    REPOSITORY_ROOT
                    / "src/app/src/github_public_path_fixture.cpp"
                ).resolve(),
                compiled_files,
            )

            compile_invocations = "\n".join(
                entry.get("command", "")
                or " ".join(entry.get("arguments", []))
                for entry in compilation
            )
            self.assertNotIn("BPR_BUILD_TESTING", compile_invocations)
            self.assertNotIn("BPR_NATIVE_HTTPS_TESTING", compile_invocations)

            build_edges = ninja_build_edges(
                read_from(build_directory / "build.ninja")
            )
            self.assertTrue(
                any("github_application.cpp" in edge for edge in build_edges)
            )
            self.assertFalse(
                [
                    edge
                    for edge in build_edges
                    if "github_public_path_fixture.cpp" in edge
                ]
            )

            built = subprocess.run(
                [
                    ARGUMENTS.cmake,
                    "--build",
                    str(build_directory),
                    "--config",
                    "Release",
                    "--target",
                    "app_better_pull_review",
                    "--parallel",
                    "2",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                built.returncode,
                0,
                built.stdout + built.stderr,
            )

            executable = find_better_pull_review_executable(build_directory)
            composed_hosts = subprocess.run(
                [
                    sys.executable,
                    str(REPOSITORY_ROOT / "scripts/verify_composed_hosts.py"),
                    "--profile",
                    "production",
                    str(executable),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                composed_hosts.returncode,
                0,
                composed_hosts.stdout + composed_hosts.stderr,
            )
            self.assertIn(
                "COMPOSED_HOSTS_OK: profile=production",
                composed_hosts.stdout,
            )
            binary = executable.read_bytes()
            forbidden_bytes = list(RELEASE_BINARY_FORBIDDEN_BYTES)
            forbidden_bytes.extend(
                (
                    str(
                        REPOSITORY_ROOT
                        / "tests/fixtures/github/contracts/v1"
                    ).encode(),
                    str(
                        REPOSITORY_ROOT
                        / "tests/fixtures/github/capabilities/v1"
                    ).encode(),
                )
            )
            for forbidden in forbidden_bytes:
                with self.subTest(forbidden=forbidden.decode(errors="replace")):
                    self.assertNotIn(forbidden, binary)
            for marker in MAC_CLI_BINARY_MARKERS:
                with self.subTest(cli_marker=marker.decode(errors="replace")):
                    if sys.platform == "darwin":
                        self.assertIn(marker, binary)
                    else:
                        self.assertNotIn(marker, binary)

            runtime_environment = os.environ.copy()
            runtime_environment["QT_QPA_PLATFORM"] = "offscreen"
            expected_selection = (
                "true"
                if (
                    sys.platform == "darwin"
                    or sys.platform.startswith("linux")
                    or sys.platform == "win32"
                )
                else "false"
            )
            self.assert_release_self_test(
                executable,
                runtime_environment,
                expected_selection,
                1,
            )
            self.assert_release_self_test(
                executable,
                runtime_environment,
                expected_selection,
                2,
                "--ghes-host",
                "ghe.example.invalid",
            )

            failed_render = subprocess.run(
                [
                    str(executable),
                    "--render-evidence",
                    str(build_directory),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
                env=runtime_environment,
            )
            self.assertNotEqual(failed_render.returncode, 0)
            self.assertIn("AUTOMATED_PROFILE_PRIVATE_OK", failed_render.stdout)
            self.assertIn("PUBLIC_PATH_STAGE:3", failed_render.stdout)
            self.assertIn("RENDER_FAILED", failed_render.stdout)
            self.assertNotIn(
                "NATIVE_APPEARANCE_",
                failed_render.stdout,
            )


def read_from(path: Path) -> str:
    return path.read_text(encoding="utf-8")


# The names the application can be built under. The macOS Qt bundle carries the
# shipped product name (BPR-0328); every other configuration keeps the target's
# own lowercase name. A finder that knew only the latter reported a successful
# tests-disabled Release build as producing no executable at all.
EXECUTABLE_NAMES = (
    "better_pull_review",
    "better_pull_review.exe",
    "BetterPullReview",
    "BetterPullReview.exe",
)


def find_better_pull_review_executable(build_directory: Path) -> Path:
    application_directory = build_directory / "src/app"
    for name in EXECUTABLE_NAMES:
        candidate = application_directory / name
        if candidate.is_file():
            return candidate

    candidates = sorted(
        path
        for path in build_directory.rglob("*")
        if path.is_file() and path.name in set(EXECUTABLE_NAMES)
    )
    if len(candidates) != 1:
        rendered = ", ".join(str(path) for path in candidates) or "none"
        raise AssertionError(
            "expected exactly one built BetterPullReview executable; "
            f"found {rendered}"
        )
    return candidates[0]


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *UNITTEST_ARGUMENTS])
