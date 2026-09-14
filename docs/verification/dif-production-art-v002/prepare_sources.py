"""Offline, review-only Dif source preparation; never touches runtime assets."""
from pathlib import Path
import hashlib
import json
import math
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PACK = ROOT / 'docs/rfcs/assets/bpr-dif-integration-v1'
MASTER = HERE.parent / 'dif-production-art-v001/dif-neutral-master-candidate.png'
sys.path.insert(0, str(ROOT / 'scripts'))
import dif_assets

# Full accepted digest, deliberately checked before any output.
EXPECTED = 'bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_bytes(image):
    pixels = np.array(image.convert('RGBA'))
    pixels[pixels[:, :, 3] == 0] = 0
    return dif_assets.encode_png(dif_assets.RgbaImage(image.width, image.height, pixels.tobytes()))


def save(image, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png_bytes(image))


def runtime_cell(image):
    rgba = dif_assets.resize_box(dif_assets.RgbaImage(image.width, image.height, image.tobytes()), clear_rounded_transparency=True)
    return Image.frombytes('RGBA', (256, 256), rgba.pixels)


def breath(master, amount):
    # Smooth local vertical displacement, zero at all ROI edges. No global scale.
    data = np.array(master)
    y, x = np.mgrid[0:1024, 0:1024]
    weight = np.maximum(0, 1 - ((x - 512) / 270) ** 2) ** 2
    weight *= np.where((y > 580) & (y < 792), np.sin(np.pi * np.clip((y - 580) / 212, 0, 1)) ** 2, 0)
    source_y = y + amount * 4 * weight
    low = np.floor(source_y).astype(int).clip(0, 1023)
    high = (low + 1).clip(0, 1023)
    fraction = (source_y - low)[..., None]
    result = np.rint(data[low, x] * (1 - fraction) + data[high, x] * fraction).astype('uint8')
    return Image.fromarray(result)


def wave(master, angle):
    # Provisional articulated original paw. Layer isolation/reconstruction is
    # explicit maintainer retouch, not claimed as extraction from incoming greet.
    mask = Image.new('L', master.size)
    ImageDraw.Draw(mask).polygon([(673, 572), (727, 565), (806, 658), (802, 742), (739, 780), (674, 756), (628, 698), (646, 640)], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(2))
    arm = master.copy()
    arm.putalpha(Image.fromarray(np.minimum(np.array(mask), np.array(master.getchannel('A')))))
    base = np.array(master)
    # Clone adjacent belly texture under the removed arm, then taper its contour.
    for y in range(560, 790):
        edge = 718 + 18 * math.sin((y - 560) / 230 * math.pi)
        for x in range(624, 815):
            blend = mask.getpixel((x, y)) / 255
            if blend:
                sample = base[y, 600 - (x - 624) % 42].copy()
                sample[3] = round(sample[3] * min(1, max(0, (edge - x) / 4)))
                base[y, x] = np.rint(base[y, x] * (1 - blend) + sample * blend)
    moved = arm.rotate(angle, Image.Resampling.BICUBIC, center=(697, 603))
    return Image.alpha_composite(Image.fromarray(base), moved)


def main():
    assert digest(MASTER) == EXPECTED
    master = Image.open(MASTER).convert('RGBA')
    (HERE / 'source').mkdir(exist_ok=True)
    save(master, HERE / 'source/dif-neutral-master.png')
    neutral = runtime_cell(master)
    clips = {'rest': [neutral], 'calm_idle': [runtime_cell(breath(master, a)) for a in [0, .5, 1, 1.5, 2, 1.5, 1, .5, 0, -.5, -1, -.5]], 'greet': [neutral] + [runtime_cell(wave(master, a)) for a in [15, 45, 80, 65, 45, 15]] + [neutral]}
    for name, frames in clips.items():
        for i, frame in enumerate(frames):
            if name != 'greet':
                save(frame, HERE / 'source' / name / f'frame-{i:03}.png')
        sheet = Image.new('RGBA', (256 * len(frames), 256))
        for i, frame in enumerate(frames):
            sheet.paste(frame, (256 * i, 0))
        save(sheet, HERE / 'preview' / f'{name}-row.png')
    inventory = []
    extended = ['blink', 'compare_glance', 'consider', 'comment_ready', 'attention', 'acknowledge', 'blocked']
    raw_plate = Image.new('RGB', (1024, 1036), '#eeeef4')
    raw_draw = ImageDraw.Draw(raw_plate)
    for asset in json.loads((PACK / 'assets/INPUT-MANIFEST.json').read_text())['assets']:
        path = PACK / asset['path']
        assert digest(path) == asset['sha256']
        original = Image.open(path).convert('RGBA')
        for i, rect in enumerate(asset['crop_proposals_half_open']):
            crop = original.crop(rect)
            output = HERE / 'native-crops' / asset['clip_id'] / f'frame-{i:03}.png'
            data = png_bytes(crop)
            if '--write-native-crops' in sys.argv:
                save(crop, output)
            if asset['clip_id'] in extended:
                x, y = i * 128, extended.index(asset['clip_id']) * 148
                raw_draw.text((x, y), f"{asset['clip_id']} {i}", fill='black')
                thumb = crop.copy()
                thumb.thumbnail((124, 124))
                raw_plate.paste(thumb, (x, y + 20), thumb)
            inventory.append({'clip': asset['clip_id'], 'frame': i, 'source_sha256': asset['sha256'], 'half_open_crop': rect, 'native_size': list(crop.size), 'sha256': hashlib.sha256(data).hexdigest(), 'status': 'provisional crop; zero hidden RGB only; no registration or matte repair'})
    raw_plate.save(HERE / 'preview/extended-raw-contact.png')
    (HERE / 'crop-inventory.json').write_text(json.dumps(inventory, indent=2) + '\n')
    plate = Image.new('RGB', (12 * 100, 3 * 124), '#ececf5')
    draw = ImageDraw.Draw(plate)
    for row, (name, frames) in enumerate(clips.items()):
        for i, frame in enumerate(frames):
            x, y = i * 100, row * 124
            draw.text((x + 2, y + 2), f'{name} {i}', fill='black')
            plate.paste(frame.resize((96, 96), Image.Resampling.LANCZOS), (x, y + 20), frame.resize((96, 96), Image.Resampling.LANCZOS))
    plate.save(HERE / 'preview/contact.png')
    print('Prepared 13 rest/idle candidate frames, a rejected wave contact study and 71 reproducible crop records.')


if __name__ == '__main__':
    main()
