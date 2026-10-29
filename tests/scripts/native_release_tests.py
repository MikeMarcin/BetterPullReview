#!/usr/bin/env python3
"""Native release ordering, final bytes, wrong signers and offline publication guards."""
from __future__ import annotations

import contextlib
import copy
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
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
from skip_report import SkipReportingTestRunner

sys.path.insert(0, str(ROOT / "packaging"))
import package as producer
import update_release as publisher
import package_runtime as runtime
import release_signing as signing
import verify_native_release as release
from release_gpg_vault_tests import VaultTests
from release_build_tests import ReleaseBuildTests
import release_build_fixture


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

    def package(self, generator, signed=True, use_vault=False, vault_failure=False, tamper_library=False, release_cohort=None,
                previous_msi_bytes=b"prior actual MSI fixture"):
        build = self.root / ("build-" + generator)
        build.mkdir()
        system = "Windows" if generator == "WIX" else "Linux"
        target = "" if generator == "WIX" else "linux-ubuntu-26.04-x64" if generator == "DEB" else "linux-rhel-10-x64"
        info = {"platform": system, "version": "1.2.3", "target": target,
                "families": {"agent_integrations": False, "local_automation": True}, "variant": "-agent-free"}
        if release_cohort is not None:
            info["releaseCohort"] = release_cohort
        image_suffix = ".exe" if system == "Windows" else ""
        app, helper = build / ("better_pull_review" + image_suffix), build / ("better_pull_review_agent_supervisor" + image_suffix)
        info.update(app=str(app), helpers=[str(helper)])
        library = build / ("bpr_xdiff.dll" if system == "Windows" else "libbpr_xdiff.so")
        libraries = [build / name for name in runtime.PRIVATE_LIBRARIES[system]]
        info["private_libraries"] = {path.name: str(path) for path in libraries}
        source = runtime.source_asset_path(build, info, self.revision)
        source.parent.mkdir(parents=True)
        with zipfile.ZipFile(source, "w") as archive:
            archive.writestr("Example-1.zip", b"source")
        binding = runtime.source_provenance(info, self.revision, runtime.digest(source))
        output = self.root / ("output-" + generator)
        extension = {"WIX": ".msi", "DEB": ".deb", "RPM": ".rpm"}[generator]
        base = "BetterPullReview-" + runtime.asset_version(info) + "-" + ("windows-x64" if generator == "WIX" else target) + "-agent-free"
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
                for path in libraries:
                    path.write_bytes(b"library")
                return
            self.events.append("cpack")
            installed = output / "_CPack_Packages/runtime"
            legal = installed / "ThirdParty"
            legal.mkdir(parents=True)
            for image in (app, helper, *libraries):
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
        def identity(path, *, msi=False):
            release = release_cohort["allocation"]
            if msi and path.name == "previous.msi":
                return {"ProductName": "BetterPullReview", "ProductVersion": "0.4.0", "ProductCode": "{AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA}", "ALLUSERS": "1", "UpgradeCode": "{18DF84A8-5883-4979-B1C6-6F226934AA68}"}
            if msi:
                return {"ProductName": "BetterPullReview", "ProductVersion": release["native"]["msiProductVersion"], "ProductCode": release["native"]["msiProductCode"], "ALLUSERS": "1", "UpgradeCode": "{18DF84A8-5883-4979-B1C6-6F226934AA68}",
                        "BPR_MARKETING_VERSION": "1.2.3", "BPR_RELEASE_BUILD": str(release["number"]), "BPR_RELEASE_COHORT": release_cohort["cohortDigest"], "BPR_RELEASE_CONFIGURATION": release["configurationDigest"]}
            return {"fileVersion": release["native"]["windowsImageVersion"], "productVersion": release["native"]["windowsImageVersion"]}
        def runtime_command(*args):
            if args[0] == "dpkg-deb":
                return release_cohort["allocation"]["native"]["debianVersion"]
            return "" if args[-1] == "--porcelain" else self.revision
        argv = ["package.py", "--build", str(build), "--output", str(output), "--generator", generator]
        if release_cohort is not None and system == "Windows":
            previous = build / "previous.msi"
            previous.write_bytes(previous_msi_bytes)
            argv += ["--previous-msi", str(previous)]
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
              patch.object(runtime, "run", side_effect=runtime_command),
              patch.object(runtime, "linux_baseline", return_value={"distribution": ["ubuntu" if generator == "DEB" else "rhel", "fixture"]}),
              patch.object(producer.subprocess, "run", side_effect=native), patch.object(signing, "sign_windows", side_effect=windows),
              patch.object(signing, "windows_identity", side_effect=identity), patch.object(signing, "authenticode"), patch.object(signing, "sign_detached", side_effect=detached),
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
                    self.assertEqual(self.events, ["build", "sign:better_pull_review.exe", "sign:better_pull_review_agent_supervisor.exe", "sign:bpr_xdiff.dll", "sign:mimalloc.dll",
                                                   "cpack", "sign:" + manifest.name.removesuffix(".release.json")])
                if generator == "RPM":
                    record = json.loads(manifest.read_text())
                    self.assertTrue((manifest.parent / record["installer"]).read_bytes().endswith(b"rpm-signed"))

    def test_unsigned_candidate_cannot_enter_publisher(self):
        manifest = self.package("WIX", signed=False)
        self.assertFalse(manifest.exists())

    def test_same_version_substitute_prior_msi_refuses_before_build_or_signing(self):
        frozen = release_build_fixture.cohort("1.2.3", self.revision)
        with self.assertRaisesRegex(ValueError, "frozen upgrade identity"):
            self.package("WIX", release_cohort=frozen, previous_msi_bytes=b"same native version but different installer")
        self.assertEqual(self.events, [])
        self.assertFalse((self.root / "output-WIX").exists())

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
        release_cohort = release_build_fixture.cohort("1.2.3", self.revision)
        manifest = self.package("DEB", release_cohort=release_cohort)
        windows = self.package("WIX", release_cohort=release_cohort)
        repo = self.root / "repository"
        for name in ("tools/publish-release.sh", "packaging/package_runtime.py", "packaging/verify_native_release.py"):
            destination = repo / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        release_build_fixture.checker_bridge(repo)
        cohort_path = self.root / "cohort.json"
        runtime.write_json(cohort_path, release_cohort)
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
    if args == ['remote', 'get-url', '--push', '--all', 'origin']: print('https://github.com/MikeMarcin/BetterPullReview.git')
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
                   "--manifest", str(manifest), "--manifest", str(windows), "--prerelease", "--cohort", str(cohort_path)]
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
        self.assertIn("identity/visibility does not match", result.stderr)
        log.unlink()
        result = subprocess.run(command + ["--public"], env={**environment, "FIXTURE_VISIBILITY": "PUBLIC"}, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        upload = next(item for item in map(json.loads, log.read_text().splitlines()) if item[:3] == ["gh", "release", "create"])
        self.assertEqual(set(upload[4:upload.index("--repo")]), expected)
        log.unlink()
        saved = windows.read_bytes()
        changed = json.loads(saved)
        changed["releaseCohort"] = release_build_fixture.cohort("1.2.3", self.revision, 2)
        windows.write_text(json.dumps(changed))
        result = subprocess.run(command + ["--dry-run"], env=environment, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("mixed native release cohorts", result.stderr)
        self.assertFalse(any(item[0] == "gh" for item in map(json.loads, log.read_text().splitlines())))
        windows.write_bytes(saved)
        log.unlink()
        record = json.loads(manifest.read_text())
        (manifest.parent / record["installer"]).write_bytes(b"tampered")
        result = subprocess.run(command, env=environment, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(item[0] == "gh" for item in map(json.loads, log.read_text().splitlines())))


SOURCE = "a" * 40
TAG = "refs/tags/v0.4.0"
ROW = {"sha256": "b" * 64, "bytes": 3}


class ExistingReleaseTests(unittest.TestCase):
    def release(self):
        return {"id": 42, "tag_name": "v0.4.0", "draft": False, "prerelease": True, "immutable": False,
                "html_url": "https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.4.0", "assets": []}

    def test_only_missing_assets_are_selected_and_old_bytes_are_preserved(self):
        baseline = {"mac.zip": ROW}
        selected = {"win.msi": ROW}
        self.assertEqual(publisher.check_assets(baseline, baseline, selected, complete=False), ["win.msi"])
        complete = baseline | selected
        self.assertEqual(publisher.check_assets(complete, baseline, selected, complete=True), [])
        for observed in [{}, {"mac.zip": ROW | {"sha256": "c" * 64}}, complete | {"unowned": ROW}]:
            with self.assertRaises(ValueError):
                publisher.check_assets(observed, baseline, selected, complete=False)
        with self.assertRaises(ValueError):
            publisher.check_assets(baseline, baseline, {"mac.zip": ROW | {"bytes": 4}}, complete=False)
        with self.assertRaises(ValueError):
            publisher.check_assets(baseline, baseline, selected, complete=True)

    def test_unknown_digest_duplicate_or_traversal_refuses(self):
        for rows in [[{"name": "../outside", **ROW}], [{"name": "x", **ROW}] * 2, [{"name": "x", **ROW, "sha256": "bad"}]]:
            with self.assertRaises(ValueError):
                publisher.inventory(rows)
        release = self.release();release["assets"] = [{"name": "x", "size": 3, "digest": None}]
        with self.assertRaises(ValueError):
            publisher.remote_inventory(release)

    def test_remote_guards_refuse_wrong_destination_visibility_immutability_source_and_state(self):
        options = {"origin": "https://github.com/MikeMarcin/BetterPullReview.git", "visibility": "PUBLIC", "immutable": False, "source": SOURCE, "prerelease": True, "id": 42}
        def command(*args, **kwargs):
            if args[:3] == ("git", "remote", "get-url"): return options["origin"]
            if args[:3] == ("gh", "repo", "view"): return json.dumps({"nameWithOwner": publisher.REPOSITORY, "visibility": options["visibility"]})
            if args[:3] == ("gh", "api", "-H"): return json.dumps({"enabled": options["immutable"], "enforced_by_owner": False})
            if args[:3] == ("git", "ls-remote", "--tags"): return "f" * 40 + "\t" + TAG + "\n" + options["source"] + "\t" + TAG + "^{}"
            if args[:2] == ("gh", "api"): return json.dumps(self.release() | {"prerelease": options["prerelease"], "id": options["id"]})
            raise AssertionError(args)
        with patch.object(publisher, "command", side_effect=command):
            self.assertEqual(publisher.guard(ROOT, SOURCE, "0.4.0", "PUBLIC", 42)["id"], 42)
            for key, bad in [("origin", "https://github.com/other/repo.git"), ("origin", options["origin"] + "\n" + options["origin"]), ("visibility", "PRIVATE"), ("immutable", True), ("source", "c" * 40), ("prerelease", False), ("id", 43)]:
                old = options[key];options[key] = bad
                with self.subTest(key=key), self.assertRaises(ValueError):
                    publisher.guard(ROOT, SOURCE, "0.4.0", "PUBLIC", 42)
                options[key] = old

    def test_append_uses_upload_without_clobber_or_tag_mutation_and_preserves_latest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifact = root / "win.msi";artifact.write_bytes(b"new")
            selected = {"win.msi": {"sha256": publisher.runtime.digest(artifact), "bytes": 3}}
            baseline = {"mac.zip": ROW}
            record = {"source": SOURCE, "version": "0.4.0", "repository": publisher.REPOSITORY, "assets": [{"name": name, **row} for name, row in baseline.items()]}
            baseline_file = root / "baseline.json";baseline_file.write_text(json.dumps(record))
            notes = root / "notes.md";notes.write_text("Qualification pending")
            release = self.release();release["assets"] = [{"name": name, "size": row["bytes"], "digest": "sha256:" + row["sha256"]} for name, row in baseline.items()]
            mutations = []
            def command(*args, **kwargs):
                if args[:3] == ("git", "status", "--porcelain"): return ""
                if args[:3] == ("git", "branch", "--show-current"): return "main"
                if args[:3] == ("git", "diff", "--name-only"): return "packaging/update_release.py"
                if args[:3] == ("git", "rev-parse", "HEAD"): return "d" * 40
                if args[:2] == ("gh", "api"): return json.dumps(release)
                raise AssertionError(args)
            def run(args, **kwargs):
                if args[:2] == ["git", "merge-base"]: return subprocess.CompletedProcess(args, 0)
                mutations.append(args)
                if args[:3] == ["gh", "release", "upload"]:
                    self.assertNotIn("--clobber", args)
                    release["assets"].append({"name": artifact.name, "size": 3, "digest": "sha256:" + selected[artifact.name]["sha256"]})
                else:
                    self.assertEqual(args[:3], ["gh", "release", "edit"])
                    self.assertNotIn("--latest=true", args)
                    self.assertNotIn("--prerelease=false", args)
                return subprocess.CompletedProcess(args, 0)
            argv = ["update_release.py", "--repo-root", str(root), "--version", "0.4.0", "--source-revision", SOURCE, "--visibility", "PUBLIC", "--notes-file", str(notes), "--baseline-inventory", str(baseline_file)]
            with patch.object(sys, "argv", argv), patch.object(publisher, "command", side_effect=command), patch.object(publisher, "selected_assets", return_value=({artifact.name: artifact}, {"Windows"})), patch.object(publisher, "guard", side_effect=lambda *a, **k: copy.deepcopy(release)), patch.object(publisher.subprocess, "run", side_effect=run), contextlib.redirect_stdout(io.StringIO()):
                publisher.main()
            self.assertEqual([x[:3] for x in mutations], [["gh", "release", "upload"], ["gh", "release", "edit"]])
            self.assertEqual(publisher.remote_inventory(release)["mac.zip"], ROW)
            self.assertTrue(release["prerelease"])



class PromotionAcceptanceTests(unittest.TestCase):
    def test_numbered_rc_promotion_refuses_before_any_remote_or_asset_access(self):
        with patch.object(sys, "argv", ["update_release.py", "--repo-root", str(ROOT), "--version", "0.4.0", "--source-revision", SOURCE,
                                       "--visibility", "PUBLIC", "--notes-file", "unused", "--baseline-inventory", "unused",
                                       "--cohort", "unused", "--promote"]), patch.object(publisher, "command") as remote:
            with self.assertRaisesRegex(ValueError, "cannot be promoted"):
                publisher.main()
            remote.assert_not_called()

    def test_pending_stable_qualification_refuses_before_remote_access(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline = root / "baseline.json"
            baseline.write_text(json.dumps({"source": SOURCE, "version": "0.4.0", "repository": publisher.REPOSITORY, "assets": [{"name": "mac.zip", **ROW}]}))
            notes = root / "notes.md";notes.write_text("pending")
            acceptance = root / "acceptance.json"
            acceptance.write_text(json.dumps({"schema": "bpr.release-promotion-acceptance.v1", "accepted": False, "source": SOURCE, "version": "0.4.0", "remainingBlocking": ["qualification pending"]}))
            artifact = root / "mac.zip";artifact.write_bytes(b"new")
            argv = ["update_release.py", "--repo-root", str(root), "--version", "0.4.0", "--source-revision", SOURCE, "--visibility", "PUBLIC", "--notes-file", str(notes), "--baseline-inventory", str(baseline), "--promote", "--promotion-acceptance", str(acceptance)]
            def command(*args, **kwargs):
                if args[:3] == ("git", "status", "--porcelain"): return ""
                if args[:3] == ("git", "branch", "--show-current"): return "main"
                if args[:3] == ("git", "diff", "--name-only"): return "packaging/update_release.py"
                raise AssertionError(args)
            with patch.object(sys, "argv", argv), patch.object(publisher, "command", side_effect=command), patch.object(publisher, "selected_assets", return_value=({"mac.zip": artifact}, {"Darwin", "Windows", "Linux"})), patch.object(publisher.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)), patch.object(publisher, "guard") as remote:
                with self.assertRaisesRegex(ValueError, "stable qualification"):
                    publisher.main()
                remote.assert_not_called()


class StableCopyTests(unittest.TestCase):
    def setUp(self):
        temporary = ContainedTemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.notes = self.root / "notes.md"
        self.notes.write_text("Synthetic unit fixture; no publication authority")
        self.cohort = release_build_fixture.cohort("0.4.0", SOURCE, 1)
        cohort_file = self.root / "cohort.json"
        cohort_file.write_text(json.dumps(self.cohort))
        self.paths = {}
        for name in ["mac.zip", "win.msi", "linux.deb"]:
            path = self.root / name
            path.write_bytes(name.encode())
            self.paths[name] = path
        self.selected = {name: {"sha256": runtime.digest(path), "bytes": path.stat().st_size} for name, path in self.paths.items()}
        self.baseline = {"mac.zip": self.selected["mac.zip"]}
        self.source_tag = self.cohort["allocation"]["native"]["releaseTag"]
        self.source = {"id": 42, "assets": self.remote_assets(self.selected)}
        self.args = SimpleNamespace(cohort=cohort_file, version="0.4.0", source_revision=SOURCE, visibility="PUBLIC", promote=False,
                                    notes_file=self.notes, stable_acceptance=None, publication_authorization=None, resume_stable=None, dry_run=False)
        self.platforms = {"Darwin", "Windows", "Linux"}
        self.mutations = []
        self.destination = {"id": 43, "tag_name": "0.4.0", "draft": True, "prerelease": True, "immutable": False,
                            "html_url": "https://github.com/MikeMarcin/BetterPullReview/releases/tag/0.4.0", "assets": [], "body": self.notes.read_text()}

    @staticmethod
    def remote_assets(rows):
        return [{"name": name, "size": row["bytes"], "digest": "sha256:" + row["sha256"]} for name, row in rows.items()]

    def permissions(self):
        for option, schema in [("stable_acceptance", "bpr.release-stable-copy-acceptance.v1"), ("publication_authorization", "bpr.release-publication-authorization.v1")]:
            path = self.root / (option + ".json")
            path.write_text(json.dumps({"schema": schema, "source": SOURCE, "version": "0.4.0", "tag": "0.4.0", "sourceTag": self.source_tag,
                                        "cohortDigest": self.cohort["cohortDigest"], "assets": [{"name": name, **row} for name, row in self.selected.items()],
                                        "accepted": True, "remainingBlocking": [], "evidence": [{"path": str(self.notes), "sha256": runtime.digest(self.notes)}]}))
            setattr(self.args, option, path)

    def command(self, *args, **kwargs):
        if args[:2] == ("git", "rev-parse"):
            return "f" * 40
        if args[:2] == ("gh", "api"):
            if "/releases/tags/0.4.0" in args[-1]:
                raise subprocess.CalledProcessError(1, args, stderr="GitHub omits drafts from tag lookup: 404")
            if "--slurp" in args:
                return json.dumps([[self.destination]])
            return json.dumps(self.destination)
        raise AssertionError(args)

    def run_stub(self, args, **kwargs):
        self.mutations.append(args)
        if args[:3] == ["gh", "release", "create"]:
            self.assertIn("--draft", args)
            self.assertIn("--prerelease", args)
            self.assertIn("--latest=false", args)
        elif args[:3] == ["gh", "release", "upload"]:
            self.assertNotIn("--clobber", args)
            name = Path(args[4]).name
            self.destination["assets"].extend(self.remote_assets({name: self.selected[name]}))
        elif args[:3] == ["gh", "release", "edit"]:
            self.assertEqual(publisher.remote_inventory(self.destination), self.selected)
            self.assertTrue(self.destination["draft"] and self.destination["prerelease"])
            self.assertIn("--draft=false", args)
            self.assertIn("--prerelease=false", args)
            self.assertIn("--latest=true", args)
            self.destination.update(draft=False, prerelease=False)
        else:
            self.assertEqual(args[:2], ["git", "tag"] if "tag" in args else ["git", "push"])
            self.assertNotIn("--force", args)
            self.assertNotIn("main", args)
        return subprocess.CompletedProcess(args, 0)

    def invoke(self, *, run=None):
        with patch.object(publisher, "guard", side_effect=lambda *a, **k: copy.deepcopy(self.source)), \
             patch.object(publisher, "stable_tag_guard"), patch.object(publisher, "release_absent"), \
             patch.object(publisher, "command", side_effect=self.command), patch.object(publisher.subprocess, "run", side_effect=run or self.run_stub), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            publisher.copy_stable(self.args, self.root, self.source_tag, copy.deepcopy(self.baseline), self.paths, self.platforms, self.selected)
        return output.getvalue()

    def test_stable_copy_remains_draft_until_every_exact_asset_is_verified(self):
        self.permissions()
        self.invoke()
        self.assertEqual([row[:3] for row in self.mutations if row[:2] == ["gh", "release"]],
                         [["gh", "release", "create"]] + [["gh", "release", "upload"]] * 3 + [["gh", "release", "edit"]])
        self.assertEqual(publisher.remote_inventory(self.source), self.selected)

    def resume_checkpoint(self):
        self.permissions()
        self.args.resume_stable = self.root / "resume.json"
        self.args.resume_stable.write_text(json.dumps({"schema": "bpr.release-stable-copy-resume.v1", "source": SOURCE,
            "version": "0.4.0", "tag": "0.4.0", "sourceTag": self.source_tag, "cohortDigest": self.cohort["cohortDigest"],
            "releaseId": 43, "tagObject": "f" * 40,
            "assets": [{"name": name, **row} for name, row in publisher.remote_inventory(self.destination).items()]}))

    def test_inspected_partial_draft_resumes_without_new_tag_release_or_replacement(self):
        self.destination["assets"] = self.remote_assets({"mac.zip": self.selected["mac.zip"]})
        self.resume_checkpoint()
        self.invoke()
        self.assertEqual([row[:3] for row in self.mutations], [["gh", "release", "upload"]] * 2 + [["gh", "release", "edit"]])

    def test_resume_refuses_changed_source_draft_id_unknown_assets_and_changed_snapshot(self):
        self.resume_checkpoint()
        original = self.args.resume_stable.read_text()
        for change in ["source", "draft-id", "unknown-asset", "changed-snapshot"]:
            checkpoint = json.loads(original)
            self.destination["assets"] = []
            self.destination["id"] = 43
            if change == "source": checkpoint["source"] = "c" * 40
            elif change == "draft-id": self.destination["id"] = 44
            elif change == "unknown-asset": checkpoint["assets"] = [{"name": "unselected.zip", **ROW}]
            else: self.destination["assets"] = self.remote_assets({"mac.zip": self.selected["mac.zip"]})
            self.args.resume_stable.write_text(json.dumps(checkpoint))
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.invoke()
            self.assertEqual(self.mutations, [])

    def test_resume_refuses_changed_draft_notes(self):
        self.resume_checkpoint()
        self.destination["body"] = "foreign draft notes"
        with self.assertRaisesRegex(ValueError, "draft notes changed"):
            self.invoke()
        self.assertEqual(self.mutations, [])

    def test_dry_run_reports_missing_source_assets_and_no_authority_without_mutation(self):
        self.args.dry_run = True
        self.source["assets"] = self.remote_assets({"mac.zip": self.selected["mac.zip"]})
        result = json.loads(self.invoke())
        self.assertEqual(result["missingSourceRCAssets"], ["linux.deb", "win.msi"])
        self.assertFalse(result["readyToPublish"])
        self.assertFalse(result["publicationAuthorized"])
        self.assertEqual(result["externalMutations"], 0)
        self.assertEqual(self.mutations, [])

    def test_incomplete_source_rc_and_missing_authorization_refuse_before_tag_creation(self):
        with self.assertRaisesRegex(ValueError, "specific publication authorization"):
            self.invoke()
        self.permissions()
        self.source["assets"] = self.remote_assets({"mac.zip": self.selected["mac.zip"]})
        with self.assertRaisesRegex(ValueError, "complete source RC"):
            self.invoke()
        self.assertEqual(self.mutations, [])

    def test_wrong_source_mixed_cohort_and_changed_permission_asset_refuse(self):
        self.args.source_revision = "c" * 40
        with self.assertRaisesRegex(ValueError, "cohort source"):
            self.invoke()
        self.args.source_revision = SOURCE
        self.permissions()
        saved = self.args.stable_acceptance.read_text()
        for change in ["cohort", "asset"]:
            record = json.loads(saved)
            if change == "cohort": record["cohortDigest"] = "c" * 64
            else: record["assets"][0]["sha256"] = "c" * 64
            self.args.stable_acceptance.write_text(json.dumps(record))
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, "identity or assets differ"):
                self.invoke()
        self.assertEqual(self.mutations, [])

    def test_changed_upload_and_failed_upload_never_mark_latest(self):
        self.permissions()
        def failed(args, **kwargs):
            if args[:3] == ["gh", "release", "upload"]:
                raise subprocess.CalledProcessError(1, args)
            return self.run_stub(args, **kwargs)
        with self.assertRaises(subprocess.CalledProcessError): self.invoke(run=failed)
        self.assertFalse(any(row[:3] == ["gh", "release", "edit"] for row in self.mutations))
        self.mutations.clear()
        self.paths["linux.deb"].write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "stable asset changed"):
            self.invoke()
        self.assertFalse(any(row[:3] == ["gh", "release", "edit"] for row in self.mutations))

    def test_occupied_local_or_remote_tag_and_release_refuse(self):
        with patch.object(publisher.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)), self.assertRaisesRegex(ValueError, "local stable tag already exists"):
            publisher.stable_tag_guard(self.root, "0.4.0", SOURCE)
        with patch.object(publisher.subprocess, "run", return_value=subprocess.CompletedProcess([], 1)), \
             patch.object(publisher, "command", return_value="f" * 40 + "\trefs/tags/0.4.0"), self.assertRaisesRegex(ValueError, "remote stable tag occupied"):
            publisher.stable_tag_guard(self.root, "0.4.0", SOURCE)
        with patch.object(publisher.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, stdout="{}")), self.assertRaisesRegex(ValueError, "already exists"):
            publisher.release_absent("0.4.0")

    def test_already_published_destination_refuses_without_upload_or_latest_edit(self):
        self.permissions()
        self.destination.update(draft=False, prerelease=False)
        with self.assertRaisesRegex(ValueError, "identity or state changed"):
            self.invoke()
        self.assertFalse(any(row[:3] in [["gh", "release", "upload"], ["gh", "release", "edit"]] for row in self.mutations))

    def test_tool_revision_cannot_change_producer_paths(self):
        baseline = self.root / "baseline.json"
        baseline.write_text(json.dumps({"source": SOURCE, "version": "0.4.0", "repository": publisher.REPOSITORY,
                                        "tag": self.source_tag, "assets": [{"name": name, **row} for name, row in self.baseline.items()]}))
        argv = ["update_release.py", "--repo-root", str(self.root), "--version", "0.4.0", "--source-revision", SOURCE,
                "--visibility", "PUBLIC", "--notes-file", str(self.notes), "--baseline-inventory", str(baseline),
                "--cohort", str(self.args.cohort), "--copy-stable", "--dry-run"]
        def command(*args, **kwargs):
            if args[:3] == ("git", "status", "--porcelain"): return ""
            if args[:3] == ("git", "branch", "--show-current"): return "main"
            if args[:3] == ("git", "diff", "--name-only"): return "src/core/src/release_update.cpp"
            raise AssertionError(args)
        with patch.object(sys, "argv", argv), patch.object(publisher, "command", side_effect=command), \
             patch.object(publisher.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)), patch.object(publisher, "guard") as remote:
            with self.assertRaisesRegex(ValueError, "changes producer inputs"):
                publisher.main()
            remote.assert_not_called()


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
