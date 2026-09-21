#!/usr/bin/env python3
"""Offline publication preflight for the exact source sidecar referenced by a macOS ZIP."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

import package_runtime as runtime


def verify(app_zip: Path, sbom: Path, source_archive: Path, version: str, revision: str) -> dict:
    info = {"platform": "Darwin", "version": version}
    inventory = json.loads(sbom.read_text(encoding="utf-8"))
    products = [item for item in inventory["packages"] if item["SPDXID"] == "SPDXRef-BetterPullReview"]
    if len(products) != 1 or products[0]["versionInfo"] != version or products[0]["sourceInfo"] != f"git:{revision}; clean=True; tests-disabled Release; Darwin":
        raise ValueError("release source producer identity mismatch")
    with zipfile.ZipFile(app_zip) as archive:
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
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-revision", required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.app_zip, args.sbom, args.source_archive, args.version, args.source_revision), sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit(f"release source preflight refused: {error}") from error
