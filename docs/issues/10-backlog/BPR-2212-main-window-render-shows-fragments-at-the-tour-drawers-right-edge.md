---
schema: "implement-rfc.card.v1"
id: "BPR-2212"
title: "The main-window render shows fragments at the Tour drawer's right edge"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0120 verification, 2026-11-16 (docs/verification/RFC-BPR-0120.md, Observed, not judged)","related_cards":["BPR-2083","BPR-2189"],"reservation":"docs/issues/_reservations/BPR-2210--BPR-2212--280bc3e89003.json"}
---

# BPR-2212: The main-window render shows fragments at the Tour drawer's right edge

The offscreen main-window render with the Tour drawer open shows fragments at the drawer's right edge. A possible cause is the drawer being wider than the space the window gives it. Pre-existing; not caused by RFC-BPR-0120, and not yet reproduced or diagnosed.

Where it lives: drawer width, clamping and bounds in `src/adapters/qt/src/workbench_window_settings_and_layout.cpp` (the Tour chat width and `tour_chat_drawer_minimum_width` / `tour_chat_drawer_maximum_width`, around lines 729-948) and `src/adapters/qt/src/drawer_parts.cpp` (the Tour drawer glyph and panel parts).

## Acceptance

- AC01: A render at the sizes used for the RFC-BPR-0120 look is reproduced and the fragments are named (clipped child, overlap with the centre, or a stale paint).
- AC02: The drawer's width never exceeds the space left for it, and no fragment draws outside it.
- AC03: A test fixes the cause at the window sizes that showed it.
