---
id: BPR-0148
title: Host correlation of threads, replies, and epochs with defect records
status: Done
rfc: RFC-BPR-0009
depends_on: [BPR-0147]
---

# Host correlation of threads, replies, and epochs with defect records

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

`correlate` ties ordinary GitHub facts to local defect records. It is a **pure
function with no provider**.

### Classifying mutates nothing shared, by construction

There is no port here and nothing that could issue a request, so
"classifying an existing thread does not mutate GitHub until the user performs
a distinct shared action" holds because this code has no way to mutate
anything. Publishing, replying, and resolving all go through RFC-BPR-0008's
mutation coordinator, which a user reaches by a separate action.

The thread carries no BetterPullReview marker and does not know it has been
classified. That is what lets a GitHub-only collaborator see an ordinary review
thread.

### The no-verify property is asserted here too, not inherited

The core already refuses to let evidence verify. The host asserts it again
rather than trusting that the core's rule was consulted: the test drives
**every** defect state with code movement, an author reply, and an external
resolution *all at once* — if any combination could verify, that is the one —
and none does.

Folding evidence through `core::observe` rather than assigning states directly
is what keeps the ceiling in the core. Deciding here what a reply means would
put the policy in the host, where the next person adding an evidence kind would
have to rediscover it.

### Precision

A defect whose anchor did not move is left alone; a thread no defect is linked
to raises nothing; and a linked defect is flagged while the one beside it is
untouched. Each is asserted per defect rather than by counting evidence, so a
correlation that flagged everything would fail rather than merely look busy.

Correlating the same facts twice reaches the same state and raises nothing the
second time.

### Non-vacuity

Two behaviours were broken separately, each failing its own assertion, then
restored to green:

- The host concluding that a resolved thread plus a reply means verified —
  "no combination of evidence ever produces verified".
- Correlation flagging every defect whenever anything changed — "a defect
  elsewhere is left alone".
