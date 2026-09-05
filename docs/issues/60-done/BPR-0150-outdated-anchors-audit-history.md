---
schema: "implement-rfc.card.v1"
id: "BPR-0150"
title: "Outdated and deleted anchors, and retained audit history"
status: "Done"
rfcs: ["RFC-BPR-0009"]
depends_on: ["BPR-0149"]
---

# Outdated and deleted anchors, and retained audit history

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

`defect_history` holds a defect together with the context it was filed against,
and `lose_anchor` records that the anchor could no longer be resolved.

### Losing an anchor decides nothing

The lifecycle state is untouched. An anchor going out of date is a fact about
the code, not a judgement about the defect: an open defect stays open and a
verified one stays verified. The test drives all seven states through both ways
of losing context and asserts none of them moves.

The tempting alternative — quietly withdrawing a defect whose code vanished —
would discard a reviewer's finding because a file was renamed. Breaking it that
way fails immediately.

### The original context is retained

`OriginalAnchor` is kept even when it can no longer be resolved, because a
defect whose context vanished still has to be answerable. A reviewer needs to
know what was being complained about, and "somewhere in this file, once" is not
an answer.

### There is deliberately no substitute anchor

The type has no field for one. Repairing a lost anchor with a nearest-line
guess would attach the defect to code nobody filed it against, and the reviewer
would have no way to tell that had happened — the defect would look correctly
placed while pointing at something unrelated. The absence of the field is the
guarantee; there is nothing to set.

### Two ways of losing context, kept apart

An outdated anchor and a deleted thread are different facts. The first means
the code moved beyond where GitHub will show the comment; the second means the
shared conversation is gone. A reviewer's next step differs, so the two are
distinguishable and say so.

### Non-vacuity

Two behaviours were broken separately, each failing its own assertion, then
restored to green:

- Losing the anchor withdrawing the defect — "an anchor going out of date
  decides nothing about the defect".
- The original anchor blanked when context is lost — "the anchor the defect was
  filed against is kept".
