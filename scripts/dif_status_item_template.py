#!/usr/bin/env python3
"""Derive the macOS menu bar status item's template image from Dif's art.

The status item shows Dif's head as a template image: black with alpha, so
macOS draws it in the menu bar's own colour for light, dark and highlighted
bars. This script is the only way those bytes are made. It reads the accepted
neutral master, keeps the head down to the neck, inks the dark fur (ears, eye
patches, eyes, nose and mouth) and a one-point outline of the silhouette so the
pale face still reads, and box-averages that coverage into an 18 x 18 image and
its 36 x 36 @2x partner.

It uses the standard library and ``scripts/dif_assets.py``'s PNG codec, never
an image tool, so ``check`` reproduces the committed bytes exactly on any
machine. Ordinary builds only consume the committed files; nothing here runs
in a build.

    python3 scripts/dif_status_item_template.py generate   # rewrite the pair
    python3 scripts/dif_status_item_template.py check      # compare and validate
"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))

import dif_assets  # noqa: E402  (the repository's own deterministic PNG codec)

SOURCE = REPOSITORY_ROOT / "src/adapters/qt/assets/dif/source/dif-neutral-master.png"
# The accepted d10 neutral master (src/adapters/qt/assets/dif/provenance.json).
# A different master is a different glyph: regenerate deliberately, not silently.
SOURCE_SHA256 = "15af5f8c072d039e98e16bdb8eeb121ce0eb74727fb3c9748217ce7ffbb4f34f"
OUTPUT_ROOT = REPOSITORY_ROOT / "src/adapters/qt/assets/status-item"
OUTPUTS = {18: "dif-template.png", 36: "dif-template@2x.png"}

# Recipe. Rows from the top of the master to the neck, the narrowest row
# between the head and the shoulders, so the arms do not join the head.
NECK_ROW = 566
OPAQUE_ALPHA = 128
# Fur darker than this (sRGB luma, 0-255) is ink: the purple ears and eye
# patches, the eyes, nose and mouth. The pale face and body are not.
DARK_LUMA = 115
# The square the head is centred in, in source pixels: 38 output pixels per
# source run at 18 and 19 at 36, so both sizes sample whole source pixels.
CANVAS_SIDE = 684
# The outline is one point wide: one pixel at 18 and two at 36. This is the
# 18 x 18 image's pixel run, in source pixels.
OUTLINE_SOURCE_PIXELS = CANVAS_SIDE // 18


class TemplateError(RuntimeError):
    """The source or a committed template differs from the recipe."""


def _luma(red: int, green: int, blue: int) -> int:
    return (2126 * red + 7152 * green + 722 * blue) // 10000


def _head_masks(image: dif_assets.RgbaImage) -> tuple[list[bytearray], list[bytearray], int, int]:
    """Silhouette and dark-fur masks over the head, placed on the square canvas."""
    width = image.width
    pixels = image.pixels
    rows = [y for y in range(NECK_ROW) if any(pixels[(y * width + x) * 4 + 3] >= OPAQUE_ALPHA for x in range(width))]
    if not rows:
        raise TemplateError("the master has no opaque head rows")
    top = rows[0]
    columns = [
        x
        for x in range(width)
        if any(pixels[(y * width + x) * 4 + 3] >= OPAQUE_ALPHA for y in range(top, NECK_ROW))
    ]
    left, right = columns[0], columns[-1] + 1
    head_width, head_height = right - left, NECK_ROW - top
    if head_width > CANVAS_SIDE or head_height > CANVAS_SIDE:
        raise TemplateError("the head does not fit the template canvas")
    offset_x = left - (CANVAS_SIDE - head_width) // 2
    # Centred, then raised so the neck cut falls on a pixel boundary at both
    # sizes; otherwise the flat chin smears across two half-covered rows.
    neck_on_canvas = (CANVAS_SIDE + head_height) // 2
    neck_on_canvas -= neck_on_canvas % OUTLINE_SOURCE_PIXELS
    offset_y = NECK_ROW - neck_on_canvas
    body = [bytearray(CANVAS_SIDE) for _ in range(CANVAS_SIDE)]
    dark = [bytearray(CANVAS_SIDE) for _ in range(CANVAS_SIDE)]
    for cy in range(CANVAS_SIDE):
        y = cy + offset_y
        if y < top or y >= NECK_ROW:
            continue
        for cx in range(CANVAS_SIDE):
            x = cx + offset_x
            if x < 0 or x >= width:
                continue
            index = (y * width + x) * 4
            if pixels[index + 3] < OPAQUE_ALPHA:
                continue
            body[cy][cx] = 1
            if _luma(pixels[index], pixels[index + 1], pixels[index + 2]) < DARK_LUMA:
                dark[cy][cx] = 1
    return body, dark, offset_x, offset_y


def _edge_distance(body: list[bytearray]) -> list[list[int]]:
    """Chamfer (3-4) distance from each silhouette pixel to the background, in thirds of a pixel."""
    side = len(body)
    far = 3 * side * 4
    distance = [[far if body[y][x] else 0 for x in range(side)] for y in range(side)]
    for y in range(side):
        row = distance[y]
        above = distance[y - 1] if y > 0 else None
        for x in range(side):
            if not row[x]:
                continue
            best = row[x]
            left = row[x - 1] + 3 if x > 0 else 3
            best = min(best, left)
            if above is not None:
                best = min(best, above[x] + 3)
                best = min(best, (above[x - 1] if x > 0 else 0) + 4)
                best = min(best, (above[x + 1] if x + 1 < side else 0) + 4)
            else:
                best = min(best, 3)
            if x == 0 or x + 1 == side:
                best = min(best, 3)
            row[x] = best
    for y in range(side - 1, -1, -1):
        row = distance[y]
        below = distance[y + 1] if y + 1 < side else None
        for x in range(side - 1, -1, -1):
            if not row[x]:
                continue
            best = row[x]
            best = min(best, (row[x + 1] if x + 1 < side else 0) + 3)
            if below is not None:
                best = min(best, below[x] + 3)
                best = min(best, (below[x + 1] if x + 1 < side else 0) + 4)
                best = min(best, (below[x - 1] if x > 0 else 0) + 4)
            else:
                best = min(best, 3)
            row[x] = best
    return distance


def render(size: int, source: dif_assets.RgbaImage | None = None) -> bytes:
    """The committed PNG bytes for one size."""
    if CANVAS_SIDE % size:
        raise TemplateError(f"{size} does not divide the template canvas")
    image = source if source is not None else _load_source()
    body, dark, _, _ = _head_masks(image)
    distance = _edge_distance(body)
    run = CANVAS_SIDE // size
    outline = 3 * OUTLINE_SOURCE_PIXELS
    area = run * run
    out = bytearray()
    for oy in range(size):
        for ox in range(size):
            ink = 0
            for y in range(oy * run, (oy + 1) * run):
                body_row, dark_row, distance_row = body[y], dark[y], distance[y]
                for x in range(ox * run, (ox + 1) * run):
                    if body_row[x] and (dark_row[x] or distance_row[x] <= outline):
                        ink += 1
            out += bytes((0, 0, 0, (255 * ink + area // 2) // area))
    return dif_assets.encode_png(dif_assets.RgbaImage(size, size, bytes(out)))


def _load_source() -> dif_assets.RgbaImage:
    data = SOURCE.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SOURCE_SHA256:
        raise TemplateError(f"the neutral master is not the accepted one ({digest})")
    return dif_assets.decode_png_bytes(data, "neutral master")


def validate(size: int, data: bytes) -> None:
    """A committed template is a black-on-alpha image of its size with a visible glyph."""
    image = dif_assets.decode_png_bytes(data, f"status item template {size}")
    if (image.width, image.height) != (size, size):
        raise TemplateError(f"the {size} template is {image.width} x {image.height}")
    pixels = image.pixels
    covered = 0
    opaque = 0
    for index in range(0, len(pixels), 4):
        red, green, blue, alpha = pixels[index : index + 4]
        if (red, green, blue) != (0, 0, 0):
            raise TemplateError(f"the {size} template has colour at pixel {index // 4}: a template image is black with alpha")
        covered += alpha > 0
        opaque += alpha >= 192
    total = size * size
    if not total // 5 <= covered <= total * 9 // 10 or opaque < total // 10:
        raise TemplateError(f"the {size} template's glyph covers {covered} of {total} pixels, {opaque} of them solid")


def check() -> None:
    source = _load_source()
    for size, name in OUTPUTS.items():
        committed = (OUTPUT_ROOT / name).read_bytes()
        validate(size, committed)
        if committed != render(size, source):
            raise TemplateError(f"{name} is not what the recipe makes; run generate")


def generate() -> None:
    source = _load_source()
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    for size, name in OUTPUTS.items():
        data = render(size, source)
        validate(size, data)
        (OUTPUT_ROOT / name).write_bytes(data)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=("generate", "check"))
    arguments = parser.parse_args(argv)
    try:
        generate() if arguments.command == "generate" else check()
    except (TemplateError, dif_assets.AssetError) as error:
        print(f"STATUS_ITEM_TEMPLATE_FAILED: {error}", file=sys.stderr)
        return 1
    print(f"STATUS_ITEM_TEMPLATE_OK: {arguments.command}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
