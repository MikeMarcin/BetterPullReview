---
schema: "implement-rfc.card.v1"
id: "BPR-0981"
title: "Resident fuzzy filters with retained selection in the repositories and files drawers"
status: "Ready"
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
