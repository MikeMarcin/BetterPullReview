---
schema: "implement-rfc.card.v1"
id: "BPR-0137"
title: "Host mutation coordinator: safe target, idempotency, retries"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0136"]
---

# Host mutation coordinator: safe target, idempotency, retries

See the RFC-BPR-0008 verification plan for this slice's place in the sequence.

Every surface here is built against the fixture provider. No slice in this RFC
performs a live mutation; the criteria that need one are BPR-0142, BPR-0143,
and BPR-0144.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- Every refusal carries its own typed reason, so two different mistakes are
  distinguishable by a caller.
- No test reaches a live host.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

`review_mutation_coordinator` performs mutations against exactly one designated
target, fixed when it is built and never replaceable.

### The target is observed twice

Once to decide whether publication is permitted, and again immediately before
submitting. A check performed only at the first point passes for a target that
changes afterwards, and that window is exactly what an automated run could slip
through.

The fake provider makes the window real: it reports the designated target on
the first observation and a different repository on the second. The coordinator
refuses, submits nothing, and the statistics show two observations and zero
submissions — so the test proves the *second* check ran rather than inferring
it from the refusal.

### The acknowledgement boundary is enforced at the host too

The core already refuses to permit a changed target however it is
acknowledged. The coordinator enforces the same boundary rather than trusting
the core's answer to have been consulted, and the test drives both the
acknowledged and unacknowledged paths. A moved head is refused as `stale_head`
and a changed pull request as `undesignated_target` — two different mistakes
with two different repairs, never one generic refusal.

### Retries are safe because the key is derived, not remembered

A request whose answer was lost is indistinguishable from one that never
arrived. `idempotency_key_for` is a pure function of the draft identity, the
target, the anchor, the disposition, and the body, so a retry produces the same
key and the provider recognises it. The test asserts the retry returns the
*original* comment identifier and that the provider holds one comment, not two.

Four separate cases assert the key changes when the body, the anchor, or the
head changes, and stays identical for identical inputs. A key that ignored any
of those would let two different comments collide.

### The request carries the designated target, not a supplied one

`publish` takes a proposal and nothing else about where it goes. The target in
the request is the coordinator's own, so no caller can name a destination.

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- The second observation removed — "a target altered after the prepare check is
  refused".
- The body dropped from the idempotency key — "a different body produces a
  different key".
- An acknowledgement authorising a changed target — "acknowledging does not
  authorise a different pull request".
