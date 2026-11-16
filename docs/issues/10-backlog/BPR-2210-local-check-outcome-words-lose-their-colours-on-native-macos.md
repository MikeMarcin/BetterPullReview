---
schema: "implement-rfc.card.v1"
id: "BPR-2210"
title: "A local check report card's Passed and Failed words are not drawn in their outcome colours on native macOS"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0120 verification, 2026-11-16 (docs/verification/RFC-BPR-0120.md, Observed, not judged)","related_cards":["BPR-2083","BPR-2189"],"reservation":"docs/issues/_reservations/BPR-2210--BPR-2212--280bc3e89003.json"}
---

# BPR-2210: A local check report card's Passed and Failed words are not drawn in their outcome colours on native macOS

[[BPR-2189]]'s verification renders showed the outcome word on a local draft check report card ("Passed", "Failed") in the ordinary text colour.

Where it lives: `src/adapters/qt/src/local_check_card.cpp`. `local_check_card::outcome_colour` picks a light or dark green or red and `QPalette::WindowText` of the outcome label is set to it (around lines 61-176). On native macOS the style draws a `QLabel` from its own palette and overrides the label palette, so the colour does not show. The same code is expected to work under Fusion.

## Acceptance

- AC01: On native macOS, in Light and Dark, each outcome word is drawn in its outcome colour, or the card carries the outcome some other way that survives the native style (for example a stylesheet colour or a custom paint).
- AC02: A render check reads back the drawn colour of the word, not the label palette.
- AC03: Other platforms are unchanged.
