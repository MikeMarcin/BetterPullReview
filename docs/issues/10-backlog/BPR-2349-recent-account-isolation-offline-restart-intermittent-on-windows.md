---
schema: "implement-rfc.card.v1"
id: "BPR-2349"
title: "The recent-account isolation course once could not revalidate the account after an offline restart on Windows"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2349--BPR-2350--4e2da0ee5392.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2349: The recent-account isolation course once could not revalidate the account after an offline restart on Windows

## Finding

In the comprehensive Release suite at build 76 (`84208973a`, the build
copied to stable 0.5.0) on the Windows qualification PC, `application_recent_account_isolation` failed once: "FAILED: offline restart cannot revalidate account".
Run alone three times on the same build in the same interactive context it
passed each time. Not seen on macOS or the Ubuntu guest in that round.

## Next step

Log what the offline restart's revalidation waited on and why it gave up, then decide whether the course or the account lease is at fault.
