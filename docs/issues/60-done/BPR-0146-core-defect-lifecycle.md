---
schema: "implement-rfc.card.v1"
id: "BPR-0146"
title: "Core defect classification and the lifecycle state machine"
status: "Done"
rfcs: ["RFC-BPR-0009"]
depends_on: ["BPR-0145"]
---

# Core defect classification and the lifecycle state machine

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

`core::defect_workflow` owns the whole lifecycle. It reaches no thread, issues
no request, and knows nothing about GitHub — classifying a thread as a defect
is a local act, and this is where that stays true. The suite runs under
`core-only`.

### `verify` is the only way in

There is deliberately **no command** by which a claim, a reply, or a code
change can verify anything. `verify` exists solely as an explicit reviewer
intent.

The test asserts this exhaustively rather than by example: every other command
is applied from every non-terminal state and none reaches `verified`. A future
convenience that made `claim_fix` verify when evidence already existed — a
plausible feature request — fails it.

### Two states short of verified, kept apart

`fix_claimed` and `needs_verification` both mean "somebody says this is done"
and neither is `verified`. They stay distinct because their source differs: an
author asserting the fix, versus evidence suggesting it. Collapsing them would
lose which happened, and a reviewer needs to know whether a person made the
claim or a heuristic noticed a change.

### Verifying nothing is refused

`verify` is legal only from `fix_claimed` or `needs_verification`. Verifying an
open defect nobody has addressed would be recording a judgement about work that
has not happened, so it is refused with its own reason rather than silently
accepted.

Reopening is legal only from `verified`, for the mirrored reason.

### Settled decisions are not transitions

`withdrawn` and `accepted_as_is` refuse every further command. Reopening one is
a new defect rather than a lifecycle move, so the earlier decision stays part
of the record instead of being overwritten by a later change of mind.

### Severity and category are local enrichment

They never appear in the published comment. A defect whose meaning lived in a
severity field would be unreadable to every collaborator not using this tool,
which is the failure mode BPR-0153 exists to catch. Reclassifying moves nothing
in the lifecycle, and naming one field leaves the other alone.

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- A fix claim verifying when evidence already existed — "no command other than
  verify ever reaches verified".
- Verifying permitted from any state — "verifying a defect nobody has addressed
  is refused".
- A settled defect accepting commands — "a settled defect refuses every further
  command".
