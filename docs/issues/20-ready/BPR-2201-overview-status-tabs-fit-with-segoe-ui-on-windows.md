---
schema: "implement-rfc.card.v1"
id: "BPR-2201"
title: "On Windows, Overview's Status drawer tabs fit their bar with Segoe UI 9 pt while a tour is read"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-2200's Windows Qt run with the fonts loaded, Windows station over SSH, 2026-11-15","reservation":"docs/issues/_reservations/BPR-2201--BPR-2203--f448700c62dd.json","related_cards":["BPR-2200","BPR-2197"],"kind":"product bug"}
---

# BPR-2201: On Windows, Overview's Status drawer tabs fit their bar with Segoe UI 9 pt while a tour is read

## Found

BPR-2200 made every Windows Qt course run offscreen with the Windows fonts
and Segoe UI 9 pt. With them, `qt_drawer_surfaces_fit_scale_100`, `_125`
and `_200` fail; with no fonts they fail earlier, on the Agent tab. Lane
`claude/v0-5-0-prerelease-targets-52265d` at `18a70bb96`, Release:

    CLIPPED: Overview's tour-reader light at 340 (tour: reader) header: clipped: QTabBar 'overviewStatusTabs''s tab 4 "Agent" ends at 310 in a 262 px bar (hint 310, header 340)
    CLIPPED: Overview's tour-reader light at 560 (tour: reader) header: clipped: QTabBar 'overviewStatusTabs''s tab 4 "Agent" ends at 310 in a 262 px bar (hint 310, header 560)
    FAILED: 16 surface states are clipped or break the principles

All 16 states are Overview's tour-reader surface (four themes, 340 and 560)
and each is clipped the same way. The bar is 262 pixels wide even in a
560-pixel header, while its tabs need 310. So the fold
(`overview_status_drawer::fit_tabs`, `drawer_tab_bar::fold`) picks a form
it measures at 262 that the tab bar then lays out at 310. The same states
pass on macOS.

## Scope and acceptance

- With Segoe UI 9 pt on Windows, Overview's Status drawer tabs fit their bar
  in every tour-reader state: the fold's measure of a form matches the
  width the tab bar lays it out at.
- `qt_drawer_surfaces_fit_scale_100/125/200` pass on the Windows station
  with the fonts loaded, and still pass on macOS and Linux.
- Presentation only, in Qt.

## Reproduce on the Windows station

As BPR-2200's card describes: a scratch clone and a lane bundle, `windows-dev`
with the existing dependency cache, Release, `-m:8` in Git Bash. Build
`test_better_pull_review_qt_drawer_surfaces_fit_tests`. Then, with the two DLL
folders on `PATH`:
`QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.0 BPR_EXPECTED_QT_SCALE=1.0 better_pull_review_qt_drawer_surfaces_fit_tests.exe`.
The fonts load without any further setting (`windows_fonts_startup.cpp`).
