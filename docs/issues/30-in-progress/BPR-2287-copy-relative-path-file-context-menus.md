---
schema: "implement-rfc.card.v1"
id: "BPR-2287"
title: "A file's context menu copies its path from the repository root: the files drawer, the overview's file list and the document tabs"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2287--BPR-2287--31b031ddd5d4.json","related_cards":["BPR-1132","BPR-1034"]}
---

# BPR-2287: A file's context menu copies its path from the repository root

## Asked

Owner, 2026-11-19: "I want to be able to right click a file in the review
and copy it's relative path from the repo root to the clipboard. Either the
file in the file drawer or overview file list or the open file tab bar."

## Acceptance

- AC1. A file row's menu in the files drawer (`files_tree_view`) offers
  "Copy Relative Path" after Open in New Tab. It writes the file's path from
  the repository root, as the diff names it, to the clipboard and dispatches
  nothing. A folder row has no file menu.
- AC2. A file row's menu in the overview's file list offers the same item,
  after the row's comment counts and before its generated gestures, on a live
  and a cached overview. A renamed file copies its new path. A folder row
  offers no path.
- AC3. A file tab's menu in the document tab strip offers the same item after
  its comparison items; it writes the tab's file path and sends no tab intent.
  A report tab keeps Close alone.
- AC4. No widget performs a host effect; the clipboard write is the Qt
  adapter's own. `coding-format-check` passes on the touched files.

## Design

One helper, `src/adapters/qt/src/copy_relative_path_action.hpp`, adds the
action to an existing menu; each surface reuses the menu it already had
([[BPR-1034]]'s heap menus, [[BPR-1132]]'s Open in New Tab). Each surface
already holds the pull request's path: the drawer row's title, the overview
row's `path_role`, the tab presentation's `Path`. All three are the head
path, so a rename copies its new path with no extra lookup.
