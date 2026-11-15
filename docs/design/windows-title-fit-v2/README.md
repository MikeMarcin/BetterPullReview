# The modes fold the way the drawer tabs do (round 2)

Mockups for the owner to accept or reject. No product code changed.

[Round 1](../windows-title-fit-v1/README.md) drew four ways to fit the title
row on Windows with a fifth mode. The owner chose **C, icons that open** on
2026-11-15 and said, in the look's note and afterwards:

> I like icons when it gets narrow because we already do that for the drawer
> tabs. What about using ^ for Ctrl? Eg ^1

> Okay nvm the ^ on windows.

> I think all 3 platforms should do the same icon on narrow trick.

So this round is one design for macOS, Windows and Linux: the mode switcher
takes the drawer tabs' faces, and each platform keeps its own key while there
is room for it, `⌘1` to `⌘5` on macOS and `Ctrl+1` to `Ctrl+5` on Windows and
Linux.

[index.html](index.html) is standalone HTML with Dark and Light, a long and a
short title, and Windows at 100%, 125% and 150%. Every strip in it and in
`png/` was drawn on its own platform; none is a screen capture and none is an
estimate. The pull request, its branch and its titles are invented. The
`frontend-design` skill was loaded for this work.

## The design

- **The faces, widest first.** Names and keys; names; icons with the open
  mode named; icons. The first two are what the switcher shows today. The
  last two are the drawer tabs' narrow faces (`fit_tray_tabs`:
  `face::open_named`, `face::glyphs`). As in the drawers since BPR-1755, a
  mode shows no icon while every name is showing.
  Windows [dark](png/sheet-faces-windows-dark-100.png), [light](png/sheet-faces-windows-light-100.png);
  macOS [dark](png/sheet-faces-macos-dark-200.png), [light](png/sheet-faces-macos-light-200.png);
  Linux [dark](png/sheet-faces-linux-dark-100.png), [light](png/sheet-faces-linux-light-100.png).
- **Which face.** The widest beside which the whole name fits: repository,
  number and title. When none does, the open mode keeps its name for as long
  as a title of the row's 80-pixel minimum still fits beside it; the last face
  is the icons alone. So the other modes' names yield before the title is cut,
  and the open mode's name yields before the title is lost. The room only
  shrinks as the window narrows, so nothing returns at a narrower width.
- **What it replaces.** The three-letter prefixes, the row's last yield
  today. With a fifth mode they no longer tell the modes apart: Review and
  Revise are both `Rev`.
- **Tooltips.** A mode showing only its glyph says its name first, as a
  folded tab does. The tooltip already spells out the key (BPR-1324):
  "Review / Review the code in the drawers you last arranged (Ctrl+2). Choose
  Review again to open Files and Comments."

## The icons

Nothing is new art. Each is a glyph the application already draws beside the
drawers, by the same painters, at 16 pixels and the tabs' stroke.

| Mode | Glyph | Where it is today |
|---|---|---|
| Overview | Three lines of text, the first a heading | A post's rendered view in Comments, `conversation_glyph::rendered` |
| Review | Angle brackets around a slash | A post's source view in Comments, `conversation_glyph::source` |
| Discuss | A speech balloon | The Comments tab's glyph when it folds, `conversation_glyph::comment` |
| Threads | A thread of posts with a lens | Find a thread, `conversation_glyph::finder` |
| Revise | A frame with a text caret in its rows | The Edit view, which the Revise design moves into Revise, `strip_glyph::edit_file` |

Two of these are worn elsewhere in the same window: the balloon is also the
Comments tab's, and the caret frame is also the Edit view's in Revise's bar.

## What was measured

The switcher's width in each face, in logical pixels:

| | Names and keys | Names | Icons, open mode named | Icons | Three-letter prefixes, today |
|---|---|---|---|---|---|
| Windows, Segoe UI 9 pt | 507 | 312 | 205 | 162 | 205 |
| macOS, system font 13 pt | 488 | 357 | 214 | 162 | 232 |
| Linux, Ubuntu Sans 11 pt | 615 | 374 | 216 | 162 | 236 |

What the name shows, on a pull request I own, in Revise, with the branch
chip. "Today" is today's rules with the fifth mode added.

Long title:

| Width | Windows today | Windows folded | macOS today | macOS folded | Linux today | Linux folded |
|---|---|---|---|---|---|---|
| 1600 | cut to 397 | whole | cut to 487 | whole | cut to 422 | whole |
| 1366 | no title | cut to 466 | cut to 250 | cut to 524 | no title | cut to 581 |
| 1280 | `engine #214` | cut to 377 | no title | cut to 436 | `engine #214` | cut to 501 |
| 1100 | `engine #214` | cut to 244 | `engine #214` | cut to 254 | `engine #214` | cut to 316 |
| 960 | nothing | `engine #214` | nothing | no title | nothing | no title |

Short title, "Search results":

| Width | Windows today | Windows folded | macOS today | macOS folded | Linux today | Linux folded |
|---|---|---|---|---|---|---|
| 1600 | whole | whole, keys showing | whole | whole, keys showing | whole | whole, keys showing |
| 1366 | no title | whole | whole | whole, keys showing | no title | whole |
| 1280 | `engine #214` | whole | no title | whole | `engine #214` | whole |
| 1100 | `engine #214` | whole | `engine #214` | whole | `engine #214` | whole |
| 960 | nothing | `engine #214` | nothing | no title | nothing | no title |

"No title" is `bpr-fixtures/engine #214`. Windows at 125% and 150% is within
five pixels of 100% and takes the same face at every width; `data/` has all
of it. Sheets of the five widths, widest first:

| | Long title | Short title |
|---|---|---|
| Windows 100% | [dark](png/sheet-fold-windows-long-dark-100.png), [light](png/sheet-fold-windows-long-light-100.png), [today](png/sheet-today-windows-long-dark-100.png) | [dark](png/sheet-fold-windows-short-dark-100.png), [light](png/sheet-fold-windows-short-light-100.png), [today](png/sheet-today-windows-short-dark-100.png) |
| Windows 150% | [dark](png/sheet-fold-windows-long-dark-150.png), [light](png/sheet-fold-windows-long-light-150.png) | [dark](png/sheet-fold-windows-short-dark-150.png), [light](png/sheet-fold-windows-short-light-150.png) |
| macOS 2x | [dark](png/sheet-fold-macos-long-dark-200.png), [light](png/sheet-fold-macos-long-light-200.png), [today](png/sheet-today-macos-long-dark-200.png) | [dark](png/sheet-fold-macos-short-dark-200.png), [light](png/sheet-fold-macos-short-light-200.png), [today](png/sheet-today-macos-short-dark-200.png) |
| Linux 100% | [dark](png/sheet-fold-linux-long-dark-100.png), [light](png/sheet-fold-linux-long-light-100.png), [today](png/sheet-today-linux-long-dark-100.png) | [dark](png/sheet-fold-linux-short-dark-100.png), [light](png/sheet-fold-linux-short-light-100.png), [today](png/sheet-today-linux-short-dark-100.png) |

Single strips are `png/PLATFORM-OPTION-TITLE-WIDTH-THEME-SCALE.png`, with
OPTION `fold` or `today`.

## Linux

Yes, the same. Linux writes its keys as Windows does, so the keyed switcher is
the widest of the three there: 615 pixels in Ubuntu Sans against 507 on
Windows. Under today's rules it loses the title at 1366 and the owner at 1280,
and Linux has no rule of its own for dropping the keys early as Windows has.
Linux keeps the platform's title bar, so the fold is the only thing the row
needs there.

Linux has no one system font: the row takes whatever the desktop sets. These
renders use Ubuntu Sans at 11 points, Ubuntu's desktop default, which is
installed on the station.

## Notes for the build

- A face is three properties of the segments the switcher already has: text,
  icon and tool-button style. `strip_style` already draws an icon and a name
  with the tabs' four-pixel gap. Nothing in the switcher is painted by the
  harness.
- The face rule is the one in `fit_tray_tabs` and Overview's drawer. The
  switcher would be its third copy.
- The open mode's glyph needs the highlighted-text ink, since it sits on the
  accent.
- Two rules can go: the Windows-only threshold that drops the keys below 1300
  (BPR-1324), and the rule that shows the title only if it would also fit
  beside the keyed switcher (BPR-1251). The fold decides both from the room.

## How the renders were made

`harness/` is a scratch program, not product code. It compiles the lane's
`workbench_strip.cpp`, `conversation_glyphs.cpp`,
`windows_caption_buttons.cpp`, `keyboard_shortcuts.cpp`,
`keyboard_focus_ring.cpp`, `painted_glyph_icon.cpp` and core's
`appearance_contrast.cpp` and `appearance_model.cpp` unchanged, from commit
`c2b7e7da5`, and builds the row from them with the fifth mode and the branch
chip. Its layout repeats `apply_strip_width`, `update_strip_identity`,
`strip_identity_budget` and `strip_identity_minimum` for "today", and the rule
above for the fold. Every widget has `Qt::WA_DontShowOnScreen` and is read
with `QWidget::grab()`. Nothing was shown on or taken from a desktop.

| Platform | Where | Text drawn by | Scales |
|---|---|---|---|
| Windows 11 25H2 | The Windows station, over SSH; Visual Studio 18, Qt 6.12.0 | Qt's `windows` plugin | 100%, 125%, 150% |
| macOS 26 | This Mac; Qt 6.12.0 | Qt's `cocoa` plugin, the way `qt_button_faces_cocoa` draws | 2x |
| Ubuntu 26.04 under WSL | The Linux station, over SSH; GCC, Qt 6.12.0 | Qt's `offscreen` plugin: FreeType and fontconfig, no display | 100%, 150% |

What this does not show:

- The whole `workbench_window` was not built. The row's rules are repeated in
  the harness, not run from the window.
- Over SSH the Windows session misreads the 150% message font as 13.5 points
  and Linux's offscreen plugin reports Sans Serif 9. The harness sets Segoe UI
  9 pt and Ubuntu Sans 11 pt. macOS reports its own font and is not set.
- macOS's three window buttons are the system's. They are drawn in the inset
  by the harness so it does not read as spare room.
- The branch chip is the Revise mockup's, not the application's.
- Hover, focus and pressed states are not drawn, and no tooltip is rendered.

To run it again: `harness/render-windows.sh`, `harness/render-macos.sh` and
`harness/render-linux.sh`, each beside a build of `harness/CMakeLists.txt`
with `-DBPR_ROOT=` an export of the lane.

## Status

Filed for the owner on 2026-11-15 as attended review `windows-title-fit-v002`:
one Accept or Reject question on this design. External state directory
`~/.attended-review-state/windows-title-fit-v002-2026-11-15/` (`state/` is the
session and holds its ID). Lifecycle: awaiting the owner's answer. Nothing is
built until it is given.
