#!/usr/bin/env python3
"""Build an unpublished Windows MSI or baseline Linux DEB/RPM from clean source."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tarfile

import package_runtime as runtime


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="new, empty artifact directory")
    parser.add_argument("--generator", choices=("WIX", "DEB", "RPM"), required=True)
    args = parser.parse_args()
    build = args.build.resolve(strict=True)
    info = runtime.inputs(build)
    source = runtime.ROOT
    if runtime.run("git", "-C", source, "status", "--porcelain").strip():
        raise ValueError("commit source before packaging")
    revision = runtime.run("git", "-C", source, "rev-parse", "HEAD").strip()
    if info["platform"] == "Darwin":
        raise ValueError("use tools/release.sh for macOS")
    if info["platform"] == "Windows" and args.generator != "WIX":
        raise ValueError("Windows packages use the per-user x64 WiX definition")
    if info["platform"] == "Linux":
        baseline = runtime.linux_baseline()
        expected = "DEB" if baseline["distribution"][0] == "ubuntu" else "RPM"
        if args.generator != expected:
            raise ValueError(f"this native baseline produces {expected}, not {args.generator}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    subprocess.run(["cmake", "--build", str(build), "--target", "app_better_pull_review"], check=True)
    subprocess.run(["cpack", "--config", str(build / "CPackConfig.cmake"), "-G", args.generator, "-B", str(output)], cwd=build, check=True)
    extension = {"WIX": ".msi", "DEB": ".deb", "RPM": ".rpm"}[args.generator]
    artifacts = list(output.glob("*" + extension))
    inventories = list((output / "_CPack_Packages").rglob("sbom.spdx.json"))
    if len(artifacts) != 1 or len(inventories) != 1:
        raise ValueError("exactly one package and installed runtime inventory are required")
    if runtime.run("git", "-C", source, "rev-parse", "HEAD").strip() != revision or runtime.run("git", "-C", source, "status", "--porcelain").strip():
        raise ValueError("source changed during package production; artifacts are not accepted")
    artifact = artifacts[0]
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
        directory = f"BetterPullReview-{info['version']}-linux-x64{info.get('variant', '')}"
        archive = output / (directory + "-user.tar.gz")
        with tarfile.open(archive, "w:gz") as stream:
            stream.add(inventories[0].parent, arcname=directory)
        products.append(archive)
    (output / (artifact.name + ".sha256")).write_text("".join(f"{runtime.digest(path)}  {path.name}\n" for path in products), encoding="utf-8")
    runtime.write_json(output / (artifact.name + ".provenance.json"), {"schema": "bpr.package-provenance.v1", "sourceCommit": revision,
                       "platform": info["platform"], "generator": args.generator, "families": runtime.families(info),
                       "artifactSHA256": runtime.digest(artifact), "sbomSHA256": runtime.digest(sbom),
                       "products": {path.name: runtime.digest(path) for path in products},
                       "signed": False, "published": False, "nativeAcceptance": "pending"})
    print(f"Unpublished package: {artifact}; signing and native install/upgrade/uninstall qualification are separate gates.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"package refused: {error}") from error
