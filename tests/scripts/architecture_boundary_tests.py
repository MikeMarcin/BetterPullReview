#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
VERIFY_SCRIPT = REPOSITORY_ROOT / "cmake" / "VerifyArchitectureBoundaries.cmake"

ADAPTER_ROOT = """\
add_subdirectory(github)
"""

GITHUB_TARGET = """\
add_library(better_pull_review_adapter_github INTERFACE)
add_library(BetterPullReview::AdapterGitHub ALIAS better_pull_review_adapter_github)
target_link_libraries(
    better_pull_review_adapter_github
    INTERFACE
        BetterPullReview::Application
)
target_sources(
    better_pull_review_adapter_github
    INTERFACE
        src/github_cli_process_runner.cpp
        src/gh_cli_transport.cpp
        src/native_https_transport.cpp
)
"""


class ArchitectureBoundaryTests(unittest.TestCase):
    def run_check(
        self,
        sources: dict[str, str] | None = None,
        *,
        adapter_root: str = ADAPTER_ROOT,
        github_target: str = GITHUB_TARGET,
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            files = {
                "CMakeLists.txt": "",
                "src/adapters/CMakeLists.txt": adapter_root,
                "src/adapters/github/CMakeLists.txt": github_target,
            }
            files.update(sources or {})

            for relative_path, contents in files.items():
                path = root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents, encoding="utf-8")

            return subprocess.run(
                [
                    shutil.which("cmake") or "cmake",
                    f"-DBPR_ROOT={root}",
                    "-P",
                    str(VERIFY_SCRIPT),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def assert_failure(
        self,
        expected_message: str,
        sources: dict[str, str] | None = None,
        **kwargs: str,
    ) -> None:
        result = self.run_check(sources, **kwargs)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(expected_message, result.stdout + result.stderr)

    def test_designated_github_effect_sources_are_allowed(self) -> None:
        result = self.run_check(
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    "#include <QProcess>\nQProcess* process;\n",
                "src/adapters/github/src/gh_cli_transport.cpp":
                    "const char* setting = \"GH_TELEMETRY=0\";\n",
                "src/adapters/github/src/native_https_transport.cpp":
                    "#include <QNetworkAccessManager>\n"
                    "QNetworkAccessManager* manager;\n",
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_qprocess_is_rejected_outside_designated_runner(self) -> None:
        self.assert_failure(
            "QProcess ownership violation",
            {"src/adapters/git/process.cpp": "QProcess* process;\n"},
        )

    def test_core_application_and_qt_reject_process_and_network_types(
        self,
    ) -> None:
        for region in ("core", "application", "adapters/qt"):
            for effect in ("QProcess", "QNetworkReply"):
                with self.subTest(region=region, effect=effect):
                    self.assert_failure(
                        "Core/application/Qt effect-boundary violation",
                        {
                            f"src/{region}/hostile.cpp":
                                f"{effect}* forbidden_effect;\n"
                        },
                    )

    def test_designated_effect_source_must_belong_to_github_target(self) -> None:
        self.assert_failure(
            "Designated GitHub effect source",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    "QProcess* process;\n"
            },
            github_target=GITHUB_TARGET.replace(
                "        src/github_cli_process_runner.cpp\n",
                "",
            ),
        )

    def test_network_manager_is_rejected_outside_designated_transport(self) -> None:
        self.assert_failure(
            "QNetworkAccessManager ownership violation",
            {
                "src/adapters/github/src/other_transport.cpp":
                    "QNetworkAccessManager* manager;\n"
            },
        )

    def test_qt_adapter_cannot_own_provider_effects(self) -> None:
        self.assert_failure(
            "Core/application/Qt effect-boundary violation",
            {"src/adapters/qt/provider.cpp": "QProcess* process;\n"},
        )

    def test_telemetry_hardening_is_rejected_outside_cli_transport(self) -> None:
        self.assert_failure(
            "GitHub CLI telemetry hardening ownership violation",
            {
                "src/application/src/provider.cpp":
                    "const char* setting = \"GH_TELEMETRY=0\";\n"
            },
        )

    def test_non_disabling_telemetry_setting_is_rejected_in_transport(
        self,
    ) -> None:
        self.assert_failure(
            "Forbidden telemetry surface",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    "const char* setting = \"GH_TELEMETRY=1\";\n"
            },
        )

    def test_cli_transport_source_requires_telemetry_hardening(self) -> None:
        self.assert_failure(
            "must set GH_TELEMETRY=0",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    "const char* setting = \"GH_PROMPT_DISABLED=1\";\n"
            },
        )

    def test_adapter_root_must_delegate_target_ownership(self) -> None:
        self.assert_failure(
            "Adapter root must delegate",
            adapter_root="",
        )

    def test_github_target_cannot_be_declared_at_adapter_root(self) -> None:
        self.assert_failure(
            "must be declared only",
            adapter_root=(
                ADAPTER_ROOT
                + "add_library(better_pull_review_adapter_github INTERFACE)\n"
            ),
        )


if __name__ == "__main__":
    unittest.main()
