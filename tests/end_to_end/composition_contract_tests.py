#!/usr/bin/env python3
from __future__ import annotations

import argparse
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
            testing = compile_snippet(root, "whole-review-test-declaration",
                '#define BPR_APPLICATION_TESTING 1\n' + header +
                'auto fixture() { return bpr::application::make_review_submission_testing_grant({}, {}, {}); }\n')
            self.assertEqual(testing.returncode, 0, testing.stdout + testing.stderr)
            forbidden_factory = compile_snippet(root, "whole-review-forbidden-test-factory", header +
                'auto fixture() { return bpr::application::make_review_submission_testing_grant({}, {}, {}); }\n')
            self.assert_language_sealed(forbidden_factory, "make_review_submission_testing_grant", UNDECLARED)
            private_header = (REPOSITORY_ROOT / "src/application/src/review_submission_execution_authority.hpp").as_posix()
            construction = compile_snippet(root, "whole-review-forbidden-construction",
                f'#include "{private_header}"\n' +
                'auto fixture() { return new bpr::application::review_submission_execution_authority{'
                '{}, {}, {}, std::chrono::steady_clock::time_point{}}; }\n')
            self.assert_language_sealed(construction, "review_submission_execution_authority", ACCESS_CONTROLLED)

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
        expected_cli_count: int,
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
        self.assertRegex(
            completed.stdout,
            (
                r"PUBLIC_PATH_OK[\r\n]+"
                r"NATIVE_PLATFORM_VAULT_SELECTED:"
                + expected_vault_selection
                + r"[\r\n]+CLI_UNAVAILABLE_OK"
                + rf"[\r\n]+CLI_UNAVAILABLE_COUNT:{expected_cli_count}"
            ),
        )
        self.assertNotIn("GITHUB_FIXTURE", completed.stdout)

    def test_concrete_github_selection_is_owned_by_composition(self) -> None:
        composition_source = read("src/app/src/github_application.cpp")
        required_selection = (
            "make_github_provider(",
            "native_github_wire_transport_factory",
            "make_live_review_mutation_provider(",
            "make_macos_keychain_vault(",
            "make_windows_credential_manager_vault(",
            "make_linux_secret_service_vault(",
        )
        for selection in required_selection:
            with self.subTest(selection=selection):
                self.assertIn(selection, composition_source)
        self.assertRegex(composition_source, r"worker_executor\s+ProviderEffects;")
        self.assertRegex(composition_source, r"worker_executor\s+VaultEffects;")
        self.assertNotIn(
            "make_github_cli_process_runner",
            composition_source,
            "production composition must not construct a CLI process runner",
        )

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
        self.assertNotIn("BPR_BUILD_TESTING", public_header)

        with contained_temporary_directory() as temporary_directory:
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

            build_graph = read_from(build_directory / "build.ninja")
            self.assertNotIn("github_public_path_fixture.cpp", build_graph)

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
