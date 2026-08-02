#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR: Path
CMAKE_COMMAND: Path
CPM_SOURCE_CACHE: Path
SQLITE_SOURCE: Path

SQLITE_COMPILE_DEFINITIONS = {
    "SQLITE_DEFAULT_FILE_PERMISSIONS=0600",
    "SQLITE_DEFAULT_FOREIGN_KEYS=1",
    "SQLITE_DEFAULT_SYNCHRONOUS=2",
    "SQLITE_DEFAULT_WAL_SYNCHRONOUS=2",
    "SQLITE_DQS=0",
    "SQLITE_ENABLE_API_ARMOR=1",
    "SQLITE_MAX_ATTACHED=0",
    "SQLITE_MAX_PAGE_COUNT=262144",
    "SQLITE_OMIT_AUTOINIT=1",
    "SQLITE_OMIT_DEPRECATED=1",
    "SQLITE_OMIT_LOAD_EXTENSION=1",
    "SQLITE_OMIT_SHARED_CACHE=1",
    "SQLITE_TEMP_STORE=3",
    "SQLITE_THREADSAFE=1",
    "SQLITE_TRUSTED_SCHEMA=0",
    "SQLITE_USE_URI=0",
}


class DependencyModernizationTests(unittest.TestCase):
    def configure(
        self,
        temporary_directory: str,
        *extra: str,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                str(CMAKE_COMMAND),
                "-S",
                str(ROOT),
                "-B",
                str(Path(temporary_directory) / "build"),
                "-G",
                "Ninja",
                f"-DCPM_SOURCE_CACHE={CPM_SOURCE_CACHE}",
                *extra,
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_cmake_baseline_standard_options_and_presets(self) -> None:
        root = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("cmake_minimum_required(VERSION 4.4.2)", root)
        self.assertIn("HOMEPAGE_URL", root)
        self.assertIn('include("${CMAKE_ROOT}/Modules/CTest.cmake")', root)
        self.assertIn("if(BUILD_TESTING)", root)
        self.assertNotIn("option(BPR_BUILD_TESTS", root)

        presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
        self.assertEqual(presets["version"], 12)
        self.assertIn("v4.4.2", presets["$schema"])
        configure_names = {preset["name"] for preset in presets["configurePresets"]}
        self.assertTrue({"dev", "core-only", "windows-dev"} <= configure_names)
        for preset in presets["testPresets"]:
            if not preset.get("hidden"):
                self.assertEqual(preset.get("inherits"), "_test-common")

    def test_obsolete_option_fails_with_migration_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result = self.configure(
                temporary_directory,
                "-DBPR_BUILD_TESTS=OFF",
            )
        self.assertNotEqual(result.returncode, 0)
        diagnostic = result.stdout + result.stderr
        self.assertIn("BPR_BUILD_TESTS was removed", diagnostic)
        self.assertIn("BUILD_TESTING", diagnostic)

    def test_dependency_discovery_is_centralized_and_package_based(self) -> None:
        provider = (ROOT / "cmake/BprDependencies.cmake").read_text(
            encoding="utf-8"
        )
        self.assertRegex(provider, r"find_package\(\s*Qt6\b")
        for package in ("Threads", "Python3", "PkgConfig", "Git"):
            self.assertIn(f"bpr_include_builtin_find_module({package})", provider)
        for version in ('"6.8"', '"0.20"', '"3.10"', '"2.39"'):
            self.assertIn(version, provider)
        for target in (
            "Python3::Interpreter",
            '"Qt6::${component}"',
            "PkgConfig::BPR_LIBSECRET",
            "LibSecret::LibSecret",
            "Git::Git",
        ):
            self.assertRegex(
                provider,
                rf"bpr_require_dependency_target_absent\(\s*{re.escape(target)}",
            )
        self.assertIn("Qt is never fetched automatically", provider)
        self.assertFalse(
            (ROOT / "cmake/dependency-bootstrap/CMakeLists.txt").exists()
        )

        product_files = list((ROOT / "src").rglob("CMakeLists.txt"))
        product_files.extend((ROOT / "tests").rglob("CMakeLists.txt"))
        product_files.append(ROOT / "third_party/sqlite/CMakeLists.txt")
        for path in product_files:
            contents = path.read_text(encoding="utf-8")
            self.assertNotIn("find_package(", contents, str(path))
            self.assertNotIn("pkg_check_modules(", contents, str(path))

    def test_predeclared_dependency_target_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            source = temporary / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 4.4.2)\n"
                "project(InjectedDependency LANGUAGES C CXX)\n"
                "add_executable(Python3::Interpreter IMPORTED GLOBAL)\n"
                "set_property(TARGET Python3::Interpreter PROPERTY "
                "IMPORTED_LOCATION /usr/bin/false)\n"
                "set(BPR_BUILD_QT OFF)\n"
                "set(BUILD_TESTING ON)\n"
                f"include([==[{ROOT / 'cmake/BprDependencies.cmake'}]==])\n"
                "bpr_configure_dependencies()\n",
                encoding="utf-8",
            )
            configured = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(source),
                    "-B",
                    str(temporary / "build"),
                    "-G",
                    "Ninja",
                    f"-DCPM_SOURCE_CACHE={CPM_SOURCE_CACHE}",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertNotEqual(configured.returncode, 0)
        self.assertIn(
            "Python3 target 'Python3::Interpreter' must not be defined",
            configured.stdout + configured.stderr,
        )

    def test_standard_tool_discovery_ignores_config_preference(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            source = temporary / "source"
            package = temporary / "fake package"
            source.mkdir()
            package.mkdir()
            marker = temporary / "config-package-executed"
            (package / "Python3Config.cmake").write_text(
                f"file(WRITE [==[{marker}]==] executed)\n"
                "add_executable(Python3::Interpreter IMPORTED GLOBAL)\n"
                "set_property(TARGET Python3::Interpreter PROPERTY "
                "IMPORTED_LOCATION /usr/bin/false)\n"
                "set(Python3_FOUND TRUE)\n",
                encoding="utf-8",
            )
            (package / "Python3ConfigVersion.cmake").write_text(
                "set(PACKAGE_VERSION 99.0)\n"
                "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n",
                encoding="utf-8",
            )
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 4.4.2)\n"
                "project(ConfigPreference LANGUAGES C CXX)\n"
                "set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)\n"
                "set(BPR_BUILD_QT OFF)\n"
                "set(BUILD_TESTING ON)\n"
                f"list(PREPEND CMAKE_PREFIX_PATH [==[{package}]==])\n"
                f"include([==[{ROOT / 'cmake/BprDependencies.cmake'}]==])\n"
                "bpr_configure_dependencies()\n",
                encoding="utf-8",
            )
            configured = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(source),
                    "-B",
                    str(temporary / "build"),
                    "-G",
                    "Ninja",
                    f"-DCPM_SOURCE_CACHE={CPM_SOURCE_CACHE}",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertEqual(
            configured.returncode,
            0,
            configured.stdout + configured.stderr,
        )
        self.assertFalse(marker.exists())

    def test_provider_owned_package_redirect_is_rejected_unexecuted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            source = temporary / "source"
            source.mkdir()
            marker = temporary / "redirect-executed"
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 4.4.2)\n"
                "project(PackageRedirect LANGUAGES C CXX)\n"
                "file(MAKE_DIRECTORY \"${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}\")\n"
                "file(WRITE \"${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/"
                "python3-config.cmake\" \"file(WRITE [==["
                f"{marker}"
                "]==] executed)\\n\")\n"
                "set(BPR_BUILD_QT OFF)\n"
                "set(BUILD_TESTING ON)\n"
                f"include([==[{ROOT / 'cmake/BprDependencies.cmake'}]==])\n"
                "bpr_configure_dependencies()\n",
                encoding="utf-8",
            )
            configured = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(source),
                    "-B",
                    str(temporary / "build"),
                    "-G",
                    "Ninja",
                    f"-DCPM_SOURCE_CACHE={CPM_SOURCE_CACHE}",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertNotEqual(configured.returncode, 0)
        self.assertIn(
            "Package redirect for provider-owned Python3 is forbidden",
            configured.stdout + configured.stderr,
        )
        self.assertFalse(marker.exists())

    def test_top_level_dependency_provider_is_rejected_unexecuted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            marker = temporary / "dependency-provider-executed"
            provider = temporary / "dependency-provider.cmake"
            provider.write_text(
                f"file(WRITE [==[{marker}]==] executed)\n",
                encoding="utf-8",
            )
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
                f"-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES={provider}",
            )
        self.assertNotEqual(configured.returncode, 0)
        self.assertIn(
            "does not permit CMAKE_PROJECT_TOP_LEVEL_INCLUDES",
            configured.stdout + configured.stderr,
        )
        self.assertFalse(marker.exists())

    def test_repository_modules_ignore_caller_module_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            fake_modules = temporary / "fake modules"
            fake_modules.mkdir()
            marker = temporary / "caller-module-executed"
            fake_module = (
                f"file(WRITE [==[{marker}]==] executed)\n"
                'message(FATAL_ERROR "caller module executed")\n'
            )
            for name in (
                "AitoCpm.cmake",
                "BprDependencies.cmake",
                "BprSQLiteProvider.cmake",
                "CTest.cmake",
                "FetchContent.cmake",
                "FindThreads.cmake",
            ):
                (fake_modules / name).write_text(fake_module, encoding="utf-8")
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
                f"-DCMAKE_MODULE_PATH={fake_modules}",
            )
        self.assertEqual(
            configured.returncode,
            0,
            configured.stdout + configured.stderr,
        )
        self.assertFalse(marker.exists())

    def test_configured_dependency_audit_uses_supported_providers(self) -> None:
        audit = BUILD_DIR / "_bpr/product-dependency-audit.cmake"
        self.assertTrue(audit.is_file(), audit)
        records = re.findall(r"\[==\[([^\n]+)\]==\]", audit.read_text(encoding="utf-8"))
        self.assertTrue(records)
        modes = {record.split("|", 2)[1] for record in records}
        self.assertTrue(
            modes
            <= {"CMAKE_CONFIG", "CMAKE_MODULE", "PKG_CONFIG", "TOOLCHAIN"}
        )
        targets = {record.split("|", 2)[2] for record in records}
        self.assertIn("Threads::Threads", targets)
        if any(target.startswith("Qt6::") for target in targets):
            self.assertTrue(
                {"Qt6::Core", "Qt6::Gui", "Qt6::Widgets", "Qt6::Network"}
                <= targets
            )

    def test_tests_disabled_core_graph_omits_test_and_qt_dependencies(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
                "-DCMAKE_BUILD_TYPE=Release",
            )
            self.assertEqual(
                configured.returncode, 0, configured.stdout + configured.stderr
            )
            audit = (
                Path(temporary_directory)
                / "build/_bpr/product-dependency-audit.cmake"
            ).read_text(encoding="utf-8")
            self.assertIn("Threads|TOOLCHAIN|Threads::Threads", audit)
            for dependency in ("Python3", "Git", "Qt6", "LibSecret"):
                self.assertNotIn(dependency, audit)

    def test_qt_is_package_only_and_never_fetched(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=ON",
                "-DBUILD_TESTING=OFF",
                "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON",
            )
        self.assertNotEqual(configured.returncode, 0)
        diagnostic = configured.stdout + configured.stderr
        self.assertIn("caller-installed Qt", diagnostic)
        self.assertIn("never fetched automatically", diagnostic)

    def test_sqlite_is_download_only_and_not_vendored_or_system_resolved(self) -> None:
        provider = (ROOT / "cmake/BprSQLiteProvider.cmake").read_text(
            encoding="utf-8"
        )
        target = (ROOT / "third_party/sqlite/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("CPMAddPackage(", provider)
        self.assertIn("DOWNLOAD_ONLY YES", provider)
        self.assertIn('LOCK "${sqlite_lock}"', provider)
        self.assertIn("GUARD FUNCTION", provider)
        self.assertIn(
            "628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e",
            provider,
        )
        self.assertIn("BPR_SQLITE_SOURCE_DIR", target)
        self.assertRegex(target, r"add_library\(\s*better_pull_review_sqlite3\s+STATIC")
        definitions = set(
            re.findall(r"(?m)^        (SQLITE_[A-Z0-9_]+(?:=[^\s)]+)?)$", target)
        )
        self.assertEqual(definitions, SQLITE_COMPILE_DEFINITIONS)
        self.assertNotIn("find_package(SQLite", provider + target)
        self.assertFalse((ROOT / "third_party/sqlite/amalgamation/sqlite3.c").exists())
        self.assertFalse((ROOT / "third_party/sqlite/amalgamation/sqlite3.h").exists())

    def test_sqlite_offline_cache_reuse_and_uncached_failure(self) -> None:
        self.assertTrue((CPM_SOURCE_CACHE / "cpm").is_dir())
        self.assertTrue((CPM_SOURCE_CACHE / "bprsqlite").is_dir())
        for copy_sqlite, expected_success in ((True, True), (False, False)):
            with self.subTest(copy_sqlite=copy_sqlite):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    temporary = Path(temporary_directory)
                    cache = temporary / "offline cache"
                    shutil.copytree(CPM_SOURCE_CACHE / "cpm", cache / "cpm")
                    if copy_sqlite:
                        shutil.copytree(
                            CPM_SOURCE_CACHE / "bprsqlite", cache / "bprsqlite"
                        )
                    configured = subprocess.run(
                        [
                            str(CMAKE_COMMAND),
                            "-S",
                            str(ROOT),
                            "-B",
                            str(temporary / "offline build"),
                            "-G",
                            "Ninja",
                            "-DBPR_DEPENDENCY_OFFLINE=ON",
                            "-DBPR_BUILD_QT=OFF",
                            "-DBUILD_TESTING=OFF",
                            f"-DCPM_SOURCE_CACHE={cache}",
                        ],
                        check=False,
                        capture_output=True,
                        text=True,
                    )
                    diagnostic = configured.stdout + configured.stderr
                    if expected_success:
                        self.assertEqual(configured.returncode, 0, diagnostic)
                    else:
                        self.assertNotEqual(configured.returncode, 0)
                        self.assertIn("SQLite 3.53.4", diagnostic)

    def test_sqlite_staging_excludes_unverified_cache_headers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            cache = temporary / "poisoned cache"
            shutil.copytree(CPM_SOURCE_CACHE / "cpm", cache / "cpm")
            shutil.copytree(
                CPM_SOURCE_CACHE / "bprsqlite",
                cache / "bprsqlite",
            )
            cached_sources = list((cache / "bprsqlite").rglob("sqlite3.c"))
            self.assertEqual(len(cached_sources), 1)
            (cached_sources[0].parent / "stdlib.h").write_text(
                "#error unverified cache header reached the compiler\n",
                encoding="utf-8",
            )
            build = temporary / "build"
            configured = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(ROOT),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    "-DBPR_DEPENDENCY_OFFLINE=ON",
                    "-DBPR_BUILD_QT=OFF",
                    "-DBUILD_TESTING=OFF",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    f"-DCPM_SOURCE_CACHE={cache}",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                configured.returncode,
                0,
                configured.stdout + configured.stderr,
            )
            compiled = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "--build",
                    str(build),
                    "--target",
                    "better_pull_review_sqlite3",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                compiled.returncode,
                0,
                compiled.stdout + compiled.stderr,
            )
            commands = json.loads(
                (build / "compile_commands.json").read_text(encoding="utf-8")
            )
            sqlite_commands = [
                entry for entry in commands if entry["file"].endswith("sqlite3.c")
            ]
            self.assertEqual(len(sqlite_commands), 1)
            command = sqlite_commands[0]["command"]
            self.assertIn("_bpr/sqlite/3.53.4", command)
            self.assertNotIn(str(cache / "bprsqlite"), command)

    def test_sqlite_staging_rejects_unexpected_generated_entries(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
            )
            self.assertEqual(
                configured.returncode,
                0,
                configured.stdout + configured.stderr,
            )
            stage = (
                Path(temporary_directory)
                / "build/_bpr/sqlite/3.53.4"
            )
            (stage / "unexpected.h").write_text(
                "#error unexpected staging input\n",
                encoding="utf-8",
            )
            rejected = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
            )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn(
            "Controlled SQLite staging directory contains unexpected entry",
            rejected.stdout + rejected.stderr,
        )

    def test_sqlite_rejects_source_version_and_identity_mismatches(self) -> None:
        cases = (
            (
                "sqlite3.h",
                '"3.53.4"',
                '"3.53.3"',
                "BPR_SQLITE_H_SHA3_256",
                "wrong SQLITE_VERSION",
            ),
            (
                "sqlite3.c",
                "2026-07-24 19:02:57",
                "2026-07-24 19:02:58",
                "BPR_SQLITE_C_SHA3_256",
                "wrong SQLITE_SOURCE_ID",
            ),
        )
        for filename, old, new, digest_variable, expected in cases:
            with self.subTest(filename=filename):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    source = Path(temporary_directory) / "sqlite"
                    source.mkdir()
                    for sqlite_file in ("sqlite3.c", "sqlite3.h"):
                        shutil.copy2(SQLITE_SOURCE / sqlite_file, source / sqlite_file)
                    path = source / filename
                    contents = path.read_text(encoding="utf-8")
                    path.write_text(contents.replace(old, new, 1), encoding="utf-8")
                    script = Path(temporary_directory) / "validate-sqlite.cmake"
                    script.write_text(
                        f"include([==[{ROOT / 'cmake/BprSQLiteProvider.cmake'}]==])\n"
                        f"file(SHA3_256 [==[{path}]==] mutated_digest)\n"
                        f"set({digest_variable} \"${{mutated_digest}}\")\n"
                        f"bpr_validate_sqlite_source([==[{source}]==])\n",
                        encoding="utf-8",
                    )
                    rejected = subprocess.run(
                        [str(CMAKE_COMMAND), "-P", str(script)],
                        check=False,
                        capture_output=True,
                        text=True,
                    )
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn(expected, rejected.stdout + rejected.stderr)

    def test_cpm_bootstrap_is_exactly_pinned(self) -> None:
        bootstrap = (ROOT / "cmake/AitoCpm.cmake").read_text(encoding="utf-8")
        self.assertIn('BPR_CPM_VERSION "0.43.1"', bootstrap)
        self.assertIn(
            "1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934",
            bootstrap,
        )
        self.assertIn("BPR_DEPENDENCY_OFFLINE", bootstrap)
        self.assertIn('LOCK "${cpm_lock}"', bootstrap)
        self.assertIn("GUARD FUNCTION", bootstrap)

    def test_sqlite_archive_hash_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            corrupt_archive = temporary / "sqlite-amalgamation-3530400.zip"
            corrupt_archive.write_bytes(b"not the verified SQLite archive")
            provider_directory = temporary / "cmake"
            provider_directory.mkdir()
            shutil.copy2(
                ROOT / "cmake/AitoCpm.cmake",
                provider_directory / "AitoCpm.cmake",
            )
            provider = (ROOT / "cmake/BprSQLiteProvider.cmake").read_text(
                encoding="utf-8"
            )
            provider = provider.replace(
                "https://sqlite.org/2026/sqlite-amalgamation-"
                "${BPR_SQLITE_SOURCE_NUMBER}.zip",
                corrupt_archive.as_uri(),
            )
            (provider_directory / "BprSQLiteProvider.cmake").write_text(
                provider,
                encoding="utf-8",
            )
            cache = temporary / "cache"
            shutil.copytree(CPM_SOURCE_CACHE / "cpm", cache / "cpm")
            source = temporary / "source"
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 4.4.2)\n"
                "project(CorruptSQLiteArchive LANGUAGES NONE)\n"
                f"set(CPM_SOURCE_CACHE [==[{cache}]==] CACHE PATH \"\" FORCE)\n"
                f"include([==[{provider_directory / 'BprSQLiteProvider.cmake'}]==])\n"
                "bpr_acquire_sqlite(sqlite_source)\n",
                encoding="utf-8",
            )
            rejected = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(source),
                    "-B",
                    str(temporary / "build"),
                    "-G",
                    "Ninja",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(rejected.returncode, 0)
            diagnostic = rejected.stdout + rejected.stderr
            self.assertIn("does not match expected value", diagnostic)
            self.assertIn(
                "628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e",
                diagnostic,
            )

    def test_sqlite_rejects_fetchcontent_source_override(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
                f"-DFETCHCONTENT_SOURCE_DIR_BPRSQLITE={SQLITE_SOURCE}",
            )
        self.assertNotEqual(configured.returncode, 0)
        self.assertIn(
            "FETCHCONTENT_SOURCE_DIR_BPRSQLITE is forbidden",
            configured.stdout + configured.stderr,
        )

    def test_sqlite_cpm_force_ignores_local_package_override(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            package = temporary / "fake local package"
            package.mkdir()
            marker = temporary / "local-package-executed"
            (package / "BprSQLiteConfig.cmake").write_text(
                f"file(WRITE [==[{marker}]==] executed)\n"
                f"set(BprSQLite_SOURCE_DIR [==[{SQLITE_SOURCE}]==])\n",
                encoding="utf-8",
            )
            (package / "BprSQLiteConfigVersion.cmake").write_text(
                "set(PACKAGE_VERSION 3.53.4)\n"
                "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n",
                encoding="utf-8",
            )
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=OFF",
                "-DBUILD_TESTING=OFF",
                "-DCPM_USE_LOCAL_PACKAGES=ON",
                f"-DBprSQLite_DIR={package}",
            )
            diagnostic = configured.stdout + configured.stderr
            self.assertEqual(configured.returncode, 0, diagnostic)
            self.assertFalse(marker.exists())
            self.assertNotIn("Using local package BprSQLite", diagnostic)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--cmake", required=True, type=Path)
    parser.add_argument("--cpm-source-cache", required=True, type=Path)
    parser.add_argument("--sqlite-source", required=True, type=Path)
    args, remaining = parser.parse_known_args()
    BUILD_DIR = args.build_dir.resolve()
    CMAKE_COMMAND = args.cmake.resolve()
    CPM_SOURCE_CACHE = args.cpm_source_cache.resolve()
    SQLITE_SOURCE = args.sqlite_source.resolve()
    unittest.main(argv=[__file__, *remaining])
