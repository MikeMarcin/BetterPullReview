"""Reproduce the retained G1 study and falsify full-head protection claims."""
import importlib.util
import json
from pathlib import Path
import sys

import numpy as np
from PIL import Image

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("study", HERE / "prepare.py")
study = importlib.util.module_from_spec(spec)
spec.loader.exec_module(study)
frames, lock = study.build_frames()
actual = study.measure(frames, lock)
recorded = json.loads((HERE / "checks.json").read_text())
assert json.loads(json.dumps(actual)) == recorded["frames"]
for size in (64, 72, 96, 256):
    generated = study.prior.plate(frames, size)
    saved = Image.open(HERE / f"greet-{size}-light-dark.png").convert("RGB")
    assert saved.size == generated.size and saved.tobytes() == generated.tobytes()
saved_mask = np.array(Image.open(HERE / "locked-head-mask.png")) > 0
assert np.array_equal(saved_mask, lock)
overlay, facts = study.shoulder_overlay(frames, lock)
assert Image.open(HERE / "shoulder-root-comparison.png").convert("RGB").tobytes() == overlay.tobytes()
assert json.loads(json.dumps(facts)) == recorded["shoulder_comparison"]

neutral = np.array(frames[0])
y, x = np.mgrid[:256, :256]
edge = lock & (neutral[:, :, 3] > 0) & (neutral[:, :, 3] < 255) & (x >= 150) & (y >= 100)
chin = lock & (y >= 142) & (neutral[:, :, 0] > 160) & (neutral[:, :, 1] > 140)
edge_y, edge_x = np.argwhere(edge)[0]
chin_y, chin_x = np.argwhere(chin)[0]
faults = [(int(edge_x), int(edge_y)), (int(chin_x), int(chin_y)), (80, 170), (100, 215)]
for point in faults:
    damaged = [frame.copy() for frame in frames]
    rgba = list(damaged[3].getpixel(point))
    rgba[0] ^= 1
    damaged[3].putpixel(point, tuple(rgba))
    try:
        study.measure(damaged, lock)
    except AssertionError:
        pass
    else:
        raise AssertionError(f"Locked-pixel fault escaped at {point}")

print("PASS: all frame metrics, four plates, shoulder overlay and head mask reproduce; translucent head edge, pale chin, opposite body and foot faults rejected.")
