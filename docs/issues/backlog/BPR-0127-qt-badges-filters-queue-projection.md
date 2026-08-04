---
id: BPR-0127
title: Qt badges, filters, explanations, and queue projection
status: Backlog
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
