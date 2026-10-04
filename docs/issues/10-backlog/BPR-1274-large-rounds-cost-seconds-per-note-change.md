---
schema: "implement-rfc.card.v1"
id: "BPR-1274"
title: "A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1184 large-project course, macOS arm64 Release at 397908dec","reservation":"docs/issues/_reservations/ BPR-1274--BPR-1274--d78638593452.json","related_cards":["BPR-1184","BPR-1161"]}
---

# BPR-1274: A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes

## Observation

[[BPR-1184]]'s `application_review_cycle_large_project` stress round (514 items)
measured, on macOS arm64 Release at `397908dec`
(`docs/verification/BPR-1184-large-project-2026-10-04.md`):

- each note change about 3 s;
- freezing the round 4.3 s;
- opening Share notes freezes the window for 234.8 ms, over the 100 ms input
  target.

BPR-1184 recorded these without changing them.

## Outcome

Find where each cost goes (profile first), make note changes and freezing scale
with the change rather than the round, and bring Share notes' open under the
100 ms target at 514 rows, or record a measured, named reason it cannot. Add the
case to the large-project course so it stays enforced in optimized builds.
