---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-J3"
title: "The generated row, file menus, file filters and the bulk label"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-PENDING-J1", "BPR-PENDING-J2", "BPR-1686"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone":"RFC-BPR-0089 M2","frontend_design":"frontend-design:frontend-design loaded before the generated row, the bulk label, the file-menu gestures and the chip's file-filter text were drawn","related_cards":["BPR-PENDING-J1","BPR-PENDING-J2","BPR-PENDING-J4","BPR-PENDING-J5"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8e296033dd8586fa127a1994f3f03e1ca8fe2697","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_commit_filter_public_path$'","detail":"Shipped composition, offscreen, commit-filter-v1 fixture (no .gitattributes): nothing is set aside until a gesture says so. Treat as generated from the Overview's file menu takes src/generator.in out of the Overview tree, the Files drawer, Next file and the counts (97 of 98), the Overview heading says 1 generated set aside, both trees end with the generated row naming it, and no chip shows. Clear all filters (after Hide C3) leaves it behind the row. After a restart the review ledger still sets it aside, marked generated. Review anyway from the generated row returns it and survives a restart; Set aside again, Follow .gitattributes and Show as normal work from the menus. Negative control: without adopting the ledger's marks on reload, the restart step fails. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"8e296033dd8586fa127a1994f3f03e1ca8fe2697","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|application_commit_filter_model)$'","detail":"Hide files these commits changed on C2 leaves exactly src/app.cpp and src/generator.in (2 of 98): C2's 96 regenerated paths go from C3 as well, so Next file walks neither the regeneration nor C3's two hand edits; the chip reads File filter on, 2 of 98 files, 96 hidden by file filters, and the Overview heading says why. Clear all filters removes the file filter. The model test combines a file filter with Show only C3 and refuses the gesture before the trees are read. macOS."}},{"criterion":"AC3","outcome":"pass","revision":"8e296033dd8586fa127a1994f3f03e1ca8fe2697","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_model|qt_commit_filter_view_scale_(100|200))$'","detail":"A commit changing 100 files or more carries the bulk label (model: 100 touched paths), beside its state (Hidden, bulk) and in its accessible text; smaller commits do not, and the label filters nothing. The fixture's 96-file C2 is correctly not labelled. The Commits tab offers Hide files these commits changed once the commits' files are known. macOS."}},{"criterion":"AC4","outcome":"pass","revision":"8e296033dd8586fa127a1994f3f03e1ca8fe2697","evidence":{"kind":"observation","detail":"RFC AC05 for M2's gestures: the composed course holds no review mutation terminal after file-filter and generated-file gestures; marks are local ledger rows only. macOS."}}]
---

# BPR-PENDING-J3: The generated row, file menus, file filters and the bulk label

RFC-BPR-0089 M2 through the shipped composition. The
`frontend-design:frontend-design` skill was loaded before these surfaces were
drawn; they keep to the app's palette roles and existing patterns.

## Change

- **The generated row.** Both file trees (the Overview's and the Files
  drawer's) end with "N generated files hidden" and the set-aside files' line
  totals, in a folder row's ink and weight. Opened, it lists each file and why
  it is generated ("linguist-generated", "Treated as generated"); its context
  menu offers Review anyway, Show as normal and, for a marked file, Follow
  .gitattributes.
- **File menus.** The Overview's file menu (after its comment counts) and the
  Files drawer's (after Open in New Tab) offer the core's gestures for the
  file: Treat as generated for an ordinary file; Set aside again and Show as
  normal for one reviewed anyway; Follow .gitattributes for a marked one.
- **Set aside everywhere.** The review's file set leaves generated files out
  of the trees, Next and Previous file, the counts and preparation ahead,
  under every commit filter and in Whole PR; Clear all filters keeps them
  aside (owner, Q3). A gesture is persisted to the ledger first and adopted
  once durable; marks a sibling window writes are adopted on its reload.
- **Hide files these commits changed** in the Commits tab's menu adds the
  selected commits' paths to a file filter. The chip shows while a file
  filter names paths ("File filter on"), and every count names why files are
  left out: filtered by commits, hidden by file filters, generated set aside.
- **Bulk label.** A commit of 100 files or more is tagged Bulk.

Shared files touched: `github_application.cpp`, `workbench_window`,
`pull_request_overview_page`, `files_tree` and the composed course.

## Acceptance

- **AC1.** RFC AC04 without attributes: Review anyway, Show as normal and Treat
  as generated work and persist; Clear all filters leaves generated files
  behind their row; nothing is hidden before a rule or gesture says so.
- **AC2.** RFC AC04: "Hide files these commits changed" on C2 hides those
  paths in C3 as well.
- **AC3.** The bulk label.
- **AC4.** RFC AC05: no gesture here changes GitHub's state.

## Not in this card

- `linguist-generated` from the head's `.gitattributes`: [[BPR-PENDING-J4]].
- Opening a set-aside file's diff from the row on request: [[BPR-PENDING-J5]].
- A file filter is kept with the window but not restored with it, like the
  commit filter ([[BPR-1688]]).
