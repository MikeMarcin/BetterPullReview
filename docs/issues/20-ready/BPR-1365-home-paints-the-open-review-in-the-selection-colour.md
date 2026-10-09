---
schema: "implement-rfc.card.v1"
id: "BPR-1365"
title: "Home paints the open review in the selection colour; only one Home list shows a selection"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"Owner, BPR-1364 attended review (accepted), 2026-10-09","reservation":"docs/issues/_reservations/BPR-1365--BPR-1365--b5afbbae5ee9.json","related_cards":["BPR-1364","BPR-0776","BPR-0733","BPR-1357","BPR-1157"],"requested_skill":"frontend-design"}
acceptance: []
---

# BPR-1365: Home paints the open review in the selection colour; only one Home list shows a selection

## Owner notes

Owner, BPR-1364 attended review (Accept), 2026-10-09, verbatim: "Separate
problem: the color of recent is the same as the color of selected."

The attachment (`/Users/jaeger/bpr-owner-att/BPR-1364-single-click-selects-v001-0.png`,
dark theme) shows Home over the open review of `octo-org/review-repository#17`.
Needs me `#17` is selected (blue fill), and Recently viewed `#17`, the review
open behind Home, has the identical blue fill. So "selected" and "the open
review" look the same.

## Scope

- Selection keeps the native selection colour, and only the current row of the
  Home list that holds the keyboard shows it.
- The open review gets a distinct, quieter treatment, never the selection fill.
- Only one Home list shows a selection at a time.

Every Home list is in scope: the queue in each arrangement (state panels, the
flat card, group panels and their state bands), Recently viewed and Review
Cycles. All four themes are in scope.

## Acceptance

- `BPR-1365:one-selection`: at most one Home list shows a selection, the one
  holding Home's keyboard. Clicking or tabbing into another list moves the
  selection there. Publishing Recently viewed or Review Cycles never selects a
  row in a list that does not hold the keyboard.
- `BPR-1365:reviewing`: the open review's row is drawn with a quiet treatment
  distinct from the selection fill, in every theme, and its accessible
  description says so.
- `BPR-1365:themes`: renders of light, dark, high-contrast light and
  high-contrast dark show this.
- `BPR-1365:failing-before`: the new tests fail against the code before the
  change.
- `BPR-1365:owner`: the owner accepts the change on a frozen build (one
  Accept/Reject question).
