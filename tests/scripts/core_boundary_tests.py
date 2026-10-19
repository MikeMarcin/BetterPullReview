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
    def run_check(self, source: str, cmake: str | None = None,
                  source_path: str = "src/core/policy.cpp") -> subprocess.CompletedProcess[str]:
        with contained_temporary_directory() as temporary_directory:
            root = Path(temporary_directory)
            core_source = root / source_path
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
        # sha1collisiondetection and HACL* SHA-2 (RFC-BPR-0080 Q1 and Q2),
        # then rapidfuzz-cpp (RFC-BPR-0082 Q2, [[BPR-1480]]) and dtl for
        # per-line intraline comparison ([[BPR-1481]]) and the bounded Review
        # Cycle suggestion hunks ([[BPR-1498]]).
        approved = self.run_check(
            "struct identity {};\n",
            "target_link_libraries(better_pull_review_core PRIVATE BetterPullReview::ZLib glaze::glaze BetterPullReview::Sha1dc BetterPullReview::HaclSha2 BetterPullReview::RapidFuzz BetterPullReview::Dtl BetterPullReview::RE2)\n",
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

    def test_re2_exception_has_one_private_owner(self) -> None:
        allowed = self.run_check('#include <re2/re2.h>\n',
                                 source_path="src/core/src/source_regex_engine.cpp")
        self.assertEqual(allowed.returncode, 0, allowed.stdout + allowed.stderr)
        for path in ("src/core/include/bpr/core/source_matcher.hpp", "src/core/src/policy.cpp"):
            with self.subTest(path=path):
                refused = self.run_check('#include <re2/re2.h>\n', source_path=path)
                self.assertNotEqual(refused.returncode, 0)
        refused = self.run_check('#include <absl/synchronization/mutex.h>\n',
                                 source_path="src/core/src/source_regex_engine.cpp")
        self.assertNotEqual(refused.returncode, 0)

    def test_qt_provider_and_effect_headers_are_rejected(self) -> None:
        for header in (
            "QNetworkAccessManager",
            "windows.h",
            "bcrypt.h",
            "CommonCrypto/CommonDigest.h",
            "sqlite3.h",
            "curl/curl.h",
            "thread",
            "future",
            "filesystem",
            "ctime",
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
            "clock()",
            "rand()",
            "std::chrono::utc_clock::now()",
            "clock_alias::now()",
            "std::chrono::steady_clock::now()",
            "std::chrono::system_clock::now()",
            "std::random_device{}()",
            "SecItemCopyMatching()",
            "CredRead()",
            "CredWrite()",
            "CredDelete()",
            "secret_password_lookup()",
        ):
            with self.subTest(expression=expression):
                self.assert_rejected(f"auto value = {expression};\n")

    def test_duration_arithmetic_and_explicit_random_state_are_allowed(self) -> None:
        result = self.run_check(
            "#include <chrono>\n#include <random>\n"
            "auto duration = std::chrono::milliseconds{5} + std::chrono::seconds{1};\n"
            "auto deadline = std::chrono::time_point<std::chrono::steady_clock>{} + duration;\n"
            "std::mt19937 engine{1234}; auto value = engine();\n"
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cng_link_is_owned_by_its_windows_branch(self) -> None:
        allowed = self.run_check("struct identity {};\n",
            "if(APPLE)\nelseif(WIN32)\n"
            "    target_sources(better_pull_review_core PRIVATE src/sha256_cng.cpp)\n"
            "    target_link_libraries(better_pull_review_core PRIVATE bcrypt)\nendif()\n")
        self.assertEqual(allowed.returncode, 0, allowed.stdout + allowed.stderr)
        refused = self.run_check("struct identity {};\n",
            "target_link_libraries(better_pull_review_core PRIVATE bcrypt)\n")
        self.assertNotEqual(refused.returncode, 0)

    def test_platform_sha256_exception_does_not_grant_other_capabilities(self) -> None:
        for path in ("src/core/src/sha256_cng.cpp", "src/core/src/sha256_commoncrypto.cpp"):
            for source in ("BCryptGenRandom(handle, bytes, count, 0);\n",
                           "CC_SHA1_Init(context);\n",
                           "auto stamp = std::chrono::steady_clock::now();\n"):
                with self.subTest(path=path, source=source):
                    result = self.run_check(source, source_path=path)
                    self.assertNotEqual(result.returncode, 0)

    def test_embedded_provider_endpoint_is_rejected(self) -> None:
        self.assert_rejected(
            'constexpr auto endpoint = "https://api.github.com";\n'
        )


if __name__ == "__main__":
    unittest.main()
