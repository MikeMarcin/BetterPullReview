# Extended gestures: rejected offline studies

Producer: `dif-takeover-20260914`, d6 worker continuation. BPR-0755 remains
In Progress; this is preparatory BPR-0756 evidence, not completion or a visual
acceptance. The core decides. The host performs. Qt presents.

[prepare.py](prepare.py) derives all 50 extended raw poses from immutable sheets,
checks their source and v002 native-crop digests, and retains exact fractional-grid
rectangles in [checks.json](checks.json). Native crops remain reproducible from
those immutable bytes; no duplicate native files or fictitious upscaled sources
are stored. No model calls, external mutations, runtime edits or attended session
occurred. No new rights or human verdict is asserted.

Registration uses one isotropic scale per clip, matching frame-0 head width to
the accepted neutral, then translates each frame using the lower foot support
center and bottom. Alpha above 128 determines these *provisional* anatomical
measurements: head is the top 53% of foreground height; foot support is the bottom
9%. Moving limb envelopes never determine center or scale. The transforms are
explicit diagnostic proposals, not accepted body landmarks. Low-alpha remnants
are retained and measured, not silently erased. Bicubic affine sampling and zero
hidden RGB are the only raw pixel processing.

The accepted v001 master digest is
`bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
Its fixed-box reduction is checked byte-exact against v002 rest. Every handoff
study copies its declared neutral frames from that reduction. Separate
`*-registered-raw.png` rows retain all original poses before endpoint replacement.
These diagnostic rows have no runtime gutters and are not production strips.

## Findings

All seven full-body clips are **rejected**. Static 64/72/96 light/dark plates were
inspected locally, as were the 256-pixel blink rows. Distinct pixel frames prove
non-aliasing only; they do not prove correct choreography. Seam counts below are
exact changed-pixel counts at entry/return, without transparent-area averaging.

| Clip | Distinct frames | Entry / return pixels | Maximum changed hip/foot pixels | Specific rejection |
|---|---:|---:|---:|---|
| blink | 4 | 27670 / 0 | 2437 | Mouth/chest/paw change, body shrinks; raw frame 3 stays closed rather than reopening. Open tail is exact neutral. |
| compare_glance | 7 | 29398 / 28125 | 3283 | Large head yaw and open mouth; smaller body and strong crop-edge contamination. |
| consider | 7 | 28063 / 28018 | 2572 | Real chin-touch survives, but head/torso proportions and resting paw jump against neutral. |
| comment_ready | 7 | 28167 / 28323 | 2582 | Real two-paw opening; narrow torso and small feet pop at handoff. |
| attention | 5 | 27606 / 27512 | 2538 | Raised paw survives; body/feet shrink and face shifts on entry. |
| acknowledge | 7 | 27696 / 27366 | 2509 | Head compression and closed-eye expression persist; not the required shallow local nod. |
| blocked | 5 | 28791 / 28491 | 2660 | Real tilt/open palm, but viewer-right raw paw contradicts detailed viewer-left contract; torso/feet change. |

Hip/foot region is runtime rows 198..255. Every raw interior frame changes it.
Alpha-bottom ranges include values up to 232, despite aligning opaque foot support:
this exposes matte remnants below feet rather than certifying baseline compliance.
All 50 provisional native crops have boundary-alpha counts recorded; many have
nonzero boundary pixels, and compare_glance has 895–1348 counted edge pixels per
crop (corners counted twice). Crop cleanup still requires reviewed masks.

## Bounded local blink repair attempt

[blink_local.py](blink_local.py) transfers raw registered frame 1 half lids and
frame 2 closed lids onto the neutral eye interiors, then reverses the half-lid
pose: neutral / half / closed / half / neutral / neutral, six 100-ms frames.
It uses two explicit rectangular regions with a feathered ellipse mask, retained
in `blink-local-eye-mask.png`; the source translations and hashes are recorded in
[blink-local-checks.json](blink-local-checks.json). It preserves neutral alpha
exactly, including material alpha; all pixels outside those eye rectangles are
byte-identical to neutral. This is real raw eyelid extraction, not an idle alias
or whole-character squash.

**Rejected:** the closed-frame patch retains a pale lower-eye sliver and an abrupt
lid/material boundary visible at 256 pixels. The small 64/72/96 theme plates show
stable body and feet, but do not excuse that defect. A local illustrator repair
must reconcile lid shape/material within the locked eye patches. The remaining
six gestures need separately scoped local illustration; another model run is not
authorised by this study. No complete-catalog candidate is ready for human acceptance.

## Reproduction and limits

Run from any working directory with Python 3.10+, Pillow and NumPy:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v004/extended/prepare.py
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/dif-production-art-v004/extended/blink_local.py
```

[The diagnostic player](index.html) presents the actual rows at 64/72/96 pixels
on built-in light `#F6F7F9` and dark `#1C2026` surfaces, with manual stepping and
RFC one-shot timing. It is not an attended-review packet and collects no verdict.
Browser-observed playback and native Qt behavior were not verified. The PNG plates
are direct raster evidence only. Repeated generation verified exact artifact
payload equality with this installed Pillow/compiler environment; this does not
impose cross-compressor PNG byte equality on the shipping compiler.

Semantic documentation disposition: no change to public APIs, architecture,
persistence, UI, authentication, builds or packaging. Production compiler,
gutter, resource and attended acceptance gates remain outside this rejected
source study. No background commands or processes remain running.

## One bounded repair continuation

At the integrator's direction, [blink_repair.py](blink_repair.py) attempts one
additional local repair. It keeps the raw half/closed/half trajectory but authors
a curved lid, using the raw closed-lid pixel at registered (99,113) for its stroke
color and accepted neutral same-row adjacent eye-patch material to cover the iris.
No claim is made that this new lid geometry is an unchanged native crop. The
four-times local drawing surface provides antialiasing only, not source quality.
Exact neutral alpha and outside-eye equality are enforced after reduction.

`blink-repair-eye-study.png` is a **provisional repair candidate**, not accepted
art. Agent inspection at 256 and 96 pixels found the pale lower-eye sliver removed,
a readable curved closed lid, and a stable body. Same-row donors leave faint
horizontal texture bands at 256 pixels. The integrator inspected the repair and
retained it as provisional: the bands are visible at 256 but were not an obvious
blocker at 96. This is engineering selection, not human acceptance. The
first local extraction and all seven full-body studies remain rejected. The
candidate's actual 64/72/96 light/dark plates, mask and pixel checks are retained.
Run the same absolute path command shown above with `blink_repair.py` after
`prepare.py`. No attended session or human decision was fabricated; the integrator
owns candidate selection and the eventual complete-catalog review.
