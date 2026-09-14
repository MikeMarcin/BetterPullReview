"""Bounded raw-paw retouch study; never writes production source frames."""
from pathlib import Path
import hashlib
import json
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
STUDY = HERE.parent
ROOT = STUDY.parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import dif_assets

RAW_HASH = '1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58'
MASTER_HASH = 'e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf'
# Half-open row-major rectangles from immutable intake; polygons are local to crop.
PARTS = [
    ((887,0,1331,444), [(309,247),(330,240),(355,245),(376,266),(380,299),(361,329),(337,343),(313,341),(300,326),(300,303),(312,281)]),
    ((1331,0,1774,444), [(305,285),(324,258),(339,237),(362,233),(383,242),(405,263),(399,290),(386,320),(359,340),(329,344),(306,335)]),
    ((0,444,444,887), [(299,269),(309,247),(331,220),(356,214),(376,231),(379,258),(367,288),(350,310),(323,326),(303,319),(291,298)]),
    ((444,444,887,887), [(306,249),(326,239),(349,242),(371,256),(382,276),(376,303),(355,326),(329,333),(309,322),(298,301)]),
]


def canonical(image):
    a=np.array(image.convert('RGBA'));a[a[:,:,3]==0]=0
    return Image.fromarray(a)


def base_without_arm(master):
    # Native 256 edit: retain head, opposite arm, hips and feet. Reconstruct only
    # the formerly occluded right torso with mirrored adjacent belly texture.
    a=np.array(master);out=a.copy()
    for y in range(143,198):
        edge = 179 + 5*np.sin((y-143)/55*np.pi)
        for x in range(158,212):
            blend=min(1,max(0,(x-158)/7))
            blend*=min(1,(y-142)/3,(198-y)/3)
            sample=a[y, max(125,157-round((x-158)*.65))].astype(float)
            sample[:3]*=1-.08*max(0,min(1,(x-170)/15))
            sample[3]*=max(0,min(1,(edge-x)/1.3))
            out[y,x]=np.rint(a[y,x]*(1-blend)+sample*blend)
    return canonical(Image.fromarray(out))


def part(sheet, rect, points):
    crop=sheet.crop(rect)
    mask=Image.new('L',crop.size);ImageDraw.Draw(mask).polygon(points,fill=255)
    # One-pixel feather only at the explicitly selected boundary; native alpha retained.
    mask=mask.filter(ImageFilter.GaussianBlur(.65))
    data=np.array(crop);data[:,:,3]=np.rint(data[:,:,3].astype(float)*np.array(mask)/255).astype('uint8')
    data[data[:,:,3]==0]=0
    isolated=Image.fromarray(data)
    # Uniform native-to-runtime reduction, no claim to new native detail.
    scaled=isolated.resize((round(crop.width*.54),round(crop.height*.54)),Image.Resampling.LANCZOS)
    layer=Image.new('RGBA',(256,256));layer.alpha_composite(scaled,(8,-12))
    return canonical(layer)


def main():
    raw=ROOT/'docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/dif-greet.png'
    master_path=STUDY/'source/dif-neutral-master.png'
    assert hashlib.sha256(raw.read_bytes()).hexdigest()==RAW_HASH
    assert hashlib.sha256(master_path.read_bytes()).hexdigest()==MASTER_HASH
    master=Image.open(STUDY/'source/rest/frame-000.png').convert('RGBA')
    sheet=Image.open(raw).convert('RGBA');base=base_without_arm(master)
    layers=[part(sheet,*p) for p in PARTS]
    raised=[Image.alpha_composite(base,l) for l in layers]
    # Adjacent approach/recovery are local blends, explicitly an experiment;
    # expose any double paw rather than disguising this as clean articulation.
    frames=[master,Image.blend(master,raised[0],.45),raised[0],raised[1],raised[2],raised[3],Image.blend(master,raised[3],.35),master]
    frames=[canonical(f) for f in frames]
    out=Image.new('RGB',(8*112,2*126),'#F6F7F9');d=ImageDraw.Draw(out)
    for row,(bg,fg) in enumerate([('#F6F7F9','#1F2328'),('#1C2026','#E6EDF3')]):
        d.rectangle((0,row*126,896,(row+1)*126),fill=bg)
        for i,f in enumerate(frames):
            im=f.resize((96,96),Image.Resampling.LANCZOS)
            out.paste(im,(i*112+8,row*126+23),im);d.text((i*112+8,row*126+5),f'frame {i}',fill=fg)
    out.save(HERE/'rejected-greet-96-light-dark.png')
    metrics=[];neutral=np.array(master)
    for i,f in enumerate(frames):
        a=np.array(f);box=f.getchannel('A').getbbox()
        assert np.array_equal(a[198:],neutral[198:])
        assert np.array_equal(a[:100],neutral[:100])
        assert np.array_equal(a[:,:150],neutral[:,:150])
        assert not np.any(a[a[:,:,3]==0])
        assert box[3]==223 and min(box[:2])>=16 and max(box[2:])<=240
        metrics.append({'frame':i,'rgba_sha256':hashlib.sha256(f.tobytes()).hexdigest(),'bbox':box,'next_changed_pixels':int(np.any(a!=np.array(frames[(i+1)%8]),axis=2).sum())})
    assert frames[0].tobytes()==frames[7].tobytes()==master.tobytes()
    (HERE/'checks.json').write_text(json.dumps({'status':'rejected by agent inspection: shoulder/belly seam, narrow forearm and double-paw entry/return; no human verdict','raw_sha256':RAW_HASH,'canonical_master_sha256':MASTER_HASH,'rectangles_and_polygons':PARTS,'uniform_scale':.54,'offset':[8,-12],'frames':metrics,'unchanged_regions':{'feet_hips':[0,198,256,256],'upper_head':[0,0,256,100],'opposite_body':[0,0,150,256]},'frame_duration_ms':125},indent=2)+'\n')
    print('PASS: eight study frames; exact neutral endpoints; hips/feet/opposite body locked; alpha and clearance checks. No production frames written.')

if __name__=='__main__':
    main()
