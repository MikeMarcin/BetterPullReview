---
schema: "implement-rfc.card.v1"
id: "BPR-2211"
title: "The report file tab stays light in Dark mode"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0120 verification, 2026-11-16 (docs/verification/RFC-BPR-0120.md, Observed, not judged)","related_cards":["BPR-2083","BPR-2189"],"reservation":"docs/issues/_reservations/BPR-2210--BPR-2212--280bc3e89003.json"}
---

# BPR-2211: The report file tab stays light in Dark mode

In the main-window renders for [[BPR-2189]], the tab a report opens in (the existing report viewer's file tab) kept a light face while the rest of the window was Dark.

Where it lives: the tab and its page-glyph icon are built in `src/adapters/qt/src/document_tab_strip.cpp` (`report_glyph`, and the `tab.Report` branch that calls `Bar->addTab`); the page it shows is `src/adapters/qt/src/agent_report_view.cpp` and `report_page.cpp`. Find whether the light face comes from the tab bar's native style, a fixed tab colour, or the glyph being painted from a stale palette. This predates RFC-BPR-0120.

## Acceptance

- AC01: In Dark, a report's tab face, text and glyph use the Dark palette and match the file tabs beside it.
- AC02: Switching Light and Dark while a report tab is open repaints it.
- AC03: A render check reads the tab's drawn colours in both modes.
