---
schema: "implement-rfc.card.v1"
id: "BPR-2279"
title: "A scroll gesture stays with the nested scroller it began in"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2277--BPR-2279--424d9c5fc0e9.json","found_by":"owner attended review, 2026-11-19, accepting BPR-2273","related_cards":["BPR-2271","BPR-2273"]}
---

# BPR-2279: A scroll gesture stays with the nested scroller it began in

## Observed

Owner, 2026-11-19, accepting [[BPR-2273]]: "momentum scrolling that reaches
the end of a nested scroll window won't scroll the outer window without an
additional gesture. In fact that appears to be exactly how it works in the
chrome browser."

## Acceptance

- AC1. A wheel or trackpad gesture that begins in a nested scroller (a long
  draft editor, a code block or suggested-change diff in the drawer, a
  report) stays latched to it through its momentum phase; reaching its end
  does not pass the rest of the gesture or its momentum to the outer area.
- AC2. A new gesture begun at the inner scroller's end scrolls the outer
  area. A horizontal-only inner scroller passes vertical deltas through.
- AC3. One shared helper latches every nested scroller; the per-widget wheel
  forwarding of BPR-2271 and BPR-2273 routes through it.
- AC4. Qt tests synthesize phased wheel events (begin, update, end, momentum).
- AC5. Focused tests, format and the documentation gate pass.
