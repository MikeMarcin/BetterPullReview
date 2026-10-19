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
            "target_link_libraries(better_pull_review_core PRIVATE BetterPullReview::ZLib glaze::glaze BetterPullReview::Sha1dc BetterPullReview::HaclSha2 BetterPullReview::RapidFuzz BetterPullReview::Dtl BetterPullReview::RE2 BetterPullReview::Xdiff)\n",
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

    def test_re2_has_one_private_owner(self) -> None:
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
            "std::localtime(&observed_time)",
            "std::mktime(&observed_calendar)",
            "std::ctime(&observed_time)",
            'std::strftime(output, size, "%Z", &observed_calendar)',
            "rand()",
            "std::chrono::utc_clock::now()",
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

    def test_explicit_synchronization_and_lexical_paths_are_allowed(self) -> None:
        result = self.run_check(
            "#include <atomic>\n#include <mutex>\n#include <filesystem>\n"
            "auto stopped(const std::atomic_bool& stop) { return stop.load(); }\n"
            "void update(std::mutex& guard, int& state) { std::lock_guard lock{guard}; ++state; }\n"
            'auto path = std::filesystem::path{"src"} / ".." / "include";\n'
            "auto normalized = path.lexically_normal();\n"
            "auto parent = normalized.parent_path();\n"
            "auto suffix = normalized.extension();\n"
            "auto relative_path = normalized.lexically_relative(parent);\n"
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_filesystem_effects_are_rejected(self) -> None:
        for operation in (
            "exists", "canonical", "weakly_canonical", "absolute", "relative",
            "proximate", "current_path", "temp_directory_path", "file_size",
            "status", "symlink_status", "is_directory", "is_regular_file",
            "last_write_time", "copy", "copy_file", "create_directory",
            "read_symlink", "remove", "remove_all", "rename", "resize_file",
            "permissions", "directory_iterator", "recursive_directory_iterator",
            "directory_entry",
        ):
            for prefix in ("std::filesystem::", "fs::", ""):
                with self.subTest(operation=operation, prefix=prefix):
                    preamble = "namespace fs = std::filesystem;\n" if prefix == "fs::" else ("using namespace std::filesystem;\n" if not prefix else "")
                    self.assert_rejected(preamble + f"auto value = {prefix}{operation}(path);\n")
        self.assert_rejected("using std::filesystem::directory_iterator;\n")

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

    def test_inert_standard_headers_types_and_domain_names_are_allowed(self) -> None:
        result = self.run_check(
            "#include <thread>\n#include <future>\n#include <ctime>\n"
            "#include <condition_variable>\n#include <fstream>\n"
            "auto classify(std::thread::id id, const std::tm& observed, "
            "std::future<int>& result, std::condition_variable& ready, "
            "std::ifstream& input) -> bool;\n"
            "auto stamp = observed_clock::now();\n"
            "auto sample = model.time() + model.clock() + model.random();\n"
            "auto selected = domain::connect(); auto opened = model.open();\n"
            "auto value = domain::time() + domain::clock() + domain::random();\n"
            'constexpr auto link = "https://example.test/help";\n'
            '// Documentation: http://example.test/reference\n'
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_known_clock_and_entropy_aliases_are_rejected(self) -> None:
        for source in (
            "namespace ch = std::chrono; auto stamp = ch::steady_clock::now();",
            "namespace ch = ::std::chrono; auto stamp = ch::steady_clock::now();",
            "namespace standard = std; auto stamp = standard::chrono::steady_clock::now();",
            "using clock_alias = std::chrono::system_clock; auto stamp = clock_alias::now();",
            "typedef std::chrono::high_resolution_clock clock_alias; auto stamp = clock_alias::now();",
            "using std::chrono::utc_clock; auto stamp = utc_clock::now();",
            "using namespace std::chrono; auto stamp = steady_clock::now();",
            "auto stamp = ::std::chrono::steady_clock::now();",
            "auto stamp = std :: chrono :: steady_clock ::\n now ();",
            "using entropy = std::random_device; entropy source;",
        ):
            with self.subTest(source=source):
                self.assert_rejected(source)

    def test_worker_launch_and_file_acquisition_are_rejected(self) -> None:
        for source in (
            "std::thread worker{run};",
            "std::thread worker = {run};",
            "auto worker = std::jthread(run);",
            "using worker_type = std::thread; worker_type worker(run);",
            "using std::thread; thread worker(run);",
            "auto task = std::async(std::launch::async, run);",
            "auto task = std::async(run);",
            "auto task = std::async(std::launch::deferred | std::launch::async, run);",
            "using std::async; auto task = async(run);",
            "std::this_thread::sleep_for(delay);",
            "std::this_thread::yield();",
            "std::this_thread::get_id();",
            "std::thread::hardware_concurrency();",
            "using std::this_thread::sleep_for; sleep_for(delay);",
            'std::ifstream input{"state"};',
            'std::ifstream input = {"state"};',
            'std::basic_ifstream<char> input{"state"};',
            'auto input = std::ofstream("state");',
            'using input_type = std::ifstream; input_type input("state");',
            'using std::ifstream; ifstream input("state");',
            'std::fstream input; input.open("state");',
            'void read(std::fstream& input) { input.open("state"); }',
            'std::filebuf buffer; buffer.open("state", std::ios::in);',
            'std :: ifstream input; input.open("state");',
            'auto task = std::async(std::launch::deferred, [] { return std::time(nullptr); });',
        ):
            with self.subTest(source=source):
                self.assert_rejected(source)

    def test_inert_thread_and_stream_objects_are_allowed(self) -> None:
        result = self.run_check(
            "std::thread worker; std::jthread pending{};\n"
            "std::ifstream input; std::ofstream output{}; std::filebuf buffer;\n"
            "auto mode = std::launch::deferred;\n"
            "auto task = std::async(std::launch::deferred, [] { return 5; });\n"
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_all_core_source_extensions_are_checked(self) -> None:
        for suffix in (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".m", ".mm"):
            with self.subTest(suffix=suffix):
                result = self.run_check("auto stamp = std::chrono::steady_clock::now();",
                                        source_path=f"src/core/policy{suffix}")
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("Functional-core boundary violation", result.stderr)


if __name__ == "__main__":
    unittest.main()
