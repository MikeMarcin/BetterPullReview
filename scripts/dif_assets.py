#!/usr/bin/env python3
"""Validate and deterministically compile BetterPullReview Dif sprite assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
SCHEMA_VERSION = 1
CLIP_IDS = ("calm_idle", "greet", "rest")
CLIP_KINDS = {"calm_idle": "loop", "greet": "one_shot", "rest": "static"}
CLIP_SOURCE_DIRS = {
    "calm_idle": "source/calm-idle",
    "greet": "source/greet",
    "rest": "source/rest",
}
CELL_SIZE = 256
GUTTER_CELLS = 1
MAX_SOURCE_DIMENSION = 2048
MAX_SOURCE_FILE_BYTES = 16 * 1024 * 1024
MAX_CONTENT_FRAMES = 48
MAX_FRAMES_PER_CLIP = 24
MAX_FRAMES_PER_SECOND = 12
MAX_CONTENT_DECODED_BYTES = 12 * 1024 * 1024
MAX_COMPLETE_DECODED_BYTES = 27 * 1024 * 1024 // 2
MAX_CLIP_DESCRIPTION_BYTES = 240
MAX_GENERATED_FILE_BYTES = 24 * 1024 * 1024
EXPECTED_LIMITS = {
    "max_source_dimension": MAX_SOURCE_DIMENSION,
    "max_source_file_bytes": MAX_SOURCE_FILE_BYTES,
    "max_content_frames": MAX_CONTENT_FRAMES,
    "max_frames_per_clip": MAX_FRAMES_PER_CLIP,
    "max_frames_per_second": MAX_FRAMES_PER_SECOND,
    "max_content_decoded_bytes": MAX_CONTENT_DECODED_BYTES,
    "max_complete_decoded_bytes": MAX_COMPLETE_DECODED_BYTES,
    "max_generated_file_bytes": MAX_GENERATED_FILE_BYTES,
}
EXPECTED_REFERENCES = {
    "dif_ref_sheet": (
        "cf35d96198e20575069b3dd43edd352a04393e457970219738a9e888458729c0",
        1_777_731,
    ),
    "mascot_group": (
        "5859655aa4c39e50a7e3c522e48f8b34c48bd5749553089be95860e7f2d0f39e",
        2_002_023,
    ),
    "nib_process_guidance": (
        "e924c69865ff2998fed3bc5981a405c46960a9ef561947ce64b99fa52e54aeef",
        21_969,
    ),
}
ALLOWED_ASSET_FILES = {
    "README.md",
    "provenance.json",
    "dif-sprite-manifest.json",
}
GENERATED_CATALOG = "dif-sprite-catalog.hpp"
GENERATED_DIGESTS = "dif-sprite-digests.json"


class AssetError(ValueError):
    """A stable, user-actionable asset-contract failure."""


@dataclass(frozen=True)
class RgbaImage:
    width: int
    height: int
    pixels: bytes

    def __post_init__(self) -> None:
        if self.width <= 0 or self.height <= 0:
            raise AssetError("image dimensions must be positive")
        expected = self.width * self.height * 4
        if len(self.pixels) != expected:
            raise AssetError(
                f"RGBA payload has {len(self.pixels)} bytes, expected {expected}"
            )


@dataclass(frozen=True)
class CompiledClip:
    data: dict[str, Any]
    frames: tuple[RgbaImage, ...]
    strip: RgbaImage


def _reject_duplicate_json_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AssetError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def canonical_json_bytes(value: Any) -> bytes:
    return (
        json.dumps(
            value,
            ensure_ascii=True,
            indent=2,
            sort_keys=True,
            separators=(",", ": "),
        )
        + "\n"
    ).encode("utf-8")


def load_canonical_json(path: Path) -> dict[str, Any]:
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise AssetError(f"cannot read {path.name}: {error.strerror}") from error
    try:
        value = json.loads(
            raw.decode("utf-8"), object_pairs_hook=_reject_duplicate_json_keys
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AssetError(f"invalid JSON in {path.name}: {error}") from error
    if not isinstance(value, dict):
        raise AssetError(f"{path.name} must contain one JSON object")
    if raw != canonical_json_bytes(value):
        raise AssetError(f"{path.name} is not canonical UTF-8 JSON")
    return value


def _expect_keys(
    value: dict[str, Any], expected: set[str], context: str
) -> None:
    actual = set(value)
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing or extra:
        raise AssetError(
            f"{context} keys differ: missing={missing}, unexpected={extra}"
        )


def _expect_int(value: Any, context: str, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise AssetError(f"{context} must be an integer")
    if not minimum <= value <= maximum:
        raise AssetError(f"{context} must be in [{minimum}, {maximum}]")
    return value


def _expect_string(value: Any, context: str, max_bytes: int = 256) -> str:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise AssetError(f"{context} must be a non-empty string")
    if "\r" in value or "\n" in value:
        raise AssetError(f"{context} must be one line")
    if len(value.encode("utf-8")) > max_bytes:
        raise AssetError(f"{context} exceeds {max_bytes} UTF-8 bytes")
    return value


def _expect_catalog_string(value: Any, context: str, max_bytes: int) -> str:
    text = _expect_string(value, context, max_bytes)
    if any(ord(character) < 0x20 or ord(character) > 0x7E for character in text):
        raise AssetError(f"{context} must use printable ASCII for C++ portability")
    return text


def safe_relative_path(value: Any, context: str) -> PurePosixPath:
    text = _expect_string(value, context, 240)
    if "\\" in text:
        raise AssetError(f"{context} must use POSIX separators")
    path = PurePosixPath(text)
    if path.is_absolute() or not path.parts:
        raise AssetError(f"{context} must be repository-relative")
    if any(part in {"", ".", ".."} for part in path.parts):
        raise AssetError(f"{context} contains an unsafe path component")
    return path


def _asset_path(root: Path, relative: PurePosixPath) -> Path:
    resolved_root = root.resolve()
    candidate = resolved_root
    for part in relative.parts:
        candidate = candidate / part
        if candidate.is_symlink():
            raise AssetError(f"asset path contains a symbolic link: {relative}")
    resolved = candidate.resolve()
    try:
        resolved.relative_to(resolved_root)
    except ValueError as error:
        raise AssetError(f"path escapes asset root: {relative}") from error
    return resolved


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def _filtered_rgba_rows(image: RgbaImage) -> bytes:
    rows = bytearray()
    stride = image.width * 4
    for y in range(image.height):
        rows.append(0)
        rows.extend(image.pixels[y * stride : (y + 1) * stride])
    return bytes(rows)


def encode_png(image: RgbaImage) -> bytes:
    header = struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + _png_chunk(b"IHDR", header)
        + _png_chunk(b"IDAT", zlib.compress(_filtered_rgba_rows(image), level=9))
        + _png_chunk(b"IEND", b"")
    )


def encode_apng(
    frames: Iterable[RgbaImage], frame_duration_ms: int, play_count: int
) -> bytes:
    frame_list = list(frames)
    if not frame_list:
        raise AssetError("animated preview requires at least one frame")
    first = frame_list[0]
    if any(
        (frame.width, frame.height) != (first.width, first.height)
        for frame in frame_list
    ):
        raise AssetError("animated preview frames must share dimensions")
    if not 1 <= frame_duration_ms <= 65_535:
        raise AssetError("animated preview duration is out of range")
    if not 0 <= play_count <= 65_535:
        raise AssetError("animated preview play count is out of range")

    header = struct.pack(">IIBBBBB", first.width, first.height, 8, 6, 0, 0, 0)
    output = bytearray(PNG_SIGNATURE)
    output.extend(_png_chunk(b"IHDR", header))
    output.extend(_png_chunk(b"acTL", struct.pack(">II", len(frame_list), play_count)))
    sequence = 0
    for index, frame in enumerate(frame_list):
        frame_control = struct.pack(
            ">IIIIIHHBB",
            sequence,
            frame.width,
            frame.height,
            0,
            0,
            frame_duration_ms,
            1000,
            0,
            0,
        )
        output.extend(_png_chunk(b"fcTL", frame_control))
        sequence += 1
        compressed = zlib.compress(_filtered_rgba_rows(frame), level=9)
        if index == 0:
            output.extend(_png_chunk(b"IDAT", compressed))
        else:
            output.extend(_png_chunk(b"fdAT", struct.pack(">I", sequence) + compressed))
            sequence += 1
    output.extend(_png_chunk(b"IEND", b""))
    return bytes(output)


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png_bytes(data: bytes, context: str) -> RgbaImage:
    if len(data) > MAX_SOURCE_FILE_BYTES:
        raise AssetError(f"{context} exceeds the source PNG byte limit")
    if not data.startswith(PNG_SIGNATURE):
        raise AssetError(f"{context} is not a PNG")

    offset = len(PNG_SIGNATURE)
    chunks: list[tuple[bytes, bytes]] = []
    saw_iend = False
    while offset < len(data):
        if offset + 12 > len(data):
            raise AssetError(f"{context} has a truncated PNG chunk")
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        end = offset + 12 + size
        if end > len(data):
            raise AssetError(f"{context} has a truncated {kind!r} chunk")
        payload = data[offset + 8 : offset + 8 + size]
        expected_crc = struct.unpack_from(">I", data, offset + 8 + size)[0]
        actual_crc = zlib.crc32(kind + payload) & 0xFFFFFFFF
        if expected_crc != actual_crc:
            raise AssetError(f"{context} has a bad {kind!r} CRC")
        if kind not in {b"IHDR", b"IDAT", b"IEND"}:
            raise AssetError(f"{context} contains unexpected PNG chunk {kind!r}")
        if saw_iend:
            raise AssetError(f"{context} contains data after IEND")
        chunks.append((kind, payload))
        saw_iend = kind == b"IEND"
        offset = end

    if offset != len(data) or not saw_iend:
        raise AssetError(f"{context} has no terminal IEND")
    kinds = [kind for kind, _ in chunks]
    if not kinds or kinds[0] != b"IHDR" or kinds[-1] != b"IEND":
        raise AssetError(f"{context} has invalid PNG chunk order")
    if kinds.count(b"IHDR") != 1 or kinds.count(b"IEND") != 1:
        raise AssetError(f"{context} must contain one IHDR and one IEND")
    idat_indices = [index for index, kind in enumerate(kinds) if kind == b"IDAT"]
    if not idat_indices or idat_indices != list(
        range(idat_indices[0], idat_indices[-1] + 1)
    ):
        raise AssetError(f"{context} IDAT chunks must be consecutive")

    header = chunks[0][1]
    if len(header) != 13:
        raise AssetError(f"{context} has an invalid IHDR length")
    width, height, depth, color, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", header
    )
    if not 1 <= width <= MAX_SOURCE_DIMENSION or not 1 <= height <= MAX_SOURCE_DIMENSION:
        raise AssetError(f"{context} dimensions exceed {MAX_SOURCE_DIMENSION}")
    if (depth, color, compression, filtering, interlace) != (8, 6, 0, 0, 0):
        raise AssetError(
            f"{context} must be non-interlaced 8-bit RGBA with standard compression"
        )
    expected_size = height * (1 + width * 4)
    compressed = b"".join(
        payload for kind, payload in chunks if kind == b"IDAT"
    )
    decompressor = zlib.decompressobj()
    try:
        raw = decompressor.decompress(compressed, expected_size + 1)
        if len(raw) > expected_size:
            raise AssetError(f"{context} decompressed size exceeds its dimensions")
        raw += decompressor.flush(expected_size - len(raw) + 1)
    except zlib.error as error:
        raise AssetError(f"{context} has invalid compressed image data") from error
    if (
        len(raw) != expected_size
        or not decompressor.eof
        or decompressor.unused_data
        or decompressor.unconsumed_tail
    ):
        raise AssetError(f"{context} decompressed size or stream boundary is invalid")

    stride = width * 4
    previous = bytearray(stride)
    pixels = bytearray()
    cursor = 0
    for row_index in range(height):
        filter_type = raw[cursor]
        cursor += 1
        encoded = raw[cursor : cursor + stride]
        cursor += stride
        reconstructed = bytearray(stride)
        for index, encoded_byte in enumerate(encoded):
            left = reconstructed[index - 4] if index >= 4 else 0
            up = previous[index]
            up_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                predictor = _paeth(left, up, up_left)
            else:
                raise AssetError(
                    f"{context} row {row_index} uses invalid PNG filter {filter_type}"
                )
            reconstructed[index] = (encoded_byte + predictor) & 0xFF
        pixels.extend(reconstructed)
        previous = reconstructed
    return RgbaImage(width, height, bytes(pixels))


def decode_png(path: Path, context: str) -> RgbaImage:
    try:
        data = path.read_bytes()
        image = decode_png_bytes(data, context)
        if data != encode_png(image):
            raise AssetError(f"{context} is not a canonical product PNG")
        return image
    except OSError as error:
        raise AssetError(f"cannot read {context}: {error.strerror}") from error


def resize_box(image: RgbaImage, size: int = CELL_SIZE) -> RgbaImage:
    if image.width != image.height:
        raise AssetError("source frame must be square")
    if image.width < size or image.width % size != 0:
        raise AssetError(
            f"source frame size {image.width} must be a multiple of {size}"
        )
    scale = image.width // size
    if scale == 1:
        return image
    output = bytearray(size * size * 4)
    samples = scale * scale
    for destination_y in range(size):
        for destination_x in range(size):
            alpha_sum = 0
            premultiplied = [0, 0, 0]
            for source_y in range(destination_y * scale, (destination_y + 1) * scale):
                for source_x in range(
                    destination_x * scale, (destination_x + 1) * scale
                ):
                    index = (source_y * image.width + source_x) * 4
                    alpha = image.pixels[index + 3]
                    alpha_sum += alpha
                    for channel in range(3):
                        premultiplied[channel] += image.pixels[index + channel] * alpha
            destination = (destination_y * size + destination_x) * 4
            alpha = (alpha_sum + samples // 2) // samples
            output[destination + 3] = alpha
            if alpha_sum:
                for channel in range(3):
                    output[destination + channel] = min(
                        255,
                        (premultiplied[channel] + alpha_sum // 2) // alpha_sum,
                    )
    return RgbaImage(size, size, bytes(output))


def _alpha_bounds(image: RgbaImage) -> tuple[int, int, int, int]:
    xs: list[int] = []
    ys: list[int] = []
    for y in range(image.height):
        for x in range(image.width):
            if image.pixels[(y * image.width + x) * 4 + 3]:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise AssetError("frame contains no visible pixels")
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def _mean_byte_delta(first: RgbaImage, second: RgbaImage) -> int:
    if (first.width, first.height) != (second.width, second.height):
        raise AssetError("cannot compare different frame sizes")
    total = sum(abs(a - b) for a, b in zip(first.pixels, second.pixels))
    return (total + len(first.pixels) // 2) // len(first.pixels)


def _validate_source_frame(image: RgbaImage, context: str) -> None:
    if image.width != image.height or image.width < CELL_SIZE:
        raise AssetError(f"{context} must be square and at least {CELL_SIZE} pixels")
    if image.width % CELL_SIZE != 0:
        raise AssetError(f"{context} dimension must be divisible by {CELL_SIZE}")
    alphas = image.pixels[3::4]
    if not any(alphas):
        raise AssetError(f"{context} has no visible content")
    if all(alpha == 255 for alpha in alphas):
        raise AssetError(f"{context} has no transparency")
    corner_indices = (
        3,
        (image.width - 1) * 4 + 3,
        ((image.height - 1) * image.width) * 4 + 3,
        (image.width * image.height - 1) * 4 + 3,
    )
    if any(image.pixels[index] for index in corner_indices):
        raise AssetError(f"{context} must have transparent corners")
    for index, alpha in enumerate(alphas):
        pixel = index * 4
        if alpha == 0 and any(image.pixels[pixel : pixel + 3]):
            raise AssetError(f"{context} has hidden RGB in a transparent pixel")


def _validate_provenance(root: Path) -> dict[str, Any]:
    provenance = load_canonical_json(root / "provenance.json")
    _expect_keys(
        provenance,
        {
            "schema_version",
            "asset_family",
            "authority",
            "references",
            "creation",
            "rights",
            "source_files",
        },
        "provenance",
    )
    if provenance["schema_version"] != SCHEMA_VERSION:
        raise AssetError("provenance schema_version is unsupported")
    if provenance["asset_family"] != "dif":
        raise AssetError("provenance asset_family must be dif")
    authority = provenance["authority"]
    if not isinstance(authority, dict):
        raise AssetError("provenance.authority must be an object")
    _expect_keys(
        authority,
        {"decision_subject_sha256", "decision_path", "status"},
        "provenance.authority",
    )
    if authority["status"] != "approved":
        raise AssetError("provenance authority is not approved")
    _validate_sha256(authority["decision_subject_sha256"], "authority decision")
    safe_relative_path(authority["decision_path"], "authority decision_path")

    references = provenance["references"]
    if not isinstance(references, list) or len(references) != 3:
        raise AssetError("provenance.references must contain exactly three entries")
    reference_ids: set[str] = set()
    for index, reference in enumerate(references):
        if not isinstance(reference, dict):
            raise AssetError(f"reference {index} must be an object")
        _expect_keys(
            reference, {"id", "sha256", "byte_length", "role"}, f"reference {index}"
        )
        reference_id = _expect_string(reference["id"], f"reference {index} id", 64)
        if reference_id in reference_ids:
            raise AssetError(f"duplicate provenance reference id: {reference_id}")
        reference_ids.add(reference_id)
        digest = _validate_sha256(reference["sha256"], f"reference {reference_id}")
        byte_length = _expect_int(
            reference["byte_length"],
            f"reference {reference_id} bytes",
            1,
            64 * 1024 * 1024,
        )
        _expect_string(reference["role"], f"reference {reference_id} role", 160)
    if reference_ids != {"dif_ref_sheet", "mascot_group", "nib_process_guidance"}:
        raise AssetError("provenance reference identities differ from the RFC inputs")
    for reference in references:
        expected_digest, expected_bytes = EXPECTED_REFERENCES[reference["id"]]
        if (reference["sha256"], reference["byte_length"]) != (
            expected_digest,
            expected_bytes,
        ):
            raise AssetError(
                f"provenance reference bytes differ from {reference['id']}"
            )

    creation = provenance["creation"]
    if not isinstance(creation, dict):
        raise AssetError("provenance.creation must be an object")
    _expect_keys(
        creation,
        {"creator", "tools", "method_record", "source_authority"},
        "provenance.creation",
    )
    _expect_string(creation["creator"], "creation creator", 160)
    tools = creation["tools"]
    if not isinstance(tools, list) or not tools or len(tools) > 16:
        raise AssetError("creation tools must be a non-empty bounded list")
    for index, tool in enumerate(tools):
        _expect_string(tool, f"creation tool {index}", 200)
    safe_relative_path(creation["method_record"], "creation method_record")
    if creation["source_authority"] != "checked_in_source_bytes":
        raise AssetError("source authority must be checked_in_source_bytes")

    rights = provenance["rights"]
    if not isinstance(rights, dict):
        raise AssetError("provenance.rights must be an object")
    _expect_keys(
        rights,
        {
            "basis",
            "derivative_scope",
            "distribution_scope",
            "notice_required",
            "notice_disposition",
            "foreign_identity_exclusion",
        },
        "provenance.rights",
    )
    for field in ("basis", "derivative_scope", "distribution_scope", "notice_disposition"):
        _expect_string(rights[field], f"rights {field}", 400)
    if not isinstance(rights["notice_required"], bool):
        raise AssetError("rights notice_required must be boolean")
    if rights["foreign_identity_exclusion"] != ["AllTheThings", "Mel", "Nib"]:
        raise AssetError("foreign identity exclusion must name AllTheThings, Mel, and Nib")

    source_files = provenance["source_files"]
    if not isinstance(source_files, list) or not source_files:
        raise AssetError("provenance.source_files must be a non-empty list")
    seen_paths: set[PurePosixPath] = set()
    for index, source in enumerate(source_files):
        if not isinstance(source, dict):
            raise AssetError(f"source file {index} must be an object")
        _expect_keys(source, {"path", "sha256"}, f"source file {index}")
        path = safe_relative_path(source["path"], f"source file {index} path")
        if path in seen_paths:
            raise AssetError(f"duplicate provenance source path: {path}")
        seen_paths.add(path)
        expected = _validate_sha256(source["sha256"], f"source file {path}")
        actual_path = _asset_path(root, path)
        if not actual_path.is_file() or sha256_file(actual_path) != expected:
            raise AssetError(f"provenance source digest mismatch: {path}")
    return provenance


def _validate_sha256(value: Any, context: str) -> str:
    if not isinstance(value, str) or len(value) != 64:
        raise AssetError(f"{context} SHA-256 must contain 64 lowercase hex digits")
    if any(character not in "0123456789abcdef" for character in value):
        raise AssetError(f"{context} SHA-256 must contain lowercase hex digits")
    return value


def _validate_geometry(value: Any, context: str) -> tuple[int, int, int, int]:
    if not isinstance(value, list) or len(value) != 4:
        raise AssetError(f"{context} must contain [left, top, right, bottom]")
    left, top, right, bottom = [
        _expect_int(component, f"{context}[{index}]", 0, CELL_SIZE)
        for index, component in enumerate(value)
    ]
    if left >= right or top >= bottom:
        raise AssetError(f"{context} must have positive area")
    return left, top, right, bottom


def _validate_manifest(root: Path) -> dict[str, Any]:
    manifest = load_canonical_json(root / "dif-sprite-manifest.json")
    _expect_keys(
        manifest,
        {
            "schema_version",
            "asset_family",
            "cell",
            "limits",
            "registration",
            "clips",
        },
        "manifest",
    )
    if manifest["schema_version"] != SCHEMA_VERSION:
        raise AssetError("manifest schema_version is unsupported")
    if manifest["asset_family"] != "dif":
        raise AssetError("manifest asset_family must be dif")

    cell = manifest["cell"]
    if not isinstance(cell, dict):
        raise AssetError("manifest.cell must be an object")
    _expect_keys(
        cell,
        {"width", "height", "pixel_format", "alpha", "color_space", "gutter_cells"},
        "manifest.cell",
    )
    expected_cell = {
        "width": CELL_SIZE,
        "height": CELL_SIZE,
        "pixel_format": "rgba8",
        "alpha": "straight",
        "color_space": "srgb",
        "gutter_cells": GUTTER_CELLS,
    }
    if cell != expected_cell:
        raise AssetError("manifest cell contract differs from the fixed RFC contract")
    if manifest["limits"] != EXPECTED_LIMITS:
        raise AssetError("manifest limits differ from the fixed RFC limits")

    registration = manifest["registration"]
    if not isinstance(registration, dict):
        raise AssetError("manifest.registration must be an object")
    _expect_keys(
        registration,
        {
            "visual_center_x",
            "baseline_y",
            "neutral_body_bounds",
            "foot_tolerance_px",
            "edge_clearance_px",
        },
        "manifest.registration",
    )
    _expect_int(registration["visual_center_x"], "visual_center_x", 1, CELL_SIZE - 1)
    _expect_int(registration["baseline_y"], "baseline_y", 1, CELL_SIZE - 2)
    _validate_geometry(registration["neutral_body_bounds"], "neutral_body_bounds")
    _expect_int(registration["foot_tolerance_px"], "foot_tolerance_px", 0, 2)
    _expect_int(registration["edge_clearance_px"], "edge_clearance_px", 1, 16)

    clips = manifest["clips"]
    if not isinstance(clips, list) or len(clips) != len(CLIP_IDS):
        raise AssetError("manifest must define exactly three clips")
    if [clip.get("id") if isinstance(clip, dict) else None for clip in clips] != list(CLIP_IDS):
        raise AssetError("manifest clips must be ordered calm_idle, greet, rest")
    total_frames = 0
    source_paths: set[PurePosixPath] = set()
    strip_paths: set[PurePosixPath] = set()
    for clip in clips:
        _expect_keys(
            clip,
            {
                "id",
                "kind",
                "frames",
                "strip",
                "frame_duration_ms",
                "representative_frame",
                "priority",
                "return_target",
                "body_bounds",
                "allowed_travel_px",
                "accessible_description",
                "loop_seam_mean_delta_max",
                "handoff_mean_delta_max",
            },
            f"clip {clip['id']}",
        )
        clip_id = clip["id"]
        if clip["kind"] != CLIP_KINDS[clip_id]:
            raise AssetError(f"clip {clip_id} has the wrong kind")
        frames = clip["frames"]
        if not isinstance(frames, list) or not frames:
            raise AssetError(f"clip {clip_id} must contain frames")
        if len(frames) > MAX_FRAMES_PER_CLIP:
            raise AssetError(f"clip {clip_id} exceeds the frame limit")
        if clip_id == "rest" and len(frames) != 1:
            raise AssetError("rest must contain exactly one frame")
        total_frames += len(frames)
        expected_parent = PurePosixPath(CLIP_SOURCE_DIRS[clip_id])
        for index, frame_value in enumerate(frames):
            frame = safe_relative_path(frame_value, f"clip {clip_id} frame {index}")
            if frame.parent != expected_parent or frame.name != f"frame-{index:03d}.png":
                raise AssetError(f"clip {clip_id} frame sequence is not canonical")
            if frame in source_paths:
                raise AssetError(f"source frame is reused: {frame}")
            source_paths.add(frame)
        strip = safe_relative_path(clip["strip"], f"clip {clip_id} strip")
        expected_strip_name = f"dif-{clip_id.replace('_', '-')}.png"
        if (
            strip.parent != PurePosixPath("generated")
            or strip.name != expected_strip_name
        ):
            raise AssetError(f"clip {clip_id} strip path is not canonical")
        if strip in strip_paths:
            raise AssetError(f"duplicate strip path: {strip}")
        strip_paths.add(strip)
        duration = _expect_int(clip["frame_duration_ms"], f"clip {clip_id} duration", 0, 10_000)
        if clip_id == "rest":
            if duration != 0:
                raise AssetError("rest frame duration must be zero")
        elif duration == 0 or 1000 > duration * MAX_FRAMES_PER_SECOND:
            raise AssetError(f"clip {clip_id} exceeds {MAX_FRAMES_PER_SECOND} fps")
        _expect_int(
            clip["representative_frame"],
            f"clip {clip_id} representative_frame",
            0,
            len(frames) - 1,
        )
        _expect_int(clip["priority"], f"clip {clip_id} priority", 0, 100)
        expected_return = "calm_idle" if clip_id in {"calm_idle", "greet"} else "rest"
        if clip["return_target"] != expected_return:
            raise AssetError(f"clip {clip_id} has invalid return_target")
        body_bounds = _validate_geometry(
            clip["body_bounds"], f"clip {clip_id} body_bounds"
        )
        if body_bounds != tuple(registration["neutral_body_bounds"]):
            raise AssetError(
                f"clip {clip_id} body_bounds must equal neutral_body_bounds"
            )
        travel = clip["allowed_travel_px"]
        if not isinstance(travel, list) or len(travel) != 2:
            raise AssetError(f"clip {clip_id} allowed_travel_px must contain x,y")
        _expect_int(travel[0], f"clip {clip_id} travel x", 0, 4)
        _expect_int(travel[1], f"clip {clip_id} travel y", 0, 2)
        _expect_catalog_string(
            clip["accessible_description"],
            f"clip {clip_id} accessible_description",
            MAX_CLIP_DESCRIPTION_BYTES,
        )
        seam = _expect_int(
            clip["loop_seam_mean_delta_max"],
            f"clip {clip_id} loop seam limit",
            0,
            255,
        )
        handoff = _expect_int(
            clip["handoff_mean_delta_max"],
            f"clip {clip_id} handoff limit",
            0,
            255,
        )
        if clip_id == "calm_idle" and seam == 0:
            raise AssetError("calm_idle must declare a nonzero seam tolerance")
        if clip_id != "calm_idle" and seam != 0:
            raise AssetError(f"clip {clip_id} cannot declare a loop seam tolerance")
        if clip_id == "greet" and handoff == 0:
            raise AssetError("greet must declare a nonzero handoff tolerance")
        if clip_id != "greet" and handoff != 0:
            raise AssetError(f"clip {clip_id} cannot declare a handoff tolerance")
    if total_frames > MAX_CONTENT_FRAMES:
        raise AssetError("manifest exceeds the total content-frame limit")
    content_bytes = total_frames * CELL_SIZE * CELL_SIZE * 4
    complete_bytes = (total_frames + len(clips) * 2) * CELL_SIZE * CELL_SIZE * 4
    if content_bytes > MAX_CONTENT_DECODED_BYTES:
        raise AssetError("manifest exceeds the content decoded-byte limit")
    if complete_bytes > MAX_COMPLETE_DECODED_BYTES:
        raise AssetError("manifest exceeds the complete decoded-byte limit")
    return manifest


def _pack_strip(frames: Iterable[RgbaImage]) -> RgbaImage:
    frame_list = list(frames)
    width = (len(frame_list) + 2 * GUTTER_CELLS) * CELL_SIZE
    output = bytearray(width * CELL_SIZE * 4)
    for frame_index, frame in enumerate(frame_list, start=GUTTER_CELLS):
        if (frame.width, frame.height) != (CELL_SIZE, CELL_SIZE):
            raise AssetError("runtime frame has invalid dimensions")
        for y in range(CELL_SIZE):
            source_start = y * CELL_SIZE * 4
            destination_start = (y * width + frame_index * CELL_SIZE) * 4
            output[destination_start : destination_start + CELL_SIZE * 4] = (
                frame.pixels[source_start : source_start + CELL_SIZE * 4]
            )
    return RgbaImage(width, CELL_SIZE, bytes(output))


def _validate_runtime_geometry(
    manifest: dict[str, Any], clips: list[CompiledClip]
) -> None:
    registration = manifest["registration"]
    baseline = registration["baseline_y"]
    center = registration["visual_center_x"]
    foot_tolerance = registration["foot_tolerance_px"]
    edge = registration["edge_clearance_px"]
    calm = clips[0]
    calm_representative = calm.frames[calm.data["representative_frame"]]
    for clip in clips:
        declared = tuple(clip.data["body_bounds"])
        travel_x, travel_y = clip.data["allowed_travel_px"]
        representative = clip.frames[clip.data["representative_frame"]]
        reference_bounds = _alpha_bounds(representative)
        for index, frame in enumerate(clip.frames):
            bounds = _alpha_bounds(frame)
            left, top, right, bottom = bounds
            if left < edge or top < edge or right > CELL_SIZE - edge or bottom > CELL_SIZE - edge:
                raise AssetError(f"clip {clip.data['id']} frame {index} touches edge clearance")
            exceeds_body_bounds = (
                left < declared[0]
                or top < declared[1]
                or right > declared[2]
                or bottom > declared[3]
            )
            if exceeds_body_bounds:
                raise AssetError(f"clip {clip.data['id']} frame {index} exceeds body_bounds")
            frame_center_twice = left + right - 1
            if abs(frame_center_twice - center * 2) > 2 * (travel_x + 2):
                raise AssetError(f"clip {clip.data['id']} frame {index} drifts from visual center")
            if abs(bottom - baseline) > foot_tolerance:
                raise AssetError(f"clip {clip.data['id']} frame {index} drifts from baseline")
            reference_center_twice = reference_bounds[0] + reference_bounds[2] - 1
            if abs(frame_center_twice - reference_center_twice) > 2 * travel_x:
                raise AssetError(f"clip {clip.data['id']} frame {index} exceeds x travel")
            if abs(bottom - reference_bounds[3]) > travel_y:
                raise AssetError(f"clip {clip.data['id']} frame {index} exceeds y travel")

    if len(calm.frames) < 2:
        raise AssetError("calm_idle needs at least two frames for a loop")
    if calm.frames[0].pixels == calm.frames[-1].pixels:
        raise AssetError("calm_idle duplicates its terminal frame")
    seam_delta = _mean_byte_delta(calm.frames[0], calm.frames[-1])
    if seam_delta > calm.data["loop_seam_mean_delta_max"]:
        raise AssetError(
            f"calm_idle seam delta {seam_delta} exceeds declared tolerance"
        )
    greet = clips[1]
    handoff_delta = _mean_byte_delta(greet.frames[-1], calm_representative)
    if handoff_delta > greet.data["handoff_mean_delta_max"]:
        raise AssetError(
            f"greet handoff delta {handoff_delta} exceeds declared tolerance"
        )


def compile_assets(root: Path) -> tuple[dict[str, Any], dict[str, Any], list[CompiledClip]]:
    if root.is_symlink():
        raise AssetError("asset root cannot be a symbolic link")
    if not root.is_dir():
        raise AssetError("asset root does not exist")
    manifest = _validate_manifest(root)
    provenance = _validate_provenance(root)
    expected_source_paths = {
        PurePosixPath(frame)
        for clip in manifest["clips"]
        for frame in clip["frames"]
    }
    provenance_source_paths = {
        PurePosixPath(item["path"]) for item in provenance["source_files"]
    }
    master = PurePosixPath("source/dif-neutral-master.png")
    if provenance_source_paths != expected_source_paths | {master}:
        raise AssetError("provenance source inventory differs from manifest plus master")
    expected_non_generated = set(ALLOWED_ASSET_FILES) | {
        path.as_posix() for path in provenance_source_paths
    }
    actual_non_generated: set[str] = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            raise AssetError(
                f"asset tree contains a symbolic link: {path.relative_to(root).as_posix()}"
            )
        if path.is_file() and path.relative_to(root).parts[0] != "generated":
            actual_non_generated.add(path.relative_to(root).as_posix())
    if actual_non_generated != expected_non_generated:
        raise AssetError(
            "source tree differs from the manifest: "
            f"missing={sorted(expected_non_generated - actual_non_generated)}, "
            f"unexpected={sorted(actual_non_generated - expected_non_generated)}"
        )
    master_image = decode_png(_asset_path(root, master), str(master))
    _validate_source_frame(master_image, str(master))
    if master_image.width < 512:
        raise AssetError("neutral master must be at least 512 x 512")

    compiled: list[CompiledClip] = []
    for clip in manifest["clips"]:
        runtime_frames: list[RgbaImage] = []
        for frame_value in clip["frames"]:
            frame_path = PurePosixPath(frame_value)
            source = decode_png(_asset_path(root, frame_path), str(frame_path))
            _validate_source_frame(source, str(frame_path))
            runtime_frames.append(resize_box(source))
        strip = _pack_strip(runtime_frames)
        compiled.append(CompiledClip(clip, tuple(runtime_frames), strip))
    _validate_runtime_geometry(manifest, compiled)
    return manifest, provenance, compiled


def _catalog_bytes(manifest: dict[str, Any]) -> bytes:
    manifest_digest = sha256_bytes(canonical_json_bytes(manifest))
    lines = [
        "// Generated by scripts/dif_assets.py. Do not edit.",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <string_view>",
        "",
        "namespace bpr::adapters::qt::dif {",
        "enum class clip_kind { loop, one_shot, static_frame };",
        "",
        "struct clip_descriptor final {",
        "  std::string_view id;",
        "  std::string_view resource;",
        "  std::size_t frame_count;",
        "  int frame_duration_ms;",
        "  clip_kind kind;",
        "  std::size_t representative_frame;",
        "  int priority;",
        "  std::string_view return_target;",
        "  std::string_view accessible_description;",
        "};",
        "",
        f'inline constexpr std::string_view kManifestSha256{{"{manifest_digest}"}};',
        f"inline constexpr int kSchemaVersion{{{SCHEMA_VERSION}}};",
        f"inline constexpr int kCellSize{{{CELL_SIZE}}};",
        f"inline constexpr std::array<clip_descriptor, {len(CLIP_IDS)}> kClips{{{{",
    ]
    for clip in manifest["clips"]:
        resource = ":/bpr/dif/" + PurePosixPath(clip["strip"]).name
        description = json.dumps(clip["accessible_description"], ensure_ascii=True)
        catalog_kind = {
            "loop": "clip_kind::loop",
            "one_shot": "clip_kind::one_shot",
            "static": "clip_kind::static_frame",
        }[clip["kind"]]
        lines.append(
            "  {"
            + json.dumps(clip["id"])
            + ", "
            + json.dumps(resource)
            + f", {len(clip['frames'])}, {clip['frame_duration_ms']}, "
            + catalog_kind
            + f", {clip['representative_frame']}, {clip['priority']}, "
            + json.dumps(clip["return_target"])
            + ", "
            + description
            + "},"
        )
    lines.extend(["}};", "}  // namespace bpr::adapters::qt::dif", ""])
    return "\n".join(lines).encode("utf-8")


def generated_products(
    root: Path,
) -> dict[PurePosixPath, bytes]:
    manifest, provenance, clips = compile_assets(root)
    products: dict[PurePosixPath, bytes] = {}
    for clip in clips:
        relative = PurePosixPath(clip.data["strip"]).relative_to("generated")
        products[relative] = encode_png(clip.strip)
    products[PurePosixPath(GENERATED_CATALOG)] = _catalog_bytes(manifest)
    source_digests = {
        item["path"]: item["sha256"]
        for item in provenance["source_files"]
    }
    output_digests = {
        str(relative): sha256_bytes(payload)
        for relative, payload in sorted(products.items(), key=lambda item: str(item[0]))
    }
    inventory = {
        "schema_version": SCHEMA_VERSION,
        "manifest_sha256": sha256_file(root / "dif-sprite-manifest.json"),
        "source_sha256": dict(sorted(source_digests.items())),
        "output_sha256": output_digests,
    }
    products[PurePosixPath(GENERATED_DIGESTS)] = canonical_json_bytes(inventory)
    for relative, payload in products.items():
        if len(payload) > MAX_GENERATED_FILE_BYTES:
            raise AssetError(f"generated file exceeds byte limit: {relative}")
    return products


def _prepare_empty_directory(output: Path, context: str) -> None:
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise AssetError(f"{context} directory must be empty")
    output.mkdir(parents=True, exist_ok=True)


def _write_products(products: dict[PurePosixPath, bytes], output: Path) -> None:
    _prepare_empty_directory(output, "output")
    for relative, payload in sorted(products.items(), key=lambda item: str(item[0])):
        destination = _asset_path(output, relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)


def _tree_bytes(root: Path) -> dict[PurePosixPath, bytes]:
    if not root.is_dir():
        raise AssetError(f"generated directory is absent: {root.name}")
    result: dict[PurePosixPath, bytes] = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise AssetError(
                f"generated tree contains a symbolic link: {path.relative_to(root).as_posix()}"
            )
        if path.is_file():
            result[PurePosixPath(path.relative_to(root).as_posix())] = path.read_bytes()
    return result


def _validate_asset_inventory(root: Path, manifest: dict[str, Any]) -> None:
    expected = set(ALLOWED_ASSET_FILES)
    expected.add("source/dif-neutral-master.png")
    for clip in manifest["clips"]:
        expected.update(clip["frames"])
        expected.add(clip["strip"])
    expected.add(f"generated/{GENERATED_CATALOG}")
    expected.add(f"generated/{GENERATED_DIGESTS}")
    actual = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file()
    }
    if actual != expected:
        raise AssetError(
            "asset tree differs from the manifest: "
            f"missing={sorted(expected - actual)}, unexpected={sorted(actual - expected)}"
        )


def validate_assets(root: Path) -> None:
    manifest, _provenance, _clips = compile_assets(root)
    _validate_asset_inventory(root, manifest)
    expected = generated_products(root)
    committed = _tree_bytes(root / "generated")
    if committed != expected:
        differing = sorted(
            str(path)
            for path in set(committed) | set(expected)
            if committed.get(path) != expected.get(path)
        )
        raise AssetError(f"committed generated products are stale: {differing}")


def verify_assets(root: Path) -> None:
    validate_assets(root)
    with tempfile.TemporaryDirectory(prefix="bpr-dif-assets-") as temporary:
        first = Path(temporary) / "first"
        second = Path(temporary) / "second"
        products = generated_products(root)
        _write_products(products, first)
        _write_products(generated_products(root), second)
        first_tree = _tree_bytes(first)
        if first_tree != _tree_bytes(second):
            raise AssetError("two clean regenerations differ")
        if first_tree != _tree_bytes(root / "generated"):
            raise AssetError("clean regeneration differs from committed products")


def _scale_nearest(image: RgbaImage, width: int, height: int) -> RgbaImage:
    output = bytearray(width * height * 4)
    for y in range(height):
        source_y = min(image.height - 1, y * image.height // height)
        for x in range(width):
            source_x = min(image.width - 1, x * image.width // width)
            source = (source_y * image.width + source_x) * 4
            destination = (y * width + x) * 4
            output[destination : destination + 4] = image.pixels[source : source + 4]
    return RgbaImage(width, height, bytes(output))


def _composite(background: tuple[int, int, int], image: RgbaImage) -> RgbaImage:
    output = bytearray(image.width * image.height * 4)
    for index in range(image.width * image.height):
        source = index * 4
        alpha = image.pixels[source + 3]
        for channel in range(3):
            output[source + channel] = (
                image.pixels[source + channel] * alpha
                + background[channel] * (255 - alpha)
                + 127
            ) // 255
        output[source + 3] = 255
    return RgbaImage(image.width, image.height, bytes(output))


def _join_grid(images: list[RgbaImage], columns: int, padding: int = 8) -> RgbaImage:
    if not images:
        raise AssetError("preview grid requires at least one image")
    cell_width = max(image.width for image in images)
    cell_height = max(image.height for image in images)
    rows = math.ceil(len(images) / columns)
    width = columns * cell_width + (columns + 1) * padding
    height = rows * cell_height + (rows + 1) * padding
    output = bytearray([230, 230, 230, 255] * (width * height))
    for index, image in enumerate(images):
        column = index % columns
        row = index // columns
        origin_x = padding + column * (cell_width + padding)
        origin_y = padding + row * (cell_height + padding)
        for y in range(image.height):
            source = y * image.width * 4
            destination = ((origin_y + y) * width + origin_x) * 4
            output[destination : destination + image.width * 4] = image.pixels[
                source : source + image.width * 4
            ]
    return RgbaImage(width, height, bytes(output))


def _with_registration_overlay(
    image: RgbaImage,
    body_bounds: list[int],
    visual_center_x: int,
    baseline_y: int,
) -> RgbaImage:
    output = bytearray(image.pixels)

    def set_pixel(x: int, y: int, color: tuple[int, int, int, int]) -> None:
        if 0 <= x < image.width and 0 <= y < image.height:
            index = (y * image.width + x) * 4
            output[index : index + 4] = bytes(color)

    for y in range(image.height):
        set_pixel(visual_center_x, y, (0, 180, 255, 255))
    for x in range(image.width):
        set_pixel(x, baseline_y, (255, 70, 70, 255))
    left, top, right, bottom = body_bounds
    for x in range(left, right):
        set_pixel(x, top, (255, 190, 0, 255))
        set_pixel(x, bottom - 1, (255, 190, 0, 255))
    for y in range(top, bottom):
        set_pixel(left, y, (255, 190, 0, 255))
        set_pixel(right - 1, y, (255, 190, 0, 255))
    return RgbaImage(image.width, image.height, bytes(output))


def render_previews(root: Path, output: Path) -> None:
    manifest, _provenance, clips = compile_assets(root)
    _prepare_empty_directory(output, "preview output")
    products: dict[str, bytes] = {}
    for clip in clips:
        sheet = _join_grid(list(clip.frames), columns=min(6, len(clip.frames)))
        products[f"contact-sheets/{clip.data['id']}.png"] = encode_png(sheet)
        overlays = [
            _with_registration_overlay(
                frame,
                clip.data["body_bounds"],
                manifest["registration"]["visual_center_x"],
                manifest["registration"]["baseline_y"],
            )
            for frame in clip.frames
        ]
        products[f"overlays/{clip.data['id']}.png"] = encode_png(
            _join_grid(overlays, columns=min(6, len(overlays)))
        )

    shipping_sizes = (64, 72, 96)
    calm_representative = clips[0].frames[clips[0].data["representative_frame"]]
    for clip in clips:
        for size in shipping_sizes:
            scaled_frames = [
                _scale_nearest(frame, size, size) for frame in clip.frames
            ]
            if clip.data["kind"] == "static":
                payload = encode_png(scaled_frames[0])
            else:
                payload = encode_apng(
                    scaled_frames,
                    clip.data["frame_duration_ms"],
                    0 if clip.data["kind"] == "loop" else 1,
                )
            products[f"motion/{clip.data['id']}-{size}px.png"] = payload
    for size in shipping_sizes:
        greet_handoff = [
            *(_scale_nearest(frame, size, size) for frame in clips[1].frames),
            _scale_nearest(calm_representative, size, size),
        ]
        products[f"motion/greet-handoff-{size}px.png"] = encode_apng(
            greet_handoff,
            clips[1].data["frame_duration_ms"],
            0,
        )
        seam = [
            _scale_nearest(clips[0].frames[-1], size, size),
            _scale_nearest(clips[0].frames[0], size, size),
        ]
        products[f"motion/calm-seam-{size}px.png"] = encode_apng(
            seam,
            clips[0].data["frame_duration_ms"],
            0,
        )

    backgrounds = {
        "system": (238, 238, 240),
        "light": (255, 255, 255),
        "dark": (31, 31, 35),
        "high-contrast": (0, 0, 0),
    }
    scale_sizes = (96, 120, 192)
    representatives: list[RgbaImage] = []
    for clip in clips:
        frame = clip.frames[clip.data["representative_frame"]]
        for size in scale_sizes:
            scaled = _scale_nearest(frame, size, size)
            for background in backgrounds.values():
                representatives.append(_composite(background, scaled))
    products["appearance-contexts.png"] = encode_png(
        _join_grid(representatives, columns=len(backgrounds))
    )
    reduced_motion = [
        clips[2].frames[0],
        clips[0].frames[clips[0].data["representative_frame"]],
        clips[1].frames[clips[1].data["representative_frame"]],
    ]
    products["reduced-motion-comparison.png"] = encode_png(
        _join_grid(reduced_motion, columns=3)
    )
    preview_manifest = {
        "schema_version": SCHEMA_VERSION,
        "asset_manifest_sha256": sha256_file(root / "dif-sprite-manifest.json"),
        "files": {
            path: sha256_bytes(payload) for path, payload in sorted(products.items())
        },
        "note": (
            "Mechanical contact sheets, overlays, context plates, and "
            "declared-speed APNG previews; attended approval remains separate."
        ),
    }
    products["preview-manifest.json"] = canonical_json_bytes(preview_manifest)
    for relative, payload in sorted(products.items()):
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("validate", "verify"):
        subparser = subparsers.add_parser(command)
        subparser.add_argument("--assets", type=Path, required=True)
    generate = subparsers.add_parser("generate")
    generate.add_argument("--assets", type=Path, required=True)
    generate.add_argument("--out", type=Path, required=True)
    preview = subparsers.add_parser("preview")
    preview.add_argument("--assets", type=Path, required=True)
    preview.add_argument("--out", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        if arguments.command == "validate":
            validate_assets(arguments.assets)
            print("DIF_ASSETS_VALID")
        elif arguments.command == "verify":
            verify_assets(arguments.assets)
            print("DIF_ASSETS_REPRODUCIBLE")
        elif arguments.command == "generate":
            _write_products(generated_products(arguments.assets), arguments.out)
            print("DIF_ASSETS_GENERATED")
        elif arguments.command == "preview":
            render_previews(arguments.assets, arguments.out)
            print("DIF_ASSET_PREVIEWS_GENERATED")
        else:  # pragma: no cover - argparse owns the command set.
            raise AssetError("unsupported command")
    except AssetError as error:
        print(f"DIF_ASSETS_ERROR: {error}", file=sys.stderr)
        return 1
    except OSError as error:
        detail = error.strerror or "unknown filesystem error"
        print(f"DIF_ASSETS_ERROR: filesystem operation failed: {detail}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
