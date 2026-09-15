---
id: "RFC-BPR-0030"
title: "Dif Production Animation Revision: Ten Registered Prop-Free Clips"
status: "Auto-Verified"
area: "BPR"
milestone: "Dif Integration v1"
depends_on: ["RFC-BPR-0006"]
metadata: {"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# RFC-BPR-0030: Dif production animation revision

## Summary and authority

Create one production-ready, prop-free Dif sprite catalog from the ten supplied
animation images. Preserve the actual source bytes and requested gestures; normalize
inconsistencies before selecting them for the application. The catalog is consumed
by RFC-BPR-0031. This RFC alone does not add widgets or product event semantics.

This is an additive **partial supersession** of RFC-BPR-0019: ten clips and a new
bounded resource contract replace its original three-clip limits for this revision.
The downstream prop-separation note becomes a property of every source frame. It
also makes the source-size and regeneration distinctions in the supporting
reconciliation explicit. Existing settled RFC text and historical verdicts are not
rewritten. The current owner request establishes this workstream's scope, not proof
that the generated images meet their prompts or already passed production review.

The primary implementation baseline is the September 12 source dump named by the
pack's SOURCE-BASELINE record. Verify live path/status drift at import; do not
reconstruct the product from the older September 8 snapshot.

## Problem

The existing asset compiler admits three clips totaling nine frames. The new
conversation supplied ten candidate sheets totaling 71 intended frames, some on
fractional grids and with character drift. Raw sheet cells are not registered
production frames, and blanket acceptance based on image-generation captions would
miss matte noise, chest-band drift, idle blinking, and neutral handoff discontinuities.

The current compiler/build-boundary checks intentionally prevent these images from
shipping. Removing those checks or packing the whole sheet into a QMovie would
exchange a known gap for an unbounded/incorrect implementation.

## Goals and non-goals

Produce transparent 256-square runtime cells, fixed resource strips, one approved
neutral identity, explicit choreography/timing, reproducible pixels, bounded decoded
memory, and reviewable real-speed transitions. Preserve native source artwork and
provenance for later edits. Ordinary builds consume committed outputs only.

Do not redesign Dif, add props/approval badges, reuse the flattened concept boards
as sprites, import another mascot, implement AI/domain state, generate images during
build/startup, or introduce a general animation engine or asset-download feature.

## Incoming artwork and identity

The ten hashes and generation IDs are in `assets/INPUT-MANIFEST.json` in the immutable
pack. Copy them to a BPR-owned intake record and preserve exact bytes in the approved
maintainer source location. Filenames are not identity. Store the actual tool/edit
history, reference inputs and exact derivative hashes; do not claim an unknown model,
seed, license, or operator verdict.

Use the supplied prop-free rest image as the default neutral **candidate**. Confirm
one master against the supplied Dif identity references, then bind the production
revision to that master. No neutral averaging between differing rows. The master is
1024-square RGBA; retained native/edited inputs remain available independently of
normalized frame cells. Rights/derivative/distribution disposition for these exact
bytes is recorded as part of the focused asset acceptance, reusing applicable prior
authority rather than asking the same broad rights question ten times.

Dif remains a compact purple-and-white plush-like panda with the approved eye patches,
ears, limbs, face, proportions, and material. Its base is prop-free. No card/magnifier,
text, warning/check badges, remote repository data, other mascot, baked background,
glow needed for contrast, or floor/contact shadow enters these clips.

## Catalog

| ID | Frames | Duration/frame | Kind | Choreography |
|---|---:|---:|---|---|
| calm_idle | 12 | 250 ms | loop | Three-second local chest/shoulder breath; fixed head/hips/feet; no embedded blink. |
| greet | 8 | 125 ms | one-shot | Viewer-right paw rises beside lower cheek, one small wrist wave, lowers. |
| rest | 1 | 0 | static | Exact awake neutral, no procedural motion. |
| blink | 6 | 100 ms | one-shot | Eyelids close and reopen; fixed head/body/feet. |
| compare_glance | 8 | 150 ms | one-shot | Eyes lead a restrained left/center/right/center head glance. |
| consider | 8 | 150 ms | one-shot | Viewer-left paw touches under chin, slight tilt, returns. |
| comment_ready | 8 | 125 ms | one-shot | Small two-paw presenting gesture, returns. |
| attention | 6 | 150 ms | one-shot | Polite raised paw; alert, not alarmed; returns. |
| acknowledge | 8 | 125 ms | one-shot | One shallow head nod; planted body; returns. |
| blocked | 6 | 150 ms | one-shot | Patient head tilt and upward open paw, returns. |

The pack's `specs/CLIP-CONTRACT.json` carries the detailed frame choreography and
representative indices. It is a design input, not a loadable production manifest.
These IDs/counts/timings are the initial target. A quality-driven frame/timing change
must update the source contract, tests, memory arithmetic and affected visual evidence
rather than quietly leaving the table inaccurate. No alias to idle counts as a clip.

Every one-shot starts and ends in accepted normalized neutral pixels. `rest` and
`calm_idle` representative index 0 use that exact normalized neutral. The final
idle pose must be genuinely distinct, near the start of the cycle; duplicating the
terminal is prohibited. The idle's raw closed-eye frame is repaired rather than
creating an additional blink every three seconds.

## Extraction and production-source layout

Perform creative normalization offline using the installed updater skill and the
BPR-owned consumer contract. Preserve the unmodified source sheets in the intake
area, and record explicit reviewed crop rectangles. For fractional grids, rational
rounding is only a starting proposal. Crops must not discard limbs, duplicate pixels
from neighboring frames, or use per-cell anisotropic resize. Pad rather than stretch.

Retain native editable crops/retouches outside the exact production inventory.
Production sources are square multiples of 256, from 256 through 2048. For this
batch a normalized 256-square frame is acceptable: original 362/443-pixel cells
must not be falsely advertised as native 512-pixel artwork. Use larger genuine
sources when available. Preserve a separate at-least-512 master, targeted at 1024.
The existing integer fixed-box final resize remains the compiler contract.

The live production root remains `src/adapters/qt/assets/dif/`:

```text
README.md
provenance.json
dif-sprite-manifest.json
source/dif-neutral-master.png
source/<clip-name>/frame-NNN.png
generated/dif-<clip-name>.png
generated/dif-sprite-catalog.hpp
generated/dif-sprite-digests.json
```

Maintain the compiler's explicit exact inventory; do not make file scans define clip
order. Store intake/crop/edit history in a separate maintainer tree referenced by
provenance, not as accidental runtime products. No per-frame baseline shifting,
matting, masking, inpainting, or prop removal occurs in the app.

## Registration and matte

Establish visual center x=128 and alpha-bottom-exclusive ground baseline y=223 as
the inherited target. Confirm the accepted master once; if the silhouette requires
a different global box, record it consistently instead of hard-coding false bounds.
No frame touches the 16-pixel outer clearance. Feet stay within one runtime pixel
of the baseline unless a separately reviewed intended motion interval says otherwise;
none of the initial ten gestures requires foot travel.

Separate a stable **body anchor** from the **content envelope**. The old alpha-box
center rule is not body registration when an asymmetric paw expands the silhouette.
For the new schema, validate reviewed body/foot/head anchor facts and declare a
per-clip enclosing content box and intended limb/head travel. Do not globally enlarge
x travel to excuse accidental body drift. Exact allowed geometry is frozen from the
normalized frames and tested with deliberately displaced-foot/body fixtures. Landmark
annotations are not sufficient proof on their own: verify they correspond to visible
body/foot pixels and compare unchanged masked regions where the choreography is body-
locked. Review overlay images and targeted transition differences; a low whole-cell
mean diluted by transparent padding must not hide a visibly discontinuous head or paw.

For blinks preserve unchanged body pixels; for nods rotate/deform the head rather than
squashing the entire character; for breath use local chest/shoulder changes. Shared
endpoints need adjacent-frame motion review: a byte-exact final neutral does not excuse
a jump from an incompatible preceding frame. No fake smoothness from broad seam tolerances.

All production frames are RGBA8 sRGB with straight alpha, canonical allowed PNG chunks,
and zero RGB beneath zero alpha. Preserve antialiasing. Remove detached remnants and
boundary lines with reviewed masks, not indiscriminate erosion or a guessed chroma key.
Validate light/dark/high-contrast composites using the actual RGBA, not preview captions.
Interior near-opaque alpha is not automatically a defect; material/edge appearance
must remain stable across themes.

## Manifest and deterministic compiler

Advance the production manifest/catalog schema together to version 2. Admit a closed,
ordered ten-ID catalog. Emit compile-time descriptors with frame count, duration,
representative index, kind, priority, return target, cell/resource geometry and checksums.
Keep unknown IDs, duplicate sources, invalid dimensions, out-of-range frames, bad return
targets, malformed bounds, arithmetic overflow and excess resource use fatal.

Each generated strip has one row of `(frame_count + 2)` 256-square cells: one completely
transparent leading cell, content left-to-right, one trailing cell. Runtime content
frame `i` begins at x=(i+1)*256. Review sheets contain no runtime padding; the compiler
adds it. Never display gutters as frames. Every declared content frame is referenced.

One-shots declare neutral return to `calm_idle`; the Qt surface policy in RFC-0031 may
stop at neutral instead of running idle in review context. `rest` is static and cannot
arm a deadline. Prohibit cyclic one-shot chaining. The manifest is not read at runtime.

Extend **the existing** `scripts/dif_assets.py`, tests and provenance inventory. Do not
build a competing generator. Offline art intake may use Pillow/ImageMagick/editor tools;
the shipping compiler stays offline/noninteractive and ordinary builds select neither
creative tools nor their dependencies.

Preserve the settled BPR-0378/BPR-0408 regeneration policy: compare decoded canonical
RGBA and deterministic metadata exactly, compare text exactly, and verify committed
output hashes against actual committed compressed payloads. Do not demand that zlib
and zlib-ng emit identical compressed PNG bytes. Pixel equality and byte identity for
review/distribution are different assertions, both checked where applicable. Changed
committed art bytes require the affected review identity to be refreshed, not falsified.

## Resource contract

Each 256-square RGBA cell is 262,144 bytes (0.25 MiB). Initial content is 71 cells
(17.75 MiB); twenty end gutters add 5 MiB; complete decoded strips are **22.75 MiB**.

Hard limits for this revision: ten clips; 76 content frames total; 24 frames/clip;
12 frames/second maximum; 19 MiB content pixels; 24 MiB complete decoded-strip pixels;
2048 source dimension; 16 MiB/source file; 24 MiB/generated file. A count change still
requires a reviewed revision. Bounds are enforcement, not targets to fill.

The Qt implementation separately measures image/cache/container/GPU overhead. The pixel
ceiling is not a claim that resident process memory rises by only that much. Do not
retain full parallel QImage and QPixmap catalogs without accounting for both.

## Asset acceptance and failures

Use two compact phases of one source-revision review: confirm the neutral identity
before extensive retouch, then review the complete normalized catalog, identity
consistency, actual 64/72/96-pixel playback, light/dark/high-contrast edges, loop seam,
all one-shot entry/return transitions and static rest. Existing relevant decisions
can be reused only for unchanged facts/bytes. A single reviewed contact sheet is not
motion proof. Never manufacture a dated acceptance or rights grant.

If art needs repair, park that exact pose/task and continue independent Qt work with
clearly marked fixtures. A raw frame, old card-bearing master or app icon is not a
shipping fallback. The final catalog cannot claim all-ten completion while any clip
is placeholder, unmapped, unapproved or structurally invalid.

## Acceptance criteria

- **A1 — Exact intake.** All ten immutable inputs, actual layouts, hashes, source/edit
  history and new derivative authority/disposition are recorded; no opaque concept-board
  crop, foreign mascot or guessed generator metadata is shipped.

- **A2 — One character.** The prop-free master and all ten sequences preserve one identity,
  resolve the documented drift, and have accepted neutral, body anchors and foot baseline.

- **A3 — Complete choreography.** Ten real distinct sequences have the target contracts,
  closed idle seam, intended eyelid/limb/head motion and clean neutral handoffs; no static
  alias or whole-raster squash substitutes for a gesture.

- **A4 — Canonical geometry/alpha.** Every final cell and both gutters per strip meet exact
  format, clipping, inventory, anchor, envelope and transparent-pixel checks. Adversarial
  drift/empty-frame/padding fixtures fail for the intended reason.

- **A5 — Reproducible catalog.** Empty-output regeneration reproduces canonical pixels and
  metadata, with exact committed payload digest checks and no compressor pin regression.
  Generated descriptors/resources agree and malformed/unknown/excessive catalog input fails.

- **A6 — Measured bounds.** The actual catalog stays within the revised limits; arithmetic
  includes padding and uses checked dimensions/counts. Ordinary builds neither generate
  art nor acquire creative dependencies/network resources.

- **A7 — Bound visual decision.** Exact normalized bytes, timing and preview renderer are
  covered by the focused identity/motion/theme/scale/handoff/rest review and derivative
  disposition. Any changed subject is re-reviewed only to its necessary scope.

- **A8 — Handoff and history.** Mutable docs and live asset/verification ownership identify
  this new revision; historical decisions remain unchanged; RFC-0031 consumes the accepted
  new catalog. Existing prerequisites/formal status waits are explicit, not falsely closed.

## Verification and implementation

Use the pack's acceptance matrix and authored cards 01–06. Focused tools validate
identity-independent facts; attended review validates visual identity/motion. Run the
existing asset contract plus its extended cases once against the integrated candidate,
and reuse it downstream if bytes/tool policy are unchanged. Tests-disabled resource
selection and full application reachability belong to RFC-0031.
