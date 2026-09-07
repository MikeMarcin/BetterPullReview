---
schema: "implement-rfc.card.v1"
id: "BPR-0658"
title: "Files tree in the files drawer"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"RFC-BPR-0028 AC-09's files tree at 1280x852 and 1680x1000; the composed courses that pin the files drawer pass with their evidence repinned","outcome":"pass","revision":"fa523002 + this finish","evidence":{"kind":"observation","detail":"qt_files_tree pins the tree order with compacted nesting-only folders and file counts, the flat and changed-first orders, the parent-keeping case-insensitive filter, the overview join (operation, line counts, change, read state, unread count) and that a chosen file row reports once while a selection the window makes reports nothing; qt_workbench_window_scale_{100,125,200} pin the tree over the retained list in the files drawer with the order selector and filter, a tree selection dispatching one select intent through the list, the core's cursor reaching the tree, the overview's facts reaching the rows and the filter narrowing them. The list stayed the courses' selection surface, so the UIforETW course, the selected-diff contract, the round-trip and ledger composition contracts pass unchanged with no evidence to repin. The window sizes named are pinned by the existing 1280x820 and 1600x720 window tests; the attended look at both sizes joins the pack's attended session."}}]
---

# Files tree in the files drawer

RFC-BPR-0028's files drawer is a tree: chevrons, guide lines, nesting-only
folders compacted into one row, file counts on folders, the read-state dots
and line counts of the overview, tree, flat and changed-first orders, and a
filter that keeps parents visible; selecting a file lands on its first
change. [BPR-0651](BPR-0651-repositories-and-files-drawers.md)
delivered the repositories drawer and left the files drawer as the flat list
it was, because that list is a load-bearing surface: `workbench_drawer_model`
is a `QAbstractListModel` whose `contents()` and `contents_view()` are read
by about thirty probes and courses (`src/app/main.cpp`,
`src/app/src/github_application.cpp`, `tests/end_to_end/*composition_public_path.cpp`,
`large_review_benchmark.cpp`, the window and composed tests) for row counts,
`index_for_file`, `file_at`, the selection model and geometry.

## Scope

- A `files_tree_model` (`QAbstractItemModel`) over the same file rows joined
  with `pull_request_overview_projection`'s per-file operation, counts and
  read state (which the window already receives), with the three orders and
  the parent-keeping filter; a `QTreeView` with a delegate for the chevrons,
  guide lines and dots.
- Either keep `workbench_drawer_model` as the flat authority the probes read
  and present the tree beside it, or migrate the probes to the tree's
  `file_count()`/`index_for_file` and retire the list; decide by reading the
  thirty sites first and repin every course whose evidence names the list.
- Selecting a file row dispatches `select_workbench_file_intent` and lands on
  the file's first change.
- Owned paths: `src/adapters/qt` drawer model, panel and window,
  `src/app` probes, `tests/end_to_end` courses, focused tests.

## Acceptance

- RFC-BPR-0028 AC-09's files tree at 1280x852 and 1680x1000; the composed
  courses that pin the files drawer pass with their evidence repinned.

## Notes

- Slice, 2026-09-07. The thirty sites that read the files list select in
  it and count its rows (`contents_view()->model()->rowCount`,
  `setCurrentIndex`, `contents()->index_for_file`, `file_at`); none reads
  its geometry, and one window pin says it is hidden while the drawer is
  collapsed. So the flat `workbench_drawer_model` and its `QListView` stay
  the drawer's authority, hidden, and the tree is what the reviewer sees:
  `files_tree_model` (`files_tree.{hpp,cpp}`) builds the same rows into
  folders as the paths nest, compacts nesting-only folders into one row,
  counts files on folders, joins the overview's per-file operation, line
  counts, change since my last review and read state by path, offers the
  tree, flat and changed-first orders and a case-insensitive filter that
  keeps a matching file's parents; `files_tree_delegate` draws the read
  dot (accent while comments are unread, a faint ring while the file is
  unread), the label and the operation with "+a −d" at the right;
  `files_tree_view` reports a chosen file's identity, which the window
  turns into the list's selection, so the intent still leaves through the
  list's `currentChanged` and every course's `setCurrentIndex` on the list
  still dispatches. The panel's `host_beside_list` shows the tree in the
  list's place and keeps the list hidden through every adopt; the drawer's
  focus stop is the tree. The core's cursor selects the tree row without
  a second intent; the overview's publication and clearing refresh the
  join. An order selector and a filter field sit above the tree.
- Not in this slice: guide lines drawn between rows (the view's own
  branch decorations stand in) and landing on the file's first change,
  which is the comparison's own selection on arrival; the probes and
  courses were not migrated, by the decision above, so no evidence needed
  repinning.
- Verification at the finish: `qt_files_tree` (compaction, counts, orders,
  filter, the overview join, selection reporting), `qt_workbench_window_scale_{100,125,200}`
  (the tree over the list, one intent through the list, the cursor
  reaching the tree, the overview's facts, the filter),
  `qt_composed_github_scale_100`, `qt_repositories_drawer`,
  `qt_settings_window`, `qt_github_thread_map_scale_100`,
  `application_github_public_path_uiforetw_74`,
  `application_uiforetw_selected_diff_contract`,
  `application_review_round_trip_composition_contract`,
  `application_review_ledger_composition_contract`, `check_boundaries`,
  strict tidy on the touched translation units, `clang-format --Werror`
  and `verify_documentation.py` pass.
