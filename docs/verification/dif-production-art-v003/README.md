# Dif v003 greeting anatomy study

Producer: `rfc-worker-deep`, `dif-takeover-20260914` dispatch `d5`, mandate
revision 2, delegated by `rfc-integrator-d4`. Bounded BPR-0755 continuation.
**Qualified study, rejected from production source by agent inspection.**
No greeting source frames, manifest, runtime assets, or attended verdict are
published here. BPR-0755 and the production-art milestone remain open.
The core decides. The host performs. Qt presents.

The retained [recipe](prepare.py), [measurements](checks.json), and static
light/dark plates reproduce the best bounded attempt:
[64 pixels](greet-64-light-dark.png), [72 pixels](greet-72-light-dark.png),
[96 pixels](greet-96-light-dark.png), and
[256-pixel diagnostic enlargement](greet-256-light-dark.png).
The intended sequence is eight frames at 125 ms per frame; these plates are
untimed. No playback observation, native Qt rendering, or human acceptance is
claimed. Earlier browser playback was denied in v002; no alternate playback
access was attempted here.

## Exact inputs and authored changes

The canonical 1024-square [v002 master](../dif-production-art-v002/source/dif-neutral-master.png)
has SHA-256
`e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
Its decoded pixels equal the accepted [v001 neutral](../dif-production-art-v001/dif-neutral-master-candidate.png),
SHA-256
`bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
The unchanged neutral identity accept is session
`cbd48b91000b00f1e72e1ad62c7c384c`, sequence 379. That decision does not
accept this study's retouch or motion.

The immutable greeting input is
`docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/dif-greet.png`, SHA-256
`1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58`.
The recipe checks both consumed payload hashes before constructing output.
Only zero-based raw frames 3 and 4 supply pixels to the final retained study:
half-open crops `[1331,0,1774,444)` and `[0,444,444,887)`, respectively.
Their polygons, anchors, scales and local root deformation are explicit in
the recipe and measurements. The two actual native dimensions are 443×444
and 444×443. Sampling them into the 1024 editing canvas does not create native
1024 detail; all measured frame cells are reduced to 256 square using the
existing compiler's alpha-weighted box reduction and zero-alpha cleanup.

| Output frame | Pixel source and operation |
|---|---|
| 0, 7 | Exact normalized accepted neutral. |
| 1, 2 | Accepted neutral arm, isolated with a recorded polygon, rotated 45° and 110° around native `(710,596)`. |
| 3 | Raw open padded paw and forearm, uniformly scaled by 0.50 into runtime coordinates; manually widened by at most 8% near the lower root. |
| 4 | Raw wrist-tilted padded paw and forearm, uniformly scaled by 0.53; the same bounded root widening. |
| 5, 6 | Accepted neutral arm rotated 105° and 40° around the same pivot. |

No neighboring pose is cross-faded into another pose. Mask feathering is
confined to layer boundaries and joins. The native masks use a blue-minus-green
material threshold to remove pale torso intrusions while retaining pink pads;
this threshold is not applied to the accepted master globally. A first trial
incorrectly removed pads and was corrected before the retained result.

The newly exposed belly is explicitly authored, not recovered: in master
coordinates x=600..823, y=560..791, the recipe clones adjacent central belly
texture, shades its side by up to 13%, and imposes an interpolated side contour
through x=`719,743,752,744,716` at y=`560,620,700,760,792`. Boundary blending
joins that local replacement to unchanged pixels. The raised poses also use a
manually defined rounded shoulder surface sampled from the accepted purple
arm, darkened to 64–77% of the sampled RGB. A smooth root mask joins the raw
forearm to this surface. These are new geometry, alpha and shading decisions,
not proof of the hidden anatomy. The original head occludes the moving limb;
opaque head pixels are checked for exact equality.

## Result and unresolved geometry

Local inspection covered all eight frames at actual 64/72/96-pixel size on
`#F6F7F9` and `#1C2026`, plus the enlarged diagnostic. Approach and return now
retain recognizable neutral fingers and attach to the body. The open palm
reaches beside the lower cheek and the following source pose turns its wrist.
There is no double-paw cross-fade. Feet, opposite body and face remain planted.

This is still unsuitable for promotion:

- The rotated neutral arm loses visible forearm area as it passes behind the
  fixed head. The raw open-palm pose introduces a different elbow/forearm
  surface. Measured visible limb area changes from 1,036 to 1,783 runtime
  pixels at 2→3 (+72.1%), then from 1,785 to 1,084 at 4→5 (−39.3%). The 2→3
  colored-region centroids differ by only about 1.3 pixels: this is primarily
  a shape/occlusion mismatch, not a hand translation that registration fixes.
- A short dark crease remains where the raw forearm meets the constructed
  shoulder in frames 3/4, around runtime x=169..181, y=146..165. The enlarged
  plate makes the different material planes apparent. The smooth mask reduces
  a hard cut but does not establish a believable elbow.
- The cloned belly supplies a coherent outer contour, but its formerly hidden
  surface and local texture join are authored approximations. They have no
  independent anatomical reference or human approval.

The next artwork operation must establish one consistent upper-arm/elbow
surface and forearm thickness through the lift, then turn the hand at a
separate wrist while opening the fingers. More rigid rotations of the entire
neutral limb, whole-cell registration, or mask feathering alone do not solve
that change of visible surface. The retained raw palms remain useful source
material. This report does not claim a model run is necessary.

## Focused verification

The recipe passes checks for eight frames and six distinct moving poses;
exact neutral first/last pixels equal to the existing v002 rest cell; unchanged
hip/foot rows 198..255, upper-head rows 0..99, opposite-body columns 0..149,
and every opaque neutral head pixel above row 142; zero RGB beneath zero
alpha; baseline y=223; and at least 16 pixels of outer clearance. Bounds and
decoded RGBA digests are recorded for every ephemeral frame.

Visible colored-limb area uses an explicit region excluding ears, eye patches
and feet and a documented material/alpha threshold. It measures projected
colored area, not physical volume or motion quality. Adjacent differences use
the union of the two non-neutral regions rather than a whole-cell average
diluted by transparent padding. These checks support the rejection diagnosis;
they are not acceptance tolerances or replacement tests for the asset compiler.

Reproduce from any working directory with Python 3.10+, Pillow and NumPy:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v003/prepare.py
```

The final recipe was repeated and reproduced the exact retained plate/check
payloads. The repository documentation gate passed with `DOCUMENTATION_OK`.
[SHA256SUMS](SHA256SUMS) binds this compact study's retained files. No background
commands remain. No source or preview under v001/v002 was changed; no model,
remote mutation, shared-skill replacement, board movement or commit was made.

The next human checkpoint remains the later complete-catalog review in
RFC-BPR-0030. No partial attended session was created because this attempt
still has unresolved artwork work, rather than only a pending human verdict.
Semantic documentation disposition: no product/API, persistence, architecture,
build, packaging or runtime behavior change.
