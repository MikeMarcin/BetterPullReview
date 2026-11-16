---
schema: "implement-rfc.card.v1"
id: "BPR-2199"
title: "On Windows, the Tour tab shows every stop of the route with Segoe UI 9 pt"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-2197's Windows width courses, Windows station over SSH, 2026-11-15","reservation":"docs/issues/_reservations/BPR-2199--BPR-2200--d19a716e3a2b.json","related_cards":["BPR-2197","BPR-2200"],"kind":"product bug"}
---

# BPR-2199: On Windows, the Tour tab shows every stop of the route with Segoe UI 9 pt

## Found

BPR-2197's Windows width courses were settled on the Windows station on
2026-11-15. There the window course's Tour case passed with Qt's offscreen
fallback font and failed once the application font was Segoe UI 9 pt, the
font a Windows desktop gives:

    FAILED: the Tour tab is the route: every stop in view, no list of its own to scroll

The case is `test_the_tour_is_a_route_expanding_to_its_page` in
`tests/adapters_qt/workbench_window_tests.cpp`, Release, lane
`claude/v0-5-0-prerelease-targets-52265d` at `22220f4f0`.

Treat it as a product bug. With the real Windows font, either the Tour tab's
stops do not all fit in view, or the tab grows a list of its own that
scrolls. With the fallback font (no fonts loaded, about twice as wide) the
same case passes. So the layout depends on font metrics that only a real
Windows font exposes.

## Reproduce on the Windows station

1. Export the lane to a scratch folder beside the station checkout.
   - Clone: `git clone --shared --no-checkout C:/code/git/BetterPullReview SCRATCH/src`.
   - On the Mac, `git bundle create lane.bundle BASE..HEAD`, where BASE is a
     commit the station has; fetch it into the clone and check it out.
2. Configure:
   `cmake --preset windows-dev -DCPM_SOURCE_CACHE=C:/code/git/BetterPullReview/build/.cpm-cache`
3. Build:
   `cmake --build build/windows-dev --config Release --target test_better_pull_review_qt_workbench_window_tests -- -m:8`
   - In Git Bash, write MSBuild switches with a dash (`-m:8`); Git Bash
     rewrites `/m:8` into a path.
4. Put two folders on `PATH`:
   - `build/windows-dev/Release`, for `bpr_xdiff.dll`;
   - Qt's `.cpm-cache/bprqt/6.12.0/6.12.0/msvc2022_64/bin`.
5. Run the case directly, with ctest's environment and the fonts loaded:
   `QT_QPA_FONTDIR=C:/Windows/Fonts QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.0 BPR_EXPECTED_QT_SCALE=1.0 BPR_WORKBENCH_ONLY_CASE=test_the_tour_is_a_route_expanding_to_its_page better_pull_review_qt_workbench_window_tests.exe`
   - The application font must be Segoe UI 9 pt. BPR-2200 makes that the
     test set's default on Windows. Until then, set it in the test's `main`,
     as the `--title-row` courses do.
6. Remove the scratch folder afterwards.

ctest itself cannot run this over SSH. Its network sandbox
(`tool_better_pull_review_network_sandbox`) fails to start the test process
in an SSH session with 0xc0000142 (DLL initialization failed), before the
test runs. Run the executable directly with the environment ctest would set,
or run ctest from a desktop session.

## Scope and acceptance

- With Segoe UI 9 pt on Windows, the Tour tab shows every stop of the route
  in view, with no list of its own to scroll, as on macOS and Linux.
- The case passes on the Windows station with the fonts loaded, and still
  passes on macOS and Linux.
- Architecture: presentation only, in Qt.

## Intended verification

`test_the_tour_is_a_route_expanding_to_its_page` on the Windows station with
the fonts loaded (after BPR-2200, the ordinary window course), and on macOS
and Linux. A native render of the Tour tab at Segoe UI 9 pt if the layout
changes.

## Cause and fix (2026-11-15)

The font was not the fault; it only exposed one. `fit_route` sets the
route's height to the sum of `sizeHintForRow` over its stops.
`sizeHintForRow` hands the delegate an option with no rect, so
`route_delegate::sizeHint` measured each stop's excerpt at a guessed 300
pixels. The list itself lays the rows out at its viewport's width, 268 in
this drawer. An excerpt that fits one line at 300 but needs two at 268 was
summed one line short, and the route scrolled by the difference. In this
case that was 72 against 84 pixels for the first stop, with a scroll maximum
of 8.

The offscreen fallback font is about twice as wide, so its excerpt took
two lines at either width and the case passed. The same fault reproduced
on macOS with narrow fonts: Helvetica 8 and 9, Arial 9 and Times 9 failed,
while Helvetica 10 to 14 and Verdana 9 passed.

Fix (`4bfbbd063`, presentation only): with no width in the option, the
delegate measures at the view's viewport width, and only falls back to 300
when it has no view. The case keeps its check unchanged and adds the
invariant that held the bug: every route row measures what the list lays it
out at (`sizeHintForRow(row) == visualItemRect(row).height()`).

## Verification

- macOS, before the fix, with the application font forced through a
  temporary local hook (not committed): Helvetica 9 had route rows 72/84
  and scroll maximum 8, and failed. After: rows 84/84 and maximum 0, and
  every font listed above passes.
- macOS `dev`, after the fix: `qt_workbench_window_scale_100/125/200`,
  `qt_drawer_surfaces_fit_scale_100/125/200`, every `qt_*tour*` course
  (`qt_review_tour_*`, `qt_tour_*`) and
  `application_local_draft_tour_desktop_public_path`, 20 of 20 passed.
  `qt_workbench_window_scale_*` passed again at `2c17d36f6`.
- Windows station, Release, `18a70bb96`, with the fonts loaded and Segoe UI
  9 pt through BPR-2200's startup:
  `BPR_WORKBENCH_ONLY_CASE=test_the_tour_is_a_route_expanding_to_its_page`
  passes. With BPR-2200's 1280 correction, the whole window course passes
  at 100%, 125% and 200%. It was run directly with ctest's environment,
  since ctest's sandbox cannot start a process over SSH.
- Linux: not run. The change is platform-neutral Qt layout, and the macOS
  courses above cover it.
- No layout change, so no new render: the route now holds the height it
  was always meant to.
