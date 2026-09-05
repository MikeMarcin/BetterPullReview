---
schema: "implement-rfc.card.v1"
id: "BPR-0135"
title: "Core draft, edit, discard, and publish state"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0134"]
---

# Core draft, edit, discard, and publish state

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

`core::review_round_trip` owns the whole draft policy. It performs no request,
reaches no provider, and knows nothing about HTTP; the suite runs under
`core-only`, which links no provider at all.

### `publishing` is a state, not a moment

Publication is not instantaneous and the interval is observable. Without a
`publishing` state a draft would be either editable or published, and a second
publish issued while the first was in flight would look legal. During that
interval the body cannot be edited, the draft cannot be discarded out from
under the request, and a second publish is refused.

### A failed publication returns the text to its author

`fail_publish` returns the draft to `drafting` with its body intact, not to
`discarded`. Throwing away what a reviewer wrote because the network
misbehaved would be the tool losing their work. The test asserts the text
survives and that the reviewer can try again.

### Shared is a consequence, not a flag

`shared()` is derived from reaching `published`. There is no state in which a
draft is partly shared and no way to set the flag independently, so a draft in
flight — which has left the reviewer's hands but is not yet on the pull request
— cannot be mistaken for a public one.

### A published comment is GitHub's

Once published, no draft command touches it. Editing a published comment is a
different operation against a different authority, and allowing a draft command
to reach it would create a second, local truth about text GitHub owns.

### Whitespace is not a comment

A body of spaces or newlines is refused. Publishing one would put an empty
bubble on someone's pull request, which is worse than refusing it. A body with
a single readable character is accepted — the rule is "nothing to read", not "a
minimum length".

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- `fail_publish` discarding the draft — "the draft returns to composition with
  its text intact".
- The emptiness check reduced to `!empty()` — "a body with nothing to read is
  refused".
- `shared()` returning true for a draft in flight — "a draft in flight is not
  yet shared".

The second break initially failed to compile, because removing the check left
`meaningful` unused under `-Werror`. The suite then reported the *previous*
break's message from a stale binary. It was redone so it compiles, and the
build exit code is checked before any result is believed.
