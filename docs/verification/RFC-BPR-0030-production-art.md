# RFC-BPR-0030 production-art frontier

Run `dif-takeover-20260914`, dispatch `d6`, producer `rfc-integrator-d4`,
mandate revision `3`. The adopted `production-art` milestone binds
BPR-0755/0756/0757 and both authored exits. This record is the current
candidate/evidence pointer, not acceptance of the rest/idle/greet motion or a
ten-clip catalog.

## Neutral identity candidate — BPR-0755

The immutable 1254-square incoming `rest` image has SHA-256
`eacedea5ddfdc6317b007e2032093f620620777279c70f87250eadd79a4a90ae`.
[The offline recipe](dif-production-art-v001/prepare_neutral_candidate.py)
verifies that hash and the authorized reference-input hashes before writing
[a separate 1024-square candidate](dif-production-art-v001/dif-neutral-master-candidate.png).
The candidate's SHA-256 is
`bb6ccdbb00cf55624f15aabcdf08f47b8d97ce1835af9bcc7e01d4d4471ebeef`.
It uniformly resizes the substantive raw rest subject, preserving four native
pixels of margin, without averaging another pose or introducing model output.
The raw alpha=1 distant specks are outside that crop. The raw source remains
unchanged. Candidate alpha bounds are `[186,86,838,892)` at 1024; the
256-square box downsampling lands at the inherited bottom-exclusive y=223.
This is a proposed master only, outside the runtime source inventory. The
existing accepted three-clip, card-bearing source/manifest/generated tree is
unchanged.

The [identity comparison](dif-production-art-v001/dif-neutral-identity-review.png)
shows the authorized Dif reference crops, previously accepted card-bearing
master, and new prop-free candidate. The [theme plate](dif-production-art-v001/dif-neutral-master-review.png)
shows the proposed candidate over light, dark and high contrast, and the
[size plate](dif-production-art-v001/dif-neutral-sizes-review.png) shows
64/72/96-pixel previews on light and dark. These are candidate review media;
they do not establish motion, production matte acceptance or Qt presentation.
The reference crops are review-only and contain no shipped foreign art.

Focused objective checks on the candidate:

- The installed sprite-sheet validator with explicit 1024-square, one-frame
  layout and `--no-body-check` reported `ok: true`. Its blue sample-character
  detector was disabled, so this says nothing about Dif identity.
- Pillow decoded RGBA 1024×1024, found zero nonzero RGB values beneath
  zero alpha, and measured `[186,86,838,892)` alpha bounds. Box-reduced
  256-square alpha ended at y=223. Identity/landmarks still need review.
- `PYTHONDONTWRITEBYTECODE=1 python3 docs/verification/dif-production-art-v001/prepare_neutral_candidate.py`
  regenerated all four exact candidate outputs; their hashes are in the
  [review manifest](dif-production-art-v001/README.md).

The required first-phase decision was whether this **exact prop-free neutral**
preserves Dif's authorized identity, proportions, awake closed-mouth expression
and resting paw/foot pose for the new catalog. The human accepted that exact
question in the shared session at sequence `379`, candidate fingerprint
`1bcdc33cbd9175db6ee843f60f90e3a70e9800470a19562bd4814b18d600ad81`,
decision `6eabb190-287d-4e7c-a19f-bd14f4075701`, with an empty note.
The candidate may be promoted into a new production source revision, then
idle/greet and the seven extended gestures normalized against its exact bytes.
No pose normalization or final catalog acceptance follows from this decision.
New derivative/distribution
disposition and complete motion/theme/scale/handoff review remain required at
the final catalog candidate, and old BPR-0291 exact-byte decisions do not cover
these derivatives.

The shared attended-review session `cbd48b91000b00f1e72e1ad62c7c384c`
has the protected candidate:
`BPR-0755-neutral-identity-v001`, fingerprint
`1bcdc33cbd9175db6ee843f60f90e3a70e9800470a19562bd4814b18d600ad81`,
at event sequence `378`. The three review images are embedded in that exact
candidate. Sequence `379` holds the scoped accept above; sequence `380` is the
reviewer's hand-back with no pending item in that shared session. The read-only
watch exited and the loopback host was stopped after hand-back; the durable
session and protected candidate remain intact.

The review page also displayed older, completed initiatives because
`attended_review.py init` merges candidates into the checkout's existing
session, while its HTML renders every decided candidate in an Answered group.
That shared session has 30 answered candidates. The old decisions are retained
without rewriting or deletion. Prepare the later exact-byte full-art review in
a fresh Dif-only external `--state` session so its page contains only current
questions; bind that new session to its own final candidate and verdict.

No task-owned model call, GitHub/GHES mutation, source replacement, runtime
selection or app build occurred in this candidate-preparation slice.

## v002 source preparation — partial, not production

The [offline study](dif-production-art-v002/README.md) preserves the accepted
neutral's original bytes and has a new compiler-canonical master whose decoded
1024-square RGBA pixels are identical. The original uses twelve IDAT chunks;
the production compiler accepts one, so the canonical payload has a new SHA-256
`e2820eeff1f1ebf2584b1947a2b2062f98f29b541dd587cc8ce92196c71ab0cf`.
That new exact payload is not silently treated as the reviewed digest. The final
catalog decision must bind it and every final derivative.

The study records thirteen canonical 256-square source candidates: exact rest
and twelve-frame calm idle with local chest motion, no embedded blink, and
unchanged head, eyes, hips and feet. Objective checks verify the master pixel
equality, exact schema-2 reduction, baseline/alpha/clearance, distinct idle
seam, retained hashes and static theme plates at 64/72/96 px. No real playback
or human motion verdict is claimed. The greeting arm experiment has a visible
joint discontinuity and insufficient cheek-height wave, so it is excluded
from source. The seven extended raw sequences remain unnormalized. The crop
inventory gives exact provisional rectangles and hashes for all 71 raw cells;
the [v002 record](dif-production-art-v002/README.md) names each necessary
limb/head/occlusion retouch. Neither staging media nor fixture sources enter
the shipped three-clip catalog.

The accepted master also exposed a schema-2 compiler seam: fixed-box alpha can
round to zero while legacy color averaging leaves hidden RGB. The schema-2
reduction now clears those colors; schema 1 keeps its previously accepted
generated bytes. A red-before-fix low-alpha regression, a complete schema-2
master-path test, the then-current 56-test focused asset suite, shipped direct
validate/verify, empty-output generation, dev/core-only asset CTests, the
selected-graph boundary verifier, documentation gate and digest inventory
passed. The full suite result predates the added schema-2 master-path test;
that added test passed separately. The selected-graph check establishes only
the unchanged shipped schema-1 boundary, not final ten-clip selection.

The focused [raw-paw greeting continuation](dif-production-art-v002/rejected-greet-retouch/README.md)
isolated actual raised paw/forearm pixels from raw frames 2–5. Its eight-frame
study retained exact neutral endpoints and structural alpha/foot/clearance
checks, and the paw reached cheek height. Actual-size light/dark inspection
still found a shoulder/belly seam, too-narrow forearm and ghosted approach/return
from cross-fading disparate silhouettes. It was rejected from source. The
reproducible crop/mask/retouch recipe, 96-px plate and checks remain evidence,
not an accepted pose or motion verdict.

The next source task is careful offline illustration/layer retouch of the real
greet paw and seven contextual gestures against the accepted master, followed
by normalized compilation and one complete digest-bound art/derivative review.
No new model run is authorized in these cards. This is a concrete production-art
blocker; neither authored exit is satisfied yet.

## v003 connected-anatomy study — rejected from production

The [d5 offline recipe and plates](dif-production-art-v003/README.md) retain a
third bounded greeting study, without individual production source frames.
It verifies the accepted master and immutable raw greet hashes, composes six
distinct moving poses between exact neutral endpoints, and uses actual raw
padded-paw pixels for the two open-palm poses. Repeated regeneration and the
retained SHA-256 inventory agree. Objective checks pass fixed hips/feet,
opposite body, opaque head, alpha cleanup, baseline and clearance. The
integrator inspected the 96-pixel light/dark plate: the neutral-arm approach
has an attached shoulder, but the raw-palm forearm arrives as a much larger
surface, then disappears on return. The study measures visible limb area
1,036→1,783 pixels at frames 2→3 (+72.1%) and 1,785→1,084 at 4→5
(−39.3%); a dark shoulder crease remains. These are a diagnosed shape and
occlusion discontinuity, not a production greeting or a new human verdict.

The next discriminating artwork operation is an authored, continuous
upper-arm/elbow and forearm at the accepted master's scale across the lift,
with wrist/paw articulation handled separately. Rigid neutral-arm rotation,
whole-cell registration and mask feathering have not solved that mismatch.
Seven extended gestures, compilation, final derivative disposition and one
complete Dif-only exact-digest review remain pending. That review should use a
fresh external `--state` so older answered initiatives do not appear on the
review page. No new attended candidate is ready and no shipped source/runtime
asset changed. Actual-speed playback and native Qt presentation remain
unverified.

## v004 authorized image-edit inputs — unaccepted

The user explicitly authorized scoped offline image editing for missing
connected arm poses in d6. This supersedes the earlier BPR-0755 no-model-run
boundary for that candidate operation; it does not approve generated pixels
or change ordinary build/runtime behavior. Two built-in image-edit results,
their exact prompts, input roles, source and output hashes are retained in
[v004 generated inputs](dif-production-art-v004/generated-inputs/README.md).
The first uses the accepted neutral as edit target and immutable raw greet as
pose reference. The second lowers the first pose. Both tool outputs are
1254-square RGBA, not the requested 1024-square canvas. Inspection shows
potentially useful connected arm surfaces but changed head/body scale and
facial pixels. They are supporting candidates only. A local derivation must
preserve the accepted neutral's locked head, body, opposite arm, hips and
feet, then prove a smooth limb transition at actual shipping sizes. The
generated full-body rasters must not replace the exact neutral or be counted
as production frames. The d6 greeting and seven-gesture studies are under
bounded separate ownership; no source/catalog/review acceptance is claimed.
