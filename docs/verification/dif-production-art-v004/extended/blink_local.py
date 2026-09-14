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
    for index in [1,2]:
        source = raw.crop((index*256,0,(index+1)*256,256))
        transfer = neutral.copy()
        for x0,y0,x1,y1,dx,dy in regions:
            transfer.paste(source.crop((x0-dx,y0-dy,x1-dx,y1-dy)),(x0,y0))
        frame = Image.composite(transfer,neutral,mask)
        frame.putalpha(neutral.getchannel('A'))
        edited.append(frame)
    frames=[neutral,edited[0],edited[1],edited[0],neutral,neutral]
    row=Image.new('RGBA',(1536,256))
    for i,f in enumerate(frames): row.paste(f,(256*i,0))
    prepare.save(row,'blink-local-eye-study.png')
    prepare.save(mask.convert('RGBA'),'blink-local-eye-mask.png')
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
        plate.save(HERE/f'blink-local-{size}-themes.png')
    (HERE/'blink-local-checks.json').write_text(json.dumps({'state':'rejected: pale lower-eye sliver and lid/material seam in closed frame',
       'raw_registration_sha256':prepare.sha(HERE/'blink-registered-raw.png'),
       'regions_half_open_and_source_translation':regions,'source_raw_indices':[None,1,2,1,None,None],
       'duration_ms':100,'alpha_exact_neutral':True,'checks':checks},indent=2)+'\n')
    print('Local blink: exact alpha and non-eye pixels, six 100ms frames.')


if __name__=='__main__': main()
