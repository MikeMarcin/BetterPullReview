"""Focused objective checks and offline theme/motion review media."""
from pathlib import Path
import hashlib
import json
import sys
import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import dif_assets


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def frames(name):
    return [Image.open(path).convert('RGBA') for path in sorted((HERE / 'source' / name).glob('frame-*.png'))]


def main():
    neutral = frames('rest')[0]
    idle = frames('calm_idle')
    assert len(idle) == 12
    original = np.array(neutral)
    assert np.array_equal(original, np.array(idle[0]))
    assert not np.array_equal(original, np.array(idle[-1]))
    measurements = []
    for i, frame in enumerate(idle):
        pixels = np.array(frame)
        assert frame.size == (256, 256)
        assert not np.any(pixels[pixels[:, :, 3] == 0])
        assert np.array_equal(pixels[:145], original[:145]), 'head/eyes moved'
        assert np.array_equal(pixels[198:], original[198:]), 'hips/feet moved'
        box = frame.getchannel('A').getbbox()
        assert box[3] == 223 and min(box[:2]) >= 16 and max(box[2:]) <= 240
        difference = np.any(pixels != original, axis=2)
        ys, xs = np.where(difference)
        next_pixels = np.array(idle[(i + 1) % len(idle)])
        measurements.append({'frame': i, 'alpha_bbox': box, 'changed_pixels_from_neutral': int(difference.sum()), 'change_bbox': [int(xs.min()), int(ys.min()), int(xs.max()+1), int(ys.max()+1)] if len(xs) else None, 'next_frame_changed_pixels': int(np.any(pixels != next_pixels, axis=2).sum())})
    # Rest must be the compiler's exact fixed-box reduction of the accepted master.
    master_image = Image.open(HERE / 'source/dif-neutral-master.png').convert('RGBA')
    master = dif_assets.RgbaImage(1024, 1024, master_image.tobytes())
    accepted = Image.open(HERE.parent / 'dif-production-art-v001/dif-neutral-master-candidate.png').convert('RGBA')
    assert accepted.tobytes() == master.pixels
    assert dif_assets.decode_png(HERE / 'source/dif-neutral-master.png', 'canonical master').pixels == master.pixels
    reduced = np.array(Image.frombytes('RGBA', (256, 256), dif_assets.resize_box(master).pixels))
    hidden_channels = int(np.count_nonzero(reduced[reduced[:, :, 3] == 0]))
    reduced[reduced[:, :, 3] == 0] = 0
    assert reduced.tobytes() == neutral.tobytes()
    assert dif_assets.resize_box(master, clear_rounded_transparency=True).pixels == neutral.tobytes()
    surfaces = [('Light', '#F6F7F9', '#1F2328'), ('Dark', '#1C2026', '#E6EDF3'), ('High contrast', '#000000', '#FFFFFF')]
    plates = []
    for frame in idle:
        plate = Image.new('RGBA', (650, 378), '#F6F7F9')
        draw = ImageDraw.Draw(plate)
        for row, (name, background, foreground) in enumerate(surfaces):
            top = row * 126
            draw.rectangle((0, top, 650, top+126), fill=background)
            draw.text((8, top+8), name, fill=foreground)
            for x, size in [(140, 64), (300, 72), (470, 96)]:
                draw.text((x, top+6), f'{size}px', fill=foreground)
                plate.alpha_composite(frame.resize((size, size), Image.Resampling.LANCZOS), (x, top+25))
        plates.append(dif_assets.RgbaImage(plate.width, plate.height, plate.tobytes()))
    (HERE / 'preview/calm-idle-themes.png').write_bytes(dif_assets.encode_apng(plates, 250, 0))
    (HERE / 'preview/rest-themes.png').write_bytes(dif_assets.encode_png(plates[0]))
    page = '''<!doctype html><meta charset="utf-8"><title>Dif v002 offline studies</title>
<style>body{font:16px system-ui;margin:24px;background:#eee;color:#222}img{max-width:none}button{font:inherit}</style>
<h1>Dif v002: rest and local breath study</h1><p>Candidate only. No acceptance controls. Native theme colors; 64/72/96 CSS pixels at 100% browser zoom. Idle is 12 frames at 250 ms, repeating. Rest remains static.</p>
<button id="toggle">Show static rest</button><p><img id="motion" src="calm-idle-themes.png" width="650" height="378"></p>
<p>Unchanged master head, eyes, hips and feet. No embedded blink. Greeting below is a rejected articulation experiment, not production art.</p><img src="contact.png" width="1200" height="372">
<h2>Extended raw cells: diagnostic only</h2><img src="extended-raw-contact.png" width="1024" height="1036">
<script>let still=false;document.querySelector('#toggle').onclick=()=>{still=!still;document.querySelector('#motion').src=still?'rest-themes.png':'calm-idle-themes.png';document.querySelector('#toggle').textContent=still?'Play local breath':'Show static rest';};</script>'''
    (HERE / 'preview/index.html').write_text(page)
    report = {'status': 'objective preparation only; no final catalog or human motion verdict', 'canonical_master_sha256': sha(HERE/'source/dif-neutral-master.png'), 'accepted_master_sha256': sha(HERE.parent/'dif-production-art-v001/dif-neutral-master-candidate.png'), 'master_decoded_rgba_equal': True, 'timing_ms': 250, 'head_mask': [0,0,256,145], 'hips_feet_mask': [0,198,256,256], 'idle': measurements, 'rest_matches_compiler_box_after_hidden_rgb_clear': True, 'legacy_box_result_hidden_rgb_channels_cleared': hidden_channels, 'matches_integrated_schema2_box': True, 'alpha_zero_rgb': True, 'theme_source': 'src/adapters/qt/src/appearance_profile.cpp application_surface builtins', 'greet': 'rejected study; joint seam and insufficient wave; not in source inventory'}
    (HERE / 'checks.json').write_text(json.dumps(report, indent=2)+'\n')
    paths = sorted(p for p in HERE.rglob('*') if p.is_file() and p.name != 'SHA256SUMS')
    (HERE / 'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.relative_to(HERE)}\n' for p in paths))
    print('PASS: exact master reduction, 12-frame idle, distinct seam, head/eyes/feet invariant, alpha/baseline/clearance checks.')


if __name__ == '__main__':
    main()
