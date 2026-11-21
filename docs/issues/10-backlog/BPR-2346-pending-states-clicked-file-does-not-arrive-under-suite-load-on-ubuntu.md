---
schema: "implement-rfc.card.v1"
id: "BPR-2346"
title: "The pending-states course's clicked file did not arrive within 90 s under suite load on the Ubuntu guest"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2346--BPR-2347--8f9f14ff6fa8.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2346: The pending-states course's clicked file did not arrive within 90 s under suite load on the Ubuntu guest

## Finding

In the comprehensive Release suite at build 73 (`3b68c7dec`) on the Intel
Mac's Ubuntu 26.04 Lima guest, `application_pending_states_public_path`
failed after 171 s: "comparison after 90 s: waiting 'review.cpp' held
'review.cpp' context 0 result 0 look 3", then "FAILED: the clicked file
arrives". Run alone three times on the same build it passed each time
(129 s). Not seen on macOS or Windows.

## Next step

Find what holds the clicked file's comparison under load: the course's logs
are kept under the guest's `course-logs/pending-states-20261121T151726905-*`.
