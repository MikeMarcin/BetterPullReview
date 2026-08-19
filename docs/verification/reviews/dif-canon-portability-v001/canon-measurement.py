#!/usr/bin/env python3
"""Measure what `decode_png`'s byte-equality term actually asserts. BPR-0378.

This replaces a first version of this script whose central result was wrong. That
version compared an altered file's scanlines against scanlines derived from the
*committed* asset held in memory, and reported that the replacement rejects a
hand-edited sprite. `decode_png` has no committed reference to compare against —
it is called on the source master and on every source frame, where the only
inputs are the file's own bytes — so that comparison is not one the canon site can
make. See the Correction section of the card.

What is measured here is only what the canon site can actually decide, plus the
mechanism that does defend the property the card cares about.

Run from anywhere:

    python3 docs/verification/reviews/dif-canon-portability-v001/canon-measurement.py
"""

from __future__ import annotations

import hashlib
import pathlib
import struct
import sys
import zlib

# Resolved from this file rather than hard-coded, so the measurement is
# reproducible on a host whose checkout lives elsewhere.
ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "scripts"))

import dif_assets as DA  # noqa: E402

MASTER = ROOT / "src/adapters/qt/assets/dif/source/dif-neutral-master.png"


def chunks_of(data: bytes) -> list[tuple[bytes, bytes]]:
    offset = len(DA.PNG_SIGNATURE)
    out: list[tuple[bytes, bytes]] = []
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        out.append((data[offset + 4 : offset + 8], data[offset + 8 : offset + 8 + size]))
        offset += 12 + size
    return out


def rebuild(chunks: list[tuple[bytes, bytes]]) -> bytes:
    out = bytearray(DA.PNG_SIGNATURE)
    for kind, payload in chunks:
        out.extend(DA._png_chunk(kind, payload))
    return bytes(out)


def scanlines_of(data: bytes) -> bytes:
    return zlib.decompress(
        b"".join(payload for kind, payload in chunks_of(data) if kind == b"IDAT")
    )


def accepted_by_structure(data: bytes, label: str) -> bool:
    try:
        DA.decode_png_bytes(data, label)
        return True
    except DA.AssetError:
        return False


def canon_term(data: bytes) -> bool:
    """`decode_png`'s actual test: is this file exactly what encode_png emits?"""
    return data == DA.encode_png(DA.decode_png_bytes(data, "probe"))


original = MASTER.read_bytes()
image = DA.decode_png_bytes(original, "master")
chunks = chunks_of(original)

print(f"zlib: compile={zlib.ZLIB_VERSION} runtime={zlib.ZLIB_RUNTIME_VERSION}")
print(f"master: {MASTER.relative_to(ROOT)} ({len(original)} bytes)")
print(f"IDAT chunks: {sum(1 for k, _ in chunks if k == b'IDAT')}")
print(f"canon term holds on the committed file: {canon_term(original)}")

print()
print("--- 1. Does the canon term defend against a hand-edited sprite? ---")
pixels = bytearray(image.pixels)
offset = (len(pixels) // 8) * 4
pixels[offset] = (pixels[offset] + 17) % 256
altered = DA.encode_png(DA.RgbaImage(image.width, image.height, bytes(pixels)))
altered_image = DA.decode_png_bytes(altered, "altered")
print(f"one channel changed at byte {offset}, then re-encoded by encode_png")
print(f"  accepted by the structural check:      {accepted_by_structure(altered, 'a')}")
print(f"  accepted by TODAY's canon term:        {canon_term(altered)}")
print(
    "  accepted by a scanline comparison:     "
    f"{scanlines_of(altered) == DA._filtered_rgba_rows(altered_image)}"
)
print("  -> No. Both accept it. `decode_png` asserts that a file is in the tool's")
print("     canonical *serialization*, not that its pixels are the committed ones.")
print("     A scanline comparison at that site is vacuous besides: every row that")
print("     encode_png writes uses filter byte 0, so it can only ever agree.")

print()
print("--- 2. What does defend against it ---")
provenance = MASTER.parent.parent / "provenance.json"
recorded = None
if provenance.is_file():
    import json

    for entry in json.loads(provenance.read_text())["source_files"]:
        if entry["path"].endswith("dif-neutral-master.png"):
            recorded = entry["sha256"]
print(f"provenance record: {provenance.name}")
print(f"  recorded digest: {recorded}")
print(f"  file digest:     {hashlib.sha256(original).hexdigest()}")
print(f"  altered digest:  {hashlib.sha256(altered).hexdigest()}")
print("  -> `_validate_provenance` raises `provenance source digest mismatch`")
print("     (scripts/dif_assets.py:612-620). That is the check the edited sprite")
print("     fails, and no option in this decision touches it.")

print()
print("--- 3. What byte equality catches that the structural check does not ---")
recompressed = rebuild(
    [
        (k, zlib.compress(scanlines_of(original), level=6) if k == b"IDAT" else p)
        for k, p in chunks
    ]
)
padded_iend = rebuild(
    [(k, b"PADDINGPADDING" if k == b"IEND" else p) for k, p in chunks]
)
split_idat_chunks: list[tuple[bytes, bytes]] = []
for kind, payload in chunks:
    if kind == b"IDAT":
        split_idat_chunks.append((b"IDAT", payload[: len(payload) // 2]))
        split_idat_chunks.append((b"IDAT", payload[len(payload) // 2 :]))
    else:
        split_idat_chunks.append((kind, payload))
split_idat = rebuild(split_idat_chunks)

for label, variant, note in (
    ("level-6 recompression", recompressed, "identical pixels and filters; this is the zlib-ng case"),
    ("non-empty IEND payload", padded_iend, "not a DEFLATE property"),
    ("IDAT split in two", split_idat, "not a DEFLATE property"),
):
    print(f"{label}:")
    print(f"  bytes differ from committed:      {variant != original}")
    print(f"  accepted by the structural check: {accepted_by_structure(variant, label)}")
    print(
        "  same decoded scanlines:           "
        f"{scanlines_of(variant) == scanlines_of(original)}"
    )
    print(f"  ({note})")

print()
print("-> Byte equality's marginal coverage is three things, not one. Two of them")
print("   have nothing to do with which zlib is installed, so a replacement that")
print("   only stops comparing compressed bytes must assert them explicitly:")
print("   exactly one IDAT, and an empty IEND payload.")
