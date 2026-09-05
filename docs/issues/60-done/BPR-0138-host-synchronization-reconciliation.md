---
schema: "implement-rfc.card.v1"
id: "BPR-0138"
title: "Host synchronization, refresh cursors, and external-change reconciliation"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0137"]
---

# Host synchronization, refresh cursors, and external-change reconciliation

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

`reconcile` applies an observed server snapshot to the local projection. GitHub
is the authority and the projection exists to mirror it, not to argue with it.

### The rule this slice exists for

A local assumption never overwrites an external change. When a comment carries
an **unpublished local edit** and the server has also changed it, the server's
text still becomes the projection — and the entry is reported `conflicted` so a
person can decide what to do with the edit they were holding.

That is a refusal to resolve, not a resolution. Discarding either side quietly
would lose someone's words, and this code is not entitled to choose between
them.

Holding an unpublished edit on a comment the server did *not* touch is not a
conflict, which the tests assert separately. Otherwise every unsaved draft
would raise a false alarm on every refresh.

### External shapes

Created, edited, deleted, resolved, and reopened each round-trip. The edit case
is asserted to *update* rather than duplicate, because GitHub keeps the
identity across an edit and identity alone cannot tell a reader the text
changed under them. A comment the server no longer reports leaves the
projection: retaining it because the projection remembers it would show a
reader a comment that is not there.

### Two refreshes agree exactly

The projection and the entries are ordered by identity, so refreshing the same
state twice produces an identical result rather than an equivalent one. The
test supplies the local and observed sets in different orders and asserts the
outputs are equal and that nothing is reported changed.

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- A local edit winning over the server — "the server's text becomes the
  projection, not the local edit".
- A conflict reported as an ordinary update — "the collision is reported as a
  conflict rather than an update".
- A dropped comment retained — the deletion loses its entry entirely.
