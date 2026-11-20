---
schema: "implement-rfc.card.v1"
id: "BPR-2334"
title: "A right-click opens a row's menu without choosing or opening the row"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"reservation":"docs/issues/_reservations/BPR-2334--BPR-2334--96867b905429.json","origin":"owner's attended review bbbd62f881b93ad3315947522fe41e0d (frozen from main 7aeba8bdf), defect noted during the review","related_cards":["BPR-1034","BPR-1132","BPR-1712","BPR-2287"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2334: A right-click opens a row's menu without choosing or opening the row

## The owner's words

Noted during attended review session `bbbd62f881b93ad3315947522fe41e0d`
(frozen from main `7aeba8bdf`):

"right clicking a file in the files drawer loads that file in the current
pane. I don't think just a right click by itself should do that... other
apps just open the context menu for the thing you mouse over instead of
selecting it."

## What was wrong

A `QAbstractItemView` makes the pressed row current for any mouse button.
The files drawer opens the current file, so a right-click loaded the file
before its menu appeared. Source search jumps to the current result the
same way, and the Overview's file list moved its current row.

## What changed

`open_menu_without_choosing` (`src/adapters/qt/src/context_click.hpp`) puts
a filter on an item view's viewport that consumes the secondary button's
presses, releases, double clicks and drags. The context menu event still
arrives, and each of these views builds its menu from the row under the
pointer:

| View | Menu | Change |
|---|---|---|
| Files drawer (`files_tree_view`) | Open in New Tab, Copy Relative Path, the window's extensions | Filter added |
| Overview file list | The file's menu (`overviewFileMenu`) | Filter added |
| Source search results | Open, Open in New Tab, Copy, Copy Path | Filter added. Copy and Copy Path act on the selection when the right-clicked row is in it, else on that row alone (`menu_rows`), as Finder does |
| Generated files list | Its row menu | Filter added |

Checked and left alone:

- Threads (`conversation_cards_view`): its press handler already ignores
  every button but the left, and its menu is built for the card under the
  pointer.
- Document tabs: `QTabBar` changes the current tab on a left press only,
  and the tab menu names the tab under the pointer.
- Home lists, the Agents settings list, the tour outline and the commit
  filter: their menus act on the selection, which a right-click sets on
  purpose, and selecting a row there opens nothing.

## Acceptance

- AC1. A right-click on another file in the files drawer opens that file's
  menu; the current file stays current and selected and nothing is opened.
  Its Copy Relative Path and Open in New Tab are the right-clicked file's.
  A right-click on the current file does not open it again; a left click
  still opens a file.
- AC2. The same for the Overview's file list: the menu opens, the current
  row stays and no file is opened; its Copy Relative Path is the
  right-clicked file's.
- AC3. Source search: a right-click outside the selection leaves the
  selection, the current result and the open file alone, and the menu's
  Copy writes the right-clicked result; inside the selection the menu acts
  on the selection.
- AC4. The files drawer's check fails with the filter removed.

## Decisions

- One filter shared by the views rather than each view overriding its press
  handler, because two of the views are plain `QTreeView` and
  `QTreeWidget` instances owned by a page.

## Verification

macOS (arm64), dev preset in a task tree of its own built with
`-gline-tables-only` debug info to stay inside the shared disk (behaviour is
unchanged; only debug tables are smaller). Candidate: BPR-2332 `7c93a2fc7`,
BPR-2334 `bc23441e6`, BPR-2333 `1dd688040` on main `02d9a2f84`. Every changed
C++ file passes `format_check.py` (clang-format 22.1.8) and `tidy.py
--warnings-as-errors` (the changed `.cpp` files); `architecture_boundaries`
and `python3 scripts/verify_documentation.py --root .` (`DOCUMENTATION_OK`)
pass.

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2 | `qt_files_tree` (`test_right_click_opens_the_rows_menu_and_chooses_nothing`, through the window system's mouse entry: files drawer and Overview file list; also a tab bar's right-click leaves the active tab and asks for that tab's menu) | pass |
| AC3 | `qt_source_search_panel_scale_1`, `_2` (the right-click course now expects the selection, the current result and the open file left alone, and the menu's Copy to write the right-clicked result) | pass |
| AC4 | `qt_files_tree` rebuilt with the files drawer's `open_menu_without_choosing` commented out | fails at "a right-click on another file neither chooses nor opens it"; passes with it |
| Regression | `qt_pull_request_overview_page`, `qt_composed_github_scale_100`, `qt_workbench_window_scale_100` | pass |

Not run: Windows and Linux (the context menu arrives on release there
rather than on press; the filter consumes both, and the course drives both),
a full `ctest`. The generated files list has the filter but no dedicated
check.
