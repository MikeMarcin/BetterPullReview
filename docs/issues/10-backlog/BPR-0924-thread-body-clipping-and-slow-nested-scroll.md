---
schema: "implement-rfc.card.v1"
id: "BPR-0924"
title: "Fix clipped thread bodies and slow nested scrolling"
status: "Backlog"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028", "RFC-BPR-0013"]
depends_on: []
priority: "P1"
---

# Fix clipped thread bodies and slow nested scrolling

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-07. Earlier RFC references retain their governing contracts.

## Owner feedback

A closed comment thread on a file renders attractively but appears fixed in
height or fails to grow. Only about two-thirds is visible, with an inner scroll
inside the main scroll. “The nested scroll is really really slow for some reason.”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Investigate body measurement, viewport constraints and wheel/trackpad routing
for long thread content. “Closed” is the owner's term; confirm whether the app
models the observed thread as resolved, collapsed, or another state. A fixed
height and the scrolling cause are hypotheses, not yet measured facts.

Compare a content-sized thread with an explicit collapsed/expanded long-body
presentation. Preserve access to the complete body without accidental nested
scroll traps. Measure the slow inner scroll separately from clipping and compare
it with the normally scrolling second representation in [[BPR-0925]]. Coordinate
Markdown [[BPR-0916]] and long-description work [[BPR-0918]] without making those
implementations prerequisites for diagnosis.

Starting points: `src/adapters/qt/src/conversation_cards_view.cpp`,
`src/adapters/qt/src/github_conversation_delegate.cpp`, and
`src/adapters/qt/src/github_conversation_panel.cpp`.

## Acceptance

- Reproduce the approximately two-thirds-visible long thread with synthetic content and identify which widget/layout owns clipping and scrolling.
- All admitted thread text is reachable through a deliberate reading interaction at representative widths, fonts and display scales.
- Wheel, trackpad and keyboard scrolling remain responsive; record before/after observations or measurements and prevent unintended competing scroll regions.
- Expansion, refresh and thread selection preserve sensible focus and reading position; distinguish resolved/collapsed state from body truncation.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
