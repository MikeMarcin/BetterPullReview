#!/usr/bin/env python3
"""The menu bar status item's template image is reproducible and a template.

[[BPR-1250]]. The committed 18 x 18 and 36 x 36 images must be exactly what
``scripts/dif_status_item_template.py`` makes from the accepted neutral master,
and each must be black with alpha: AppKit reads a template image's alpha and
nothing else, so any colour is a mistake the menu bar would hide.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))

import dif_assets  # noqa: E402
import dif_status_item_template as template  # noqa: E402


def _image(size: int, pixel: tuple[int, int, int, int], glyph: tuple[int, int, int, int] | None = None) -> bytes:
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            inside = glyph is not None and size // 4 <= x < size * 3 // 4 and size // 4 <= y < size * 3 // 4
            pixels += bytes(glyph if inside else pixel)
    return dif_assets.encode_png(dif_assets.RgbaImage(size, size, bytes(pixels)))


class StatusItemTemplateTests(unittest.TestCase):
    def test_committed_images_are_the_recipe_and_templates(self) -> None:
        template.check()

    def test_each_size_is_black_with_alpha_and_has_a_glyph(self) -> None:
        for size, name in template.OUTPUTS.items():
            image = dif_assets.decode_png_bytes((template.OUTPUT_ROOT / name).read_bytes(), name)
            self.assertEqual((image.width, image.height), (size, size))
            alphas = image.pixels[3::4]
            self.assertEqual(set(image.pixels[0::4]) | set(image.pixels[1::4]) | set(image.pixels[2::4]), {0})
            self.assertIn(0, alphas)
            self.assertIn(255, alphas)

    def test_control_a_coloured_glyph_is_refused(self) -> None:
        coloured = _image(18, (0, 0, 0, 0), (120, 80, 200, 255))
        with self.assertRaisesRegex(template.TemplateError, "colour"):
            template.validate(18, coloured)

    def test_control_the_wrong_size_is_refused(self) -> None:
        with self.assertRaisesRegex(template.TemplateError, "is 36 x 36"):
            template.validate(18, (template.OUTPUT_ROOT / template.OUTPUTS[36]).read_bytes())

    def test_control_an_empty_image_is_refused(self) -> None:
        with self.assertRaisesRegex(template.TemplateError, "covers"):
            template.validate(18, _image(18, (0, 0, 0, 0)))

    def test_control_a_changed_byte_is_not_the_recipe(self) -> None:
        committed = (template.OUTPUT_ROOT / template.OUTPUTS[18]).read_bytes()
        self.assertEqual(committed, template.render(18))
        image = dif_assets.decode_png_bytes(committed, "18")
        pixels = bytearray(image.pixels)
        pixels[3] ^= 0xFF
        changed = dif_assets.encode_png(dif_assets.RgbaImage(18, 18, bytes(pixels)))
        self.assertNotEqual(changed, template.render(18))


if __name__ == "__main__":
    unittest.main()
