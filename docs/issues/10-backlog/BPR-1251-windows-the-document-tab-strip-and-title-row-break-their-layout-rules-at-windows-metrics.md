---
schema: "implement-rfc.card.v1"
id: "BPR-1251"
title: "Windows: the document tab strip is narrower than one tab, and the title returns at a narrower width"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "linux"]
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

## Linux: bpr-main-linux, 2026-10-01

At `db021262a` (WSL2, GCC 15.2, offscreen, `build/dev` Debug), the three
`qt_document_tabs_scale_{100,125,200}` rows fail on Linux at the check
Windows fails: `activation reveals the entire active tab including after
restoration`. They failed in bpr-main-win's BPR-0905 full suite in this
checkout and again in a serial rerun, 0.15 s each. So this card's Linux half
is real, and "still pass on Linux" does not hold at this revision. On Linux
the `qt_workbench_window_scale_*` rows fail at a different check, which
[[BPR-1255]] owns.

### Linux half: measured and fixed (bpr-main-linux, 2026-10-01)

A probe, not committed, at the failing check printed:

```
count=1 width=1100 strip=205 viewport=179 plus=26 tab=220
workbenchCentralSplitter sizes=256,256,205,371 (repositories, files, centre, conversations)
workbenchCentralSurface minimumSizeHint=194
```

With all three drawers open at their Linux metric widths, the splitter gives
the centre 205 pixels. That is less than one 220-pixel tab plus the 26-pixel
plus button. The strip reported a minimum of 0 and used an `Ignored`
horizontal policy, so nothing kept room for the tab. On Windows the same
shape appears as viewport 200.

**Fix (`42e536f67`, in `src/adapters/qt/src/document_tab_strip.cpp`).**
The strip's minimum is one tab at `minimum_tab_width` plus the plus button's
size hint. The strip sets it as its minimum width (an `Ignored` policy makes
layouts read that), and resets it on a font or style change. The splitter
now takes the room from the drawers rather than from the centre. The drawers'
own minimums are 61, 154 and 125 pixels, so the 900-pixel floor still fits.
The workbench specification's rule, "Activating a tab brings all of it into
view", needs that room.

| Check (Linux) | Result |
|---|---|
| `qt_document_tabs_scale_{100,125,200}` | Pass, 0.78, 1.08 and 1.98 s |
| `ctest --preset dev -R '^(qt_\|application_)' -E '^application_composition_contract$' -j10` | 306 of 314 pass. The failures are [[BPR-1255]]'s six and the known [[BPR-1245]] and [[BPR-1236]] rows. |
| clang-format 22.1.8; `tidy.py --paths` on the file | Clean |

The change is portable, so it probably also clears this card's Windows tab
rows. That has not been run. The Windows `qt_workbench_window_scale_*` failure
("the title yields once") is a different check, and it is still this card's.

## Windows after the Linux fix (bpr-main-win, 2026-10-01)

At `6e56ebe17`, which carries `42e536f67`: Windows 11 Home 10.0.26200,
Visual Studio 18, Qt 6.11.2, offscreen, `build/windows-dev` Debug.
`cmake --build --preset windows-dev` printed no `warning C` line. The six
rows ran serially:

```powershell
ctest --preset windows-dev -j1 --output-on-failure -R "^qt_(document_tabs|workbench_window)_scale_(100|125|200)$"
```

| Row | Result |
|---|---|
| `qt_document_tabs_scale_100` | Pass, 7.66 s |
| `qt_document_tabs_scale_125` | Pass, 8.81 s |
| `qt_document_tabs_scale_200` | Pass, 13.66 s |
| `qt_workbench_window_scale_100` | Fail, 25.98 s: `the title yields once and does not return at a narrower width` |
| `qt_workbench_window_scale_125` | Fail, 27.59 s: the same check |
| `qt_workbench_window_scale_200` | Fail, 29.24 s: the same check |

So `42e536f67` clears the tab half on Windows too. The title half has the
signature this card first recorded and is still open. It is not
[[BPR-1255]]'s check: on Windows these rows stop at the title rule before they
reach the review-bar overflow check. The card stays open for the title row.
