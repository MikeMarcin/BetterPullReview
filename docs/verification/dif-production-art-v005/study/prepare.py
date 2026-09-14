"""Bounded G1 extraction study. All writes remain inside this study directory."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
VERIFICATION = HERE.parents[1]
PREVIOUS = VERIFICATION / "dif-production-art-v004/prepare.py"
EXPECTED_PREVIOUS = "b7573fce71ce61429349e858ca67044293fd738791683964a2eb7b5802c83ea2"
BRIDGE = HERE.parent / "generated-inputs/greet-partly-open-bridge.png"
EXPECTED_BRIDGE = "36c5871091ddccfee8cb1297da15dedf659051b6a05eebf4ed4bc7e351f6012f"
PEAK = HERE.parent / "generated-inputs/greet-shoulder-fixed-open-peak.png"
EXPECTED_PEAK = "e64f4de396ccfa88b0fd1002ef3855a93cdd0424c259284ea70e7efdef7966c0"
POLYGON = [(806,723),(866,698),(898,666),(911,617),(937,594),(975,592),
           (1008,608),(1040,621),(1050,655),(1063,677),(1068,711),(1049,756),
           (1028,790),(987,821),(936,839),(864,840),(839,798)]
PEAK_POLYGON = [(811,735),(869,711),(925,675),(943,619),(950,582),(982,570),
                (1010,575),(1026,590),(1054,585),(1080,611),(1087,637),
                (1114,651),(1121,682),(1109,723),(1084,768),(1041,813),
                (983,846),(928,862),(868,855),(840,807)]
REGISTRATION = {"source_shoulder": [845,744], "target_shoulder": [690,592],
                "uniform_scale": .816, "bridge_rotation_degrees": -18,
                "return_rotation_degrees": -12}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


assert digest(PREVIOUS) == EXPECTED_PREVIOUS
spec = importlib.util.spec_from_file_location("v004", PREVIOUS)
previous = importlib.util.module_from_spec(spec)
spec.loader.exec_module(previous)
prior = previous.prior


def extracted_arm(path, expected, polygon):
    assert digest(path) == expected
    source = Image.open(path).convert("RGBA")
    assert source.size == (1254, 1254)
    mask = Image.new("L", source.size)
    ImageDraw.Draw(mask).polygon(polygon, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(.7))
    data = np.array(source)
    rgb = data[:, :, :3].astype(float)
    purple = np.clip((rgb[:, :, 2] - rgb[:, :, 1] - 12) / 12, 0, 1)
    data[:, :, 3] = np.rint(data[:, :, 3].astype(float) * np.array(mask) / 255 * purple).astype("uint8")
    scale = REGISTRATION["uniform_scale"]
    # Inverse uniform transform: source shoulder -> exact master shoulder anchor.
    affine = (1/scale, 0, 845-690/scale, 0, 1/scale, 744-592/scale)
    return prior.clean(Image.fromarray(data).transform((1024, 1024), Image.Transform.AFFINE,
                                                      affine, Image.Resampling.BICUBIC))


def head_mask(master):
    """Conservative 4x4 footprint of the accepted head, including its alpha fringe."""
    data = np.array(master)
    y, x = np.mgrid[:1024, :1024]
    visible = data[:, :, 3] > 0
    head = visible & ((y < 568) | ((y < 592) & (data[:, :, 0] > 160) & (data[:, :, 1] > 140)))
    return head.reshape(256, 4, 256, 4).any(axis=(1, 3))


def build_frames():
    assert digest(prior.MASTER) == prior.EXPECTED_MASTER
    master = Image.open(prior.MASTER).convert("RGBA")
    accepted = Image.open(VERIFICATION / "dif-production-art-v001/dif-neutral-master-candidate.png").convert("RGBA")
    assert accepted.tobytes() == master.tobytes()
    neutral = np.array(prior.cell(master))
    lock = head_mask(master)
    belly = prior.reconstruct_belly(master)
    bridge = extracted_arm(BRIDGE, EXPECTED_BRIDGE, POLYGON)
    open_arm = extracted_arm(PEAK, EXPECTED_PEAK, PEAK_POLYGON)
    moving = [
        prior.neutral_arm(master, 30),
        bridge.rotate(-18, Image.Resampling.BICUBIC, center=(690,592)),
        open_arm,
        previous.wrist_turn(open_arm, 12),
        bridge.rotate(-12, Image.Resampling.BICUBIC, center=(690,592)),
        prior.neutral_arm(master, 25),
    ]
    frames = [Image.fromarray(neutral)]
    for limb in moving:
        composite = prior.protect_head(Image.alpha_composite(belly, limb), master)
        data = np.array(prior.cell(composite))
        # Offline cell construction: never resample/repaint the exact locked pixels.
        data[lock] = neutral[lock]
        frames.append(prior.clean(Image.fromarray(data)))
    frames.append(Image.fromarray(neutral))
    return frames, lock


def measure(frames, lock):
    metrics = previous.measure_frames(frames)
    neutral = np.array(frames[0])
    for frame, row in zip(frames, metrics):
        data = np.array(frame)
        assert np.array_equal(data[lock], neutral[lock]), "full accepted head/chin mask changed"
        row["full_head_chin_mask_changed_pixels"] = int(np.any(data[lock] != neutral[lock], axis=1).sum())
        assert row["all_nonzero_alpha_head_changed_pixels"] == 0
        assert row["pale_lower_chin_changed_pixels"] == 0
        # Diagnostic topology only: connected colored pixels do not prove anatomy.
        y, x = np.mgrid[:256, :256]
        rgb = data[:, :, :3].astype(int)
        region = (x >= 150) & (y >= 100) & (y < 194) & ~lock
        colored = region & (data[:, :, 3] >= 128) & (rgb[:, :, 2] - rgb[:, :, 1] > 25)
        unseen = set(zip(*np.nonzero(colored)))
        components = []
        while unseen:
            pending = [unseen.pop()]
            component = []
            while pending:
                py, px = pending.pop()
                component.append((py, px))
                for neighbor in ((py-1,px), (py+1,px), (py,px-1), (py,px+1)):
                    if neighbor in unseen:
                        unseen.remove(neighbor)
                        pending.append(neighbor)
            components.append(component)
        largest = max(components, key=len)
        row["largest_colored_component_pixels"] = len(largest)
        row["colored_pixels_outside_largest_component"] = int(colored.sum()) - len(largest)
        row["largest_component_intersects_shoulder_window"] = any(145 <= py < 166 and 168 <= px < 187 for py, px in largest)
    return metrics


def shoulder_overlay(frames, lock):
    """Show final G1/G2 pixels and thresholded occupancy at the same cell coordinates."""
    crop = (160, 136, 228, 179)
    root = (168, 145, 187, 166)
    y, x = np.mgrid[:256, :256]
    masks = []
    for index in (2, 3):
        data = np.array(frames[index])
        rgb = data[:, :, :3].astype(int)
        masks.append((x >= 150) & (y >= 100) & (y < 194) & ~lock &
                     (data[:, :, 3] >= 128) & (rgb[:, :, 2] - rgb[:, :, 1] > 25))
    first, second = masks
    diagnostic = np.full((256, 256, 3), 246, dtype="uint8")
    diagnostic[first & second] = (95, 95, 110)
    diagnostic[first & ~second] = (190, 45, 65)
    diagnostic[second & ~first] = (35, 100, 215)
    panel_width = (crop[2] - crop[0]) * 4
    height = (crop[3] - crop[1]) * 4 + 64
    plate = Image.new("RGB", (panel_width * 3, height), "#F6F7F9")
    draw = ImageDraw.Draw(plate)
    titles = ("G1 / frame 2", "G2 / frame 3", "Red G1 only; blue G2 only; gray both")
    for panel, index in enumerate((2, 3, None)):
        if index is None:
            image = Image.fromarray(diagnostic)
        else:
            image = Image.new("RGBA", (256, 256), "#F6F7F9")
            image.alpha_composite(frames[index])
            image = image.convert("RGB")
        plate.paste(image.crop(crop).resize((panel_width, height-64), Image.Resampling.NEAREST),
                    (panel * panel_width, 40))
        draw.text((panel * panel_width + 5, 5), titles[panel], fill="#1F2328")
        rx = panel * panel_width + (root[0] - crop[0]) * 4
        ry = 40 + (root[1] - crop[1]) * 4
        draw.rectangle((rx, ry, rx+(root[2]-root[0])*4-1, ry+(root[3]-root[1])*4-1),
                       outline="#D78C00", width=2)
    draw.text((5, height-18), "Crop [160,136,228,179); orange root window [168,145,187,166); nearest 4x", fill="#1F2328")
    inside = (x >= root[0]) & (x < root[2]) & (y >= root[1]) & (y < root[3])
    facts = {"frames": [2, 3], "crop_bounds": crop, "root_window_bounds": root,
             "root_g1_only_pixels": int((inside & first & ~second).sum()),
             "root_g2_only_pixels": int((inside & second & ~first).sum()),
             "root_shared_colored_pixels": int((inside & first & second).sum()),
             "limit": "Pixel occupancy of two different poses; no threshold here accepts anatomical continuity."}
    return plate, facts


def main():
    frames, lock = build_frames()
    metrics = measure(frames, lock)
    for size in (64, 72, 96, 256):
        prior.plate(frames, size).save(HERE / f"greet-{size}-light-dark.png")
    Image.fromarray((lock * 255).astype("uint8")).save(HERE / "locked-head-mask.png")
    overlay, shoulder_facts = shoulder_overlay(frames, lock)
    overlay.save(HERE / "shoulder-root-comparison.png")
    old = json.loads((VERIFICATION / "dif-production-art-v004/checks.json").read_text())
    report = {
        "status": "rejected greeting: partly open bridge is useful, but shoulder and peak/return shape continuity remain unresolved; no human verdict",
        "bridge_input_sha256": digest(BRIDGE), "peak_input_sha256": digest(PEAK),
        "master_sha256": digest(prior.MASTER),
        "v004_recipe_sha256": digest(PREVIOUS), "registration": REGISTRATION,
        "extraction_polygon_native": POLYGON, "peak_extraction_polygon_native": PEAK_POLYGON,
        "frame_duration_ms": 125,
        "frame_recipe": ["exact neutral", "neutral arm +30 degrees", "G1 arm -18 degrees",
                         "G2 open arm peak (unaccepted)", "G2 with local +12-degree wrist turn",
                         "G1 arm -12 degrees", "neutral arm +25 degrees", "exact neutral"],
        "locked_head_mask_pixels": int(lock.sum()),
        "shoulder_comparison": shoulder_facts,
        "locked_head_definition": "Any 4x4 master footprint containing nonzero alpha above row 568 or pale head/chin pixels above row 592; exact neutral RGBA restored offline after reduction.",
        "actual_speed_observed": False, "human_verdict": None,
        "v004_visible_limb_areas": [row["visible_limb_area_px"] for row in old["frames"]],
        "frames": metrics,
    }
    (HERE / "checks.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: exact full-head/chin mask, neutral endpoints, opposite body, feet, alpha, baseline and clearance.")
    print("Limb areas:", [row["visible_limb_area_px"] for row in metrics])
    print("Pink-color proxies:", [row["pink_material_proxy_area_px"] for row in metrics])


if __name__ == "__main__":
    main()
