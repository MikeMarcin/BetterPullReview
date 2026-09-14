# G1/G2 greeting bridge and peak study

Producer: `rfc-worker-deep`, run `dif-takeover-20260914`, dispatch d7,
mandate revision 5, bounded BPR-0755 continuation. **The full greeting remains
rejected.** G1 and G2 add distinct partially open and fully open finger anatomy, and
this recipe fixes exact head-edge/chin preservation. Their local composites
still do not establish a consistent rounded shoulder/elbow junction. No source, catalog, runtime,
attended verdict, card status or commit changes here.
The core decides. The host performs. Qt presents.

The [recipe](prepare.py) reconstructs eight 256-square study frames, nominally
125 ms each, and the untimed [64](greet-64-light-dark.png),
[72](greet-72-light-dark.png), [96](greet-96-light-dark.png) and
[256-pixel](greet-256-light-dark.png) light/dark plates. Only compact study
media are retained; individual rejected frames remain reproducible in memory.
The [measurements](checks.json) record their exact decoded RGBA hashes.

## Bound inputs and construction

The integrator supplied the [G1 brief](../PROMPTS.md) and
[1254-square RGBA bridge](../generated-inputs/greet-partly-open-bridge.png),
SHA-256 `36c5871091ddccfee8cb1297da15dedf659051b6a05eebf4ed4bc7e351f6012f`.
The integrator then supplied the [G2 open-paw peak](../generated-inputs/greet-shoulder-fixed-open-peak.png),
SHA-256 `e64f4de396ccfa88b0fd1002ef3855a93cdd0424c259284ea70e7efdef7966c0`,
also native 1254-square RGBA, under the G2 brief. Both full generated images
change the head, face and body; none of those pixels is used. This worker
made no image-generation call. Generation provenance and the supplied prompts
remain integrator-owned outside this study directory.

The accepted identity is the unchanged
[v002 canonical neutral](../../dif-production-art-v002/source/dif-neutral-master.png),
SHA-256 `e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
The recipe verifies its decoded pixels equal the accepted v001 original and
its 256 reduction equals the existing v002 rest cell. Neutral identity
acceptance at session `cbd48b91000b00f1e72e1ad62c7c384c`, event 379, does not
accept G1, the exposed belly or this greeting.

Each explicitly recorded native arm polygon is feathered by 0.7 native pixels and
multiplied by original alpha plus the bounded purple-material mask. The arm
alone is uniformly reduced by 0.816, registering the selected source shoulder
anchor `(845,744)` to master `(690,592)` in both inputs. This is a manually
selected registration point, not proof of matching anatomical roots. There
is no whole-body registration or native-detail
upscaling claim. G1 rotates −18° at frame 2 and −12° at frame 5 around that
fixed shoulder anchor. Those are local rigid shoulder articulations, not new
finger artwork or proof of an anatomical joint.

| Frames | Pixel sources and operation |
|---|---|
| 0, 7 | Exact normalized accepted neutral. |
| 1, 6 | Existing neutral arm rotations, 30° and 25°. |
| 2, 5 | Extracted partly open G1 with the two stated shoulder rotations. |
| 3, 4 | The extracted G2 open-paw peak and a local 12° wrist turn using the existing deformation helper. |

The recipe imports the hash-pinned [v004 helpers](../../dif-production-art-v004/prepare.py)
and their already pinned v003 helpers/compiler. No v004 frame or generated-arm pixels enter the final study. The exposed
belly remains the v003 authored clone/shaded-contour reconstruction.
These unchanged dependencies are experiments, not accepted anatomy. No two
hands are cross-faded and no full-raster squash is used.

## Exact head/chin repair

The [retained lock mask](locked-head-mask.png) covers 17,539 runtime pixels.
It includes every 4×4 master footprint containing a nonzero-alpha head pixel
above source row 568, plus pale lower-head/chin pixels above row 592. After
alpha-weighted reduction, those cells copy the exact neutral RGBA values.
This conservative block footprint preserves translucent edge RGB **and** alpha,
rather than checking opaque face pixels alone. The repair happens in offline
candidate construction; there is no runtime matte operation.

Every frame now has zero changed pixels in that complete mask, zero changed
nonzero-alpha head pixels above runtime row 142, and zero changed pale chin
pixels below it. Opposite-body columns 0..149, upper rows 0..99, hip/foot rows
198..255, the neutral endpoints, alpha-bottom-exclusive baseline 223, zero
hidden RGB and at least 16 pixels of outer clearance also pass exactly.
All six moving cells are distinct.

## Measurements and visual judgment

| Frame | Colored limb area | Pink-material proxy | Largest connected colored arm region |
|---|---:|---:|---:|
| 0 | 1513 | 21 | 1503 |
| 1 | 1459 | 13 | 1449 |
| 2 | 1351 | 141 | 1416 |
| 3 | 1587 | 230 | 1652 |
| 4 | 1555 | 217 | 1616 |
| 5 | 1284 | 115 | 1349 |
| 6 | 1467 | 13 | 1457 |
| 7 | 1513 | 21 | 1503 |

The first metric retains v004's color/region definition for comparison. The
connectivity screen instead excludes the full head mask and uses one local
arm region; each frame has one four-connected colored component intersecting
the stated shoulder window. These different region definitions explain the
different counts. They prove pixel connectivity, not a believable elbow or
shoulder. The pink threshold is a color proxy, not segmented pads or physical
volume; its nonzero neutral value demonstrates false positives.

Agent inspection covered all static 64/72/96-pixel frames on `#F6F7F9` and
`#1C2026`, plus the 256 diagnostic. The bridge visibly retains a curled outer
finger while exposing fewer pads than the open peak. It introduces a useful
partial opening state between the closed approach and open hand. The face,
opposite arm and feet remain steady.

The full greeting still fails:

- G1's inner upper-arm boundary is abrupt against the belly under the cheek
  at roughly runtime x=170..180, y=145..161. Fixed-anchor registration and
  single-component connectivity do not make that contour a rounded shoulder.
- G2 supplies a genuine fully open state, but its registered inner root is
  still diagonal and its forearm silhouette differs from G1. The visible limb
  area rises 17.5% at 2→3, and drops 17.4% at 4→5. The issue is the changing
  root/forearm contour seen on the diagnostic, not a numeric rejection limit.
  Distinct finger states alone do not make the neighboring arm surfaces agree.
- A mirrored use of the same G1 input supplies a useful lowering study but
  is not independently authored closure anatomy. Actual-speed motion and
  wrist readability have not been observed. No human art verdict is claimed.

The [shoulder overlay](shoulder-root-comparison.png) makes the local repair
target explicit. It crops runtime `[160,136,228,179)` and shows frames 2/3
at nearest-neighbor 4×. Orange marks the common root window
`[168,145,187,166)`, equivalent to master `[672,580,748,664)`. Within it,
298 thresholded colored pixels are shared, 87 belong only to G1, and none
belong only to G2. G1's inner boundary is nearly vertical; G2 removes a wedge
and recedes diagonally toward the elbow. Red/blue/gray show G1-only/G2-only/
shared occupancy; this is diagnostic geometry, not a target area tolerance.

The concrete remaining need was sent to the integrator: reconcile G1 and G2
around one consistent local upper-arm/elbow attachment under the accepted
cheek, retaining their distinct partly open and open fingers. The two source
states now exist, so another unspecified generation call is not the missing
step. The shoulder contour, its exposed belly boundary and forearm breadth
must agree across those states; common anchor coordinates and pixel
connectivity alone have not accomplished that. Retain the exact head/chin
mask during this local retouch, then inspect timed entry, wrist and closure.
Neither G1 nor G2 is promoted as accepted source by this study.

Author the rounded attachment inside that master-coordinate repair window,
continuing the same shoulder surface into each pose's elbow and preserving
the locked head footprint along its top. Resolve the current disappearing
inner wedge as connected material, with coherent lighting and belly occlusion;
do not simply fill the 87 pixels or feather the cut to satisfy the count.
The per-input polygons and inverse affine registration in the recipe/checks
identify exactly which native surfaces supplied these pixels.

## Focused verification

Reproduce from any working directory with Python 3.10+, Pillow and NumPy:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v005/study/prepare.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v005/study/verify.py
```

The focused verifier reproduces all frame metrics, four decoded plates,
the shoulder comparison/occupancy facts and the exact lock mask. Four in-memory faults are rejected: changed translucent
head-edge RGB, pale chin RGB, opposite body and foot. These specifically
exercise the previous head/chin gap as well as existing body locks.
[SHA256SUMS](SHA256SUMS) binds only this worker's study files. The repository
documentation gate passed with `DOCUMENTATION_OK`; no application test or
runtime rendering is claimed.

Remaining work is artwork, not solely a human verdict, so no attended session
was created. No previous study or integrator-owned input was changed. No
background commands remain at handoff. Semantic disposition: no product API,
architecture, persistence, build, packaging or runtime behavior changes.
