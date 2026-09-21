#!/usr/bin/env python3
"""Negative controls for producer binding and emitted hardening evidence."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
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
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            declared = [{"name": "fixture", "version": "1", "url": "https://example.invalid/a.zip",
                         "sha256": hashlib.sha256(b"source").hexdigest()}]
            (root / "source-provenance.json").write_text(json.dumps({"schema": "bpr.release-sources.v1", "components": declared}))
            with patch.object(package.runtime, "legal_root", return_value=root), patch.object(package.runtime, "components", return_value=declared):
                for name, payload, error in (("unexpected.zip", b"source", "inventory-mismatch"),
                                             ("fixture-1.zip", b"substitute", "digest-mismatch")):
                    with zipfile.ZipFile(root / "corresponding-source.zip", "w") as archive:
                        archive.writestr(name, payload)
                    with self.assertRaisesRegex(ValueError, error):
                        security.source_and_notices(root, root / "unused.json", {})

    def test_compile_hardening_requires_selected_flags(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            record = {"file": str(package.ROOT / "src/core/src/release_maintenance.cpp"),
                      "command": "clang++ -fstack-protector-strong -fno-omit-frame-pointer -D_FORTIFY_SOURCE=3"}
            (root / "compile_commands.json").write_text(json.dumps([record]))
            self.assertEqual(security.compile_hardening(root, "Darwin")["compile_units"], 1)
            record["command"] = record["command"].replace("-fstack-protector-strong", "")
            (root / "compile_commands.json").write_text(json.dumps([record]))
            with self.assertRaisesRegex(ValueError, "missing-compile-hardening"):
                security.compile_hardening(root, "Darwin")

    def test_map_requires_exact_producer_image_identity_and_addresses(self):
        with tempfile.TemporaryDirectory() as directory:
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
        with tempfile.TemporaryDirectory() as directory:
            producer = Path(directory) / "app.exe"
            producer.write_bytes(image())
            producer.with_suffix(".map").write_text(map_text("?github_public_path_fixture@private_impl@@YAXXZ"))
            with patch.object(pe.shutil, "which", return_value="undname"):
                symbols = pe.map_symbols(producer, producer)
            with self.assertRaisesRegex(ValueError, "fixture-symbol"):
                package.check_payload(image(), symbols)

    def test_pe_hardening_requires_aslr_dep_and_nonwritable_code(self):
        with tempfile.TemporaryDirectory() as directory:
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

    def test_macho_checks_every_slice_and_segment(self):
        with patch.object(security.subprocess, "check_output", side_effect=["ARM64 EXECUTE PIE\nX86_64 EXECUTE\n", ""]):
            with self.assertRaisesRegex(ValueError, "without-pie"):
                security.binary_hardening(Path("app"), "Darwin")
        with patch.object(security.subprocess, "check_output", side_effect=["ARM64 EXECUTE PIE\nX86_64 EXECUTE PIE\n", " initprot 0x00000005\n initprot 0x00000007\n"]):
            with self.assertRaisesRegex(ValueError, "writable-executable"):
                security.binary_hardening(Path("app"), "Darwin")


if __name__ == "__main__":
    unittest.main()
