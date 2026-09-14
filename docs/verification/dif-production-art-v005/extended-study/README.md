# BPR-0756 d7 extended input screen

Producer: rfc-worker, dispatch d7, mandate revision 5. **All six native inputs
fail direct promotion.** This screen identifies useful pose material; it does
not qualify a masked composite or a complete sequence. No source, manifest,
runtime asset, card or RFC status changed. The core decides, the host performs,
Qt presents.

## Evidence and reproduction

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v005/extended-study/screen.py
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/verify_documentation.py --root /Users/jaeger/Documents/GitHub/BetterPullReview
```

Python, Pillow and NumPy are required. The script pins the accepted neutral
SHA-256 `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
[checks.json](checks.json) binds all native input and plate hashes, dimensions,
alpha bounds, hidden RGB and full-canvas comparison counts. All six are native
1254-square RGBA with zero RGB beneath alpha zero. All contain substantial
partial alpha; that fact alone does not diagnose bad material. Faint nonzero
alpha extends to the canvas bottom for compare, consider and comment, and to
the left edge for consider, comment and acknowledge. These are not clean
production bounds and must not determine a body-registration transform.

Agent inspected [64](comparison-64.png), [72](comparison-72.png),
[96](comparison-96.png) and [256](comparison-256.png) pixel comparisons on
light `#F6F7F9` and dark `#1C2026`. Each pair shows neutral then candidate.
Thumbnails uniformly fit the entire source canvas; they are deliberately not
independently body-fitted. Metrics compare the input resized to 1024 with the
neutral and measure obvious drift, not an approved registration. Every input
changes tens of thousands of pixels in the lower foot region. Face, feet,
material and head/body proportions cannot be carried across as a full image.
Static plates are not animation or native Qt evidence.

## Per-input findings

| Input | Result | Concrete observation and extraction assessment |
|---|---|---|
| compare-glance-left | FAIL | Viewer-left eye has a large pale sclera wedge on its right, indicating leftward pupil travel, while viewer-right eye retains a central dark oval with no corresponding wedge. The [native eye detail](compare-eye-detail.png) confirms this asymmetry. Both pupils must travel together; an eye-only extraction of this pose would retain the defect. Author the missing right-eye travel and restrain the exposed sclera before strict eye-mask extraction. |
| consider-half-lift | FAIL as half-lift | Correct viewer-left arm, continuous rounded forearm, readable curled paw; no obvious detached limb. But the paw is already at the lower chin/cheek and overlaps the face edge, rather than halfway below it. Retaining the exact head would occlude the upper paw. Potential peak arm reference, not the requested intermediate. Author a lower halfway pose with a stable shoulder and leave clearance below the exact chin. |
| comment-ready-open-peak | FAIL pending extraction | Both paws open palm-up and the action is readable at all sizes. No obvious extra resting paw. Torso narrows and the new arm roots sit close under the head. This is useful two-arm material, but a bounded composite must reconstruct exposed belly at the accepted torso breadth and demonstrate smooth shoulder joins without changing the chin or feet. No local extraction was performed, so it is not yet a pass. |
| attention-chest-paw | FAIL as chest-height action | Correct viewer-right connected arm, readable pads. The paw rises beside the cheek near mouth/nose height, with a fully spread hand; this resembles the higher greeting action rather than the requested restrained chest pose. Lower the forearm/paw through actual elbow articulation while holding the shoulder root. Merely cropping or translating this whole arm will not establish the join. |
| acknowledge-nod-peak | FAIL | Awake eyes and closed smile are retained in intent, but the head appears smaller and lower and the torso/feet also change. The isolated pose does not establish a shallow local head incline rather than scale/translation. A head-only donor might support further study, but needs matched ear/eye/nose landmarks, original head scale, an authored neck join and a shallower preparation pose. Do not accept the whole-body shrink as a nod. |
| blocked-left-paw | FAIL pending corrected head/extraction | Correct viewer-left open paw near belly, fixing the v004 wrong-side issue. The visible ear/eye slope falls toward viewer-right, so the generated head appears tilted toward viewer-right, opposite the specified viewer-left tilt. The arm is a useful bounded donor; retain neutral opposite arm/feet and separately author the required leftward head tilt plus partly opening entry. |

The material is broadly plush and the hand shapes remain legible at 64 pixels.
Unlike the earlier blink transplant, these uncomposited poses do not show the
same horizontal donor bands. That does not establish seam-free extraction:
new mask edges, exposed belly and head/neck joins have not been produced or
inspected. Accepted neutral outside-region/alpha equality remains a required
next check, not a claim from these full-body inputs.

## Disposition

No model call, asset promotion, live service mutation, commit, or attended
verdict occurred. The assigned unattended screen is complete. All six still
need artwork or extraction verification and sequence construction, so none
has only a human verdict remaining. The integrator owns the next bounded
work and eventual complete-catalog attended review.
