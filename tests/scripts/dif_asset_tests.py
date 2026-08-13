#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY_ROOT / "scripts/dif_assets.py"
BOUNDARY_SCRIPT = REPOSITORY_ROOT / "scripts/verify_dif_asset_boundaries.py"


def load_test_module(name: str, path: Path) -> ModuleType:
    specification = importlib.util.spec_from_file_location(name, path)
    if specification is None or specification.loader is None:
        raise RuntimeError(f"could not load {path.relative_to(REPOSITORY_ROOT)}")
    module = importlib.util.module_from_spec(specification)
    sys.modules[specification.name] = module
    specification.loader.exec_module(module)
    return module


DIF = load_test_module("dif_assets", SCRIPT)
BOUNDARY = load_test_module("verify_dif_asset_boundaries", BOUNDARY_SCRIPT)


def _test_arguments(argv: list[str]) -> tuple[list[str], list[str]]:
    unittest_argv = [argv[0]]
    compiler: list[str] = []
    index = 1
    while index < len(argv):
        if argv[index] == "--cxx":
            if index + 1 >= len(argv):
                raise RuntimeError("--cxx requires a compiler path")
            compiler = [argv[index + 1]]
            index += 2
            continue
        unittest_argv.append(argv[index])
        index += 1
    return unittest_argv, compiler


UNITTEST_ARGV, CONFIGURED_CXX = _test_arguments(sys.argv)


def cxx_command() -> list[str]:
    if CONFIGURED_CXX:
        return CONFIGURED_CXX
    configured = os.environ.get("CXX")
    if configured:
        return shlex.split(configured)
    discovered = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    if discovered is None:
        raise AssertionError("a C++ compiler is required for the catalog consumer test")
    return [discovered]


def compile_catalog(catalog: Path) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory(prefix="bpr-dif-catalog-") as directory:
        root = Path(directory)
        shutil.copy2(catalog, root / "dif-sprite-catalog.hpp")
        consumer = root / "catalog-consumer.cpp"
        consumer.write_text(
            '#include "dif-sprite-catalog.hpp"\n'
            "static_assert(bpr::adapters::qt::dif::kSchemaVersion == 1);\n"
            "static_assert(bpr::adapters::qt::dif::kClips.size() == 3);\n"
            "static_assert(bpr::adapters::qt::dif::kCellSize == 256);\n"
            "int main() { return bpr::adapters::qt::dif::kClips[0].id.empty(); }\n",
            encoding="utf-8",
        )
        command = cxx_command()
        executable = Path(command[0]).name.lower()
        if executable in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}:
            arguments = [*command, "/nologo", "/std:c++20", "/Zs", str(consumer)]
        else:
            arguments = [
                *command,
                "-std=c++20",
                "-fsyntax-only",
                "-x",
                "c++",
                str(consumer),
            ]
        return subprocess.run(arguments, check=False, capture_output=True, text=True)


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(DIF.canonical_json_bytes(value))


def synthetic_frame(
    variant: int, greeting: bool = False, size: int = 256
) -> DIF.RgbaImage:
    scale = size // 256
    if scale <= 0 or size % 256:
        raise ValueError("synthetic frame size must be a positive multiple of 256")
    pixels = bytearray(size * size * 4)
    for y in range(40 * scale, 230 * scale):
        for x in range(56 * scale, 200 * scale):
            # A bounded opaque body with transparent surroundings. The slight
            # inset at the top keeps the source obviously non-plate-shaped.
            if y < 56 * scale and (x < 76 * scale or x >= 180 * scale):
                continue
            index = (y * size + x) * 4
            pixels[index : index + 4] = bytes((238, 232, 244, 255))
    for y in range(68 * scale, 110 * scale):
        for x in range(72 * scale, 112 * scale):
            index = (y * size + x) * 4
            pixels[index : index + 4] = bytes((92, 55, 142, 255))
        for x in range(144 * scale, 184 * scale):
            index = (y * size + x) * 4
            pixels[index : index + 4] = bytes((92, 55, 142, 255))
    # A tiny deterministic expression/prop variation exercises real pixel
    # changes without moving the common registration bounds.
    accent_x = (126 + variant) * scale
    accent_y = (124 if not greeting else 116) * scale
    for y in range(accent_y, accent_y + 6 * scale):
        for x in range(accent_x, accent_x + 6 * scale):
            index = (y * size + x) * 4
            pixels[index : index + 4] = bytes((105, 63, 160, 255))
    return DIF.RgbaImage(size, size, bytes(pixels))


def make_pack(root: Path) -> None:
    root.mkdir(parents=True)
    (root / "README.md").write_text(
        "# Synthetic Dif asset pack\n\n"
        "The checked-in PNGs, not a prompt replay, are the source of truth.\n\n"
        "The exact prompt was:\n\n```text\nsynthetic fixture\n```\n",
        encoding="utf-8",
    )
    source_images = {
        "source/dif-neutral-master.png": synthetic_frame(0, size=512),
        "source/calm-idle/frame-000.png": synthetic_frame(0),
        "source/calm-idle/frame-001.png": synthetic_frame(1),
        "source/calm-idle/frame-002.png": synthetic_frame(2),
        "source/greet/frame-000.png": synthetic_frame(3, greeting=True),
        "source/greet/frame-001.png": synthetic_frame(1, greeting=True),
        "source/greet/frame-002.png": synthetic_frame(1),
        "source/rest/frame-000.png": synthetic_frame(0),
    }
    for relative, image in source_images.items():
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(DIF.encode_png(image))

    manifest = {
        "asset_family": "dif",
        "cell": {
            "alpha": "straight",
            "color_space": "srgb",
            "gutter_cells": 1,
            "height": 256,
            "pixel_format": "rgba8",
            "width": 256,
        },
        "clips": [
            {
                "accessible_description": "Dif calmly breathes as a decorative companion.",
                "allowed_travel_px": [0, 0],
                "body_bounds": [54, 38, 202, 230],
                "frame_duration_ms": 125,
                "frames": [
                    "source/calm-idle/frame-000.png",
                    "source/calm-idle/frame-001.png",
                    "source/calm-idle/frame-002.png",
                ],
                "handoff_mean_delta_max": 0,
                "id": "calm_idle",
                "kind": "loop",
                "loop_seam_mean_delta_max": 2,
                "priority": 0,
                "representative_frame": 1,
                "return_target": "calm_idle",
                "strip": "generated/dif-calm-idle.png",
            },
            {
                "accessible_description": "Dif gives one restrained decorative greeting.",
                "allowed_travel_px": [0, 0],
                "body_bounds": [54, 38, 202, 230],
                "frame_duration_ms": 125,
                "frames": [
                    "source/greet/frame-000.png",
                    "source/greet/frame-001.png",
                    "source/greet/frame-002.png",
                ],
                "handoff_mean_delta_max": 2,
                "id": "greet",
                "kind": "one_shot",
                "loop_seam_mean_delta_max": 0,
                "priority": 10,
                "representative_frame": 0,
                "return_target": "calm_idle",
                "strip": "generated/dif-greet.png",
            },
            {
                "accessible_description": "Dif rests awake without procedural motion.",
                "allowed_travel_px": [0, 0],
                "body_bounds": [54, 38, 202, 230],
                "frame_duration_ms": 0,
                "frames": ["source/rest/frame-000.png"],
                "handoff_mean_delta_max": 0,
                "id": "rest",
                "kind": "static",
                "loop_seam_mean_delta_max": 0,
                "priority": 0,
                "representative_frame": 0,
                "return_target": "rest",
                "strip": "generated/dif-rest.png",
            },
        ],
        "limits": DIF.EXPECTED_LIMITS,
        "registration": {
            "baseline_y": 230,
            "edge_clearance_px": 4,
            "foot_tolerance_px": 2,
            "neutral_body_bounds": [54, 38, 202, 230],
            "visual_center_x": 128,
        },
        "schema_version": 1,
    }
    write_json(root / "dif-sprite-manifest.json", manifest)

    source_files = [
        {"path": relative, "sha256": DIF.sha256_file(root / relative)}
        for relative in sorted(source_images)
    ]
    provenance = {
        "asset_family": "dif",
        "authority": {
            "decision_path": "docs/verification/reviews/dif-provenance-v001/decision-subject.txt",
            "decision_subject_sha256": "1" * 64,
            "status": "approved",
        },
        "creation": {
            "creator": "Synthetic test fixture",
            "method_record": "src/adapters/qt/assets/dif/README.md",
            "source_authority": "checked_in_source_bytes",
            "tools": ["tests/scripts/dif_asset_tests.py"],
        },
        "references": [
            {
                "byte_length": 1_777_731,
                "id": "dif_ref_sheet",
                "role": "identity guidance only",
                "sha256": "cf35d96198e20575069b3dd43edd352a04393e457970219738a9e888458729c0",
            },
            {
                "byte_length": 2_002_023,
                "id": "mascot_group",
                "role": "brand-family guidance only",
                "sha256": "5859655aa4c39e50a7e3c522e48f8b34c48bd5749553089be95860e7f2d0f39e",
            },
            {
                "byte_length": 21_969,
                "id": "nib_process_guidance",
                "role": "process guidance only",
                "sha256": "e924c69865ff2998fed3bc5981a405c46960a9ef561947ce64b99fa52e54aeef",
            },
        ],
        "rights": {
            "basis": "Synthetic fixture authority",
            "derivative_scope": "Synthetic test derivatives only",
            "distribution_scope": "Repository tests only",
            "foreign_identity_exclusion": ["AllTheThings", "Mel", "Nib"],
            "notice_disposition": "No product notice claim",
            "notice_required": False,
        },
        "schema_version": 1,
        "source_files": source_files,
    }
    write_json(root / "provenance.json", provenance)
    DIF._write_products(DIF.generated_products(root), root / "generated")


def refresh_source_digest(root: Path, relative: str) -> None:
    provenance_path = root / "provenance.json"
    provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    for item in provenance["source_files"]:
        if item["path"] == relative:
            item["sha256"] = DIF.sha256_file(root / relative)
    write_json(provenance_path, provenance)


def png_chunks(data: bytes) -> list[tuple[bytes, bytes]]:
    if not data.startswith(DIF.PNG_SIGNATURE):
        raise AssertionError("preview is not a PNG")
    chunks: list[tuple[bytes, bytes]] = []
    offset = len(DIF.PNG_SIGNATURE)
    while offset < len(data):
        size = DIF.struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + size]
        expected = DIF.struct.unpack_from(">I", data, offset + 8 + size)[0]
        actual = DIF.zlib.crc32(kind + payload) & 0xFFFFFFFF
        if expected != actual:
            raise AssertionError(f"bad preview chunk CRC: {kind!r}")
        chunks.append((kind, payload))
        offset += 12 + size
    if offset != len(data) or not chunks or chunks[-1][0] != b"IEND":
        raise AssertionError("preview chunks do not consume the PNG")
    return chunks


def make_selected_build(root: Path, profile: str) -> Path:
    build = root / profile
    build.mkdir(parents=True)
    build_testing = "OFF" if profile == "app-release" else "ON"
    build_qt = "OFF" if profile == "core-only" else "ON"
    (build / "CMakeCache.txt").write_text(
        f"BPR_BUILD_QT:BOOL={build_qt}\nBUILD_TESTING:BOOL={build_testing}\n",
        encoding="utf-8",
    )
    sources = [REPOSITORY_ROOT / "src/core/src/model.cpp"]
    if profile == "dev":
        sources.append(REPOSITORY_ROOT / "src/adapters/qt/src/foundation_window.cpp")
    elif profile == "app-release":
        sources.extend(
            [
                REPOSITORY_ROOT / "src/adapters/qt/src/foundation_window.cpp",
                REPOSITORY_ROOT / "src/app/main.cpp",
            ]
        )
    commands = [
        {
            "directory": str(build),
            "command": f"c++ -c {source}",
            "file": str(source),
            "output": str(build / f"{index}.o"),
        }
        for index, source in enumerate(sources)
    ]
    (build / "compile_commands.json").write_text(
        json.dumps(commands), encoding="utf-8"
    )
    (build / "build.ninja").write_text("build all: phony\n", encoding="utf-8")
    if profile != "app-release":
        ctest = build / "tests/CTestTestfile.cmake"
        ctest.parent.mkdir(parents=True)
        ctest.write_text(
            'add_test("dif_assets_contract" "python" '
            '"run_network_isolated_test.py" "dif_asset_tests.py" '
            '"--cxx" "c++")\n'
            'set_tests_properties("dif_assets_contract" PROPERTIES '
            'LABELS "bpr_network_isolated")\n',
            encoding="utf-8",
        )
    if profile in {"dev", "app-release"}:
        application = build / "src/app/better_pull_review"
        application.parent.mkdir(parents=True)
        application.write_bytes(b"BPR_SYNTHETIC_APPLICATION\0")
    if profile == "core-only":
        library = build / "src/core/libbetter_pull_review_core.a"
        library.parent.mkdir(parents=True)
        library.write_bytes(b"BPR_SYNTHETIC_CORE_LIBRARY\0")
    return build


class DifAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.base_temporary = tempfile.TemporaryDirectory(prefix="bpr-dif-base-")
        cls.base_root = Path(cls.base_temporary.name) / "dif"
        make_pack(cls.base_root)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.base_temporary.cleanup()

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="bpr-dif-test-")
        self.root = Path(self.temporary.name) / "dif"
        shutil.copytree(self.base_root, self.root)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_valid_pack_reproduces_every_generated_byte(self) -> None:
        DIF.verify_assets(self.root)
        with tempfile.TemporaryDirectory(prefix="bpr-dif-generate-") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            DIF._write_products(DIF.generated_products(self.root), first)
            DIF._write_products(DIF.generated_products(self.root), second)
            self.assertEqual(DIF._tree_bytes(first), DIF._tree_bytes(second))
            self.assertEqual(
                DIF._tree_bytes(first), DIF._tree_bytes(self.root / "generated")
            )

    def test_cli_validate_generate_verify_and_preview(self) -> None:
        with tempfile.TemporaryDirectory(prefix="bpr-dif-cli-") as directory:
            output = Path(directory) / "generated"
            preview = Path(directory) / "preview"
            commands = (
                (["validate", "--assets", str(self.root)], "DIF_ASSETS_VALID"),
                (["verify", "--assets", str(self.root)], "DIF_ASSETS_REPRODUCIBLE"),
                (
                    ["generate", "--assets", str(self.root), "--out", str(output)],
                    "DIF_ASSETS_GENERATED",
                ),
                (
                    ["preview", "--assets", str(self.root), "--out", str(preview)],
                    "DIF_ASSET_PREVIEWS_GENERATED",
                ),
            )
            for arguments, marker in commands:
                with self.subTest(arguments=arguments):
                    result = subprocess.run(
                        [sys.executable, str(SCRIPT), *arguments],
                        check=False,
                        capture_output=True,
                        text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn(marker, result.stdout)
            self.assertEqual(DIF._tree_bytes(output), DIF._tree_bytes(self.root / "generated"))
            preview_manifest = json.loads(
                (preview / "preview-manifest.json").read_text(encoding="utf-8")
            )
            self.assertIn("appearance-contexts.png", preview_manifest["files"])
            self.assertIn("contact-sheets/calm_idle.png", preview_manifest["files"])
            self.assertIn("overlays/greet.png", preview_manifest["files"])
            motion = (preview / "motion/calm_idle-64px.png").read_bytes()
            chunks = png_chunks(motion)
            kinds = [kind for kind, _payload in chunks]
            self.assertEqual(kinds.count(b"fcTL"), 3)
            self.assertEqual(kinds.count(b"fdAT"), 2)
            animation = next(payload for kind, payload in chunks if kind == b"acTL")
            self.assertEqual(DIF.struct.unpack(">II", animation), (3, 0))
            sequence_numbers = [
                DIF.struct.unpack_from(">I", payload)[0]
                for kind, payload in chunks
                if kind in {b"fcTL", b"fdAT"}
            ]
            self.assertEqual(sequence_numbers, list(range(5)))
            greet = png_chunks((preview / "motion/greet-64px.png").read_bytes())
            greet_animation = next(
                payload for kind, payload in greet if kind == b"acTL"
            )
            self.assertEqual(DIF.struct.unpack(">II", greet_animation), (3, 1))

    def test_png_round_trip_and_all_standard_filters(self) -> None:
        original = synthetic_frame(2)
        encoded = DIF.encode_png(original)
        self.assertEqual(DIF.decode_png_bytes(encoded, "round-trip"), original)

        # Re-encode the first two rows with each legal filter to constrain the
        # controlled decoder independently of the canonical filter-zero writer.
        width = 2
        height = 2
        pixels = bytes(range(width * height * 4))
        image = DIF.RgbaImage(width, height, pixels)
        for filter_type in range(5):
            rows = bytearray()
            previous = bytes(width * 4)
            for row_index in range(height):
                row = pixels[row_index * width * 4 : (row_index + 1) * width * 4]
                encoded_row = bytearray()
                for index, value in enumerate(row):
                    left = row[index - 4] if index >= 4 else 0
                    up = previous[index]
                    up_left = previous[index - 4] if index >= 4 else 0
                    predictors = (
                        0,
                        left,
                        up,
                        (left + up) // 2,
                        DIF._paeth(left, up, up_left),
                    )
                    encoded_row.append((value - predictors[filter_type]) & 0xFF)
                rows.append(filter_type)
                rows.extend(encoded_row)
                previous = row
            header = DIF.struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
            filtered_png = (
                DIF.PNG_SIGNATURE
                + DIF._png_chunk(b"IHDR", header)
                + DIF._png_chunk(b"IDAT", DIF.zlib.compress(bytes(rows), 9))
                + DIF._png_chunk(b"IEND", b"")
            )
            with self.subTest(filter_type=filter_type):
                self.assertEqual(DIF.decode_png_bytes(filtered_png, "filtered"), image)

    def test_fixed_box_resize_preserves_registration_and_zeroes_transparency(self) -> None:
        resized = DIF.resize_box(synthetic_frame(2, size=512))
        self.assertEqual((resized.width, resized.height), (256, 256))
        self.assertEqual(DIF._alpha_bounds(resized), (56, 40, 200, 230))
        for index, alpha in enumerate(resized.pixels[3::4]):
            if alpha == 0:
                self.assertEqual(resized.pixels[index * 4 : index * 4 + 3], b"\0\0\0")

    def test_rejects_ancillary_png_chunks(self) -> None:
        source = self.root / "source/rest/frame-000.png"
        data = source.read_bytes()
        insertion = len(DIF.PNG_SIGNATURE) + 12 + 13
        source.write_bytes(
            data[:insertion]
            + DIF._png_chunk(b"tEXt", b"private-path=/Users/example")
            + data[insertion:]
        )
        refresh_source_digest(self.root, "source/rest/frame-000.png")
        with self.assertRaisesRegex(DIF.AssetError, "unexpected PNG chunk"):
            DIF.compile_assets(self.root)

    def test_rejects_noncanonical_source_png(self) -> None:
        relative = "source/rest/frame-000.png"
        source = self.root / relative
        image = DIF.decode_png(source, relative)
        header = DIF.struct.pack(
            ">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0
        )
        noncanonical = (
            DIF.PNG_SIGNATURE
            + DIF._png_chunk(b"IHDR", header)
            + DIF._png_chunk(
                b"IDAT", DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=1)
            )
            + DIF._png_chunk(b"IEND", b"")
        )
        self.assertNotEqual(noncanonical, DIF.encode_png(image))
        source.write_bytes(noncanonical)
        refresh_source_digest(self.root, relative)
        with self.assertRaisesRegex(DIF.AssetError, "not a canonical product PNG"):
            DIF.compile_assets(self.root)

    def test_rejects_hidden_rgb_in_transparent_pixels(self) -> None:
        relative = "source/rest/frame-000.png"
        source = self.root / relative
        image = DIF.decode_png(source, relative)
        pixels = bytearray(image.pixels)
        pixels[0:4] = bytes((83, 69, 67, 0))
        source.write_bytes(
            DIF.encode_png(DIF.RgbaImage(image.width, image.height, bytes(pixels)))
        )
        refresh_source_digest(self.root, relative)
        with self.assertRaisesRegex(DIF.AssetError, "hidden RGB"):
            DIF.compile_assets(self.root)

    def test_rejects_unsafe_source_path(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][0]["frames"][0] = "source/../outside.png"
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "unsafe path component"):
            DIF.compile_assets(self.root)

    def test_rejects_unknown_or_reordered_clip(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][0]["id"] = "thinking"
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "ordered calm_idle"):
            DIF.compile_assets(self.root)

    def test_rejects_excessive_frame_rate(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][0]["frame_duration_ms"] = 83
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "exceeds 12 fps"):
            DIF.compile_assets(self.root)

    def test_rejects_nonportable_catalog_text(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][0]["accessible_description"] = "Dif smiles \U0001f600"
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "printable ASCII"):
            DIF.compile_assets(self.root)

    def test_rejects_clip_specific_neutral_body_bounds(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][1]["body_bounds"] = [40, 20, 216, 240]
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "must equal neutral_body_bounds"):
            DIF.compile_assets(self.root)

    def test_rejects_undeclared_large_body_travel(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][0]["allowed_travel_px"] = [5, 3]
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "travel x"):
            DIF.compile_assets(self.root)

    def test_rejects_stale_generated_output(self) -> None:
        strip = self.root / "generated/dif-rest.png"
        strip.write_bytes(strip.read_bytes() + b"stale")
        with self.assertRaisesRegex(DIF.AssetError, "stale"):
            DIF.validate_assets(self.root)

    def test_rejects_stale_manifest_catalog_digest(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["clips"][0]["accessible_description"] = (
            "Dif breathes calmly as a decorative companion."
        )
        write_json(path, manifest)
        with self.assertRaisesRegex(DIF.AssetError, "stale"):
            DIF.validate_assets(self.root)

    def test_rejects_stale_digest_inventory(self) -> None:
        path = self.root / "generated/dif-sprite-digests.json"
        inventory = json.loads(path.read_text(encoding="utf-8"))
        inventory["output_sha256"]["dif-rest.png"] = "0" * 64
        write_json(path, inventory)
        with self.assertRaisesRegex(DIF.AssetError, "stale"):
            DIF.validate_assets(self.root)

    def test_rejects_unlisted_asset_file(self) -> None:
        (self.root / "source/extra.png").write_bytes(DIF.encode_png(synthetic_frame(0)))
        with self.assertRaisesRegex(DIF.AssetError, "unexpected=.*extra"):
            DIF.validate_assets(self.root)

    def test_rejects_source_symlink_before_digest_read(self) -> None:
        outside = Path(self.temporary.name) / "outside-private.png"
        outside.write_bytes(DIF.encode_png(synthetic_frame(0)))
        source = self.root / "source/rest/frame-000.png"
        source.unlink()
        try:
            source.symlink_to(outside)
        except OSError as error:
            self.skipTest(f"symbolic links are unavailable: {error}")
        with self.assertRaisesRegex(
            DIF.AssetError,
            r"asset path contains a symbolic link: source/rest/frame-000\.png",
        ):
            DIF.compile_assets(self.root)

    def test_rejects_noncanonical_json_and_duplicate_keys(self) -> None:
        path = self.root / "dif-sprite-manifest.json"
        value = json.loads(path.read_text(encoding="utf-8"))
        path.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(DIF.AssetError, "not canonical"):
            DIF.compile_assets(self.root)
        path.write_text('{"schema_version":1,"schema_version":1}\n', encoding="utf-8")
        with self.assertRaisesRegex(DIF.AssetError, "duplicate JSON key"):
            DIF.compile_assets(self.root)

    def test_generated_catalog_has_fixed_resources_and_no_source_paths(self) -> None:
        catalog = DIF.generated_products(self.root)[DIF.PurePosixPath(DIF.GENERATED_CATALOG)]
        self.assertIn(b":/bpr/dif/dif-calm-idle.png", catalog)
        self.assertIn(b"kManifestSha256", catalog)
        self.assertIn(b"clip_kind::one_shot", catalog)
        self.assertIn(b"clip_kind::static_frame", catalog)
        self.assertNotIn(b"source/", catalog)
        self.assertNotIn(str(self.root).encode("utf-8"), catalog)

    def test_generated_catalog_compiles_in_narrow_consumers(self) -> None:
        production = (
            REPOSITORY_ROOT
            / "src/adapters/qt/assets/dif/generated/dif-sprite-catalog.hpp"
        )
        self.assertTrue(production.is_file(), "the committed production catalog is absent")
        catalogs = (self.root / "generated/dif-sprite-catalog.hpp", production)
        for catalog in catalogs:
            with self.subTest(catalog=catalog):
                result = compile_catalog(catalog)
                self.assertEqual(
                    result.returncode,
                    0,
                    f"catalog consumer compilation failed:\n{result.stdout}{result.stderr}",
                )

    def test_boundary_verifier_accepts_exact_selected_profiles(self) -> None:
        root = Path(self.temporary.name) / "selected-builds"
        for profile in ("dev", "core-only", "app-release"):
            with self.subTest(profile=profile):
                result = BOUNDARY.validate_profile(
                    REPOSITORY_ROOT,
                    make_selected_build(root, profile),
                    profile,
                )
                self.assertGreater(result["first_party_translation_units"], 0)

    def test_boundary_verifier_rejects_cut_ctest_registration(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "core-only"
        )
        (build / "tests/CTestTestfile.cmake").write_text(
            "# registration cut\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(
            BOUNDARY.BoundaryError, "registered exactly once"
        ):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "core-only")

    def test_boundary_verifier_rejects_qt_app_probe_in_core_only(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "core-only"
        )
        ctest = build / "tests/CTestTestfile.cmake"
        ctest.write_text(
            ctest.read_text(encoding="utf-8")
            + 'add_test("test_network_isolation_application" '
            '"app_better_pull_review")\n',
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            BOUNDARY.BoundaryError, "Qt application probe"
        ):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "core-only")

    def test_boundary_verifier_rejects_selected_art_tool_dependency(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "dev"
        )
        (build / "build.ninja").write_text(
            "build generated: CUSTOM_COMMAND ImageMagick\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "imagemagick"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "dev")

    def test_boundary_verifier_rejects_qt_source_in_core_only(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "core-only"
        )
        path = build / "compile_commands.json"
        commands = json.loads(path.read_text(encoding="utf-8"))
        source = REPOSITORY_ROOT / "src/adapters/qt/src/foundation_window.cpp"
        commands.append(
            {
                "directory": str(build),
                "command": f"c++ -c {source}",
                "file": str(source),
                "output": str(build / "qt.o"),
            }
        )
        path.write_text(json.dumps(commands), encoding="utf-8")
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "Qt/app source"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "core-only")

    def test_boundary_verifier_rejects_test_source_in_release_app(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "app-release"
        )
        path = build / "compile_commands.json"
        commands = json.loads(path.read_text(encoding="utf-8"))
        source = REPOSITORY_ROOT / "tests/core/core_tests.cpp"
        commands.append(
            {
                "directory": str(build),
                "command": f"c++ -c {source}",
                "file": str(source),
                "output": str(build / "test.o"),
            }
        )
        path.write_text(json.dumps(commands), encoding="utf-8")
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "test source"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "app-release")

    def test_boundary_verifier_rejects_dif_marker_in_application(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "app-release"
        )
        application = build / "src/app/better_pull_review"
        application.write_bytes(
            application.read_bytes() + b":/bpr/dif/dif-calm-idle.png"
        )
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "binary embeds"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "app-release")

    def test_boundary_verifier_rejects_dif_marker_in_core_artifact(self) -> None:
        build = make_selected_build(
            Path(self.temporary.name) / "selected-builds", "core-only"
        )
        library = build / "src/core/libbetter_pull_review_core.a"
        library.write_bytes(library.read_bytes() + b"dif-sprite-manifest.json")
        with self.assertRaisesRegex(
            BOUNDARY.BoundaryError, "library artifact embeds"
        ):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "core-only")

    def test_boundary_verifier_rejects_network_capable_asset_tool(self) -> None:
        path = Path(self.temporary.name) / "network_asset_tool.py"
        path.write_text("import urllib.request\n", encoding="utf-8")
        with self.assertRaisesRegex(
            BOUNDARY.BoundaryError, "unapproved capability"
        ):
            BOUNDARY.validate_offline_tool(path)

    def test_boundary_verifier_rejects_private_path_in_asset_text(self) -> None:
        readme = self.root / "README.md"
        readme.write_text(
            readme.read_text(encoding="utf-8") + "private=/Users/example/repo\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            BOUNDARY.BoundaryError, "private/active marker"
        ):
            BOUNDARY.validate_asset_file_hygiene(self.root)

    def test_boundary_verifier_requires_prompt_authority_disposition(self) -> None:
        (self.root / "README.md").write_text(
            "# Prompt is not an asset authority\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "prompt disposition"):
            BOUNDARY.validate_asset_file_hygiene(self.root)

    def test_live_dif_tool_and_runtime_source_boundary_pass(self) -> None:
        tool = BOUNDARY.validate_offline_tool(BOUNDARY_SCRIPT.parent / "dif_assets.py")
        self.assertIn("zlib", tool["imports"])
        runtime = BOUNDARY.validate_runtime_source_absence(REPOSITORY_ROOT)
        self.assertGreater(runtime["checked_files"], 0)
        assets = BOUNDARY.validate_asset_tree(REPOSITORY_ROOT)
        self.assertTrue(assets["prompt_recorded_as_provenance_only"])


if __name__ == "__main__":
    unittest.main(argv=UNITTEST_ARGV)
