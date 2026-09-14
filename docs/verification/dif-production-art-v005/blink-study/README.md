# BPR-0755 d7 blink extraction study

Result: **FAIL for promotion**. This is a bounded offline extraction experiment,
not a production frame, complete blink clip, or attended acceptance. The core
decides, the host performs, Qt presents; no runtime behavior changed.

The closed eyes are legible at 64, 72 and 96 pixels on light, dark and black
surfaces. Agent inspection of all four comparison plates finds the accepted
neutral identity and planted body preserved. At 256 pixels, however, a pale
curved/diagonal sliver remains below the viewer-right lid near the nose, and
both extracted interiors have uneven shading relative to the surrounding
purple patches. The old horizontal donor bands are reduced, but replacement
boundary defects prevent a clean-material verdict. No human verdict is claimed.

## Reproduce and inspect

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v005/blink-study/prepare.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/verify_documentation.py --root /Users/jaeger/Documents/GitHub/BetterPullReview
```

Requires Python with Pillow and NumPy. `prepare.py` pins the accepted neutral
and generated input by SHA-256, retains the full 1024-square neutral, downsamples
the actual 1254-square generated candidate to that registration space, and
translates the two donor eyes separately. Two elliptical masks centered at
(398,389) and (625,389), radii (49,65), constrain the blend. Their outer 1/8
of squared radial distance feathers inward; all exterior RGBA and all alpha
are copied exactly from neutral. This is local candidate compositing, not
new generated detail or runtime matte repair.

`checks.json` records every exact input/output PNG hash, mask parameters and
asserted pixel bounds. The native result changes 19,861 pixels, confined to
exclusive box [350,325,674,454]. Outside-mask changed pixels: zero. Alpha
changed pixels: zero. RGB under zero alpha: zero. Native alpha bounds remain
[186,86,838,892]; fixed-box 256 reduction retains baseline bottom 223.
The comparison plates show neutral / v004 closed panel / v005 candidate on
three themes. The v004 panel retains its historical normalization; it is a
material-defect reference rather than proof of identical registration.

Exact candidate PNG SHA-256:

- Native: `d3e7df3d333099105ef7c0f500cb066c76e4b85ffcd3fefade06ec2d17cbeb41`
- 256: `2064314ec8a7758401ac60d5a847dc7e925540e07028426fdeb28d2260db9bd0`

## Remaining work

A further eye-local edit must continue the accepted patch material through the
former iris areas without importing pale face pixels at the inner/lower edge.
Author the lid narrower within the existing patch interior and match the
neutral's purple gradient locally; do not expand the mask into the face or
replace the full patch. This attempt does not establish that every alternate
extraction is impossible. Half-lowered lids, six-frame timing and real-speed
handoffs remain unbuilt, so the remaining evidence is not merely a human
verdict. No attended packet is warranted for this rejected pose. The integrator
owns any next creative-edit authorization and final catalog review.
