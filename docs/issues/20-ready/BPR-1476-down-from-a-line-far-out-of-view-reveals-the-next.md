---
schema: "implement-rfc.card.v1"
id: "BPR-1476"
title: "Down from a line far out of view reveals the next line"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: []
metadata: {"kind":"defect","found_by":"application_large_review_responsiveness on main in a Release Qt build, six runs before and after RFC-BPR-0080's adoptions (BPR-1446, BPR-1447)","reservation":"docs/issues/_reservations/BPR-1476--BPR-1476--5b5a33ca9d76.json","related_cards":["BPR-1212","BPR-1413"],"related_rfcs":["RFC-BPR-0073"]}
---

# Down from a line far out of view reveals the next line

`application_large_review_responsiveness` fails on main in every Release run
seen: "Down from a line out of view did not reveal the next". The course
selects line 10,000 of the 20,000-line comparison without scrolling, presses
Down, and waits for line 10,001 to be selected and shown. The selection moves,
but the line never comes into view, and the wait expires at the 90 s course
clock.

## Acceptance

- AC1: Down from a selected line far below the view, and Up from one far above
  it, select the next line and reveal it at the nearest displayed edge, as
  RFC-BPR-0073's keyboard reveal requires for a clipped row, in split view
  (either pane, equal and unequal comparisons) and in unified view.
- AC2: A Qt test fails on the code before the fix and passes after it.
- AC3: `application_large_review_responsiveness` passes its keyboard reveal in
  a Release build, and the existing keyboard-reveal and fractional-viewport
  tests still pass.
