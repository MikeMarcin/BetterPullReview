---
schema: "implement-rfc.card.v1"
id: "BPR-1476"
title: "Down from a line far out of view reveals the next line"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: []
metadata: {"kind":"defect","found_by":"application_large_review_responsiveness on main in a Release Qt build, six runs before and after RFC-BPR-0080's adoptions (BPR-1446, BPR-1447)","reservation":"docs/issues/_reservations/BPR-1476--BPR-1476--5b5a33ca9d76.json","related_cards":["BPR-1212","BPR-1413"],"related_rfcs":["RFC-BPR-0073"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b85b2f48d","evidence":{"kind":"command","path":"tests/adapters_qt/diff_workbench_tests.cpp","command":"ctest --test-dir build/app-release-tests -R '^qt_diff_workbench_scale_(100|125|200)$'","detail":"test_keyboard_reveal_from_far_out_of_view: 3,000 lines in a 520 px view, equal and unequal comparisons, split view on each pane and unified view on each side; Down from the line two thirds down selects the next and shows it flush with the bottom within one device pixel, Up from the line one third down (far above) shows the previous flush with the top, no ribbon mismatch. Passes at all three scales, also on the merge with main at b97e3349a."}},{"criterion":"AC2","outcome":"pass","revision":"b85b2f48d","evidence":{"kind":"command","path":"tests/adapters_qt/diff_workbench_tests.cpp","command":"ctest --test-dir build/app-release-tests -R '^qt_diff_workbench_scale_(100|125|200)$'","detail":"On the claimed base all three scales fail at 'Down from a line far below the view reveals the next flush with the bottom'; with the split-view fix alone the unified case still failed 4 px short (the document margin); with both, all pass."}},{"criterion":"AC3","outcome":"pass","revision":"b85b2f48d","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","command":"ctest --test-dir build/app-release-tests -j1 -R '^application_large_review_(responsiveness|launch)$'","detail":"Release, macOS arm64. Base: keyboard_line_reveal expired at the 90 s course clock (twice). Fixed: responsiveness passed in four runs with no other test running (keyboard_line_reveal 12.7-15.5 ms, no budget failures), launch in two; two runs inside parallel batches at load 36-45 missed completion budgets only on phases this change does not touch. test_keyboard_minimal_reveal, test_fractional_unified_and_keyboard_viewports, test_the_keyboard_line_cursor_and_return and test_fractional_native_viewport still pass; the focused qt_ and public-path rows pass except three that fail identically on main's sources (qt_colour_role_list, qt_appearance_profile, qt_profile_management_window)."}}]
---

# Down from a line far out of view reveals the next line

`application_large_review_responsiveness` fails on main in every Release run
seen: "Down from a line out of view did not reveal the next". The course
selects line 10,000 of the 20,000-line comparison without scrolling, presses
Down, and waits for line 10,001 to be selected and shown. The selection moves,
but the line never comes into view, and the wait expires at the 90 s course
clock.

## Acceptance

- AC1: Down from a selected line far below the view, and Up from one far above
  it, select the next line and reveal it at the nearest displayed edge, as
  RFC-BPR-0073's keyboard reveal requires for a clipped row, in split view
  (either pane, equal and unequal comparisons) and in unified view.
- AC2: A Qt test fails on the code before the fix and passes after it.
- AC3: `application_large_review_responsiveness` passes its keyboard reveal in
  a Release build, and the existing keyboard-reveal and fractional-viewport
  tests still pass.

## Cause

`59a2fa10c` ("Fix minimum keyboard reveal and adaptive scroll axes",
RFC-BPR-0073's AC08) replaced the keyboard cursor's exact reveal with a
minimal one. That reveal measures the target row with `line_row_rect`, which is
`QPlainTextEdit::blockBoundingGeometry`. Qt answers that by walking blocks from
the first visible one, and it stops about two viewport heights below it (one
above), returning the rect of the block it stopped at. For a row further away
the rect is wrong but still off screen, so the reveal scrolled by a bounded
amount and stopped short. A diagnostic print in the course showed it: line
10,001's row reported at y = 1517 in a 736 px view with line 18 at the top, and
after Down the top line was 72. No core decision was wrong. The core selected
line 10,001; the Qt presentation of the reveal measured the wrong row.

In split view, the reveal's origin already came from the measured axis's
line tops (`measured_document_origin`) while the row came from Qt's walk. The
two now share the axis. The unified pane has no axis, so the row is placed by
its one row stride; its reveal also now counts the document margin that only
row 0 shows. Without that, a reveal starting at the top of the file stopped one
margin short of the edge. The new test's unified case found this, since
`test_keyboard_minimal_reveal` starts below the top.

## Evidence

Implementation `b85b2f48d`, macOS arm64, Qt 6.11.2, Apple clang, Release
(`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON
-DCMAKE_TEST_BUILD_DEPENDS=ON`), the machine under load from other agents
(load average 30 to 45).

- AC1 and AC2: `test_keyboard_reveal_from_far_out_of_view` in
  `tests/adapters_qt/diff_workbench_tests.cpp`: 3,000 lines in a 520 px view,
  equal and unequal (three lines inserted above) comparisons, split view on
  each pane and unified view on each side. It selects the line two thirds down
  without scrolling, presses Down, and requires the next line selected,
  visible and flush with the bottom within one device pixel. It then selects
  the line one third down (now far above), presses Up, and requires the
  previous line flush with the top, with no ribbon mismatch in split view.
  Before the fix, `qt_diff_workbench_scale_{100,125,200}` fail at "Down from a
  line far below the view reveals the next flush with the bottom". With the
  split fix alone, the unified case still failed 4 px short (the document
  margin). After both, all three scales pass, including
  `test_keyboard_minimal_reveal`, `test_fractional_unified_and_keyboard_viewports`,
  `test_the_keyboard_line_cursor_and_return` and `test_fractional_native_viewport`.
- AC3: `application_large_review_responsiveness` reproduced on the claimed
  base (`keyboard_line_reveal` expired at the 90 s course clock, twice). With
  the fix it passed in four runs with no other test running, the last on the
  merge with main at `b97e3349a`: `keyboard_line_reveal` 12.7 to 15.5 ms where
  recorded, no input or completion budget failures.
  `application_large_review_launch` passed in two such runs. Two runs inside
  parallel batches, at load average 36 to 45, missed completion budgets on
  phases this change does not touch (drawers, zoom, startup, rapid selection),
  while their `keyboard_line_reveal` took 16.1 to 41.6 ms against its 100 ms
  target.
- Focused rows: all 146 `qt_` tests,
  `application_first_tab_comparison`,
  `application_review_tour_composition_*`, `application_profile_window_layout_*`,
  `application_overview_summary_*`, `application_accepted_file_in_overview`,
  `application_profile_window_review_restoration_public_path`,
  `application_header_source_navigation{,_large}`,
  `application_review_navigation_entry` and `application_source_pr_find*`
  pass, except three that fail the same way with main's `diff_workbench`
  sources restored: `qt_colour_role_list` ("58 of the 60 roles": `c58bdebda`
  raised the core's count to 62; a follow-up is suggested),
  `qt_appearance_profile` (the font-resize reprojection assertion already
  recorded in `docs/verification/RFC-BPR-0072.md` and BPR-1479) and
  `qt_profile_management_window` (BPR-1304). Rows that the shared disk filling
  up stopped partway were rerun and pass.
- Gates: `_coding-standard/scripts/format.py` (clang-format 22.1.8) on the
  three changed C++ files; clang-tidy 22.1.8 and 23.1.1 on
  `diff_workbench.cpp` and `diff_workbench_tests.cpp` report nothing; the
  documentation gate passes. No `src/app` source changed, so the round-trip
  composition contract needs no reseal.
