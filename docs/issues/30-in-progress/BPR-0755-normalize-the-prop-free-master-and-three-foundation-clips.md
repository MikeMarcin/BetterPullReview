---
schema: "implement-rfc.card.v1"
id: "BPR-0755"
title: "Normalize the prop-free master and three foundation clips"
status: "In Progress"
rfcs: ["RFC-BPR-0030"]
depends_on: ["BPR-0753"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"production-art","acceptance":["A2","A3","A4"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0755: Normalize the prop-free master and three foundation clips

## Outcome

Use the updater to normalize rest, idle and greet; preserve actual artwork, remove accidental idle blink and align articulated motion with the accepted master. Use a focused neutral check before extensive retouch.

## Acceptance

- Master registration and prop-free identity are confirmed against the supplied Dif references.
- Rest/idle representative/greeting endpoints share exact normalized neutral and smooth adjacent transitions.
- No foot hops, whole-raster squat stand-in, fictitious native 512-pixel source, or runtime matte repair.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

The [production-art frontier](../../verification/RFC-BPR-0030-production-art.md)
records the exact 1024-square prop-free neutral candidate, offline recipe,
identity/theme/size plates, hashes and structural checks. Attended-review session
`cbd48b91000b00f1e72e1ad62c7c384c` recorded human **accept** at sequence
`379` for candidate `BPR-0755-neutral-identity-v001`, fingerprint
`1bcdc33cbd9175db6ee843f60f90e3a70e9800470a19562bd4814b18d600ad81`,
decision `6eabb190-287d-4e7c-a19f-bd14f4075701`, with an empty note.
This accepts the exact neutral identity question and permits normalization
against those bytes. It does not accept the rest/idle/greet motion or the final
catalog. The shipped three-clip source is still unchanged at this checkpoint.

The phase-one identity wait is consumed. The card was promoted and claimed on
`main`; remaining idle/greet normalization and final acceptance are open.

The [v002 offline source study](../../verification/dif-production-art-v002/README.md)
retains a canonical 1024-square master with decoded pixels exactly equal to the
accepted neutral, plus a 256-square exact rest and twelve local-breath idle
candidate frames. It verifies alpha, head/feet lock and seam structure, but
actual-speed playback and full art judgment are unproved. The attempted
greeting arm has a visible shoulder seam and misses cheek height. A second
[raw-paw study](../../verification/dif-production-art-v002/rejected-greet-retouch/README.md)
reaches cheek height but still shows shoulder/belly seams, a narrow forearm and
ghosted entry/return. Both are rejected studies, not source. Real greet retouch
needs connected shoulder anatomy, an exposed belly contour and actual
intermediate poses at the accepted master's scale. BPR-0755 remains In Progress;
no source or generated
runtime asset has been replaced. The compiler's schema-2-only zero-alpha
rounding repair and unchanged schema-1 output are covered by focused tests and
the [art ledger](../../verification/RFC-BPR-0030-production-art.md).

The d5 [connected-anatomy study](../../verification/dif-production-art-v003/README.md)
keeps exact neutral endpoints and uses actual raw padded-paw pixels for its
raised poses. Its 64/72/96-pixel light/dark plates and repeatable checks pass
the structural baseline, alpha, fixed-body and clearance screens. It is still
rejected from production source: visible limb area rises 72.1% at frames 2→3,
falls 39.3% at 4→5, and a shoulder crease remains. A consistent upper-arm and
elbow surface through the lift, with separately articulated wrist/fingers, is
needed before this can be a smooth greeting. The v003 study does not alter the
accepted neutral, source inventory or shipped assets. No actual-speed or human
motion verdict exists, and this card remains In Progress.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
