#!/usr/bin/env python3
"""Build, optionally sign, and finalize unpublished native packages from clean source."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tarfile
import zipfile

import package_runtime as runtime
import release_signing as signing
import release_gpg_vault as vault


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="fresh artifact directory; default build/releases/VERSION-build.N for numbered RCs")
    parser.add_argument("--generator", choices=("WIX", "DEB", "RPM"), required=True)
    parser.add_argument("--signtool", type=Path)
    parser.add_argument("--azure-dlib", type=Path)
    parser.add_argument("--azure-metadata", type=Path)
    parser.add_argument("--previous-msi", type=Path, help="actual prior per-machine MSI; required for a numbered Windows cohort")
    parser.add_argument("--publisher", help="approved exact certificate publisher, e.g. Michael Marcin")
    parser.add_argument("--gpg-key", help="approved full OpenPGP fingerprint; already enrolled locally")
    parser.add_argument("--gpg-vault-config", type=Path,
                        help="absolute approved Windows/WSL vault config; unlock automatically without pinentry")
    args = parser.parse_args()
    build = args.build.resolve(strict=True)
    info = runtime.inputs(build)
    source = runtime.ROOT
    if runtime.run("git", "-C", source, "status", "--porcelain").strip():
        raise ValueError("commit source before packaging")
    revision = runtime.run("git", "-C", source, "rev-parse", "HEAD").strip()
    release = runtime.release_identity(info, revision)
    native_identity = {}
    if release and info["platform"] == "Windows":
        if not args.previous_msi or not args.previous_msi.is_file():
            raise ValueError("numbered Windows production requires the actual --previous-msi before building/signing")
        previous = signing.windows_identity(args.previous_msi, msi=True)
        floor = release["windowsUpgradeFloor"]
        declared = runtime.release_build.floor_identity(floor)
        if (previous.get("ProductName") != "BetterPullReview" or previous.get("ALLUSERS") != "1"
                or previous.get("UpgradeCode") != "{18DF84A8-5883-4979-B1C6-6F226934AA68}"
                or previous.get("ProductVersion") != declared["msiProductVersion"]
                or previous.get("BPR_MARKETING_VERSION", previous.get("ProductVersion")) != declared["marketingVersion"]
                or runtime.digest(args.previous_msi) != floor["installerSHA256"]):
            raise ValueError("actual previous per-machine MSI differs from the frozen upgrade identity")
        runtime.release_build.native(info["version"], release["number"], declared)
        native_identity["previousMsiSHA256"] = runtime.digest(args.previous_msi)
        native_identity["previousWindows"] = declared

    if info["platform"] == "Darwin":
        raise ValueError("use tools/release.sh for macOS")
    if info["platform"] == "Windows" and args.generator != "WIX":
        raise ValueError("Windows packages use the per-machine x64 WiX definition")
    if release and info["platform"] == "Linux" and args.generator != "DEB":
        raise ValueError("numbered Linux release cohorts use the selected Debian package route")
    if info["platform"] == "Linux":
        baseline = runtime.linux_baseline()
        expected = "DEB" if baseline["distribution"][0] == "ubuntu" else "RPM"
        if args.generator != expected:
            raise ValueError(f"this native baseline produces {expected}, not {args.generator}")
        if info.get("target") != runtime.linux_target():
            raise ValueError("reconfigure the Linux build for its baseline-qualified release names")
    windows_options = (args.signtool, args.azure_dlib, args.azure_metadata, args.publisher)
    if any(windows_options) and (not all(windows_options) or info["platform"] != "Windows"):
        raise ValueError("Windows signing requires signtool, azure-dlib, azure-metadata and publisher together")
    if args.gpg_key and info["platform"] != "Linux":
        raise ValueError("OpenPGP signing is only supported for Linux packages")
    gpg_options = {}
    if args.gpg_vault_config:
        if info["platform"] != "Linux" or args.generator != "DEB":
            raise ValueError("Windows/WSL vault signing is only supported for the Ubuntu DEB producer")
        binding = vault.prepare_linux_signing(args.gpg_vault_config, args.gpg_key)
        args.gpg_key = binding["keyFingerprint"]
        gpg_options = {"gpg_home": Path(binding["gnupgHome"])}
    if args.gpg_key:
        args.gpg_key = signing.fingerprint(args.gpg_key)
    for path in (args.signtool, args.azure_dlib, args.azure_metadata):
        if path and not path.is_file():
            raise ValueError(f"signing input is absent: {path}")
    output = (args.output or source / "build/releases" / runtime.asset_version(info)).resolve()
    output.mkdir(parents=True, exist_ok=False)
    subprocess.run(["cmake", "--build", str(build), "--target", "app_better_pull_review"], check=True)
    signatures = {}
    producers = {}
    if all(windows_options):
        for image in [Path(info["app"]), *(Path(value) for value in info["helpers"]),
                      *runtime.private_library_producers(info, source_built_only=True)]:
            if image.name in producers:
                raise ValueError("duplicate first-party image name")
            receipt = signing.sign_windows(image, args.signtool, args.azure_dlib, args.azure_metadata, args.publisher)
            producers[image.name] = {"sha256": runtime.digest(image), **receipt}
    subprocess.run(["cpack", "--config", str(build / "CPackConfig.cmake"), "-G", args.generator, "-B", str(output)], cwd=build, check=True)
    extension = {"WIX": ".msi", "DEB": ".deb", "RPM": ".rpm"}[args.generator]
    artifacts = list(output.glob("*" + extension))
    inventories = list((output / "_CPack_Packages").rglob("sbom.spdx.json"))
    if len(artifacts) != 1 or len(inventories) != 1:
        raise ValueError("exactly one package and installed runtime inventory are required")
    if runtime.run("git", "-C", source, "rev-parse", "HEAD").strip() != revision or runtime.run("git", "-C", source, "status", "--porcelain").strip():
        raise ValueError("source changed during package production; artifacts are not accepted")
    artifact = artifacts[0]
    installed = inventories[0].parent
    if release:
        if info["platform"] == "Windows":
            observed = signing.windows_identity(artifact, msi=True)
            expected = {"ProductName": "BetterPullReview", "ProductVersion": release["native"]["msiProductVersion"], "ALLUSERS": "1",
                        "UpgradeCode": "{18DF84A8-5883-4979-B1C6-6F226934AA68}", "BPR_MARKETING_VERSION": info["version"],
                        "BPR_RELEASE_BUILD": str(release["number"]), "BPR_RELEASE_COHORT": info["releaseCohort"]["cohortDigest"],
                        "BPR_RELEASE_CONFIGURATION": release["configurationDigest"]}
            if observed != expected:
                raise ValueError("actual MSI product/cohort metadata differs from frozen identity")
            native_identity["msiProperties"] = observed
            expected_image = {"fileVersion": release["native"]["windowsImageVersion"], "productVersion": release["native"]["windowsImageVersion"]}
            native_identity["images"] = {}
            for image in [Path(info["app"]), *(Path(value) for value in info["helpers"])]:
                observed_image = signing.windows_identity(image)
                if observed_image != expected_image:
                    raise ValueError("actual PE cohort version differs: " + image.name)
                native_identity["images"][image.name] = observed_image
        else:
            observed = runtime.run("dpkg-deb", "-f", artifact, "Version").strip()
            if args.generator != "DEB" or observed != release["native"]["debianVersion"]:
                raise ValueError("actual DEB version differs from frozen cohort")
            native_identity["debianVersion"] = observed

    if all(windows_options):
        for name, receipt in producers.items():
            image = installed / name
            if runtime.digest(image) != receipt["sha256"]:
                raise ValueError("CPack changed a signed first-party image: " + name)
            signing.authenticode(image, args.signtool, args.publisher)
        signatures[artifact.name] = signing.sign_windows(artifact, args.signtool, args.azure_dlib, args.azure_metadata, args.publisher)
    if args.gpg_key and args.generator == "RPM":
        signing.sign_rpm(artifact, args.gpg_key)
        signing.verify_rpm(artifact, args.gpg_key)
    sbom = output / (artifact.name + ".spdx.json")
    shutil.copy2(inventories[0], sbom)
    provenance = runtime.installed_source_provenance(inventories[0].parent, info, revision)
    source_input = runtime.source_asset_path(build, info, revision)
    runtime.verify_source_asset(source_input, provenance)
    source_asset = output / provenance["asset"]["name"]
    shutil.copy2(source_input, source_asset)
    products = [artifact, sbom, source_asset]
    if info["platform"] == "Linux":
        # DEB/RPM remain corporate-managed system packages. The same restricted
        # runtime also supplies the default no-administrator per-user route.
        directory = artifact.stem
        archive = output / (directory + "-user.tar.gz")
        with tarfile.open(archive, "w:gz") as stream:
            stream.add(inventories[0].parent, arcname=directory)
        products.append(archive)
    else:
        archive = output / (artifact.stem + "-runtime.zip")
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as stream:
            for path in sorted(installed.rglob("*")):
                if path.is_file():
                    stream.write(path, path.relative_to(installed).as_posix())
        products.append(archive)
    if args.gpg_key:
        for path in list(products):
            signature, receipt = signing.sign_detached(path, args.gpg_key, **gpg_options)
            signatures[path.name] = {"signature": signature.name, **receipt}
            products.append(signature)
    checksums = output / (artifact.name + ".sha256")
    checksums.write_text("".join(f"{runtime.digest(path)}  {path.name}\n" for path in products), encoding="utf-8")
    provenance_file = output / (artifact.name + ".provenance.json")
    runtime.write_json(provenance_file, {"schema": "bpr.package-provenance.v1", "sourceCommit": revision, **runtime.release_fields(info), **({"nativeIdentity": native_identity} if release else {}),
                       "platform": info["platform"], "generator": args.generator, "families": runtime.families(info),
                       "artifactSHA256": runtime.digest(artifact), "sbomSHA256": runtime.digest(sbom),
                       "products": {path.name: runtime.digest(path) for path in products},
                       "signed": bool(signatures), "signatures": signatures, "firstPartyImages": producers,
                       "published": False, "nativeAcceptance": "pending"})
    if signatures:
        products.extend([checksums, provenance_file])
        if args.gpg_key:
            for path in (checksums, provenance_file):
                signature, receipt = signing.sign_detached(path, args.gpg_key, **gpg_options)
                signatures[path.name] = {"signature": signature.name, **receipt}
                products.append(signature)
        if runtime.run("git", "-C", source, "rev-parse", "HEAD").strip() != revision or runtime.run("git", "-C", source, "status", "--porcelain").strip():
            raise ValueError("source changed during signing; no release manifest will be emitted")
        runtime.write_json(output / (artifact.name + ".release.json"), {
            "schema": "bpr.native-release.v1", "version": info["version"], "sourceCommit": revision, **runtime.release_fields(info), **({"nativeIdentity": native_identity} if release else {}),
            "platform": info["platform"], "generator": args.generator, "families": runtime.families(info),
            "target": info.get("target", ""), "installer": artifact.name, "runtimeArchive": archive.name,
            "sbom": sbom.name, "sourceArchive": source_asset.name, "checksums": checksums.name,
            "provenance": provenance_file.name, "signatures": signatures, "firstPartyImages": producers,
            "products": {path.name: runtime.digest(path) for path in products},
            "nativeAcceptance": "pending; qualify the final installer separately"})
    print(f"Unpublished package: {artifact}; native install/upgrade/uninstall qualification remains separate.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"package refused: {error}") from error
