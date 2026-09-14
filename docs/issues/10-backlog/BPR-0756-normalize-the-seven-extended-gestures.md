---
schema: "implement-rfc.card.v1"
id: "BPR-0756"
title: "Normalize the seven extended gestures"
status: "Backlog"
rfcs: ["RFC-BPR-0030"]
depends_on: ["BPR-0755"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"production-art","acceptance":["A1","A2","A3","A4"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0756: Normalize the seven extended gestures

## Outcome

Author actual blink/glance/consider/present/attention/nod/blocked sequences from their incoming sheets. Fix the chest-band/neutral/proportion drift only where needed and preserve gesture intent.

## Acceptance

- Every extended clip is a distinct real sequence with the declared action and timing.
- Body/paw/face continuity, clean alpha and registered feet survive entry, peak and return.
- Native source and any retouch provenance remain available; no alternative mascot or idle alias is substituted.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

The d6 [offline seven-gesture study](../../verification/dif-production-art-v004/extended/README.md)
checks the 50 immutable raw cells and their exact provisional crops, then
inspects all seven registered clips at 64/72/96 pixels on light/dark surfaces.
Every whole-body raw registration is rejected: face, body and feet jump against
the accepted neutral, with matte remnants and crop-edge contamination. A
bounded **local-eye blink repair** preserves the neutral alpha and every
pixel outside the eye patches, removes a pale sliver, and has a readable curved
closed lid at 96 pixels. It remains a provisional source study with faint
texture bands at 256 pixels and no actual-speed or human verdict. The other
six gestures still need local anatomy/matte/handoff retouch. BPR-0755 remains
In Progress, so this dependency-bound card remains Backlog and no extended
source or catalog is accepted.

In d7, the user requested detailed pose prompts because no hand-authored
source poses are available. The integrator retained the exact-source
[v005 prompt brief](../../verification/dif-production-art-v005/PROMPTS.md)
for all seven extended gestures and generated one separate, traceable edited
input for each of six non-blink peaks/transitions plus a closed-blink input;
the [input ledger](../../verification/dif-production-art-v005/generated-inputs/README.md)
records exact prompts, references, output hashes and native dimensions.
All generated full-body outputs drift from the accepted neutral and are
unaccepted. A [strict eye-local study](../../verification/dif-production-art-v005/blink-study/README.md)
keeps outside-eye RGBA and all alpha exact but **fails** at 256 pixels because
of a pale lower-eye seam and uneven purple donor shading. Half lids and timed
blink motion remain absent. The [six-input screen](../../verification/dif-production-art-v005/extended-study/README.md)
finds every uncomposited output unsuitable for direct promotion. E1 moves
only one pupil clearly, E2 misses the halfway lift, E4 is too high, E5's
smaller/lower head does not prove a nod, and E6's head tilts in the wrong
direction. E3's two open paws and E6's correct-side paw remain useful donor
ideas, but local shoulder/belly/face/foot registration is unproved. No full
extended sequence or source promotion is claimed.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.

The later user-authorized d6/d7 offline image-edit continuation supersedes
the original model-run boundary for **scoped candidate making** only. It does
not accept edited pixels, alter the production build/runtime, or authorize
remote mutation. The final objective and focused human review still govern.

The d8 [six-donor graft diagnosis](../../verification/dif-production-art-v006/extended/README.md)
keeps the accepted neutral exact outside finite local masks and confirms fixed
feet, clean transparent RGB, baseline and clearance. Static 64/72/96/256
inspection shows E3's two-paw presentation and E6's correct-side open paw are
useful material, but their belly/shoulder joins and complete transitions are
not qualified. E1's eye graft doubles highlights, E2's high paw is occluded
by the exact head, E4 truncates the too-high paw, and E5 leaves old ear
fragments. No extended clip is accepted or ready for a final art verdict.
BPR-0755 remains the unmet dependency; this card stays Backlog.

The d8 [blink eye-material continuation](../../verification/dif-production-art-v006/blink/README.md)
retains a separate six-frame neutral/half/closed/half/neutral/neutral local
study at 100 ms per frame. Static 64/72/96/256 theme inspection shows
soft lids without v005's pale lower-eye sliver and preserves the accepted
body and alpha outside the eye interiors. It is not accepted source: the
timed player could not be observed in the subagent browser because `file:`
navigation was policy-blocked, and no native Qt or focused human art verdict
exists. The other six gestures still lack qualified complete sequences.
