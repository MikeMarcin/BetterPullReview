---
id: BPR-0151
title: Documentation for defect semantics, mapping, and verification rules
status: Done
rfc: RFC-BPR-0009
depends_on: [BPR-0150]
---

# Documentation for defect semantics, mapping, and verification rules

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

`docs/architecture/defect-workflow.md` covers every mandatory topic: defect
semantics, the state machine, GitHub mapping, severity and category privacy,
verification rules, anchor history, and the user-facing action labels. It
carries the ownership table and the typed
command/decision/observation/delta vocabulary.

The badges and action texts were read out of the source rather than recalled —
all seven badges and the four action labels match what the widget declares.

### The document leads with the rule, not the taxonomy

A fix claim is not a verification. Everything else — the state machine, the
three evidence kinds, the three-layer enforcement — reads as a consequence of
that rather than as separate rules to memorise.

The privacy rule is stated the same way: severity and category never appear in
the published comment, and that is not a limitation to work around. A defect
whose meaning lived in a severity field would be unreadable to every
collaborator not using this tool, which the RFC's non-goals forbid outright.

### Two no-impact regions recorded rather than assumed

Defects are a projection over the threads the round-trip document describes;
the shared protocol is unchanged and no new GitHub surface is introduced. And
defect state shares no table with the private receipt ledger. Both are written
down, because a later reader who found three documents about review state would
otherwise have to work out for themselves whether they overlap.

### A placement correction

The cross-reference into the round-trip document first landed above its opening
paragraph, so the file began by pointing elsewhere before saying what it was.
Moved below the introduction.
