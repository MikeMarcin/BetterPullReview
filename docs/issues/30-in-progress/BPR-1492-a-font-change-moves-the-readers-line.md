---
schema: "implement-rfc.card.v1"
id: "BPR-1492"
title: "A font change or zoom moves the reader's line"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# A font change or zoom moves the reader's line

Two rows fail on main at cd4860e30, in the full gate and again alone:

- `qt_appearance_profile`: "the reprojected position keeps its comparison
  and visible reading line within one row" (BPR-1336 C4).
- `application_diff_zoom_public_path`: "the reader's line stays where it
  was: 249 to 271".

Both say a font change (appearance or diff zoom) no longer keeps the visible
reading line in place.

## Acceptance

- AC1: The cause is named with the commit that introduced it, and the fix
  keeps the reader's visible line within one row across a font change and a
  zoom, as BPR-1336 C4 requires, without loosening either assertion.
- AC2: Both rows pass on macOS, and the neighbouring scroll and zoom rows
  still pass.

## Boundaries

If the product behaviour is correct and a test reads it wrongly, say so
plainly and fix the test only.

## Evidence

Cause: two defects on the font-change hold path, both introduced by
60aa86361 (BPR-1413, native fractional diff origins). The product was wrong;
both test assertions read correct behaviour and are unchanged.

- `source_pane::visible_origin().second` became `contentOffset().y() -
  AppliedOffset`, so it now carries the within-line offset. The BPR-1336
  `diff_workbench::hold_reading_position` was not updated: it still
  subtracted that value when it placed the new top line, which lands flush
  with no within-line offset. The reader's line moved down by the old offset
  scaled to the new row height. In `qt_appearance_profile` under ctest
  (offscreen, 15 px to 20 px rows, 12 px offset) line 39 moved from 265 to
  287, more than one 20 px row.
- The new `source_pane::scrollContentsBy` override cleared the within-line
  offset before calling `QPlainTextEdit::scrollContentsBy`. Clearing it
  resizes the viewport (the placement height follows the new font's line
  spacing), and `QPlainTextEdit`'s resize readjusts the bar to the top line
  the base last recorded, which was the old one. A backtrace on the left bar
  showed `set_top_line(21)` reverted to 20 inside that resize. A single font
  change survived, because the remeasured axis re-placed the line, but the
  second Zoom In in one turn noted its position from the reverted top line,
  so `application_diff_zoom_public_path` moved line 37 from 249 to 271.

Fix (`src/adapters/qt/src/diff_workbench.cpp` only, no `src/app` change):

- `hold_reading_position` computes the new top line from the noted offset
  alone and clears the pane's within-line offset after `set_top_line`, so the
  temporary whole-line placement matches the computation.
- `scrollContentsBy` calls the base class first, then clears the within-line
  offset.

Each change alone repairs exactly one row: with only the reorder,
`qt_appearance_profile` still fails; with only the hold formula,
`application_diff_zoom_public_path` still fails with "249 to 271".

Before (main a8fd17aea, same code as cd4860e30 on this path):

```sh
ctest --preset dev -R 'qt_appearance_profile$|application_diff_zoom_public_path' --output-on-failure
```

- `qt_appearance_profile`: Failed, "the reprojected position keeps its
  comparison and visible reading line within one row".
- `application_diff_zoom_public_path`: Failed, "the reader's line stays where
  it was: 249 to 271".

After, the same command: both Passed. With the instrumentation still in,
line 39 sat at 265 before and 267 after the font change, against a 20 px row.

Neighbouring scroll, zoom, reading and appearance rows, after the fix:

```sh
ctest --preset dev -R 'qt_diff_workbench_scale|qt_diff_link_map_scale|qt_comparison_scroll_controller_scale|qt_appearance_settings_panel$|qt_appearance_role_display|qt_workbench_conversation_reveal|application_appearance_host|application_appearance_profile_consumers|application_diff_workbench_public_path|application_cpp_identifier_appearance|application_code_font|qt_appearance_profile$|application_diff_zoom_public_path|application_appearance_gesture_contract|application_native_appearance_probe_contract|application_appearance_theme_customization_contract|application_appearance_older_theme_contract|application_appearance_store_fallback_contract' --output-on-failure
```

24 of 24 passed, including `qt_diff_workbench_scale_{100,125,200}` (the
BPR-1413 fractional-origin and pan tests) and
`qt_comparison_scroll_controller_scale_{100,125,200}`. The core and SQLite
appearance rows do not link the changed file and were not run.
