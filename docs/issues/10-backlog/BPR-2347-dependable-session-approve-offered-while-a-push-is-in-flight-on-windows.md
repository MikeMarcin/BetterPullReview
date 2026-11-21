---
schema: "implement-rfc.card.v1"
id: "BPR-2347"
title: "The dependable-session course once saw Approve offered while a push was in flight on Windows"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2346--BPR-2347--8f9f14ff6fa8.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2347: The dependable-session course once saw Approve offered while a push was in flight on Windows

## Finding

In the comprehensive Release suite at build 73 (`3b68c7dec`) on the Windows
qualification PC, `application_dependable_session_public_path` failed once:
"FAILED: #18's Approve is offered while a push is in flight". Run alone three
times on the same build in the same interactive context it passed each time.

## Next step

Decide whether Approve can be offered during a push in flight (a product
race) or the course observes too early, and log the push state at the check.
