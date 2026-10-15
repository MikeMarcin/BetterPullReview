---
schema: "implement-rfc.card.v1"
id: "BPR-1530"
title: "Show a Review file-tree folder's file count only while it is collapsed"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"I think the file count on folders in the file tree explorer is a bit too noisy. Lets hide that and only show it when the folder is collapsed.","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0658","BPR-1363","BPR-1529"],"reservation":"docs/issues/_reservations/BPR-1530--BPR-1530--82f5f2096afd.json","requested_skill":"frontend-design"}
---

# BPR-1530: Show a Review file-tree folder's file count only while it is collapsed

The owner asked on 2026-10-15: "I think the file count on folders in the file
tree explorer is a bit too noisy. Lets hide that and only show it when the
folder is collapsed."

The file tree explorer is Review's files drawer tree ([[BPR-0658]],
`files_tree_view`). Every folder row draws its file count at the row's right
edge, open or closed. An open folder already shows its files beneath it, so
the count repeats what is visible; a closed folder hides them, so the count is
the only sign of what it holds.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
files drawer ("file counts on folders"), amended by the owner for this one
choice and recorded in the
[File drawer specification](../../ux/workbench-specification.md#file-drawer).
This card changes only the Review drawer. Overview's folder rows keep their
count slot ([[BPR-1363]] v002): the owner named the file tree explorer, and
Overview's counts are a column the owner asked for separately; if the owner
wants Overview changed too, that is a separate request.

The model and the accessible text are unchanged: a folder's accessible text
still says "name, N files" whether it is open or closed, because assistive
technology reads the row, not the painted digits, and the count is not
redundant there.

## Acceptance

- AC1: A collapsed folder row draws its file count at the right edge as
  before; an expanded folder row draws no count. The count appears and
  disappears as the folder is collapsed and expanded by mouse, by keyboard,
  and by expand-all and collapse-all; a check against always-drawn counts
  fails.
- AC2: The folder's name, its ink ([[BPR-1529]]), row geometry, file rows,
  the folder's accessible text with its file count, selection, focus and
  keyboard behaviour are unchanged.
- AC3 (human): The owner accepts the exact frozen candidate's Review file
  tree, shown with folders expanded and collapsed in offscreen Light and Dark
  renders.

## Implementation

Load frontend-design:frontend-design before design work. Qt presents: the
change is `files_tree_delegate::paint`'s folder detail, read from the view's
expansion state; no core, host, provider or storage change. Shares one frozen
build with [[BPR-1529]] and asks its own question in the same session. Never
drive or capture the owner's desktop; renders are offscreen.
