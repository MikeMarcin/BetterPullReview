# Dif v004 connected greeting study

Producer: `rfc-worker-deep`, run `dif-takeover-20260914`, dispatch d6,
mandate revision 3 with the revision 4 helper-invocation amendment.
Bounded BPR-0755 continuation delegated by `rfc-integrator-d4`.
**Rejected from production source by agent inspection.** The correction improves
the open-paw anatomy but does not finish the greeting. No source inventory,
manifest, runtime asset, issue status, RFC status or human verdict is changed.
The core decides. The host performs. Qt presents.

The [recipe](prepare.py) reproduces eight 256-square study cells, with intended
timing 125 ms each, and the retained untimed light/dark plates:
[64 pixels](greet-64-light-dark.png), [72 pixels](greet-72-light-dark.png),
[96 pixels](greet-96-light-dark.png), and
[256-pixel diagnostic](greet-256-light-dark.png).
Individual rejected frame files are not retained; the recipe reconstructs them
in memory. The [measurements](checks.json) bind their decoded RGBA digests.
These images establish neither actual-speed playback nor native Qt rendering.

## Inputs, extraction and authored geometry

The exact canonical neutral is the unchanged
[v002 master](../dif-production-art-v002/source/dif-neutral-master.png), SHA-256
`e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
The recipe verifies its decoded pixels equal the accepted
[v001 original](../dif-production-art-v001/dif-neutral-master-candidate.png).
The existing neutral identity acceptance, session
`cbd48b91000b00f1e72e1ad62c7c384c`, sequence 379, is unchanged and does not
accept this motion study or its generated derivative.

The integrator supplied [two generated full-body inputs](generated-inputs/README.md)
with their exact prompts/hashes. Both are native RGBA 1254×1254. Registration
of the original curled lower paw and original raised paw reduced aggregate
limb-area jumps, but inspection found a rounded horizontal fist changing into
a slender diagonal open palm. Their unchanged pixels remain retained as inputs.
They supply no final frame pixels.

One authorized built-in image edit produced a connected open-paw intermediate.
Its [complete prompt, native output and provenance](generated-correction/README.md)
are retained. The result has SHA-256
`71aed58a2c88fec60e76ca00d314c55cfe65c1dd539ebd42e47b010930fd347f`.
No further model calls were made. It supplies only a local arm layer; its
changed face, opposite body and feet are excluded.

The recipe records the native arm polygons, 0.7-pixel boundary feather,
original-alpha multiplication, and local purple-material extraction threshold.
Only the arm is uniformly scaled by 0.816 and registered from source shoulder
`(845,728)` to master `(690,592)`. A local outward shift reaches five master
pixels on the correction. The 1254-native artwork is sampled onto the master's
1024 editing canvas; no native-detail upscaling claim is made.

| Frame | Retained pose construction |
|---|---|
| 0, 7 | Exact normalized accepted neutral. |
| 1, 6 | Accepted neutral arm rotated 30° and 25° around `(710,596)`. |
| 2 | Extracted connected open-paw correction at the recorded registration. |
| 3 | Correction arm rotated 20° around `(690,592)`, then translated 16 master pixels right. |
| 4 | Frame 3 arm with a local 12° wrist deformation around `(839,526)`, smoothly reduced toward the shoulder. |
| 5 | Correction arm rotated 8° around `(690,592)`. |

There is no cross-fade between hands. These are two actual source arm states
plus local articulation; they do **not** constitute a newly authored finger
uncurl sequence. Frame 3's rigid arm rotation and translation also do not
prove an anatomically valid shoulder movement.

The exposed belly reuses the exact v003 cloned-texture recipe: master region
x=600..823, y=560..791, a recorded shaded contour, and local boundary blending.
It is authored hidden anatomy, not recovered pixels. The v003 head-occlusion,
cell reduction and plate helpers are reused read-only; no rejected v003 arm or
frame is promoted. The recipe pins the v003 helper and existing asset compiler
hashes. Reduction uses the compiler's alpha-weighted 4×4 box and zero-alpha
RGB cleanup. Creative tools and this study are absent from ordinary builds.

## Objective result and visual rejection

All eight cells pass exact neutral endpoints against v002 rest; exact opaque
head pixels above row 142; exact upper rows 0..99, opposite-body columns
0..149 and hip/foot rows 198..255; clean zero-alpha RGB; baseline y=223;
and at least 16 pixels of outside clearance. Six moving poses are distinct.

| Frame | Visible colored limb area | Pink-material proxy | Changed translucent head-edge pixels | Changed pale chin pixels |
|---|---:|---:|---:|---:|
| 0 | 1513 | 21 | 0 | 0 |
| 1 | 1456 | 9 | 16 | 12 |
| 2 | 1705 | 207 | 24 | 16 |
| 3 | 1606 | 205 | 103 | 12 |
| 4 | 1581 | 209 | 73 | 12 |
| 5 | 1654 | 209 | 53 | 14 |
| 6 | 1464 | 9 | 15 | 12 |
| 7 | 1513 | 21 | 0 | 0 |

The limb metric keeps the v003 region/material definition for comparison;
it measures projected colored area, not physical volume. The pink threshold
is only a color proxy: its nonzero neutral reading demonstrates false positives,
so these counts are not segmented anatomical pad areas. The JSON also records
centroids, bounds and adjacent differences over the union of moving regions.

Inspection covered all eight static frames at 64/72/96 on `#F6F7F9` and
`#1C2026`, and the 256 diagnostic. The correction keeps the middle open palm
recognizable and avoids substituting the original skinny raised-arm surface.
Feet remain planted and the face stays readable. Rejection remains necessary:

- Frame 1→2 substitutes a fully open padded palm for closed neutral fingers,
  with visible limb area increasing 17.1%. The corresponding return drops
  11.5% at 5→6. More importantly, there is no actual partially uncurling paw
  between the two silhouettes. Matching total colored area would not fix it.
- Raised frames 3/4 retain a sharp diagonal shoulder/body junction near runtime
  x=174..188, y=146..165. Rotating and translating the whole connected source
  arm moves its attachment away from the accepted shoulder surface.
- Opaque head equality is **not full-head equality**. Compositing changes
  15–103 nonzero-alpha edge pixels above row 142 and 12–16 pale lower-chin
  pixels in moving frames. The expanded measurements expose this limitation;
  the checked opaque-region equality must not conceal it.
- The wrist difference is restrained in the static small-size plates. Its
  timing/readability has no actual-speed observation or human motion verdict.

The next artwork operation is concrete: author a shoulder-fixed sequence at
the exact accepted master's scale, with a rounded upper arm/elbow that remains
attached under the cheek while the forearm lifts, and separately author a
partially open hand between the accepted curled fingers and the correction's
open padded palm. Preserve comparable paw breadth and forearm thickness through
those states; do not solve area counts by stretching the whole arm. Then mask
the exact accepted head edge and lower chin as immutable pixels and retouch
only the newly exposed torso below that boundary. This still requires artwork
and mask work, so the remaining evidence is not merely an attended verdict.

## Verification and disposition

Reproduce from any directory with Python 3.10+, Pillow and NumPy:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v004/prepare.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v004/verify.py
```

The focused verifier reproduces all recorded frame metrics and all four decoded
plates, and rejects five in-memory fault controls: changed opaque face,
opposite body, foot, hidden RGB, and a non-neutral terminal. It does not accept
the full head mask, anatomy or timing. The actual-file digests are bound in
[SHA256SUMS](SHA256SUMS). The repository documentation gate passed with `DOCUMENTATION_OK` after
creating the digest receipt; no broader application test is claimed here.

No attended session, runtime selection, remote mutation, board movement or
commit occurred. The accepted neutral and v001–v003 remain unchanged. No
background commands remain at handoff. Semantic documentation disposition:
no public API, architecture, persistence, UI, build or packaging behavior change.
