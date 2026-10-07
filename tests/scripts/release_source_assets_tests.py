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

    @unittest.skipIf(sys.platform == "win32", "the existing macOS publication command requires Bash")
    def test_publication_checks_and_upload_set_use_same_source_asset(self):
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
import json, pathlib, sys
tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with pathlib.Path({str(commands)!r}).open("a") as stream:
    stream.write(json.dumps([tool, *args]) + "\\n")
if tool == "git":
    args = args[2:] if args[:1] == ["-C"] else args
    if args[:2] == ["rev-parse", "--verify"]: sys.exit(1)
    if args[:1] == ["branch"]: print("main")
    if args == ["rev-parse", "HEAD"]: print({self.revision!r})
else:
    if args[:2] == ["repo", "view"]: print("MikeMarcin/BetterPullReview\\tPRIVATE")
    if args[:2] == ["release", "view"] and not pathlib.Path({str(self.root / "created")!r}).exists(): sys.exit(1)
    if args[:2] == ["release", "create"]: pathlib.Path({str(self.root / "created")!r}).touch()
'''
        for name in ("git", "gh"):
            path = fake_bin / name
            path.write_text(stub)
            path.chmod(0o755)
        environment = {**runtime.os.environ, "PATH": str(fake_bin) + runtime.os.pathsep + runtime.os.environ["PATH"]}
        command = ["bash", str(repo / "tools/publish-release.sh"), "--version", "1.2.3", "--notes-file", str(notes)]
        source = dist / self.asset.name
        payload = source.read_bytes()
        source.unlink()
        result = subprocess.run(command + ["--dry-run"], env=environment, text=True, capture_output=True)
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
