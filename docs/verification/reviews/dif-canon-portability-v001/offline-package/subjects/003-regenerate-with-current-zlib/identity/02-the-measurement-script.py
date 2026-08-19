#!/usr/bin/env python3
"""Measure what the Dif asset canon's byte-equality term actually adds. BPR-0378.

Question 1: does the committed tree pass the current canon on this interpreter?
Question 2: if byte equality is replaced by "the decompressed filtered scanlines
            equal the generator's own filtering", does a deliberately altered
            pixel still fail?
Question 3: what exactly does byte equality assert that the replacement does not?
"""

from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

ROOT = Path("/Users/jaeger/Documents/GitHub/BetterPullReview")
sys.path.insert(0, str(ROOT / "scripts"))

import dif_assets as DA  # noqa: E402

ASSET_ROOT = None
for candidate in (
    ROOT / "assets" / "dif",
    ROOT / "docs" / "assets" / "dif",
):
    if candidate.is_dir():
        ASSET_ROOT = candidate
        break
if ASSET_ROOT is None:
    hits = sorted(ROOT.rglob("dif-neutral-master.png"))
    hits = [h for h in hits if "build" not in h.parts]
    if not hits:
        raise SystemExit("could not locate the Dif asset tree")
    ASSET_ROOT = hits[0].parent.parent

MASTER = ASSET_ROOT / "source" / "dif-neutral-master.png"
print(f"zlib: compile={zlib.ZLIB_VERSION} runtime={zlib.ZLIB_RUNTIME_VERSION}")
print(f"asset root: {ASSET_ROOT.relative_to(ROOT)}")
print(f"master: {MASTER.relative_to(ROOT)} ({MASTER.stat().st_size} bytes)")


def scanlines_of(data: bytes) -> bytes:
    """The decompressed filtered scanlines a PNG carries, per its own IDATs."""
    offset = len(DA.PNG_SIGNATURE)
    idats = []
    header = None
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + size]
        if kind == b"IHDR":
            header = payload
        elif kind == b"IDAT":
            idats.append(payload)
        offset += 12 + size
    assert header is not None
    return zlib.decompress(b"".join(idats))


def count_idats(data: bytes) -> int:
    offset = len(DA.PNG_SIGNATURE)
    count = 0
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        if data[offset + 4 : offset + 8] == b"IDAT":
            count += 1
        offset += 12 + size
    return count


original = MASTER.read_bytes()
image = DA.decode_png_bytes(original, "master")
reencoded = DA.encode_png(image)

print()
print("--- Q1: the current canon on this interpreter ---")
print(f"byte equality (current canon): {original == reencoded}")
print(f"original bytes {len(original)}  re-encoded {len(reencoded)}")
print(f"IDAT chunks in the committed file: {count_idats(original)}")

generator_scanlines = DA._filtered_rgba_rows(image)
committed_scanlines = scanlines_of(original)
print()
print("--- Q3: what the replacement keeps ---")
print(
    "committed filtered scanlines == the generator's own filtering: "
    f"{committed_scanlines == generator_scanlines}"
)
print(f"scanline bytes: {len(committed_scanlines)}")

print()
print("--- Q2: does an altered pixel still fail the replacement? ---")
# Flip one pixel's red channel by one, in the middle of the image, then
# re-encode it into a structurally valid canonical PNG. This is the
# hand-edited-sprite threat: a real PNG that decodes cleanly but is not what
# the generator produces.
pixels = bytearray(image.pixels)
midpoint = (len(pixels) // 8) * 4
before = pixels[midpoint]
pixels[midpoint] = (before + 17) % 256
altered_image = DA.RgbaImage(image.width, image.height, bytes(pixels))
altered_bytes = DA.encode_png(altered_image)
print(f"altered one channel at offset {midpoint}: {before} -> {pixels[midpoint]}")

# Does it survive structural validation? It must, or the test is vacuous.
altered_decoded = DA.decode_png_bytes(altered_bytes, "altered")
print(f"altered file passes decode_png_bytes structural validation: True")

altered_scanlines = scanlines_of(altered_bytes)
print(
    "REPLACEMENT verdict on the altered file "
    "(scanlines == generator filtering of the COMMITTED image): "
    f"{altered_scanlines == generator_scanlines}"
)
print(
    "  -> False means the replacement rejects a hand-edited sprite, "
    "which is the property the canon exists for."
)
print(
    "CURRENT verdict on the altered file (byte equality against the committed "
    f"file): {altered_bytes == original}"
)

print()
print("--- Q3b: the one property byte equality adds ---")
# Re-compress the identical scanlines at a different level: same pixels, same
# filtering, different compressed bytes. This is exactly what a different zlib
# does, and it is the only thing byte equality catches that the replacement
# does not.
recompressed = (
    DA.PNG_SIGNATURE
    + DA._png_chunk(
        b"IHDR",
        struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0),
    )
    + DA._png_chunk(b"IDAT", zlib.compress(generator_scanlines, level=6))
    + DA._png_chunk(b"IEND", b"")
)
print(f"level-6 recompression differs in bytes: {recompressed != original}")
print(
    "level-6 recompression has identical scanlines: "
    f"{scanlines_of(recompressed) == generator_scanlines}"
)
print(f"level-6 size {len(recompressed)} vs committed {len(original)}")
print(
    "  -> This file changes no pixel and no filter choice. Byte equality "
    "rejects it; the replacement accepts it."
)
