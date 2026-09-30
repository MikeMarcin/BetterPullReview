---
schema: "implement-rfc.card.v1"
id: "BPR-1251"
title: "Windows: the document tab strip is narrower than one tab, and the title returns at a narrower width"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run of the main-stabilization Qt rows, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1251--BPR-1251--080a64f21ab7.json","related_cards":["BPR-1242","BPR-1033","BPR-1106"]}
---

# BPR-1251: Windows: the document tab strip is narrower than one tab, and the title returns at a narrower width

Found by bpr-main-win on 2026-09-30: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, offscreen, `build/windows-dev` Debug, at `69493e23`. On macOS these
rows pass since the main-stabilization milestone. On Windows all six fail at
every scale.

## Observation

A probe, not committed, printed the failing measurements at scale 1.0.

**`qt_document_tabs_scale_{100,125,200}`**, `activation reveals the entire
active tab including after restoration`
(`tests/adapters_qt/workbench_window_tests.cpp`, about line 6238):

```
count=1 left=-20 right=199 viewport=200 scroll=20/20
```

One tab at its pinned 220-pixel minimum sits in a 200-pixel viewport. The strip
is narrower than a single tab, so no scroll position can show the whole active
tab. The scroll is already at its maximum. The check is right. The strip is
too narrow, which suggests the controls beside it take more room under Windows
font metrics.

**`qt_workbench_window_scale_{100,125,200}`**, `the title yields once and does
not return at a narrower width` (about line 1749):

```
title returns at width=1099 text=[microsoft/terminal  #20607 Fix the ScreenBufferTests suite so that e…] labelWidth=383
```

The title gave way at a wider window, then reappeared at 1,099 pixels. So
something else in the title row yields at that width and gives the identity
label its room back. The rule this row pins (RFC-BPR-0028, [[BPR-1106]]) is
that the title yields once.

## Scope

- Measure the title row and the tab strip's neighbours at the failing widths
  under Windows metrics. Find what takes the room, and what yields first.
- Fix the layout that owns each rule, not the check.
- Keep macOS and Linux passing.

## Acceptance

- The six rows pass on Windows, and still pass on macOS and Linux.
