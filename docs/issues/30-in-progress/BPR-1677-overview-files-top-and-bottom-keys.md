---
schema: "implement-rfc.card.v1"
id: "BPR-1677"
title: "Overview files: Command-Up and Command-Down go to the top and bottom"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-20","owner_note":"in the overview page on the file listing i want cmd+up and down to go to the top and bottom of the files list","related_rfcs":["RFC-BPR-0037"],"related_cards":["BPR-1213","BPR-1269","BPR-1328","BPR-1567"]}
acceptance: []
---

# BPR-1677: Overview files: Command-Up and Command-Down go to the top and bottom

## Owner note

The owner, 2026-10-20, verbatim:

> in the overview page on the file listing i want cmd+up and down to go to the
> top and bottom of the files list

## Before

Overview's files are a `QTreeView` of flat folder rows and their files
([[BPR-1213]], RFC-BPR-0037 AC-10). Up and Down step one row, Left and Right
fold a folder, Enter reads a file and Tab steps onto a row's comment chips
([[BPR-1089]]). Command-Up and Command-Down did what the tree does with any
Up and Down: one row. Nothing took the list to its ends from the keyboard on
a Mac keyboard, which has no Home or End key.

## Change

The page's files key handler reads the platform's document start and end:
`QKeySequence::MoveToStartOfDocument` and `MoveToEndOfDocument` (Ctrl+Home
and Ctrl+End on Windows and Linux), and Command-Up and Command-Down on macOS
under every platform plugin, the offscreen one too. They are the code panes'
keys for the top and bottom of the comparison ([[BPR-1328]]), and one
classifier, `document_edge_for` in `keyboard_shortcuts`, now decides them for
both; `comparison_scroll_edge_for` maps its answer.

- **Top.** The first shown row, the first folder row, becomes current and
  selected and the list scrolls to its top.
- **Bottom.** The last shown row becomes current and selected: the last file
  of the last open folder, or that folder's row when it is closed. The list
  scrolls to its bottom.
- The current column is kept, as the tree's own Home and End keep it. A
  comment chip focused on the old row is left, as any row change leaves it.
  The keyboard stays on the list; the tree tells accessibility of the new
  current row as for any row change. Nothing opens until Enter. With no rows
  the keys do nothing.
- Each of the files' accessible descriptions names both keys in the words a
  screen reader speaks ("Command-Up Arrow and Command-Down Arrow go to the
  top and bottom row" on macOS): the page's own, the current catalogue's,
  which replaces it once a pull request is presented, and the cached
  catalogue's.
- Help > Keyboard Shortcuts lists "Top or bottom of the files", In Overview's
  files, from the catalogue's new `files_ends` entry (a key the list reads
  itself).

### Why no core transition

The list's current row is Qt-local presentation state: the page's tree owns
it, no core or application state records it, and moving it dispatches
nothing (a file opens only on activation). Up, Down, Left and Right on the
same list are the tree's own for the same reason. The core decides review
navigation (Next file, the change commands); this is not one of those, so the
decision stays at the Qt boundary with the other list keys and needs no core
transition or core test.

### Top row, not first file

The owner asked for the top and bottom of the list. The top row is always a
folder row, so Command-Up lands on the heading, as Up would on reaching it;
Command-Down lands on the last file under normal expansion. Enter then reads
the file. If the owner would rather Command-Up land on the first file, that
is a one-line change in `move_files_to`.

## Acceptance

- **AC1.** In Overview's files, the platform's document end (Command-Down on
  macOS, Ctrl+End elsewhere) makes the last shown row current and selected,
  keeps the column, scrolls the list to its bottom with the row in view,
  keeps the keyboard on the list, reports the row to accessibility as
  selected and focused, and opens nothing; with the last folder closed it
  goes to that folder's row.
- **AC2.** The document start (Command-Up, Ctrl+Home) makes the top row
  current and selected and scrolls to the top, likewise; pressed at the top
  it stays; Up still steps one row; with no rows both keys do nothing.
- **AC3.** The keys come from one catalogue entry (`files_ends`, read,
  In Overview's files) listed by Help > Keyboard Shortcuts and named in the
  files' accessible description; the code panes' comparison ends keep their
  keys through the shared classifier.

## Verification

- `qt_pull_request_overview_page`:
  `test_document_keys_go_to_the_top_and_bottom_of_the_files` covers AC1-AC3
  on a list of 44 files that scrolls. Failing-before control: with the new
  branch in `handle_files_key` disabled the test fails at "the end makes the
  last file current, in the same column" (the tree moved one row).
- `qt_keyboard_shortcuts_scale_{100,125,200}`, `qt_change_end_keys` and
  `qt_workbench_window_scale_{100,125,200}` (the comparison-edge key test)
  pass against the catalogue and the shared classifier.

macOS only. Windows and Linux station runs of
`ctest --preset dev -R '^qt_pull_request_overview_page$'` are owed, not
gating; the Ctrl+Home and Ctrl+End branch is the standard-key path the code
panes already use there.
