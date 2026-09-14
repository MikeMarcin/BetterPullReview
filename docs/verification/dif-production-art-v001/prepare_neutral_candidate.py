"""Prepare the first RFC-BPR-0030 neutral identity candidate offline.

This script writes review media only. It never changes runtime Dif resources.
"""

from __future__ import annotations

import hashlib
import io
from pathlib import Path
import zipfile

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
INPUT = ROOT / "docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/dif-rest.png"
EXPECTED_INPUT_SHA256 = "eacedea5ddfdc6317b007e2032093f620620777279c70f87250eadd79a4a90ae"
MASTER = HERE / "dif-neutral-master-candidate.png"
PLATE = HERE / "dif-neutral-master-review.png"
SIZE_PLATE = HERE / "dif-neutral-sizes-review.png"
REFERENCE_PLATE = HERE / "dif-neutral-identity-review.png"
REFERENCE_ARCHIVE = ROOT / "docs/rfcs/assets/bpr-dif-integration-v1/references/BetterPullReview-Dif-Animation-Prompt-Pack-v1.zip"
OLD_MASTER = ROOT / "src/adapters/qt/assets/dif/source/dif-neutral-master.png"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    if sha256(INPUT) != EXPECTED_INPUT_SHA256:
        raise ValueError("incoming rest bytes changed")

    original = Image.open(INPUT).convert("RGBA")
    if original.size != (1254, 1254):
        raise ValueError(f"unexpected rest geometry: {original.size}")

    # The source has isolated alpha=1 specks far from the panda. Retain four
    # native pixels around the alpha>1 subject; crop no substantive content.
    alpha = original.getchannel("A")
    subject = alpha.point(lambda value: 255 if value > 1 else 0).getbbox()
    if subject != (250, 209, 1003, 1138):
        raise ValueError(f"unexpected rest subject: {subject}")
    left, top, right, bottom = subject
    crop = original.crop((left - 4, top - 4, right + 4, bottom + 4))

    # Uniform resize preserves the supplied proportions. At 256 px, the
    # substantive subject is about 200 px tall, centered at x=128 and planted
    # at the inherited y=223 baseline. The 1024-square result remains editable.
    target_height = 808
    target_width = round(crop.width * target_height / crop.height)
    scaled = crop.resize((target_width, target_height), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (1024, 1024))
    canvas.alpha_composite(scaled, ((1024 - target_width) // 2, 892 - target_height))

    # Straight-alpha PNGs must not retain hidden RGB. Antialiased nonzero-alpha
    # edges and the original subject interior remain untouched.
    pixels = bytearray(canvas.tobytes())
    for index in range(0, len(pixels), 4):
        if pixels[index + 3] == 0:
            pixels[index : index + 3] = b"\0\0\0"
    canvas = Image.frombytes("RGBA", canvas.size, bytes(pixels))
    canvas.save(MASTER, format="PNG", optimize=False)

    font = ImageFont.load_default()
    plate = Image.new("RGB", (960, 430), "#ececf5")
    draw = ImageDraw.Draw(plate)
    surfaces = [("Light", "#f8f8fc", 16), ("Dark", "#20212b", 332), ("High contrast", "#000000", 648)]
    preview = canvas.resize((256, 256), Image.Resampling.LANCZOS)
    for title, background, x in surfaces:
        draw.rounded_rectangle((x, 42, x + 296, 366), radius=12, fill=background)
        draw.text((x + 12, 14), title, fill="#232333", font=font)
        plate.paste(preview, (x + 20, 60), preview)
    draw.text((16, 392), "Review candidate only | raw rest SHA-256: " + EXPECTED_INPUT_SHA256[:20] + "...", fill="#232333", font=font)
    plate.save(PLATE, format="PNG", optimize=False)

    sizes = Image.new("RGB", (720, 310), "#ececf5")
    size_draw = ImageDraw.Draw(sizes)
    for row, (title, background) in enumerate((("Light", "#f8f8fc"), ("Dark", "#20212b"))):
        y = 20 + row * 145
        size_draw.text((16, y), title, fill="#232333", font=font)
        for x, side in ((120, 64), (320, 72), (520, 96)):
            size_draw.rounded_rectangle((x - 8, y - 6, x + 152, y + 122), radius=8, fill=background)
            sprite = canvas.resize((side, side), Image.Resampling.LANCZOS)
            sizes.paste(sprite, (x + (144 - side) // 2, y), sprite)
            size_draw.text((x + 60, y + 101), f"{side}px", fill="#f5f5fa" if row else "#232333", font=font)
    sizes.save(SIZE_PLATE, format="PNG", optimize=False)

    with zipfile.ZipFile(REFERENCE_ARCHIVE) as archive:
        def reference(suffix: str, expected: str) -> Image.Image:
            name = next(name for name in archive.namelist() if name.endswith(suffix))
            data = archive.read(name)
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f"reference bytes changed: {suffix}")
            return Image.open(io.BytesIO(data)).convert("RGBA")

        brand = reference(
            "reference-inputs/Dif Panda Code Review Brand Board.png",
            "cf35d96198e20575069b3dd43edd352a04393e457970219738a9e888458729c0",
        ).crop((280, 165, 600, 475))
        lineup = reference(
            "reference-inputs/BetterPullReview Mascot Lineup.png",
            "5859655aa4c39e50a7e3c522e48f8b34c48bd5749553089be95860e7f2d0f39e",
        ).crop((1010, 430, 1500, 880))

    identity = Image.new("RGB", (1320, 380), "#ececf5")
    identity_draw = ImageDraw.Draw(identity)
    panels = (
        ("Dif brand reference (prop guidance only)", brand),
        ("Dif in lineup (foreign art excluded)", lineup),
        ("Previously accepted card-bearing master", Image.open(OLD_MASTER).convert("RGBA")),
        ("New prop-free candidate", canvas),
    )
    for index, (title, subject_image) in enumerate(panels):
        x = 12 + index * 328
        identity_draw.rounded_rectangle((x, 32, x + 312, 368), radius=10, fill="#f8f8fc")
        identity_draw.text((x + 8, 10), title, fill="#232333", font=font)
        thumb = subject_image.copy()
        thumb.thumbnail((296, 312), Image.Resampling.LANCZOS)
        identity.paste(thumb, (x + (312 - thumb.width) // 2, 44 + (312 - thumb.height) // 2), thumb)
    identity.save(REFERENCE_PLATE, format="PNG", optimize=False)

    print(f"master_sha256={sha256(MASTER)}")
    print(f"plate_sha256={sha256(PLATE)}")
    print(f"size_plate_sha256={sha256(SIZE_PLATE)}")
    print(f"identity_plate_sha256={sha256(REFERENCE_PLATE)}")
    print(f"master_alpha_bounds={canvas.getchannel('A').getbbox()}")


if __name__ == "__main__":
    main()
