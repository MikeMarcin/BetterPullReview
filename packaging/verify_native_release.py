#!/usr/bin/env python3
"""Offline native asset preflight. Native trust and qualification remain separate gates."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import tarfile
import zipfile

import package_runtime as runtime


def safe_name(name: str) -> str:
    if not name or Path(name).name != name or "/" in name or "\\" in name or "\n" in name or "\r" in name or name in {".", ".."}:
        raise ValueError("invalid release asset name")
    return name


def relative_name(name: str) -> str:
    value = PurePosixPath(name)
    if value.is_absolute() or ".." in value.parts or "\\" in name:
        raise ValueError("unsafe runtime member")
    return value.as_posix()


def archive_contents(path: Path, platform: str) -> tuple[dict[str, str], dict[str, str]]:
    hashes, material = {}, {}
    def read(name, stream):
        name = relative_name(name)
        if name in hashes:
            raise ValueError("duplicate runtime member")
        if name in {"ThirdParty/source-provenance.json", "ThirdParty/SOURCE-OFFERS.txt", "ThirdParty/THIRD-PARTY-NOTICES.txt"}:
            data = stream.read()
            hashes[name] = hashlib.sha256(data).hexdigest()
            material[PurePosixPath(name).name] = data.decode("utf-8")
        else:
            hashes[name] = runtime.stream_digest(stream, "sha256")
    if platform == "Windows":
        with zipfile.ZipFile(path) as archive:
            for entry in archive.infolist():
                relative_name(entry.filename)
                if ":" in entry.filename:
                    raise ValueError("unsafe Windows runtime member")
                if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError("Windows runtime contains a symlink")
                if not entry.is_dir():
                    with archive.open(entry) as stream:
                        read(entry.filename, stream)
    else:
        with tarfile.open(path, "r:gz") as archive:
            entries = archive.getmembers()
            roots = {relative_name(entry.name).split("/")[0] for entry in entries}
            if roots != {path.name.removesuffix("-user.tar.gz")}:
                raise ValueError("Linux runtime requires its versioned root directory")
            prefix = next(iter(roots)) + "/"
            for entry in entries:
                if entry.isdir():
                    continue
                if not entry.name.startswith(prefix):
                    raise ValueError("invalid Linux runtime root")
                if entry.issym() or entry.islnk():
                    target = entry.linkname if entry.islnk() else posixpath.join(posixpath.dirname(entry.name), entry.linkname)
                    if target.startswith("/") or not posixpath.normpath(target).startswith(prefix):
                        raise ValueError("runtime link escapes archive")
                elif not entry.isfile():
                    raise ValueError("unexpected special runtime file")
                with archive.extractfile(entry) as stream:
                    read(entry.name[len(prefix):], stream)
    return hashes, material


def verify(manifest: Path, version: str, revision: str) -> list[Path]:
    record = json.loads(manifest.read_text(encoding="utf-8"))
    if record.get("schema") != "bpr.native-release.v1" or record.get("version") != version or record.get("sourceCommit") != revision:
        raise ValueError("native release version or producer mismatch")
    platform, generator = record["platform"], record["generator"]
    if (platform, generator) not in {("Windows", "WIX"), ("Linux", "DEB"), ("Linux", "RPM")}:
        raise ValueError("invalid native release platform/generator")
    info = {"platform": platform, "version": version, "families": runtime.families(record), "target": record["target"]}
    if platform == "Linux" and info["target"] != runtime.LINUX_TARGETS[generator]:
        raise ValueError("Linux baseline and generator mismatch")
    products = record["products"]
    paths = {}
    for name, digest in products.items():
        path = manifest.parent / safe_name(name)
        if not re.fullmatch(r"[a-f0-9]{64}", digest) or path.is_symlink() or not path.is_file() or runtime.digest(path) != digest:
            raise ValueError("native release asset digest mismatch: " + name)
        paths[name] = path
    for field in ("installer", "runtimeArchive", "sbom", "sourceArchive", "checksums", "provenance"):
        if record[field] not in paths:
            raise ValueError("missing required native asset: " + field)
    extension = {"WIX": ".msi", "DEB": ".deb", "RPM": ".rpm"}[generator]
    variant = ("" if info["families"]["agent_integrations"] else "-agent-free") + ("" if info["families"]["local_automation"] else "-automation-off")
    target = "windows-x64" if platform == "Windows" else info["target"]
    base = f"BetterPullReview-{version}-{target}{variant}"
    installer = base + extension
    if (record["installer"] != installer or manifest.name != installer + ".release.json"
            or record["runtimeArchive"] != base + ("-runtime.zip" if platform == "Windows" else "-user.tar.gz")
            or any(record[field] != installer + suffix for field, suffix in (("sbom", ".spdx.json"), ("checksums", ".sha256"), ("provenance", ".provenance.json")))):
        raise ValueError("native release asset names do not match its variant and baseline")
    provenance = json.loads(paths[record["provenance"]].read_text(encoding="utf-8"))
    if (provenance.get("schema") != "bpr.package-provenance.v1" or provenance.get("sourceCommit") != revision
            or provenance.get("platform") != platform or provenance.get("generator") != generator
            or provenance.get("families") != info["families"] or provenance.get("signed") is not True
            or provenance.get("artifactSHA256") != products[record["installer"]]
            or provenance.get("sbomSHA256") != products[record["sbom"]]):
        raise ValueError("native provenance mismatch or unsigned candidate")
    checksums = paths[record["checksums"]].read_text(encoding="utf-8")
    expected = provenance["products"]
    expected_names = {record[field] for field in ("installer", "runtimeArchive", "sbom", "sourceArchive")}
    if platform == "Linux":
        expected_names |= {name + ".asc" for name in list(expected_names)}
    if set(expected) != expected_names:
        raise ValueError("unexpected provenance asset set")
    if not expected or any(products.get(name) != digest for name, digest in expected.items()):
        raise ValueError("native provenance product mismatch")
    if checksums != "".join(f"{digest}  {name}\n" for name, digest in expected.items()):
        # JSON is sorted by the writer; checksum line ordering is not significant.
        if sorted(checksums.splitlines()) != sorted(f"{digest}  {name}" for name, digest in expected.items()):
            raise ValueError("native checksums mismatch")
    inventory = json.loads(paths[record["sbom"]].read_text(encoding="utf-8"))
    product = [item for item in inventory["packages"] if item["SPDXID"] == "SPDXRef-BetterPullReview"]
    if len(product) != 1 or product[0]["versionInfo"] != version or product[0]["sourceInfo"] != runtime.product_source_info(info, revision, True):
        raise ValueError("native SPDX producer mismatch")
    hashes, material = archive_contents(paths[record["runtimeArchive"]], platform)
    declared = {}
    for entry in inventory["files"]:
        name = relative_name(entry["fileName"].removeprefix("./"))
        if name in declared or entry["checksums"] != [{"algorithm": "SHA256", "checksumValue": hashes.get(name)}]:
            raise ValueError("runtime bytes differ from SPDX")
        declared[name] = hashes[name]
    if set(hashes) - {"sbom.spdx.json"} != set(declared):
        raise ValueError("runtime inventory mismatch")
    if hashes.get("sbom.spdx.json") != products[record["sbom"]]:
        raise ValueError("installed and sidecar SPDX differ")
    source = runtime.check_source_provenance(json.loads(material["source-provenance.json"]), info, revision)
    runtime.check_source_references(source, material)
    runtime.verify_source_asset(paths[record["sourceArchive"]], source)
    signatures = record["signatures"]
    producer_signatures = {name: receipt for name, receipt in signatures.items() if name not in {record["checksums"], record["provenance"]}}
    if provenance.get("firstPartyImages") != record["firstPartyImages"] or provenance.get("signatures") != producer_signatures:
        raise ValueError("native signing receipts differ from provenance")
    if platform == "Windows":
        images = {"better_pull_review.exe", "better_pull_review_agent_supervisor.exe",
                  *runtime.PRIVATE_LIBRARIES["Windows"]}
        if info["families"]["agent_integrations"]:
            images.add("better_pull_review_mcp_context_helper.exe")
        receipts = record["firstPartyImages"]
        if set(receipts) != images or set(signatures) != {record["installer"]}:
            raise ValueError("incomplete Authenticode verification receipts")
        for name, receipt in receipts.items():
            if name not in hashes or not re.fullmatch(r"[a-f0-9]{64}", receipt.get("sha256") or "") or receipt["sha256"] != hashes[name]:
                raise ValueError("signed producer differs from runtime")
        publisher = signatures[record["installer"]].get("publisher")
        if not publisher or any(receipt.get("publisher") != publisher for receipt in receipts.values()):
            raise ValueError("inconsistent Authenticode publisher")
        for receipt in [*receipts.values(), signatures[record["installer"]]]:
            if any(not re.fullmatch(r"[0-9A-Fa-f]{40}", receipt.get(key, "")) for key in ("certificateSHA1", "timestampCertificateSHA1")):
                raise ValueError("missing certificate or timestamp receipt")
    else:
        images = {"better_pull_review", "better_pull_review_agent_supervisor"}
        if info["families"]["agent_integrations"]:
            images.add("better_pull_review_mcp_context_helper")
        if not images <= set(hashes):
            raise ValueError("required Linux first-party image missing")
        signed = {record[field] for field in ("installer", "runtimeArchive", "sbom", "sourceArchive", "checksums", "provenance")}
        if set(signatures) != signed or record["firstPartyImages"]:
            raise ValueError("incomplete Linux signature set")
        keys = set()
        for name, receipt in signatures.items():
            if receipt.get("signature") != name + ".asc" or name + ".asc" not in paths:
                raise ValueError("missing detached signature asset")
            for key in ("keyFingerprint", "signingFingerprint"):
                if not re.fullmatch(r"(?:[0-9A-F]{40}|[0-9A-F]{64})", receipt.get(key, "")):
                    raise ValueError("invalid OpenPGP fingerprint receipt")
            keys.add(receipt["keyFingerprint"])
        if len(keys) != 1:
            raise ValueError("inconsistent Linux release key")
    allowed = set(expected) | {record["checksums"], record["provenance"]}
    if platform == "Linux":
        allowed |= {record["checksums"] + ".asc", record["provenance"] + ".asc"}
    if set(products) != allowed:
        raise ValueError("unexplained native release asset")
    return [*paths.values(), manifest]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, action="append", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--asset-list", type=Path, required=True, help="NUL-delimited verified absolute paths")
    args = parser.parse_args()
    assets = {}
    for manifest in args.manifest:
        for path in verify(manifest.resolve(strict=True), args.version, args.source_revision):
            if path.name in assets and runtime.digest(path) != runtime.digest(assets[path.name]):
                raise ValueError("conflicting release asset name: " + path.name)
            assets[path.name] = path
    args.asset_list.write_bytes(b"".join(str(path).encode() + b"\0" for path in assets.values()))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile, tarfile.TarError) as error:
        raise SystemExit(f"native release preflight refused: {error}") from error
