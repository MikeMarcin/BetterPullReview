---
schema: "implement-rfc.card.v1"
id: "BPR-0152"
title: "Simplification and closeout preparation for RFC-BPR-0009"
status: "Done"
rfcs: ["RFC-BPR-0009"]
depends_on: ["BPR-0151"]
closeout_for: ["RFC-BPR-0009"]
---

# Simplification and closeout preparation for RFC-BPR-0009

See the RFC-BPR-0009 verification plan for this slice's place in the sequence.

The rule this RFC exists to protect: **a nearby code change is evidence, not a
verdict.** Evidence reaches `needs verification` and never `verified`. Only an
explicit reviewer intent verifies, and this slice must not open a path around
that however convenient it would be.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- No path through this layer reaches `verified` without the explicit reviewer
  intent.
- Every refusal carries its own typed reason.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

**This card makes no closure claim.** RFC-BPR-0009 stays `Proposed`.

### Two rows were still unproven at closeout

Reading the result column alone — as at the previous two closeouts, because
bulk-editing a matrix has twice given one row another row's marker — found two
rows still `Planned`. Closing out around either would have been dishonest.

**Reopening is a separate explicit action.** Now proven the same way the
verification rule is: every other command and all three evidence kinds, from
every state, and none reaches `reopened`. Nothing may produce it as a side
effect of doing something else, or a reviewer would find defects back in their
queue without having put them there. Breaking it so withdrawing a verified
defect reopens it fails immediately.

**The published prose stands alone.** Verifying this found something stronger
than a test: neither `review_publication_proposal` nor `review_mutation_request`
has a field for severity or category. No publication path can carry them
because there is nothing to carry them in. The absence is the guarantee.

What that does *not* settle is whether the remaining prose is **sufficient**.
That is a judgement about words, and BPR-0153 owns it.

### Claim matrix reconciled

Eight rows. Two are deliberately partial and say which half is missing:

- *Counts and navigation agree* — the card derives everything from the
  presented projection and holds no state, so there is no separate tally to
  disagree; a queue view aggregating many defects is not built.
- *The published prose stands alone* — structurally guaranteed, judgement
  deferred.

### Simplification

Nothing was extracted. The lifecycle, the evidence policy, the correlator, the
card, and the anchor history each own one decision. The evidence policy folds
through `core::observe` rather than duplicating the ceiling, and the card's
enablement mirrors the core's legality rather than restating it — both already
avoid the duplication a later reader would have had to reconcile.

### Replay

`core-only` 47/47, `dev` 110/110, format, tidy, documentation verifier, pack
validator, zero workflows, `git diff --check`.

### Remaining gap

| Card | What is missing |
|---|---|
| BPR-0153 | The designated repository from BPR-0142, plus a person who has never used this tool, to read a published defect and say what corrective work is being asked for |
