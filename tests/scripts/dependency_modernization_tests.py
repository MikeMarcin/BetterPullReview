#!/usr/bin/env python3
from __future__ import annotations

import sys
import argparse
import json
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from skip_report import SkipReportingTestRunner  # noqa: E402

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


def cache_entry(build_directory: Path, name: str) -> str | None:
    """The value a configured tree's CMakeCache.txt holds for name, if any."""
    prefix = f"{name}:"
    cache = (build_directory / "CMakeCache.txt").read_text(encoding="utf-8")
    for line in cache.splitlines():
        if line.startswith(prefix):
            return line.partition("=")[2]
    return None


def copy_core_archives(cache: Path) -> None:
    """The Qt-free core graph needs SQLite, zlib ([[BPR-1435]]), utf8proc
    ([[BPR-1434]]) and glaze ([[BPR-1433]])."""
    cache.mkdir(parents=True, exist_ok=True)
    for archive in ("zlib-1.3.2.tar.gz", "utf8proc-2.12.0.tar.gz", "glaze-v8.3.0.tar.gz"):
        shutil.copy2(CPM_SOURCE_CACHE / archive, cache / archive)


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
        with contained_temporary_directory() as temporary_directory:
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
        # BPR-0337 replaced the package-only policy with a pinned provider, so
        # what this asserts is the policy that now exists: acquisition is
        # centralized in the provider, and turning it off demands a
        # caller-installed Qt rather than silently reaching for one.
        self.assertIn("BPR_QT_PROVIDER=OFF requires caller-installed Qt", provider)
        self.assertIn("bpr_acquire_qt(bpr_qt_prefix)", provider)
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
        with contained_temporary_directory() as temporary_directory:
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
        with contained_temporary_directory() as temporary_directory:
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
        with contained_temporary_directory() as temporary_directory:
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
                # Inside a quoted CMake string a Windows separator is an
                # escape (`\U`), so the marker is spelled with forward
                # slashes, which CMake accepts on every platform (BPR-0426).
                f"{marker.as_posix()}"
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
        with contained_temporary_directory() as temporary_directory:
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
        with contained_temporary_directory() as temporary_directory:
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
                "BprQuillProvider.cmake",
                "BprGlazeProvider.cmake",
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
                # CMake re-emits the module path into every try-compile
                # as a quoted `set()`, where a Windows separator is an
                # escape; forward slashes survive that on every platform
                # (BPR-0426).
                f"-DCMAKE_MODULE_PATH={fake_modules.as_posix()}",
            )
        self.assertEqual(
            configured.returncode,
            0,
            configured.stdout + configured.stderr,
        )
        self.assertFalse(marker.exists())

    def test_syntax_provider_forwards_list_values_whole(self) -> None:
        # The provider escapes a value's `;` and appends `-D<name>=<value>` to
        # an accumulator. Appending by expanding the accumulator unquoted
        # unescaped every earlier element, so a list such as
        # CMAKE_CONFIGURATION_TYPES survived its own round and was split on
        # the next; the nested configure then built one configuration and the
        # provider asked for four. Only a multi-configuration generator could
        # reach it, which is why it was first seen on Windows (BPR-0428).
        with contained_temporary_directory() as temporary_directory:
            script = Path(temporary_directory) / "forward.cmake"
            provider = ROOT / "cmake" / "BprSyntaxHighlightingProvider.cmake"
            script.write_text(
                "cmake_minimum_required(VERSION 3.25)\n"
                f"include([==[{provider}]==])\n"
                'set(CMAKE_CXX_FLAGS "/W4 /EHsc")\n'
                'set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo")\n'
                'set(CMAKE_PREFIX_PATH "first;second")\n'
                "set(forwarded)\n"
                "foreach(name IN ITEMS CMAKE_CXX_FLAGS CMAKE_CONFIGURATION_TYPES CMAKE_PREFIX_PATH)\n"
                '    bpr_syntax_forward(forwarded "${name}")\n'
                "endforeach()\n"
                "list(LENGTH forwarded count)\n"
                'message(STATUS "FORWARDED_COUNT=${count}")\n'
                "foreach(element IN LISTS forwarded)\n"
                '    message(STATUS "FORWARDED_ELEMENT=${element}")\n'
                "endforeach()\n",
                encoding="utf-8",
            )
            completed = subprocess.run(
                [str(CMAKE_COMMAND), "-P", str(script)],
                check=False,
                capture_output=True,
                text=True,
            )
        output = completed.stdout + completed.stderr
        self.assertEqual(completed.returncode, 0, output)
        elements = re.findall(r"FORWARDED_ELEMENT=(.*)", output)
        self.assertIn("FORWARDED_COUNT=3", output, output)
        self.assertEqual(
            elements,
            [
                "-DCMAKE_CXX_FLAGS=/W4 /EHsc",
                "-DCMAKE_CONFIGURATION_TYPES=Debug;Release;MinSizeRel;RelWithDebInfo",
                "-DCMAKE_PREFIX_PATH=first;second",
            ],
            output,
        )

    def test_syntax_provider_executes_forwarded_lists_whole(self) -> None:
        # The accumulator being whole is not enough: the helper that runs the
        # nested configure receives the command as function arguments, and
        # ARGN re-joins those with bare semicolons. This drives the real
        # helper with a forwarded list and reads the log it writes (BPR-0428).
        with contained_temporary_directory() as temporary_directory:
            temporary = Path(temporary_directory)
            script = temporary / "execute.cmake"
            log = temporary / "echo.log"
            provider = ROOT / "cmake" / "BprSyntaxHighlightingProvider.cmake"
            script.write_text(
                "cmake_minimum_required(VERSION 3.25)\n"
                f"include([==[{provider}]==])\n"
                'set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo")\n'
                "set(forwarded)\n"
                'bpr_syntax_forward(forwarded "CMAKE_CONFIGURATION_TYPES")\n'
                f'bpr_syntax_execute("echo" [==[{log}]==] "${{CMAKE_COMMAND}}" -E echo ${{forwarded}})\n',
                encoding="utf-8",
            )
            completed = subprocess.run(
                [str(CMAKE_COMMAND), "-P", str(script)],
                check=False,
                capture_output=True,
                text=True,
            )
            output = completed.stdout + completed.stderr
            self.assertEqual(completed.returncode, 0, output)
            echoed = log.read_text(encoding="utf-8").strip()
        self.assertEqual(
            echoed,
            "-DCMAKE_CONFIGURATION_TYPES=Debug;Release;MinSizeRel;RelWithDebInfo",
        )

    def test_syntax_build_inputs_use_parent_generator(self) -> None:
        package = cache_entry(BUILD_DIR, "KF6SyntaxHighlighting_DIR")
        if package is None or package.endswith("-NOTFOUND"):
            self.skipTest("this tree does not build KSyntaxHighlighting")
        version = cache_entry(BUILD_DIR, "CPM_PACKAGE_BprKSyntaxHighlighting_VERSION")
        self.assertIsNotNone(version)
        staged = BUILD_DIR / "_bpr" / "ksyntaxhighlighting" / str(version)
        parent_generator = cache_entry(BUILD_DIR, "CMAKE_GENERATOR")
        for name in ("extra-cmake-modules-build", "syntax-highlighting-build"):
            with self.subTest(project=name):
                nested = staged / name
                self.assertEqual(
                    cache_entry(nested, "CMAKE_GENERATOR"), parent_generator
                )
                for option in ("CMAKE_GENERATOR_PLATFORM", "CMAKE_GENERATOR_TOOLSET"):
                    self.assertEqual(
                        cache_entry(nested, option), cache_entry(BUILD_DIR, option)
                    )

    def test_syntax_prefix_installs_one_library_per_configuration(self) -> None:
        # A multi-configuration generator builds and installs KSyntaxHighlighting
        # once per configuration into one prefix. Without a per-configuration
        # postfix every install wrote the same library name, so the last one
        # won and a Debug application linked a release-runtime library
        # (BPR-0429). This reads the installed exports of the configured build
        # and requires each configuration to name its own library. On a
        # single-configuration tree there is one export and nothing to compare.
        #
        # The exports are read from the directory find_package used, as the
        # tree's cache records it. A Linux install puts them under
        # lib/<multiarch>/cmake, which a fixed prefix/lib/cmake glob missed, so
        # the case skipped as "not a multi-configuration tree" even on a
        # two-configuration tree (BPR-0871). Each skip says what it found.
        package = cache_entry(BUILD_DIR, "KF6SyntaxHighlighting_DIR")
        if package is None or package.endswith("-NOTFOUND"):
            recorded = (
                "no KF6SyntaxHighlighting_DIR"
                if package is None
                else f"KF6SyntaxHighlighting_DIR={package}"
            )
            self.skipTest(
                f"this tree does not build KSyntaxHighlighting: its cache has {recorded}"
            )
        package_directory = Path(package)
        exports = sorted(
            package_directory.glob("KF6SyntaxHighlightingTargets-*.cmake")
        )
        self.assertTrue(
            exports,
            f"no per-configuration export in {package_directory}, the directory"
            " the tree found KSyntaxHighlighting in",
        )
        locations: dict[str, str] = {}
        for export in exports:
            text = export.read_text(encoding="utf-8")
            match = re.search(
                r'IMPORTED_LOCATION_(?P<config>[A-Z]+) "(?P<location>[^"]+)"', text
            )
            self.assertIsNotNone(match, export)
            assert match is not None
            locations[match.group("config")] = match.group("location")
        if len(locations) < 2:
            self.skipTest(
                "one installed KSyntaxHighlighting configuration, "
                f"{', '.join(locations)}, in {package_directory}: a"
                " single-configuration tree has nothing to compare"
            )
        self.assertEqual(
            len(set(locations.values())),
            len(locations),
            f"configurations share an installed library: {locations!r}",
        )

    def test_quill_provider_is_pinned_and_download_only(self) -> None:
        provider = (ROOT / "cmake/BprQuillProvider.cmake").read_text(encoding="utf-8")
        self.assertIn("88b4a1542125577a4d51cf444c51e34d63618c422ba6a4fa9bd23894b49d696b", provider)
        self.assertIn("2061083", provider)
        self.assertIn("BPR_DEPENDENCY_OFFLINE", provider)
        self.assertIn("FETCHCONTENT_SOURCE_DIR_BPRQUILL", provider)
        self.assertIn("file(SHA256", provider)
        self.assertIn("SYSTEM INTERFACE", provider)
        self.assertNotIn("find_package(", provider)
        self.assertNotIn("add_subdirectory(", provider)
        sink = (ROOT / "src/adapters/logging/src/quill_log.cpp").read_text(encoding="utf-8")
        self.assertNotIn("stderr", sink)
        self.assertNotIn("std::cerr", sink)
        self.assertIn("BoundedDropping", sink)

    def test_configured_dependency_audit_uses_supported_providers(self) -> None:
        audit = BUILD_DIR / "_bpr/product-dependency-audit.cmake"
        self.assertTrue(audit.is_file(), audit)
        records = re.findall(r"\[==\[([^\n]+)\]==\]", audit.read_text(encoding="utf-8"))
        self.assertTrue(records)
        if any(record.startswith("LibArchive|") for record in records):
            self.assertIn("LibArchive|CPM_PINNED|BetterPullReview::ReleaseArchive", records)
        modes = {record.split("|", 2)[1] for record in records
                 if record != "LibArchive|CPM_PINNED|BetterPullReview::ReleaseArchive"}
        self.assertTrue(
            modes
            <= {"CMAKE_CONFIG", "CMAKE_MODULE", "PKG_CONFIG", "TOOLCHAIN", "PINNED_ARCHIVE"}
        )
        targets = {record.split("|", 2)[2] for record in records}
        self.assertIn("Threads::Threads", targets)
        if any(target.startswith("Qt6::") for target in targets):
            self.assertIn("Quill|PINNED_ARCHIVE|quill::quill", records)
            self.assertIn("Glaze|PINNED_ARCHIVE|glaze::glaze", records)
            self.assertTrue(
                {"Qt6::Core", "Qt6::Gui", "Qt6::Widgets", "Qt6::Network"}
                <= targets
            )

    def test_tests_disabled_core_graph_omits_test_and_qt_dependencies(self) -> None:
        with contained_temporary_directory() as temporary_directory:
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

    def test_qt_provider_off_requires_a_caller_installed_qt(self) -> None:
        """The `OFF` branch fails closed rather than finding a stray Qt.

        This used to be `test_qt_is_package_only_and_never_fetched`, and it
        asserted a policy BPR-0337 deliberately replaced: Qt *is* fetched now,
        by a pinned provider, and `BPR_QT_PROVIDER` defaults ON. The test kept
        the old name and the old expectation, configured without turning the
        provider off, and so asserted against a diagnostic only the other
        branch emits.
        """
        with contained_temporary_directory() as temporary_directory:
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=ON",
                "-DBUILD_TESTING=OFF",
                "-DBPR_QT_PROVIDER=OFF",
                "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON",
            )
        self.assertNotEqual(configured.returncode, 0)
        diagnostic = configured.stdout + configured.stderr
        self.assertIn("caller-installed Qt", diagnostic)
        self.assertIn("re-enable the pinned provider", diagnostic)

    def test_windows_qt_611_uses_the_pinned_upstream_repository_layout_fix(self):
        with contained_temporary_directory() as temporary_directory:
            script = Path(temporary_directory) / "installer.cmake"
            script.write_text(
                f'include("{ROOT.as_posix()}/cmake/BprQtProvider.cmake")\n'
                'bpr_qt_installer_requirement(requirement)\n'
                'message(STATUS "REQUIREMENT:${requirement}")\n'
                'set(BPR_QT_VERSION "6.10.3")\n'
                'bpr_qt_installer_requirement(legacy_requirement)\n'
                'message(STATUS "LEGACY:${legacy_requirement}")\n'
            )
            for system in ("Windows", "Darwin", "Linux"):
                with self.subTest(system=system):
                    result = subprocess.run(
                        ["cmake", f"-DCMAKE_SYSTEM_NAME={system}", "-P", str(script)],
                        text=True, capture_output=True, check=True,
                    )
                    expected = (
                        "git+https://github.com/miurahr/aqtinstall.git@"
                        "8c3695d4a4e1ceabf6a74dc6c79681656dc6b74b"
                        if system == "Windows" else "aqtinstall==3.3.0"
                    )
                    self.assertIn("REQUIREMENT:" + expected, result.stdout)
                    self.assertIn("LEGACY:aqtinstall==3.3.0", result.stdout)

    def test_qt_provider_keeps_every_mechanism_it_replaced_the_old_policy_with(
        self,
    ) -> None:
        """The five properties that make fetching Qt as strict as refusing to.

        BPR-0337 replaced "Qt is never fetched automatically" with a provider,
        and BPR-0373 records why re-pointing the assertions at whatever the file
        now says is not enough: a contract edited to agree with the build is not
        a contract. The old policy's rigour came from being absolute. This one's
        has to come from naming each mechanism, so the contract fails if the
        provider stops pinning, stops locking, stops refusing offline, or starts
        accepting an arbitrary Qt.

        Each assertion below is paired with the failure it exists to catch.
        """
        raw_provider = (ROOT / "cmake/BprQtProvider.cmake").read_text(
            encoding="utf-8"
        )
        # Comments stripped before anything is asserted. Every `assertIn` below
        # would otherwise be satisfiable by a `#` line naming the thing it asks
        # for, which is how a contract comes to describe a build it no longer
        # constrains. The two ordering checks depend on this too: `str.find`
        # returns the first occurrence, so a comment mentioning
        # `if(BPR_DEPENDENCY_OFFLINE)` above the real gate would let the gate
        # move below the download while the index comparison still held.
        provider = "\n".join(
            re.sub(r"#.*$", "", line) for line in raw_provider.splitlines()
        )

        # Unpinned, a range, or an environment default would all let two hosts
        # build against different Qt.
        self.assertRegex(
            provider, r"(?m)^set\(BPR_QT_VERSION \"\d+\.\d+\.\d+\"\)$"
        )
        # And the only one. A second `set(BPR_QT_VERSION …)` guarded by
        # `if(DEFINED ENV{…})` leaves the literal line intact and still lets two
        # hosts build against different Qt, which is the failure the line above
        # exists to catch rather than merely to describe.
        self.assertEqual(
            len(re.findall(r"set\(\s*BPR_QT_VERSION\b", provider)),
            1,
            "BPR_QT_VERSION is assigned more than once",
        )

        # Not namespaced by version, a cache populated under one pin would be
        # accepted for the next one.
        self.assertIn(
            'set(prefix_root "${CPM_SOURCE_CACHE}/bprqt/${BPR_QT_VERSION}")',
            provider,
        )

        # Without the lock, two concurrent configures race to write and validate
        # the same prefix; without the fail-closed result check, a timeout would
        # proceed as though it held it.
        self.assertRegex(
            provider,
            r"file\(\s*LOCK \"\$\{CPM_SOURCE_CACHE\}/bprqt-\$\{BPR_QT_VERSION\}\.lock\""
            r"\s+GUARD FUNCTION\s+TIMEOUT \d+\s+RESULT_VARIABLE lock_result\s*\)",
        )
        self.assertRegex(
            provider,
            r"if\(lock_result\)\s*\n\s*message\(\s*\n?\s*FATAL_ERROR",
        )

        # The offline gate has to sit strictly before the download, or offline
        # mode silently reaches the network.
        offline_gate = provider.find("if(BPR_DEPENDENCY_OFFLINE)")
        download_call = provider.find('bpr_download_qt("${qt_host}"')
        self.assertNotEqual(offline_gate, -1)
        self.assertNotEqual(download_call, -1)
        self.assertLess(offline_gate, download_call)
        self.assertIn("BPR_DEPENDENCY_OFFLINE=ON requires Qt", provider)

        # Validation is presence-of-packages *plus* an exact version match. File
        # existence alone would accept an interrupted download or a stale pin,
        # and the version has to be read from the Impl file because the
        # forwarding file never carries the literal.
        # Read off the validator's own `foreach` line as whole words. A plain
        # `assertIn` over the file would pass with the list reduced to one
        # package, because `Qt6` is a substring of `Qt6Core` and `Qt6Core` of
        # `Qt6CoreTools`.
        # [[BPR-1243]]. d62902a86 builds the non-GUI tour runtime without Qt Gui
        # and Widgets, so the list is the always-required packages plus the GUI
        # pair a Qt build appends, and the loop walks that list.
        base = re.search(r"set\(bpr_qt_packages ([^)]*)\)", provider)
        gui = re.search(
            r"if\(BPR_BUILD_QT\)\s*\n\s*list\(APPEND bpr_qt_packages ([^)]*)\)",
            provider,
        )
        self.assertIsNotNone(base, "the validator's package list is gone")
        self.assertIsNotNone(gui, "a Qt build no longer validates Gui and Widgets")
        self.assertRegex(provider, r"foreach\(package IN LISTS bpr_qt_packages\)")
        self.assertEqual(
            set((base.group(1) if base else "").split()),
            {"Qt6", "Qt6Core", "Qt6Network", "Qt6CoreTools"},
        )
        self.assertEqual(
            set((gui.group(1) if gui else "").split()),
            {"Qt6Gui", "Qt6Widgets"},
        )
        self.assertIn("Qt6ConfigVersionImpl.cmake", provider)
        self.assertIn(
            r'REGEX "^set\\(PACKAGE_VERSION \"${BPR_QT_VERSION}\"\\)$"',
            provider,
        )

        # And the validator has to be what every acquisition arm consults, not a
        # function one of them calls. Three arms reach a prefix: the cache hit,
        # a caller-supplied `BPR_QT_ROOT`, and each download attempt. Binding
        # only the first would let `-DBPR_QT_ROOT=/any/dir` bypass the pin and
        # the package checks entirely, and would let the retry loop accept an
        # empty prefix on a zero exit -- the exact intermittent failure the
        # provider's own comment says exit status cannot rule out.
        for arm, guard in (
            ('bpr_validate_qt_prefix("${prefix}" cached_is_valid)', "if(NOT cached_is_valid)"),
            ('bpr_validate_qt_prefix("${BPR_QT_ROOT}" supplied_is_valid)', "if(NOT supplied_is_valid)"),
            ('bpr_validate_qt_prefix("${prefix}" attempt_is_valid)', "if(attempt_is_valid)"),
        ):
            self.assertIn(arm, provider)
            self.assertIn(guard, provider)

        # The lock has to be held before the prefix is read or written, not
        # merely to exist somewhere in the file.
        lock_call = provider.find("LOCK \"${CPM_SOURCE_CACHE}/bprqt-")
        self.assertNotEqual(lock_call, -1)
        self.assertLess(lock_call, download_call)
        self.assertLess(
            lock_call,
            provider.find('bpr_validate_qt_prefix("${prefix}" cached_is_valid)'),
        )

    def test_qt_provider_is_the_default_acquisition_path(self) -> None:
        """The other branch, which nothing covered.

        A test that only exercises `OFF` would pass with the provider removed
        entirely, which is the half that actually ships.
        """
        provider = (ROOT / "cmake/BprQtProvider.cmake").read_text(
            encoding="utf-8"
        )
        self.assertRegex(
            provider,
            r"option\(\s*BPR_QT_PROVIDER\s+\"[^\"]*\"\s+ON\s*\)",
        )
        with contained_temporary_directory() as temporary_directory:
            stale_qt = Path(temporary_directory) / "stale-qt"
            stale_qt.mkdir()
            marker = stale_qt / "executed"
            (stale_qt / "Qt6Config.cmake").write_text(
                f"file(WRITE [==[{marker}]==] stale)\n"
                "message(FATAL_ERROR \"Cached Qt from the previous pin was selected\")\n",
                encoding="utf-8",
            )
            (stale_qt / "Qt6ConfigVersion.cmake").write_text(
                "set(PACKAGE_VERSION 6.10.3)\n"
                "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n",
                encoding="utf-8",
            )
            configured = self.configure(
                temporary_directory,
                "-DBPR_BUILD_QT=ON",
                "-DBUILD_TESTING=OFF",
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DQt6_DIR={stale_qt}",
            )
            self.assertEqual(
                configured.returncode,
                0,
                configured.stdout + configured.stderr,
            )
            self.assertFalse(marker.exists(), "A stale Qt package must not execute before selecting the pin")
            audit = (
                Path(temporary_directory)
                / "build/_bpr/product-dependency-audit.cmake"
            ).read_text(encoding="utf-8")
            for component in ("Core", "Gui", "Widgets", "Network"):
                self.assertIn(f"Qt6|CMAKE_CONFIG|Qt6::{component}", audit)

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
                with contained_temporary_directory() as temporary_directory:
                    temporary = Path(temporary_directory)
                    cache = temporary / "offline cache"
                    shutil.copytree(CPM_SOURCE_CACHE / "cpm", cache / "cpm")
                    copy_core_archives(cache)
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
        with contained_temporary_directory() as temporary_directory:
            temporary = Path(temporary_directory)
            cache = temporary / "poisoned cache"
            shutil.copytree(CPM_SOURCE_CACHE / "cpm", cache / "cpm")
            copy_core_archives(cache)
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
            # MSVC's line spells the staged include with backslashes and
            # `-external:I`; the property is the directory on the include
            # path, not the separator, so both are compared with one spelling
            # (BPR-0426).
            command = sqlite_commands[0]["command"].replace("\\", "/")
            self.assertIn("_bpr/sqlite/3.53.4", command)
            self.assertNotIn((cache / "bprsqlite").as_posix(), command)

    def test_sqlite_staging_rejects_unexpected_generated_entries(self) -> None:
        with contained_temporary_directory() as temporary_directory:
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
                with contained_temporary_directory() as temporary_directory:
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
        with contained_temporary_directory() as temporary_directory:
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
            # A local path, not a `file://` URI: ExternalProject strips the
            # scheme and keeps the leading slash, which on Windows leaves
            # `/C:/...` and "File not found" instead of the hash check this
            # test exists for (BPR-0426).
            provider = provider.replace(
                "https://sqlite.org/2026/sqlite-amalgamation-"
                "${BPR_SQLITE_SOURCE_NUMBER}.zip",
                corrupt_archive.as_posix(),
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
        with contained_temporary_directory() as temporary_directory:
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
        with contained_temporary_directory() as temporary_directory:
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
    unittest.main(argv=[__file__, *remaining], testRunner=SkipReportingTestRunner)
