#!/usr/bin/env python3
"""Negative controls for producer binding and emitted hardening evidence."""
from pathlib import Path
from contextlib import contextmanager
import hashlib
import io
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


def metadata_image(payload=b"", data_flags=0x40000040):
    data = bytearray(image())
    struct.pack_into("<H", data, 70, 2)
    data[368:376] = b".rdata\0\0"
    struct.pack_into("<IIII", data, 376, 100, 0x2000, 128, 640)
    struct.pack_into("<I", data, 404, data_flags)
    return bytes(data) + payload


def metadata_map(symbol="main"):
    text = map_text(symbol).replace(" entry point at", " 0002:00000010 __NULL_IMPORT_DESCRIPTOR 0000000140002010 bcrypt:bcrypt.dll\n\n entry point at")
    return text + " 0002:ffffe000 .idata$6 0000000140000000 bcrypt:bcrypt.dll\n"


@contextmanager
def metadata_tools(library, imports="    KERNEL32.dll\n"):
    # Synthetic specimen only for portable controls. The native course uses the
    # unmodified production hash and the independently inspected actual SDK file.
    with patch.object(pe, "BCRYPT_IMPORT_LIBRARY_SHA256", hashlib.sha256(library.read_bytes()).hexdigest()), \
            patch.object(pe.shutil, "which", return_value="undname"), \
            patch.object(pe.dependencies, "windows_dependency_command", return_value=["dumpbin", "/dependents", "app.exe"]), \
            patch.object(pe.subprocess, "check_output", return_value=imports):
        yield


class ReleaseSecurityTests(unittest.TestCase):
    def test_package_and_security_cli_forward_explicit_specimen_only_to_audits(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build, runtime = root / "build", root / "runtime"
            build.mkdir()
            runtime.mkdir()
            sbom, library = root / "sbom.json", root / "bcrypt.lib"
            sbom.write_text("{}")
            library.write_bytes(b"synthetic specimen")
            arguments = ["audit", "--build", str(build), "--root", str(runtime), "--sbom", str(sbom),
                         "--source-revision", "a" * 40, "--windows-import-library", str(library)]
            with patch.object(sys, "argv", arguments), patch.object(sys, "stdout", io.StringIO()), \
                    patch.object(package, "audit", return_value={"binaries": []}) as audited:
                self.assertEqual(package.main(), 0)
                self.assertEqual(audited.call_args.kwargs["windows_import_library"], library)
            with patch.object(sys, "argv", arguments), patch.object(sys, "stdout", io.StringIO()), \
                    patch.object(package, "audit", return_value={"binaries": []}) as audited, \
                    patch.object(package.runtime, "inputs", return_value={"platform": "Windows"}), \
                    patch.object(security, "source_and_notices", return_value={}), \
                    patch.object(security, "compile_hardening", return_value={}):
                self.assertEqual(security.main(), 0)
                self.assertEqual(audited.call_args.kwargs["windows_import_library"], library)

    def test_specimen_argument_does_not_apply_to_another_platform(self):
        with patch.object(package.runtime, "inputs", return_value={"platform": "Darwin"}), \
                patch.object(package, "package_paths", return_value=(Path("app"), [])), \
                patch.object(package, "check_file_inventory", return_value={}):
            with self.assertRaisesRegex(ValueError, "requires-windows"):
                package.audit(Path("build"), Path("runtime"), Path("sbom"), "a" * 40,
                              windows_import_library=Path("/explicit/bcrypt.lib"))

    def test_reviewed_sdk_file_is_only_a_metadata_specimen(self):
        with ContainedTemporaryDirectory() as directory:
            library = Path(directory).resolve() / "bcrypt.lib"
            library.write_bytes(b"synthetic metadata specimen")
            with self.assertRaisesRegex(ValueError, "specimen-unreviewed"):
                pe.import_metadata_specimen(library)
            with metadata_tools(library):
                proof = pe.import_metadata_specimen(library)
                self.assertIn("selection is not established", proof["role"])
                self.assertEqual(proof["sha256"], hashlib.sha256(library.read_bytes()).hexdigest())
                library.write_bytes(b"changed specimen")
                with self.assertRaisesRegex(ValueError, "specimen-unreviewed"):
                    pe.import_metadata_specimen(library)
            for missing in (library.with_name("missing.lib"), Path("bcrypt.lib")):
                with self.assertRaisesRegex(ValueError, "specimen-required"):
                    pe.import_metadata_specimen(missing)

    def test_discarded_import_metadata_needs_explicit_proof_and_stays_scanned(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory).resolve()
            producer, library = root / "app.exe", root / "bcrypt.lib"
            producer.write_bytes(metadata_image())
            library.write_bytes(b"synthetic metadata specimen")
            producer.with_suffix(".map").write_text(metadata_map())
            with metadata_tools(library):
                with self.assertRaisesRegex(ValueError, "symbol-address-mismatch"):
                    pe.map_symbols(producer, producer)
                symbols = pe.map_symbols(producer, producer, windows_import_library=library)
                self.assertIn(".idata$6", symbols)
                self.assertIn("__NULL_IMPORT_DESCRIPTOR", symbols)
                producer.with_suffix(".map").write_text(metadata_map("github_public_path_fixture"))
                symbols = pe.map_symbols(producer, producer, windows_import_library=library)
                with self.assertRaisesRegex(ValueError, "fixture-symbol"):
                    package.check_payload(producer.read_bytes(), symbols)

    def test_discarded_import_shape_mutants_and_ordinary_bad_addresses_refuse(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory).resolve()
            producer, library = root / "app.exe", root / "bcrypt.lib"
            producer.write_bytes(metadata_image())
            library.write_bytes(b"synthetic metadata specimen")
            text = metadata_map()
            row = "0002:ffffe000 .idata$6 0000000140000000 bcrypt:bcrypt.dll"
            mutants = [text.replace(row, value) for value in (
                row.replace(".idata$6", ".idata$5"),
                row.replace(".idata$6", ".idata$6_untrusted"),
                row.replace("bcrypt:bcrypt.dll", "other:bcrypt.dll"),
                row.replace("bcrypt:bcrypt.dll", "bcrypt:KERNEL32.dll"),
                row.replace("0000000140000000 bcrypt", "0000000140000000 f bcrypt"),
                row.replace("0000000140000000 bcrypt", "0000000140000000 i bcrypt"),
                row.replace("ffffe000", "ffffe001"),
                row.replace("0000000140000000", "0000000140000001"),
                row.replace("0002:", "0001:"),
                row.replace(".idata$6", "ordinary_function"),
            )]
            mutants += [text.replace(" Static symbols\n", ""),
                        text.replace("__NULL_IMPORT_DESCRIPTOR", "unrelated_descriptor"),
                        text.replace("0000000140001000 f", "0000000140001001 f")]
            with metadata_tools(library):
                for index, invalid in enumerate(mutants):
                    with self.subTest(index=index):
                        producer.with_suffix(".map").write_text(invalid)
                        with self.assertRaisesRegex(ValueError, "symbol-address-mismatch"):
                            pe.map_symbols(producer, producer, windows_import_library=library)
                producer.with_suffix(".map").write_text(text)
                for flags in (0x60000040, 0x40000080):
                    producer.write_bytes(metadata_image(data_flags=flags))
                    with self.assertRaisesRegex(ValueError, "symbol-address-mismatch"):
                        pe.map_symbols(producer, producer, windows_import_library=library)

    def test_discarded_import_actual_import_payload_and_failed_observation_refuse(self):
        with ContainedTemporaryDirectory() as directory:
            root = Path(directory).resolve()
            producer, library = root / "app.exe", root / "bcrypt.lib"
            library.write_bytes(b"synthetic metadata specimen")
            producer.write_bytes(metadata_image())
            producer.with_suffix(".map").write_text(metadata_map())
            for imports, reason in (("KERNEL32.dll\nBCRYPT.dll\n", "still-referenced"), ("", "evidence-incomplete")):
                with metadata_tools(library, imports):
                    with self.assertRaisesRegex(ValueError, reason):
                        pe.map_symbols(producer, producer, windows_import_library=library)
            with metadata_tools(library):
                for payload in (b"BCRYPT.DLL\0", "Bcrypt.dll".encode("utf-16le")):
                    producer.write_bytes(metadata_image(payload))
                    with self.assertRaisesRegex(ValueError, "still-referenced"):
                        pe.map_symbols(producer, producer, windows_import_library=library)
                producer.write_bytes(metadata_image())
                with patch.object(pe.subprocess, "check_output", side_effect=OSError("native import reader failed")):
                    with self.assertRaises(OSError):
                        pe.map_symbols(producer, producer, windows_import_library=library)

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
            (qt / "sbom/qtbase-6.12.0.spdx.json").write_text('{"files":[]}')
            binary = root / "unknown.dll"
            binary.write_bytes(image())
            producer = root / "bpr_xdiff.dll"
            producer.write_bytes(image())
            info = {"platform": "Windows", "app": str(root / "app.exe"), "helpers": [],
                    "qt_root": str(qt), "qt_version": "6.12.0",
                    "private_libraries": {producer.name: str(producer)}}
            with self.assertRaisesRegex(ValueError, "undeclared-pe-producer"):
                pe.inspect(binary, info, root, private_producers=tuple(package.runtime.private_library_producers(info)))

    def test_pe_module_documents_bind_exact_svg_and_webp_producers(self):
        for module, name, relative in (("qtsvg", "Qt6Svg.dll", "bin/Qt6Svg.dll"),
                                       ("qtimageformats", "qwebp.dll", "plugins/imageformats/qwebp.dll")):
            with self.subTest(module=module), ContainedTemporaryDirectory() as directory:
                root = Path(directory).resolve()
                qt = root / "qt"
                (qt / "sbom").mkdir(parents=True)
                (qt / "sbom/qtbase-6.12.0.spdx.json").write_text('{"files":[]}')
                binary, producer = root / name, qt / relative
                producer.parent.mkdir(parents=True)
                binary.write_bytes(image())
                producer.write_bytes(image())
                item = {"SPDXID": "file", "fileName": relative, "fileTypes": ["BINARY"],
                        "checksums": [{"algorithm": "SHA256", "checksumValue": hashlib.sha256(image()).hexdigest()}]}
                path = qt / f"sbom/{module}-6.12.0.spdx.json"
                def document(files):
                    path.write_text(json.dumps({"files": files}))
                document([item])
                info = {"qt_root": str(qt), "qt_version": "6.12.0"}
                self.assertEqual(pe.upstream_dll(binary, info, root), producer)
                with patch.object(pe, "qt_signed_spdx_match", return_value=False):
                    document([{**item, "checksums": [{"algorithm": "SHA256", "checksumValue": "0" * 64}]}])
                    with self.assertRaisesRegex(ValueError, "spdx-mismatch"):
                        pe.upstream_dll(binary, info, root)
                document([{**item, "checksums": []}])
                with self.assertRaisesRegex(ValueError, "checksum-missing"):
                    pe.upstream_dll(binary, info, root)
                document([item, {**item, "SPDXID": "duplicate-file"}])
                with self.assertRaisesRegex(ValueError, "ambiguous-qt-producer-file"):
                    pe.upstream_dll(binary, info, root)
                # Byte equality at a same-named path outside Qt grants no authority.
                document([{**item, "fileName": "../" + name}])
                with self.assertRaisesRegex(ValueError, "producer-byte-mismatch"):
                    pe.upstream_dll(binary, info, root)
                document([item])
                producer.write_bytes(image() + b"changed")
                with self.assertRaisesRegex(ValueError, "producer-byte-mismatch"):
                    pe.upstream_dll(binary, info, root)
                producer.write_bytes(image())
                document([])
                with self.assertRaisesRegex(ValueError, "undeclared-pe-producer"):
                    pe.upstream_dll(binary, info, root)
                path.unlink()
                with self.assertRaises(FileNotFoundError):
                    pe.upstream_dll(binary, info, root)

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
