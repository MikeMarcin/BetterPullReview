#!/usr/bin/env python3
"""Writes the constructed inline-images contract the product can serve.

RFC-BPR-0084. The owner judges whether images in a pull request's description
and comments show the way GitHub shows them. This copies the constructed
nested-folders contract (`tests/fixtures/github/contracts/v1/nested-folders-v1`,
BPR-1213), gives it a description and two pull request comments that hold
GitHub's image forms (uploaded attachments, `<img width>`, a table, a list,
a linked image, an animated GIF, a `<picture>` with a dark source, an SVG badge,
a WebP photograph and an image that is gone), and writes, beside it, what GitHub's rendering
would serve for them:

  tests/fixtures/github/markdown-images/v1/markdown-images-v1/bodies.json
      each body node's rendered HTML, as GraphQL `bodyHTML` answers it;
  tests/fixtures/github/markdown-images/v1/markdown-images-v1/manifest.json
      each served address's image file and content type;
  tests/fixtures/github/markdown-images/v1/markdown-images-v1/*.png|gif|svg|webp
      the images, drawn here from code (the WebP is embedded, see SUNSET_WEBP).

Every image is synthetic and deterministic; nothing is fetched, and no
identity, address or signature is real (`jwt=fixture`).

Usage:
    build_markdown_images_fixture.py <repository root>
"""

from __future__ import annotations

import base64
import json
import math
import shutil
import struct
import sys
import zlib
from pathlib import Path

SOURCE = "tests/fixtures/github/contracts/v1/nested-folders-v1"
TARGET = "tests/fixtures/github/contracts/v1/markdown-images-v1"
IMAGES = "tests/fixtures/github/markdown-images/v1/markdown-images-v1"

ATTACHMENT = "https://github.com/user-attachments/assets/5b1d1c1e-0f2a-4c3b-9d4e-00000000000{}"
EXTERNAL = "https://images.example.invalid/{}"
PRIVATE = "https://private-user-images.githubusercontent.com/90001/9000000{0}-5b1d1c1e-0f2a-4c3b-9d4e-00000000000{0}.png?jwt=fixture"


def camo(address: str) -> str:
    digest = zlib.crc32(address.encode("utf-8"))
    return f"https://camo.githubusercontent.com/{digest:08x}{'0' * 56}/{address.encode('utf-8').hex()}"


DESCRIPTION = f"""## Export summary card

The weekly export summary becomes a card. Screenshots from the issue it fixes:

![The summary before the change]({ATTACHMENT.format(1)})

<img width="420" alt="The summary card at a set width" src="{ATTACHMENT.format(2)}" />

| Before | After |
|---|---|
| <img width="300" alt="Before" src="{ATTACHMENT.format(3)}" /> | <img width="300" alt="After" src="{ATTACHMENT.format(4)}" /> |

- A transparent PNG through GitHub's image proxy: ![Dice with transparency]({EXTERNAL.format("dice.png")})
- A linked photograph: [![A flower]({EXTERNAL.format("flower.png")})]({EXTERNAL.format("flower")})
- An animated GIF at a set height: <img height="48" alt="Loading" src="{EXTERNAL.format("loading.gif")}">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="{EXTERNAL.format("theme-dark.png")}">
  <img alt="A themed banner" src="{EXTERNAL.format("theme-light.png")}">
</picture>

Build status: ![build]({EXTERNAL.format("badge.svg")})

A WebP photograph: ![Sunset over the bay]({EXTERNAL.format("sunset.webp")})

`<img src="not-an-image.png">` stays code.
"""

COMMENTS = (
    ("IC_mi_1", "tbutler", f'Narrow layout for the card: <img width="240" alt="Narrow layout" src="{ATTACHMENT.format(5)}" />\n'),
    ("IC_mi_2", "jiayin", f"Spinner while it loads: ![Loading spinner]({EXTERNAL.format('spinner.gif')})\n\nAnd one that is gone: ![Missing image]({EXTERNAL.format('missing.png')})\n"),
)

AUTHORS = {"tbutler": "U_fixture_tbutler", "jiayin": "U_fixture_jiayin"}


# --------------------------------------------------------------------------- images


def png(width: int, height: int, pixel) -> bytes:
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x, y))
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def lzw(indices: bytes, minimum: int) -> bytes:
    clear, end = 1 << minimum, (1 << minimum) + 1
    table = {bytes([value]): value for value in range(clear)}
    size, next_code = minimum + 1, end + 1
    output, bits, count = bytearray(), 0, 0

    def emit(code: int) -> None:
        nonlocal bits, count
        bits |= code << count
        count += size
        while count >= 8:
            output.append(bits & 0xFF)
            bits >>= 8
            count -= 8

    emit(clear)
    current = b""
    for value in indices:
        candidate = current + bytes([value])
        if candidate in table:
            current = candidate
            continue
        emit(table[current])
        if next_code < 4096:
            table[candidate] = next_code
            next_code += 1
            if next_code > (1 << size) and size < 12:
                size += 1
        else:
            emit(clear)
            table = {bytes([v]): v for v in range(clear)}
            size, next_code = minimum + 1, end + 1
        current = bytes([value])
    if current:
        emit(table[current])
    emit(end)
    if count:
        output.append(bits & 0xFF)
    blocks = bytearray([minimum])
    for start in range(0, len(output), 255):
        piece = output[start:start + 255]
        blocks.append(len(piece))
        blocks.extend(piece)
    blocks.append(0)
    return bytes(blocks)


def gif(width: int, height: int, palette: list[tuple[int, int, int]], frames: list[bytes], delay_cs: int) -> bytes:
    depth = max(1, math.ceil(math.log2(len(palette))))
    entries = 1 << depth
    table = b"".join(bytes(colour) for colour in palette) + b"\x00\x00\x00" * (entries - len(palette))
    data = bytearray(b"GIF89a" + struct.pack("<HHBBB", width, height, 0x80 | (depth - 1), 0, 0) + table)
    data += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"
    for frame in frames:
        data += b"\x21\xF9\x04\x00" + struct.pack("<H", delay_cs) + b"\x00\x00"
        data += b"\x2C" + struct.pack("<HHHHB", 0, 0, width, height, 0)
        data += lzw(frame, max(2, depth))
    data += b"\x3B"
    return bytes(data)


def screenshot(width: int, height: int, accent: tuple[int, int, int], dark: bool = False):
    background = (30, 33, 39) if dark else (246, 248, 250)
    panel = (44, 48, 56) if dark else (255, 255, 255)
    line = (70, 76, 88) if dark else (208, 215, 222)
    text = (150, 158, 170) if dark else (110, 118, 129)

    def pixel(x: int, y: int) -> bytes:
        if y < 48:
            return bytes((36, 41, 47, 255)) if not dark else bytes((13, 17, 23, 255))
        if x < width // 5:
            return bytes((*((238, 241, 244) if not dark else (22, 27, 34)), 255))
        left, top = width // 5 + 32, 80
        if left <= x < width - 32 and top <= y < height - 32:
            inside = left + 1 <= x < width - 33 and top + 1 <= y < height - 33
            if not inside:
                return bytes((*line, 255))
            if top + 24 <= y < top + 40 and left + 24 <= x < left + 24 + (width - left) // 3:
                return bytes((*accent, 255))
            row = (y - top - 64) // 28
            if y > top + 64 and (y - top - 64) % 28 < 10 and left + 24 <= x < left + 24 + ((width - left) // 2 - row * 23 % 160):
                return bytes((*text, 255))
            return bytes((*panel, 255))
        return bytes((*background, 255))

    return pixel


def dice(x: int, y: int) -> bytes:
    for cx, cy, radius, colour in ((90, 110, 70, (200, 40, 40)), (190, 100, 60, (40, 110, 200))):
        if (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius:
            pip = any((x - (cx + dx)) ** 2 + (y - (cy + dy)) ** 2 <= 64 for dx, dy in ((-25, -25), (0, 0), (25, 25)))
            return bytes((255, 255, 255, 255)) if pip else bytes((*colour, 210))
    return bytes((0, 0, 0, 0))


def flower(x: int, y: int) -> bytes:
    cx, cy = 160, 106
    angle = math.atan2(y - cy, x - cx)
    distance = math.hypot(x - cx, y - cy)
    petal = 60 + 25 * math.cos(6 * angle)
    if distance < 22:
        return bytes((240, 200, 40, 255))
    if distance < petal:
        return bytes((220, 90 + int(distance), 150, 255))
    return bytes((70, 120 + (y * 60) // 213, 60, 255))


def banner(dark: bool):
    def pixel(x: int, y: int) -> bytes:
        if dark:
            base = (13, 17, 23)
            mark = (88, 166, 255)
        else:
            base = (255, 255, 255)
            mark = (9, 105, 218)
        if 20 <= y < 60 and 20 <= x < 60:
            return bytes((*mark, 255))
        if 30 <= y < 38 and 76 <= x < 300 or 46 <= y < 52 and 76 <= x < 220:
            return bytes((*mark, 255)) if y < 38 else bytes((*(mark if dark else (87, 96, 106)), 255))
        return bytes((*base, 255))
    return pixel


def spinner_frames(size: int, count: int) -> list[bytes]:
    frames = []
    centre = (size - 1) / 2
    for frame in range(count):
        indices = bytearray()
        for y in range(size):
            for x in range(size):
                dx, dy = x - centre, y - centre
                distance = math.hypot(dx, dy)
                if size * 0.28 <= distance <= size * 0.46:
                    angle = (math.degrees(math.atan2(dy, dx)) + 360.0 - frame * (360.0 / count)) % 360.0
                    indices.append(1 if angle < 90.0 else 2)
                else:
                    indices.append(0)
        frames.append(bytes(indices))
    return frames


# RFC-BPR-0084 owner amendment 1. A 240x120 lossy WebP sunset, embedded
# because no WebP encoder is assumed here: drawn with Pillow and encoded once
# with `cwebp -q 80 -m 6` (libwebp 1.5).
SUNSET_WEBP = base64.b64decode(
    "UklGRloDAABXRUJQVlA4IE4DAAAwGQCdASrwAHgAPm02l0ikIyIhJjToAIANiWNu/Hv0OPJEwt+S"
    "uOmuq3G3pgG87ZDOrt/ugiZFSdlrwkXBKr85k9ZS0E9mKk7LXhIt5LNXhouSld0RG3b37zXQgxgQ"
    "J4fG2UUrREXJS0E38mhABBbppjCwVvbNkUTr8pzMVJ2WstgMkBVKrWSQP5YqfkPnJmVeznB+DPAH"
    "y27gkkaYCv38RKHvPsJ3bJncgTINKq0YYTObPUN7fuSph6YTLmla/Wl9+92rSLbbbbbbbbbaKGI8"
    "FLwA/u/zaBbn/+dg+PdE+1D/+SnxxxZ5L69U3ChcuT6o1EYfBM2UaiMPgmbKN1Z0ddPuN/AXfkup"
    "Z4DWk0E0vLoAgp5SXLy1/LoAqNUf0j/yJ9DoaZaAAAG1+8gJtUVIPiYEM4Bi7QT8O9DE3sod/ef7"
    "Nfp1PrB2pRmR+OhrEcN551J5DzL2LGPlcnpH/kT6HJEk8RgSIi0DmI6s4oLOzfxA2SPCk27hwozq"
    "QccEn5GRQP3kL+CW/Xx6682hwHdBjI6L25HO6/lxw6AtMidjj44V1WvUx/+EH5wdVVjHFfemZmny"
    "F0yMJsD54h2Cyf1R3asH7Pg4cFd+ciAkJItyZ4dWXktKdyw+EgkjgSiS/ManwehE4wFEGw730Ua7"
    "37V3l5aVNlGYBWu4Gh+Ipq45uC5xzzoYpa/CI4WuqmCjDwlgoxIyrMbel0/ypp/0MrbcDTGuS5ke"
    "dpz7PXmo9yTet6YFfHlEksRmg31ZPhZeNzpkiX2jBrxGa1rMZ3uSHYaHqnFOWVJ5eU6t0kmwXgjv"
    "wpYb03mCEatB4cJe5NXJH//Ftja27H39VmWEjy2IugPO46u3w9RbwV/GS/JOp9WC0L6R8njANFgu"
    "+e1+yaE9lhLqm453DE7rxP/f/XUfknSJ0WpJ+nz2ROrfKBe45+hlg5md+5/+x7lCzl6G5RQrg2U3"
    "nqGvs/5aE5UnJWlHJwmJ0QaOoF1f3zvRW0SQdy/oZr3DkaaQZoG4OrNHtltr2jMapgk1MuP62+X4"
    "aj8BG2DJfiiPPU8G77Ug4QXwV5HqGeiyRmvLBYVHvmJQ1Hys/C+62DvamrxhooZkuDz52mSYHxCW"
    "i24XfD+ZpgAAAAA="
)


BADGE = """<svg xmlns="http://www.w3.org/2000/svg" width="90" height="20"><rect width="90" height="20" rx="3" fill="#555"/><rect x="37" width="53" height="20" rx="3" fill="#4c1"/><text x="6" y="14" fill="#fff" font-family="Verdana" font-size="11">build</text><text x="42" y="14" fill="#fff" font-family="Verdana" font-size="11">passing</text></svg>
"""


def write(path: Path, document: object) -> None:
    path.write_text(json.dumps(document, indent="\t", sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8", newline="")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    source, target, images = root / SOURCE, root / TARGET, root / IMAGES
    for directory in (target, images):
        if directory.exists():
            shutil.rmtree(directory)
    shutil.copytree(source, target)
    images.mkdir(parents=True)

    pull_path = target / "responses/pull_request.json"
    pull = json.loads(pull_path.read_text(encoding="utf-8"))
    pull["title"] = "Show the weekly export summary as a card"
    pull["body"] = DESCRIPTION
    write(pull_path, pull)
    write(target / "responses/issue_comments.json", [
        {"node_id": identity, "user": {"node_id": AUTHORS[author], "login": author}, "body": text}
        for identity, author, text in COMMENTS
    ])
    repository = json.loads((target / "responses/repository.json").read_text(encoding="utf-8"))

    files = {
        "attachment-1.png": (png(1252, 625, screenshot(1252, 625, (9, 105, 218))), "image/png"),
        "attachment-2.png": (png(1253, 625, screenshot(1253, 625, (26, 127, 55))), "image/png"),
        "attachment-3.png": (png(600, 400, screenshot(600, 400, (207, 34, 46))), "image/png"),
        "attachment-4.png": (png(600, 400, screenshot(600, 400, (26, 127, 55))), "image/png"),
        "attachment-5.png": (png(480, 640, screenshot(480, 640, (130, 80, 223), dark=True)), "image/png"),
        "dice.png": (png(280, 210, dice), "image/png"),
        "flower.png": (png(320, 213, flower), "image/png"),
        "theme-light.png": (png(360, 80, banner(False)), "image/png"),
        "theme-dark.png": (png(360, 80, banner(True)), "image/png"),
        "loading.gif": (gif(64, 64, [(255, 255, 255), (9, 105, 218), (208, 215, 222)], spinner_frames(64, 8), 8), "image/gif"),
        "spinner.gif": (gif(32, 32, [(255, 255, 255), (130, 80, 223), (234, 238, 242)], spinner_frames(32, 12), 6), "image/gif"),
        "badge.svg": (BADGE.encode("utf-8"), "image/svg+xml; charset=utf-8"),
        "sunset.webp": (SUNSET_WEBP, "image/webp"),
    }
    for name, (data, _) in files.items():
        (images / name).write_bytes(data)

    served = {}
    for index in range(1, 6):
        served[ATTACHMENT.format(index)] = (PRIVATE.format(index), f"attachment-{index}.png", None)
    for name in ("dice.png", "flower.png", "loading.gif", "theme-dark.png", "theme-light.png", "badge.svg", "sunset.webp", "spinner.gif", "missing.png"):
        address = EXTERNAL.format(name)
        served[address] = (camo(address), name if name != "missing.png" else None, address)

    def img(source: str, attributes: str = "") -> str:
        address, _, canonical = served[source]
        extra = ' data-canonical-src="' + canonical + '"' if canonical else ""
        return '<img src="' + address + '"' + attributes + extra + ' style="max-width: 100%;">'

    dark_served = served[EXTERNAL.format("theme-dark.png")][0]
    description_html = "".join((
        "<h2>Export summary card</h2><p>The weekly export summary becomes a card.</p>",
        "<p>" + img(ATTACHMENT.format(1), ' alt="The summary before the change"') + "</p>",
        "<p>" + img(ATTACHMENT.format(2), ' width="420"') + "</p>",
        "<table><tr><td>" + img(ATTACHMENT.format(3), ' width="300"') + "</td><td>" + img(ATTACHMENT.format(4), ' width="300"') + "</td></tr></table>",
        "<ul><li>" + img(EXTERNAL.format("dice.png")) + "</li>",
        '<li><a href="' + EXTERNAL.format("flower") + '">' + img(EXTERNAL.format("flower.png")) + "</a></li>",
        "<li>" + img(EXTERNAL.format("loading.gif"), ' height="48"') + "</li></ul>",
        '<picture><source media="(prefers-color-scheme: dark)" srcset="' + dark_served + '" data-canonical-src="' + EXTERNAL.format("theme-dark.png") + '">',
        img(EXTERNAL.format("theme-light.png")) + "</picture>",
        "<p>Build status: " + img(EXTERNAL.format("badge.svg")) + "</p>",
        "<p>A WebP photograph: " + img(EXTERNAL.format("sunset.webp")) + "</p>",
        '<p><code>&lt;img src="not-an-image.png"&gt;</code> stays code.</p>',
    ))
    bodies = {
        pull["node_id"]: description_html,
        "IC_mi_1": "<p>Narrow layout for the card: " + img(ATTACHMENT.format(5), ' width="240"') + "</p>",
        "IC_mi_2": "<p>Spinner while it loads: " + img(EXTERNAL.format("spinner.gif")) + "</p><p>And one that is gone: " + img(EXTERNAL.format("missing.png")) + "</p>",
    }
    write(images / "bodies.json", {"repository": repository["node_id"], "pullRequest": pull["node_id"], "bodies": bodies})
    manifest = []
    for _, (address, file, _) in sorted(served.items()):
        if file is not None:
            manifest.append({"address": address, "file": file, "content_type": files[file][1]})
    write(images / "manifest.json", {"images": manifest})
    print(f"MARKDOWN_IMAGES_FIXTURE_OK:{len(manifest)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
