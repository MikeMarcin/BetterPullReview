# Dif production pose-edit prompts — d7 candidate brief

Run `dif-takeover-20260914`, dispatch `d7`, producer `rfc-integrator-d4`,
mandate revision 5. The user has no hand-authored poses and requested detailed
prompts; scoped offline image editing was authorized in d6. These prompts are
**candidate-making instructions**, not accepted images, model provenance,
permission to replace the master, or a substitute for the final art verdict.
Use one image-edit call per pose, inspect each result, and retain the exact
prompt, reference roles, native output bytes/dimensions and hash. Do not run
generation in ordinary builds or the app.

## Locked sources and common instruction

- **Edit target:** the exact accepted prop-free neutral
  `docs/verification/dif-production-art-v001/dif-neutral-master-candidate.png`,
  SHA-256 `bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
  The human accepted its neutral identity only in session
  `cbd48b91000b00f1e72e1ad62c7c384c`, event 379. The pixel-equal
  compiler-canonical master has a different PNG digest
  `e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
- **Gesture reference:** the matching immutable incoming sheet under
  `docs/rfcs/assets/bpr-dif-integration-v1/assets/incoming/`. Its action is
  useful; its open mouth, differing paws/body size, chest band, matte specks
  and crop boundaries are not approved. The
  [v002 crop inventory](../dif-production-art-v002/crop-inventory.json) records
  actual fractional-grid rectangles and hashes.
- The immutable sheet identities below come from the pack's
  `assets/INPUT-MANIFEST.json`; inspect the actual indexed poses in the v002
  contact/inventory before selecting a reference cell. A sheet hash does not
  imply its crop or visual quality was approved.

| Clip | Exact incoming PNG SHA-256 |
|---|---|
| greet | `1e740dad2a7f4a9f6465421a537d3ce3142525f8dfb2f9722560778c292c0b58` |
| blink | `e67c8220ec8cd094d16272661b1358ee3f1d3b23ed0dcba019d12abf3424b6a0` |
| compare_glance | `2838d50c8d6b3bec0f4300f03e439f07b3e8e812aaf0ac4bb2805d3f3e7bdff1` |
| consider | `e7bcad4b7d8be15e7ff5a85663fbbce165a06161cdac883cb7202c2d06ade1d7` |
| comment_ready | `212929b9b44bd66c587965a2cbf7f7ee4b7529224554dabcb656fa9212eb8865` |
| attention | `4cff8d667eebcab8cf642f32481a39c377841f942a150ec07a76d0e16924e710` |
| acknowledge | `0b426c9cd9633d22a76143181afa2f7c1f6278940d00cdf1fb451c843dca90ef` |
| blocked | `e0d6cba0d111af520d47f5437088882d8e386b9f1eb947ed9609e0f067b8d542` |

- **Known corrected arm reference:**
  [v004 open-paw intermediate](../dif-production-art-v004/generated-correction/open-paw-intermediate-fullbody.png),
  SHA-256 `71aed58a2c88fec60e76ca00d314c55cfe65c1dd539ebd42e47b010930fd347f`.
  It shows pads and a connected forearm, but its full-body identity drifts.
  The [v004 study](../dif-production-art-v004/README.md) still fails the
  closed-to-open hand change, shoulder join and full head-edge lock. It is a
  reference, never a source frame by itself.

For every pose below: keep one compact purple-and-white plush Dif, the
accepted neutral's closed-mouth smile, eyes, eye patches, ears, head scale and
location, opposite arm, torso, hips, planted feet, baseline and frontal soft
material/light. Preserve the same subject scale and center as the 1024-square
neutral, with transparent background and no cast/contact shadow. Do not add
props, text, badges, code, symbols, duplicate paws, other mascots, floating
objects, or a new expression. State explicitly which small body region may
change. The image tool may still return a different canvas or drift elsewhere:
retain it as a native candidate and locally extract only approved moving
regions onto the exact accepted neutral. Do not claim a generated full-body
result is pixel-locked or upsample it to claim native detail. The final 256
runtime cell must have baseline alpha-bottom-exclusive y=223, visual center
x=128, at least 16 pixels of edge clearance, zero RGB under alpha zero,
one blank gutter at each strip end, and exact one-shot neutral endpoints.

## Greeting: the two missing source states

The eight 125-ms frames are neutral / small curled lift / **partly uncurling
bridge** / **fully open lower-cheek peak** / small wrist return / partly
lowered open paw / nearly neutral curl / exact neutral. The generated v004
frame 1→2 jumps from a closed fist to an open palm (+17.1% projected limb
area); its raised shoulder cuts sharply across the torso and 15–103
translucent head-edge plus 12–16 lower-chin pixels change. A new frame must
change finger articulation gradually while keeping the shoulder attachment
fixed. Do not solve those defects by feathering, translating the whole arm,
or matching an aggregate area number alone.

**G1 — partially opening bridge.** Input image 1 is the accepted neutral edit
target; image 2 is the v004 corrected open-paw reference; image 3 is immutable
incoming `dif-greet.png`, pose guidance only. Copy this prompt for a single
edit call:

```text
Use case: precise-object-edit.
Asset type: one transparent Dif greeting intermediate pose, not a sheet.
Input images: Image 1 is the exact accepted neutral edit target. Image 2 shows the intended connected purple arm and open pink pads but is unaccepted full-body reference. Image 3 is the original greeting motion reference only; do not copy its open mouth, altered proportions, background artifacts or resting hands.
Primary request: On the panda's left arm (viewer-right), create the missing halfway transition between the accepted curled resting paw and the small fully open greeting paw. The shoulder stays rooted under the same lower-cheek point of Image 1. Raise a thick, rounded upper arm and forearm toward upper-chest/lower-cheek height with a softly bent elbow; make the hand only PARTLY uncurl. Two or three small pink finger-pad hints and a partly visible palm pad may appear, but the fingers are neither a closed round fist nor a fully splayed wave. Keep the paw breadth and forearm thickness continuous with both neighboring states. Preserve a rounded plush shoulder surface with no straight cut, crease, detached blob, second paw or abrupt material-plane change. Keep the arm behind the exact head silhouette, never covering its face.
Invariants: preserve Image 1's exact visible head shape, eyes, mouth, opposite arm, belly silhouette, hips, both planted feet, camera, center, scale and soft purple/lavender-white material. Change only the viewer-right arm and the small belly surface genuinely exposed by its lift. Do not change the lower chin or translucent head edge. Output a single full-body 1024-square pose on genuinely transparent RGBA with clear margin; if the tool emits another native size, do not pretend it is 1024 detail. No props, text, background shadow, halo or watermark.
```

**G2 — shoulder-fixed open-paw peak.** Use accepted neutral as edit target,
G1 (if it passes) as the transition reference, and the v004 corrected open
paw as pad reference. This is a separate pose call:

```text
Use case: precise-object-edit.
Asset type: one transparent Dif greeting peak pose, not a sheet.
Input images: Image 1 is the exact accepted neutral edit target. Image 2 is the inspected partially opening bridge; preserve its shoulder, elbow, forearm volume and body registration. Image 3 is the v004 corrected open-paw input for pink pad shape only; do not inherit its changed full body.
Primary request: Continue the same viewer-right arm from Image 2 to a small relaxed wave beside the lower cheek, below the ear and within the head-width envelope. Keep the shoulder root fixed in place under the cheek and the rounded upper-arm/elbow continuous as the forearm lifts. Open the same paw fully toward the viewer, with three small finger pads and one larger palm pad. Rotate only the wrist slightly outward; do not translate or scale the whole arm to fake a wave. Its apparent paw breadth and forearm thickness must stay compatible with the partially open bridge.
Invariants: retain Image 1's head, all facial pixels and translucent edge, closed smile, opposite arm, torso outside the exposed shoulder/belly join, hips, planted feet, scale and baseline. One character, one paw, one pose, transparent RGBA, no text, prop, effect, cast shadow, extra object or camera change.
```

The small inward wrist pose at frame 4 should be a further **wrist-only** edit
of an accepted G2 output, with the same shoulder/elbow/forearm and paw size;
frames 5 and 6 must lower/close in reverse with distinct real art, not a
cross-fade of incompatible silhouettes. Frames 0 and 7 must be copied from
the accepted normalized neutral, not generated.

## Extended gestures: peak and transition prompts

For each clip, set image 1 to the exact accepted neutral edit target and image
2 to its immutable incoming sheet as **action-only** reference. After a peak
passes identity/locked-region inspection, use it with the neutral as references
for the neighboring entry/return poses, preserving the same anatomy and
material. The frame count and timing below come from
`specs/CLIP-CONTRACT.json`; one generated peak is not an entire clip.

| Clip | Required authored state(s) and copyable edit direction | Specific raw/v004 defect to avoid |
|---|---|---|
| **blink** — 6×100 ms | Edit only the two eye interiors: one half-lowered upper lid and one gently closed curved lid, then reverse them. Keep eye patches, head alpha, face, body and feet exact. The v004 repaired blink can guide the relaxed curvature. | Remove its faint horizontal donor-texture bands at 256 px without a pale lower-eye sliver, rectangular lid stroke, cheek lift or happy-eye crescent. Frames 0, 4 and 5 are copied neutral. |
| **compare_glance** — 8×150 ms | Move pupils a little viewer-left/center/viewer-right/center; any head yaw is at most about 2°. Keep the mouth closed and shoulder line stable. Generate left and right eye/head states separately so return frames can ease through center. | Raw large head yaw, open mouth, altered paws/body and crop-edge matte must not enter. Frames 0 and 7 are copied neutral. |
| **consider** — 8×150 ms | On the viewer-left, lift one connected paw to just beneath the chin, lightly touch without covering nose/mouth, and tilt the head about 2° with a small upward glance. Generate halfway lift and chin-touch states with continuous shoulder/forearm; reverse on return. | Raw small body and different resting hand, torso gap and head/foot pop. Do not add thought bubbles or change the smile. Frames 0 and 7 are copied neutral. |
| **comment_ready** — 8×125 ms | Let both accepted paws lift a little from the lower belly, then open gently palm-up below the face as a restrained presentation. Generate preparation, open peak and closing states with stable shoulder roots and reconstructed exposed belly. | Raw narrow torso/small feet and handoff jumps. No note, pen, speech bubble, code, author badge or talking mouth. Frames 0 and 7 are copied neutral. |
| **attention** — 6×150 ms | Lift the viewer-right paw only to chest height, pads angled gently forward, with an alert but calm gaze. Generate an intermediate curled lift and a connected open-paw peak; lower in reverse. The greeting bridge may guide arm material but this is a smaller action. | Raw face/body/foot shift and the v004 fist-to-open snap. No alarm, red flash, bug, exclamation or accusing point. Frames 0 and 5 are copied neutral. |
| **acknowledge** — 8×125 ms | Author one shallow head nod: preparation, about 3° downward head incline, rise and settle. Keep neutral eyes and closed smile; rotate/deform only the head and its local neck join, never squash or move torso/feet. | Raw whole-body compression and closed-eye smile are not the required nod. No check, medal, confetti or second nod. Frames 0 and 7 are copied neutral. |
| **blocked** — 6×150 ms | Tilt the head about 3° toward viewer-left, and open the **viewer-left** paw near the belly with a continuous attached arm. Hold briefly, then straighten and close. Keep a patient, capable expression. | Raw sheet puts the prominent open paw on viewer-right, contrary to the clip contract; do not copy that side. No tear, sweat, anger, broken object, question mark or helpless shrug. Frames 0 and 5 are copied neutral. |

## Candidate checks before any promotion

Inspect each actual 64/72/96-pixel pose and timed transition over real light
`#F6F7F9`, dark `#1C2026` and high-contrast `#000000` surfaces. Compare
against exact neutral endpoints and neighboring poses, particularly paw
opening, shoulder root, head alpha/chin, exposed belly, planted feet, matte
and the idle seam. Record per-frame decoded hashes, exact locked-region
equality, content bounds, alpha/hidden-RGB, baseline, body anchors, edge
clearance and adjacent local differences. Reject any static alias, full-body
squash, source-quality claim based on upscaling, or smoothness claim based only
on whole-cell mean/area. Keep raw, prompts, generated inputs, local masks,
normalized sources and review media separate. Only after all ten real clips
and 71 content frames pass objective/agent inspection should the exact
source/manifest/generated digests enter one fresh Dif-only attended human
art/derivative review; preserve historical shared-session decisions.
