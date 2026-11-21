---
schema: "implement-rfc.card.v1"
id: "BPR-2343"
title: "After a push a tour stop can stay \"not compared yet\" with nothing to ask again"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2343--BPR-2343--b12b541dcd31.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2343: After a push a tour stop can stay "not compared yet" with nothing to ask again

## Finding

`application_review_tour_publication_push` fails intermittently on macOS at
"every stop is compared with the pushed head" (the wait at line 261): both
stops stay `unchecked`. 2 of 15 at `ed5e79de9`, once alone at 22.7 s; it
passes on Windows and Ubuntu.

`check_tour_material` runs when the tour's presentation changes and skips a
stop whose code the epoch cannot yet realign to the new head ("asked again"
on a later presentation). When the head's diff is published after the last
presentation, nothing asks again. `ed5e79de9` makes the window ask again
whenever a selected diff is published; that narrows the gap but does not close
it.

## Next step

Log each tour-material check (stop, head, outcome) and each diff publication
(file, head), so a miss shows which publication the stops never saw, then
close the gap.

## Acceptance

- AC1. The log shows, on a miss, the checks and publications after the push.
- AC2. Every stop is compared once the pushed head's diff is available,
  whatever order presentation and publication arrive in.
- AC3. The course passes 30 of 30 on macOS under suite load.
