---
schema: "implement-rfc.card.v1"
id: "BPR-2199"
title: "On Windows, the Tour tab shows every stop of the route with Segoe UI 9 pt"
status: "Ready"
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
