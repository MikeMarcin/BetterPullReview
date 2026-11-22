---
schema: "implement-rfc.card.v1"
id: "BPR-2350"
title: "The tour publication repairs course failed once under suite load on Windows"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2349--BPR-2350--4e2da0ee5392.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2350: The tour publication repairs course failed once under suite load on Windows

## Finding

In the comprehensive Release suite at build 76 (`84208973a`, the build
copied to stable 0.5.0) on the Windows qualification PC, `application_review_tour_publication_repairs` failed once.
Run alone three times on the same build in the same interactive context it
passed each time. Not seen on macOS or the Ubuntu guest in that round.

## Next step

Read its kept course logs from the build 76 suite on the PC (`D:\\BetterPullReview-release-v050-stable\\ops\\win76\\suite`) and relate it to the other tour publication races, [[BPR-2339]] and [[BPR-2343]].
