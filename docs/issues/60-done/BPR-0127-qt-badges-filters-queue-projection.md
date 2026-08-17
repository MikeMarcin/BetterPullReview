---
id: BPR-0127
title: Qt badges, filters, explanations, and queue projection
status: Done
rfc: RFC-BPR-0007
depends_on: [BPR-0123, BPR-0125]
---

# Qt badges, filters, explanations, and queue projection

The adapter presents badges, filters, explanations, actions, and the queue and
file-tree projection, and translates events. It **cannot mark state by mutating
a Qt model directly** — every state change goes out as a semantic intent and
comes back as an accepted projection.

The explanation is rendered from the facts the core supplies. An adapter that
composes its own reason text is deciding policy.

Colour is never the only signal for a review state, which the appearance work
already requires of every state.

## Required direct evidence

- Marking read, accepted, or excluded from the UI emits an intent and changes
  nothing until the accepted projection arrives.
- Bounded changes use incremental model signals rather than a full reset.
- The rendered explanation matches the facts the core named, and changing a
  fact changes the rendered text.
- Every review state carries a non-colour signal, and accessible names are
  unique across focusable widgets.
- Each assertion is shown to fail when the behaviour it constrains is neutered.

## Outcome

`review_queue_model` presents the queue the core ranked. It decides no order
and composes no reason of its own.

### The explanation is rendered, not authored

`explanation_for` and `marker_for` are total mappings over the core's closed
reason vocabulary, so a reason added to the core cannot reach the screen as an
empty string. The test asserts the rendered text *follows* the core's reason —
changing the reason changes the text, and the text equals what the mapping
publishes for it. An adapter that returned one constant string fails that,
which is the shape of "the adapter decided policy while looking like
presentation".

### Colour is not the only signal

Every reason carries its own textual marker, all seven are asserted pairwise
distinct, and the marker *leads* the visible label so the state reads without
consulting a colour. The accessible text carries the full explanation rather
than the marker alone, so a screen reader gets the sentence and not the
shorthand.

### Bounded changes stay bounded

A reset discards selection and scroll position and tells assistive technology
that everything it knew is gone. One changed row emits exactly one
`dataChanged` naming that row; a longer queue inserts; a shorter one removes;
an identical queue emits nothing at all. No path through the model resets it,
which the tests assert directly rather than by inspection.

`QSignalSpy` lives in Qt6::Test, which this build does not link. Rather than
widen the build's Qt component list for a test, the suite connects to the
signals directly.

### Non-vacuity

Three behaviours were broken separately and each failed its own assertion, then
restored to green:

- `apply` replaced by a model reset — "a changed row does not reset the model".
- Two reasons sharing one marker — "two reasons never share one marker".
- `explanation_for` returning one constant — "changing the core's reason
  changes the rendered explanation".

### Not covered here

**Closed by BPR-0139.** The card also asks that marking read, accepted, or
excluded from the UI emit an intent and change nothing until the accepted
projection arrives. That path is
built and proven at the host in BPR-0125, where the fake store and projection
share one log; wiring it to a widget with actions belongs with the composition
in BPR-0128, which is where a window exists to put the actions in. This slice
delivers the projection surface those actions will update, and the model has no
mutating entry point at all — the only way in is `apply`, which is what makes
"the adapter cannot mark state by mutating a Qt model" true by construction
rather than by convention.
