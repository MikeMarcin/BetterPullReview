#!/usr/bin/env python3
"""Closed-data tests for package selection and fail-closed evidence generation."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("package_runtime", ROOT / "packaging/package_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class PackageRuntimeTests(unittest.TestCase):
    def test_declared_sources_are_closed_and_pinned(self):
        items = runtime.components({"platform": "Darwin"})
        self.assertEqual({item["name"] for item in items}, {"QtBase", "KSyntaxHighlighting", "SQLite", "Quill", "Glaze", "LibArchive", "extra-cmake-modules"})
        for item in items:
            self.assertTrue(item["url"].startswith("https://"))
            self.assertRegex(item.get("sha256", item.get("sha3_256")), r"^[0-9a-f]{64}$")
        self.assertNotIn("LibArchive", {item["name"] for item in runtime.components({"platform": "Windows"})})

    def test_runtime_module_allowlist(self):
        for name in ("Qt6Core.dll", "libQt6Network.so.6", "libQt6XcbQpa.so.6.10.3"):
            self.assertTrue(runtime.qt_library(name))
        for name in ("Qt6Cored.dll", "Qt6WebEngineCore.dll", "libQt6Sql.so.6", "libQt6Qml.so.6", "Qt6Gui.dll.bak"):
            self.assertFalse(runtime.qt_library(name))

    def test_native_baseline_does_not_silently_rise(self):
        with patch.object(Path, "read_text", return_value='ID=ubuntu\nVERSION_ID="26.04"\n'):
            with self.assertRaisesRegex(ValueError, "not a supported"):
                runtime.linux_baseline()
        with patch.object(Path, "read_text", return_value='ID=rhel\nVERSION_ID="10.0"\n'), patch.object(runtime.platform, "machine", return_value="x86_64"):
            self.assertEqual(runtime.linux_baseline()["cpuFloor"], "x86-64-v3")

    def test_development_build_cannot_package(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "_bpr").mkdir()
            (build / "_bpr/package-inputs.json").write_text("{}")
            (build / "CMakeCache.txt").write_text("BUILD_TESTING:BOOL=ON\n")
            with self.assertRaisesRegex(ValueError, "BUILD_TESTING"):
                runtime.inputs(build)

    def test_unpinned_cached_source_is_refused_without_network(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "release-sources").mkdir()
            (build / "release-sources/Example-1.tar.gz").write_bytes(b"tampered")
            item = {"name": "Example", "version": "1", "url": "https://example.invalid/a.tar.gz", "sha256": "0" * 64}
            with patch.object(runtime, "components", return_value=[item]), patch.object(runtime.urllib.request, "urlopen") as network:
                with self.assertRaisesRegex(ValueError, "digest mismatch"):
                    runtime.source_archives(build, {})
                network.assert_not_called()

    def test_qt_runtime_graph_excludes_unshipped_build_tools(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sbom").mkdir()
            packages = [{"SPDXID": "p-" + name, "name": name} for name in ("Core", "Gui", "Widgets", "Network", "rcc", "pcre2")]
            files = [{"SPDXID": "f-" + name, "fileTypes": ["BINARY"], "fileName": "bin/Qt6" + name + ".dll"} for name in ("Core", "Gui", "Widgets", "Network", "rcc")]
            relations = [{"spdxElementId": "p-" + name, "relatedSpdxElement": "f-" + name, "relationshipType": "CONTAINS"} for name in ("Core", "Gui", "Widgets", "Network", "rcc")]
            relations.append({"spdxElementId": "p-Core", "relatedSpdxElement": "p-pcre2", "relationshipType": "DEPENDS_ON"})
            (root / "sbom/qtbase-6.10.3.spdx.json").write_text(json.dumps({"documentNamespace": "example", "packages": packages, "files": files, "relationships": relations}))
            stage = root / "stage"
            stage.mkdir()
            for name in ("Core", "Gui", "Widgets", "Network"):
                (stage / ("Qt6" + name + ".dll")).touch()
            selected = runtime.qt_records({"qt_root": str(root)}, stage)
            self.assertEqual({item["name"] for item in selected["packages"]}, {"Core", "Gui", "Widgets", "Network", "pcre2"})

    def test_installed_audit_needs_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, "corresponding source"):
                runtime.audit(root, {"platform": "Linux"}, root, root / "sbom.json")

    def test_linux_deployment_inspects_pinned_provider_before_staged_copies(self):
        with tempfile.TemporaryDirectory() as directory:
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
                    return "GLIBC_2.39 GLIBCXX_3.4.32"
                self.assertEqual(arguments[0], "patchelf")
                return ""

            with patch.object(runtime, "linux_baseline", return_value={"glibcFloor": "2.39"}), patch.object(runtime, "run", side_effect=inspect), patch.object(runtime, "notices"), patch.object(runtime, "audit"):
                runtime.deploy(build, {"platform": "Linux", "qt_root": str(qt)}, stage)
            closure = json.loads((stage / "runtime-closure.json").read_text())
            self.assertIn("libQt6Core.so.6", closure["files"])
            self.assertEqual(closure["baseline"]["requiredGLIBCXX"], ["3.4.32"])

    def test_linux_sonames_bind_versioned_qt_producer_records(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sbom").mkdir()
            names = ("Core", "Gui", "Widgets", "Network")
            packages = [{"SPDXID": "p-" + name, "name": name} for name in names]
            files = [{"SPDXID": "f-" + name, "fileTypes": ["BINARY"], "fileName": "lib/libQt6" + name + ".so.6.10.3"} for name in names]
            relations = [{"spdxElementId": "p-" + name, "relatedSpdxElement": "f-" + name, "relationshipType": "CONTAINS"} for name in names]
            (root / "sbom/qtbase-6.10.3.spdx.json").write_text(json.dumps({"documentNamespace": "example", "packages": packages, "files": files, "relationships": relations}))
            stage = root / "stage"
            stage.mkdir()
            for name in names:
                (stage / ("libQt6" + name + ".so.6")).touch()
            self.assertEqual(len(runtime.qt_records({"qt_root": str(root)}, stage)["packages"]), 4)

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
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ThirdParty").mkdir()
            (root / "ThirdParty/corresponding-source.zip").touch()
            upstream = {"upstreamNamespace": "https://example.invalid/qt-input", "files": [], "relationships": [], "hasExtractedLicensingInfos": [],
                        "packages": [{"SPDXID": "SPDXRef-QtCore", "name": "Core", "filesAnalyzed": True,
                                      "packageVerificationCode": {"packageVerificationCodeValue": "upstream-only"}, "licenseInfoFromFiles": ["MIT"]}]}
            with patch.object(runtime, "qt_records", return_value=upstream), patch.object(runtime, "components", return_value=[]), patch.object(runtime, "run", side_effect=["a" * 40, "", "2026-09-21T00:00:00+00:00"]):
                runtime.audit(root, {"platform": "Linux", "version": "1.0.0"}, root, root / "sbom.json")
            record = json.loads((root / "sbom.json").read_text())["packages"][1]
            self.assertFalse(record["filesAnalyzed"])
            self.assertNotIn("packageVerificationCode", record)
            self.assertNotIn("licenseInfoFromFiles", record)
            self.assertIn("Qt producer input", record["sourceInfo"])
            self.assertTrue(upstream["packages"][0]["filesAnalyzed"], "full input evidence remains intact")


if __name__ == "__main__":
    unittest.main()
