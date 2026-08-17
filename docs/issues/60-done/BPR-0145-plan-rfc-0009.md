---
id: BPR-0145
title: Plan RFC-BPR-0009 verification and allocate its cards
status: Done
rfc: RFC-BPR-0009
depends_on: []
---

# Plan RFC-BPR-0009 verification and allocate its cards

RFC-BPR-0009 restores inspection-style defect semantics as a local projection
over ordinary GitHub review threads, keeping an author's claim that something
is fixed strictly separate from a reviewer's verification that it is.

## Outcome

`docs/verification/RFC-BPR-0009.md` records the allocation, an eight-row claim
matrix, the deferred gate, and cards BPR-0146 through BPR-0152.

## The rule the whole plan serves

**A nearby code change is evidence, not a verdict.** Code moving near a defect,
or an author replying to say they fixed it, reaches `needs verification` and
never `verified`. Only an explicit reviewer intent verifies.

This is written into every implementation card, because it is the criterion
most likely to be eroded by a convenience. An auto-verify for "obviously fixed"
defects is a plausible feature request and would destroy the distinction the
RFC exists to restore. The core tests it, and the host and Qt slices must each
prove they cannot reach `verified` on their own — three separate proofs rather
than one, because a rule enforced in one layer is a rule the other two can
route around.

## One deferred gate, and it is about prose rather than code

BPR-0153: a collaborator who has never used BetterPullReview must understand
and respond to a published defect. No fixture can settle that. The fixture
holds the words this tool wrote; the question is whether a person lacking all
local context can act on them.

The card says what must not be done instead. It must not be answered by reading
the prose oneself — knowing the local severity and category cannot be un-known.
And it must not be answered by adding a marker or legend to the comment, since
the RFC forbids hidden metadata and a comment needing a legend has already
failed.

The RFC's own matrix says "real workflow" for *fix is not verification*, but
that phrasing overstates what needs a live target: pushing a nearby change is
modelled as evidence and the policy is core-testable. Only the prose question
genuinely needs a person.
