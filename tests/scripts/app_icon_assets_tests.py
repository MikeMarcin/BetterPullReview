#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import struct
import unittest
import zlib
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ICON_ROOT = REPOSITORY_ROOT / "packaging/icons"
PNG_SIZES = (16, 24, 32, 40, 48, 64, 128, 256, 512, 1024)
WINDOWS_SIZES = (16, 24, 32, 40, 48, 64, 128, 256)
CANONICAL_SVG_SHA256 = (
    "8a15a0cf806ffe26e2e19fe77bcfbe5a6c958d982c32240a6954f825980dae8b"
)


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise AssertionError(f"not a PNG with an IHDR: {path}")
    return struct.unpack(">II", data[16:24])


def png_top_left_alpha(path: Path) -> int:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"not a PNG: {path}")

    color_type = data[25]
    if data[24] != 8 or color_type != 6:
        raise AssertionError(f"not an 8-bit RGBA PNG: {path}")

    compressed = bytearray()
    offset = 8
    while offset < len(data):
        chunk_size = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk_data = data[offset + 8 : offset + 8 + chunk_size]
        if chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        offset += 12 + chunk_size

    scanlines = zlib.decompress(compressed)
    return scanlines[4]


def ico_sizes(path: Path) -> tuple[int, ...]:
    data = path.read_bytes()
    reserved, image_type, count = struct.unpack_from("<HHH", data)
    if reserved != 0 or image_type != 1:
        raise AssertionError(f"not a Windows icon: {path}")

    sizes: list[int] = []
    for index in range(count):
        offset = 6 + index * 16
        width, height = struct.unpack_from("BB", data, offset)
        width = width or 256
        height = height or 256
        if width != height:
            raise AssertionError(f"non-square ICO entry: {width}x{height}")
        byte_count, image_offset = struct.unpack_from("<II", data, offset + 8)
        if byte_count == 0 or image_offset + byte_count > len(data):
            raise AssertionError(f"invalid ICO payload at entry {index}")
        sizes.append(width)
    return tuple(sizes)


def icns_types(path: Path) -> set[bytes]:
    data = path.read_bytes()
    if data[:4] != b"icns" or struct.unpack_from(">I", data, 4)[0] != len(data):
        raise AssertionError(f"invalid ICNS header: {path}")

    types: set[bytes] = set()
    offset = 8
    while offset < len(data):
        if offset + 8 > len(data):
            raise AssertionError("truncated ICNS entry")
        entry_type = data[offset : offset + 4]
        entry_size = struct.unpack_from(">I", data, offset + 4)[0]
        if entry_size < 8 or offset + entry_size > len(data):
            raise AssertionError(f"invalid ICNS entry: {entry_type!r}")
        types.add(entry_type)
        offset += entry_size
    if offset != len(data):
        raise AssertionError("ICNS entries do not consume the container")
    return types


class AppIconAssetsTests(unittest.TestCase):
    def test_canonical_svg_has_reviewed_framing(self) -> None:
        source = (ICON_ROOT / "better_pull_review.svg").read_bytes()
        self.assertEqual(hashlib.sha256(source).hexdigest(), CANONICAL_SVG_SHA256)
        self.assertIn(b'viewBox="0 0 1024 1024"', source)
        self.assertIn(
            b'<clipPath id="app-icon"><rect width="1024" height="1024" rx="190"/></clipPath>',
            source,
        )
        self.assertIn(b'scale(0.84)', source)
        self.assertNotIn(b"<image", source)

    def test_png_family_has_every_runtime_and_desktop_size(self) -> None:
        for size in PNG_SIZES:
            with self.subTest(size=size):
                path = ICON_ROOT / f"png/{size}x{size}/better_pull_review.png"
                self.assertEqual(png_size(path), (size, size))
                self.assertEqual(png_top_left_alpha(path), 0)

    def test_windows_icon_contains_the_complete_size_family(self) -> None:
        self.assertEqual(
            ico_sizes(ICON_ROOT / "windows/better_pull_review.ico"),
            WINDOWS_SIZES,
        )
        resource = (ICON_ROOT / "windows/better_pull_review.rc").read_text(
            encoding="utf-8"
        )
        self.assertIn('ICON "better_pull_review.ico"', resource)

    def test_macos_icon_contains_standard_and_retina_entries(self) -> None:
        self.assertTrue(
            {
                b"ic11",
                b"ic12",
                b"ic07",
                b"ic13",
                b"ic08",
                b"ic14",
                b"ic09",
                b"ic10",
            }.issubset(icns_types(ICON_ROOT / "macos/better_pull_review.icns"))
        )

    def test_platform_and_runtime_bindings_are_complete(self) -> None:
        cmake = (REPOSITORY_ROOT / "src/app/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        main = (REPOSITORY_ROOT / "src/app/main.cpp").read_text(encoding="utf-8")
        macos_info = (REPOSITORY_ROOT / "packaging/macos/Info.plist.in").read_text(
            encoding="utf-8"
        )
        desktop = (
            ICON_ROOT
            / "linux/io.github.MikeMarcin.BetterPullReview.desktop"
        ).read_text(encoding="utf-8")

        self.assertIn("qt6_add_resources", cmake)
        self.assertIn("windows/better_pull_review.rc", cmake)
        self.assertIn("macos/better_pull_review.icns", cmake)
        self.assertIn("MACOSX_BUNDLE_ICON_FILE", cmake)
        self.assertIn("MACOSX_BUNDLE_INFO_PLIST", cmake)
        self.assertIn("<key>CFBundleDisplayName</key>", macos_info)
        self.assertIn(
            "<string>${MACOSX_BUNDLE_BUNDLE_NAME}</string>", macos_info
        )
        self.assertIn(
            'QGuiApplication::setApplicationDisplayName(\n'
            '\t\tQStringLiteral("BetterPullReview"))',
            main,
        )
        self.assertIn("QApplication::setWindowIcon(application_icon())", main)
        for size in PNG_SIZES[:-1]:
            self.assertIn(f":/icons/{size}x{size}/better_pull_review.png", main)
        self.assertIn("Icon=io.github.MikeMarcin.BetterPullReview", desktop)
        self.assertIn("Exec=better_pull_review", desktop)


if __name__ == "__main__":
    unittest.main()
