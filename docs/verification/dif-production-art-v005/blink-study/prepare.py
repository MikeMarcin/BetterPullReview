"""Offline bounded eye extraction; no runtime or production output."""
from pathlib import Path
import hashlib,json
import numpy as np
from PIL import Image,ImageDraw
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
paths={'neutral':ROOT/'docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png','generated':HERE.parent/'generated-inputs/blink-closed-lids.png','v004':ROOT/'docs/verification/dif-production-art-v004/extended/blink-repair-eye-study.png'}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(paths['neutral'])=='bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef'
assert sha(paths['generated'])=='fb6764b7ab9df2c087ca909b5e2c748a70245591c17cb2cbe81e2fabd516174e'
n=Image.open(paths['neutral']).convert('RGBA'); a=np.array(n)
g=Image.open(paths['generated']).convert('RGBA').resize((1024,1024),Image.Resampling.LANCZOS)
out=a.copy(); mask=np.zeros((1024,1024),dtype=np.uint8)
y,x=np.mgrid[:1024,:1024]
for cx,dx in [(398,20),(625,3)]:
    d=((x-cx)/49)**2+((y-389)/65)**2
    weight=np.clip((1-d)*8,0,1)
    donor=np.array(g.transform(g.size,Image.Transform.AFFINE,(1,0,-dx,0,1,10),Image.Resampling.BICUBIC))
    out[:,:,:3]=np.rint(out[:,:,:3]*(1-weight[:,:,None])+donor[:,:,:3]*weight[:,:,None]).astype('uint8')
    mask[weight>0]=255
assert np.array_equal(out[:,:,3],a[:,:,3])
assert np.array_equal(out[mask==0],a[mask==0])
Image.fromarray(out).save(HERE/'closed-native-study.png')
Image.fromarray(mask).save(HERE/'strict-eye-mask.png')
# Fixed-box 4x reduction for opaque eyes; Pillow alpha-weighted BOX for neutral edges.
neutral=n.resize((256,256),Image.Resampling.BOX)
closed=Image.fromarray(out).resize((256,256),Image.Resampling.BOX)
old=Image.open(paths['v004']).convert('RGBA').crop((512,0,768,256))
for size in [64,72,96,256]:
    plate=Image.new('RGB',(size*3,(size+22)*3))
    draw=ImageDraw.Draw(plate)
    for j,bg in enumerate(['#F6F7F9','#1C2026','#000000']):
        draw.rectangle((0,j*(size+22),size*3,(j+1)*(size+22)),fill=bg)
        for i,(label,f) in enumerate([('neutral',neutral),('v004',old),('v005',closed)]):
            thumb=f.resize((size,size),Image.Resampling.LANCZOS)
            plate.paste(thumb,(i*size,j*(size+22)+22),thumb)
            draw.text((i*size+2,j*(size+22)+3),label,fill='black' if j==0 else 'white')
    plate.save(HERE/f'comparison-{size}.png')
closed.save(HERE/'closed-256-study.png')
changed=np.any(a!=out,axis=2); yy,xx=np.where(changed)
checks={'inputs':{k:{'path':str(p.relative_to(ROOT)),'sha256':sha(p),'size':Image.open(p).size} for k,p in paths.items()},'registration':{'generated_resize':[1024,1024],'eye_donor_translation':[[20,-10],[3,-10]],'mask_ellipses':[[398,389,49,65],[625,389,49,65]],'edge_weight':'clip((1-elliptical_distance_squared)*8,0,1)'},'changed_pixels':int(changed.sum()),'changed_bbox_exclusive':[int(xx.min()),int(yy.min()),int(xx.max()+1),int(yy.max()+1)],'non_eye_changed_pixels':0,'alpha_changed_pixels':0,'neutral_alpha_bbox':n.getbbox(),'hidden_rgb_pixels':int(((out[:,:,3]==0)&np.any(out[:,:,:3]!=0,axis=2)).sum()),'outputs':{p.name:sha(p) for p in sorted(HERE.glob('*.png'))}}
(HERE/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(checks,indent=2))
