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
        self.assertIn("include(CTest)", root)
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
        for package in ("Threads", "Python3", "Qt6", "PkgConfig", "Git"):
            self.assertRegex(provider, rf"find_package\(\s*{package}\b")
        for version in ('"6.8"', '"0.20"', '"3.10"', '"2.39"'):
            self.assertIn(version, provider)
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

    def test_configured_dependency_audit_uses_supported_providers(self) -> None:
        audit = BUILD_DIR / "_bpr/product-dependency-audit.cmake"
        self.assertTrue(audit.is_file(), audit)
        records = re.findall(r"\[==\[([^\n]+)\]==\]", audit.read_text(encoding="utf-8"))
        self.assertTrue(records)
        modes = {record.split("|", 2)[1] for record in records}
        self.assertTrue(modes <= {"PACKAGE", "PKG_CONFIG", "TOOLCHAIN"})
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
                        f"list(APPEND CMAKE_MODULE_PATH [==[{ROOT / 'cmake'}]==])\n"
                        "include(BprSQLiteProvider)\n"
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

    def test_sqlite_archive_hash_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            corrupt_archive = temporary / "sqlite-amalgamation-3530400.zip"
            corrupt_archive.write_bytes(b"not the verified SQLite archive")
            destination = temporary / "downloaded.zip"
            script = temporary / "verify-archive.cmake"
            script.write_text(
                "file(DOWNLOAD\n"
                f"  [==[{corrupt_archive.as_uri()}]==]\n"
                f"  [==[{destination}]==]\n"
                "  EXPECTED_HASH "
                "[==[SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e]==]\n"
                ")\n",
                encoding="utf-8",
            )
            rejected = subprocess.run(
                [str(CMAKE_COMMAND), "-P", str(script)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("HASH mismatch", rejected.stdout + rejected.stderr)

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
