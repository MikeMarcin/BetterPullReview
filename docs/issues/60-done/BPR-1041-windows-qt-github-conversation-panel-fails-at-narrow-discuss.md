---
schema: "implement-rfc.card.v1"
id: "BPR-1041"
title: "Windows: qt_github_conversation_panel fails at \"narrow Discuss begins in the focused conversation index\""
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win rows under BPR-1029","found_at":"7d68d5a3","reproduced_at":["7d68d5a3","04d22b4a"],"related_cards":["BPR-1029"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
acceptance: [{"criterion":"AC1 qt_github_conversation_panel passes on Windows N of N (N >= 3), contained as registered","outcome":"pass","revision":"47fdc0ed","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, offscreen: contained as registered, passes 3 of 3 with --repeat until-fail:3 (5.2-6.4 s), before and after a negative-control round trip; before the fix it failed at 'narrow Discuss begins in the focused conversation index' (1.26 s) because the fixed 500 px panel exceeded the 425 px narrow threshold at a 5 px average character width.","path":"docs/issues/60-done/BPR-1041-windows-qt-github-conversation-panel-fails-at-narrow-discuss.md#fixed-in-the-test-bpr-main-win-2026-09-23"}},{"criterion":"AC2 The test still fails if the panel shows both panes when it is narrow by its own rule (negative control, not committed)","outcome":"pass","revision":"47fdc0ed","evidence":{"kind":"command","detail":"With github_conversation_panel.cpp's rule forced to narrow = false (local, never committed), the suite fails at 'narrow Discuss begins in the focused conversation index' after the new precondition check passes; the product source was restored and the tree rebuilt, and the row passes again 3 of 3.","path":"docs/issues/60-done/BPR-1041-windows-qt-github-conversation-panel-fails-at-narrow-discuss.md#fixed-in-the-test-bpr-main-win-2026-09-23"}},{"criterion":"AC3 macOS and Linux still pass the row","outcome":"pass","revision":"47fdc0ed","evidence":{"kind":"freeform","detail":"Not run from this station. The change is test-only: on those platforms the panel becomes 60 average characters wide (narrower than the previous 500 px wherever the font averages under 8.3 px), which is narrow by the same unchanged product rule under which the row passed there; a font that ever made it wide would now fail by name at the precondition check."}}]
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

## Fixed in the test (bpr-main-win, 2026-09-23)

`test_index_arrows_retain_focus_until_explicit_activation` now sizes the
panel to 60 average characters of its own font, instead of a fixed 500
pixels. It also asserts the precondition by name before the layout check:
"the panel is narrower than 85 average characters, which is narrow by its own
rule". The product threshold is unchanged. On Windows the panel is now 300
pixels wide, against a threshold of 425. On a platform whose font averages 7
pixels it would be 420, against 595.

**Rows (`build/dev`, MSVC 19.51 Debug, Qt 6.11.2, offscreen).** The logs are
station records under `build/logs/bpr-1038-1041-main-win/` and are not
committed.

- `qt_github_conversation_panel`, contained as registered, passes 3 of 3
  (`--repeat until-fail:3`, 5.2–6.4 s). Before the fix it stopped at this
  check after 1.26 s. The rest of the suite, which never ran before, passes
  too.
- **Negative control (not committed).** With the product rule forced wide
  (`narrow = false`), the suite fails at "narrow Discuss begins in the
  focused conversation index". So the test still refuses a panel that shows
  both panes when it is narrow by its own rule. The product source was
  restored and the tree rebuilt, and the row passes again, 3 of 3.

`clang-format --dry-run --Werror` is clean. macOS and Linux were not run from
this station. There the panel gets narrower than it was: 60 average
characters is less than 500 pixels wherever the font averages under 8.3
pixels. It is still narrow by the same rule. And the new precondition check
names the failure if a font ever makes it not narrow.

## Post-close: macOS at `14afcdee` (Mac main station, 2026-09-23)

This is the macOS half of AC3, which the Windows station reasoned from code.
macOS 26.6.2 arm64, `build/dev` (Apple clang 21.0.0, Debug, Qt 6.11.2), offscreen, serial (`ctest -j1`), as registered. `14afcdee` contains `47fdc0ed`. It also contains [[BPR-1043]]
(`7b5d78be`), which changed the conversation panel after this fix.

`qt_github_conversation_panel` passes 7 of 7: once in the batch of this
card's siblings (0.78 s), once verbose, and 5 of 5 with
`--repeat until-fail:5` (0.51–0.77 s). The narrow-Discuss test has no
platform guard, so its new precondition check ("the panel is narrower than
85 average characters, which is narrow by its own rule") and the layout
check ran. Linux is still reported only by reasoning.
