"""Verify exact blink locks, adversarial changes, and empty-output regeneration."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import numpy as np
from PIL import Image

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

def pixels(path):
    return np.array(Image.open(path).convert('RGBA'))

def locks(frame,neutral,mask):
    assert np.array_equal(frame[:,:,3],neutral[:,:,3]), 'Alpha drift'
    assert np.array_equal(frame[~mask],neutral[~mask]), 'Outside-eye drift'
    assert not np.any(frame[frame[:,:,3]==0,:3]), 'Hidden RGB'

checks=json.loads((HERE/'checks.json').read_text())
for rel,h in checks['input_sha256'].items():
    assert hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()==h, 'Input payload drift'
for rel,h in checks['outputs'].items():
    assert hashlib.sha256((HERE/rel).read_bytes()).hexdigest()==h, 'Candidate payload drift'
for directory in ['native','frames']:
    assert {p.name for p in (HERE/directory).glob('*.png')}=={f'frame-{i:03d}.png' for i in range(6)}, 'Frame inventory drift'
neutral=pixels(ROOT/'docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png')
mask=np.array(Image.open(HERE/'eye-mask.png'))>0
native=[pixels(HERE/'native'/f'frame-{i:03d}.png') for i in range(6)]
frames=[pixels(HERE/'frames'/f'frame-{i:03d}.png') for i in range(6)]
for frame in native:
    locks(frame,neutral,mask)
for i in [0,4,5]:
    assert np.array_equal(native[i],neutral), 'Neutral handoff drift'
assert np.array_equal(native[1],native[3]), 'Asymmetric reopen'
assert len({a.tobytes() for a in native})==3, 'Missing blink poses'
assert np.array_equal(native[1][390:],neutral[390:]), 'Half lid changed lower eyeball'
mask256=mask.reshape(256,4,256,4).any(axis=(1,3))
for frame in frames:
    locks(frame,frames[0],mask256)
strip=pixels(HERE/'candidate-strip.png')
assert strip.shape==(256,2048,4)
assert not np.any(strip[:,:256]) and not np.any(strip[:,7*256:]), 'Nonblank gutter'
for i,frame in enumerate(frames):
    assert np.array_equal(strip[:,(i+1)*256:(i+2)*256],frame), 'Strip cell mismatch'
for name,xy,channel in [('alpha',(399,389),3),('outside_eye',(511,600),0)]:
    bad=native[2].copy();x,y=xy;bad[y,x,channel]^=1
    try:
        locks(bad,neutral,mask)
    except AssertionError:
        pass
    else:
        raise AssertionError('Adversarial '+name+' drift was accepted')
with tempfile.TemporaryDirectory(prefix='regeneration-',dir=HERE) as scratch:
    subprocess.run([sys.executable,str(HERE/'prepare.py'),'--output',scratch],check=True,capture_output=True)
    for rel in checks['outputs']:
        assert np.array_equal(pixels(HERE/rel),pixels(Path(scratch)/rel)), 'Regenerated pixel drift '+rel
    regenerated=json.loads((Path(scratch)/'checks.json').read_text())
    # Compressed hashes are recorded per run; only canonical pixels bind replay.
    for key in checks.keys()-{'outputs'}:
        assert checks[key]==regenerated[key], 'Regenerated metadata drift '+key
print('PASS: 6 frames, 3 distinct poses, exact native and 256 locks, neutral handoffs, gutters, 2 adversarial mutations, empty-output pixel/metadata reproduction.')
