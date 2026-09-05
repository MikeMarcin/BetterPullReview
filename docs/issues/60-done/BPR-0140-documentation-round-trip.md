---
schema: "implement-rfc.card.v1"
id: "BPR-0140"
title: "Documentation for round-trip mapping, drafts, identity, and safe-test policy"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0139"]
---

# Documentation for round-trip mapping, drafts, identity, and safe-test policy

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

`docs/architecture/review-round-trip.md` covers every mandatory topic: review,
comment, and thread mapping; drafts; publish semantics; identity; stale-head
behaviour; retries; capability fallback; safe-test policy; and the mixed-client
contract. It carries the ownership table and the typed
command/decision/proposal/request/completion/observation/delta vocabulary the
RFC requires.

The vocabularies were checked against the source rather than written from
memory — the five draft command kinds, the five reconciliation kinds, and the
ten inputs to the idempotency key all match what the code declares.

### The constraint the document leads with

A comment this tool publishes is a GitHub comment: readable, replyable,
editable, and deletable from any client, with no marker identifying where it
came from. No bot, no app, no label scheme, no hidden metadata. Everything else
in the document is in service of that, and saying so first makes the rest read
as consequences rather than as unrelated rules.

### Two things stated as prohibitions rather than descriptions

**Nothing in this repository designates a safe mutation target today.** A
reader looking for the designated repository will not find one, and the
document says so instead of leaving them to conclude it from silence.

**Weakening the coordinator's target check to let a test through would remove
the property the test exists to exercise.** That is the shortcut most likely to
be reached for under time pressure, so it is named and refused in writing.

### The boundary with the private ledger

The ledger document now links here and states that the two share no table and
no authority: one is local and private, the other mirrors GitHub. Recording the
no-impact region explicitly is what the RFC asks for, and it stops a later
reader from assuming the two stores overlap.
