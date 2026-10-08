---
schema: "implement-rfc.card.v1"
id: "BPR-1330"
title: "The Overview summary resize cursor flickers back to the arrow"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
parked_on: "human-review"
metadata: {"found_by":"owner chat report, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1330--BPR-1330--538b1caef455.json","related_cards":["BPR-0739","BPR-0746","BPR-1312","BPR-1313"],"acceptance":["BPR-1330:hold","BPR-1330:owner-look"]}
acceptance: [{"criterion":"AC1 A Qt test pins that the summary seam and the files and conversations drawer seams show their resize cursor on arrival, again after each move over the seam and after the seam's tooltip opens once the window has been given the arrow, and stop asserting it once the pointer has left; with the hold removed it fails","outcome":"pass","revision":"8f3452054","evidence":{"kind":"command","detail":"At 8f3452054 (rebased onto 6bfde1e6d and rebuilt), serially: ctest --preset dev -R '^(architecture_boundaries|application_overview_summary_.*|qt_composed_github_scale_100|qt_pull_request_overview_page|qt_workbench_window_scale_(100|125|200))$' is 9 of 9. Control: with hold_resize_cursor's install replaced by return, qt_pull_request_overview_page fails 'summary seam, after a move over the seam: handle underMouse 1, window cursor 0, seam cursor 11' and qt_workbench_window_scale_100 fails 'conversations seam, after a move over the seam: handle underMouse 1, window cursor 0, seam cursor 12'. A -j4 run of the same nine had five red that pass serially; tests/AGENTS.md asks for serial Qt fixtures. clang-format-diff is clean on the diff; tidy.py --warnings-as-errors reports nothing in resize_handle_cursor.cpp, and only existing lines (1167-1170, 1217-1218) in pull_request_overview_page_tests.cpp."}},{"criterion":"AC2 The owner rests and moves the pointer on the Overview summary seam and the conversations drawer seam in the macOS app and the resize cursor stays","outcome":"pending","revision":"8f3452054","evidence":{"kind":"observation","detail":"Not yet observed. The offscreen check cannot see AppKit; the owner looks at a macOS build carrying 8f3452054."}}]
---

# The Overview summary resize cursor flickers back to the arrow

The owner, in chat on 2026-10-08, verbatim:

> The resize handle mouse cursor on the overview page below summary seems to
> flicker to the correct cursor and then return to the normal pointer like
> the drawer resize handle used to

The seam is `overview_summary_handle`
(`src/adapters/qt/src/overview_summary_splitter.hpp`), added with
[[BPR-1312]] and persisted by [[BPR-1313]].

## How the drawer's flicker was fixed

It was not, in code. [[BPR-0739]] drew the seam rule and [[BPR-0746]]
diagnosed cursor *delivery*, but neither changed the cursor path: the owner
reported the drawer cursor working on 2026-09-12 on a program identical on
that path to the one he had rejected, and BPR-0746 closed with its probe
withdrawn and hypothesis 1 open -- Qt applies a seam's cursor once, on
arrival; `QWindowPrivate::setCursor` returns early for an unchanged shape and
`QCocoaWindow::setWindowCursor` for an unchanged `NSCursor`, so a cursor the
platform replaces while the pointer stays on the seam is never put back.
`git log` and `grep` find no later change to the drawer seam's cursor
handling either. There was no fix to copy.

## What differs on this seam

Read offscreen at this card's base (`2f91aecbb`), Qt's own record holds: with
the pointer delivered through the window system onto the summary seam and
moved over it eight times, 300 ms apart, the handle stays under the pointer,
its geometry does not move, and the window's cursor stays `SplitVCursor`
throughout. The flicker is therefore below Qt's widget layer, as BPR-0746
found for the drawer, and cannot be observed offscreen. What this seam has
that the drawer seams do not is a tooltip, focus and an explicit `setCursor`;
none of these changes Qt's record, and a tooltip window opening is one
plausible way for AppKit to replace the cursor.

## Change

`hold_resize_cursor` (`src/adapters/qt/include/bpr/qt/resize_handle_cursor.hpp`,
`src/adapters/qt/src/resize_handle_cursor.cpp`, the macOS step in
`resize_handle_cursor_mac.mm`) is installed by both seams: the summary seam
and `workbench_splitter_handle`, so the drawer seams and the summary seam
share one cursor path. On the pointer's arrival, every move over the seam and
the seam's tooltip, it waits for that event to be handled and then puts the
seam's cursor back: on the native window if Qt's record has drifted from the
seam's shape, and on macOS by asking Qt's own view for a cursor update, which
sets the `NSCursor` the view already holds. It does nothing while the pointer
is off the seam or the seam is disabled, and never shows another shape in
between. The comparison's `diff_link_map` handle is a map with its own
pointer behaviour and is not changed.

## Acceptance

- AC1: `tests/support/resize_cursor_hold_check.hpp`, run by
  `test_summary_seam_holds_its_cursor` (`qt_pull_request_overview_page`) and
  `test_the_drawer_seams_hold_their_cursor`
  (`qt_workbench_window_scale_{100,125,200}`). The offscreen platform has no
  cursor of its own, so the check gives the window the arrow, as the owner's
  platform did, and requires the seam's cursor back after each move and after
  the tooltip. With `hold_resize_cursor` reduced to nothing both fail at the
  first move: summary seam window cursor 0 against 11, conversations seam 0
  against 12.
- AC2: the owner's look in the macOS app. The offscreen check exercises the
  hold's Qt-level re-application and its triggers; it cannot observe AppKit,
  so whether this removes the flicker the owner sees is his to say.

## Evidence

AC1 passes at `8f3452054`, rebased onto `6bfde1e6d` and rebuilt, run serially: `architecture_boundaries`,
`qt_composed_github_scale_100`, `qt_pull_request_overview_page`,
`qt_workbench_window_scale_{100,125,200}` and
`application_overview_summary_{1,1_25,2}`, 9 of 9. The control, with the
hold's install replaced by `return`, is red on both seams at the first move
(window cursor 0 where 11 and 12 are required). A `-j4` run of the same nine
had five red that pass serially.

AC2 is pending. Parked on human review: the owner's look on a build carrying
`8f3452054`. No attended session was prepared by this slice.
