"""Reproducible offline connected-arm study; writes only beside this recipe."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PRIOR = HERE.parent / "dif-production-art-v003/prepare.py"
assert hashlib.sha256(PRIOR.read_bytes()).hexdigest() == "1984ad73db84b5c8bccba30daa6eca58b6513bfe8a7c365bb5dbf19ad2fe1000"
assert hashlib.sha256((ROOT / "scripts/dif_assets.py").read_bytes()).hexdigest() == "c0361ed24130ff81a8a1b32dab4cca26123ce08e35205304765c3fd09de86abf"
spec = importlib.util.spec_from_file_location("prior_study", PRIOR)
prior = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prior)

INPUTS = {
    "correction": ("../generated-correction/open-paw-intermediate-fullbody.png", "71aed58a2c88fec60e76ca00d314c55cfe65c1dd539ebd42e47b010930fd347f"),
    "raised": ("greet-raised-paw-fullbody.png", "4c95bdec249c92ba1397df740dca1a9cec3eb2941926fc0bc920af1e14077d68"),
    "lower": ("greet-lower-paw-fullbody.png", "691f021099c25aa415f8b2494c141c6ac706c2d2a49ff1d1bedb62a49bc1d944"),
}
POLYGONS = {
    "correction": [(804,710),(858,686),(917,715),(943,696),(952,651),(978,623),(1011,625),(1028,638),(1055,634),(1080,656),(1101,668),(1117,701),(1105,750),(1075,792),(1024,830),(964,856),(902,859),(858,841),(837,799)],
    "raised": [(809,707),(866,681),(895,631),(913,564),(950,548),(1006,548),(1030,572),(1061,578),(1080,608),(1077,668),(1044,720),(1005,763),(952,799),(860,823),(836,779)],
    "lower": [(810,710),(866,688),(905,714),(930,684),(975,681),(1021,711),(1041,755),(1038,812),(1000,851),(961,874),(902,873),(861,848),(840,796)],
}
REGISTRATION = {"source_shoulder": [845,728], "target_shoulder": [690,592], "uniform_scale": .816,
                "outward_reach_by_pose": {"raised": 64, "lower": 28, "correction": 5}}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(name):
    filename, expected = INPUTS[name]
    path = HERE / "generated-inputs" / filename
    assert digest(path) == expected, path
    source = Image.open(path).convert("RGBA")
    assert source.size == (1254,1254)
    mask = Image.new("L",source.size)
    ImageDraw.Draw(mask).polygon(POLYGONS[name],fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(.7))
    data = np.array(source)
    rgb = data[:,:,:3].astype(float)
    material = np.clip((rgb[:,:,2]-rgb[:,:,1]-12)/12,0,1)
    data[:,:,3] = np.rint(data[:,:,3].astype(float)*np.array(mask)/255*material).astype("uint8")
    source = prior.clean(Image.fromarray(data))
    scale = REGISTRATION["uniform_scale"]
    reach = REGISTRATION["outward_reach_by_pose"][name]
    def point(x,y):
        # Continuous local reach change; zero at the shoulder and maximal at paw.
        weight = np.clip((592-y)/100,0,1) if name == "raised" else np.clip((x-690)/120,0,1)
        return (845+(x-690-reach*weight)/scale,728+(y-592)/scale)
    mesh = []
    for top in range(0,1024,8):
        for left in range(600,1024,8):
            right,bottom=left+8,top+8
            mesh.append(((left,top,right,bottom),(*point(left,top),*point(left,bottom),*point(right,bottom),*point(right,top))))
    return prior.clean(source.transform((1024,1024),Image.Transform.MESH,mesh,Image.Resampling.BICUBIC))


def wrist_turn(limb, degrees):
    angle = np.deg2rad(degrees)
    def point(x,y):
        t = np.clip((620-y)/100,0,1)
        theta = angle*t*t*(3-2*t)
        dx,dy=x-839,y-526
        return (839+dx*np.cos(theta)-dy*np.sin(theta),526+dx*np.sin(theta)+dy*np.cos(theta))
    mesh=[]
    for top in range(0,1024,8):
        for left in range(600,1024,8):
            right,bottom=left+8,top+8
            mesh.append(((left,top,right,bottom),(*point(left,top),*point(left,bottom),*point(right,bottom),*point(right,top))))
    return prior.clean(limb.transform((1024,1024),Image.Transform.MESH,mesh,Image.Resampling.BICUBIC))


def articulate_raise(limb):
    """Rotate the correction arm 20 degrees, then shift it 16 native pixels right."""
    rotated=limb.rotate(20,Image.Resampling.BICUBIC,center=(690,592))
    return prior.clean(rotated.transform((1024,1024),Image.Transform.AFFINE,(1,0,-16,0,1,0),Image.Resampling.BICUBIC))


def build_frames():
    assert digest(prior.MASTER) == prior.EXPECTED_MASTER
    master=Image.open(prior.MASTER).convert("RGBA")
    accepted=Image.open(HERE.parent / "dif-production-art-v001/dif-neutral-master-candidate.png").convert("RGBA")
    assert master.tobytes() == accepted.tobytes()
    belly=prior.reconstruct_belly(master)
    lower=extract("correction")
    raised=articulate_raise(lower)
    moving=[prior.neutral_arm(master,30),lower,raised,wrist_turn(raised,12),lower.rotate(8,Image.Resampling.BICUBIC,center=(690,592)),prior.neutral_arm(master,25)]
    large=[master]+[prior.protect_head(Image.alpha_composite(belly,limb),master) for limb in moving]+[master]
    return [prior.cell(prior.clean(frame)) for frame in large]


def measure_frames(frames):
    assert len(frames) == 8
    neutral=np.array(frames[0])
    assert frames[0].tobytes()==Image.open(prior.MASTER.parent / "rest/frame-000.png").convert("RGBA").tobytes()
    y,x=np.mgrid[:256,:256]
    head=(y<142)&(neutral[:,:,3]==255)
    entire_head=(y<142)&(neutral[:,:,3]>0)
    chin=(y>=142)&(y<148)&(neutral[:,:,0]>160)&(neutral[:,:,1]>140)&(neutral[:,:,3]>0)
    roi=(x>=150)&(y>=100)&(y<194)&((y>=142)|(x>198))
    metrics=[]
    for i,frame in enumerate(frames):
        data=np.array(frame)
        assert np.array_equal(data[198:],neutral[198:])
        assert np.array_equal(data[:100],neutral[:100])
        assert np.array_equal(data[:,:150],neutral[:,:150])
        assert np.array_equal(data[head],neutral[head])
        assert not np.any(data[data[:,:,3]==0])
        bounds=frame.getchannel("A").getbbox()
        assert bounds[3]==223
        assert min(bounds[:2])>=16 and max(bounds[2:])<=240, bounds
        rgb=data[:,:,:3].astype(int)
        limb=roi&(data[:,:,3]>=128)&(rgb[:,:,2]-rgb[:,:,1]>25)
        pads=limb&(rgb[:,:,0]>140)&(rgb[:,:,0]-rgb[:,:,1]>25)&(rgb[:,:,0]>=rgb[:,:,2]-30)
        ly,lx=np.nonzero(limb)
        next_data=np.array(frames[(i+1)%8])
        changed=np.any(data!=next_data,axis=2)
        motion=np.any(data!=neutral,axis=2)|np.any(next_data!=neutral,axis=2)
        alpha=data[:,:,3:4]/255
        next_alpha=next_data[:,:,3:4]/255
        bg=np.array([246,247,249])
        difference=np.max(np.abs(data[:,:,:3]*alpha+bg*(1-alpha)-next_data[:,:,:3]*next_alpha-bg*(1-next_alpha)),axis=2)
        metrics.append({"frame":i,"rgba_sha256":hashlib.sha256(frame.tobytes()).hexdigest(),"bounds":bounds,
            "visible_limb_area_px":int(limb.sum()),"visible_limb_centroid":[round(float(lx.mean()),2),round(float(ly.mean()),2)],
            "pink_material_proxy_area_px":int(pads.sum()),
            "all_nonzero_alpha_head_changed_pixels":int(np.any(data[entire_head]!=neutral[entire_head],axis=1).sum()),
            "pale_lower_chin_changed_pixels":int(np.any(data[chin]!=neutral[chin],axis=1).sum()),
            "visible_limb_bounds":[int(lx.min()),int(ly.min()),int(lx.max()+1),int(ly.max()+1)],
            "adjacent_changed_pixels":int(changed.sum()),"adjacent_motion_roi_pixels":int(motion.sum()),
            "adjacent_motion_roi_max_channel_mean":round(float(difference[motion].mean()),2) if motion.any() else 0,
            "adjacent_pixels_over_24_channel_delta":int((difference>24).sum())})
    assert frames[0].tobytes()==frames[7].tobytes()
    assert len({f.tobytes() for f in frames[1:7]})==6
    for i,m in enumerate(metrics[:-1]):
        m["next_visible_limb_area_ratio"]=round(metrics[i+1]["visible_limb_area_px"]/m["visible_limb_area_px"],3)
    return metrics


def main():
    for filename,expected in INPUTS.values():
        assert digest(HERE / "generated-inputs" / filename) == expected
    frames=build_frames()
    metrics=measure_frames(frames)
    for size in (64,72,96,256):
        prior.plate(frames,size).save(HERE / f"greet-{size}-light-dark.png")
    report={"status":"rejected study: sharp raised shoulder junction and closed-to-open paw substitution remain; no human verdict",
        "inputs":{name:{"file":v[0],"sha256":v[1]} for name,v in INPUTS.items()},
        "master_sha256":digest(prior.MASTER),"reused_recipe_sha256":digest(PRIOR),
        "compiler_sha256":digest(ROOT / "scripts/dif_assets.py"),
        "registration":REGISTRATION,"extraction_polygons_native":POLYGONS,
        "correction_prompt_sha256":digest(HERE / "generated-correction/prompt.txt"),
        "final_limb_pixel_sources":["accepted neutral", "generated correction"],
        "frame_duration_ms":125,"frame_recipe":["exact neutral","neutral arm +30 degrees","generated correction","correction arm +20 degrees then +16 native pixels right","raised with local +12-degree wrist turn","correction arm +8 degrees","neutral arm +25 degrees","exact neutral"],
        "unchanged_regions":{"feet_hips":[0,198,256,256],"upper_head":[0,0,256,100],"opposite_body":[0,0,150,256],"opaque_head":"neutral alpha=255 pixels above row 142"},
        "actual_speed_observed":False,"human_verdict":None,"frames":metrics}
    (HERE / "checks.json").write_text(json.dumps(report,indent=2)+"\n")
    print("PASS: exact neutral endpoints; fixed head/opposite body/hips/feet; clean alpha, baseline and clearance; eight frames, six distinct moving poses.")
    print("Limb areas:",[m["visible_limb_area_px"] for m in metrics])
    print("Area ratios:",[m.get("next_visible_limb_area_ratio") for m in metrics[:-1]])


if __name__=="__main__":
    main()
