"""Offline blink candidate assembly from pinned neutral and retained edited donor."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
NEUTRAL = ROOT / 'docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png'
DONOR = HERE / 'closed-donor.png'
INPUTS = {NEUTRAL: 'bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef',
          DONOR: '573661259948227f86ce7966ae647696fd9f179df5f443859bbf8456ad0b49a9'}
EYES = [(398, 383), (625, 641)]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=HERE)
OUT = parser.parse_args().output.resolve()
assert OUT == HERE or HERE in OUT.parents, 'Outputs must stay inside this owned study'
OUT.mkdir(parents=True, exist_ok=True)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png(image, path):
    path.write_bytes(codec.encode_png(codec.RgbaImage(image.width, image.height, image.tobytes())))


def box256(image):
    reduced = codec.resize_box(codec.RgbaImage(image.width, image.height, image.tobytes()), 256, clear_rounded_transparency=True)
    return Image.frombytes('RGBA', (256, 256), reduced.pixels)


sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('dif_blink_codec', ROOT / 'scripts/dif_assets.py')
codec = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = codec
spec.loader.exec_module(codec)
for path, digest in INPUTS.items():
    assert sha(path) == digest, f'Input drift: {path}'
neutral = Image.open(NEUTRAL).convert('RGBA')
n = np.array(neutral)
donor = Image.open(DONOR).convert('RGBA').resize((1024, 1024), Image.Resampling.LANCZOS)
closed = n.copy()
half = n.copy()
mask = np.zeros((1024, 1024), dtype=bool)
y, x = np.mgrid[:1024, :1024]
fits = []
for cx, gx in EYES:
    distance = ((x-cx)/49)**2 + ((y-389)/65)**2
    weight = np.clip((1-distance)*8, 0, 1)
    region = distance < 1
    sampled = np.array(donor.transform(donor.size, Image.Transform.AFFINE,
                       (1.25, 0, gx-1.25*cx, 0, 1, 0), Image.Resampling.BICUBIC))[:, :, :3].astype(float)
    # Fit a low-frequency RGB plane at the purple boundary; retain donor texture/crease.
    basis = np.stack([np.ones_like(x), (x-cx)/49, (y-389)/65], axis=2)
    ring = (distance > .85) & (distance < 1.08)
    coeff = np.linalg.lstsq(basis[ring], n[:, :, :3][ring].astype(float)-sampled[ring], rcond=None)[0]
    material = np.clip(sampled + basis @ coeff, 0, 255)
    closed[:, :, :3] = np.rint(closed[:, :, :3]*(1-weight[:, :, None]) + material*weight[:, :, None]).astype('uint8')
    # Occlude the upper half with the same material; lower eyeball remains exactly native.
    lid_y = 382 + 8*np.clip(1-((x-cx)/40)**2, 0, 1)
    half_weight = weight*np.clip((lid_y-y)/1.5, 0, 1)
    half[:, :, :3] = np.rint(half[:, :, :3]*(1-half_weight[:, :, None]) + material*half_weight[:, :, None]).astype('uint8')
    mask |= region
    fits.append({'neutral_center': [cx,389], 'donor_center_x': gx,
                 'donor_x_scale': 1.25, 'rgb_boundary_plane': coeff.tolist()})
frames = [n, half, closed, half, n, n]
(OUT/'native').mkdir(exist_ok=True)
(OUT/'frames').mkdir(exist_ok=True)
checks = []
for i, a in enumerate(frames):
    assert np.array_equal(a[:,:,3],n[:,:,3]), 'Alpha changed'
    assert np.array_equal(a[~mask],n[~mask]), 'Outside-eye pixels changed'
    assert not np.any(a[a[:,:,3]==0,:3]), 'Hidden RGB'
    changed = np.any(a!=n,axis=2)
    yy,xx=np.where(changed)
    f = Image.fromarray(a)
    png(f,OUT/'native'/f'frame-{i:03d}.png')
    normalized = box256(f)
    png(normalized,OUT/'frames'/f'frame-{i:03d}.png')
    checks.append({'frame': i, 'changed_native_pixels':int(changed.sum()),
                   'changed_bbox_exclusive': [int(xx.min()),int(yy.min()),int(xx.max()+1),int(yy.max()+1)] if len(xx) else None,
                   'outside_eye_changes':0, 'alpha_changes':0, 'hidden_rgb':0,
                   'alpha_bbox_256': normalized.getbbox()})
Image.fromarray(mask.astype('uint8')*255).save(OUT/'eye-mask.png')
normalized_frames = [Image.open(OUT/'frames'/f'frame-{i:03d}.png') for i in range(6)]
for size in [64,72,96,256]:
    plate = Image.new('RGB',(size*6,(size+24)*3))
    draw = ImageDraw.Draw(plate)
    for row,bg in enumerate(['#F6F7F9','#1C2026','#000000']):
        draw.rectangle((0,row*(size+24),size*6,(row+1)*(size+24)),fill=bg)
        for i,f in enumerate(normalized_frames):
            thumb=f.resize((size,size),Image.Resampling.LANCZOS)
            plate.paste(thumb,(i*size,row*(size+24)+24),thumb)
            draw.text((i*size+2,row*(size+24)+5),f'{i:02} / {i*100}ms',fill='black' if row==0 else 'white')
    plate.save(OUT/f'themes-{size}.png')
strip=Image.new('RGBA',(8*256,256))
for i,f in enumerate(normalized_frames):
    strip.paste(f,((i+1)*256,0))
png(strip,OUT/'candidate-strip.png')
comparison=Image.new('RGB',(1024,300),'#F6F7F9')
d=ImageDraw.Draw(comparison)
for i,(label,f) in enumerate([('accepted neutral',normalized_frames[0]),('v005 failed closed',Image.open(ROOT/'docs/verification/dif-production-art-v005/blink-study/closed-256-study.png')),('v006 half',normalized_frames[1]),('v006 closed',normalized_frames[2])]):
    comparison.paste(f,(i*256,30),f)
    d.text((i*256+5,8),label,fill='black')
comparison.save(OUT/'comparison.png')
result={'status':'unaccepted_candidate','input_sha256':{str(p.relative_to(ROOT)):h for p,h in INPUTS.items()},
        'recipe_sha256':sha(Path(__file__)), 'compiler_sha256':sha(ROOT/'scripts/dif_assets.py'),
        'timing_ms':[100]*6,'neutral_frames':[0,4,5], 'half_frames':[1,3], 'closed_frames':[2],
        'mask_ellipses':[[cx,389,49,65] for cx,_ in EYES], 'donor_registration':fits,'frames':checks,
        'outputs':{str(p.relative_to(OUT)):sha(p) for p in sorted([*OUT.glob('themes-*.png'),OUT/'eye-mask.png',OUT/'candidate-strip.png',OUT/'comparison.png',*(OUT/'native').glob('*.png'),*(OUT/'frames').glob('*.png')])}}
(OUT/'checks.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'frames':checks,'distinct_poses':len({a.tobytes() for a in frames})},indent=2))
