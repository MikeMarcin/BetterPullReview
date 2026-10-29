#!/usr/bin/env python3
"""Offline publication preflight for the exact source sidecar referenced by a macOS ZIP."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

import package_runtime as runtime


def verify(app_zip: Path, sbom: Path, source_archive: Path, version: str, revision: str, cohort: dict | None = None, legacy_zip: Path | None = None) -> dict:
    info = {"platform": "Darwin", "version": version, **({"releaseCohort": cohort} if cohort else {})}
    receipt = runtime.release_identity(info, revision)
    if receipt:
        expected_name = f"BetterPullReview-{runtime.asset_version(info)}-macos-universal.zip"
        alias_name = f"BetterPullReview-{version}-macos-universal.zip"
        if (app_zip.name != expected_name or legacy_zip is None or legacy_zip.name != alias_name
                or legacy_zip.is_symlink() or runtime.digest(legacy_zip) != runtime.digest(app_zip)
                or legacy_zip.with_name(alias_name + ".sha256").read_text().strip() != runtime.digest(app_zip) + "  " + alias_name):
            raise ValueError("numbered macOS release requires its exact byte-identical legacy updater alias")
    inventory = json.loads(sbom.read_text(encoding="utf-8"))
    products = [item for item in inventory["packages"] if item["SPDXID"] == "SPDXRef-BetterPullReview"]
    if len(products) != 1 or products[0]["versionInfo"] != version or products[0]["sourceInfo"] != runtime.product_source_info(info, revision, True):
        raise ValueError("release source producer identity mismatch")
    with zipfile.ZipFile(app_zip) as archive:
        if receipt:
            import plistlib
            identity = plistlib.loads(archive.read("BetterPullReview.app/Contents/Info.plist"))
            if (identity.get("CFBundleShortVersionString") != version
                    or identity.get("CFBundleVersion") != str(receipt["number"])
                    or identity.get("BPRReleaseCohort") != cohort["cohortDigest"]
                    or identity.get("BPRReleaseConfiguration") != receipt["configurationDigest"]):
                raise ValueError("macOS native release cohort mismatch")
        if any("corresponding-source" in name and name.endswith(".zip") for name in archive.namelist()):
            raise ValueError("corresponding source is embedded in the application ZIP")
        material = {}
        for name in ("source-provenance.json", "SOURCE-OFFERS.txt", "THIRD-PARTY-NOTICES.txt"):
            relative = "Contents/Resources/ThirdParty/" + name
            member = "BetterPullReview.app/" + relative
            if archive.namelist().count(member) != 1:
                raise ValueError("release source material missing or duplicated")
            data = archive.read(member)
            entries = [item for item in inventory["files"] if item["fileName"] == "./" + relative]
            if len(entries) != 1 or entries[0]["checksums"] != [{"algorithm": "SHA256", "checksumValue": hashlib.sha256(data).hexdigest()}]:
                raise ValueError("release source material differs from SPDX")
            material[name] = data.decode("utf-8")
    provenance = runtime.check_source_provenance(json.loads(material["source-provenance.json"]), info, revision)
    runtime.check_source_references(provenance, material)
    runtime.verify_source_asset(source_archive, provenance)
    return {"result": "RELEASE_SOURCE_ASSET_OK", "asset": provenance["asset"], "source_revision": revision,
            "legal_approval": "pending separately; offline preflight cannot verify recipient access or retention"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app-zip", type=Path, required=True)
    parser.add_argument("--sbom", type=Path, required=True)
    parser.add_argument("--source-archive", type=Path, required=True)
    parser.add_argument("--cohort", type=Path)
    parser.add_argument("--legacy-zip", type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-revision", required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.app_zip, args.sbom, args.source_archive, args.version, args.source_revision, runtime.release_build.read(args.cohort) if args.cohort else None, args.legacy_zip), sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit(f"release source preflight refused: {error}") from error
