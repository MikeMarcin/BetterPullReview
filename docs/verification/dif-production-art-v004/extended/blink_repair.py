"""Bounded offline eye-region extraction study, using the raw blink gestures."""
from pathlib import Path
import json
import hashlib
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
import prepare


def main():
    neutral = Image.open(prepare.V2 / 'source/rest/frame-000.png').convert('RGBA')
    raw = Image.open(HERE / 'blink-registered-raw.png').convert('RGBA')
    # Native source is bound by prepare.py; rectangles here are in its 256px
    # registration space. Do not transplant the mismatched full eye-patch shape.
    regions = [(86,79,113,113,0,-14), (143,79,168,113,-2,-14)]
    mask = Image.new('L',(256,256))
    for x0,y0,x1,y1,dx,dy in regions:
        ImageDraw.Draw(mask).ellipse((x0,y0,x1-1,y1-1),fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1))
    # Hard bounds make all non-eye pixels independently checkable.
    bounds = Image.new('L',(256,256))
    for x0,y0,x1,y1,dx,dy in regions:
        ImageDraw.Draw(bounds).rectangle((x0,y0,x1-1,y1-1),fill=255)
    mask = Image.fromarray(np.minimum(np.array(mask),np.array(bounds)))
    edited=[]
    # One bounded local repair: clone accepted adjacent eye-patch material
    # over the iris, then curve the lid using dark color sampled from raw lids.
    # Raw blink's half/closed trajectory is retained; geometry is authored.
    for closed in [False, True]:
        data=np.array(neutral).copy()
        for cx, donor in [(99,82),(156,173)]:
            for y in range(80,113):
                for x in range(cx-12,cx+13):
                    distance=((x-cx)/11)**2+((y-96)/16)**2
                    if distance>=1: continue
                    edge=94+2*(1-((x-cx)/11)**2)
                    if not closed and y>edge: continue
                    weight=min(1,max(0,(1-distance)*6))
                    # Same-row donor retains the neutral material gradient.
                    data[y,x,:3]=np.rint(data[y,x,:3]*(1-weight)+data[y,donor,:3]*weight)
        frame=Image.fromarray(data)
        # Curved restrained lid, sampled from real raw closed-eye stroke.
        if closed:
            high=frame.resize((1024,1024),Image.Resampling.NEAREST)
            draw=ImageDraw.Draw(high)
            for cx in [99,156]:
                points=[(4*x,4*(99+2*(1-((x-cx)/10)**2))) for x in range(cx-10,cx+11)]
                draw.line(points,fill=(*raw.getpixel((2*256+99,113))[:3],255),width=6)
            frame=high.resize((256,256),Image.Resampling.LANCZOS)
        # Enforce the same hard outside-eye lock after antialiasing.
        data=np.array(frame); n=np.array(neutral); outside=np.array(bounds)==0
        data[outside]=n[outside];data[:,:,3]=n[:,:,3]
        edited.append(Image.fromarray(data))
    frames=[neutral,edited[0],edited[1],edited[0],neutral,neutral]
    row=Image.new('RGBA',(1536,256))
    for i,f in enumerate(frames): row.paste(f,(256*i,0))
    prepare.save(row,'blink-repair-eye-study.png')
    prepare.save(mask.convert('RGBA'),'blink-repair-eye-mask.png')
    checks=[]
    outside=np.array(bounds)==0
    for frame in frames:
        a=np.array(frame); n=np.array(neutral)
        assert np.array_equal(a[outside],n[outside])
        assert np.array_equal(a[:,:,3],n[:,:,3])
        checks.append({'non_eye_changed_pixels':int(np.any(a[outside]!=n[outside],axis=1).sum()),
                       'changed_pixels':int(np.any(a!=n,axis=2).sum()),
                       'rgba_sha256':hashlib.sha256(frame.tobytes()).hexdigest()})
    for size in [64,72,96]:
        plate=Image.new('RGB',(6*size,2*(size+22)))
        for j,(color,ink) in enumerate([('#F6F7F9','black'),('#1C2026','white')]):
            ImageDraw.Draw(plate).rectangle((0,j*(size+22),6*size,(j+1)*(size+22)),fill=color)
            for i,frame in enumerate(frames):
                thumb=frame.resize((size,size),Image.Resampling.LANCZOS)
                plate.paste(thumb,(i*size,j*(size+22)+22),thumb)
                ImageDraw.Draw(plate).text((i*size,j*(size+22)+3),str(i),fill=ink)
        plate.save(HERE/f'blink-repair-{size}-themes.png')
    (HERE/'blink-repair-checks.json').write_text(json.dumps({'state':'provisional repair candidate; faint donor texture bands require integrator inspection',
       'repair':'same-row neutral patch donor, authored curved lid following raw half/closed motion; no native-resolution claim',
       'raw_registration_sha256':prepare.sha(HERE/'blink-registered-raw.png'),
       'regions_half_open_and_source_translation':regions,'source_raw_indices':[None,1,2,1,None,None],
       'duration_ms':100,'alpha_exact_neutral':True,'checks':checks},indent=2)+'\n')
    print('Local blink: exact alpha and non-eye pixels, six 100ms frames.')


if __name__=='__main__': main()
