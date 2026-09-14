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

The following prompts make each missing **peak** state concrete. For every
call, image 1 is the exact accepted neutral named above and image 2 is the
matching immutable incoming sheet named in the hash table. Use the sheet for
action intent only, then author separate preparation and return states using
the accepted peak; a peak alone cannot satisfy the clip contract.

**E1 — compare glance, viewer-left eye state:**

```text
Use case: precise-object-edit. Make one transparent full-body Dif compare-glance pose, not a sheet. Image 1 is the exact accepted neutral and is the identity/registration target. Image 2 is the immutable dif-compare-glance.png sheet and supplies only the notion of a small look around; do not copy its open mouth, wide head turn, body or feet.
Move both pupils together modestly toward viewer-left inside their existing eye patches, about one pupil radius from neutral. Preserve each eye's roundness, highlight, eye-patch boundary and fine plush texture. Keep the gaze readable at a 64-pixel presentation. The head remains almost frontal; if any head turn is needed, keep it under approximately 2 degrees with the same head silhouette, chin and translucent edge. Do not move the shoulders, forearms, torso, hips or planted feet.
Exactly preserve Image 1's closed smile, nose, ears, body size/center, baseline, opposite gaze-neutral frame identity and transparent background. No blink, talking mouth, extra eye, duplicated face, shadow, prop, text, cursor, code or badge. Return one pose only. Non-eye/head drift will be discarded locally.
```

For the viewer-right glance, repeat E1 with `viewer-left` replaced by
`viewer-right`; use the accepted neutral as image 1 again. The center and
return states must be authored or copied with pupil travel that eases across
all eight 150-ms frames, ending in the exact neutral.

**E2 — consider, halfway connected lift:**

```text
Use case: precise-object-edit. Make one transparent full-body Dif considering transition. Image 1 is the exact accepted neutral identity target. Image 2 is immutable dif-consider.png, for a thoughtful paw-lift idea only; reject its smaller body, different resting paw, torso gap and altered face.
On the panda's right arm (viewer-left), lift the same attached purple plush paw halfway from the lower-belly rest toward a gentle under-chin touch. Keep one rounded shoulder root fixed to Image 1, with an actual continuous upper arm, elbow and forearm; show no floating cutout, sharp seam, thin stick limb or duplicated resting paw. The hand stays small and softly curled, pads barely visible. Reconstruct only the small belly surface genuinely uncovered by this lift. The other arm and both feet remain as in Image 1.
Do not yet touch or cover the chin, nose or closed smile. Keep head angle, face, ears, translucent edge, body scale/center and baseline from Image 1. One character, transparent RGBA, no bubble, text, prop, shadow or extra object. Output one pose, not a row; non-arm drift will be excluded during extraction.
```

For the peak, edit the accepted neutral again with the **qualified** E2 as a
third reference. Continue that exact shoulder/elbow/forearm to a light
under-chin contact, at most about 2 degrees of head tilt and a small upward
gaze. Retain an uncovered smile; then create distinct return art rather than
flipping or fading a mismatched limb.

**E3 — comment ready, two-paw open peak:**

```text
Use case: precise-object-edit. Make one transparent full-body Dif comment-ready presentation pose. Image 1 is the exact accepted neutral target. Image 2 is immutable dif-comment-ready.png, action guidance only; its narrow torso, smaller feet and handoff jumps are defects.
Lift both original purple plush paws a little from their lower-belly rests, keeping their left and right shoulder roots fixed and both elbows visibly connected. Open the paws gently palm-up below the chest/face, as a restrained invitation to comment. Keep the two hands symmetric in intent but naturally shaped, with small pink pads, continuous forearm thickness and no duplicate lower resting paws. Expose and reconstruct only the belly fabric that lifting the arms reveals; retain torso breadth and texture from Image 1.
Lock Image 1's entire face, closed smile, head/translucent edge, ears, scale, center, hips, legs, planted feet and baseline. The hands may not obscure code or carry anything. One pose and one character on transparent RGBA; no note, pen, speech bubble, badge, text, cast shadow or background. Any full-body drift is unaccepted.
```

Author a separate half-lift with both paws still mostly curled, and a closing
state with the same arm volumes. Frame 0 and 7 remain exact neutral.

**E4 — attention, connected chest-height paw:**

```text
Use case: precise-object-edit. Make one transparent full-body Dif attention pose. Image 1 is the exact accepted neutral target. Image 2 is immutable dif-attention.png, used only for the small one-paw attention cue; its face, body and feet must not replace Image 1. If supplied as Image 3, the qualified G1 greeting bridge shows connected-arm material and a partly uncurling hand, not the required higher greeting action.
Raise only the panda's left arm (viewer-right) to chest height, below the cheek. Keep the shoulder rooted at Image 1's same body point, a broad soft upper arm and elbow continuously connected, and a small paw turned gently forward with partially visible pink pads. The cue is quiet and capable, not a point or alarm. Maintain forearm volume and pad articulation consistent with a separate curled half-lift and return pose.
Lock Image 1's head silhouette, eyes, smile, opposite arm, belly outside the exposed arm join, hips, feet, center, scale and baseline. One original body, no duplicated paw or straight shoulder cut. Transparent RGBA; no flash, bug, exclamation, symbol, prop, text, halo or shadow. Any generated non-arm drift is excluded locally.
```

**E5 — acknowledge, shallow nod peak:**

```text
Use case: precise-object-edit. Make one transparent full-body Dif acknowledgement nod peak. Image 1 is the exact accepted neutral target. Image 2 is immutable dif-acknowledge.png, action guidance only; its closed-eye grin and whole-body compression are not approved.
Incline the same plush head about 3 degrees downward at its local neck connection, a single polite nod. Preserve both awake eyes, eye patches, nose, ears and closed smile, with natural small neck fabric adjustment and continuous head alpha; do not squash, shrink or translate the whole mascot. Shoulders, paws, torso, hips and both feet stay in Image 1's exact registration and baseline. A second prep pose should be shallower so the eight 125-ms frames enter, rise and settle without a snap.
Return one transparent pose of one character. No happy-eye crescent, talking mouth, oversized bow, halo, check mark, medal, confetti, text, prop, shadow or background. Generated pixels outside the carefully bounded head/neck motion remain unaccepted.
```

**E6 — blocked, viewer-left open-paw peak:**

```text
Use case: precise-object-edit. Make one transparent full-body Dif blocked-state pose. Image 1 is the exact accepted neutral identity target. Image 2 is immutable dif-blocked.png, used only for the patient pause/tilt idea. Its dominant open paw appears on the wrong side: do not copy it. The contract requires the panda's right paw (viewer-left) to open near the belly.
Tilt the same head about 3 degrees toward viewer-left without changing its face or scale. Near the lower belly, open only the viewer-left paw gently toward the user, with a small pink palm pad, continuous purple forearm and rounded attached shoulder. Keep the other paw resting as in Image 1. Author a separate partly opening entry state; do not snap from a fist to a fully splayed hand or change the limb area abruptly. The expression is calm and capable, never helpless or angry.
Preserve Image 1's closed smile, awake eyes, head-edge transparency, torso width, belly fabric outside the local arm join, hips, planted feet, center and baseline. One transparent character/pose; no tears, sweat, broken item, question mark, exclamation, text, extra paw, prop or cast shadow. Exclude generated full-body drift when composing frames.
```

**B1 — clean closed blink lid.** This is a localized correction to the
provisional v004 blink, independent of the greeting. Input image 1 is the
accepted neutral edit target; image 2 is the
[v004 repaired blink row](../dif-production-art-v004/extended/blink-repair-eye-study.png)
for the closed-eye action in its third panel only. The previous local repair
retained faint horizontal donor bands at 256 pixels:

```text
Use case: precise-object-edit.
Asset type: one transparent Dif closed-eye blink pose, not a row or sheet.
Input images: Image 1 is the exact accepted awake neutral edit target. Image 2 is a six-panel diagnostic reference; only its THIRD panel (index 2, gently closed eyes) shows the intended relaxed blink. Its texture bands are a defect, not a style instruction.
Primary request: Change only the interiors of the two existing purple eye patches in Image 1. Close both eyes softly with matching smooth curved upper lids and a fine natural dark lid crease, like one calm blink. Hide both irises and highlights completely for this single frame. Fill the formerly visible eye areas with the same purple plush material and continuous fine texture as the neighboring untouched eye patches; eliminate horizontal rectangular donor bands, pale lower-eye slivers and hard patch edges. Keep the eyes centered in their existing patch shapes. Do not change the surrounding face, cheeks, nose, closed-mouth smile, ears, head silhouette or head alpha.
Invariants: leave Image 1's entire body, arms, belly, feet, scale, baseline, camera, lighting and transparent background unchanged. One full-body pose, one character; no squinting happy crescents, raised cheeks, mouth opening, head movement, props, text, shadows, halos or extra objects. Output genuine transparent RGBA. Any non-eye drift will be discarded during local extraction onto the exact neutral.
```

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
