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
    parser.add_argument("--qt-root", default="")
    parser.add_argument("--qt-dir", default="")
    parser.add_argument("--cpm-source-cache", default="")
    return parser.parse_known_args()


ARGUMENTS, UNITTEST_ARGUMENTS = parse_arguments()


def read(relative_path: str) -> str:
    return (REPOSITORY_ROOT / relative_path).read_text(encoding="utf-8")


def source_files(relative_root: str) -> list[Path]:
    root = REPOSITORY_ROOT / relative_root
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    )


class CompositionContractTests(unittest.TestCase):
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
            "src/adapters/ai",
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
