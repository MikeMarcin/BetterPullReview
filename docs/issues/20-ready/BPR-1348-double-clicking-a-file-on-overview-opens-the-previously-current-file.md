---
schema: "implement-rfc.card.v1"
id: "BPR-1348"
title: "Double-clicking a file on Overview opens the previously current file in Review"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"BPR-1339 owner look, session 0d079940affe6da5c393df07d82a22d8, BPR-1339-overview-accepted-mark-v002 Accept note","decision_id":"e675d148-d02a-45b9-b234-370b91f65729","reservation":"docs/issues/_reservations/BPR-1346--BPR-1348--d02ea45d5cd2.json","related_cards":["BPR-1339","BPR-0647","BPR-0656","BPR-0658"],"acceptance":["BPR-1348:reproduced","BPR-1348:fixed"]}
---

# BPR-1348: Double-clicking a file on Overview opens the previously current file in Review

## Owner note

In attended session `0d079940affe6da5c393df07d82a22d8` the owner accepted
[[BPR-1339]]'s Overview mark (`BPR-1339-overview-accepted-mark-v002`, decision
`e675d148-d02a-45b9-b234-370b91f65729`, event 12, 2026-10-09T04:12:56Z) with
this note, verbatim:

> bug: I double clicked main.cpp and it opened to renderer.cpp in the review.

Two attachments. The first is Overview on `bpr-fixtures/nested-folders#21`
("Move renderer, shaders and widgets into nested folders") with the
`src/main.cpp` row selected, while the review bar says "File 5 of 17" and the
status bar "Actionable file 5 of 17. Private progress: unread, accepted.":
`src/engine/renderer.cpp`, the file the frozen build opened in Review, still
current. The second is Review right after the double-click, showing
`renderer.cpp` with its row current in the Files drawer.

## Likely cause (to confirm)

Double-click opened the file that was current rather than the row that was
double-clicked: the Overview row became current, Review was entered, and the
selection did not move to `main.cpp`.

## Acceptance

- `BPR-1348:reproduced`: a composed course on the frozen build's fixture
  opens `renderer.cpp` in Review as the launcher did, returns to Overview and
  double-clicks `src/main.cpp`'s row by pointer; against the code before the
  fix Review shows `renderer.cpp`.
- `BPR-1348:fixed`: the same course shows `src/main.cpp`, and a root file and
  a deeply nested file open the same way; Return on a row still opens it.
