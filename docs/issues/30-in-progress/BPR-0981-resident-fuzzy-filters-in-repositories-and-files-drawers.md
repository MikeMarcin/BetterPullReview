---
schema: "implement-rfc.card.v1"
id: "BPR-0981"
title: "Resident fuzzy filters with retained selection in the repositories and files drawers"
status: "In Progress"
rfcs: ["RFC-BPR-0025"]
depends_on: ["BPR-0596"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0025-planning-reconciliation","criteria":["AC-02","AC-09"]}
---

# Resident fuzzy filters with retained selection in the repositories and files drawers

## Scope

The one product slice RFC-BPR-0025 still owns after the owner-locked
RFC-BPR-0028 direction superseded its layout and left browser-style tabs
unscheduled ([reconciliation](../../rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md)).
It completes AC-02 and the filter half of AC-09 on the shipped drawers:

- One Qt-owned resident matcher shared by both trees. A needle matches when
  its characters, case-folded, appear in order in the candidate text; a
  contiguous substring is not required. The matcher decides nothing and
  performs no effect: the filter is presentation over the projections the
  trees already hold, as [[BPR-0658]] placed it, so it stays in the Qt
  adapter rather than the core or the host.
- The files drawer's existing `filesFilter` moves from literal substring to
  the matcher over the repository-relative path (which carries the file
  name); a matching file keeps its folders visible in every order.
- The repositories drawer gains its own `repositoriesFilter` above the tree,
  matching repository, pull request title and number; a matching pull
  request keeps its repository row and a matching repository keeps its
  rows, so parent context is never lost.
- Each drawer exposes an honest empty state when nothing matches, with a
  Clear filter action that restores the tree; neither filtering nor
  clearing closes or changes the open pull request or the selected file.
- Selection is restored by semantic identity: the files tree keeps the
  selected `workbench_file_id` across filter and rebuild (already so), and
  the repositories tree keeps the current pull request ref across filter
  and queue re-publication without dispatching a second open intent.
- Both filters and the files order selector join the window's declared
  focus order with unique accessible names and descriptions.

Owned surfaces: `src/adapters/qt` files tree, repositories drawer, workbench
window composition and focus order; `tests/adapters_qt` files tree,
repositories drawer, composed GitHub and workbench window courses;
`docs/ux/workbench-specification.md`. No core, host, provider, storage or
live GitHub change.

## Acceptance

- AC-02 (whole): independent resident fuzzy filters find pull requests by
  repository, title and number and files by name or path, keep matching
  parent context, expose an honest empty result with a Clear filter action,
  send no provider request and leave the open pull request and selected
  file in place.
- AC-09 (filters): both filters and the files order selector are declared,
  uniquely named, walkable focus stops in the composed window at 100, 125
  and 200 percent.
- Selection restoration by semantic identity in both trees produces no
  spurious open or select intent across filter, order and re-publication.

## Planned direct checks

- `qt_files_tree`: subsequence matching, case folding, ancestor retention in
  tree/flat/changed-first, empty state and clear, selected file retained.
- `qt_repositories_drawer`: repository/title/number matching, parent
  retention, empty state and clear, current pull request retained across
  filter and re-apply with exactly one open dispatch.
- `qt_composed_github_scale_100`: the public gesture path through the
  composed window with a published queue and pull request: type in both
  filters, read the trees, clear from the empty state, and assert the intent
  vector stays empty and the open pull request and selected file are unchanged.
- `qt_workbench_window_scale_100`, `_125`, `_200`: the declared focus order
  includes both filters and the order selector with unique accessible names.
- Touched-file format check and the documentation gate.

## Evidence

Pending. At planning head `9097723a` the files filter is a literal
case-insensitive substring over the path with no empty state beyond the line
edit's clear button, and the repositories tree has no text filter; neither
filter is in the declared focus order. Nothing here is claimed passed.

## Checkpoint — 2026-09-22, owner stop condition

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0025-claude`, milestone
`close-rfc-0025` (bound `build/rfc0025-claude-milestone-checkpoint-close.json`,
definition `d1cc095c…`, obligations BPR-0981 and BPR-0597). The owner's stop
condition arrived after the product, test and specification edits were
written and **before the first build**. They are committed as `fe41b77b`
so nothing is lost; that commit is **unbuilt, untested and unformatted**, and
nothing in it is claimed to pass.

What `fe41b77b` contains: `src/adapters/qt/include/bpr/qt/resident_filter.hpp`
and `src/resident_filter.cpp` (space-separated terms, ordered case-folded
subsequence match; `filter_empty_notice` with a named Clear filter button),
registered in `src/adapters/qt/CMakeLists.txt`; `files_tree.cpp` uses the
matcher; `repositories_drawer.hpp/.cpp` gain `set_filter`, a `rebuild` over
retained items and saved repositories (repository or host match keeps every
row beneath, title or `#number` match keeps the repository row; the rail
count stays the queue's), and the view keeps `Current` and restores it under
an `Applying` guard without a second open; `workbench_window.hpp/.cpp` add
the `repositoriesFilter` surface, both `filter_empty_notice` widgets,
`refresh_filter_notices()`, accessors and the declared focus stops; tests in
`files_tree_tests.cpp`, `repositories_drawer_tests.cpp`,
`composed_github_tests.cpp` (`test_drawer_filters_are_resident_and_keep_the_open_views`)
and `workbench_window_tests.cpp` (focus-order names); the File drawer
section of `docs/ux/workbench-specification.md`.

Next action for the resuming owner, in order: build
`test_better_pull_review_qt_files_tree_tests`,
`test_better_pull_review_qt_repositories_drawer_tests`,
`test_better_pull_review_qt_github_tests` and
`test_better_pull_review_qt_workbench_window_tests` with `cmake --build
--preset dev`; run `ctest --preset dev -R
'^(qt_files_tree|qt_repositories_drawer|qt_composed_github_scale_100|qt_workbench_window_scale_(100|125|200))$'`;
fix what fails (likely spots: the composed test's `choose_modality(review)`
visibility assumptions use `isHidden()` deliberately; `QStringView::split`
with `u' '`; the focus-order walk now includes the two Clear filter buttons
while hidden); run `coding-format-check` on the touched C++; then the
documentation gate, board validate, and `finish BPR-0981` with the exact
selectors and elapsed times before BPR-0597 starts. No attended verdict is
required by this slice; none was prepared.

No task-owned process is running. No push, merge to `main`, live provider
mutation or credential was used.
