---
schema: "implement-rfc.card.v1"
id: "BPR-1363"
title: "Opening a folder in the Files table moves every column; the disclosure mark is not leftmost"
status: "Parked"
rfcs: []
depends_on: []
parked_on: "human-review"
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"owner, chat 2026-10-09 (Overview Files tree, nested-folders fixture)","reservation":"docs/issues/_reservations/BPR-1363--BPR-1363--b87c15b2f6df.json","related_cards":["BPR-1213","BPR-1089"]}
acceptance: []
---

# BPR-1363: Opening a folder in the Files table moves every column; the disclosure mark is not leftmost

## Finding

Owner, 2026-10-09: "I dont like that opening this disclosure shift the
column widths". On the Overview with the nested-folders fixture and every
folder closed, opening `docs/api` to show `reference.md` moved the File,
Added, Removed, Changed, Read, Comments and Note columns right by a few
pixels. Every column of the Overview Files table is sized to the contents of
the rows on screen (`ResizeToContents` with precision 0), so a row that
appears or disappears, or a scroll, can change a width.

Owner, same day (through the coordinator): "The disclosure arrow not being
the leftmost thing on the overview page for the folder rows is really
bothering me." A folder row showed its file count ("1", "2", "3") in the
first column, left of the ▸/▾ mark.

## Acceptance

- Opening or closing any folder, or all of them, never changes any column's
  x position or width in the Overview Files table. A test records every
  column's x and width, opens and closes folders, and asserts they are
  identical. It fails before the fix and passes after, at 100%, 125% and 200%
  scale.
- The File column is wide enough for every file and folder in the whole
  tree, whether its folder is open or not, or follows another stable rule.
  The widths stay the same across a refresh of the same pull request.
- A width the user drags stays until the user changes it.
- The Files drawer tree is checked for the same defect, and fixed if it has
  it.
- On every Overview folder row the disclosure mark is the leftmost thing
  painted; a test asserts the mark's x is the smallest of any painted element
  on the row. The folder's file count moves elsewhere and keeps its
  accessible text. File names still start where folder names start. The
  Files drawer is checked too.
- The owner accepts each change, as its own Accept/Reject question, on a
  frozen build that opens on the Overview with the nested-folders fixture.

## Built and verified (bpr-main-mac, 2026-10-09)

**Design.** Loaded `frontend-design:frontend-design` first. The rule the
owner asked for is stable widths, so the visual change is kept to the two
things that were reported: nothing moves, and the arrow leads the row.

**Columns.** `steady_columns` (`src/adapters/qt/include/bpr/qt/steady_columns.hpp`)
owns a tree's column widths. Each column is as wide as the widest cell of the
whole model, measured for every row at its depth whether or not its folder is
open, and never narrower than its header label. It measures again only when
the model's contents, the font or the style change, never on an expand, a
collapse or a scroll. A refresh with the same contents gives the same widths.
The cost stays bounded on a large review: every cell is estimated by its text
advance, and the delegate measures only the widest eight per column and row
kind. The thousand-file test sees fewer than 100 measured paths, as the old
visible-rows sizing did (`cdbb208df`). One column fills what the others leave:
Note on the Overview, the only column in the drawer. The space a vertical
scroll bar takes is set aside whether or not the bar shows, wherever the
style's scroll bars take space (not transient). A bar that comes when opened
folders outgrow the view therefore narrows nothing. With macOS overlay scroll
bars nothing is set aside. Where bars take space, the row ends a bar's width
short of the edge while no bar shows. Columns other than the fill and the fixed
Comments width are Interactive: a width the user drags stays through opening,
closing and refresh. A double-click on its edge gives back the measured
width. On the Overview this replaces `ResizeToContents` with precision 0.
In the drawer it replaces the stretched last section.

**Folder rows.** The Overview folder row's first cell now holds only the
native disclosure mark (`overview_row_delegate::paint_disclosure`), at the
row's left edge, sized to `PM_TreeViewIndentation`. The folder's file count
moved from that cell to after its path, in the regular weight and the same
muted ink as the path: "src/engine  2" (`file_count_role`). The folder's
accessible text is unchanged ("Folder src/engine, 2 files"). Folder paths and
file names both start at the File column's left edge, so a file's name sits
under its folder's name. The Files drawer was already right: Qt draws its
mark in the row's indentation, before the name, and a pixel test now holds
it there.

**Tests (fail before, pass after, at 100%, 125% and 200%).**
- `test_folder_disclosure_never_moves_a_column`
  (`tests/adapters_qt/pull_request_overview_page_tests.cpp`) presents the
  nested-folders pull request file for file. It records every column's
  viewport x and width with every folder closed. It then opens and closes
  `docs/api`, opens each folder in turn until the table scrolls, scrolls to
  the bottom, closes each, uses Expand all and Collapse all, and refreshes.
  After every step it asserts the record is identical. It also asserts that
  every folder and file cell of the whole tree fits its column at the
  delegate's measure, and that a real mouse drag on the File column's edge
  widens it by 40 and stays through expand, collapse and refresh. Before the
  fix, at all three scales:
  `0+16 16+180 196+45 … 470+294` became `0+17 17+180 197+45 … 471+293` on
  opening `docs/api`. That is the owner's shift: `reference.md`'s "M" widened
  the first column by 1 px.
- `test_folder_disclosure_never_moves_the_column`
  (`tests/adapters_qt/files_tree_tests.cpp`) does the same for the drawer.
  Before the fix the column narrowed `0+318` to `0+300` when a scroll bar
  came (offscreen Fusion).
- `test_folder_disclosure_is_the_leftmost_mark`, in both files, grabs the
  viewport. On every folder row it finds the first device pixel that differs
  from the row's ground and asserts it lies in the disclosure mark's rect,
  left of the path's ink. On the Overview it also asserts the first cell
  shows no text, the count's ink follows the path, the accessible text still
  names the path and count, and a file's name starts where its folder's path
  starts. It was not run against the old code. Its first-cell assertion is
  false there, because that cell displayed the count.
- `test_column_sizing_measures_visible_files` now asserts the whole-tree
  rule: the widest name, at the end of a thousand files, fits before it is
  ever shown, and scrolling to it moves no column. It also asserts the
  bounded measure.
- New ctest rows `qt_pull_request_overview_folder_rows_{125,200}` and
  `qt_files_tree_folder_rows_{125,200}`; the 100% rows run inside
  `qt_pull_request_overview_page` and `qt_files_tree`.

**Integration.** `ctest --preset dev -j1` over 30 rows passed:
`qt_files_tree*`, `qt_pull_request_overview*`, `qt_overview_read_marks`,
`qt_composed_github_scale_{100,125,200}`, `qt_workbench_overview`,
`qt_workbench_window_scale_{100,125,200}`, `qt_composed_markdown_*`,
`application_setup_destination_public_path`,
`application_nested_overview_folders_public_path`,
`application_overview_{feedback,labels}_public_path`,
`application_overview_summary_*`, `application_accepted_file_in_overview`,
`application_overview_double_click`, `application_offline_inventory` and
`application_active_changed_scaling`. `architecture_boundaries` passed.
clang-format 22.1.8 check passed on the seven touched C++ files. clang-tidy
on the touched translation units reports only findings on lines older than
this card (`68f00eb15`, `10cd1cbb2`, `ba3c970b9`); none are in the new code.

## Owner look — prepared 2026-10-09

- Landed on main as `90527564b`. Frozen root
  `/Users/jaeger/bpr1363-folder-rows-v001.x9hgdb3c`, made by
  `scripts/freeze_attended_build.py freeze --open-pull-request --fixture
  nested-folders-v1 --commit 90527564b`. It opens
  bpr-fixtures/nested-folders#21 on its Overview. `better_pull_review`
  SHA-256 `f1278a954a0cdbe1c63d70ccd0dee5fb51d2fa095ad000228300e92a931cd141`.
  The launcher's own check reached `ATTENDED_PULL_REQUEST_OPENED`.
- Launch: `python3 /Users/jaeger/bpr1363-folder-rows-v001.x9hgdb3c/launch.py`.
- New short session `d827fdb7041e884f47af97defe1d59bd`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1363-folder-rows-v001/state`,
  repo root the main checkout, host detached. The offline page is
  `review.html` in the state directory. Two Accept/Reject candidates, ready
  at event cursor 6:
  `BPR-1363-columns-v001` (`BPR-1363:owner-columns`) and
  `BPR-1363-disclosure-v001` (`BPR-1363:owner-disclosure`). Both attach the
  offscreen render at 100% and 200%.
- Next: on hand-back, read `status --after 6`. Each Accept closes its
  criterion; a Reject's note is the next round's change. When both are
  accepted, finish the card and remove the frozen root with
  `scripts/freeze_attended_build.py remove`.
