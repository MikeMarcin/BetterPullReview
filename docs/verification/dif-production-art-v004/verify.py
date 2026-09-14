"""Focused study verification and in-memory fault controls; no runtime writes."""
import importlib.util
import json
from pathlib import Path
import sys

from PIL import Image

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("study", HERE / "prepare.py")
study = importlib.util.module_from_spec(spec)
spec.loader.exec_module(study)

frames = study.build_frames()
metrics = study.measure_frames(frames)
recorded = json.loads((HERE / "checks.json").read_text())
assert json.loads(json.dumps(metrics)) == recorded["frames"]
for size in (64, 72, 96, 256):
    expected = study.prior.plate(frames, size)
    actual = Image.open(HERE / f"greet-{size}-light-dark.png").convert("RGB")
    assert expected.size == actual.size and expected.tobytes() == actual.tobytes()

faults = [
    ("changed opaque face", 3, (128, 70), (1, 2, 3, 255)),
    ("changed opposite body", 3, (80, 170), (1, 2, 3, 255)),
    ("changed foot", 3, (100, 215), (1, 2, 3, 255)),
    ("hidden RGB in moving region", 3, (230, 180), (1, 2, 3, 0)),
    ("non-neutral terminal", 7, (180, 170), (1, 2, 3, 255)),
]
for name, index, point, rgba in faults:
    damaged = [frame.copy() for frame in frames]
    damaged[index].putpixel(point, rgba)
    try:
        study.measure_frames(damaged)
    except AssertionError:
        pass
    else:
        raise AssertionError(f"Fault escaped verification: {name}")

print("PASS: recorded metrics and four decoded plates reproduce; five in-memory fault controls rejected.")
