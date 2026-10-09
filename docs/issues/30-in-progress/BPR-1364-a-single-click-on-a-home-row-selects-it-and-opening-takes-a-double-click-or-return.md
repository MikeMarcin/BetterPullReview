---
schema: "implement-rfc.card.v1"
id: "BPR-1364"
title: "A single click on a Home row selects it; opening takes a double-click or Return"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner, BPR-1357 attended review, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1364--BPR-1364--15f2a5b9a853.json","related_cards":["BPR-1357","BPR-1342","BPR-1338","BPR-0776","BPR-0736","BPR-1157"]}
acceptance: []
---

# BPR-1364: A single click on a Home row selects it; opening takes a double-click or Return

## Owner notes

Owner, BPR-1357 attended review (accepted), 2026-10-09, verbatim: "feedback:
I don't like a single click on home card opening the pr."

## Scope

A single click on any pull-request row or card on Home only selects it: the
row becomes current, takes focus and is highlighted. Opening takes a
double-click, Return, or the row menu's Open Review. BPR-1357's "Opening #N…"
still appears in the same turn as the opening gesture.

Every Home list is in scope: the queue in each arrangement (state panels, the
flat card, group panels and their state bands, which personal discovery's rows
also fill), Recently viewed, and the Review Cycles list beside them.

## Acceptance

- `BPR-1364:select`: a single click on a row of every Home list makes it
  current, focused and selected, and opens nothing, under the platform's own
  style and under a style that asks for single-click activation (as KDE does).
- `BPR-1364:open`: a double-click on a row opens it once; Return opens the
  current row; the row menu's Open Review opens it.
- `BPR-1364:same-turn`: through the composed application with the fixture
  delay, a double-click on `#17` says "Opening #17…" on that row with no
  event-loop turn after it, and a single click before it asked for nothing.
- `BPR-1364:accessible`: every Home list's accessible description says
  "Double-click or press Return to open".
- `BPR-1364:failing-before`: the new tests fail against the code before the
  change.
- `BPR-1364:owner`: the owner accepts single-click selection on a frozen
  build (one Accept/Reject question).
