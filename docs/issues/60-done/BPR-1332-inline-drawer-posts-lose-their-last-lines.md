---
schema: "implement-rfc.card.v1"
id: "BPR-1332"
title: "Inline drawer posts lose their last lines: an empty control row takes the reader's spacing"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"coordinator, 2026-10-08: qt_composed_inline_drawer_scale_{100,125,200} fail on main on macOS and Windows; the owner's v0.1.5 release note names inline-drawer course failures","reservation":"docs/issues/_reservations/BPR-1332--BPR-1332--4db1651fcb2e.json","first_bad_commit":"1f81ec5b4","related_cards":["BPR-1290","BPR-1314","BPR-1083"]}
acceptance: [{"criterion":"BPR-1332:reproduce","outcome":"pass","revision":"88957a791","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^qt_composed_inline_drawer_scale_' --output-on-failure (macOS arm64 Debug, offscreen)","detail":"3 of 3 failed with \"each post's body fits its width and shows all of its lines\". At the 280 px drawer the first post measured reader height 75 = heightForWidth 75, rendered body 71, document 74.06: the body was 4 px short of its text."}},{"criterion":"BPR-1332:bisect","outcome":"pass","revision":"1f81ec5b4","evidence":{"kind":"command","command":"git bisect start origin/main d5c4b3659; git bisect run <build test_better_pull_review_qt_github_tests; ctest -R ^qt_composed_inline_drawer_scale_100$>","detail":"First bad commit 1f81ec5b4 (Reclaim Overview reader space and copy original Markdown on Select All). Its parent side 1868e55b7 passed; 1f81ec5b4, 2499010b0, 5ac277081, a1833efcf and df27af7d7 failed. The Windows station's narrowing (good d5c4b3659) was the good bound; BPR-1305 (7289987eb) precedes it and is not the cause."}},{"criterion":"BPR-1332:cause","outcome":"pass","revision":"1f81ec5b4","evidence":{"kind":"observation","path":"src/adapters/qt/src/markdown_body_view.cpp","detail":"1f81ec5b4 moved the reader's control row from a sub-layout into a Controls widget and showed it whenever context controls were off. In place every child is hidden, but the widget stayed visible, so the vertical layout kept its 4 px in-place spacing between the empty row and the pages. heightForWidth counts only the notice and the document, so the pages got the height less 4 px and the last line was cut. An empty sub-layout had taken no room, which is why the course passed before."}},{"criterion":"BPR-1332:fix-and-control","outcome":"pass","revision":"15abd795b","evidence":{"kind":"command","command":"ctest --test-dir build/dev -j1 -R '^(qt_markdown_body_|qt_composed_inline_drawer_scale_)' with src/ reverted to main and the new test kept; then with only the heightForWidth row term removed","detail":"Without the fix all 6 rows failed (drawer: the post line check; reader: \"an in-place body shows all of its lines at the height for its width\"). Without the row term only, qt_markdown_body_1_0 failed on \"with the mark's row shown, the body still shows all of its lines\". With the fix all pass."}},{"criterion":"BPR-1332:rows","outcome":"pass","revision":"15abd795b","evidence":{"kind":"command","command":"ctest -j1 -R '^(qt_markdown_body_|qt_composed_inline_drawer_scale_|qt_conversation_cards_view$)' in build/dev (Debug) and build/opt (Release); plus qt_composed_markdown_{1_0,1_25,2_0}, qt_pull_request_overview_page and qt_feedback_label_threads in Debug","detail":"Debug 7 of 7 and 9 of 9, Release 7 of 7, macOS arm64, offscreen. The drawer rows pass at 100, 125 and 200 percent in both builds. Rerun 7 of 7 in each build after rebasing over BPR-1212 slice 2 (a90419791, compose in the drawer), at 33677d37d."}}]
---

# BPR-1332: Inline drawer posts lose their last lines

## Defect

`qt_composed_inline_drawer_scale_{100,125,200}` failed on main on macOS and
Windows at "each post's body fits its width and shows all of its lines".
They passed at 399aade8e. This is reader-visible: each open thread post in
the drawer was 4 px shorter than its text, so its last line was clipped.

## Cause

First bad commit: 1f81ec5b4, "Reclaim Overview reader space and copy original
Markdown on Select All" (bisected on macOS from d5c4b3659, the Windows
station's good bound, to main).

That commit wrapped the reader's control row (mark, View source, Copy source)
in a `Controls` widget and showed it whenever the Overview's context controls
were off. In place (thread posts) every child is hidden, but the empty widget
stayed visible, and the vertical layout put its 4 px in-place spacing after
it. `heightForWidth` counts only the notice and the document, so the post's
holder gave the reader exactly its text height and the layout took 4 px of it
from the body.

## Fix

`markdown_body_view::refresh_control_row` shows the row only outside place, or
in place when a Conventional Comments mark shows; `heightForWidth` counts the
row and its spacing when it shows. The Overview's context-controls mode is
unchanged (row hidden).

`check_in_place_height` in `tests/adapters_qt/markdown_body_view_tests.cpp`
places an in-place reader at its height for three widths, with and without a
mark, and fails without either half of the fix.

## Not covered

- A failing course ends with `TEST_RUNNER_CHILD_EXIT:-11`: `expect` calls
  `std::exit` with the window alive, and teardown faults. That is separate
  from this defect and was not changed.
- Windows confirmation is a station run (commands in the handoff), not an
  acceptance criterion here.

## Windows station run (bpr-main-win, 2026-10-08)

At `4cfc15256`, a clean tree that holds `15abd795b`, on Windows 11 with MSVC
and Qt 6.11.2. Both trees rebuilt with no `warning C` line, and the rows ran
`ctest -j1` on the offscreen platform.

- `build/windows-dev-tour-cli` (`windows-dev`, Debug): 7 of 7 pass. These are
  `qt_composed_inline_drawer_scale_100`, `_125` and `_200` (11.7, 12.7 and
  18.2 s), `qt_conversation_cards_view`, and `qt_markdown_body_1_0`, `_1_25`
  and `_2_0`.
- `build/bpr0967-release` (MSVC `/O2`, tests on): the same 7 of 7 pass.

Before the fix, on Windows, `qt_composed_inline_drawer_scale_100` failed at
"each post's body fits its width and shows all of its lines". That was at
`7fc367064`, built with the C4458 rename applied for the measurement. It was
good at `d5c4b3659`.
