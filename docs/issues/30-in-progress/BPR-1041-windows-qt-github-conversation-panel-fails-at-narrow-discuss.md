---
schema: "implement-rfc.card.v1"
id: "BPR-1041"
title: "Windows: qt_github_conversation_panel fails at \"narrow Discuss begins in the focused conversation index\""
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win rows under BPR-1029","found_at":"7d68d5a3","reproduced_at":["7d68d5a3","04d22b4a"],"related_cards":["BPR-1029"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
---

# BPR-1041: Windows: qt_github_conversation_panel fails at "narrow Discuss begins in the focused conversation index"

## Defect

`test_index_arrows_retain_focus_until_explicit_activation`
(`tests/adapters_qt/github_conversation_panel_tests.cpp:581`) sizes the panel
to 500 × 900, enters Discuss, and expects the narrow layout: the cards pane
visible and focused, and the discussion column hidden. On Windows the
discussion column is visible, so the check at `:593` fails.

The panel's layout rule is font-relative. It is narrow when
`width() < fontMetrics().averageCharWidth() * 85`
(`src/adapters/qt/src/github_conversation_panel.cpp:549`). The test's width
is a fixed 500 pixels. A local probe (not committed) printed the values on
Windows, offscreen:

```text
cards-visible=1 discuss-visible=1 cards-focus=1 active=1 platform=offscreen
width=500 avg-char=5 threshold=425 font=Sans Serif@9
```

The default font's average character width is 5 pixels here, so the
threshold is 425 and a 500-pixel panel is wide. The test's width counts as
narrow only when the average character width is at least 6 pixels
(threshold 510 or more). Focus is fine; only the layout assumption fails.
The product behaves as designed: at Windows' font size, 500 pixels is wide
enough for both panes.

## Evidence (bpr-main-win)

- At `04d22b4a`: fails as registered (contained, 1.26 s) and 3 of 3 run
  directly (uncontained, offscreen), so containment plays no part.
- It failed at the same line in [[BPR-1029]]'s measurement on `7d68d5a3`'s
  source, including with the cards view rebuilt from `7bf7de5b`'s source.
  So it is older than BPR-1029's change. The logs are station records under
  `build/logs/{bpr-1029-main-win,bpr-1038-1041-main-win}/` and are not
  committed.

## Scope

Make the test state the width it means in the unit the rule uses. For
example, size the panel to `averageCharWidth() * 60` for the narrow case,
and check the wide case too. Do not change the product threshold to fit a
test.

## Acceptance

- AC1 `qt_github_conversation_panel` passes on Windows N of N (N ≥ 3),
  contained as registered.
- AC2 The test still fails if the panel shows both panes when it is narrow
  by its own rule (a negative control, not committed).
- AC3 macOS and Linux still pass the row.
