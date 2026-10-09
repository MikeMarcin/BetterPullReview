---
schema: "implement-rfc.card.v1"
id: "BPR-1363"
title: "Opening a folder in the Files table moves every column; the disclosure mark is not leftmost"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"owner, chat 2026-10-09 (Overview Files tree, nested-folders fixture)","reservation":"docs/issues/_reservations/BPR-1363--BPR-1363--b87c15b2f6df.json","related_cards":["BPR-1213","BPR-1089"]}
acceptance: [{"criterion":"BPR-1363:columns-steady","outcome":"pass","revision":"d2fd4de09","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"qt_pull_request_overview_page and qt_pull_request_overview_folder_rows_{125,200} test_folder_disclosure_never_moves_a_column: every column's x and width identical across opening and closing each folder, scrolling, Expand all, Collapse all and refresh; before the fix opening docs/api moved 0+16 16+180 ... to 0+17 17+180 ... at all three scales. Passed on mac and on linux at fc89f79e3."}},{"criterion":"BPR-1363:file-column-whole-tree","outcome":"pass","revision":"d2fd4de09","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"steady_columns measures every cell of the whole model at its depth whether open or not; test_folder_disclosure_never_moves_a_column asserts every folder and file cell fits, and test_column_sizing_measures_visible_files asserts the widest of a thousand names fits before it is shown with a bounded measure; widths are the same across refresh."}},{"criterion":"BPR-1363:dragged-width","outcome":"pass","revision":"d2fd4de09","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"test_folder_disclosure_never_moves_a_column: a real mouse drag widens the File column by 40 and it stays through expand, collapse and refresh."}},{"criterion":"BPR-1363:drawer","outcome":"pass","revision":"d2fd4de09","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"qt_files_tree and qt_files_tree_folder_rows_{125,200} test_folder_disclosure_never_moves_the_column (before the fix 0+318 narrowed to 0+300 when a scroll bar came) and test_folder_disclosure_is_the_leftmost_mark."}},{"criterion":"BPR-1363:disclosure-leftmost","outcome":"pass","revision":"d2fd4de09","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"test_folder_disclosure_is_the_leftmost_mark and test_folder_counts_share_one_column at 100/125/200%: the arrow is the leftmost ink on every folder row, the count sits in one right-aligned slot after it, accessible text keeps path and count, file names start where folder names start."}},{"criterion":"BPR-1363:owner-columns","outcome":"pass","revision":"90527564b","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"Owner Accept, session d827fdb7041e884f47af97defe1d59bd, candidate BPR-1363-columns-v001 on frozen 90527564b, no note."}},{"criterion":"BPR-1363:owner-disclosure","outcome":"pass","revision":"d2fd4de09","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1363-opening-a-folder-in-the-files-table-moves-every-column.md","detail":"Owner select 'Count before the name' (the built, recommended look), no note, 2026-10-09T15:02:28Z: session 649d005d36464a5d6475078836069a17, candidate BPR-1363-count-placement-v002 (frozen d2fd4de09, better_pull_review sha256 ea202c24..., fingerprint e170c453d3bb...), decision 9c04e98b-5bba-4c7e-83d8-101f04f3ec59 at event 4, handed back at 5, released at 6. v001 Reject note led to v002."}}]
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

## Owner look v001 — returned 2026-10-09

Session `d827fdb7041e884f47af97defe1d59bd`, handed back at cursor 9.

- `BPR-1363-columns-v001` (`BPR-1363:owner-columns`): **Accept**, no note.
  Opening or closing a folder moves no column, and a dragged width stays.
  This criterion is met on `90527564b`.
- `BPR-1363-disclosure-v001` (`BPR-1363:owner-disclosure`): **Reject**. The
  note, verbatim: "Yes, but I would prefer if the number had a consistent
  column alignment, either put it as a column out to the right or move it to
  the left of the folder name."

The session was released with `resume-development --all` (cursor 10), its
host (PID 27175) stopped by PID, and the frozen root
`/Users/jaeger/bpr1363-folder-rows-v001.x9hgdb3c` removed with
`scripts/freeze_attended_build.py remove`.

## v002 — the count in one column (bpr-main-mac, 2026-10-09)

**Design.** Loaded `frontend-design:frontend-design` first. The owner asked
for one thing: a count that lines up. The arrow still leads the row. Right
after it, in the row's first cell, every folder's file count sits in one
slot. The slot is as wide as the largest count in the pull request, written
as zeros (`"000"` for 120), in the regular weight with tabular figures
(`tnum`), right-aligned, in the muted ink the folder path uses (highlighted
ink when selected), and a space separates it from the name. Since the slot
lives in the first column, the File header still sits over the names, and
folder and file names both start at the File column's edge. The slot
depends on the contents only (`overview_file_model::largest_folder_file_count`,
recomputed with the tree), so `steady_columns` still measures it once per
contents change, never on expand, collapse or scroll. The folder's
accessible text is unchanged ("Folder src/engine, 2 files"). The count no
longer follows the path, and the File column no longer reserves room for it.

**Alternative rendered, not built.** "Count in its own column": the arrow
alone in the first cell and the count right-aligned at the File column's
right edge. It was rendered from a temporary patch (never committed), so
the owner can compare the two looks.

**Tests.**
- `test_folder_counts_share_one_column` (new; runs in
  `qt_pull_request_overview_page` and the `--folder-rows` rows at 125% and
  200%) presents folders of 1, 7, 12 and 120 files at path depths 1 to 4
  (`a`, `h/i/j`, `b/c/d/e`, `f/g`). On every folder row it grabs the
  viewport and checks four things. The arrow is the leftmost ink. The slot
  (`overview_row_delegate::count_rect`) starts right after the arrow and is
  as wide as three digits. The count's ink lies between the arrow and the
  name. All four slots share one x and width. Across the rows, the counts'
  right ink edges agree within 2 px, and a longer count reaches further
  left, not further right. Opening the 120-file folder moves no column, and
  a file's name starts where its folder's name starts. A mutation check
  that left-aligned the count failed on "the counts are right-aligned".
- `test_folder_disclosure_is_the_leftmost_mark` now asserts that the count
  is drawn between the arrow's ink and the path's ink, in the slot right
  after the arrow, on all twelve nested-folders rows.
- `test_folder_controls_preserve_identity_and_refresh_state`: the folder row's
  first cell holds the arrow and the count's slot; the longest folder path
  fits unelided without a count after it.

**Integration.** `ctest --preset dev -j4` over 29 rows passed:
`qt_files_tree*`, `qt_pull_request_overview*`, `qt_overview_read_marks`,
`qt_composed_github_scale_{100,125,200}`, `qt_workbench_overview`,
`qt_workbench_window_scale_{100,125,200}`,
`application_nested_overview_folders_public_path`,
`application_overview_*`, `application_accepted_file_in_overview`,
`application_offline_inventory`, `application_active_changed_scaling` and
`architecture_boundaries`. clang-format 22.1.8 check passed on the three
touched C++ files. clang-tidy on the two touched translation units reports
only the findings on older lines that v001 listed (`68f00eb15`,
`10cd1cbb2`, `ba3c970b9`). The documentation gate printed
`DOCUMENTATION_OK`. The Files drawer keeps its counts in their own column at
its right edge, as v001 left it; it needs no change.

## Owner look v002 — prepared 2026-10-09

- Landed on main as `d2fd4de09`. Frozen root
  `/Users/jaeger/bpr1363-folder-counts-v002.xqvwchhl`, made by
  `scripts/freeze_attended_build.py freeze --name bpr1363-folder-counts-v002
  --fixture nested-folders-v1 --open-pull-request --commit d2fd4de09`. It
  opens bpr-fixtures/nested-folders#21 on its Overview. Its
  `src/adapters/qt/src/pull_request_overview_page.cpp` is byte-identical to
  `d2fd4de09`'s. `better_pull_review` SHA-256
  `ea202c2423b9b6b830c9c16c50233d71a23729d8ae43fe9bd92c57762fe65880`. The
  launcher's own check reached `ATTENDED_PULL_REQUEST_OPENED`.
- Launch: `python3 /Users/jaeger/bpr1363-folder-counts-v002.xqvwchhl/launch.py`.
- New short session `649d005d36464a5d6475078836069a17`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1363-folder-counts-v002/state`,
  repo root the main checkout, host detached. The offline page is
  `review.html` in the state directory. One choice candidate, ready at
  event cursor 3: `BPR-1363-count-placement-v002`
  (`BPR-1363:owner-disclosure`), options "Count before the name" (built,
  recommended) and "Count in its own column" (rendered only). It attaches
  both looks at 100%, on the nested-folders pull request and on counts of
  1, 12 and 120, and the built look at 200%.
- Next: on hand-back, read `status --after 3`. "Count before the name" closes
  the criterion; then finish the card and remove the frozen root with
  `scripts/freeze_attended_build.py remove`. "Count in its own column" means
  building that look as v003. A Reject's note is the next round's change.

## Linux station run (bpr-main-linux, 2026-10-09)

At `fc89f79e3`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner, all pass:
- the `qt_files_tree` and `qt_pull_request_overview` rows, including the
  125 and 200 folder rows;
- `qt_overview_read_marks`, `qt_workbench_overview` and the window scales;
- `qt_composed_github` and the `qt_composed_markdown` rows;
- `application_accepted_file_in_overview`, `application_active_changed_scaling`,
  `application_nested_overview_folders_public_path`, `application_offline_inventory`,
  `application_overview_double_click`, the overview summary rows and the
  overview feedback and labels public paths.

## Owner look v002 — returned 2026-10-09

Session `649d005d36464a5d6475078836069a17`, handed back at cursor 5.

- `BPR-1363-count-placement-v002` (`BPR-1363:owner-disclosure`): **select**
  "Count before the name", no note. Decision
  `9c04e98b-5bba-4c7e-83d8-101f04f3ec59`, event 4, saved
  2026-10-09T15:02:28Z, on the frozen `d2fd4de09` build
  (`better_pull_review` SHA-256 `ea202c24…`). This is the built,
  recommended look, so the criterion is met on `d2fd4de09` and no v003 is
  needed. With v001's columns Accept, every acceptance criterion is met.

The session was released with `resume-development --all` (cursor 6), its
host (PID 75444) stopped through the session's `stop`, and the frozen root
`/Users/jaeger/bpr1363-folder-counts-v002.xqvwchhl` removed with
`scripts/freeze_attended_build.py remove`.

## Windows station run (bpr-main-win, 2026-10-09)

At `fc89f79e3`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4` (52 rows for the two cards together):

- **Pass:**
  - `qt_files_tree` and `qt_files_tree_folder_rows_{125,200}`;
  - `qt_pull_request_overview_page`,
    `qt_pull_request_overview_folder_rows_{125,200}` and
    `qt_pull_request_overview_chips_scale_{125,200}`;
  - `qt_workbench_overview`, `qt_overview_read_marks`,
    `qt_workbench_window_scale_{100,125,200}`,
    `qt_composed_github_scale_{100,125,200}` and
    `qt_composed_markdown_{1_0,1_25,2_0}`;
  - `application_nested_overview_folders_public_path`,
    `application_accepted_file_in_overview`,
    `application_overview_double_click`, `application_overview_summary_{1,1_25,2}`,
    `application_overview_{feedback,labels}_public_path`,
    `application_setup_destination_public_path`,
    `application_active_changed_scaling` and `application_offline_inventory`;
  - `architecture_boundaries` and `test_network_isolation_manifest`.
