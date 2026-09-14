"""Offline greeting anatomy study. Writes only this owned staging directory."""
from pathlib import Path
import hashlib
import json
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import dif_assets

MASTER = HERE.parent / "dif-production-art-v002/source/dif-neutral-master.png"
RAW = ROOT / "docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/dif-greet.png"
EXPECTED_MASTER = "e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf"
EXPECTED_RAW = "1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58"

# Half-open native crops and limb-only silhouettes, in native crop coordinates.
# Registration is local to each limb, never a whole-body rescale.
POSES = [
    {"crop": [1331, 0, 1774, 444], "mask": [(290,298),(316,278),(329,245),(350,233),(374,230),(394,246),(409,267),(400,293),(382,322),(354,341),(306,348),(292,332)], "anchor": [295,303], "target": [174,149], "scale": .50, "root_widen": .08},
    {"crop": [0, 444, 444, 887], "mask": [(284,283),(306,264),(312,237),(331,217),(350,209),(378,224),(385,247),(375,280),(353,305),(321,321),(295,324),(285,304)], "anchor": [291,289], "target": [175,152], "scale": .53, "root_widen": .08},
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def clean(image):
    data = np.array(image.convert("RGBA"))
    data[data[:, :, 3] == 0] = 0
    return Image.fromarray(data)


def cell(image):
    rgba = dif_assets.resize_box(dif_assets.RgbaImage(1024, 1024, image.tobytes()), clear_rounded_transparency=True)
    return Image.frombytes("RGBA", (256, 256), rgba.pixels)


def reconstruct_belly(master):
    """Clone known belly texture under the old glove; author a shaded side contour."""
    original = np.array(master)
    result = original.copy()
    for y in range(560, 792):
        # Side contour and shading are authored geometry, not recovered anatomy.
        edge = np.interp(y, [560, 620, 700, 760, 792], [719, 743, 752, 744, 716])
        for x in range(600, 824):
            source_x = round(599 - (x - 600) * .50)
            sample = original[y, source_x].astype(float)
            sample[:3] *= 1 - .13 * np.clip((x - 655) / 100, 0, 1) ** 1.5
            sample[3] *= np.clip((edge - x) / 3, 0, 1)
            blend = np.clip((x - 600) / 28, 0, 1)
            blend *= np.clip((y - 560) / 8, 0, 1)
            blend *= np.clip((792 - y) / 9, 0, 1)
            result[y, x] = np.rint(original[y, x] * (1 - blend) + sample * blend)
    return clean(Image.fromarray(result))


def shoulder(master):
    """Authored rounded upper-arm geometry sampled from the neutral purple arm."""
    source = np.array(master)
    y, x = np.mgrid[:1024,:1024] / 4
    t = np.clip((y - 145) / 18, 0, 1)
    center = 176 + 3*t
    radius = 11 + 4*np.sin(t*np.pi)
    # Rounded geometry forms a continuous surface beneath the raw root mask.
    distance = np.abs(x-center) / radius
    end = np.clip((145-y)/9,0,None) + np.clip((y-163)/7,0,None)
    coverage = np.clip((1 - np.sqrt(distance**2 + end**2))*4,0,1)
    sx = np.clip(np.rint(743+(x-center)/radius*27),0,1023).astype(int)
    sy = np.clip(np.rint(612+t*82),0,1023).astype(int)
    data = source[sy,sx].copy()
    data[:,:,:3] = np.rint(data[:,:,:3] * (.77-.13*t[:,:,None])).astype('uint8')
    data[:,:,3] = np.rint(255*coverage).astype('uint8')
    return clean(Image.fromarray(data))


def neutral_arm(master, angle):
    mask = Image.new("L", master.size)
    ImageDraw.Draw(mask).polygon([(676,568),(722,561),(762,591),(790,639),(804,695),(796,736),(762,776),(716,779),(675,764),(644,728),(627,698),(635,657),(665,634)], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.2))
    data = np.array(master)
    rgb = data[:,:,:3].astype(float)
    material = np.clip((rgb[:,:,2]-rgb[:,:,1]-12)/12,0,1)
    data[:,:,3] = np.rint(data[:,:,3]*(np.array(mask)/255)*material).astype('uint8')
    return clean(Image.fromarray(data).rotate(angle, Image.Resampling.BICUBIC, center=(710,596)))


def protect_head(frame, master):
    data = np.array(frame)
    original = np.array(master)
    y, x = np.mgrid[:1024,:1024]
    head = (y < 568) | ((y < 592) & (original[:,:,0] > 160) & (original[:,:,1] > 140))
    head &= original[:,:,3] > 0
    # Original straight-alpha head is an occluding layer over the moving arm.
    coverage = original[:,:,3:4].astype(float)/255
    rgb = np.rint(original[:,:,:3]*coverage + data[:,:,:3]*(1-coverage)).astype('uint8')
    data[:,:,:3][head] = rgb[head]
    data[:,:,3][head] = np.maximum(data[:,:,3],original[:,:,3])[head]
    return clean(Image.fromarray(data))


def raw_limb(sheet, pose):
    crop = sheet.crop(pose["crop"])
    mask = Image.new("L", crop.size)
    ImageDraw.Draw(mask).polygon(pose["mask"], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(.5))
    data = np.array(crop)
    rgb = data[:, :, :3].astype(float)
    # Remove pale head/belly intrusions inside hand-authored limb masks only.
    purple = np.clip((rgb[:,:,2] - rgb[:,:,1] - 12) / 12, 0, 1)
    if "root_widen" in pose:
        y, x = np.mgrid[:crop.height,:crop.width]
        blend = np.clip((x-286)/30,0,1)
        purple *= blend*blend*(3-2*blend)
    data[:,:,3] = np.rint(data[:,:,3] * (np.array(mask) / 255) * purple).astype("uint8")
    data[data[:,:,3] == 0] = 0
    scale = pose["scale"] * 4
    ax, ay = pose["anchor"]
    tx, ty = [v * 4 for v in pose["target"]]
    def point(x, y):
        row = y/4
        weight = np.clip((row-132)/18,0,1) * np.clip((183-row)/15,0,1)
        x = 940 + (x-940)/(1+pose.get("root_widen",0)*weight)
        return (ax+(x-tx)/scale, ay+(y-ty)/scale)
    mesh = []
    for top in range(0,1024,8):
        bottom = top+8
        quad = (*point(0,top), *point(0,bottom), *point(1024,bottom), *point(1024,top))
        mesh.append(((0,top,1024,bottom),quad))
    return clean(Image.fromarray(data).transform((1024,1024), Image.Transform.MESH, mesh, Image.Resampling.BICUBIC))


def plate(frames, size):
    width = (size + 16) * len(frames)
    height = (size + 30) * 2
    result = Image.new("RGB", (width,height))
    draw = ImageDraw.Draw(result)
    for row, (background, foreground) in enumerate([("#F6F7F9", "#1F2328"), ("#1C2026", "#E6EDF3")]):
        y = row * (size + 30)
        draw.rectangle((0,y,width,y+size+30), fill=background)
        for i, frame in enumerate(frames):
            x = i * (size + 16) + 8
            draw.text((x,y+4), f"{i} / {size}px", fill=foreground)
            reduced = frame.resize((size,size), Image.Resampling.LANCZOS)
            result.paste(reduced, (x,y+23), reduced)
    return result


def build_frames():
    assert digest(MASTER) == EXPECTED_MASTER
    assert digest(RAW) == EXPECTED_RAW
    master = Image.open(MASTER).convert("RGBA")
    sheet = Image.open(RAW).convert("RGBA")
    belly = reconstruct_belly(master)
    base = Image.alpha_composite(belly, shoulder(master))
    moving = [neutral_arm(master, 45), neutral_arm(master, 110), raw_limb(sheet, POSES[0]), raw_limb(sheet, POSES[1]), neutral_arm(master, 105), neutral_arm(master, 40)]
    large = [master] + [protect_head(Image.alpha_composite(base if i in (2,3) else belly, limb), master) for i,limb in enumerate(moving)] + [master]
    frames = [cell(clean(frame)) for frame in large]
    return frames


def main():
    frames = build_frames()
    for size in (64,72,96,256):
        plate(frames,size).save(HERE / f"greet-{size}-light-dark.png")
    metrics = []
    neutral = np.array(frames[0])
    assert frames[0].tobytes() == Image.open(MASTER.parent / "rest/frame-000.png").convert("RGBA").tobytes()
    y, x = np.mgrid[:256,:256]
    opaque_head = (y < 142) & (neutral[:,:,3] == 255)
    limb_roi = (x >= 150) & (y >= 100) & (y < 194) & ((y >= 142) | (x > 198))
    for i, frame in enumerate(frames):
        data = np.array(frame)
        assert np.array_equal(data[198:], neutral[198:])
        assert np.array_equal(data[:100], neutral[:100])
        assert np.array_equal(data[:,:150], neutral[:,:150])
        assert np.array_equal(data[opaque_head], neutral[opaque_head])
        assert not np.any(data[data[:,:,3] == 0])
        bounds = frame.getchannel("A").getbbox()
        assert bounds[3] == 223
        assert min(bounds[:2]) >= 16 and max(bounds[2:]) <= 240
        color = data[:,:,:3].astype(int)
        visible_limb = limb_roi & (data[:,:,3] >= 128) & (color[:,:,2]-color[:,:,1] > 25)
        ly, lx = np.nonzero(visible_limb)
        next_data = np.array(frames[(i+1)%8])
        changed = np.any(data != next_data,axis=2)
        motion_roi = np.any(data != neutral,axis=2) | np.any(next_data != neutral,axis=2)
        bg = np.array([246,247,249])
        alpha = data[:,:,3:4].astype(float)/255
        next_alpha = next_data[:,:,3:4].astype(float)/255
        composite = data[:,:,:3]*alpha + bg*(1-alpha)
        next_composite = next_data[:,:,:3]*next_alpha + bg*(1-next_alpha)
        difference = np.max(np.abs(composite-next_composite),axis=2)
        metrics.append({
            "frame": i,
            "rgba_sha256": hashlib.sha256(frame.tobytes()).hexdigest(),
            "bounds": bounds,
            "visible_limb_area_px": int(visible_limb.sum()),
            "visible_limb_centroid": [round(float(lx.mean()),2),round(float(ly.mean()),2)],
            "visible_limb_bounds": [int(lx.min()),int(ly.min()),int(lx.max()+1),int(ly.max()+1)],
            "adjacent_changed_pixels": int(changed.sum()),
            "adjacent_motion_roi_pixels": int(motion_roi.sum()),
            "adjacent_motion_roi_max_channel_mean": round(float(difference[motion_roi].mean()),2) if motion_roi.any() else 0,
            "adjacent_pixels_over_24_channel_delta": int((difference > 24).sum()),
        })
    assert frames[0].tobytes() == frames[7].tobytes()
    assert len({frame.tobytes() for frame in frames[1:7]}) == 6
    for i, metric in enumerate(metrics[:-1]):
        metric["next_visible_limb_area_ratio"] = round(metrics[i+1]["visible_limb_area_px"] / metric["visible_limb_area_px"], 3)
    (HERE / "checks.json").write_text(json.dumps({
        "status": "qualified study rejected from production source by agent inspection: shoulder crease and pose-volume discontinuity; no human verdict",
        "canonical_master_sha256": EXPECTED_MASTER,
        "raw_sha256": EXPECTED_RAW,
        "frame_duration_ms": 125,
        "frame_recipe": ["exact neutral", "neutral arm +45 degrees", "neutral arm +110 degrees", "raw pose 3", "raw pose 4", "neutral arm +105 degrees", "neutral arm +40 degrees", "exact neutral"],
        "raw_poses": POSES,
        "unchanged_regions": {"feet_hips": [0,198,256,256], "upper_head": [0,0,256,100], "opposite_body": [0,0,150,256], "opaque_head": "all neutral alpha=255 pixels above row 142"},
        "metrics_limit": "Visible colored-limb area excludes opaque head but includes projected upper arm/paw; it is not physical volume or smooth-motion proof. Delta means use the union of the two non-neutral regions, not the whole cell.",
        "actual_speed_observed": False,
        "human_verdict": None,
        "frames": metrics,
    }, indent=2) + "\n")
    print("PASS: eight frames, exact neutral endpoints, fixed feet/upper head/opposite body, clear alpha and bounded envelope. Study only.")


if __name__ == "__main__":
    main()
