"""Native PE evidence from the exact producer, including stripped first-party images.

A linker map is trusted build output, not a signature. Bind its image identity,
timestamp, entry point and symbol addresses to the byte-identical producer PE.
Keep every decorated public/static name for fixture-seam scanning; only composition
anchors need demangling. Third-party DLLs instead require exact upstream bytes and
their exported symbol table. Neither path is independent distribution approval.
"""
from __future__ import annotations

import hashlib
import importlib.util
import base64
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
from typing import BinaryIO

import verify_binary_dependencies as dependencies
import verify_composed_hosts as composition


# Reuse the packaging module's exact deployed-module document selection.
_spec = importlib.util.spec_from_file_location("bpr_pe_package_runtime", Path(__file__).resolve().parents[1] / "packaging/package_runtime.py")
runtime = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(runtime)


# BPR-1654: an independently inspected Windows SDK import-library specimen.
# Its .idata$6 static/value-zero symbol names a bcrypt.dll string,
# not implementation. This pin does not establish the linked library. Changed
# specimen bytes need new evidence, never a wider skip.
BCRYPT_IMPORT_LIBRARY_SHA256 = "9fe31df255d17b0339391ff80a71bf2c5d442cc301bd2fe6eeea00b626f10785"
BCRYPT_IMPORT_OWNER = "bcrypt:bcrypt.dll"
# v3.5.3's pinned archive supplies this prebuilt x64 redirector. It is not
# represented as a source-built BPR DLL or accepted by filename alone.
MIMALLOC_REDIRECT_X64_SHA256 = "bc3dfdf51fbc80b88cb62cdb6a6784ecc894164bdd4ae7f3885048d870809491"


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def stream_digest(stream: BinaryIO, algorithm: str) -> str:
    result = hashlib.new(algorithm)
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        result.update(block)
    return result.hexdigest()


def digest(path: Path, algorithm: str = "sha256") -> str:
    with path.open("rb") as stream:
        return stream_digest(stream, algorithm)


def pe_headers(payload: bytes) -> dict:
    require(len(payload) >= 64 and payload[:2] == b"MZ", "invalid-pe-dos-header")
    offset = struct.unpack_from("<I", payload, 60)[0]
    require(offset + 24 <= len(payload) and payload[offset:offset + 4] == b"PE\0\0", "invalid-pe-header")
    machine, count, timestamp, _, _, size, characteristics = struct.unpack_from("<HHIIIHH", payload, offset + 4)
    optional = offset + 24
    require(machine == 0x8664 and 0 < count <= 96 and size >= 112
            and optional + size + count * 40 <= len(payload), "invalid-pe-x64-layout")
    require(struct.unpack_from("<H", payload, optional)[0] == 0x20b, "invalid-pe-plus-header")
    sections = []
    for index in range(count):
        start = optional + size + index * 40
        virtual_size, address, raw_size, raw_offset = struct.unpack_from("<IIII", payload, start + 8)
        flags = struct.unpack_from("<I", payload, start + 36)[0]
        require(raw_offset + raw_size <= len(payload), "pe-section-outside-file")
        sections.append({"address": address, "size": max(virtual_size, raw_size), "flags": flags})
    return {"timestamp": timestamp, "image_base": struct.unpack_from("<Q", payload, optional + 24)[0],
            "entry": struct.unpack_from("<I", payload, optional + 16)[0],
            "dll_flags": struct.unpack_from("<H", payload, optional + 70)[0],
            "characteristics": characteristics, "sections": sections}


def import_metadata_specimen(library: Path) -> dict:
    require(library.is_absolute() and library.name.lower() == "bcrypt.lib"
            and not library.is_symlink() and library.is_file(), "pe-import-metadata-specimen-required")
    require(digest(library) == BCRYPT_IMPORT_LIBRARY_SHA256, "pe-import-metadata-specimen-unreviewed")
    return {"library": str(library.resolve()), "sha256": BCRYPT_IMPORT_LIBRARY_SHA256,
            "role": "Independently reviewed metadata specimen; actual producer library selection is not established",
            "reviewed_metadata": "bcrypt.dll import-name .idata$6: static, value 0, type 0, section 3; 12 bytes bcrypt.dll NUL NUL"}


def map_symbols(binary: Path, producer: Path, *, windows_import_library: Path | None = None) -> tuple[str, ...]:
    require(digest(binary) == digest(producer), "pe-producer-byte-mismatch")
    header = pe_headers(binary.read_bytes())
    proof = import_metadata_specimen(windows_import_library) if windows_import_library is not None else None
    path = producer.with_suffix(".map")
    require(path.is_file() and not path.is_symlink(), "pe-producer-map-missing")
    text = path.read_text(encoding="utf-8", errors="strict")
    require(text.splitlines()[0].strip() == producer.stem, "pe-map-image-mismatch")
    stamp = re.search(r"(?m)^\s*Timestamp is ([0-9a-fA-F]+)\b", text)
    base = re.search(r"(?m)^\s*Preferred load address is ([0-9a-fA-F]+)\s*$", text)
    entry = re.search(r"(?m)^\s*entry point at\s+([0-9a-fA-F]{4}):([0-9a-fA-F]+)\s*$", text)
    require(stamp is not None and int(stamp[1], 16) == header["timestamp"], "pe-map-timestamp-mismatch")
    require(base is not None and int(base[1], 16) == header["image_base"], "pe-map-base-mismatch")
    require(entry is not None and 1 <= int(entry[1], 16) <= len(header["sections"]), "pe-map-entry-missing")
    section = header["sections"][int(entry[1], 16) - 1]
    require(section["address"] + int(entry[2], 16) == header["entry"], "pe-map-entry-mismatch")
    in_statics, owner_descriptor = False, False
    imports = None
    for line in text.splitlines():
        if composition.MAP_STATICS_HEADER.fullmatch(line):
            in_statics = True
        elif composition.MAP_PUBLICS_HEADER.fullmatch(line) or composition.MAP_ENTRY_POINT.fullmatch(line):
            in_statics = False
        record = composition.MAP_SYMBOL_RECORD.fullmatch(line)
        if record is None or int(record["section"], 16) == 0:
            continue
        index = int(record["section"], 16) - 1
        require(index < len(header["sections"]), "pe-map-section-mismatch")
        section = header["sections"][index]
        displacement = int(record["offset"], 16)
        address = int(record["rva"], 16)
        # MSVC emits linker-only CFG index records at image-base-relative
        # offsets before the first mapped section. Their section displacement
        # wraps at 32 bits; these names still participate in forbidden-seam
        # scanning, but are not represented as executable functions.
        guard_metadata = (record["name"].startswith(("__guard_fids_", "__guard_iats_", "__guard_ehcont_"))
                          and header["image_base"] <= address < header["image_base"] + min(item["address"] for item in header["sections"])
                          and (section["address"] + displacement) & 0xffffffff == address - header["image_base"])
        if guard_metadata:
            continue
        # MSVC retains this SDK static symbol after its DLL-name contribution
        # is discarded. Accept only the reviewed zero-RVA metadata shape, not
        # arbitrary wrapped addresses, import sections or function symbols.
        discarded_import_name = (proof is not None and in_statics and owner_descriptor
                                 and record["name"] == ".idata$6" and not record["flags"].strip()
                                 and record["object"] == BCRYPT_IMPORT_OWNER
                                 and address == header["image_base"] and 0 <= displacement < 2**32
                                 and section["address"] + displacement == 2**32
                                 and section["flags"] & 0x40 and not section["flags"] & 0x20000000)
        if discarded_import_name:
            if imports is None:
                output = subprocess.check_output(dependencies.windows_dependency_command(binary), text=True, timeout=30)
                imports = {name.lower() for name in dependencies.dependency_references(output, "Windows")}
                require("kernel32.dll" in imports, "pe-sdk-import-evidence-incomplete")
            payload = binary.read_bytes().lower()
            require("bcrypt.dll" not in imports and b"bcrypt.dll" not in payload
                    and "bcrypt.dll".encode("utf-16le") not in payload, "pe-sdk-import-still-referenced")
            continue
        require(displacement < section["size"]
                and address == header["image_base"] + section["address"] + displacement,
                "pe-map-symbol-address-mismatch")
        if (record["name"] == "__NULL_IMPORT_DESCRIPTOR" and not record["flags"].strip()
                and record["object"] == BCRYPT_IMPORT_OWNER):
            owner_descriptor = True
    decorated = composition.parse_map_defined_symbols(text)
    undname = shutil.which("undname")
    require(undname is not None, "pe-map-needs-native-undname")
    return (*decorated, *composition.demangle_map_symbols(undname, decorated, subprocess.run))


def qt_signed_spdx_match(producer: Path, hashes: list[dict]) -> bool:
    """Bind Qt's pre-sign SPDX to its signed bytes without changing the DLL.

    Only the PE checksum, certificate directory and trailing certificate may
    differ. The reconstructed *whole file* must match every upstream digest;
    native Authenticode must independently validate Qt's publisher signature.
    """
    payload = bytearray(producer.read_bytes())
    pe_headers(payload)
    optional = struct.unpack_from("<I", payload, 60)[0] + 24
    certificate, size = struct.unpack_from("<II", payload, optional + 144)
    if size < 8 or certificate + size != len(payload) or certificate < optional + 152:
        return False
    payload[optional + 64:optional + 68] = bytes(4)
    payload[optional + 144:optional + 152] = bytes(8)
    payload = payload[:certificate]
    if not all(hashlib.new(item["algorithm"].lower(), payload).hexdigest() == item["checksumValue"].lower() for item in hashes):
        return False
    command = ("$ProgressPreference='SilentlyContinue'; Get-AuthenticodeSignature -LiteralPath '"
               + str(producer).replace("'", "''")
               + "' | Select-Object Status,@{n='Signer';e={$_.SignerCertificate.Subject}} | ConvertTo-Json -Compress")
    encoded = base64.b64encode(command.encode("utf-16le")).decode("ascii")
    result = subprocess.check_output(["powershell", "-NoProfile", "-NonInteractive", "-EncodedCommand", encoded], text=True, timeout=30)
    signature = json.loads(result)
    return signature.get("Status") == 0 and re.search(r"(?:^|,\s*)O=The QT Company Oy(?:,|$)", signature.get("Signer", ""), re.I) is not None


def upstream_dll(binary: Path, info: dict, redist_root: Path | None) -> Path:
    qt = Path(info["qt_root"]).resolve(strict=True)
    record = runtime.qt_upstream_records(info, {binary.name})
    matching = [item for item in record["files"] if Path(item["fileName"]).name.lower() == binary.name.lower()
                and "BINARY" in item.get("fileTypes", [])]
    if matching:
        require(len(matching) == 1, "ambiguous-qt-producer-file")
        item = matching[0]
        producer = (qt / item["fileName"]).resolve(strict=True)
        require(producer.is_relative_to(qt) and digest(producer) == digest(binary), "qt-producer-byte-mismatch")
        hashes = [value for value in item["checksums"] if value["algorithm"] in {"SHA1", "SHA256"}]
        require(bool(hashes), "qt-producer-spdx-checksum-missing")
        raw_match = all(digest(producer, value["algorithm"].lower()) == value["checksumValue"].lower() for value in hashes)
        require(raw_match or qt_signed_spdx_match(producer, hashes), "qt-producer-spdx-mismatch")
        return producer
    # No filename alone can vouch for a toolchain DLL. The explicitly selected
    # licensed redist directory and exact byte equality are both required.
    if redist_root is None:
        configured = os.environ.get("VCToolsRedistDir")
        require(bool(configured), "pe-msvc-redist-root-required")
        redist_root = Path(configured)
    root = redist_root.resolve(strict=True)
    require(re.fullmatch(r"(?:msvcp140(?:_1|_2|_atomic_wait|_codecvt_ids)?|vcruntime140(?:_1|_threads)?|concrt140)\.dll", binary.name, re.I) is not None,
            "undeclared-pe-producer")
    matching_paths = [path.resolve() for path in (root / "x64").glob("Microsoft.VC*.CRT/*.dll")
                      if path.name.lower() == binary.name.lower() and path.resolve().is_relative_to(root)
                      and digest(path) == digest(binary)]
    require(len(matching_paths) == 1, "msvc-producer-byte-mismatch")
    return matching_paths[0]


def inspect(binary: Path, info: dict, redist_root: Path | None = None, *,
            private_producers: tuple[Path, ...] = (),
            windows_import_library: Path | None = None) -> composition.symbol_table:
    producers = [Path(info["app"]), *(Path(path) for path in info["helpers"]), *private_producers]
    selected = [path for path in producers if path.name == binary.name]
    if binary.name == "mimalloc-redirect.dll":
        require(len(selected) == 1 and selected[0] in private_producers, "pinned-mimalloc-redirect-producer-required")
        require(digest(binary) == MIMALLOC_REDIRECT_X64_SHA256
                and digest(selected[0]) == MIMALLOC_REDIRECT_X64_SHA256, "pinned-mimalloc-redirect-byte-mismatch")
    elif selected:
        require(len(selected) == 1, "ambiguous-first-party-producer")
        if selected[0] in private_producers:
            require(pe_headers(binary.read_bytes())["characteristics"] & 0x2000, "declared-private-producer-not-dll")
        return composition.symbol_table(map_symbols(binary, selected[0], windows_import_library=windows_import_library),
                                        "byte-identical PE + bound full link /MAP + undname anchors")
    if binary.name != "mimalloc-redirect.dll":
        upstream_dll(binary, info, redist_root)
    reader = shutil.which("llvm-readobj")
    require(reader is not None, "pe-exports-needs-llvm-readobj")
    output = subprocess.check_output([reader, "--coff-exports", str(binary)], text=True, timeout=30)
    names = tuple(re.findall(r"(?m)^\s+Name: (\S+)\s*$", output))
    require(bool(names), "empty-upstream-pe-exports")
    return composition.symbol_table(names, "byte-identical upstream PE + exported symbols")
