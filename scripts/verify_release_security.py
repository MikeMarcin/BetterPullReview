#!/usr/bin/env python3
"""Collect bounded exact-package security evidence; missing approval is never a pass.

This supplements verify_release_package, not the authoritative release workflow.
It performs no signing, trust enrollment, upload, repair or package mutation.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import zipfile

import verify_pe_release as pe
import verify_release_package as package

require = package.require


def source_and_notices(root: Path, sbom: Path, info: dict) -> dict:
    legal = package.runtime.legal_root(root, info)
    provenance = package.runtime.installed_source_provenance(root, info)
    notices = (legal / "THIRD-PARTY-NOTICES.txt").read_text(encoding="utf-8")
    for relative in package.runtime.NOTICE_FILES:
        require((package.ROOT / "third_party" / relative).read_text(encoding="utf-8") in notices, "required-notice-missing")
    qt = json.loads((legal / "qt-runtime-provenance.json").read_text(encoding="utf-8"))
    upstream = package.runtime.qt_records(info, root)
    require(qt == upstream, "qt-runtime-provenance-drift")
    packages = {item["SPDXID"]: item for item in json.loads(sbom.read_text(encoding="utf-8"))["packages"]}
    for item in qt["packages"]:
        require(item["SPDXID"] in packages and packages[item["SPDXID"]].get("versionInfo") == item.get("versionInfo"),
                "qt-transitive-package-missing")
    for item in qt.get("hasExtractedLicensingInfos", []):
        require(item["extractedText"] in notices, "qt-license-text-missing")
    if info["platform"] == "Windows":
        require("Microsoft Visual C++ Runtime redistribution notice" in notices, "msvc-notice-missing")
    offers = (legal / "SOURCE-OFFERS.txt").read_text(encoding="utf-8")
    require(all(value in offers for value in ("ABI-compatible", "corresponding-source.zip", "KSyntaxHighlighting",
                                             "equivalent access", "Retention is indefinite", "Corporate/offline redistribution",
                                             "separately replaceable shared library", "git-xdiff-2.56.0-build/")), "source-offer-incomplete")
    return {"source_archives": len(provenance["components"]), "qt_packages": len(qt["packages"]),
            "source_archive_sha256": provenance["asset"]["sha256"],
            "notices_sha256": package.runtime.digest(legal / "THIRD-PARTY-NOTICES.txt"),
            "legal_approval": "pending: first-party terms, mixed syntax-data licenses, toolchain terms and corresponding build/relink obligations, including the LGPL xdiff library's"}


def compile_hardening(build: Path, system: str) -> dict:
    records = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    selected = [item for item in records if Path(item["file"]).resolve().is_relative_to(package.ROOT / "src")
                or item["file"].replace("\\", "/").endswith("/sqlite3.c")]
    require(bool(selected), "empty-hardening-compile-inventory")
    required = ("/sdl", "/guard:cf", "/GS") if system == "Windows" else ("-fstack-protector-strong", "-fno-omit-frame-pointer", "-D_FORTIFY_SOURCE=3")
    failures = []
    for item in selected:
        arguments = item.get("arguments")
        if arguments is None:
            arguments = shlex.split(item["command"], posix=system != "Windows")
        # MSVC /GS is enabled by default; an explicit /GS- invalidates that default.
        absent = [flag for flag in required if flag not in arguments and not (flag == "/GS" and "/GS-" not in arguments)]
        disabling = {"/GS-", "/sdl-", "/guard:cf-", "-fno-stack-protector", "-fomit-frame-pointer", "-U_FORTIFY_SOURCE"}
        if absent or disabling.intersection(arguments):
            failures.append(Path(item["file"]).name)
    require(not failures, "missing-compile-hardening:" + ",".join(sorted(set(failures))))
    return {"compile_units": len(selected), "required": required,
            "third_party_external_builds": "separate qualification: prebuilt Qt and external KSyntaxHighlighting/LibArchive"}


def binary_hardening(binary: Path, system: str) -> dict:
    if system == "Windows":
        header = pe.pe_headers(binary.read_bytes())
        required = 0x20 | 0x40 | 0x100  # HIGH_ENTROPY_VA, DYNAMIC_BASE, NX_COMPAT.
        require(header["dll_flags"] & required == required, "pe-missing-aslr-or-dep")
        require(not any(section["flags"] & 0xA0000000 == 0xA0000000 for section in header["sections"]), "pe-writable-executable-section")
        return {"aslr_dep": True, "cfg_declared": bool(header["dll_flags"] & 0x4000)}
    if system == "Darwin":
        headers = subprocess.check_output(["otool", "-hv", str(binary)], text=True)
        for line in headers.splitlines():
            if "EXECUTE" in line:
                require("PIE" in line, "mach-executable-without-pie")
            require("ALLOW_STACK_EXECUTION" not in line, "mach-executable-stack")
        loads = subprocess.check_output(["otool", "-l", str(binary)], text=True)
        permissions = re.findall(r"(?m)^\s*initprot 0x([0-9a-fA-F]+)\s*$", loads)
        require(bool(permissions), "mach-segment-evidence-missing")
        require(all(int(value, 16) & 6 != 6 for value in permissions), "mach-writable-executable-segment")
        return {"pie_where_executable": True, "writable_executable_segments": False}
    headers = subprocess.check_output(["readelf", "-W", "-h", "-l", "-d", str(binary)], text=True)
    require(re.search(r"Type:\s+DYN", headers) is not None, "elf-not-position-independent")
    require("GNU_RELRO" in headers and ("BIND_NOW" in headers or re.search(r"FLAGS.*NOW", headers)), "elf-incomplete-relro")
    require(re.search(r"GNU_STACK.*RW\s", headers) is not None and not re.search(r"LOAD.*RWE", headers), "elf-executable-stack-or-writable-code")
    return {"pie_relro_now": True, "executable_stack": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--sbom", required=True, type=Path)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--windows-redist-root", type=Path)
    parser.add_argument("--source-archive", type=Path, help="defaults to the versioned asset beside the SPDX")
    args = parser.parse_args()
    try:
        build, root, sbom = (path.resolve(strict=True) for path in (args.build, args.root, args.sbom))
        evidence = package.audit(build, root, sbom, args.source_revision, windows_redist_root=args.windows_redist_root,
                                 source_archive=args.source_archive)
        info = package.runtime.inputs(build)
        evidence["sources_notices"] = source_and_notices(root, sbom, info)
        evidence["compile_hardening"] = compile_hardening(build, info["platform"])
        evidence["binary_hardening"] = {name: binary_hardening(root / name, info["platform"]) for name in evidence["binaries"]}
        evidence["result"] = "RELEASE_SECURITY_MECHANICS_OK"
        evidence["release_acceptance"] = "pending independent trust/legal, scan policy and complete native runtime/vault/crash evidence"
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, subprocess.SubprocessError, package.composition.verification_error) as error:
        print(f"RELEASE_SECURITY_FAILED:{error}", file=sys.stderr)
        return 1
    print(json.dumps(evidence, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
