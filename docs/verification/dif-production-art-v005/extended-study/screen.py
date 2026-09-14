"""Read-only input screen; writes only this offline study directory."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw
import numpy as np
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
N=ROOT/'docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png'
NAMES=['compare-glance-left','consider-half-lift','comment-ready-open-peak','attention-chest-paw','acknowledge-nod-peak','blocked-left-paw']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(N)=='bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef'
neutral=Image.open(N).convert('RGBA')
frames=[neutral]; data=[]
for name in NAMES:
 p=HERE.parent/'generated-inputs'/f'{name}.png'; original=Image.open(p); im=original.convert('RGBA'); a=np.array(im)
 normalized=im.resize((1024,1024),Image.Resampling.LANCZOS); b=np.array(normalized); n=np.array(neutral)
 data.append({'name':name,'sha256':sha(p),'native_size':im.size,'mode':original.mode,'alpha_bbox':im.getbbox(),'transparent_pixels':int((a[:,:,3]==0).sum()),'partial_alpha_pixels':int(((a[:,:,3]>0)&(a[:,:,3]<255)).sum()),'hidden_rgb_pixels':int(((a[:,:,3]==0)&np.any(a[:,:,:3]!=0,axis=2)).sum()),'screen_resize':'uniform full-canvas LANCZOS to 1024; not body registration','resized_alpha_bbox':normalized.getbbox(),'resized_changed_pixels_vs_neutral':int(np.any(b!=n,axis=2).sum()),'resized_changed_alpha_pixels':int((b[:,:,3]!=n[:,:,3]).sum()),'resized_foot_region_changed_pixels_y780_plus':int(np.any(b[780:]!=n[780:],axis=2).sum())})
 frames.append(im)
frames[1].crop((340,320,900,580)).save(HERE/'compare-eye-detail.png')
for size in [64,72,96,256]:
 # Each row has neutral plus one candidate on each theme.
 plate=Image.new('RGB',(4*size,6*(size+24)))
 draw=ImageDraw.Draw(plate)
 for j,name in enumerate(NAMES):
  for k,bg in enumerate(['#F6F7F9','#1C2026']):
   draw.rectangle((k*2*size,j*(size+24),(k+1)*2*size,(j+1)*(size+24)),fill=bg)
   draw.text((k*2*size+2,j*(size+24)+3),name,fill='black' if k==0 else 'white')
   for i,im in enumerate([neutral,frames[j+1]]):
    t=im.resize((size,size),Image.Resampling.LANCZOS)
    plate.paste(t,((k*2+i)*size,j*(size+24)+24),t)
 plate.save(HERE/f'comparison-{size}.png')
(HERE/'checks.json').write_text(json.dumps({'neutral_sha256':sha(N),'neutral_alpha_bbox':neutral.getbbox(),'inputs':data,'plates':{p.name:sha(p) for p in sorted(HERE.glob('*.png'))}},indent=2)+'\n')
print(json.dumps(data,indent=2))
