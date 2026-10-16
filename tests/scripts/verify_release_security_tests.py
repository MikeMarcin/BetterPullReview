#!/usr/bin/env python3
"""Negative controls for producer binding and emitted hardening evidence."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import verify_pe_release as pe
import verify_release_package as package
import verify_release_security as security


def image():
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 64)
    data[64:68] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 68, 0x8664, 1, 123, 0, 0, 240, 2)
    struct.pack_into("<H", data, 88, 0x20b)
    struct.pack_into("<I", data, 104, 0x1000)
    struct.pack_into("<Q", data, 112, 0x140000000)
    struct.pack_into("<H", data, 158, 0x4160)
    data[328:336] = b".text\0\0\0"
    struct.pack_into("<IIII", data, 336, 100, 0x1000, 128, 512)
    struct.pack_into("<I", data, 364, 0x60000020)
    return bytes(data)


def map_text(symbol="main"):
    return (" app\n\n Timestamp is 0000007b (fixture)\n\n Preferred load address is 0000000140000000\n"
            " Address Publics by Value Rva+Base Lib:Object\n\n"
            f" 0001:00000000 {symbol} 0000000140001000 f app.obj\n\n"
            " entry point at 0001:00000000\n\n Static symbols\n")


class ReleaseSecurityTests(unittest.TestCase):
    def test_source_archive_inventory_and_digest_fail_closed(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            declared = [{"name": "fixture", "version": "1", "url": "https://example.invalid/a.zip",
                         "sha256": hashlib.sha256(b"source").hexdigest()}]
            info = {"platform": "Darwin", "version": "1.2.3"}
            source = root / package.runtime.source_asset_name(info)
            with patch.object(package.runtime, "components", return_value=declared):
                for name, payload, error in (("unexpected.zip", b"source", "inventory-mismatch"),
                                             ("fixture-1.zip", b"substitute", "digest-mismatch")):
                    with zipfile.ZipFile(source, "w") as archive:
                        archive.writestr(name, payload)
                    provenance = package.runtime.source_provenance(info, "a" * 40, package.runtime.digest(source))
                    with self.assertRaisesRegex(ValueError, error):
                        package.runtime.verify_source_asset(source, provenance)

    def test_compile_hardening_requires_selected_flags(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            record = {"file": str(package.ROOT / "src/core/src/release_maintenance.cpp"),
                      "command": "clang++ -fstack-protector-strong -fno-omit-frame-pointer -D_FORTIFY_SOURCE=3"}
            (root / "compile_commands.json").write_text(json.dumps([record]))
            self.assertEqual(security.compile_hardening(root, "Darwin")["compile_units"], 1)
            record["command"] += " -fno-stack-protector"
            (root / "compile_commands.json").write_text(json.dumps([record]))
            with self.assertRaisesRegex(ValueError, "missing-compile-hardening"):
                security.compile_hardening(root, "Darwin")
            record["command"] = record["command"].replace("-fstack-protector-strong", "")
            (root / "compile_commands.json").write_text(json.dumps([record]))
            with self.assertRaisesRegex(ValueError, "missing-compile-hardening"):
                security.compile_hardening(root, "Darwin")

    def test_map_requires_exact_producer_image_identity_and_addresses(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            producer, installed = root / "app.exe", root / "installed.exe"
            producer.write_bytes(image())
            installed.write_bytes(image())
            link_map = producer.with_suffix(".map")
            link_map.write_text(map_text())
            with patch.object(pe.shutil, "which", return_value="undname"):
                self.assertIn("main", pe.map_symbols(installed, producer))
                for old, new, error in ((" app\n", " other\n", "image-mismatch"),
                                        ("0000007b", "0000007c", "timestamp-mismatch"),
                                        ("is 0000000140000000", "is 0000000240000000", "base-mismatch"),
                                        ("entry point at 0001:00000000", "entry point at 0001:00000001", "entry-mismatch"),
                                        ("0000000140001000 f", "0000000140001001 f", "symbol-address-mismatch")):
                    link_map.write_text(map_text().replace(old, new))
                    with self.assertRaisesRegex(ValueError, error):
                        pe.map_symbols(installed, producer)
                link_map.write_text(map_text())
                installed.write_bytes(image() + b"changed")
                with self.assertRaisesRegex(ValueError, "producer-byte-mismatch"):
                    pe.map_symbols(installed, producer)

    def test_complete_decorated_table_catches_non_anchor_testing_seam(self):
        with ContainedTemporaryDirectory() as directory:
            producer = Path(directory) / "app.exe"
            producer.write_bytes(image())
            producer.with_suffix(".map").write_text(map_text("?github_public_path_fixture@private_impl@@YAXXZ"))
            with patch.object(pe.shutil, "which", return_value="undname"):
                symbols = pe.map_symbols(producer, producer)
            with self.assertRaisesRegex(ValueError, "fixture-symbol"):
                package.check_payload(image(), symbols)

    def test_declared_private_dll_needs_exact_bytes_and_full_bound_map(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            build, installed = root / "build", root / "installed"
            build.mkdir()
            installed.mkdir()
            producer, binary = build / "bpr_xdiff.dll", installed / "bpr_xdiff.dll"
            payload = bytearray(image())
            struct.pack_into("<H", payload, 86, 0x2002)
            producer.write_bytes(payload)
            binary.write_bytes(payload)
            info = {"platform": "Windows", "app": str(build / "app.exe"), "helpers": [],
                    "private_libraries": {binary.name: str(producer)}}
            declared = tuple(package.runtime.private_library_producers(info))
            bound_map = map_text("xdl_diff").replace(" app\n", " bpr_xdiff\n")
            link_map = producer.with_suffix(".map")
            with patch.object(pe.shutil, "which", return_value="undname"):
                with self.assertRaisesRegex(ValueError, "producer-map-missing"):
                    pe.inspect(binary, info, private_producers=declared)
                link_map.write_text(bound_map)
                table = pe.inspect(binary, info, private_producers=declared)
                self.assertIn("xdl_diff", table.symbols)
                self.assertIn("bound full link /MAP", table.inspector)
                link_map.write_text(bound_map.replace(" bpr_xdiff\n", " other\n"))
                with self.assertRaisesRegex(ValueError, "map-image-mismatch"):
                    pe.inspect(binary, info, private_producers=declared)
                link_map.write_text(bound_map)
                binary.write_bytes(payload + b"substituted")
                with self.assertRaisesRegex(ValueError, "producer-byte-mismatch"):
                    pe.inspect(binary, info, private_producers=declared)
                binary.write_bytes(image())
                with self.assertRaisesRegex(ValueError, "private-producer-not-dll"):
                    pe.inspect(binary, info, private_producers=declared)
                binary.write_bytes(payload)
                link_map.write_text(bound_map.replace("xdl_diff", "github_public_path_fixture"))
                table = pe.inspect(binary, info, private_producers=declared)
                with self.assertRaisesRegex(ValueError, "fixture-symbol"):
                    package.check_payload(bytes(payload), table.symbols)

    def test_identical_unknown_dll_still_has_no_producer_authority(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory)
            qt = root / "qt"
            (qt / "sbom").mkdir(parents=True)
            (qt / "sbom/qtbase-6.11.2.spdx.json").write_text('{"files":[]}')
            binary = root / "unknown.dll"
            binary.write_bytes(image())
            producer = root / "bpr_xdiff.dll"
            producer.write_bytes(image())
            info = {"platform": "Windows", "app": str(root / "app.exe"), "helpers": [],
                    "qt_root": str(qt), "qt_version": "6.11.2",
                    "private_libraries": {producer.name: str(producer)}}
            with self.assertRaisesRegex(ValueError, "undeclared-pe-producer"):
                pe.inspect(binary, info, root, private_producers=tuple(package.runtime.private_library_producers(info)))

    def test_linker_only_cfg_indexes_are_bound_and_still_scanned(self):
        with ContainedTemporaryDirectory() as directory:
            producer = Path(directory) / "app.exe"
            producer.write_bytes(image())
            text = map_text("__guard_fids_github_public_path_fixture")
            text = text.replace("0001:00000000 __guard", "0001:fffff004 __guard").replace("0000000140001000 f", "0000000140000004 f")
            producer.with_suffix(".map").write_text(text)
            with patch.object(pe.shutil, "which", return_value="undname"):
                symbols = pe.map_symbols(producer, producer)
                with self.assertRaisesRegex(ValueError, "fixture-symbol"):
                    package.check_payload(image(), symbols)
                for invalid in (text.replace("__guard_fids_", "untrusted_"), text.replace("fffff004", "fffff005")):
                    producer.with_suffix(".map").write_text(invalid)
                    with self.assertRaisesRegex(ValueError, "symbol-address-mismatch"):
                        pe.map_symbols(producer, producer)

    def test_pe_hardening_requires_aslr_dep_and_nonwritable_code(self):
        with ContainedTemporaryDirectory() as directory:
            binary = Path(directory) / "app.exe"
            binary.write_bytes(image())
            self.assertTrue(security.binary_hardening(binary, "Windows")["cfg_declared"])
            for offset, value, error in ((158, 0x4140, "aslr-or-dep"), (364, 0xE0000020, "writable-executable")):
                data = bytearray(image())
                struct.pack_into("<H" if offset == 158 else "<I", data, offset, value)
                binary.write_bytes(data)
                with self.assertRaisesRegex(ValueError, error):
                    security.binary_hardening(binary, "Windows")

    def test_truncated_and_foreign_pe_fail_closed(self):
        for payload in (b"", b"MZ", image()[:150], b"ELF" + image()[3:]):
            with self.assertRaises(ValueError):
                pe.pe_headers(payload)

    def test_qt_presign_digest_requires_unchanged_payload_and_valid_publisher(self):
        with ContainedTemporaryDirectory() as directory:
            binary = Path(directory) / "Qt6Core.dll"
            signed = bytearray(image())
            struct.pack_into("<I", signed, 152, 1234)  # Signed-image checksum.
            struct.pack_into("<II", signed, 232, len(signed), 16)
            signed.extend(bytes(16))
            binary.write_bytes(signed)
            hashes = [{"algorithm": "SHA1", "checksumValue": hashlib.sha1(image()).hexdigest()}]
            with patch.object(pe.subprocess, "check_output", return_value='{"Status":0,"Signer":"CN=Qt, O=The QT Company Oy, C=FI"}'):
                self.assertTrue(pe.qt_signed_spdx_match(binary, hashes))
                signed[600] = 1
                binary.write_bytes(signed)
                self.assertFalse(pe.qt_signed_spdx_match(binary, hashes))
                signed[600] = 0
                binary.write_bytes(signed)
            for reply in ('{"Status":2,"Signer":"O=The QT Company Oy"}', '{"Status":0,"Signer":"O=Unknown"}'):
                with patch.object(pe.subprocess, "check_output", return_value=reply):
                    self.assertFalse(pe.qt_signed_spdx_match(binary, hashes))

    def test_macho_checks_every_slice_and_segment(self):
        with patch.object(security.subprocess, "check_output", side_effect=["ARM64 EXECUTE PIE\nX86_64 EXECUTE\n", ""]):
            with self.assertRaisesRegex(ValueError, "without-pie"):
                security.binary_hardening(Path("app"), "Darwin")
        with patch.object(security.subprocess, "check_output", side_effect=["ARM64 EXECUTE PIE\nX86_64 EXECUTE PIE\n", " initprot 0x00000005\n initprot 0x00000007\n"]):
            with self.assertRaisesRegex(ValueError, "writable-executable"):
                security.binary_hardening(Path("app"), "Darwin")


if __name__ == "__main__":
    unittest.main()
