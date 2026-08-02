#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
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
    def run_bootstrap_fixture(
        self,
        temporary_directory: str,
        *,
        generator: str = "Ninja",
        extra: tuple[str, ...] = (),
    ) -> tuple[subprocess.CompletedProcess[str], Path]:
        fixture = ROOT / "tests/fixtures/cps-producer/path with spaces/Qt6"
        build = Path(temporary_directory) / "bootstrap build"
        overlay = Path(temporary_directory) / "overlay path"
        result = subprocess.run(
            [
                str(CMAKE_COMMAND),
                "--fresh",
                "-S",
                str(ROOT / "cmake/dependency-bootstrap"),
                "-B",
                str(build),
                "-G",
                generator,
                f"-DBPR_CPS_OVERLAY={overlay}",
                "-DBPR_BOOTSTRAP_QT=ON",
                "-DBPR_BOOTSTRAP_LIBSECRET=OFF",
                "-DBPR_BOOTSTRAP_TEST_TOOLS=OFF",
                "-DBPR_BOOTSTRAP_GIT=OFF",
                f"-DQt6_DIR={fixture}",
                *extra,
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        return result, overlay

    def validate_overlay(
        self, temporary_directory: str, overlay: Path
    ) -> subprocess.CompletedProcess[str]:
        script = Path(temporary_directory) / "validate-overlay.cmake"
        script.write_text(
            "list(APPEND CMAKE_MODULE_PATH [==["
            f"{ROOT / 'cmake'}"
            "]==])\n"
            "include(BprDependencies)\n"
            "bpr_validate_overlay_artifacts([==["
            f"{overlay}"
            "]==])\n",
            encoding="utf-8",
        )
        return subprocess.run(
            [str(CMAKE_COMMAND), "-P", str(script)],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_cmake_baseline_and_standard_test_option(self) -> None:
        root = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("cmake_minimum_required(VERSION 4.4.2)", root)
        for relative_path in (
            "cmake/dependency-bootstrap/CMakeLists.txt",
            "tests/end_to_end/epoch_persistence_contract.cmake",
            "tests/end_to_end/epoch_maintenance_public_path_contract.cmake",
        ):
            with self.subTest(relative_path=relative_path):
                self.assertTrue(
                    (ROOT / relative_path)
                    .read_text(encoding="utf-8")
                    .startswith("cmake_minimum_required(VERSION 4.4.2)"),
                    relative_path,
                )
        self.assertIn("include(CTest)", root)
        self.assertIn("if(BUILD_TESTING)", root)
        self.assertNotIn("option(BPR_BUILD_TESTS", root)

    def test_obsolete_option_fails_with_migration_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(ROOT),
                    "-B",
                    temporary_directory,
                    "-DBPR_BUILD_TESTS=OFF",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertNotEqual(result.returncode, 0)
        diagnostic = result.stdout + result.stderr
        self.assertIn("BPR_BUILD_TESTS was removed", diagnostic)
        self.assertIn("BUILD_TESTING", diagnostic)

    def test_product_cmake_owns_no_legacy_package_discovery(self) -> None:
        product_files = list((ROOT / "src").rglob("CMakeLists.txt"))
        product_files.extend((ROOT / "tests").rglob("CMakeLists.txt"))
        product_files.append(ROOT / "third_party" / "sqlite" / "CMakeLists.txt")
        for path in product_files:
            contents = path.read_text(encoding="utf-8")
            self.assertNotIn("find_package(", contents, str(path))
            self.assertNotIn("pkg_check_modules(", contents, str(path))
            self.assertNotIn("PkgConfig::", contents, str(path))

    def test_main_graph_resolution_audit_is_cps_only(self) -> None:
        audit = BUILD_DIR / "_bpr" / "product-dependency-audit.cmake"
        self.assertTrue(audit.is_file(), audit)
        contents = audit.read_text(encoding="utf-8")
        overlay_match = re.search(
            r"set\(BPR_PRODUCT_CPS_OVERLAY \[==\[(.*?)\]==\]\)", contents
        )
        self.assertIsNotNone(overlay_match)
        overlay = Path(overlay_match.group(1)).resolve()  # type: ignore[union-attr]
        self.assertEqual(overlay, (BUILD_DIR / "_bpr/cps-overlay").resolve())
        records = re.findall(r"\[==\[([^\n]+\|[^\n]+)\]==\]", contents)
        self.assertGreaterEqual(len(records), 1)
        for record in records:
            package, config, target = record.split("|", 2)
            config_path = Path(config).resolve()
            self.assertEqual(config_path.suffix, ".cps", package)
            self.assertTrue(config_path.is_relative_to(overlay), config_path)
            self.assertTrue(config_path.is_file(), config_path)
            self.assertIn("::", target)

    def test_cps_documents_are_valid_and_artifact_fingerprinted(self) -> None:
        overlay = BUILD_DIR / "_bpr" / "cps-overlay"
        documents = sorted((overlay / "cps").glob("*/*.cps"))
        self.assertTrue(documents)
        for document in documents:
            data = json.loads(document.read_text(encoding="utf-8"))
            self.assertEqual(data["cps_version"], "0.14.1")
            self.assertTrue(data["components"])
            self.assertGreaterEqual(data["version"], "0")
        fingerprint = (overlay / "fingerprint.txt").read_text(encoding="utf-8").strip()
        self.assertRegex(fingerprint, r"^[0-9a-f]{64}$")
        manifest = overlay / "artifacts.json"
        self.assertTrue(manifest.is_file())
        self.assertEqual(
            json.loads(manifest.read_text(encoding="utf-8"))["generator_revision"],
            "2",
        )
        self.assertFalse(list(overlay.rglob("*.cmake")))

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
                    build = temporary / "offline build"
                    overlay = build / "_bpr/cps-overlay"
                    cache = temporary / "offline cache"
                    shutil.copytree(BUILD_DIR / "_bpr/cps-overlay", overlay)
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
                            str(build),
                            "-G",
                            "Ninja",
                            "-DBPR_DEPENDENCY_OFFLINE=ON",
                            "-DBPR_BUILD_QT=OFF",
                            "-DBUILD_TESTING=ON",
                            "-DCMAKE_BUILD_TYPE=Release",
                            f"-DCPM_SOURCE_CACHE={cache}",
                        ],
                        check=False,
                        capture_output=True,
                        text=True,
                    )
                    diagnostic = configured.stdout + configured.stderr
                    if not expected_success:
                        self.assertNotEqual(configured.returncode, 0)
                        self.assertIn("SQLite 3.53.4", diagnostic)
                        continue
                    self.assertEqual(configured.returncode, 0, diagnostic)
                    built = subprocess.run(
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
                        built.returncode, 0, built.stdout + built.stderr
                    )
                    static_libraries = list(
                        (build / "third_party/sqlite").glob(
                            "*better_pull_review_sqlite3.*"
                        )
                    )
                    self.assertEqual(len(static_libraries), 1)
                    self.assertEqual(
                        static_libraries[0].suffix,
                        ".lib" if os.name == "nt" else ".a",
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
                    self.assertIn(old, contents)
                    path.write_text(contents.replace(old, new, 1), encoding="utf-8")
                    script = Path(temporary_directory) / "validate-sqlite.cmake"
                    script.write_text(
                        "list(APPEND CMAKE_MODULE_PATH [==["
                        f"{ROOT / 'cmake'}"
                        "]==])\n"
                        "include(BprSQLiteProvider)\n"
                        f"file(SHA3_256 [==[{path}]==] mutated_digest)\n"
                        f"set({digest_variable} \"${{mutated_digest}}\")\n"
                        "bpr_validate_sqlite_source([==["
                        f"{source}"
                        "]==])\n",
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
            build = Path(temporary_directory) / "override build"
            configured = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(ROOT),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    "-DBPR_BUILD_QT=OFF",
                    "-DBUILD_TESTING=OFF",
                    f"-DCPM_SOURCE_CACHE={CPM_SOURCE_CACHE}",
                    f"-DFETCHCONTENT_SOURCE_DIR_BPRSQLITE={SQLITE_SOURCE}",
                ],
                check=False,
                capture_output=True,
                text=True,
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
            configured = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "-S",
                    str(ROOT),
                    "-B",
                    str(temporary / "local package build"),
                    "-G",
                    "Ninja",
                    "-DBPR_BUILD_QT=OFF",
                    "-DBUILD_TESTING=OFF",
                    "-DCPM_USE_LOCAL_PACKAGES=ON",
                    f"-DBprSQLite_DIR={package}",
                    f"-DCPM_SOURCE_CACHE={CPM_SOURCE_CACHE}",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            diagnostic = configured.stdout + configured.stderr
            self.assertEqual(configured.returncode, 0, diagnostic)
            self.assertFalse(marker.exists())
            self.assertNotIn("Using local package BprSQLite", diagnostic)

    def test_cps_bootstrap_preserves_configs_spaces_and_requirements(self) -> None:
        for generator in ("Ninja", "Ninja Multi-Config"):
            with self.subTest(generator=generator):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    result, overlay = self.run_bootstrap_fixture(
                        temporary_directory, generator=generator
                    )
                    self.assertEqual(
                        result.returncode, 0, result.stdout + result.stderr
                    )
                    document = json.loads(
                        (overlay / "cps/Qt6/Qt6.cps").read_text(
                            encoding="utf-8"
                        )
                    )
                    self.assertEqual(
                        document["configurations"],
                        ["debug", "release", "relwithdebinfo", "minsizerel"],
                    )
                    core = document["components"]["Core"]["configurations"]
                    self.assertIn("path with spaces", core["debug"]["location"])
                    self.assertIn("core-debug.dylib", core["debug"]["location"])
                    self.assertIn("core-release.dylib", core["release"]["location"])
                    self.assertNotIn(
                        "QT_NO_DEBUG", core["debug"]["definitions"]["*"]
                    )
                    self.assertIn(
                        "QT_NO_DEBUG", core["release"]["definitions"]["*"]
                    )
                    self.assertEqual(
                        core["debug"]["compile_features"], ["c++17", "c++11"]
                    )
                    gui = document["components"]["Gui"]["configurations"]
                    self.assertIn(":Core", gui["debug"]["requires"])
                    self.assertIn(
                        "fake-platform-library", gui["debug"]["link_libraries"]
                    )

    def test_cps_bootstrap_preserves_windows_dll_and_import_library_metadata(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result, overlay = self.run_bootstrap_fixture(
                temporary_directory, extra=("-DBPR_FAKE_QT_WINDOWS_ARTIFACTS=ON",)
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            document = json.loads(
                (overlay / "cps/Qt6/Qt6.cps").read_text(encoding="utf-8")
            )
            debug = document["components"]["Core"]["configurations"]["debug"]
            self.assertTrue(debug["location"].endswith("windows-debug.dll"))
            self.assertTrue(
                debug["link_location"].endswith("windows-debug.lib")
            )
            manifest = json.loads(
                (overlay / "artifacts.json").read_text(encoding="utf-8")
            )
            artifact_paths = {record["path"] for record in manifest["artifacts"]}
            self.assertTrue(
                any(path.endswith("windows-debug.dll") for path in artifact_paths)
            )
            self.assertTrue(
                any(path.endswith("windows-debug.lib") for path in artifact_paths)
            )

        with tempfile.TemporaryDirectory() as temporary_directory:
            result, overlay = self.run_bootstrap_fixture(
                temporary_directory,
                extra=(
                    "-DBPR_FAKE_QT_WINDOWS_ARTIFACTS=ON",
                    "-DBPR_FAKE_QT_WINDOWS_MAP_DEBUG_RELEASE=ON",
                ),
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            document = json.loads(
                (overlay / "cps/Qt6/Qt6.cps").read_text(encoding="utf-8")
            )
            debug = document["components"]["Core"]["configurations"]["debug"]
            self.assertTrue(debug["location"].endswith("windows-release.dll"))
            self.assertTrue(
                debug["link_location"].endswith("windows-release.lib")
            )

        with tempfile.TemporaryDirectory() as temporary_directory:
            result, overlay = self.run_bootstrap_fixture(
                temporary_directory,
                extra=(
                    "-DBPR_FAKE_QT_WINDOWS_ARTIFACTS=ON",
                    "-DBPR_FAKE_QT_WINDOWS_IMPLIB_ONLY=ON",
                ),
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            document = json.loads(
                (overlay / "cps/Qt6/Qt6.cps").read_text(encoding="utf-8")
            )
            debug = document["components"]["Core"]["configurations"]["debug"]
            self.assertNotIn("location", debug)
            self.assertTrue(
                debug["link_location"].endswith("windows-debug.lib")
            )

    def test_cps_bootstrap_preserves_cmake_false_supported_values(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result, overlay = self.run_bootstrap_fixture(
                temporary_directory,
                extra=("-DBPR_FAKE_QT_FALSE_SUPPORTED_VALUES=ON",),
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            document = json.loads(
                (overlay / "cps/Qt6/Qt6.cps").read_text(encoding="utf-8")
            )
            debug = document["components"]["Core"]["configurations"]["debug"]
            self.assertIn("OFF", debug["link_libraries"])
            self.assertIn("OFF", debug["definitions"]["*"])

    def test_cps_bootstrap_fingerprint_is_stable_until_metadata_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            first, overlay = self.run_bootstrap_fixture(temporary_directory)
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            fingerprint = overlay / "fingerprint.txt"
            first_contents = fingerprint.read_text(encoding="utf-8")
            first_mtime = fingerprint.stat().st_mtime_ns
            second, _ = self.run_bootstrap_fixture(temporary_directory)
            self.assertEqual(second.returncode, 0, second.stdout + second.stderr)
            self.assertEqual(
                fingerprint.read_text(encoding="utf-8"), first_contents
            )
            self.assertEqual(fingerprint.stat().st_mtime_ns, first_mtime)

    def test_cps_bootstrap_prunes_stale_documents(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            generated, overlay = self.run_bootstrap_fixture(temporary_directory)
            self.assertEqual(
                generated.returncode, 0, generated.stdout + generated.stderr
            )
            self.assertTrue((overlay / "cps/Qt6/Qt6.cps").is_file())
            regenerated = subprocess.run(
                [
                    str(CMAKE_COMMAND),
                    "--fresh",
                    "-S",
                    str(ROOT / "cmake/dependency-bootstrap"),
                    "-B",
                    str(Path(temporary_directory) / "bootstrap build"),
                    "-G",
                    "Ninja",
                    f"-DBPR_CPS_OVERLAY={overlay}",
                    "-DBPR_BOOTSTRAP_QT=OFF",
                    "-DBPR_BOOTSTRAP_LIBSECRET=OFF",
                    "-DBPR_BOOTSTRAP_TEST_TOOLS=OFF",
                    "-DBPR_BOOTSTRAP_GIT=OFF",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                regenerated.returncode, 0, regenerated.stdout + regenerated.stderr
            )
            self.assertFalse(list((overlay / "cps").rglob("*.cps")))
            manifest = json.loads(
                (overlay / "artifacts.json").read_text(encoding="utf-8")
            )
            self.assertEqual(manifest["documents"], [])
            valid = self.validate_overlay(temporary_directory, overlay)
            self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)

    def test_copied_overlay_validates_its_own_documents(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            generated, overlay = self.run_bootstrap_fixture(temporary_directory)
            self.assertEqual(
                generated.returncode, 0, generated.stdout + generated.stderr
            )
            copied = Path(temporary_directory) / "copied overlay"
            shutil.copytree(overlay, copied)
            copied_document = copied / "cps/Qt6/Qt6.cps"
            copied_document.write_text(
                copied_document.read_text(encoding="utf-8") + "\n",
                encoding="utf-8",
            )
            rejected = self.validate_overlay(temporary_directory, copied)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn(
                "CPS overlay DOCUMENT changed", rejected.stdout + rejected.stderr
            )

    def test_overlay_manifest_is_data_and_never_executes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            generated, overlay = self.run_bootstrap_fixture(temporary_directory)
            self.assertEqual(
                generated.returncode, 0, generated.stdout + generated.stderr
            )
            canary = Path(temporary_directory) / "manifest-executed"
            sentinel = Path(temporary_directory) / "network-sentinel"
            sentinel.write_text("must not be fetched", encoding="utf-8")
            (overlay / "artifacts.json").write_text(
                "file(DOWNLOAD "
                f"[==[{sentinel.as_uri()}]==] [==[{canary}]==])\n",
                encoding="utf-8",
            )
            rejected = self.validate_overlay(temporary_directory, overlay)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn(
                "fingerprint does not match", rejected.stdout + rejected.stderr
            )
            self.assertFalse(canary.exists())

    def test_cps_overlay_rejects_tampering_and_cmake_package_scripts(self) -> None:
        for tamper in ("document", "fingerprint", "cmake-script"):
            with self.subTest(tamper=tamper):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    generated, overlay = self.run_bootstrap_fixture(
                        temporary_directory
                    )
                    self.assertEqual(
                        generated.returncode,
                        0,
                        generated.stdout + generated.stderr,
                    )
                    valid = self.validate_overlay(temporary_directory, overlay)
                    self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)
                    if tamper == "document":
                        document = overlay / "cps/Qt6/Qt6.cps"
                        document.write_text(
                            document.read_text(encoding="utf-8") + "\n",
                            encoding="utf-8",
                        )
                        expected = "CPS overlay DOCUMENT changed"
                    elif tamper == "fingerprint":
                        (overlay / "fingerprint.txt").write_text(
                            "0" * 64 + "\n", encoding="utf-8"
                        )
                        expected = "fingerprint does not match"
                    else:
                        (overlay / "Qt6Config.cmake").write_text(
                            "message(FATAL_ERROR \"must not execute\")\n",
                            encoding="utf-8",
                        )
                        expected = "CMake-script package metadata is forbidden"
                    rejected = self.validate_overlay(temporary_directory, overlay)
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn(expected, rejected.stdout + rejected.stderr)

    def test_cps_bootstrap_rejects_unsupported_metadata_and_old_versions(self) -> None:
        cases = (
            (
                ("-DBPR_FAKE_QT_UNSUPPORTED=ON",),
                "Unsupported generator expression",
            ),
            (
                ("-DBPR_FAKE_QT_INCOMPLETE=ON",),
                "Incomplete CPS path metadata",
            ),
            (
                ("-DBPR_FAKE_QT_DIRECT_LINK=ON",),
                "Unsupported CPS usage requirement",
            ),
            (
                ("-DBPR_FAKE_QT_FALSE_DIRECT_LINK=ON",),
                "Unsupported CPS usage requirement",
            ),
            (
                (
                    "-DBPR_FAKE_QT_WINDOWS_ARTIFACTS=ON",
                    "-DBPR_FAKE_QT_WINDOWS_INCOMPLETE_DEBUG=ON",
                ),
                "Incomplete paired imported artifact",
            ),
            (
                (
                    "-DBPR_FAKE_QT_WINDOWS_ARTIFACTS=ON",
                    "-DBPR_FAKE_QT_WINDOWS_MAP_MISSING_WITH_GENERAL=ON",
                ),
                "has no artifact",
            ),
            (("-DBPR_FAKE_QT_VERSION=6.7.9",), "caller-installed Qt >= 6.8"),
        )
        for extra, expected in cases:
            with self.subTest(expected=expected):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    result, _ = self.run_bootstrap_fixture(
                        temporary_directory, extra=extra
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(expected, result.stdout + result.stderr)

    def test_product_cps_resolution_rejects_all_below_floor_versions(self) -> None:
        cases = (
            ("Python3", "3.9.99", "3.10", "ExecutableMetadata"),
            ("Git", "2.38.99", "2.39", "ExecutableMetadata"),
            ("LibSecret", "0.19.99", "0.20", "LibSecret"),
        )
        for package, provided, required, component in cases:
            with self.subTest(package=package):
                with tempfile.TemporaryDirectory() as temporary_directory:
                    temporary = Path(temporary_directory)
                    overlay = temporary / "overlay"
                    document = overlay / f"cps/{package}/{package}.cps"
                    document.parent.mkdir(parents=True)
                    document.write_text(
                        json.dumps(
                            {
                                "cps_version": "0.14.1",
                                "name": package,
                                "version": provided,
                                "compat_version": provided,
                                "cps_path": f"@prefix@/cps/{package}",
                                "components": {component: {"type": "interface"}},
                            },
                            separators=(",", ":"),
                        )
                        + "\n",
                        encoding="utf-8",
                    )
                    document_digest = hashlib.sha256(
                        document.read_bytes()
                    ).hexdigest()
                    manifest = {
                        "generator_revision": "2",
                        "artifacts": [],
                        "documents": [
                            {
                                "path": f"cps/{package}/{package}.cps",
                                "sha256": document_digest,
                            }
                        ],
                    }
                    manifest_text = (
                        json.dumps(manifest, separators=(",", ":")) + "\n"
                    )
                    (overlay / "artifacts.json").write_text(
                        manifest_text, encoding="utf-8"
                    )
                    (overlay / "fingerprint.txt").write_text(
                        hashlib.sha256(manifest_text.encode()).hexdigest() + "\n",
                        encoding="utf-8",
                    )
                    source = temporary / "source"
                    source.mkdir()
                    (source / "CMakeLists.txt").write_text(
                        "cmake_minimum_required(VERSION 4.4.2)\n"
                        "project(CpsFloor LANGUAGES NONE)\n"
                        f"list(APPEND CMAKE_MODULE_PATH [==[{ROOT / 'cmake'}]==])\n"
                        "include(BprDependencies)\n"
                        f"set(BPR_CPS_OVERLAY [==[{overlay}]==])\n"
                        "bpr_validate_overlay_artifacts(${BPR_CPS_OVERLAY})\n"
                        f"bpr_find_cps_package({package} {required} "
                        f"TARGET {package}::{component} COMPONENTS {component})\n",
                        encoding="utf-8",
                    )
                    configured = subprocess.run(
                        [
                            str(CMAKE_COMMAND),
                            "-S",
                            str(source),
                            "-B",
                            str(temporary / "build"),
                        ],
                        check=False,
                        capture_output=True,
                        text=True,
                    )
                    self.assertNotEqual(configured.returncode, 0)
                    self.assertIn(required, configured.stdout + configured.stderr)


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
