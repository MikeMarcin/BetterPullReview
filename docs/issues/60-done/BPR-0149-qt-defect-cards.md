---
schema: "implement-rfc.card.v1"
id: "BPR-0149"
title: "Qt defect cards, badges, evidence, and explicit reviewer actions"
status: "Done"
rfcs: ["RFC-BPR-0009"]
depends_on: ["BPR-0148"]
---

# Qt defect cards, badges, evidence, and explicit reviewer actions

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

`defect_card_view` presents one defect and offers the reviewer's explicit
actions. It is given one dispatch callback and nothing else — no defect store,
no provider — so there is no path by which it could decide a defect is
verified.

### The third independent no-verify proof

The core refuses it, the host refuses it, and now the adapter does too. Each
layer asserts it separately rather than inheriting the others' assertion,
because a rule enforced in one layer is one the other two can route around.

The test presents **every** defect state, triggers **every** action, and
asserts the card's own state never moves. A card that advanced its own badge
would show a defect as verified before anyone had decided it was, which is the
exact failure this RFC exists to prevent. Breaking it that way fails
immediately.

### Enablement mirrors what the core would allow

Verify is offered only from `fix_claimed` and `needs_verification`, reopen only
from `verified`, and a settled defect offers no further decision. An action
offered where the core would refuse it teaches the reviewer that the tool is
unreliable — they press it, nothing happens, and they stop trusting the rest.

The test walks all seven states and checks each action's enablement against the
core's rule rather than against a remembered list.

### Reading without colour

All seven states carry distinct textual badges, asserted pairwise, and the
badge leads the label so the state reads in greyscale or through a screen
reader. Each state also carries a sentence, because "[needs verification]"
tells a reviewer the label but not what is being asked of them.

### The classification says it is local

Severity and category never reach the published comment, so the card names them
as *local classification* rather than showing them bare. A reviewer who saw
"critical, security" with no qualifier could reasonably assume the collaborator
they are talking to has seen it too. They have not.

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- The card advancing its own badge on verify — "no action moves the card's own
  state".
- Verify offered everywhere — "verify is offered exactly where the core would
  allow it".
- Two states sharing a badge — "two states never share a badge".
