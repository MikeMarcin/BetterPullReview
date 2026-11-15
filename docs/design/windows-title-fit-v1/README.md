# The Windows title row with five modes (round 1)

Mockups for the owner to choose from. No product code changed.

The owner, 2026-11-15:

> I do worry a little about the title bar fit on windows, I wonder if we can
> make a better font choice there to alleviate the crunch

> I hope we get real windows renders using the remote station

The review window's title row is the window's title bar on Windows
([BPR-0653](../../issues/40-parked/BPR-0653-windows-caption-in-the-strip.md)).
The accepted [Revise design](../local-draft-workflow-v2/README.md) adds a fifth
mode to its switcher and a branch chip after the name.

[index.html](index.html) is standalone HTML with Dark and Light, and 100%,
125% and 150% scaling. Every strip in it and in `png/` was drawn by Windows on
the Windows station; none is a screen capture and none is an estimate. The
pull request, its branch and its title are invented. The `frontend-design`
skill was loaded for this work.

## What Windows uses today

- **Face.** Segoe UI at 9 points, 12 pixels at 100%. `appearance_profile.cpp`
  takes `QFontDatabase::GeneralFont`, which on Windows is the system message
  font. Nothing in the application picks Segoe UI Variable or any other face;
  the Qt adapter's rule is to inherit the system's.
- **Keys.** `modality_control::relabel` sets each mode as its name, two spaces
  and `QKeySequence::NativeText`. Windows names the keys `Ctrl+1` to `Ctrl+4`,
  and Revise's would be `Ctrl+5`. The key is part of the button's one string,
  in the name's ink.
- **What yields.** `workbench_window::apply_strip_width`: the title, then the
  owner, then the keys, then the name, and last the modes fall to three-letter
  prefixes. On Windows the keys go below 1300 pixels
  ([BPR-1324](../../issues/60-done/BPR-1324-windows-title-row-owner-yield-after-the-fourth-modality.md)).
  The title shows only if it would also fit beside the switcher with its keys,
  so that it does not come and go as the keys do (BPR-1251).

## What was measured

Segoe UI 9 pt at 100% scaling. The 125% and 150% figures are within five
pixels of these and are in `data/`.

| | Pixels |
|---|---|
| Five modes, names and keys (the switcher) | 507 |
| Five modes, names only | 312 |
| Five modes, three-letter prefixes | 205 |
| Four modes today, names and keys | 413 |
| Four modes today, names only | 257 |
| The five `Ctrl+N` hints | 195 |
| Everything else that is fixed: menu, Home, two drawers, Open on GitHub, refresh, two drawer buttons, the rule, account, settings, caption buttons | 471 |
| The branch chip `feature/search-results ↑2` | 161 |
| `bpr-fixtures/engine  #214 Search results` | 216 |
| `bpr-fixtures/engine  #214` and the long title | 493 |

Today's rules with Revise and the branch chip, on a pull request I own:

| Width | Short title, "Search results" | Long title |
|---|---|---|
| 1280 | `engine  #214`, and 213 pixels left empty | the same |
| 1366 | `bpr-fixtures/engine  #214`, no title | the same |
| 1600 | whole | cut to 397 pixels |

The faces, on the five names with their keys:

| Face | Pixels | Against Segoe UI 9 |
|---|---|---|
| Segoe UI 9 pt, today | 395 | 0 |
| Segoe UI Variable Text 9 pt | 392 | −3 |
| Segoe UI Variable Display 9 pt | 397 | +2 |
| Segoe UI Variable Small 9 pt | 406 | +11 |
| Segoe UI 8.5 pt (11 px) | 383 | −12 |
| Segoe UI Semibold 8.5 pt | 381 | −14 |
| Segoe UI 9 pt, tracked −0.3 px | 374 | −21 |
| Tahoma 8.25 pt | 372 | −23 |
| Bahnschrift SemiCondensed 9 pt | 353 | −42 |
| Bahnschrift Condensed 9.5 pt | 318 | −77 |

The answer to the question as asked: no Segoe face is narrower by more than
three pixels at the same size. The hints cost 195.

## The options

Each at 1280, 1366 and 1600 logical pixels, long title, in Revise.
Sheets stack, top to bottom: what ships with four modes, today's rules with
Revise, A, B, C, D.

| Width | Dark, 100% | Light, 100% | Dark, 150% |
|---|---|---|---|
| 1280 | [sheet](png/sheet-1280-dark-100.png) | [sheet](png/sheet-1280-light-100.png) | [sheet](png/sheet-1280-dark-150.png) |
| 1366 | [sheet](png/sheet-1366-dark-100.png) | [sheet](png/sheet-1366-light-100.png) | [sheet](png/sheet-1366-dark-150.png) |
| 1600 | [sheet](png/sheet-1600-dark-100.png) | [sheet](png/sheet-1600-light-100.png) | [sheet](png/sheet-1600-dark-150.png) |

Single strips are `png/OPTION-WIDTH-THEME-SCALE.png`, with OPTION one of
`shipped`, `today`, `a-type`, `b-keys`, `c-icons`, `d-number`.

| | A. Condensed face | B. Keys in tooltips | C. Icons that open | D. Number only |
|---|---|---|---|---|
| What changes | The row is set in Bahnschrift SemiCondensed 9 pt. Keys stay, quieter | Modes show names only on Windows, at every width. Segoe UI stays | Every mode gets a glyph. When the title would be cut, only the current mode keeps its name | The row names the repository and number. The title is in Overview, the tooltip and the taskbar |
| Switcher | 465 | 312 | 412 open, 197 folded | 507 |
| Long title at 1280 | none, `engine  #214` | 272 pixels of it | 390 pixels of it | none |
| Long title at 1366 | 213 pixels of it | 361 pixels of it | 473 pixels of it | none |
| Long title at 1600 | 446 pixels of it | whole | whole | none |
| Short title | whole from 1366 | whole at every width | whole at every width | none |
| Costs | The row no longer matches the window's Segoe UI; heavier lettering | The row stops teaching the keys. They stay in the tooltips, the View menu and Keyboard Shortcuts | Five glyphs to draw and learn (the pencil is Compose review's). Segments change width with the mode | Gives nothing back. The title is hidden even when there is room |

The glyphs in C are placeholders drawn for this round.

## Recommendation

**B, keys in tooltips.** It answers the crunch at every width without a new
visual language, and the row already drops the keys below 1300 today. It also
ends the rule that refuses the title at 1280 while 213 pixels sit empty: with
no keyed form, the title has nothing to wait on.

A is the question as asked, and it is not enough. If Segoe UI Variable Text is
wanted for its look on Windows 11, it can go with B at no cost in width; that
is a separate choice.

## How the renders were made

- `harness/` is a scratch program, not product code. It compiles the lane's
  `workbench_strip.cpp`, `windows_caption_buttons.cpp`,
  `keyboard_shortcuts.cpp`, `keyboard_focus_ring.cpp`,
  `painted_glyph_icon.cpp` and core's `appearance_contrast.cpp` and
  `appearance_model.cpp` unchanged, from commit `c2b7e7da5`. The strip, its
  button style, its glyphs, the four-mode `modality_control` and the caption
  buttons are the application's. The review bar's progress track and Updates
  disclosure share that source file and are stubbed; the title row never
  reaches them.
- The fifth mode, the branch chip and the options are drawn in the harness on
  the strip's style. Its layout repeats `apply_strip_width`,
  `update_strip_identity`, `strip_identity_budget` and
  `strip_identity_minimum`. Its switcher's four segments measure the same as
  the application's in all three forms (403, 247 and 154 pixels; `parity` in
  `data/rows-100.json`).
- Built on the station (Windows 11 25H2, build 26200) with Visual Studio 18
  and the lane's pinned Qt 6.12.0, and run over SSH with Qt's `windows`
  platform plugin, so the text is Windows' own. Every widget has
  `Qt::WA_DontShowOnScreen` and is read with `QWidget::grab()`. Nothing was
  shown on or taken from a desktop.
- Scaling is `QT_SCALE_FACTOR` 1, 1.25 and 1.5 with `QT_FONT_DPI=96`, which
  gives the device pixel ratio and the 12-pixel logical font a window has at
  that Windows scale. A strip 1280 wide is 1280, 1600 and 1920 pixels.
- The palette is `appearance_profile::derive` over the built-in Dark and
  Light profiles, repeated by hand in the harness.

Three things this does not show:

- The SSH session has no DPI awareness and reads the station's 150% message
  font, 18 pixels high, as 13.5 points. The harness takes the family Windows
  reports, Segoe UI, and sets 9 points, which is what 18 pixels at 144 DPI is.
  `harness/ncm.py` prints the values.
- The whole `workbench_window` was not built. The row's rules are repeated in
  the harness, not run from the window.
- Hover, focus and the pressed states are not drawn.

To run it again: `harness/render.sh` and `harness/measure.sh`, beside a build
of `harness/CMakeLists.txt` with `-DBPR_ROOT=` an export of the lane.

## Status

Filed for the owner on 2026-11-15 as attended review `windows-title-fit-v001`:
one choice between A, B, C and D. External state directory
`~/.attended-review-state/windows-title-fit-v001-2026-11-15/` (`state/` is the
session and holds its ID). Lifecycle: awaiting the owner's answer. Nothing is
built until it is given.
