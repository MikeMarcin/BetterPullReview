#!/usr/bin/env python3
"""Native release ordering, final bytes, wrong signers and offline publication guards."""
from __future__ import annotations

import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
from skip_report import SkipReportingTestRunner

sys.path.insert(0, str(ROOT / "packaging"))
import package as producer
import package_runtime as runtime
import release_signing as signing
import verify_native_release as release
from release_gpg_vault_tests import VaultTests


class NativeReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = ContainedTemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.revision = "a" * 40
        self.components = [{"name": "Example", "version": "1", "url": "https://example.invalid/source.zip",
                            "sha256": hashlib.sha256(b"source").hexdigest()}]
        self.patches = patch.object(runtime, "components", return_value=self.components)
        self.patches.start()
        self.addCleanup(self.patches.stop)
        self.key = "B" * 40

    def package(self, generator, signed=True, use_vault=False, vault_failure=False, tamper_library=False):
        build = self.root / ("build-" + generator)
        build.mkdir()
        system = "Windows" if generator == "WIX" else "Linux"
        target = "" if generator == "WIX" else "linux-ubuntu-26.04-x64" if generator == "DEB" else "linux-rhel-10-x64"
        info = {"platform": system, "version": "1.2.3", "target": target,
                "families": {"agent_integrations": False, "local_automation": True}, "variant": "-agent-free"}
        image_suffix = ".exe" if system == "Windows" else ""
        app, helper = build / ("better_pull_review" + image_suffix), build / ("better_pull_review_agent_supervisor" + image_suffix)
        info.update(app=str(app), helpers=[str(helper)])
        library = build / ("bpr_xdiff.dll" if system == "Windows" else "libbpr_xdiff.so")
        info["private_libraries"] = {library.name: str(library)}
        source = runtime.source_asset_path(build, info, self.revision)
        source.parent.mkdir(parents=True)
        with zipfile.ZipFile(source, "w") as archive:
            archive.writestr("Example-1.zip", b"source")
        binding = runtime.source_provenance(info, self.revision, runtime.digest(source))
        output = self.root / ("output-" + generator)
        extension = {"WIX": ".msi", "DEB": ".deb", "RPM": ".rpm"}[generator]
        base = "BetterPullReview-1.2.3-" + ("windows-x64" if generator == "WIX" else target) + "-agent-free"
        self.events = []
        self.detached_options = []
        def unlock(path, key):
            self.events.append("vault-unlock")
            if vault_failure:
                raise producer.vault.VaultError("stored credential unavailable; no interactive fallback")
            return {"keyFingerprint": self.key, "gnupgHome": "/scoped/release/gnupg"}
        def native(command, **kwargs):
            if command[0] == "cmake":
                self.events.append("build")
                app.write_bytes(b"app")
                helper.write_bytes(b"helper")
                library.write_bytes(b"library")
                return
            self.events.append("cpack")
            installed = output / "_CPack_Packages/runtime"
            legal = installed / "ThirdParty"
            legal.mkdir(parents=True)
            for image in (app, helper, library):
                shutil.copyfile(image, installed / image.name)
            if tamper_library:
                (installed / library.name).write_bytes(b"substituted library")
            (legal / "source-provenance.json").write_text(json.dumps(binding))
            for name in ("SOURCE-OFFERS.txt", "THIRD-PARTY-NOTICES.txt"):
                (legal / name).write_text("\n".join(binding["asset"].values()))
            files = [{"fileName": "./" + path.relative_to(installed).as_posix(), "checksums": [{"algorithm": "SHA256", "checksumValue": runtime.digest(path)}]}
                     for path in sorted(installed.rglob("*")) if path.is_file()]
            runtime.write_json(installed / "sbom.spdx.json", {"packages": [{"SPDXID": "SPDXRef-BetterPullReview", "versionInfo": "1.2.3",
                "sourceInfo": runtime.product_source_info(info, self.revision, True)}], "files": files})
            (output / (base + extension)).write_bytes(b"installer")
        def windows(path, *args):
            self.events.append("sign:" + path.name)
            path.write_bytes(path.read_bytes() + b"signed")
            return {"publisher": "Michael Marcin", "certificateSHA1": "C" * 40, "timestampCertificateSHA1": "D" * 40}
        def detached(path, key, **options):
            self.detached_options.append(options)
            signature = path.with_name(path.name + ".asc")
            signature.write_bytes(b"fixture-signature")
            return signature, {"keyFingerprint": key, "signingFingerprint": key}
        def rpm(path, key):
            path.write_bytes(path.read_bytes() + b"rpm-signed")
        argv = ["package.py", "--build", str(build), "--output", str(output), "--generator", generator]
        if signed and generator == "WIX":
            inputs = [build / name for name in ("signtool.exe", "dlib.dll", "metadata.json")]
            for path in inputs:
                path.write_text("fixture only")
            argv += ["--signtool", str(inputs[0]), "--azure-dlib", str(inputs[1]), "--azure-metadata", str(inputs[2]), "--publisher", "Michael Marcin"]
        elif signed:
            argv += ["--gpg-key", self.key]
        if use_vault:
            argv += ["--gpg-vault-config", str(self.root / "vault.json")]
        with (patch.object(sys, "argv", argv), patch.object(runtime, "inputs", return_value=info),
              patch.object(runtime, "run", side_effect=lambda *args: "" if args[-1] == "--porcelain" else self.revision),
              patch.object(runtime, "linux_baseline", return_value={"distribution": ["ubuntu" if generator == "DEB" else "rhel", "fixture"]}),
              patch.object(producer.subprocess, "run", side_effect=native), patch.object(signing, "sign_windows", side_effect=windows),
              patch.object(signing, "authenticode"), patch.object(signing, "sign_detached", side_effect=detached),
              patch.object(signing, "sign_rpm", side_effect=rpm), patch.object(signing, "verify_rpm"),
              patch.object(producer.vault, "prepare_linux_signing", side_effect=unlock)):
            producer.main()
        return output / (base + extension + ".release.json")

    def test_signing_order_and_final_byte_manifest_all_native_generators(self):
        for generator in ("WIX", "DEB", "RPM"):
            with self.subTest(generator=generator):
                manifest = self.package(generator)
                paths = release.verify(manifest, "1.2.3", self.revision)
                self.assertIn(manifest, paths)
                if generator == "WIX":
                    self.assertEqual(self.events, ["build", "sign:better_pull_review.exe", "sign:better_pull_review_agent_supervisor.exe", "sign:bpr_xdiff.dll",
                                                   "cpack", "sign:" + manifest.name.removesuffix(".release.json")])
                if generator == "RPM":
                    record = json.loads(manifest.read_text())
                    self.assertTrue((manifest.parent / record["installer"]).read_bytes().endswith(b"rpm-signed"))

    def test_unsigned_candidate_cannot_enter_publisher(self):
        manifest = self.package("WIX", signed=False)
        self.assertFalse(manifest.exists())

    def test_changed_staged_library_is_refused_before_installer_signing(self):
        with self.assertRaisesRegex(ValueError, "CPack changed a signed first-party image: bpr_xdiff.dll"):
            self.package("WIX", tamper_library=True)
        self.assertEqual(self.events[-1], "cpack")

    def test_missing_library_receipt_is_refused_after_rehashing(self):
        manifest = self.package("WIX")
        record = json.loads(manifest.read_text())
        record["firstPartyImages"].pop("bpr_xdiff.dll")
        self.rehash(manifest, record)
        with self.assertRaisesRegex(ValueError, "incomplete Authenticode"):
            release.verify(manifest, "1.2.3", self.revision)

    def test_vault_unlock_precedes_build_and_scopes_every_detached_signature(self):
        manifest = self.package("DEB", use_vault=True)
        self.assertEqual(self.events, ["vault-unlock", "build", "cpack"])
        self.assertEqual(len(self.detached_options), 6)
        self.assertTrue(all(item == {"gpg_home": Path("/scoped/release/gnupg")} for item in self.detached_options))
        self.assertIn(manifest, release.verify(manifest, "1.2.3", self.revision))

    def test_failed_vault_unlock_creates_no_output_and_never_builds(self):
        with self.assertRaisesRegex(producer.vault.VaultError, "no interactive fallback"):
            self.package("DEB", use_vault=True, vault_failure=True)
        self.assertEqual(self.events, ["vault-unlock"])
        self.assertFalse((self.root / "output-DEB").exists())

    def test_linux_manifest_refuses_obsolete_or_mismatched_target(self):
        manifest = self.package("DEB")
        record = json.loads(manifest.read_text())
        self.assertEqual(record["target"], "linux-ubuntu-26.04-x64")
        self.assertIn("linux-ubuntu-26.04-x64", record["sourceArchive"])
        for target in ("linux-ubuntu-24.04-x64", "linux-rhel-10-x64", "linux-x64"):
            with self.subTest(target=target):
                record["target"] = target
                runtime.write_json(manifest, record)
                with self.assertRaisesRegex(ValueError, "baseline and generator mismatch"):
                    release.verify(manifest, "1.2.3", self.revision)

    def test_stale_commit_tampered_product_and_missing_signature_refused(self):
        manifest = self.package("DEB")
        with self.assertRaisesRegex(ValueError, "producer mismatch"):
            release.verify(manifest, "1.2.3", "e" * 40)
        record = json.loads(manifest.read_text())
        path = manifest.parent / record["installer"]
        path.write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            release.verify(manifest, "1.2.3", self.revision)
        record["products"][path.name] = runtime.digest(path)
        manifest.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "provenance mismatch"):
            release.verify(manifest, "1.2.3", self.revision)

    def test_wrong_first_party_receipt_and_runtime_inventory_refused(self):
        manifest = self.package("WIX")
        record = json.loads(manifest.read_text())
        record["firstPartyImages"]["better_pull_review.exe"]["sha256"] = "f" * 64
        manifest.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "receipts differ"):
            release.verify(manifest, "1.2.3", self.revision)

    def rehash(self, manifest, record):
        # Deliberately regenerate all ordinary container/metadata hashes so a
        # structural rejection cannot be mistaken for an outer hash mismatch.
        provenance_path = manifest.parent / record["provenance"]
        provenance = json.loads(provenance_path.read_text())
        provenance["firstPartyImages"] = record["firstPartyImages"]
        provenance["products"] = {name: runtime.digest(manifest.parent / name) for name in provenance["products"]}
        provenance["sbomSHA256"] = runtime.digest(manifest.parent / record["sbom"])
        checksums = manifest.parent / record["checksums"]
        checksums.write_text("".join(f"{digest}  {name}\n" for name, digest in provenance["products"].items()))
        runtime.write_json(provenance_path, provenance)
        record["products"] = {name: runtime.digest(manifest.parent / name) for name in record["products"]}
        runtime.write_json(manifest, record)

    def test_missing_executable_with_null_receipt_is_refused_after_rehashing(self):
        manifest = self.package("WIX")
        record = json.loads(manifest.read_text())
        sbom = manifest.parent / record["sbom"]
        inventory = json.loads(sbom.read_text())
        inventory["files"] = [entry for entry in inventory["files"] if entry["fileName"] != "./better_pull_review.exe"]
        runtime.write_json(sbom, inventory)
        archive_path = manifest.parent / record["runtimeArchive"]
        with zipfile.ZipFile(archive_path) as archive:
            data = {name: archive.read(name) for name in archive.namelist() if name != "better_pull_review.exe"}
        data["sbom.spdx.json"] = sbom.read_bytes()
        with zipfile.ZipFile(archive_path, "w") as archive:
            for name, value in data.items():
                archive.writestr(name, value)
        record["firstPartyImages"]["better_pull_review.exe"]["sha256"] = None
        self.rehash(manifest, record)
        with self.assertRaisesRegex(ValueError, "producer differs"):
            release.verify(manifest, "1.2.3", self.revision)

    def test_unsafe_zip_directory_is_refused_after_rehashing(self):
        manifest = self.package("WIX")
        record = json.loads(manifest.read_text())
        with zipfile.ZipFile(manifest.parent / record["runtimeArchive"], "a") as archive:
            archive.writestr("../../outside-release-root/", b"")
        self.rehash(manifest, record)
        with self.assertRaisesRegex(ValueError, "unsafe runtime member"):
            release.verify(manifest, "1.2.3", self.revision)

    def test_missing_linux_executable_is_refused_after_rehashing(self):
        manifest = self.package("DEB")
        record = json.loads(manifest.read_text())
        sbom = manifest.parent / record["sbom"]
        inventory = json.loads(sbom.read_text())
        inventory["files"] = [entry for entry in inventory["files"] if entry["fileName"] != "./better_pull_review"]
        runtime.write_json(sbom, inventory)
        archive_path = manifest.parent / record["runtimeArchive"]
        with tarfile.open(archive_path) as archive:
            data = {entry.name: archive.extractfile(entry).read() for entry in archive.getmembers()
                    if entry.isfile() and not entry.name.endswith("/better_pull_review")}
        root_name = archive_path.name.removesuffix("-user.tar.gz")
        data[root_name + "/sbom.spdx.json"] = sbom.read_bytes()
        with tarfile.open(archive_path, "w:gz") as archive:
            for name, value in data.items():
                entry = tarfile.TarInfo(name)
                entry.size = len(value)
                archive.addfile(entry, io.BytesIO(value))
        self.rehash(manifest, record)
        with self.assertRaisesRegex(ValueError, "Linux first-party image missing"):
            release.verify(manifest, "1.2.3", self.revision)

    def test_incomplete_gpg_receipts_and_escaping_products_refused(self):
        manifest = self.package("DEB")
        original = manifest.read_text()
        record = json.loads(original)
        record["signatures"].pop(record["installer"])
        manifest.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "receipts differ"):
            release.verify(manifest, "1.2.3", self.revision)
        record = json.loads(original)
        record["products"]["../secret"] = "a" * 64
        manifest.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "invalid release asset name"):
            release.verify(manifest, "1.2.3", self.revision)

    def test_duplicate_manifest_deduplicates_only_identical_assets(self):
        manifest = self.package("WIX")
        duplicate = self.root / "duplicate"
        shutil.copytree(manifest.parent, duplicate)
        other = duplicate / manifest.name
        listing = self.root / "assets"
        argv = ["verify_native_release.py", "--manifest", str(manifest), "--manifest", str(other),
                "--version", "1.2.3", "--source-revision", self.revision, "--asset-list", str(listing)]
        with patch.object(sys, "argv", argv):
            release.main()
        self.assertEqual(len(listing.read_bytes().split(b"\0")) - 1, len(release.verify(manifest, "1.2.3", self.revision)))
        record = json.loads(other.read_text())
        record["nativeAcceptance"] = "a different observation"
        other.write_text(json.dumps(record))
        with patch.object(sys, "argv", argv), self.assertRaisesRegex(ValueError, "conflicting release asset"):
            release.main()

    def test_gpg_verification_requires_full_approved_primary_or_subkey(self):
        path = self.root / "payload"
        with self.assertRaisesRegex(ValueError, "full OpenPGP"):
            signing.fingerprint("B" * 16)
        for status, passes in (("[GNUPG:] VALIDSIG " + "C" * 40 + " 2026-10-10 1 0 4 0 1 8 00 " + self.key, True),
                               ("[GNUPG:] VALIDSIG " + "C" * 40 + " 2026-10-10 1 0 4 0 1 8 00 " + "D" * 40, False),
                               ("[GNUPG:] VALIDSIG malformed", False)):
            with patch.object(signing.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, status)):
                if passes:
                    self.assertEqual(signing.verify_detached(path, path, self.key)["keyFingerprint"], self.key)
                else:
                    with self.assertRaises(ValueError):
                        signing.verify_detached(path, path, self.key)

    @unittest.skipIf(sys.platform == "win32", "the publisher requires Bash")
    def test_publisher_uses_native_assets_and_refuses_bad_bytes_before_github(self):
        manifest = self.package("DEB")
        windows = self.package("WIX")
        repo = self.root / "repository"
        for name in ("tools/publish-release.sh", "packaging/package_runtime.py", "packaging/verify_native_release.py"):
            destination = repo / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        (repo / "packaging/dependencies.json").write_text(json.dumps({"components": self.components}))
        (repo / "CMakeLists.txt").write_text("project(\n VERSION 1.2.3\n)\n")
        notes = repo / "notes.md"
        notes.write_text("fixture only")
        log = self.root / "commands.jsonl"
        bin_path = self.root / "bin"
        bin_path.mkdir()
        stub = f'''#!{sys.executable}
import json, os, pathlib, sys
tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with pathlib.Path({str(log)!r}).open('a') as stream:
    stream.write(json.dumps([tool, *args]) + '\\n')
if tool == 'git':
    args = args[2:] if args[:1] == ['-C'] else args
    if args[:2] == ['rev-parse', '--verify']: sys.exit(1)
    if args[:1] == ['branch']: print('main')
    if args == ['rev-parse', 'HEAD']: print({self.revision!r})
else:
    if args[:2] == ['repo', 'view']: print('MikeMarcin/BetterPullReview\\t' + os.environ.get('FIXTURE_VISIBILITY', 'PRIVATE'))
    if args[:2] == ['release', 'view'] and '--json' not in args: sys.exit(1)
'''
        for name in ("git", "gh"):
            path = bin_path / name
            path.write_text(stub)
            path.chmod(0o755)
        environment = {**os.environ, "PATH": str(bin_path) + os.pathsep + os.environ["PATH"]}
        command = ["bash", str(repo / "tools/publish-release.sh"), "--version", "1.2.3", "--notes-file", str(notes),
                   "--manifest", str(manifest), "--manifest", str(windows)]
        result = subprocess.run(command + ["--dry-run"], env=environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(any(item[:3] == ["gh", "release", "create"] for item in map(json.loads, log.read_text().splitlines())))
        result = subprocess.run(command + ["--prerelease"], env=environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = list(map(json.loads, log.read_text().splitlines()))
        upload = next(item for item in calls if item[:3] == ["gh", "release", "create"])
        expected = {str(path.resolve()) for path in release.verify(manifest, "1.2.3", self.revision) + release.verify(windows, "1.2.3", self.revision)}
        self.assertEqual(set(upload[4:upload.index("--repo")]), expected)
        self.assertIn("--prerelease", upload)
        self.assertIn("--latest=false", upload)
        self.assertTrue(any("--atomic" in item for item in calls))
        result = subprocess.run(command, env={**environment, "FIXTURE_VISIBILITY": "PUBLIC"}, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not the expected private", result.stderr)
        log.unlink()
        record = json.loads(manifest.read_text())
        (manifest.parent / record["installer"]).write_bytes(b"tampered")
        result = subprocess.run(command, env=environment, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(item[0] == "gh" for item in map(json.loads, log.read_text().splitlines())))


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
