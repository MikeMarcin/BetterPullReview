"""Offline bounded donor diagnosis; writes exclusively beside this recipe."""
from pathlib import Path
import hashlib
import json
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / 'scripts'))
import dif_assets

NAMES = ['compare-glance-left', 'consider-half-lift', 'comment-ready-open-peak',
         'attention-chest-paw', 'acknowledge-nod-peak', 'blocked-left-paw']
NEUTRAL = ROOT / 'docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png'
EXPECTED = 'bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef'
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def clean(im):
    a = np.array(im)
    a[a[:, :, 3] == 0] = 0
    return Image.fromarray(a)
def cell(im):
    result = dif_assets.resize_box(dif_assets.RgbaImage(1024,1024,im.tobytes()), clear_rounded_transparency=True)
    return Image.frombytes('RGBA',(256,256), result.pixels)

assert sha(NEUTRAL) == EXPECTED
master = Image.open(NEUTRAL).convert('RGBA')
neutral = cell(master)
n = np.array(neutral)
y,x = np.mgrid[:256,:256]
# Conservative exact head lock including transparent fringe and lower chin.
head = (y < 142) | ((y < 149) & (n[:,:,0] > 160) & (n[:,:,1] > 140) & (n[:,:,3] > 0))
regions = [
    [(83,91),(113,91),(113,122),(83,122)],
    [(53,143),(96,143),(106,169),(98,195),(51,195)],
    [(33,130),(217,130),(224,187),(201,199),(52,199),(31,185)],
    [(158,146),(220,146),(226,184),(202,198),(158,198)],
    [(45,28),(208,28),(215,142),(187,149),(70,149),(40,138)],
    [(50,145),(98,145),(108,179),(99,196),(51,196)],
]
frames=[]; facts=[]
for i,name in enumerate(NAMES):
    source = ROOT / 'docs/verification/dif-production-art-v005/generated-inputs' / (name+'.png')
    donor = Image.open(source).convert('RGBA').resize((1024,1024),Image.Resampling.LANCZOS)
    # Full-canvas uniform fit is a diagnostic, not independently established body registration.
    donor = cell(clean(donor))
    mask = Image.new('L',(256,256))
    draw=ImageDraw.Draw(mask); draw.polygon(regions[i],fill=255)
    if i==0:
        draw.rectangle((141,91,170,122),fill=255)
    mask=mask.filter(ImageFilter.GaussianBlur(1))
    allowed=np.array(mask)>0
    if i == 2: allowed &= ~(head & (n[:,:,3] > 0))
    elif i!=4: allowed &= ~head
    if i==0: allowed=np.array(mask)>0
    allowed &= y<198
    # Restore exact pixels outside the finite graft support after blending.
    a=np.array(Image.composite(donor,neutral,mask))
    a[~allowed]=n[~allowed]
    candidate=clean(Image.fromarray(a)); a=np.array(candidate)
    candidate.save(HERE/(name+'-trial.png'))
    Image.fromarray((allowed*255).astype('uint8')).save(HERE/(name+'-allowed-mask.png'))
    assert np.array_equal(a[~allowed],n[~allowed])
    assert np.array_equal(a[198:],n[198:])
    assert not np.any(a[a[:,:,3]==0])
    bounds=candidate.getchannel('A').getbbox()
    assert bounds[3]==223 and min(bounds[:2])>=16 and max(bounds[2:])<=240
    frames.append(candidate)
    facts.append({'id':'E'+str(i+1),'input':str(source.relative_to(ROOT)), 'input_sha256':sha(source),
        'recipe':'uniform native canvas fit to 1024, canonical box reduction, finite feathered local graft; diagnostic only',
        'polygon_256':regions[i], 'extra_eye_rectangle': [141,91,170,122] if i==0 else None,
        'outside_mask_changed_pixels':int(np.any(a[~allowed]!=n[~allowed],axis=1).sum()),
        'feet_hips_changed_pixels':int(np.any(a[198:]!=n[198:],axis=2).sum()),
        'head_changed_pixels':int(np.any(a[head]!=n[head],axis=1).sum()),
        'changed_pixels':int(np.any(a!=n,axis=2).sum()), 'alpha_bbox':bounds,
        'rgba_sha256':hashlib.sha256(candidate.tobytes()).hexdigest(),
        'status':'unaccepted diagnostic; incomplete choreography'})
for size in (64,72,96,256):
    plate=Image.new('RGB',(size*6,6*(size+22)))
    d=ImageDraw.Draw(plate)
    for row,candidate in enumerate(frames):
        for theme,bg in enumerate(('#F6F7F9','#1C2026','#000000')):
            left=theme*size*2; top=row*(size+22)
            d.rectangle((left,top,left+size*2,top+size+22),fill=bg)
            d.text((left+2,top+3),'E'+str(row+1)+' neutral / graft',fill='black' if theme==0 else 'white')
            for col,im in enumerate((neutral,candidate)):
                t=im.resize((size,size),Image.Resampling.LANCZOS)
                plate.paste(t,(left+col*size,top+22),t)
    plate.save(HERE/f'comparison-{size}.png')
report={'neutral_sha256':EXPECTED,'producer':'d8 mandate revision 5', 'frames':facts,
        'actual_speed_observed':False,'human_verdict':None,
        'motion_limit':'Only isolated failed grafts; no interpolated/duplicated poses presented as sequences.',
        'files':{p.name:sha(p) for p in sorted(HERE.glob('*.png'))}}
(HERE/'checks.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: six finite-region graft diagnostics; exact outside-mask RGBA, feet/hips, baseline, clearance and hidden RGB checks.')
