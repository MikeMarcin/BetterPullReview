---
schema: "implement-rfc.card.v1"
id: "BPR-1251"
title: "Windows: the document tab strip is narrower than one tab, and the title returns at a narrower width"
status: "In Progress"
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

## The title row, measured on Windows (bpr-main-win, 2026-10-01)

At `27a0bf89e`, `qt_workbench_window_scale_100`. A probe, not committed, in
the narrowing loop of `test_strip_identity_stays_at_every_width` printed, at
each width, whether the identity label still carries the title, the label's
width, and the modality's width and form:

| Width | Title | Label | Modality | Text |
|---:|:-:|---:|---|---|
| 1,102 | yes | 231 | 411, full | `microsoft/terminal  #20607 Fix the Scre…` |
| 1,101 | **no** | 152 | 411, full | `microsoft/terminal  #20607` |
| 1,100 | no | 152 | 411, full | `microsoft/terminal  #20607` |
| 1,099 | **yes** | 383 | 255, compact | `microsoft/terminal  #20607 Fix the ScreenBufferTests suite so that e…` |

**Cause.** The title row makes two decisions independently
(`src/adapters/qt/src/workbench_window.cpp`):

- `apply_strip_width` makes the modality compact at a fixed width,
  `available < 1'100`.
- `update_strip_identity` keeps the title while the identity's room is at
  least the `owner/repository  #number` advance plus `identity_title_minimum`
  (80). That width depends on font metrics.

At Windows metrics, the title's give-up width (1,101) lands two pixels above
the fixed compaction width. Compaction then frees 156 pixels, and the title
comes back, which breaks RFC-BPR-0028's "the title yields once". Where the
title's give-up width falls below 1,100, compaction happens while the title is
still shown, and the title yields later for good. Every other step of the
narrowing passed on Windows up to this check.

This is not [[BPR-1255]]'s floor. That is the review bar overflowing at a
doubled font; this is two title-row thresholds racing at the default font.

**Ways to fix it, in the layout:**

1. **Decide the title from the room with the full modality.** The title shows
   only when it would fit with the modality's shortcuts shown. That rule is
   monotone in width, so the title cannot return. It is also the order the code's own
   comment states ("the name's title and owner, then the modality's
   shortcuts"). Compaction's freed room still goes to the other forms of the
   name. Cost: on any platform where the title now survives below 1,100 with
   a compact modality, it yields earlier, at the width where the full-modality
   room runs out. That may include macOS, and was not measured there.
2. **Compact before the title would yield.** Compaction also fires when the
   full-modality room is too small for the title. The title then keeps its
   place longer, and the shortcuts go first. Cost: it reverses the stated
   order near the threshold.

Neither is applied yet. Option 1 keeps the stated order. Both change where the
title yields on macOS and Linux, so the rows there need a rerun.

## Option 1 applied (bpr-main-win, 2026-10-01)

The coordinator chose option 1. In `apply_strip_width`, the identity's room
is measured once with the modality in its whole form, before compaction is
decided, and kept as `IdentityTitleBudget`. In `update_strip_identity`, the
title shows only while both the current room and that room hold it. The room
beside the whole modality depends only on the window's width, so it only
shrinks as the window narrows. The title therefore yields once, as the code's
stated order says, and the room a compact modality frees goes to the name's
other forms. The narrowing loop's existing check, "the title yields once and
does not return at a narrower width", is the pin: on Windows it failed without
this change.

Windows 11 Home 10.0.26200, Visual Studio 18 (MSVC 14.51), Qt 6.11.2,
`build/windows-dev` Debug, at `50870a833` plus this change. The build printed
no warning, and `clang-format --dry-run --Werror` is clean on both files.

```powershell
ctest --preset windows-dev -j1 --output-on-failure -R "^(qt_workbench_window_scale_(100|125|200)|qt_workbench_title_row_scale_(100|125|200)|qt_document_tabs_scale_(100|125|200)|application_windows_title_row|architecture_boundaries)$"
```

- `qt_document_tabs_scale_{100,125,200}` pass (7.51, 8.67 and 12.21 s), and
  so does `architecture_boundaries` (25.28 s).
- All six `qt_workbench_window_scale_*` and `qt_workbench_title_row_scale_*`
  rows now pass the title check and the rest of the narrowing course. Each
  stops later, at the review-bar course's `no review-bar control folds into an
  overflow menu at any width`. That is [[BPR-1255]]'s check, which these rows
  had not reached on Windows before.
- `application_windows_title_row` still fails with its earlier headline,
  `a published review replaces Home with the review surface` ([[BPR-0653]]).

So this card's title half is fixed on Windows. The six rows now share
[[BPR-1255]]'s parked overflow check, which [[BPR-1255]] had expected to pass
on Windows. The card stays open until the Mac and Linux reruns confirm the
title check there and [[BPR-1255]] is decided.

### Linux after `facf69d53` (bpr-main-linux, 2026-10-01)

At `facf69d53`, which includes `facf69d53`, on WSL2 with GCC 15.2, `build/dev` Debug
and offscreen. The dev build has no warnings.

- `ctest --preset dev -j6 -R '^qt_workbench_(window|title_row)_scale_(100|125|200)$'`:
  all six fail, each only at [[BPR-1255]]'s `no review-bar control folds into
  an overflow menu at any width`. None reports `the title yields once and does
  not return at a narrower width`.
- A row stops at its first failed expectation, so I also ran the six with that
  one overflow expectation bypassed in a local build, not committed and then
  reverted. **All six pass**: title row in 3.0, 4.1 and 5.8 s, window in 25.9,
  27.3 and 30.9 s. So on Linux the title-yield rule holds at every scale,
  and only [[BPR-1255]]'s owner decision keeps these rows red.

### macOS after `facf69d53` (bpr-main-mac, 2026-10-01)

Clean worktree at `9751195d0`, Apple clang, `dev` preset. The build exits 0
with no project warning (this also builds [[BPR-1262]]'s portable changes under
Apple clang). `ctest --preset dev -j1 -R '^qt_workbench_(window|title_row)_scale_'`:
**all six pass** (title row 3.55, 2.09, 2.76 s; window 21.96, 22.77, 24.30 s),
so on macOS the title still yields from the whole-modality room and the
overflow fold is reached and met.

What remains for this card's acceptance is the Windows and Linux six, which now
stop only at [[BPR-1255]]'s overflow check. The card finishes when the owner's
[[BPR-1255]] decision lands and those rows are rerun.

## Verified on Windows after BPR-1255 (bpr-main-win, 2026-10-06)

Nothing in the layout was changed for this close. Both of this card's fixes
were already in place: the strip's one-tab minimum (`42e536f67`), and the
title decided from the room beside the whole modality (option 1). The rows
stopped only at [[BPR-1255]]'s overflow check, which the floor that follows
the review bar has since answered.

At `c6ab4fb61`, on `build/windows-dev` (Debug, MSVC 14.51, Qt 6.11.2,
offscreen), the whole tree built with no `warning C` line. With `-j1`, 9 of 9
pass:

- `qt_document_tabs_scale_{100,125,200}`: 7.1, 8.1 and 11.6 s;
- `qt_workbench_title_row_scale_{100,125,200}`: 22.5, 25.3 and 29.2 s;
- `qt_workbench_window_scale_{100,125,200}`: 120.5, 127.2 and 138.3 s.

The window rows include "the title yields once and does not return at a
narrower width" and the review-bar fold. Getting there needed one Windows
test fix on the way, in [[BPR-1255]]'s run (`fe300517d`): the Comment course
held a stale card iterator.

**macOS and Linux**, from their latest records:

- Linux passes the window rows with [[BPR-1255]]'s floor at `1fecfaef5`
  ([[BPR-1290]]'s Linux note), and the document-tab rows since `42e536f67`.
- macOS passed the window and title-row rows at `9751195d0`; the document-tab
  rows have passed there since the main-stabilization milestone.
- Neither has rerun all six at `c6ab4fb61`. That is owed to the coordinator,
  not gating, since no layout code changed for this close.
