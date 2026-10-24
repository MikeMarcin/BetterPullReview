#!/usr/bin/env python3
"""Hostile controls for exact package identity, graph and byte assertions."""
from __future__ import annotations

import json
from pathlib import Path
import plistlib
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import verify_release_package as audit


class ReleasePackageTests(unittest.TestCase):
    def test_linux_qt_dynamic_symbols_require_exact_producer_and_relocation(self):
        with ContainedTemporaryDirectory() as directory:
            base = Path(directory).resolve()
            root, qt = base / "runtime", base / "qt"
            root.mkdir()
            (qt / "lib").mkdir(parents=True)
            producer = qt / "lib/libQt6Core.so.6"
            binary = root / producer.name
            producer.write_bytes(b"exact prebuilt fixture")
            binary.write_bytes(producer.read_bytes())
            info = {"platform": "Linux", "qt_root": str(qt)}
            record = {"SPDXID": "SPDXRef-controlled-producer", "fileName": "./lib/" + producer.name,
                      "fileTypes": ["BINARY"], "checksums": [{"algorithm": "SHA1", "checksumValue": audit.runtime.digest(producer, "sha1")}]}
            # The production audit runs outside an AppContainer. Give this
            # contained fixture's internal relocation root the same access as
            # its surrounding fixture; all producer/relocation checks still run.
            with (patch.object(audit.tempfile, "TemporaryDirectory", ContainedTemporaryDirectory),
                  patch.object(audit.runtime, "qt_upstream_records", return_value={"files": [record]}),
                  patch.object(audit.runtime, "run"),
                  patch.object(audit.subprocess, "check_output", return_value="0000000000001234 T actual_function\n0000000000002000 R actual_data\n") as command):
                table, binding = audit.linux_qt_symbol_evidence(binary, root, info)
                self.assertEqual(set(table.symbols), {"actual_function", "actual_data"})
                self.assertEqual(binding["defined_dynamic_symbols"], 2)
                command.assert_called_once_with(["nm", "-D", "--demangle", str(binary)], text=True)
                for text in ("", "0000000000000000 A Qt_6\n", "                 U actual_function\n", "malformed dynamic record\n"):
                    with self.subTest(dynamic=text), patch.object(audit.subprocess, "check_output", return_value=text):
                        with self.assertRaises(audit.composition.verification_error):
                            audit.linux_qt_symbol_evidence(binary, root, info)
                binary.write_bytes(b"changed delivered image")
                with self.assertRaisesRegex(ValueError, "relocated-digest"):
                    audit.linux_qt_symbol_evidence(binary, root, info)
                binary.write_bytes(producer.read_bytes())
                producer.write_bytes(b"changed producer")
                with self.assertRaisesRegex(ValueError, "producer-digest"):
                    audit.linux_qt_symbol_evidence(binary, root, info)
                producer.write_bytes(binary.read_bytes())
                with patch.object(audit.runtime, "qt_upstream_records", return_value={"files": []}):
                    with self.assertRaisesRegex(ValueError, "producer-record"):
                        audit.linux_qt_symbol_evidence(binary, root, info)
                outside = base / "outside"
                producer.unlink()
                if sys.platform == "win32":
                    # AppContainers cannot acquire the symlink privilege. A
                    # real directory junction exercises the same resolved
                    # producer escape without changing the contained token.
                    import _winapi
                    outside.mkdir()
                    (outside / producer.name).write_bytes(binary.read_bytes())
                    producer.parent.rmdir()
                    _winapi.CreateJunction(str(outside), str(producer.parent))
                    target = outside / producer.name
                    try:
                        self.assertEqual(producer.resolve(strict=True), target)
                    except OSError as error:
                        # The restricted token can reject traversal itself.
                        self.assertEqual(error.winerror, 448)  # untrusted mount point
                    original_resolve = Path.resolve
                    def resolved_observation(path, *args, **kwargs):
                        return target if path == producer else original_resolve(path, *args, **kwargs)
                    # Exercise the verifier's exact origin predicate as well
                    # when the OS refuses to supply that resolved observation.
                    with patch.object(Path, "resolve", autospec=True, side_effect=resolved_observation):
                        with self.assertRaisesRegex(ValueError, "producer-origin"):
                            audit.linux_qt_symbol_evidence(binary, root, info)
                else:
                    outside.write_bytes(binary.read_bytes())
                    producer.symlink_to(outside)
                    with self.assertRaisesRegex(ValueError, "producer-origin"):
                        audit.linux_qt_symbol_evidence(binary, root, info)

    def test_first_party_source_built_and_unowned_images_never_get_dynamic_shortcut(self):
        root = Path("/runtime")
        with patch.object(audit.runtime, "qt_upstream_records") as producer, patch.object(audit.subprocess, "check_output") as dynamic:
            for name in ("better_pull_review", "better_pull_review_agent_supervisor", "libbpr_xdiff.so",
                         "libicuuc.so.73", "libQt6Unknown.so.6", "unknown.so", "plugins/unowned/libqxcb.so"):
                self.assertIsNone(audit.linux_qt_symbol_evidence(root / name, root, {"platform": "Linux"}))
            self.assertIsNone(audit.linux_qt_symbol_evidence(root / "libQt6Core.so.6", root, {"platform": "Darwin"}))
            producer.assert_not_called()
            dynamic.assert_not_called()
        # The retained ordinary inspector still refuses a stripped first-party
        # image; it never asks for a dynamic-only table after this failure.
        with (patch.object(audit.composition.shutil, "which", side_effect=lambda name: "/usr/bin/" + name if name in {"nm", "c++filt"} else None),
              patch.object(audit.composition.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "no symbols"))):
            with self.assertRaises(audit.composition.verification_error):
                audit.composition.inspect_symbols(root / "better_pull_review")

    def test_package_layout_requires_each_helper_and_only_declared_plugins(self):
        for system in ("Darwin", "Windows", "Linux"):
            with self.subTest(system=system), ContainedTemporaryDirectory() as directory:
                root = Path(directory).resolve()
                info = {"platform": system, "version": "1.2.3"}
                binaries = root / "Contents/MacOS" if system == "Darwin" else root
                binaries.mkdir(parents=True, exist_ok=True)
                suffix = ".exe" if system == "Windows" else ""
                app = binaries / ("BetterPullReview" if system == "Darwin" else "better_pull_review" + suffix)
                helpers = [binaries / (name + suffix) for name in ("better_pull_review_agent_supervisor", "better_pull_review_mcp_context_helper")]
                if system == "Darwin":
                    helpers.append(binaries / "bpr_update_helper")
                    identity = root / "Contents/Info.plist"
                    identity.write_bytes(plistlib.dumps({"CFBundleIdentifier": "com.gamecoretech.betterpullreview", "CFBundleExecutable": app.name, "CFBundleShortVersionString": "1.2.3"}))
                for path in (app, *helpers):
                    path.touch()
                # [[BPR-0974]]. The Windows image's resource bundle, and nothing
                # declared on the other platforms.
                data = [binaries / name for name in audit.runtime.RUNTIME_DATA[system]]
                self.assertEqual([path.name for path in data], ["better_pull_review.rcc"] if system == "Windows" else [])
                for path in data:
                    path.touch()
                # [[BPR-1485]]. Git's xdiff, its own shared library where the
                # application loads it.
                libraries = audit.private_libraries(root, info)
                self.assertEqual([path.relative_to(root).as_posix() for path in libraries],
                                 {"Darwin": ["Contents/Frameworks/libbpr_xdiff.dylib", "Contents/Frameworks/libmimalloc.3.dylib"], "Windows": ["bpr_xdiff.dll", "mimalloc.dll", "mimalloc-redirect.dll"],
                                  "Linux": ["libbpr_xdiff.so"]}[system])
                for path in libraries:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                plugins = root / "Contents/PlugIns" if system == "Darwin" else root / "plugins"
                for name in audit.runtime.PLUGINS[system]:
                    path = plugins / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                self.assertEqual(audit.package_paths(root, info), (app, helpers))
                for required in (*helpers, *data, *libraries):
                    required.unlink()
                    with self.assertRaisesRegex(ValueError, "capability-missing:" + required.name):
                        audit.package_paths(root, info)
                    required.touch()
                (plugins / "untrusted-plugin").touch()
                with self.assertRaisesRegex(ValueError, "plugin-set-mismatch"):
                    audit.package_paths(root, info)
                if system == "Darwin":
                    identity.write_bytes(plistlib.dumps({"CFBundleIdentifier": "com.example.development"}))
                    with self.assertRaisesRegex(ValueError, "invalid-package-identity"):
                        audit.package_paths(root, info)

    def test_agent_free_package_omits_the_mcp_helper_and_says_so(self):
        """[[BPR-1179]]. The helper set and the product record follow the families built."""
        free = {"agent_integrations": False, "local_automation": True}
        for system in ("Darwin", "Windows", "Linux"):
            with self.subTest(system=system), ContainedTemporaryDirectory() as directory:
                root = Path(directory).resolve()
                info = {"platform": system, "version": "1.2.3", "families": free}
                binaries = root / "Contents/MacOS" if system == "Darwin" else root
                binaries.mkdir(parents=True, exist_ok=True)
                suffix = ".exe" if system == "Windows" else ""
                app = binaries / ("BetterPullReview" if system == "Darwin" else "better_pull_review" + suffix)
                helpers = [binaries / ("better_pull_review_agent_supervisor" + suffix)]
                if system == "Darwin":
                    helpers.append(binaries / "bpr_update_helper")
                    (root / "Contents/Info.plist").write_bytes(plistlib.dumps({"CFBundleIdentifier": "com.gamecoretech.betterpullreview", "CFBundleExecutable": app.name, "CFBundleShortVersionString": "1.2.3"}))
                for path in (app, *helpers, *(binaries / name for name in audit.runtime.RUNTIME_DATA[system]),
                             *audit.private_libraries(root, info)):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()
                plugins = root / "Contents/PlugIns" if system == "Darwin" else root / "plugins"
                for name in audit.runtime.PLUGINS[system]:
                    (plugins / name).parent.mkdir(parents=True, exist_ok=True)
                    (plugins / name).touch()
                self.assertEqual(audit.package_paths(root, info), (app, helpers))
                # The same layout is incomplete for a full package...
                with self.assertRaisesRegex(ValueError, "capability-missing:better_pull_review_mcp_context_helper"):
                    audit.package_paths(root, {"platform": system, "version": "1.2.3"})
                # ...and an agent-free package that still carries the helper is refused.
                (binaries / ("better_pull_review_mcp_context_helper" + suffix)).touch()
                with self.assertRaisesRegex(ValueError, "omitted-family-file-present:better_pull_review_mcp_context_helper"):
                    audit.package_paths(root, info)
        revision = "c" * 40
        full = audit.runtime.product_source_info({"platform": "Linux"}, revision, True)
        self.assertEqual(full, f"git:{revision}; clean=True; tests-disabled Release; Linux")
        self.assertEqual(audit.runtime.product_source_info({"platform": "Linux", "families": free}, revision, True), full + "; agent integrations absent")
        neither = {"agent_integrations": False, "local_automation": False}
        self.assertEqual(audit.runtime.product_source_info({"platform": "Linux", "families": neither}, revision, True),
                         full + "; agent integrations absent; local automation absent")
        for hostile in ({"agent_integrations": False}, {"agent_integrations": "no", "local_automation": True}, {**free, "telemetry": True}):
            with self.assertRaisesRegex(ValueError, "unknown component family"):
                audit.runtime.families({"families": hostile})

    def test_inventory_detects_revision_file_hash_omission_and_path_escape(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory).resolve()
            legal = root / "ThirdParty"
            legal.mkdir()
            source = legal / "SOURCE-OFFERS.txt"
            source.write_bytes(b"controlled source fixture")
            sbom = root / "sbom.spdx.json"
            revision = "a" * 40
            record = {"packages": [{"SPDXID": "SPDXRef-BetterPullReview", "versionInfo": "1", "sourceInfo": f"git:{revision}; clean=True; tests-disabled Release; Linux"}],
                      "files": [{"fileName": "./ThirdParty/SOURCE-OFFERS.txt", "checksums": [{"algorithm": "SHA256", "checksumValue": audit.runtime.digest(source)}]}]}
            provenance = patch.object(audit.runtime, "installed_source_provenance")
            provenance.start()
            self.addCleanup(provenance.stop)
            def write():
                sbom.write_text(json.dumps(record))
            write()
            check = lambda: audit.check_file_inventory(root, sbom, revision, {"platform": "Linux", "version": "1"})
            self.assertEqual(check()["file_count"], 1)
            with ContainedTemporaryDirectory() as external_directory:
                external = Path(external_directory) / "delivered.spdx.json"
                external.write_bytes(sbom.read_bytes())
                self.assertEqual(audit.check_file_inventory(root, external, revision, {"platform": "Linux", "version": "1"})["file_count"], 1)
                external.write_text("{}")
                with self.assertRaisesRegex(ValueError, "installed-inventory-mismatch"):
                    audit.check_file_inventory(root, external, revision, {"platform": "Linux", "version": "1"})
            source.write_bytes(b"substituted")
            with self.assertRaisesRegex(ValueError, "file-digest-mismatch"):
                check()
            source.write_bytes(b"controlled source fixture")
            for name in ("../outside", "./../outside", "./a/../b", "/absolute", "./a\\b", "./a//b"):
                record["files"][0]["fileName"] = name
                write()
                with self.assertRaisesRegex(ValueError, "invalid-inventory-path"):
                    check()
            record["files"] = []
            write()
            with self.assertRaisesRegex(ValueError, "file-set-mismatch"):
                check()
            # [[BPR-1179]]. A full package's record does not pass as an agent-free one.
            free = {"platform": "Linux", "version": "1", "families": {"agent_integrations": False, "local_automation": True}}
            record["files"] = [{"fileName": "./ThirdParty/SOURCE-OFFERS.txt", "checksums": [{"algorithm": "SHA256", "checksumValue": audit.runtime.digest(source)}]}]
            write()
            with self.assertRaisesRegex(ValueError, "source-identity-mismatch"):
                audit.check_file_inventory(root, sbom, revision, free)
            record["packages"][0]["sourceInfo"] += "; agent integrations absent"
            write()
            self.assertEqual(audit.check_file_inventory(root, sbom, revision, free)["file_count"], 1)
            record["files"] = []
            record["packages"][0]["sourceInfo"] = "git:" + "b" * 40
            write()
            with self.assertRaisesRegex(ValueError, "source-identity-mismatch"):
                check()

    def test_byte_symbol_controls_fail_without_printing_canaries(self):
        audit.check_payload(b"production fixture-free content", ("main",))
        for payload in (b"--test-application-data-root", b"BPR_CANARY_SOURCE_private", b"BPR_CANARY_SECRET_private", b"ghp_12345678901234567890"):
            with self.subTest(payload=payload):
                with self.assertRaises(ValueError) as failure:
                    audit.check_payload(payload, ("main",))
                self.assertNotIn(payload.decode(), str(failure.exception))
        with self.assertRaisesRegex(ValueError, "empty-symbol"):
            audit.check_payload(b"", ())
        with self.assertRaisesRegex(ValueError, "fixture-symbol"):
            audit.check_payload(b"production", ("bpr::app::detail::github_application_fixture_access::start",))
        # [[BPR-0974]]. Declared runtime data gets the same byte controls
        # without a symbol table.
        audit.check_data_payload(b"qres production resources")
        with self.assertRaisesRegex(ValueError, "fixture-or-canary-bytes"):
            audit.check_data_payload(b"qres BPR_CANARY_SECRET_private")

    def test_static_dependency_omission_and_wrong_provenance_fail(self):
        with ContainedTemporaryDirectory() as directory:
            sbom = Path(directory) / "sbom.json"
            dependency = {"name": "StaticLibrary", "version": "1", "url": "https://example.invalid/source", "sha256": "a" * 64}
            record = {"SPDXID": "SPDXRef-StaticLibrary", "versionInfo": "1", "downloadLocation": dependency["url"],
                      "checksums": [{"algorithm": "SHA256", "checksumValue": "a" * 64}]}
            with patch.object(audit.runtime, "components", return_value=[dependency]):
                sbom.write_text(json.dumps({"packages": [record]}))
                audit.check_dependency_inventory(sbom, {})
                for records in ([], [record, record], [{**record, "versionInfo": "2"}], [{**record, "checksums": []}]):
                    sbom.write_text(json.dumps({"packages": records}))
                    with self.assertRaisesRegex(ValueError, "dependency"):
                        audit.check_dependency_inventory(sbom, {})

    def test_graph_requires_real_regions_and_excludes_testing_and_core_effects(self):
        with ContainedTemporaryDirectory() as directory:
            build = Path(directory)
            commands = [{"file": str(ROOT / "src" / region / "source.cpp"), "command": "c++ -O3"} for region in ("core", "application", "adapters", "app")]
            commands.append({"file": str(ROOT / "src/app/src/installed_runtime.cpp"), "command": "c++ -DBPR_DISTRIBUTION_BUILD=1"})
            def check():
                (build / "compile_commands.json").write_text(json.dumps(commands))
                return audit.check_graph(build, {"platform": "Linux"}, "a" * 40)
            with patch.object(audit.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)):
                self.assertEqual(check()["first_party_compile_units"], 5)
                for command in ("c++ -DBPR_BUILD_TESTING=1", "cl /DBPR_GITHUB_TESTING=1", "c++ -I/QtCore"):
                    commands[0]["command"] = command
                    with self.assertRaisesRegex(ValueError, "fixture-definition|effect-dependency"):
                        check()
                commands[0]["command"] = "c++"
                commands[0]["file"] = str(ROOT / "tests/fixture.cpp")
                with self.assertRaisesRegex(ValueError, "tests-present"):
                    check()
                commands.clear()
                with self.assertRaisesRegex(ValueError, "incomplete-package-graph"):
                    check()

    def test_dependency_check_rejects_external_origin_even_without_browser_tokens(self):
        root = Path("/candidate/BetterPullReview.app")
        binary = root / "Contents/MacOS/BetterPullReview"
        with patch.object(audit.dependencies, "dependency_output", return_value="app:\n /untrusted/libUseful.dylib (compatibility version 1.0.0)\n"):
            with self.assertRaisesRegex(ValueError, "untrusted-library-origin"):
                audit.check_dependencies(binary, root, "Darwin")
        with (patch.object(audit.dependencies, "dependency_output", return_value="app:\n /usr/lib/libSystem.B.dylib (compatibility version 1.0.0)\n"),
              patch.object(audit.subprocess, "check_output", return_value="cmd LC_RPATH\n cmdsize 48\n path /tmp/untrusted (offset 12)\n")):
            with self.assertRaisesRegex(ValueError, "untrusted-library-search-path"):
                audit.check_dependencies(binary, root, "Darwin")


if __name__ == "__main__":
    unittest.main()
