#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import os
import shlex
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from types import ModuleType


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import ContainedTemporaryDirectory, contained_temporary_directory  # noqa: E402

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


def compile_catalog(catalog: Path, version: int = 1, clip_count: int = 3) -> subprocess.CompletedProcess[str]:
    with contained_temporary_directory(prefix="bpr-dif-catalog-") as directory:
        root = Path(directory)
        shutil.copy2(catalog, root / "dif-sprite-catalog.hpp")
        consumer = root / "catalog-consumer.cpp"
        consumer.write_text(
            '#include "dif-sprite-catalog.hpp"\n'
            f"static_assert(bpr::adapters::qt::dif::kSchemaVersion == {version});\n"
            f"static_assert(bpr::adapters::qt::dif::kClips.size() == {clip_count});\n"
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


def registered_frame(variant: int = 0, *, limb: bool = False, blink: bool = False) -> DIF.RgbaImage:
    pixels = bytearray(256 * 256 * 4)

    def rectangle(box: tuple[int, int, int, int], color: tuple[int, int, int, int]) -> None:
        left, top, right, bottom = box
        for y in range(top, bottom):
            for x in range(left, right):
                pixels[(y * 256 + x) * 4:(y * 256 + x + 1) * 4] = bytes(color)

    for box in ((80, 40, 176, 110), (96, 116, 160, 195),
                (94, 201, 118, 223), (138, 201, 162, 223)):
        rectangle(box, (238, 232, 244, 255))
    rectangle((110, 176, 117, 184), (92, 55, 142, 255))
    rectangle((101, 211, 108, 218), (92, 55, 142, 255))
    rectangle((145, 211, 152, 218), (92, 55, 142, 255))
    if variant:
        y = 70 if blink else 140
        rectangle((123, y, 128, y + 4), (105 + variant, 63, 160, 255))
    if limb:
        rectangle((160, 124, 220, 144), (92, 55, 142, 255))
    return DIF.RgbaImage(256, 256, bytes(pixels))


def make_v2_pack(root: Path) -> None:
    make_pack(root)
    manifest = json.loads((root / "dif-sprite-manifest.json").read_text())
    provenance = json.loads((root / "provenance.json").read_text())
    shutil.rmtree(root / "source")
    shutil.rmtree(root / "generated")
    manifest["schema_version"] = 2
    manifest["limits"] = DIF.V2_LIMITS
    manifest["registration"] = {
        "baseline_y": 223, "edge_clearance_px": 16, "foot_tolerance_px": 1,
        "neutral_body_bounds": [56, 22, 200, 224], "visual_center_x": 128,
        "anchor_regions": {
            "head": [64, 24, 192, 112], "body": [88, 170, 168, 196],
            "left_foot": [86, 197, 124, 226], "right_foot": [132, 197, 170, 226],
        },
    }
    template = manifest["clips"][0]
    manifest["clips"] = []
    source_images = {"source/dif-neutral-master.png": DIF._scale_nearest(registered_frame(), 1024, 1024)}
    # The immutable design input, not compiler constants, supplies the oracle.
    contract = json.loads((REPOSITORY_ROOT / "docs/rfcs/assets/bpr-dif-integration-v1/specs/CLIP-CONTRACT.json").read_text())
    for spec in contract["clips"]:
        clip = dict(template)
        clip_id = spec["id"]
        clip.update({
            "id": clip_id, "kind": spec["kind"], "frame_duration_ms": spec["frame_duration_ms"],
            "representative_frame": spec["representative_frame"], "priority": spec["priority"],
            "return_target": spec["asset_return_target"], "body_bounds": [56, 22, 200, 224],
            "content_bounds": [56, 22, 224, 226], "head_travel_px": [0, 0],
            "handoff_mean_delta_max": 0, "loop_seam_mean_delta_max": 2 if clip_id == "calm_idle" else 0,
            "frames": [f"source/{clip_id.replace('_', '-')}/frame-{i:03d}.png" for i in range(spec["frame_count"])],
            "strip": f"generated/dif-{clip_id.replace('_', '-')}.png",
        })
        for index, path in enumerate(clip["frames"]):
            neutral = index in spec["neutral_frames"]
            source_images[path] = registered_frame(
                0 if neutral else index + 1,
                limb=not neutral and clip_id not in {"calm_idle", "blink"},
                blink=clip_id == "blink",
            )
        bounds = [DIF._alpha_bounds(source_images[path]) for path in clip["frames"]]
        clip["content_bounds"] = [min(box[0] for box in bounds), min(box[1] for box in bounds),
                                  max(box[2] for box in bounds), max(box[3] for box in bounds)]
        manifest["clips"].append(clip)
    for relative, image in source_images.items():
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(DIF.encode_png(image))
    provenance["source_files"] = [
        {"path": path, "sha256": DIF.sha256_file(root / path)} for path in sorted(source_images)
    ]
    write_json(root / "provenance.json", provenance)
    write_json(root / "dif-sprite-manifest.json", manifest)
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
        application.write_bytes(b"BPR_SYNTHETIC_APPLICATION\0" + b"".join(
            (REPOSITORY_ROOT / BOUNDARY.PRODUCTION_ROOT / name).read_bytes()
            for name in BOUNDARY.PRODUCTION_STRIPS
        ))
        qrc = build / ".qt/rcc/bpr_dif.qrc"
        qrc.parent.mkdir(parents=True)
        qrc.write_text('<RCC><qresource prefix="/bpr/dif">' + ''.join(
            f'<file alias="{name}">{REPOSITORY_ROOT / BOUNDARY.PRODUCTION_ROOT / name}</file>'
            for name in BOUNDARY.PRODUCTION_STRIPS
        ) + '</qresource></RCC>', encoding="utf-8")
    if profile == "core-only":
        library = build / "src/core/libbetter_pull_review_core.a"
        library.parent.mkdir(parents=True)
        library.write_bytes(b"BPR_SYNTHETIC_CORE_LIBRARY\0")
    return build


class DifAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.base_temporary = ContainedTemporaryDirectory(prefix="bpr-dif-base-")
        cls.base_root = Path(cls.base_temporary.name) / "dif"
        make_pack(cls.base_root)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.base_temporary.cleanup()

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory(prefix="bpr-dif-test-")
        self.root = Path(self.temporary.name) / "dif"
        shutil.copytree(self.base_root, self.root)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_valid_pack_reproduces_every_generated_byte(self) -> None:
        DIF.verify_assets(self.root)
        with contained_temporary_directory(prefix="bpr-dif-generate-") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            DIF._write_products(DIF.generated_products(self.root), first)
            DIF._write_products(DIF.generated_products(self.root), second)
            self.assertEqual(DIF._tree_bytes(first), DIF._tree_bytes(second))
            self.assertEqual(
                DIF._tree_bytes(first), DIF._tree_bytes(self.root / "generated")
            )

    def test_cli_validate_generate_verify_and_preview(self) -> None:
        with contained_temporary_directory(prefix="bpr-dif-cli-") as directory:
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

    def test_fixed_box_resize_clears_color_when_alpha_rounds_to_zero(self) -> None:
        pixels = bytearray(512 * 512 * 4)
        pixels[:4] = bytes((120, 50, 90, 1))
        for y in range(4, 6):
            for x in range(4, 6):
                offset = (y * 512 + x) * 4
                pixels[offset : offset + 4] = bytes((25, 75, 125, 255))
        image = DIF.RgbaImage(512, 512, bytes(pixels))
        resized = DIF.resize_box(image, clear_rounded_transparency=True)
        self.assertEqual(resized.pixels[:4], b"\0\0\0\0")
        retained = (2 * 256 + 2) * 4
        self.assertEqual(
            resized.pixels[retained : retained + 4], bytes((25, 75, 125, 255))
        )
        self.assertEqual(DIF.resize_box(image).pixels[:4], bytes((120, 50, 90, 0)))

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

    def _rebuild_source_frame(
        self,
        relative: str,
        *,
        idat_payloads,
        iend_payload: bytes = b"",
    ) -> None:
        """Replace a source frame with a hand-built PNG and re-stamp its digest."""
        source = self.root / relative
        image = DIF.decode_png(source, relative)
        header = DIF.struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0)
        rebuilt = DIF.PNG_SIGNATURE + DIF._png_chunk(b"IHDR", header)
        for payload in idat_payloads(image):
            rebuilt += DIF._png_chunk(b"IDAT", payload)
        rebuilt += DIF._png_chunk(b"IEND", iend_payload)
        source.write_bytes(rebuilt)
        refresh_source_digest(self.root, relative)

    def test_accepts_a_recompression_that_changes_no_pixel(self) -> None:
        # BPR-0378. The canon used to compare the file against a fresh
        # encode_png of its own pixels, which pinned the exact DEFLATE output
        # and so failed under zlib-ng, where the same pixels compress smaller.
        # A different compression level stands in for a different zlib.
        relative = "source/rest/frame-000.png"
        original = (self.root / relative).read_bytes()
        self._rebuild_source_frame(
            relative,
            idat_payloads=lambda image: [
                DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=1)
            ],
        )
        self.assertNotEqual((self.root / relative).read_bytes(), original)
        DIF.compile_assets(self.root)

    def test_rejects_a_source_png_split_across_two_idat_chunks(self) -> None:
        relative = "source/rest/frame-000.png"

        def split(image):
            whole = DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=9)
            return [whole[:20], whole[20:]]

        self._rebuild_source_frame(relative, idat_payloads=split)
        with self.assertRaisesRegex(DIF.AssetError, "exactly one IDAT chunk"):
            DIF.compile_assets(self.root)

    def test_rejects_a_source_png_with_a_non_empty_iend_payload(self) -> None:
        relative = "source/rest/frame-000.png"
        self._rebuild_source_frame(
            relative,
            idat_payloads=lambda image: [
                DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=9)
            ],
            iend_payload=b"x",
        )
        with self.assertRaisesRegex(DIF.AssetError, "empty IEND payload"):
            DIF.compile_assets(self.root)

    def test_rejects_a_source_png_row_filtered_with_sub(self) -> None:
        relative = "source/rest/frame-000.png"

        def sub_filter_first_row(image):
            stride = image.width * 4
            rows = bytearray()
            for row_index in range(image.height):
                row = image.pixels[row_index * stride : (row_index + 1) * stride]
                if row_index == 0:
                    rows.append(1)
                    rows.extend(
                        (value - (row[index - 4] if index >= 4 else 0)) & 0xFF
                        for index, value in enumerate(row)
                    )
                else:
                    rows.append(0)
                    rows.extend(row)
            return [DIF.zlib.compress(bytes(rows), level=9)]

        self._rebuild_source_frame(relative, idat_payloads=sub_filter_first_row)
        with self.assertRaisesRegex(DIF.AssetError, "filters every row with 0"):
            DIF.compile_assets(self.root)

    def test_an_edited_pixel_is_caught_by_provenance_not_by_the_canon(self) -> None:
        # BPR-0378's required final state says a hand-edited sprite must still
        # fail. It does, and this records which check does it: re-encoding the
        # edited pixels through encode_png produces a perfectly canonical PNG,
        # so the canon accepts it and the per-file digest rejects it.
        relative = "source/rest/frame-000.png"
        source = self.root / relative
        image = DIF.decode_png(source, relative)
        pixels = bytearray(image.pixels)
        pixels[4:8] = bytes((pixels[4] ^ 0xFF, pixels[5], pixels[6], pixels[7]))
        edited = DIF.encode_png(DIF.RgbaImage(image.width, image.height, bytes(pixels)))
        source.write_bytes(edited)

        # The canon itself is satisfied by the edited file.
        self.assertEqual(
            DIF.decode_png_bytes(edited, relative, require_canonical_form=True).pixels,
            bytes(pixels),
        )
        # Without re-stamping the digest, provenance is what fails.
        with self.assertRaisesRegex(DIF.AssetError, "provenance source digest"):
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

    def _restamp_generated_digest(self, name: str) -> None:
        """Point the inventory at whatever `generated/<name>` now contains."""
        path = self.root / "generated/dif-sprite-digests.json"
        inventory = json.loads(path.read_text(encoding="utf-8"))
        inventory["output_sha256"][name] = DIF.sha256_bytes(
            (self.root / "generated" / name).read_bytes()
        )
        write_json(path, inventory)

    def test_accepts_a_generated_product_recompressed_at_another_level(self) -> None:
        # BPR-0408. The generated-tree comparison used to pin DEFLATE the same
        # way decode_png did, so this is the property being traded away.
        name = "dif-rest.png"
        path = self.root / "generated" / name
        image = DIF.decode_png(path, name)
        path.write_bytes(
            DIF.PNG_SIGNATURE
            + DIF._png_chunk(
                b"IHDR",
                DIF.struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0),
            )
            + DIF._png_chunk(
                b"IDAT", DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=1)
            )
            + DIF._png_chunk(b"IEND", b"")
        )
        self._restamp_generated_digest(name)
        DIF.verify_assets(self.root)

    def test_rejects_a_generated_product_whose_pixels_differ(self) -> None:
        # The digest is re-stamped so the inventory agrees with the file on
        # disk. That leaves the decoded-content comparison as the only check
        # that can reject this, which is the point.
        name = "dif-rest.png"
        path = self.root / "generated" / name
        image = DIF.decode_png(path, name)
        pixels = bytearray(image.pixels)
        pixels[0] ^= 0xFF
        path.write_bytes(
            DIF.encode_png(DIF.RgbaImage(image.width, image.height, bytes(pixels)))
        )
        self._restamp_generated_digest(name)
        with self.assertRaisesRegex(DIF.AssetError, "stale"):
            DIF.validate_assets(self.root)

    def test_rejects_a_generated_product_that_is_not_in_canonical_form(self) -> None:
        # Same pixels, two IDAT chunks, digest re-stamped. Only the canonical
        # form requirement can reject it.
        name = "dif-rest.png"
        path = self.root / "generated" / name
        image = DIF.decode_png(path, name)
        whole = DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=9)
        path.write_bytes(
            DIF.PNG_SIGNATURE
            + DIF._png_chunk(
                b"IHDR",
                DIF.struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0),
            )
            + DIF._png_chunk(b"IDAT", whole[:20])
            + DIF._png_chunk(b"IDAT", whole[20:])
            + DIF._png_chunk(b"IEND", b"")
        )
        self._restamp_generated_digest(name)
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
        catalogs = (
            (self.root / "generated/dif-sprite-catalog.hpp", 1, 3),
            (production, 2, 10),
        )
        for catalog, version, clip_count in catalogs:
            with self.subTest(catalog=catalog):
                result = compile_catalog(catalog, version, clip_count)
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

    def test_boundary_verifier_finds_a_renamed_bundled_application(self) -> None:
        """BPR-0395. The release executable's name is not `better_pull_review`.

        BPR-0328 renamed the tests-disabled macOS executable to
        `BetterPullReview` and the literal-name lookup kept searching for the
        old one, so a clean `app-release` graph resolved to zero candidates. It
        looked healthy only where a stale pre-rename bundle happened to remain.
        """
        build = make_selected_build(
            Path(self.temporary.name) / "renamed-builds", "app-release"
        )
        (build / "src/app/better_pull_review").unlink()
        bundle = build / "src/app/BetterPullReview.app/Contents"
        (bundle / "MacOS").mkdir(parents=True)
        (bundle / "MacOS/BetterPullReview").write_bytes(
            b"BPR_SYNTHETIC_APPLICATION\0"
        )
        (bundle / "Info.plist").write_text(
            "<plist><dict>"
            "<key>CFBundleExecutable</key><string>BetterPullReview</string>"
            "</dict></plist>\n",
            encoding="utf-8",
        )
        found = BOUNDARY._discover_application(build)
        self.assertEqual(found.name, "BetterPullReview")

    def test_boundary_verifier_refuses_a_stale_second_application(self) -> None:
        """Exactly one stays the rule: two means one is stale."""
        build = make_selected_build(
            Path(self.temporary.name) / "stale-builds", "app-release"
        )
        bundle = build / "src/app/BetterPullReview.app/Contents"
        (bundle / "MacOS").mkdir(parents=True)
        (bundle / "MacOS/BetterPullReview").write_bytes(
            b"BPR_SYNTHETIC_APPLICATION\0"
        )
        (bundle / "Info.plist").write_text(
            "<plist><dict>"
            "<key>CFBundleExecutable</key><string>BetterPullReview</string>"
            "</dict></plist>\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "found 2"):
            BOUNDARY._discover_application(build)

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
            application.read_bytes() + b":/bpr/dif/dif-unaccepted.png"
        )
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "binary embeds"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "app-release")

    def test_boundary_verifier_requires_all_compiled_production_bytes(self) -> None:
        build = make_selected_build(Path(self.temporary.name) / "missing-bytes", "dev")
        (build / "src/app/better_pull_review").write_bytes(b"no-resources")
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "lacks accepted compiled bytes"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "dev")

    def test_boundary_verifier_rejects_extra_resource_alias(self) -> None:
        build = make_selected_build(Path(self.temporary.name) / "extra-resource", "dev")
        qrc = build / ".qt/rcc/bpr_dif.qrc"
        qrc.write_text(qrc.read_text().replace('</qresource>', '<file alias="raw.png">raw.png</file></qresource>'))
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "accepted ten strips"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "dev")

    def test_boundary_verifier_allows_exact_generated_graph_paths(self) -> None:
        build = make_selected_build(Path(self.temporary.name) / "accepted-graph", "dev")
        graph = build / "build.ninja"
        graph.write_text(graph.read_text() + "\n" + " ".join(
            f"{REPOSITORY_ROOT}/{BOUNDARY.PRODUCTION_ROOT}/{name}" for name in BOUNDARY.PRODUCTION_PRODUCTS
        ))
        BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "dev")
        graph.write_text(graph.read_text() + " src/adapters/qt/assets/dif/generated/dif-sprite-digests.json")
        with self.assertRaisesRegex(BOUNDARY.BoundaryError, "forbidden"):
            BOUNDARY.validate_profile(REPOSITORY_ROOT, build, "dev")

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


class SchemaTwoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temporary = ContainedTemporaryDirectory(prefix="bpr-dif-v2-")
        cls.base = Path(cls.temporary.name) / "base"
        make_v2_pack(cls.base)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary.cleanup()

    def setUp(self) -> None:
        self.root = Path(self.temporary.name) / "case"
        shutil.copytree(self.base, self.root)

    def tearDown(self) -> None:
        shutil.rmtree(self.root)

    def manifest(self) -> dict:
        return json.loads((self.root / "dif-sprite-manifest.json").read_text())

    def write_manifest(self, manifest: dict) -> None:
        write_json(self.root / "dif-sprite-manifest.json", manifest)

    def replace_frame(self, relative: str, image: DIF.RgbaImage) -> None:
        (self.root / relative).write_bytes(DIF.encode_png(image))
        refresh_source_digest(self.root, relative)

    def test_target_catalog_and_all_gutters(self) -> None:
        manifest, _, clips = DIF.compile_assets(self.root)
        self.assertEqual(sum(len(clip.frames) for clip in clips), 71)
        self.assertEqual(sum(len(clip.strip.pixels) for clip in clips), 23_855_104)
        for clip in clips:
            self.assertEqual(clip.strip.width, (len(clip.frames) + 2) * 256)
            self.assertEqual(clip.strip.height, 256)
            for i, frame in enumerate(clip.frames):
                self.assertEqual(DIF._region_pixels(clip.strip, [(i + 1) * 256, 0, (i + 2) * 256, 256]), frame)
            for x in (0, clip.strip.width - 256):
                self.assertFalse(any(DIF._region_pixels(clip.strip, [x, 0, x + 256, 256]).pixels))
        products = DIF.generated_products(self.root)
        catalog = products[DIF.PurePosixPath(DIF.GENERATED_CATALOG)].decode()
        self.assertIn("kSchemaVersion{2}", catalog)
        self.assertIn("clip_descriptor, 10>", catalog)
        for clip in clips:
            self.assertIn(DIF.sha256_bytes(clip.strip.pixels), catalog)
            self.assertIn(f"256, 256, 256, {clip.strip.width}, 256, {len(clip.strip.pixels)}", catalog)
        result = compile_catalog(self.root / "generated/dif-sprite-catalog.hpp", 2, 10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        DIF.verify_assets(self.root)

    def test_master_downsample_ignores_color_under_rounded_off_alpha(self) -> None:
        path = "source/dif-neutral-master.png"
        master = DIF.decode_png(self.root / path, path)
        pixels = bytearray(master.pixels)
        offset = (20 * master.width + 20) * 4
        self.assertEqual(pixels[offset : offset + 4], b"\0\0\0\0")
        pixels[offset : offset + 4] = bytes((120, 50, 90, 1))
        self.replace_frame(path, DIF.RgbaImage(master.width, master.height, bytes(pixels)))
        _, _, clips = DIF.compile_assets(self.root)
        neutral_pixel = (5 * 256 + 5) * 4
        self.assertEqual(
            clips[0].frames[0].pixels[neutral_pixel : neutral_pixel + 4],
            b"\0\0\0\0",
        )

    def test_rejects_unknown_reordered_and_missing_ids(self) -> None:
        original = self.manifest()
        for change in ("unknown", "reorder", "missing"):
            manifest = json.loads(json.dumps(original))
            if change == "unknown":
                manifest["clips"][3]["id"] = "surprise"
            elif change == "reorder":
                manifest["clips"][3:5] = reversed(manifest["clips"][3:5])
            else:
                manifest["clips"].pop()
            self.write_manifest(manifest)
            with self.subTest(change=change), self.assertRaises(DIF.AssetError):
                DIF._validate_manifest(self.root)

    def test_rejects_bad_counts_times_returns_and_limits(self) -> None:
        original = self.manifest()
        changes = [
            ("frame_duration_ms", 83), ("frame_duration_ms", 151),
            ("return_target", "greet"), ("return_target", ["calm_idle"]),
            ("representative_frame", 99), ("priority", True),
            ("frames", original["clips"][1]["frames"][:-1]),
        ]
        for field, value in changes:
            manifest = json.loads(json.dumps(original))
            manifest["clips"][1][field] = value
            self.write_manifest(manifest)
            with self.subTest(field=field, value=value), self.assertRaises(DIF.AssetError):
                DIF._validate_manifest(self.root)
        for field in ("max_complete_decoded_bytes", "max_content_frames", "max_clips"):
            manifest = json.loads(json.dumps(original))
            manifest["limits"][field] += 1
            self.write_manifest(manifest)
            with self.subTest(field=field), self.assertRaisesRegex(DIF.AssetError, "fixed RFC limits"):
                DIF._validate_manifest(self.root)

    def test_rejects_wrong_padding_and_canonical_paths(self) -> None:
        original = self.manifest()
        for value in (0, 2, True):
            manifest = json.loads(json.dumps(original))
            manifest["cell"]["gutter_cells"] = value
            self.write_manifest(manifest)
            with self.subTest(value=value), self.assertRaisesRegex(DIF.AssetError, "cell contract"):
                DIF._validate_manifest(self.root)
        manifest = json.loads(json.dumps(original))
        manifest["clips"][0]["frames"][0] = "source//calm-idle/frame-000.png"
        self.write_manifest(manifest)
        with self.assertRaisesRegex(DIF.AssetError, "canonical path"):
            DIF._validate_manifest(self.root)

    def test_rejects_displaced_body_and_each_foot_with_unchanged_annotations(self) -> None:
        manifest = self.manifest()
        relative = manifest["clips"][1]["frames"][3]
        manifest["clips"][1]["content_bounds"] = [72, 32, 224, 230]
        self.write_manifest(manifest)
        original = DIF.decode_png(self.root / relative, relative)
        for name, dx, dy in (("body", 3, 0), ("left_foot", 3, 0),
                             ("right_foot", 3, 0), ("head", 3, 0),
                             ("left_foot", 0, 2), ("right_foot", 0, 2)):
            box = manifest["registration"]["anchor_regions"][name]
            # Every subcase starts from the immutable original pixels, not the
            # previous modified file. Only this named anchor is displaced.
            pixels = bytearray(original.pixels)
            left, top, right, bottom = box
            for y in range(top, bottom):
                for x in range(left, right):
                    source_x, source_y = x - dx, y - dy
                    pixel = (y * 256 + x) * 4
                    source = (source_y * 256 + source_x) * 4
                    pixels[pixel:pixel + 4] = (
                        original.pixels[source:source + 4]
                        if source_x >= left and source_y >= top else bytes(4)
                    )
            self.replace_frame(relative, DIF.RgbaImage(256, 256, bytes(pixels)))
            expected = "head travel" if name == "head" else f"changes {name} anchor pixels"
            with self.subTest(name=name, dx=dx, dy=dy), self.assertRaisesRegex(DIF.AssetError, expected):
                DIF.compile_assets(self.root)

    def test_rejects_missing_content_and_false_anchor_annotation(self) -> None:
        relative = self.manifest()["clips"][1]["frames"][3]
        self.replace_frame(relative, DIF.RgbaImage(256, 256, bytes(256 * 256 * 4)))
        with self.assertRaisesRegex(DIF.AssetError, "no visible content"):
            DIF.compile_assets(self.root)
        self.replace_frame(relative, registered_frame(3, limb=True))
        manifest = self.manifest()
        manifest["registration"]["anchor_regions"]["body"] = [100, 170, 120, 190]
        self.write_manifest(manifest)
        with self.assertRaisesRegex(DIF.AssetError, "enclose the visible anchor"):
            DIF.compile_assets(self.root)

    def test_rejects_non_neutral_return_and_blink_body_change(self) -> None:
        relative = self.manifest()["clips"][1]["frames"][-1]
        self.replace_frame(relative, registered_frame(1))
        with self.assertRaisesRegex(DIF.AssetError, "exact neutral entry and return"):
            DIF.compile_assets(self.root)
        self.replace_frame(relative, registered_frame())
        relative = self.manifest()["clips"][3]["frames"][2]
        self.replace_frame(relative, registered_frame(2))
        with self.assertRaisesRegex(DIF.AssetError, "locked blink body"):
            DIF.compile_assets(self.root)

    def test_generated_gutter_corruption_fails_even_with_restamped_digest(self) -> None:
        name = "dif-calm-idle.png"
        path = self.root / "generated" / name
        image = DIF.decode_png_bytes(path.read_bytes(), name, max_dimension=6656)
        for x in (0, image.width - 1):
            pixels = bytearray(image.pixels)
            pixels[x * 4:x * 4 + 4] = bytes((1, 2, 3, 255))
            path.write_bytes(DIF.encode_png(DIF.RgbaImage(image.width, 256, bytes(pixels))))
            inventory_path = self.root / "generated/dif-sprite-digests.json"
            inventory = json.loads(inventory_path.read_text())
            inventory["output_sha256"][name] = DIF.sha256_file(path)
            write_json(inventory_path, inventory)
            with self.subTest(x=x), self.assertRaisesRegex(DIF.AssetError, "stale"):
                DIF.validate_assets(self.root)

    def test_verify_accepts_other_compressor_and_rejects_stale_payload_digest(self) -> None:
        name = "dif-calm-idle.png"
        path = self.root / "generated" / name
        image = DIF.decode_png_bytes(path.read_bytes(), name, max_dimension=6656)
        path.write_bytes(DIF.PNG_SIGNATURE + DIF._png_chunk(b"IHDR", DIF.struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0))
                         + DIF._png_chunk(b"IDAT", DIF.zlib.compress(DIF._filtered_rgba_rows(image), level=1))
                         + DIF._png_chunk(b"IEND", b""))
        with self.assertRaisesRegex(DIF.AssetError, "stale"):
            DIF.validate_assets(self.root)
        inventory_path = self.root / "generated/dif-sprite-digests.json"
        inventory = json.loads(inventory_path.read_text())
        inventory["output_sha256"][name] = DIF.sha256_file(path)
        write_json(inventory_path, inventory)
        DIF.verify_assets(self.root)

    def test_rejects_broad_envelope_and_static_alias(self) -> None:
        manifest = self.manifest()
        manifest["clips"][0]["content_bounds"] = [64, 32, 224, 230]
        self.write_manifest(manifest)
        with self.assertRaisesRegex(DIF.AssetError, "measured content union"):
            DIF.compile_assets(self.root)
        manifest["clips"][0]["content_bounds"] = [80, 40, 176, 223]
        manifest["clips"][1]["content_bounds"] = [80, 40, 176, 223]
        self.write_manifest(manifest)
        for relative in manifest["clips"][1]["frames"]:
            self.replace_frame(relative, registered_frame())
        with self.assertRaisesRegex(DIF.AssetError, "alias static neutral"):
            DIF.compile_assets(self.root)

    def test_rejects_source_size_before_digest_read(self) -> None:
        relative = self.manifest()["clips"][0]["frames"][0]
        with (self.root / relative).open("wb") as output:
            output.truncate(DIF.MAX_SOURCE_FILE_BYTES + 1)
        with self.assertRaisesRegex(DIF.AssetError, "source PNG byte limit"):
            DIF.compile_assets(self.root)

    def test_complete_resource_budget_includes_twenty_gutters(self) -> None:
        self.assertEqual(DIF._decoded_resource_bytes(71, 10, DIF.V2_LIMITS), (18_612_224, 23_855_104))
        self.assertEqual(DIF._decoded_resource_bytes(76, 10, DIF.V2_LIMITS), (19_922_944, 25_165_824))
        for count in (77, 2**64, True):
            with self.subTest(count=count), self.assertRaises(DIF.AssetError):
                DIF._decoded_resource_bytes(count, 10, DIF.V2_LIMITS)
        with self.assertRaisesRegex(DIF.AssetError, "complete decoded-byte"):
            DIF._decoded_resource_bytes(71, 10, {**DIF.V2_LIMITS, "max_complete_decoded_bytes": 23_855_103})

    def test_source_and_generated_decode_limits(self) -> None:
        image = DIF.RgbaImage(2560, 1, bytes(2560 * 4))
        payload = DIF.encode_png(image)
        with self.assertRaisesRegex(DIF.AssetError, "dimensions exceed"):
            DIF.decode_png_bytes(payload, "source")
        with self.assertRaisesRegex(DIF.AssetError, "decoded-byte"):
            DIF.decode_png_bytes(payload, "generated", max_dimension=6656, max_decoded_bytes=4096)
        with self.assertRaisesRegex(DIF.AssetError, "byte limit"):
            DIF.decode_png_bytes(payload, "source", max_file_bytes=len(payload) - 1)


if __name__ == "__main__":
    unittest.main(argv=UNITTEST_ARGV)
