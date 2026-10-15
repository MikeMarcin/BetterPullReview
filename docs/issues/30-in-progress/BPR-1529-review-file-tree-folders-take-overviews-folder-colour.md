---
schema: "implement-rfc.card.v1"
id: "BPR-1529"
title: "Draw Review's file-tree folders in Overview's folder colour"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"For the file tree in review can we draw folders different to files, maybe the same color as they are in overview","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0658","BPR-1213","BPR-1331","BPR-1363"],"reservation":"docs/issues/_reservations/BPR-1529--BPR-1529--1cf250f53166.json","requested_skill":"frontend-design"}
---

# BPR-1529: Draw Review's file-tree folders in Overview's folder colour

The owner asked on 2026-10-15: "For the file tree in review can we draw
folders different to files, maybe the same color as they are in overview".

The file tree in Review is the files drawer's tree ([[BPR-0658]],
`files_tree_view`). Its folder rows are DemiBold but draw their names in the
same ordinary text ink as file names, so the two read alike. Overview's flat
folder rows ([[BPR-1213]], `overview_row_delegate::paint_folder`) draw their
paths DemiBold in the palette's `PlaceholderText`, the muted ink the derived
appearance palette sets for every variant as the midpoint of text and base.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
files drawer, recorded in the
[File drawer specification](../../ux/workbench-specification.md#file-drawer).
The tree's structure, order, compaction, counts, file marks, filter,
expansion, selection, focus, keyboard behaviour and accessible names are
unchanged; only a folder name's ink changes.

## Acceptance

- AC1: In the tree order, an unselected folder row draws its name in the
  palette's `PlaceholderText`, exactly the ink Overview's folder row draws its
  path in under the same appearance profile, and DemiBold as before. A file
  row keeps ordinary `Text` ink. This holds in Light, Dark, High Contrast
  Light and High Contrast Dark; a check against the old `Text` ink fails.
- AC2: Folder counts, file marks, operation and line-count colours, the unread
  dot, the selected row's `HighlightedText`, row geometry, the flat and
  changed-first orders, and the tree's accessible names and keyboard
  behaviour are unchanged.
- AC3 (human): The owner accepts the exact frozen candidate's Review file
  tree, shown before and after in offscreen Light and Dark renders beside an
  Overview render for comparison.

## Implementation

Load frontend-design:frontend-design before design work. Qt presents: the
change is `files_tree_delegate::paint`'s folder label ink, reading the same
palette role Overview reads; no core, host, provider, storage or new
appearance role. Never drive or capture the owner's desktop; renders are
offscreen. Visual judgement goes through a fresh short attended-review session
and a frozen build.
