#!/usr/bin/env python3
"""Exercise separate source assets and the existing publication command offline."""
from __future__ import annotations

import hashlib
import json
import os
import shlex
from pathlib import Path
import shutil
import subprocess
import sys
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
from skip_report import SkipReportingTestRunner

sys.path.insert(0, str(ROOT / "packaging"))
import package_runtime as runtime
import package as native_package
import verify_release_sources as release


class SourceAssetTests(unittest.TestCase):
    def setUp(self):
        temporary = ContainedTemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.info = {"version": "1.2.3", "platform": "Darwin"}
        self.revision = "a" * 40
        self.declared = [{"name": "Example", "version": "1", "url": "https://example.invalid/source.zip",
                          "sha256": hashlib.sha256(b"source").hexdigest()}]
        component_patch = patch.object(runtime, "components", return_value=self.declared)
        component_patch.start()
        self.addCleanup(component_patch.stop)
        self.asset = self.root / runtime.source_asset_name(self.info)
        with zipfile.ZipFile(self.asset, "w") as archive:
            archive.writestr("Example-1.zip", b"source")
        self.provenance = runtime.source_provenance(self.info, self.revision, runtime.digest(self.asset))
        self.material = {"source-provenance.json": json.dumps(self.provenance),
                         "SOURCE-OFFERS.txt": "\n".join(self.provenance["asset"].values()),
                         "THIRD-PARTY-NOTICES.txt": "\n".join(self.provenance["asset"].values())}
        self.app_zip, self.sbom = self.root / "app.zip", self.root / "app.spdx.json"
        self.write_package()

    def write_package(self, embedded=False):
        entries = []
        with zipfile.ZipFile(self.app_zip, "w") as archive:
            for name, text in self.material.items():
                relative = "Contents/Resources/ThirdParty/" + name
                archive.writestr("BetterPullReview.app/" + relative, text)
                entries.append({"fileName": "./" + relative, "checksums": [{"algorithm": "SHA256", "checksumValue": hashlib.sha256(text.encode()).hexdigest()}]})
            if embedded:
                archive.writestr("BetterPullReview.app/Contents/Resources/ThirdParty/corresponding-source.zip", b"unexpected")
        self.sbom.write_text(json.dumps({"packages": [{"SPDXID": "SPDXRef-BetterPullReview", "versionInfo": "1.2.3",
            "sourceInfo": f"git:{self.revision}; clean=True; tests-disabled Release; Darwin"}], "files": entries}))

    def verify(self):
        return release.verify(self.app_zip, self.sbom, self.asset, "1.2.3", self.revision)

    def test_exact_separate_asset_and_recipient_references(self):
        self.assertEqual(self.verify()["asset"], self.provenance["asset"])
        for name in ("SOURCE-OFFERS.txt", "THIRD-PARTY-NOTICES.txt"):
            original = self.material[name]
            self.material[name] = "unversioned source offer"
            self.write_package()
            with self.assertRaisesRegex(ValueError, "reference missing"):
                self.verify()
            self.material[name] = original

    def test_builds_and_checksums_sort_before_corresponding_source(self):
        for system, target, extensions in (("Darwin", "macos-universal", (".dmg", ".zip")),
                                           ("Windows", "windows-x64", (".msi",)),
                                           ("Linux", "linux-x64", (".deb", ".rpm"))):
            with self.subTest(platform=system):
                base = f"BetterPullReview-1.2.3-{target}"
                source = runtime.source_asset_name({**self.info, "platform": system})
                builds = [base + suffix for extension in extensions for suffix in (extension, extension + ".sha256")]
                sources = [source, source + ".sha256"]
                self.assertEqual(sorted(builds + sources), sorted(builds) + sources)
                self.assertTrue(runtime.source_asset_url({**self.info, "platform": system}).endswith("/" + source))

    def test_missing_corrupt_renamed_or_embedded_source_is_refused(self):
        saved = self.asset.read_bytes()
        self.asset.unlink()
        with self.assertRaisesRegex(ValueError, "missing or misnamed"):
            self.verify()
        self.asset.write_bytes(b"substituted source")
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            self.verify()
        self.asset.write_bytes(saved)
        renamed = self.asset.with_name("corresponding-source.zip")
        self.asset.rename(renamed)
        with self.assertRaisesRegex(ValueError, "missing or misnamed"):
            release.verify(self.app_zip, self.sbom, renamed, "1.2.3", self.revision)
        renamed.rename(self.asset)
        self.write_package(embedded=True)
        with self.assertRaisesRegex(ValueError, "embedded"):
            self.verify()

    def test_rehashed_stale_version_revision_or_source_url_is_refused(self):
        for section, key, value in (("release", "version", "1.2.2"), ("release", "sourceRevision", "b" * 40),
                                    ("asset", "url", "https://example.invalid/latest.zip"), ("asset", "name", "corresponding-source.zip")):
            with self.subTest(key=key):
                changed = json.loads(json.dumps(self.provenance))
                changed[section][key] = value
                self.material["source-provenance.json"] = json.dumps(changed)
                self.write_package()
                with self.assertRaisesRegex(ValueError, "corresponding source"):
                    self.verify()

    def test_source_offer_must_match_actual_zip_and_spdx(self):
        original = self.sbom.read_bytes()
        self.material["SOURCE-OFFERS.txt"] += "changed"
        self.write_package()
        self.sbom.write_bytes(original)
        with self.assertRaisesRegex(ValueError, "differs from SPDX"):
            self.verify()

    def test_native_package_driver_emits_source_beside_each_installer(self):
        for system, generator, extension in (("Windows", "WIX", ".msi"), ("Linux", "DEB", ".deb"), ("Linux", "RPM", ".rpm")):
            with self.subTest(generator=generator):
                build = self.root / ("build-" + generator)
                build.mkdir()
                output = self.root / ("output-" + generator)
                info = {**self.info, "platform": system}
                if system == "Linux":
                    info["target"] = "linux-ubuntu-26.04-x64" if generator == "DEB" else "linux-rhel-10-x64"
                source = runtime.source_asset_path(build, info, self.revision)
                source.parent.mkdir(parents=True)
                source.write_bytes(self.asset.read_bytes())
                provenance = runtime.source_provenance(info, self.revision, runtime.digest(source))
                def cpack(command, **kwargs):
                    if command[0] != "cpack":
                        return
                    (output / ("fixture" + extension)).write_bytes(b"installer")
                    installed = output / "_CPack_Packages/runtime"
                    legal = installed / "ThirdParty"
                    legal.mkdir(parents=True)
                    (installed / "sbom.spdx.json").write_text("{}")
                    (legal / "source-provenance.json").write_text(json.dumps(provenance))
                    for name in ("SOURCE-OFFERS.txt", "THIRD-PARTY-NOTICES.txt"):
                        (legal / name).write_text("\n".join(provenance["asset"].values()))
                with (patch.object(sys, "argv", ["package.py", "--build", str(build), "--output", str(output), "--generator", generator]),
                      patch.object(runtime, "inputs", return_value=info),
                      patch.object(runtime, "run", side_effect=lambda *args: "" if args[-1] == "--porcelain" else self.revision),
                      patch.object(runtime, "linux_baseline", return_value={"distribution": ["ubuntu" if generator == "DEB" else "rhel", "fixture"]}),
                      patch.object(native_package.subprocess, "run", side_effect=cpack)):
                    native_package.main()
                self.assertEqual((output / source.name).read_bytes(), source.read_bytes())
                self.assertIn(f"{runtime.digest(source)}  {source.name}", (output / ("fixture" + extension + ".sha256")).read_text())
                self.assertEqual(json.loads((output / ("fixture" + extension + ".provenance.json")).read_text())["products"][source.name], runtime.digest(source))
                self.assertFalse(list((output / "_CPack_Packages").rglob("*.zip")))

    def publisher_fixture(self):
        # Only fixture git/gh executables can run here. This exercises the real
        # shell command, its checksum gates and Python verifier without a remote.
        repo = self.root / "fixture-repo"
        for path in ("tools/publish-release.sh", "packaging/package_runtime.py", "packaging/verify_release_sources.py"):
            destination = repo / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / path, destination)
        (repo / "packaging/dependencies.json").write_text(json.dumps({"components": self.declared}))
        (repo / "CMakeLists.txt").write_text("project(\n VERSION 1.2.3\n)\n")
        notes = repo / "notes.md"
        notes.write_text("Fixture release only\n")
        dist = repo / "build/releases/1.2.3"
        dist.mkdir(parents=True)
        base = "BetterPullReview-1.2.3-macos-universal"
        files = {base + ".dmg": b"fixture DMG", base + ".zip": self.app_zip.read_bytes(),
                 base + ".spdx.json": self.sbom.read_bytes(), self.asset.name: self.asset.read_bytes()}
        for name, payload in files.items():
            (dist / name).write_bytes(payload)
            (dist / (name + ".sha256")).write_text(f"{hashlib.sha256(payload).hexdigest()}  {name}\n")
        commands = self.root / "commands.jsonl"
        fake_bin = self.root / "bin"
        fake_bin.mkdir()
        stub = f'''#!{sys.executable}
import json, os, pathlib, sys
tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with pathlib.Path({str(commands)!r}).open("a") as stream:
    stream.write(json.dumps([tool, *args]) + "\\n")
if tool == "git":
    args = args[2:] if args[:1] == ["-C"] else args
    if args[:2] == ["rev-parse", "--verify"]: sys.exit(1)
    if args[:1] == ["branch"]: print("main")
    if args == ["rev-parse", "HEAD"]: print({self.revision!r})
    if args == ["remote", "get-url", "--push", "--all", "origin"]:
        print(os.environ.get("FIXTURE_PUSH_URL", "https://github.com/MikeMarcin/BetterPullReview.git"))
else:
    if args[:2] == ["repo", "view"]:
        if os.environ.get("FIXTURE_REPO_QUERY_FAIL"): sys.exit(1)
        calls = [json.loads(line) for line in pathlib.Path({str(commands)!r}).read_text().splitlines()]
        count = sum(call[:3] == ["gh", "repo", "view"] for call in calls)
        states = os.environ.get("FIXTURE_VISIBILITY", "PRIVATE").split(",")
        print(os.environ.get("FIXTURE_REPOSITORY", "MikeMarcin/BetterPullReview") + "\\t" + states[min(count - 1, len(states) - 1)])
    if args[:2] == ["release", "view"] and not pathlib.Path({str(self.root / "created")!r}).exists(): sys.exit(1)
    if args[:2] == ["release", "create"]: pathlib.Path({str(self.root / "created")!r}).touch()
'''
        for name in ("git", "gh"):
            path = fake_bin / name
            path.write_text(stub)
            path.chmod(0o755)
        environment = {**runtime.os.environ, "PATH": str(fake_bin) + runtime.os.pathsep + runtime.os.environ["PATH"]}
        command = ["bash", str(repo / "tools/publish-release.sh"), "--version", "1.2.3", "--notes-file", str(notes)]
        return command, environment, commands, dist, files

    @staticmethod
    def publication_mutations(commands):
        result = []
        for call in map(json.loads, commands.read_text().splitlines()):
            if call[0] == "git" and len(call) > 3 and call[3] in ("tag", "push"):
                result.append(call[3])
            elif call[:3] == ["gh", "release", "create"]:
                result.append("release-create")
        return result

    @unittest.skipIf(sys.platform == "win32", "the publisher requires Bash")
    def test_publication_checks_and_upload_set_use_same_source_asset(self):
        command, environment, commands, dist, files = self.publisher_fixture()
        source = dist / self.asset.name
        payload = source.read_bytes()
        source.unlink()
        for flags in ([], ["--public"]):
            result = subprocess.run(command + flags + ["--dry-run"], env=environment, text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Release artifact is absent", result.stderr)
            self.assertFalse(any(item[0] == "gh" for item in map(json.loads, commands.read_text().splitlines())))
        source.write_bytes(payload)
        result = subprocess.run(command + ["--dry-run"], env=environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertFalse((self.root / "created").exists())
        result = subprocess.run(command, env=environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        uploaded = [item for item in map(json.loads, commands.read_text().splitlines()) if item[:3] == ["gh", "release", "create"]]
        self.assertEqual(len(uploaded), 1)
        self.assertEqual(set(uploaded[0][4:12]), {str(dist / name) for name in files} | {str(dist / (name + ".sha256")) for name in files})

    @unittest.skipIf(sys.platform == "win32", "the publisher requires Bash")
    def test_publication_requires_explicit_matching_visibility_and_origin(self):
        command, environment, commands, _, _ = self.publisher_fixture()
        for flags, settings, succeeds in [
            ([], {}, True),
            (["--public"], {"FIXTURE_VISIBILITY": "PUBLIC"}, True),
            ([], {"FIXTURE_VISIBILITY": "PUBLIC"}, False),
            (["--public"], {}, False),
            (["--public"], {"FIXTURE_VISIBILITY": "UNKNOWN"}, False),
            (["--public"], {"FIXTURE_REPO_QUERY_FAIL": "1"}, False),
            (["--public"], {"FIXTURE_VISIBILITY": "PUBLIC", "FIXTURE_REPOSITORY": "fixture/other"}, False),
            (["--public"], {"FIXTURE_VISIBILITY": "PUBLIC", "FIXTURE_PUSH_URL": "https://github.com/fixture/other.git"}, False),
            (["--public"], {"FIXTURE_VISIBILITY": "PUBLIC", "FIXTURE_PUSH_URL": "https://github.com/MikeMarcin/BetterPullReview.git\nhttps://github.com/fixture/other.git"}, False),
            (["--public"], {"FIXTURE_VISIBILITY": "PUBLIC", "FIXTURE_PUSH_URL": "git@github.com:MikeMarcin/BetterPullReview.git"}, True),
        ]:
            with self.subTest(flags=flags, settings=settings):
                commands.write_text("")
                result = subprocess.run(command + flags + ["--dry-run"], env={**environment, **settings}, text=True, capture_output=True)
                self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)
                self.assertEqual(self.publication_mutations(commands), [])
                self.assertFalse((self.root / "created").exists())

    @unittest.skipIf(sys.platform == "win32", "the publisher requires Bash")
    def test_publication_rechecks_visibility_before_each_mutation(self):
        command, environment, commands, dist, files = self.publisher_fixture()
        for visibility, mutations in [("PUBLIC,PRIVATE", []), ("PUBLIC,PUBLIC,PRIVATE", ["tag", "push"]),
                                      ("PUBLIC", ["tag", "push", "release-create"])]:
            with self.subTest(visibility=visibility):
                commands.write_text("")
                result = subprocess.run(command + ["--public", "--prerelease"],
                                        env={**environment, "FIXTURE_VISIBILITY": visibility}, text=True, capture_output=True)
                self.assertEqual(result.returncode == 0, visibility == "PUBLIC", result.stdout + result.stderr)
                self.assertEqual(self.publication_mutations(commands), mutations)
        calls = list(map(json.loads, commands.read_text().splitlines()))
        upload = next(call for call in calls if call[:3] == ["gh", "release", "create"])
        self.assertEqual(set(upload[4:12]), {str(dist / name) for name in files} | {str(dist / (name + ".sha256")) for name in files})
        for flag in ("--verify-tag", "--prerelease", "--latest=false"):
            self.assertIn(flag, upload)
        self.assertTrue(any(call[0] == "git" and "--atomic" in call for call in calls))
        self.assertFalse(any(call[:3] == ["gh", "repo", "edit"] for call in calls))


@unittest.skipIf(sys.platform == "win32", "the macOS release command requires Bash")
class RelocatedLaunchCommandTests(unittest.TestCase):
    def setUp(self):
        temporary = ContainedTemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.bundle = self.root / "relocated app" / "BetterPullReview.app"
        self.build = self.root / "release build"
        self.build.mkdir()
        self.qt = self.root / "producer Qt"
        config = self.qt / "lib/cmake/Qt6"
        config.mkdir(parents=True)
        (config / "Qt6Config.cmake").write_text("fixture configuration")
        self.cache = self.build / "CMakeCache.txt"
        self.cache.write_text("Qt6_DIR:PATH=" + str(config) + "\n")
        self.plugin = self.qt / "plugins/platforms/libqoffscreen.dylib"
        self.plugin.parent.mkdir(parents=True)
        self.plugin.write_bytes(b"pinned fixture plugin")
        for framework in ("QtCore", "QtGui"):
            image = self.bundle / ("Contents/Frameworks/" + framework + ".framework/Versions/A/" + framework)
            image.parent.mkdir(parents=True)
            image.write_bytes(b"packaged fixture framework")
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.calls = self.root / "tool-calls.jsonl"
        for name in ("ditto", "lipo", "otool", "install_name_tool", "codesign"):
            tool = self.bin / name
            tool.write_text("""#!/usr/bin/env python3
import json, os, shutil, sys
from pathlib import Path
name, args = Path(sys.argv[0]).name, sys.argv[1:]
with Path(os.environ['STUB_CALLS']).open('a') as output:
    output.write(json.dumps([name, *args]) + '\\n')
if name == 'lipo':
    sys.exit(int(os.environ.get('STUB_ARCH_STATUS', '0')))
if name == 'ditto':
    if os.environ.get('STUB_ESCAPE_CLONE'):
        Path(args[1]).symlink_to(args[0], target_is_directory=True)
    else:
        shutil.copytree(args[0], args[1], symlinks=True)
        if os.environ.get('STUB_CORRUPT_COPY'):
            (Path(args[1]) / 'Contents/MacOS/BetterPullReview').write_bytes(b'wrong copy')
elif name == 'otool' and '-L' in args:
    print(args[-1] + ':')
    for dependency in os.environ.get('STUB_DEPENDENCIES', '@rpath/QtCore.framework/Versions/A/QtCore;@rpath/QtGui.framework/Versions/A/QtGui;/usr/lib/libSystem.B.dylib').split(';'):
        print('    ' + dependency + ' (compatibility version 6.0.0, current version 6.12.0)')
elif name == 'otool':
    added = Path(args[-1] + '.rpath')
    rpath = os.environ.get('STUB_FINAL_RPATH', added.read_text()) if added.exists() else '/unrelated/producer/lib'
    print('cmd LC_RPATH\\ncmdsize 80\\npath ' + rpath + ' (offset 12)')
elif name == 'install_name_tool' and '-add_rpath' in args:
    Path(args[-1] + '.rpath').write_text(args[1])
elif name == 'codesign' and '--force' in args:
    target = Path(args[-1])
    signature = b'signed fixture copy'
    if target.is_dir():
        target /= 'Contents/MacOS/BetterPullReview'
        signature = b'# app sealed fixture\\n'
    with target.open('ab') as output:
        output.write(signature)
elif name == 'codesign' and '-d' in args:
    print('TeamIdentifier=' + os.environ.get('STUB_TEAM', 'YTQKP2V2A8'), file=sys.stderr)
""")
            tool.chmod(0o700)
        executable = self.bundle / "Contents" / "MacOS" / "BetterPullReview"
        executable.parent.mkdir(parents=True)
        executable.write_text("""#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
Path(os.environ['STUB_CAPTURE']).write_text(json.dumps({
    'executable': str(Path(__file__)),
    'platform': os.environ.get('QT_QPA_PLATFORM'),
    'platform_path': os.environ.get('QT_QPA_PLATFORM_PLUGIN_PATH'),
    'plugin_path': os.environ.get('QT_PLUGIN_PATH'),
    'style': os.environ.get('QT_STYLE_OVERRIDE'),
    'home': os.environ.get('HOME'), 'argv': sys.argv[1:]}))
print(os.environ['STUB_MARKERS'])
if os.environ.get('STUB_CHANGE_ORIGINAL'):
    Path(os.environ['STUB_ORIGINAL']).write_bytes(b'changed delivery')
sys.exit(int(os.environ['STUB_STATUS']))
# signing fixture bytes follow as comments
""")
        executable.chmod(0o700)

    def launch(self, *, status=0, private=True, identity="com.gamecoretech.betterpullreview",
               prepare_only=False, overrides=None):
        script = (ROOT / "tools/release.sh").read_text()
        prefix, _ = script.split('[[ "$(uname -s)" == "Darwin" ]]', 1)
        prefix = "\n".join(
            "ROOT_DIR=" + shlex.quote(str(ROOT)) if line.startswith("ROOT_DIR=") else line
            for line in prefix.splitlines())
        profile = self.root / "isolated profile"
        capture = self.root / "command.json"
        markers = ("AUTOMATED_PROFILE_PRIVATE_OK\n" if private else "")
        markers += "NOTIFICATION_BUNDLE_IDENTITY:" + identity
        environment = dict(os.environ, QT_QPA_PLATFORM="cocoa", QT_STYLE_OVERRIDE="ambient-style",
                           QT_QPA_PLATFORM_PLUGIN_PATH="/ambient/platforms", QT_PLUGIN_PATH="/ambient/plugins",
                           PATH=str(self.bin) + os.pathsep + os.environ["PATH"], STUB_CALLS=str(self.calls),
                           STUB_ORIGINAL=str(self.bundle / "Contents/MacOS/BetterPullReview"),
                           STUB_CAPTURE=str(capture), STUB_MARKERS=markers, STUB_STATUS=str(status))
        environment.update(overrides or {})
        harness = prefix + "\nBUILD_DIR=" + shlex.quote(str(self.build))
        harness += "\nPREPARE_ONLY=" + ("1" if prepare_only else "0")
        harness += "\nverify_relocated_launch " + shlex.quote(str(self.bundle)) + " " + shlex.quote(str(profile)) + "\n"
        result = subprocess.run(["bash", "-s"], input=harness, env=environment,
                                text=True, capture_output=True)
        return result, json.loads(capture.read_text()) if capture.exists() else None, profile

    def test_actual_child_is_offscreen_with_isolated_home_and_unchanged_arguments(self):
        for status in (0, 1):
            with self.subTest(status=status):
                result, observed, profile = self.launch(status=status)
                self.assertEqual(result.returncode, 0, result.stderr)
                clone = Path(observed.pop("executable")).parents[2]
                self.assertTrue(clone.is_relative_to(profile))
                self.assertEqual(observed, {"platform": "offscreen", "style": None, "home": str(profile),
                                           "platform_path": None, "plugin_path": None,
                                           "argv": ["--self-test", "--automated-run-budget-ms", "8000"]})
                self.assertTrue(profile.is_dir())
                self.assertIn("Relocated offscreen assessment clone verified", result.stdout)
                self.assertIn("Delivered bundle unchanged after assessment", result.stdout)
                copied = clone / "Contents/PlugIns/platforms/libqoffscreen.dylib"
                self.assertEqual(self.plugin.read_bytes(), b"pinned fixture plugin")
                self.assertEqual(copied.read_bytes(), b"pinned fixture pluginsigned fixture copy")
                self.assertFalse((self.bundle / "Contents/PlugIns/platforms/libqoffscreen.dylib").exists())
                self.assertIn("source_sha256=" + hashlib.sha256(self.plugin.read_bytes()).hexdigest(), result.stdout)
                self.assertIn("signed_sha256=" + hashlib.sha256(copied.read_bytes()).hexdigest(), result.stdout)
                self.assertIn("architectures=arm64,x86_64; team=YTQKP2V2A8", result.stdout)
                calls = list(map(json.loads, self.calls.read_text().splitlines()))
                self.assertIn(["lipo", str(self.plugin.resolve()), "-verify_arch", "arm64", "x86_64"], calls)
                self.assertIn(["install_name_tool", "-delete_rpath", "/unrelated/producer/lib", str(copied)], calls)
                self.assertIn(["install_name_tool", "-add_rpath", str(clone / "Contents/Frameworks"), str(copied)], calls)
                self.assertIn(["codesign", "--force", "--timestamp", "--options", "runtime", "--sign",
                               "Developer ID Application: Michael Marcin (YTQKP2V2A8)", str(copied)], calls)
                self.assertIn(["codesign", "--verify", "--deep", "--strict", str(clone)], calls)
                self.assertIn(["codesign", "--verify", "--deep", "--strict", str(self.bundle)], calls)

    def test_missing_or_unbound_producer_qt_refuses_before_child_launch(self):
        for cache in (None, "Qt6_DIR:PATH=relative/Qt6\n", "Qt6_DIR:PATH=/missing/Qt6\n"):
            with self.subTest(cache=cache):
                if cache is None:
                    self.cache.unlink()
                else:
                    self.cache.write_text(cache)
                result, observed, _ = self.launch()
                self.assertNotEqual(result.returncode, 0)
                self.assertIsNone(observed)
                self.assertTrue(all(call[0] == "ditto" for call in map(json.loads, self.calls.read_text().splitlines())))

    def test_missing_producer_plugin_refuses_before_child_launch(self):
        self.plugin.unlink()
        result, observed, _ = self.launch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("assessment plugin is absent", result.stderr)
        self.assertIsNone(observed)

    def test_incompatible_architecture_external_dependency_and_wrong_team_refuse(self):
        for overrides, diagnostic in (({"STUB_ARCH_STATUS": "1"}, "not universal"),
                                      ({"STUB_DEPENDENCIES": "/external/QtGui"}, "External assessment dependency"),
                                      ({"STUB_FINAL_RPATH": "/external/frameworks"}, "RPATH escapes"),
                                      ({"STUB_ESCAPE_CLONE": "1"}, "not a real bundle"),
                                      ({"STUB_CORRUPT_COPY": "1"}, "differs before resealing"),
                                      ({"STUB_TEAM": "OTHER"}, "signer does not match")):
            with self.subTest(overrides=overrides):
                result, observed, _ = self.launch(overrides=overrides)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)
                self.assertIsNone(observed)

    def test_missing_packaged_framework_refuses_before_child_launch(self):
        (self.bundle / "Contents/Frameworks/QtGui.framework/Versions/A/QtGui").unlink()
        result, observed, _ = self.launch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dependency is absent from the package", result.stderr)
        self.assertIsNone(observed)

    def test_prepare_only_uses_ad_hoc_assessment_copy(self):
        result, observed, profile = self.launch(prepare_only=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNotNone(observed)
        clone = Path(observed["executable"]).parents[2]
        calls = list(map(json.loads, self.calls.read_text().splitlines()))
        self.assertIn(["codesign", "--force", "--sign", "-", "--timestamp=none",
                       str(clone / "Contents/PlugIns/platforms/libqoffscreen.dylib")], calls)
        self.assertNotIn("Developer ID Application", self.calls.read_text())

    def test_original_delivery_mutation_is_refused_after_clone_launch(self):
        result, observed, _ = self.launch(overrides={"STUB_CHANGE_ORIGINAL": "1"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNotNone(observed)
        self.assertIn("Delivered bundle changed during assessment", result.stderr)

    def test_private_profile_marker_remains_required(self):
        result, _, _ = self.launch(private=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("did not reach the composed launch probe", result.stderr)

    def test_production_bundle_identity_remains_required(self):
        result, _, _ = self.launch(identity="com.example.wrong")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("did not report the production bundle identity", result.stderr)

    def test_unexpected_child_failure_remains_refused(self):
        result, _, _ = self.launch(status=2)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("did not complete its launch probe (status 2)", result.stderr)


@unittest.skipIf(sys.platform == "win32", "the macOS release command requires Bash")
class NotaryCommandTests(unittest.TestCase):
    def submit(self, options=(), environment=None, status=0):
        script = (ROOT / "tools/release.sh").read_text()
        # Execute the real option parser and submit helper, with no build,
        # signing, Keychain access or network operation. Capture exact argv.
        prefix, body = script.split('[[ "$(uname -s)" == "Darwin" ]]', 1)
        prefix = "\n".join(
            "ROOT_DIR=" + shlex.quote(str(ROOT)) if line.startswith("ROOT_DIR=") else line
            for line in prefix.splitlines())
        calls = [line.strip() for line in body.splitlines()
                 if line.strip().startswith("notary_submit ")]
        self.assertEqual(calls, ['notary_submit "${NOTARY_ZIP}"', 'notary_submit "${DMG_PATH}"'])
        self.assertNotIn("xcrun notarytool submit", body)
        harness = prefix + '\nxcrun() { printf \'%s\\0\' "$@"; return "$STUB_STATUS"; }\n'
        harness += 'NOTARY_ZIP="/fixture with spaces/app notary.zip"\nDMG_PATH="/fixture with spaces/release.dmg"\n'
        harness += "\n".join(calls) + "\n"
        env = {key: value for key, value in os.environ.items()
               if key not in ("APPLE_NOTARY_PROFILE", "APPLE_NOTARY_KEYCHAIN")}
        env.update(environment or {})
        env["STUB_STATUS"] = str(status)
        result = subprocess.run(["bash", "-s", "--", *options], input=harness,
                                env=env, text=True, capture_output=True)
        return result, result.stdout.rstrip("\0").split("\0") if result.stdout else []

    def expected(self, profile="BetterPullReview-notary", keychain=None):
        commands = []
        for artifact in ("/fixture with spaces/app notary.zip", "/fixture with spaces/release.dmg"):
            commands += ["notarytool", "submit", artifact, "--keychain-profile", profile]
            if keychain is not None:
                commands += ["--keychain", keychain]
            commands += ["--wait"]
        return commands

    def test_default_submissions_preserve_implicit_keychain(self):
        result, argv = self.submit()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(argv, self.expected())

    def test_environment_and_cli_preserve_paths_and_cli_precedence(self):
        env = {"APPLE_NOTARY_PROFILE": "environment profile", "APPLE_NOTARY_KEYCHAIN": "/fixture keychains/env login.keychain-db"}
        result, argv = self.submit(environment=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(argv, self.expected(env["APPLE_NOTARY_PROFILE"], env["APPLE_NOTARY_KEYCHAIN"]))
        result, argv = self.submit(("--notary-profile", "chosen profile", "--notary-keychain", "/chosen keychain/login.keychain-db"), env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(argv, self.expected("chosen profile", "/chosen keychain/login.keychain-db"))

    def test_missing_or_empty_keychain_argument_refuses_before_submission(self):
        for options, message in ((["--notary-keychain"], "requires a value"),
                                 (["--notary-keychain", ""], "requires a nonempty path")):
            with self.subTest(options=options):
                result, argv = self.submit(options)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
                self.assertEqual(argv, [])

    def test_submission_failure_stops_the_workflow(self):
        result, argv = self.submit(status=69)
        self.assertEqual(result.returncode, 69)
        self.assertEqual(argv, self.expected()[:6])


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
