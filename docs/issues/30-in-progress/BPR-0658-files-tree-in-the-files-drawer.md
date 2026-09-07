---
schema: "implement-rfc.card.v1"
id: "BPR-0658"
title: "Files tree in the files drawer"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Files tree in the files drawer

RFC-BPR-0028's files drawer is a tree: chevrons, guide lines, nesting-only
folders compacted into one row, file counts on folders, the read-state dots
and line counts of the overview, tree, flat and changed-first orders, and a
filter that keeps parents visible; selecting a file lands on its first
change. [BPR-0651](../60-done/BPR-0651-repositories-and-files-drawers.md)
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
