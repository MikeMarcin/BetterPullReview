"""Reproducible rejected raw-pose studies; no runtime or production writes."""
from pathlib import Path
import hashlib
import json
import sys
import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PACK = ROOT / 'docs/rfcs/assets/bpr-dif-integration-v1'
V2 = HERE.parents[1] / 'dif-production-art-v002'
MASTER = HERE.parents[1] / 'dif-production-art-v001/dif-neutral-master-candidate.png'
sys.path.insert(0, str(ROOT / 'scripts'))
import dif_assets

NAMES = ['blink', 'compare_glance', 'consider', 'comment_ready', 'attention', 'acknowledge', 'blocked']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical(image):
    data = np.array(image.convert('RGBA'))
    data[data[:, :, 3] == 0] = 0
    return dif_assets.encode_png(dif_assets.RgbaImage(image.width, image.height, data.tobytes()))


def save(image, name):
    (HERE / name).write_bytes(canonical(image))


def anchors(image):
    """Separate head width and lower foot support, never limb envelope center."""
    data = np.array(image)
    ys, xs = np.where(data[:, :, 3] > 128)
    top, bottom = int(ys.min()), int(ys.max()) + 1
    yy, xx = np.mgrid[:image.height, :image.width]
    head = (data[:, :, 3] > 128) & (yy < top + .53 * (bottom - top))
    hx = np.where(head)[1]
    foot = (data[:, :, 3] > 128) & (yy >= bottom - .09 * (bottom - top))
    fx = np.where(foot)[1]
    return {'head_width': int(hx.max() - hx.min() + 1),
            'foot_center_x': float((fx.min() + fx.max() + 1) / 2),
            'foot_bottom_exclusive': bottom,
            'foot_interval': [int(fx.min()), int(fx.max()) + 1]}


def register(crop, scale, target):
    facts = anchors(crop)
    dx = target['foot_center_x'] - scale * facts['foot_center_x']
    dy = target['foot_bottom_exclusive'] - scale * facts['foot_bottom_exclusive']
    result = crop.transform((256, 256), Image.Transform.AFFINE,
                            (1 / scale, 0, -dx / scale, 0, 1 / scale, -dy / scale),
                            Image.Resampling.BICUBIC)
    pixels = np.array(result)
    pixels[pixels[:, :, 3] == 0] = 0
    result = Image.fromarray(pixels)
    return result, {'scale_xy': scale, 'translation_xy': [dx, dy], 'native_anchors': facts}


def changed(a, b, y0=0, y1=256):
    return int(np.any(np.array(a)[y0:y1] != np.array(b)[y0:y1], axis=2).sum())


def main():
    assert sha(MASTER) == 'bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef'
    master = Image.open(MASTER).convert('RGBA')
    reduced = dif_assets.resize_box(dif_assets.RgbaImage(1024, 1024, master.tobytes()), clear_rounded_transparency=True)
    neutral = Image.frombytes('RGBA', (256, 256), reduced.pixels)
    assert canonical(neutral) == (V2 / 'source/rest/frame-000.png').read_bytes()
    target = anchors(neutral)
    inventory = json.loads((V2 / 'crop-inventory.json').read_text())
    assets = {a['clip_id']: a for a in json.loads((PACK / 'assets/INPUT-MANIFEST.json').read_text())['assets']}
    contracts = {a['id']: a for a in json.loads((PACK / 'specs/CLIP-CONTRACT.json').read_text())['clips']}
    report = {'status': 'rejected source studies; preparatory only', 'accepted_master_sha256': sha(MASTER),
              'neutral_pixels_sha256': hashlib.sha256(neutral.tobytes()).hexdigest(),
              'neutral_anchors': target, 'clips': {}}
    all_frames = {}
    for name in NAMES:
        asset, contract = assets[name], contracts[name]
        source = PACK / asset['path']
        assert sha(source) == asset['sha256']
        sheet = Image.open(source).convert('RGBA')
        records = [r for r in inventory if r['clip'] == name]
        crops = [sheet.crop(r['half_open_crop']) for r in records]
        for crop, record in zip(crops, records):
            assert hashlib.sha256(canonical(crop)).hexdigest() == record['sha256']
        scale = target['head_width'] / anchors(crops[0])['head_width']
        registered = [register(c, scale, target) for c in crops]
        frames = [f for f, _ in registered]
        # Retain all raw poses in a separate row; handoff study uses exact neutral.
        raw_row = Image.new('RGBA', (256 * len(frames), 256))
        for i, frame in enumerate(frames):
            raw_row.paste(frame, (i * 256, 0))
        save(raw_row, f'{name}-registered-raw.png')
        for i in contract['neutral_frames']:
            frames[i] = neutral.copy()
        row = Image.new('RGBA', (256 * len(frames), 256))
        for i, frame in enumerate(frames):
            row.paste(frame, (i * 256, 0))
        save(row, f'{name}-handoff-study.png')
        frame_checks = []
        for i, frame in enumerate(frames):
            data = np.array(frame)
            alpha = frame.getchannel('A')
            frame_checks.append({'index': i, 'alpha_bounds': alpha.getbbox(),
                'hip_foot_changed_pixels_vs_neutral_y198_256': changed(frame, neutral, 198),
                'non_eye_changed_pixels_rows130_256': changed(frame, neutral, 130),
                'hidden_rgb_channels': int(np.count_nonzero(data[data[:, :, 3] == 0, :3])),
                'pixels_sha256': hashlib.sha256(Image.open(HERE / f'{name}-handoff-study.png').crop((i*256,0,(i+1)*256,256)).tobytes()).hexdigest()})
        report['clips'][name] = {'state': 'rejected', 'source_path': str(source.relative_to(ROOT)),
            'source_sha256': sha(source), 'frame_count': len(frames), 'duration_ms': contract['frame_duration_ms'],
            'native_crops': records,
            'native_crop_boundary_alpha_counts': [int(np.count_nonzero(np.array(c.getchannel('A'))[0,:])) + int(np.count_nonzero(np.array(c.getchannel('A'))[-1,:])) + int(np.count_nonzero(np.array(c.getchannel('A'))[:,0])) + int(np.count_nonzero(np.array(c.getchannel('A'))[:,-1])) for c in crops], 'transforms': [t for _, t in registered],
            'exact_neutral_indices': contract['neutral_frames'],
            'exact_endpoints': frames[0].tobytes() == frames[-1].tobytes() == neutral.tobytes(),
            'distinct_pixel_frames': len({hashlib.sha256(f.tobytes()).hexdigest() for f in frames}),
            'adjacent_changed_pixels': [changed(a,b) for a,b in zip(frames,frames[1:])],
            'checks': frame_checks}
        assert report['clips'][name]['exact_endpoints']
        assert report['clips'][name]['distinct_pixel_frames'] > 1
        all_frames[name] = frames
    for size in [64,72,96]:
        for theme, color, ink in [('light','#F6F7F9','black'), ('dark','#1C2026','white')]:
            plate = Image.new('RGB', (180+8*size, len(NAMES)*(size+24)), color)
            draw = ImageDraw.Draw(plate)
            for j, name in enumerate(NAMES):
                y = j*(size+24)
                draw.text((5,y+15), name, fill=ink)
                draw.text((5,y+32), 'REJECTED', fill=ink)
                for i, frame in enumerate(all_frames[name]):
                    thumb = frame.resize((size,size), Image.Resampling.LANCZOS)
                    plate.paste(thumb,(180+i*size,y+20),thumb)
                    draw.text((180+i*size,y+3), str(i), fill=ink)
            plate.save(HERE / f'plate-{size}-{theme}.png')
    (HERE / 'checks.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Prepared seven distinct rejected handoff studies and six actual-size theme plates.')


if __name__ == '__main__':
    main()
