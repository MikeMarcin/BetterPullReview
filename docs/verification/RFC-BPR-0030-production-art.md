# RFC-BPR-0030 production-art frontier

Run `dif-takeover-20260914`, dispatch `d2`, producer `rfc-integrator-d2`,
mandate revision `1`. The adopted `production-art` milestone binds
BPR-0755/0756/0757 and both authored exits. This record is the current
candidate/evidence pointer, not acceptance of a master or ten-clip catalog.

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

The required first-phase decision is whether this **exact prop-free neutral**
preserves Dif's authorized identity, proportions, awake closed-mouth expression
and resting paw/foot pose for the new catalog. If rejected, use the supplied
note to revise just this candidate and repeat this focused check. If accepted,
promote the candidate into the new production source revision, then normalize
idle/greet and the seven extended gestures against its exact bytes. No pose
normalization is claimed before that decision. New derivative/distribution
disposition and complete motion/theme/scale/handoff review remain required at
the final catalog candidate, and old BPR-0291 exact-byte decisions do not cover
these derivatives.

No task-owned model call, GitHub/GHES mutation, source replacement, runtime
selection or app build occurred in this candidate-preparation slice.
