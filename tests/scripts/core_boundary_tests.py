#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path
import shutil
import subprocess
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
VERIFY_SCRIPT = REPOSITORY_ROOT / "cmake" / "VerifyCoreBoundary.cmake"


class CoreBoundaryTests(unittest.TestCase):
    def run_check(self, source: str, cmake: str | None = None) -> subprocess.CompletedProcess[str]:
        with contained_temporary_directory() as temporary_directory:
            root = Path(temporary_directory)
            core_source = root / "src/core/policy.cpp"
            core_source.parent.mkdir(parents=True)
            core_source.write_text(source, encoding="utf-8")
            if cmake is not None:
                (root / "src/core/CMakeLists.txt").write_text(cmake, encoding="utf-8")

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

    def assert_rejected(self, source: str) -> None:
        result = self.run_check(source)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(
            "Functional-core boundary violation",
            result.stdout + result.stderr,
        )

    def test_standard_domain_code_is_allowed(self) -> None:
        result = self.run_check(
            "#include <cstdint>\n"
            "#include <string>\n"
            "struct identity { std::uint64_t Value; std::string Label; };\n"
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_only_owner_approved_libraries_link_into_core(self) -> None:
        # Owner, 2026-10-13: glaze, utf8proc, zlib and fmt, then
        # sha1collisiondetection and HACL* SHA-2 (RFC-BPR-0080 Q1 and Q2).
        approved = self.run_check(
            "struct identity {};\n",
            "target_link_libraries(better_pull_review_core PRIVATE BetterPullReview::ZLib glaze::glaze BetterPullReview::Sha1dc)\n",
        )
        self.assertEqual(approved.returncode, 0, approved.stdout + approved.stderr)
        for library in ("re2::re2", "Qt6::Core", "BetterPullReview::SQLite3", "Threads::Threads"):
            with self.subTest(library=library):
                refused = self.run_check(
                    "struct identity {};\n",
                    f"target_link_libraries(\n    better_pull_review_core\n    PUBLIC\n        {library}\n)\n",
                )
                self.assertNotEqual(refused.returncode, 0, refused.stdout)
                self.assertIn("not an owner-approved core library", refused.stdout + refused.stderr)

    def test_qt_provider_and_effect_headers_are_rejected(self) -> None:
        for header in (
            "QNetworkAccessManager",
            "sqlite3.h",
            "curl/curl.h",
            "thread",
            "future",
            "filesystem",
            "chrono",
            "ctime",
            "random",
            "fstream",
            "atomic",
            "mutex",
            "condition_variable",
            "asio.hpp",
            "nlohmann/json.hpp",
            "sys/socket.h",
            "winsock2.h",
        ):
            with self.subTest(header=header):
                self.assert_rejected(f"#include <{header}>\n")

    def test_hidden_effect_calls_are_rejected(self) -> None:
        for expression in (
            'std::getenv("TOKEN")',
            '::getenv("TOKEN")',
            'getenv("TOKEN")',
            'std::system("gh api /user")',
            '::system("gh api /user")',
            'system("gh api /user")',
            'fopen("state", "r")',
            'popen("gh", "r")',
            "fork()",
            "execve(nullptr, nullptr, nullptr)",
            "CreateProcess()",
            "socket()",
            "connect()",
            "listen()",
            "time(nullptr)",
            "clock_gettime(0, nullptr)",
            "GetSystemTime()",
            "SecItemCopyMatching()",
            "CredRead()",
            "CredWrite()",
            "CredDelete()",
            "secret_password_lookup()",
        ):
            with self.subTest(expression=expression):
                self.assert_rejected(f"auto value = {expression};\n")

    def test_embedded_provider_endpoint_is_rejected(self) -> None:
        self.assert_rejected(
            'constexpr auto endpoint = "https://api.github.com";\n'
        )


if __name__ == "__main__":
    unittest.main()
