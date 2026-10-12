#!/usr/bin/env python3
"""Closed-data tests for package selection and fail-closed evidence generation."""
import importlib.util
import io
import json
from pathlib import Path
import sys
import tarfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
from skip_report import SkipReportingTestRunner

spec = importlib.util.spec_from_file_location("package_runtime", ROOT / "packaging/package_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class PackageRuntimeTests(unittest.TestCase):
    def test_windows_remembers_only_its_per_user_install_directory(self):
        config = (ROOT / "packaging/Install.cmake").read_text()
        self.assertIn('set(CPACK_WIX_INSTALL_SCOPE perUser)', config)
        self.assertIn('packaging/windows/InstallRoot.xml', config)
        tree = ET.parse(ROOT / "packaging/windows/InstallRoot.xml").getroot()
        product = tree.find("./CPackWiXFragment[@Id='#PRODUCT']")
        search = product.find("./Property[@Id='BPR_PREVIOUS_INSTALL_ROOT']/RegistrySearch")
        stored = tree.find("./CPackWiXFragment[@Id='CM_CP_better_pull_review.exe']/RegistryValue")
        for key in ("Root", "Key", "Name"):
            self.assertEqual(search.get(key), stored.get(key))
        self.assertEqual(search.get("Root"), "HKCU")
        self.assertEqual(stored.get("Value"), "[INSTALL_ROOT]")
        restore = product.find("./SetProperty[@Id='INSTALL_ROOT']")
        self.assertEqual(restore.get("Before"), "CostInitialize")
        self.assertEqual(restore.get("Sequence"), "both")
        self.assertEqual(restore.get("Condition"), "BPR_PREVIOUS_INSTALL_ROOT AND NOT INSTALL_ROOT")
        self.assertEqual(restore.get("Value"), "[BPR_PREVIOUS_INSTALL_ROOT]")

    def test_declared_sources_are_closed_and_pinned(self):
        items = runtime.components({"platform": "Darwin"})
        self.assertEqual({item["name"] for item in items}, {"QtBase", "KSyntaxHighlighting", "SQLite", "Quill", "Glaze", "JetBrains-Mono-NL", "LibArchive", "extra-cmake-modules"})
        for item in items:
            self.assertTrue(item["url"].startswith("https://"))
            self.assertRegex(item.get("sha256", item.get("sha3_256")), r"^[0-9a-f]{64}$")
        self.assertNotIn("LibArchive", {item["name"] for item in runtime.components({"platform": "Windows"})})

    def test_bundled_code_font_is_pinned_noticed_and_inventoried(self):
        # [[BPR-1270]]. One archive identity in the provider, the release
        # manifest and the provenance; every retained licence text in the
        # notices both the producer and the security check use.
        import hashlib, re
        item = next(entry for entry in runtime.components({"platform": "Linux"}) if entry["name"] == "JetBrains-Mono-NL")
        self.assertEqual(item["url"], "https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip")
        self.assertEqual((item["version"], item["license"]), ("2.304", "OFL-1.1"))
        provider = (ROOT / "cmake/BprCodeFontProvider.cmake").read_text(encoding="utf-8")
        self.assertIn(f'set(digest "{item["sha256"]}")', provider)
        self.assertIn('JetBrains/JetBrainsMono/releases/download/v${BPR_CODE_FONT_VERSION}/JetBrainsMono-${BPR_CODE_FONT_VERSION}.zip', provider)
        self.assertIn('set(BPR_CODE_FONT_VERSION "2.304")', provider)
        self.assertIn("BPR_DEPENDENCY_OFFLINE", provider)
        for system in ("Darwin", "Windows", "Linux"):
            self.assertIn(item, runtime.components({"platform": system}))
        # The plain font replaced the Nerd Fonts build: nothing of that remains.
        self.assertNotIn("nerd", (ROOT / "packaging/dependencies.json").read_text(encoding="utf-8").lower())
        self.assertFalse(any("nerd" in name.lower() for name in runtime.NOTICE_FILES))
        self.assertFalse((ROOT / "third_party/jetbrains-mono-nerd-font").exists())
        retained = ROOT / "third_party/jetbrains-mono"
        provenance = (retained / "PROVENANCE.md").read_text(encoding="utf-8")
        self.assertIn(item["sha256"], provenance)
        files = sorted(path.relative_to(ROOT / "third_party").as_posix() for path in retained.rglob("*")
                       if path.is_file() and path.name != "PROVENANCE.md")
        font_notices = sorted(name for name in runtime.NOTICE_FILES if name.startswith("jetbrains-mono/"))
        self.assertEqual(files, font_notices, "every retained font licence text is a notice, and no notice is missing")
        for name in font_notices:
            data = (ROOT / "third_party" / name).read_bytes()
            self.assertIn(hashlib.sha256(data).hexdigest(), provenance, name)
        for face in re.findall(r'"(JetBrainsMonoNL-[A-Za-z]+\.ttf)\|([0-9a-f]{64})"', provider):
            self.assertIn(f"| {face[0]} | ", provenance)
            self.assertIn(face[1], provenance)
        self.assertEqual(len(re.findall(r'"JetBrainsMonoNL-[A-Za-z]+\.ttf\|', provider)), 4)
        security = (ROOT / "scripts/verify_release_security.py").read_text(encoding="utf-8")
        self.assertIn("package.runtime.NOTICE_FILES", security)

    def test_notices_carry_every_retained_text_whole(self):
        with ContainedTemporaryDirectory() as directory:
            build, root = Path(directory) / "build", Path(directory) / "root"
            build.mkdir()
            root.mkdir()
            info = {"platform": "Linux", "version": "1.2.3"}
            provenance = runtime.source_provenance(info, "0" * 40, "1" * 64)
            records = {"packages": [], "files": [], "relationships": [], "upstreamNamespace": "x", "hasExtractedLicensingInfos": []}
            empty = io.BytesIO()
            with tarfile.open(fileobj=empty, mode="w"):
                pass
            archive = Path(directory) / "empty.tar"
            archive.write_bytes(empty.getvalue())
            with patch.object(runtime, "source_archives", return_value={"QtBase": archive, "KSyntaxHighlighting": archive}), \
                    patch.object(runtime, "prepare_source_asset", return_value=provenance), \
                    patch.object(runtime, "qt_records", return_value=records):
                runtime.notices(build, info, root)
            notices = (root / "ThirdParty/THIRD-PARTY-NOTICES.txt").read_text(encoding="utf-8")
        for name in runtime.NOTICE_FILES:
            self.assertIn((ROOT / "third_party" / name).read_text(encoding="utf-8"), notices, name)
        self.assertIn("SIL OPEN FONT LICENSE Version 1.1", notices)

    def test_windows_runtime_declares_its_resource_bundle(self):
        # [[BPR-0974]]. The bundle is installed before deployment, is not a PE
        # image to inspect, and its absence refuses the runtime.
        self.assertEqual(runtime.RUNTIME_DATA, {"Darwin": [], "Windows": ["better_pull_review.rcc"], "Linux": []})
        config = (ROOT / "packaging/Install.cmake").read_text()
        self.assertLess(config.index("${BPR_QT_RESOURCE_BUNDLE_NAME}"), config.index("install(SCRIPT"))
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            for system in ("Darwin", "Linux"):
                self.assertEqual(runtime.runtime_data(root, system), set())
            with self.assertRaisesRegex(ValueError, "declared runtime data missing: better_pull_review.rcc"):
                runtime.runtime_data(root, "Windows")
            (root / "better_pull_review.rcc").mkdir()
            with self.assertRaisesRegex(ValueError, "declared runtime data missing"):
                runtime.runtime_data(root, "Windows")
            (root / "better_pull_review.rcc").rmdir()
            (root / "better_pull_review.rcc").write_bytes(b"qres")
            self.assertEqual(runtime.runtime_data(root, "Windows"), {root / "better_pull_review.rcc"})

    def test_runtime_module_allowlist(self):
        for name in ("Qt6Core.dll", "libQt6Network.so.6", "libQt6XcbQpa.so.6.11.2"):
            self.assertTrue(runtime.qt_library(name))
        for name in ("Qt6Cored.dll", "Qt6WebEngineCore.dll", "libQt6Sql.so.6", "libQt6Qml.so.6", "Qt6Gui.dll.bak"):
            self.assertFalse(runtime.qt_library(name))

    def test_native_baseline_does_not_silently_rise(self):
        for identity in (('ubuntu', '24.04'), ('ubuntu', '26.10'), ('debian', '13')):
            with self.subTest(identity=identity), patch.object(Path, "read_text", return_value=f'ID={identity[0]}\nVERSION_ID="{identity[1]}"\n'):
                with self.assertRaisesRegex(ValueError, "not a supported"):
                    runtime.linux_baseline()
        with patch.object(Path, "read_text", return_value='ID=ubuntu\nVERSION_ID="26.04"\n'), patch.object(runtime.platform, "machine", return_value="x86_64"):
            self.assertEqual(runtime.linux_baseline(), {"distribution": ("ubuntu", "26.04"), "glibcFloor": "2.43", "cpuFloor": "x86-64"})
            self.assertEqual(runtime.linux_target(), "linux-ubuntu-26.04-x64")
            with patch.object(runtime.platform, "machine", return_value="aarch64"), self.assertRaisesRegex(ValueError, "require x86_64"):
                runtime.linux_baseline()
        with patch.object(Path, "read_text", return_value='ID=rhel\nVERSION_ID="10.0"\n'), patch.object(runtime.platform, "machine", return_value="x86_64"):
            self.assertEqual(runtime.linux_baseline()["cpuFloor"], "x86-64-v3")
            self.assertEqual(runtime.linux_baseline()["glibcFloor"], "2.39")

    def test_source_name_refuses_obsolete_ubuntu_target(self):
        info = {"platform": "Linux", "version": "1.2.3", "target": "linux-ubuntu-26.04-x64"}
        self.assertEqual(runtime.source_asset_name(info), "Source-BetterPullReview-1.2.3-linux-ubuntu-26.04-x64-corresponding-source.zip")
        info["target"] = "linux-ubuntu-24.04-x64"
        with self.assertRaisesRegex(ValueError, "invalid Linux release target"):
            runtime.source_asset_name(info)

    @unittest.skipUnless(sys.platform == "linux", "the Linux CPack definition requires its native producer; run on Linux")
    def test_linux_cpack_default_matches_native_baseline(self):
        # Configure the production install definition in a tiny native graph;
        # no product dependency acquisition, compilation or installation runs.
        with ContainedTemporaryDirectory() as directory:
            source, build = Path(directory) / "source", Path(directory) / "build"
            source.mkdir()
            (source / "fixture.cpp").write_text("int main() { return 0; }\n")
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.26)
project(package_baseline_fixture VERSION 1.2.3 LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Release)
set(BUILD_TESTING OFF)
set(BPR_BUILD_AGENT_INTEGRATIONS OFF)
set(BPR_BUILD_LOCAL_AUTOMATION OFF)
set(CMAKE_INSTALL_PREFIX /usr)
set(Qt6_DIR "{ROOT.as_posix()}/fixture-qt/lib/cmake/Qt6")
set(BPR_PRODUCT_QT_VERSION 6.11.2)
add_executable(app_better_pull_review fixture.cpp)
add_executable(tool_better_pull_review_agent_supervisor fixture.cpp)
set(PROJECT_SOURCE_DIR "{ROOT.as_posix()}")
include("{ROOT.as_posix()}/packaging/Install.cmake")
''')
            runtime.run("cmake", "-S", source, "-B", build)
            info = json.loads((build / "_bpr/package-inputs.json").read_text())
            self.assertEqual(info["target"], runtime.linux_target())
            expected = "DEB" if runtime.linux_baseline()["distribution"][0] == "ubuntu" else "RPM"
            self.assertIn(f'set(CPACK_GENERATOR "{expected}")', (build / "CPackConfig.cmake").read_text())

    def test_development_build_cannot_package(self):
        with ContainedTemporaryDirectory() as directory:
            build = Path(directory)
            (build / "_bpr").mkdir()
            (build / "_bpr/package-inputs.json").write_text("{}")
            (build / "CMakeCache.txt").write_text("BUILD_TESTING:BOOL=ON\n")
            with self.assertRaisesRegex(ValueError, "BUILD_TESTING"):
                runtime.inputs(build)

    def test_package_qt_version_matches_provider_and_corresponding_source(self):
        version = next(item["version"] for item in runtime.components({"platform": runtime.platform.system()}) if item["name"] == "QtBase")
        self.assertIn(f'set(BPR_QT_VERSION "{version}")', (ROOT / "cmake/BprQtProvider.cmake").read_text())
        with ContainedTemporaryDirectory() as directory:
            build = Path(directory)
            (build / "_bpr").mkdir()
            (build / "CMakeCache.txt").write_text("BUILD_TESTING:BOOL=OFF\nBPR_DISTRIBUTION_BUILD:BOOL=ON\nCMAKE_BUILD_TYPE:STRING=Release\n")
            info = {"schema": "bpr.package-inputs.v1", "source": str(ROOT), "platform": runtime.platform.system(), "qt_version": version}
            path = build / "_bpr/package-inputs.json"
            path.write_text(json.dumps(info))
            self.assertEqual(runtime.inputs(build)["qt_version"], version)
            info["qt_version"] = "6.10.3"
            path.write_text(json.dumps(info))
            with self.assertRaisesRegex(ValueError, "pinned Qt version"):
                runtime.inputs(build)

    def test_unpinned_cached_source_is_refused_without_network(self):
        with ContainedTemporaryDirectory() as directory:
            build = Path(directory)
            (build / "release-sources").mkdir()
            (build / "release-sources/Example-1.tar.gz").write_bytes(b"tampered")
            item = {"name": "Example", "version": "1", "url": "https://example.invalid/a.tar.gz", "sha256": "0" * 64}
            with patch.object(runtime, "components", return_value=[item]), patch.object(runtime.urllib.request, "urlopen") as network:
                with self.assertRaisesRegex(ValueError, "digest mismatch"):
                    runtime.source_archives(build, {})
                network.assert_not_called()

    def test_qt_runtime_graph_excludes_unshipped_build_tools(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sbom").mkdir()
            packages = [{"SPDXID": "p-" + name, "name": name} for name in ("Core", "Gui", "Widgets", "Network", "rcc", "pcre2")]
            files = [{"SPDXID": "f-" + name, "fileTypes": ["BINARY"], "fileName": "bin/Qt6" + name + ".dll"} for name in ("Core", "Gui", "Widgets", "Network", "rcc")]
            relations = [{"spdxElementId": "p-" + name, "relatedSpdxElement": "f-" + name, "relationshipType": "CONTAINS"} for name in ("Core", "Gui", "Widgets", "Network", "rcc")]
            relations.append({"spdxElementId": "p-Core", "relatedSpdxElement": "p-pcre2", "relationshipType": "DEPENDS_ON"})
            (root / "sbom/qtbase-6.11.2.spdx.json").write_text(json.dumps({"documentNamespace": "example", "packages": packages, "files": files, "relationships": relations}))
            stage = root / "stage"
            stage.mkdir()
            for name in ("Core", "Gui", "Widgets", "Network"):
                (stage / ("Qt6" + name + ".dll")).touch()
            selected = runtime.qt_records({"qt_root": str(root), "qt_version": "6.11.2"}, stage)
            self.assertEqual({item["name"] for item in selected["packages"]}, {"Core", "Gui", "Widgets", "Network", "pcre2"})

    def test_installed_audit_needs_source(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(runtime, "run", return_value="a" * 40), self.assertRaises(FileNotFoundError):
                runtime.audit(root, {"platform": "Linux"}, root, root / "sbom.json")

    def test_notices_keep_sources_outside_every_platform_runtime(self):
        for system in ("Darwin", "Windows", "Linux"):
            with self.subTest(system=system), ContainedTemporaryDirectory() as directory:
                build = Path(directory)
                stage = build / "runtime"
                stage.mkdir()
                cache = build / "release-sources"
                cache.mkdir()
                declared = []
                for name in ("QtBase", "KSyntaxHighlighting"):
                    path = cache / f"{name}-1.tar.gz"
                    with tarfile.open(path, "w:gz") as archive:
                        entry = tarfile.TarInfo(name + "/LICENSES/fixture.txt")
                        entry.size = 7
                        archive.addfile(entry, io.BytesIO(b"license"))
                    declared.append({"name": name, "version": "1", "url": "https://example.invalid/source.tar.gz", "sha256": runtime.digest(path)})
                (build / "_bpr").mkdir()
                (build / "_bpr/LibArchive-NOTICES.txt").write_text("LibArchive notice")
                msvc = build / "msvc.txt"
                msvc.write_text("Toolchain notice fixture")
                info = {"platform": system, "version": "1.2.3"}
                with (patch.object(runtime, "components", return_value=declared),
                      patch.object(runtime, "qt_records", return_value={"packages": [], "files": [], "hasExtractedLicensingInfos": []}),
                      patch.object(runtime, "run", return_value="a" * 40),
                      patch.dict(runtime.os.environ, {"BPR_MSVC_REDISTRIBUTION_NOTICE": str(msvc)}),
                      patch.object(runtime.urllib.request, "urlopen") as network):
                    runtime.notices(build, info, stage)
                    provenance = runtime.installed_source_provenance(stage, info, "a" * 40)
                    asset = runtime.source_asset_path(build, info, "a" * 40)
                    runtime.verify_source_asset(asset, provenance)
                    self.assertFalse(list(stage.rglob("*.zip")))
                    offers = (runtime.legal_root(stage, info) / "SOURCE-OFFERS.txt").read_text()
                    for required in ("ABI-compatible", "codesign --force --deep --sign -", "equivalent access", "Retention is indefinite"):
                        self.assertIn(required, offers)
                    # A new staging location and changed source-cache times must
                    # retain the exact separately distributable archive bytes.
                    for path in cache.iterdir():
                        runtime.os.utime(path, (1_500_000_000, 1_500_000_000))
                    runtime.notices(build, info, stage)
                    self.assertEqual(runtime.digest(asset), provenance["asset"]["sha256"])
                    network.assert_not_called()
                    with self.assertRaisesRegex(ValueError, "outside the installed runtime"):
                        runtime.notices(build, info, stage, stage / asset.name)
                    (stage / "corresponding-source.zip").touch()
                    with self.assertRaisesRegex(ValueError, "not embedded"):
                        runtime.installed_source_provenance(stage, info)

    @unittest.skipIf(runtime.os.name == "nt", "Linux ldd fixture requires POSIX paths; run on a Linux or macOS host")
    def test_linux_deployment_inspects_pinned_provider_before_staged_copies(self):
        with ContainedTemporaryDirectory() as directory:
            build = Path(directory)
            qt, stage = build / "qt", build / "stage"
            stage.mkdir()
            (stage / "better_pull_review").write_bytes(b"\x7fELF")
            for name in runtime.PLUGINS["Linux"]:
                path = qt / "plugins" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"\x7fELF")
            library = qt / "lib/libQt6Core.so.6"
            library.parent.mkdir()
            library.write_bytes(b"\x7fELF")

            def inspect(*arguments, env=None):
                if arguments[0] == "ldd":
                    self.assertEqual(env["LD_LIBRARY_PATH"].split(runtime.os.pathsep)[0], str(qt / "lib"))
                    return f" libQt6Core.so.6 => {library} (0x000000)\n"
                if arguments[:2] == ("readelf", "--file-header"):
                    return "Class: ELF64\nMachine: Advanced Micro Devices X86-64\n"
                if arguments[:2] == ("readelf", "--version-info"):
                    return versions
                self.assertEqual(arguments[0], "patchelf")
                return ""

            versions = "GLIBC_2.43 GLIBCXX_3.4.35"
            with patch.object(runtime, "linux_baseline", return_value={"glibcFloor": "2.43"}), patch.object(runtime, "run", side_effect=inspect), patch.object(runtime, "notices"), patch.object(runtime, "audit"):
                runtime.deploy(build, {"platform": "Linux", "qt_root": str(qt)}, stage)
            closure = json.loads((stage / "runtime-closure.json").read_text())
            self.assertIn("libQt6Core.so.6", closure["files"])
            self.assertEqual(closure["baseline"], {"glibcFloor": "2.43", "requiredGLIBCXX": ["3.4.35"]})
            for ceiling, versions in (("2.43", "GLIBC_2.44"), ("2.39", "GLIBC_2.43")):
                with self.subTest(ceiling=ceiling), patch.object(runtime, "linux_baseline", return_value={"glibcFloor": ceiling}), patch.object(runtime, "run", side_effect=inspect), patch.object(runtime, "notices"), patch.object(runtime, "audit"):
                    # Prior deployment output is data, not an ELF input.
                    for name in ("qt.conf", "runtime-closure.json"):
                        (stage / name).unlink(missing_ok=True)
                    with self.assertRaisesRegex(ValueError, f"glibc {ceiling} ceiling"):
                        runtime.deploy(build, {"platform": "Linux", "qt_root": str(qt)}, stage)

    def test_linux_sonames_bind_versioned_qt_producer_records(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sbom").mkdir()
            names = ("Core", "Gui", "Widgets", "Network")
            packages = [{"SPDXID": "p-" + name, "name": name} for name in names]
            files = [{"SPDXID": "f-" + name, "fileTypes": ["BINARY"], "fileName": "lib/libQt6" + name + ".so.6.11.2"} for name in names]
            relations = [{"spdxElementId": "p-" + name, "relatedSpdxElement": "f-" + name, "relationshipType": "CONTAINS"} for name in names]
            (root / "sbom/qtbase-6.11.2.spdx.json").write_text(json.dumps({"documentNamespace": "example", "packages": packages, "files": files, "relationships": relations}))
            stage = root / "stage"
            stage.mkdir()
            for name in names:
                (stage / ("libQt6" + name + ".so.6")).touch()
            self.assertEqual(len(runtime.qt_records({"qt_root": str(root), "qt_version": "6.11.2"}, stage)["packages"]), 4)

    def test_release_prefix_is_nonpublishing(self):
        script = (ROOT / "tools/release.sh").read_text()
        start = script.index('if [[ "${PREPARE_ONLY}" == "1" ]]; then')
        end = script.index('log "Signing nested code', start)
        prefix = script[start:end]
        self.assertIn("exit 0", prefix)
        self.assertNotIn("notarytool", prefix)
        self.assertNotIn("security ", prefix)
        self.assertNotIn('sign "${CODESIGN_IDENTITY}"', prefix)
        self.assertIn("--force --deep --sign - --timestamp=none", prefix)

    def test_final_sbom_does_not_reuse_upstream_file_verification_codes(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ThirdParty").mkdir()
            upstream = {"upstreamNamespace": "https://example.invalid/qt-input", "files": [], "relationships": [], "hasExtractedLicensingInfos": [],
                        "packages": [{"SPDXID": "SPDXRef-QtCore", "name": "Core", "filesAnalyzed": True,
                                      "packageVerificationCode": {"packageVerificationCodeValue": "upstream-only"}, "licenseInfoFromFiles": ["MIT"]}]}
            with (patch.object(runtime, "qt_records", return_value=upstream), patch.object(runtime, "components", return_value=[]),
                  patch.object(runtime, "installed_source_provenance"), patch.object(runtime, "verify_source_asset"),
                  patch.object(runtime, "run", side_effect=["a" * 40, "", "2026-09-21T00:00:00+00:00"])):
                runtime.audit(root, {"platform": "Linux", "version": "1.0.0"}, root, root / "sbom.json")
            record = json.loads((root / "sbom.json").read_text())["packages"][1]
            self.assertFalse(record["filesAnalyzed"])
            self.assertNotIn("packageVerificationCode", record)
            self.assertNotIn("licenseInfoFromFiles", record)
            self.assertIn("Qt producer input", record["sourceInfo"])
            self.assertTrue(upstream["packages"][0]["filesAnalyzed"], "full input evidence remains intact")


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
