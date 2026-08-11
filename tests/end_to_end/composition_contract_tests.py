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
import tempfile
import unittest


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
    def test_production_mutation_authority_issuers_are_language_sealed(self) -> None:
        include_arguments = [
            f"-I{REPOSITORY_ROOT / 'src/application/include'}",
            f"-I{REPOSITORY_ROOT / 'src/core/include'}",
        ]

        def compile_source(
            temporary_root: Path,
            name: str,
            source: str,
        ) -> subprocess.CompletedProcess[str]:
            path = temporary_root / f"{name}.cpp"
            path.write_text(source, encoding="utf-8")
            return subprocess.run(
                [
                    ARGUMENTS.cxx,
                    "-std=c++23",
                    "-fsyntax-only",
                    "-D_LIBCPP_ENABLE_EXPERIMENTAL",
                    *include_arguments,
                    str(path),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )

        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            production_baseline = compile_source(
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

            testing_baseline = compile_source(
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
                    "  static_cast<void>(endpoint);\n"
                    "  static_cast<void>(grant);\n"
                    "}\n"
                ),
            )
            self.assertEqual(
                testing_baseline.returncode,
                0,
                testing_baseline.stdout + testing_baseline.stderr,
            )

            hostile_endpoint = compile_source(
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
            self.assertNotEqual(hostile_endpoint.returncode, 0)
            self.assertIn(
                "private",
                (hostile_endpoint.stdout + hostile_endpoint.stderr).lower(),
            )

            hostile_grant = compile_source(
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
            self.assertNotEqual(hostile_grant.returncode, 0)
            self.assertIn(
                "private",
                (hostile_grant.stdout + hostile_grant.stderr).lower(),
            )

            hostile_grant_call = compile_source(
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
            self.assertNotEqual(hostile_grant_call.returncode, 0)
            self.assertRegex(
                (hostile_grant_call.stdout + hostile_grant_call.stderr).lower(),
                r"(no member named|not a member)",
            )

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
            "\t\t\t\tauto erased = "
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

        with tempfile.TemporaryDirectory() as temporary_directory:
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

        links = re.search(
            r"target_link_libraries\s*\(\s*"
            r"better_pull_review_composition(?P<body>.*?)\)",
            composition_cmake,
            re.DOTALL,
        )
        self.assertIsNotNone(links, "composition dependencies are required")
        link_body = links.group("body") if links is not None else ""
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

        with tempfile.TemporaryDirectory() as temporary_directory:
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


def find_better_pull_review_executable(build_directory: Path) -> Path:
    application_directory = build_directory / "src/app"
    for name in ("better_pull_review", "better_pull_review.exe"):
        candidate = application_directory / name
        if candidate.is_file():
            return candidate

    candidates = sorted(
        path
        for path in build_directory.rglob("*")
        if path.is_file()
        and path.name in {"better_pull_review", "better_pull_review.exe"}
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
