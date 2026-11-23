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

## 0.5.2 investigation

The same Release course segfaulted again on Windows build 85
(`bf6e5a70a`). The two failures took 6.77 and 6.81 seconds and
emitted no test output. Build 76 ran with CTest's serial property. An
isolated rerun passed on build 85. Windows did not retain a crash stack.

Both failed fixtures saved the ten-stop draft at revision 20. The first
stop had been re-pinned and the second still held its original unmapped
source. The fixture pumped the Qt event loop once after re-pinning and
clicked the next relocation control through an unchecked lookup. The
lookup can return null while the repair projection hides and rebuilds the
controls. This is the strongest explanation for the two failures.

The fixture now waits for the relocation control to be visible and
enabled before clicking it. In five serial Windows Release pairs, the
original fixture passed four runs and reproduced an access violation in
one (0xC0000005 at 6.005 seconds). The updated fixture passed all five
runs with the full publication and repair checks and both screenshots.
Continue to run the course in the Windows
Release suite and retain a native stack if the failure recurs. The
0.5.1 exception applies to that release.
