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
)
RELEASE_BINARY_FORBIDDEN_BYTES = (
    b"--github-fixture",
    b"--native-appearance-probe",
    b"--changed-render-evidence",
    b"GITHUB_FIXTURE_OK",
    b"GITHUB_FIXTURE_FAILED",
    b"GITHUB_FIXTURE_EVIDENCE",
    b"NATIVE_APPEARANCE_",
    b"AppleInterfaceStyle",
    b"CFPreferencesCopyValue",
    b"CFPreferencesSynchronize",
    b"Recorded native HTTPS",
    b"PRRT_fixture_1",
    b"github_application_fixture_access",
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
    parser.add_argument("--qt-dir", default="")
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
    def test_concrete_github_selection_is_owned_by_composition(self) -> None:
        composition_source = read("src/app/src/github_application.cpp")
        required_selection = (
            "make_github_provider(",
            "native_github_wire_transport_factory",
            "make_macos_keychain_vault(",
            "make_windows_credential_manager_vault(",
            "make_linux_secret_service_vault(",
            "worker_executor ProviderEffects;",
            "worker_executor VaultEffects;",
        )
        for selection in required_selection:
            with self.subTest(selection=selection):
                self.assertIn(selection, composition_source)
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
        self.assertNotIn("BPR_BUILD_TESTS", public_header)

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
                "-DBPR_BUILD_TESTS=OFF",
                "-DCMAKE_BUILD_TYPE=Release",
            ]
            if ARGUMENTS.qt_dir:
                command.append(f"-DQt6_DIR={ARGUMENTS.qt_dir}")

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
            self.assertRegex(cache, r"(?m)^BPR_BUILD_TESTS:BOOL=OFF$")
            self.assertRegex(cache, r"(?m)^CMAKE_BUILD_TYPE:STRING=Release$")

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
            self.assertNotIn("BPR_BUILD_TESTS", compile_invocations)
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
                    "better_pull_review",
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
            runtime = subprocess.run(
                [str(executable), "--self-test"],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
                env=runtime_environment,
            )
            self.assertEqual(
                runtime.returncode,
                0,
                runtime.stdout + runtime.stderr,
            )
            expected_selection = (
                "true"
                if (
                    sys.platform == "darwin"
                    or sys.platform.startswith("linux")
                    or sys.platform == "win32"
                )
                else "false"
            )
            self.assertRegex(
                runtime.stdout,
                (
                    r"PUBLIC_PATH_OK[\r\n]+"
                    r"NATIVE_PLATFORM_VAULT_SELECTED:"
                    + expected_selection
                    + r"[\r\n]+CLI_UNAVAILABLE_OK"
                ),
            )
            self.assertNotIn("GITHUB_FIXTURE", runtime.stdout)

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
