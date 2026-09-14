# Raw-paw greeting retouch: rejected study

Continuation of BPR-0755 from committed v002 `f6e7d62d`. This is a rejected
offline layer experiment, not production artwork or an attended verdict.
Only the recipe, [96-pixel light/dark row](rejected-greet-96-light-dark.png), and
[objective measurements](checks.json) are retained. No eight-frame source series,
manifest or runtime resource was written.

The recipe verifies the immutable greeting sheet SHA-256
`1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58`
and canonical master SHA-256
`e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
It extracts zero-based raw frames 2–5 using explicit crop rectangles and local
paw/forearm polygons, with a 0.65-native-pixel boundary feather multiplied by
the original alpha. Uniform 0.54 reduction and offset `(8,-12)` align the limb
with the 256-square master. Fractional raw cells retain their actual native
443/444 dimensions; no larger native-source claim is made.

The formerly occluded right torso is reconstructed only in x=158..211,
y=143..197 using adjacent neutral belly texture, a tapered alpha contour and
a small edge shade. This is explicit cloned-pixel retouch, not recovered source
information. Intermediate entry/return experiments blend only between neutral
and the local limb composite. They are evaluated as possible failure cases,
not claimed as clean articulated movement.

Inspection of the actual 96-pixel light and dark rows found:

- Frames 3–4 reach lower-cheek height and visibly turn the genuine padded paw.
  This remedies the earlier study's absent cheek-height gesture.
- Frames 2–5 fail at the reconstructed shoulder: a pale vertical torso edge and
  inadequate arm-root connection remain visible. The raw forearm is too narrow
  relative to the accepted neutral's larger hand. Uniform body-scale registration
  cannot fix this local anatomical mismatch.
- Frames 1 and 6 show a double/ghost paw. Cross-fading the disparate resting and
  raised silhouettes is not a smooth approach or recovery, even though endpoints
  are exact neutral. These frames are rejected.

Objective checks pass for eight frames, exact neutral first/last pixels, fully
unchanged hips/feet rows 198..255, upper head rows 0..99 and opposite body columns
0..149, zero RGB under zero alpha, y=223 baseline and 16-pixel outer clearance.
The checks record exact raw-RGBA digests for every ephemeral frame plus each
adjacent changed-pixel count. Those facts do not accept the joint or choreography.

A convincing continuation needs a hand-authored connected shoulder/forearm layer
at the master's paw thickness, a shaded exposed belly contour, and actual
intermediate limb poses instead of cross-fades. The raw raised paw supplies useful
pad/wrist artwork but does not supply the hidden neutral torso or the necessary
approach poses at this anatomy. No global squash, generic raised-arm substitute,
geometry relaxation, model call or alteration of protected v001 occurred.
The bounded attempt did not achieve a production greeting. BPR-0755 remains open.

Reproduce the compact study from any working directory:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v002/rejected-greet-retouch/prepare.py
```

No attended session was created. Final human review remains reserved for a
complete normalized catalog. This local image inspection establishes neither
actual-speed motion nor native Qt rendering.
