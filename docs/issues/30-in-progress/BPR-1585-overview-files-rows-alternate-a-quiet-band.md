---
schema: "implement-rfc.card.v1"
id: "BPR-1585"
title: "Overview's file rows alternate a quiet band within each folder"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-17","owner_note":"Would the Overview files view benefit from a subtle alternating background for rows?","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0647","BPR-1089","BPR-1213","BPR-1363","BPR-1529"],"reservation":"docs/issues/_reservations/BPR-1585--BPR-1585--843eb7b989aa.json","requested_skill":"frontend-design"}
---

# BPR-1585: Overview's file rows alternate a quiet band within each folder

The owner asked on 2026-10-17: "Would the Overview files view benefit from a
subtle alternating background for rows?" The Files table is wide (File,
Added, Removed, Changed, Read, Comments, Note) and the gap between a file's
name and its far columns is large, so the eye loses its row. The table
turns alternating colours off today (`setAlternatingRowColors(false)`,
[[BPR-0647]]), and Qt's own alternation counts every visible row of the
tree, so it cannot restart at a folder.

Governing design: the Overview page of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md), with
[[BPR-1213]]'s flat folder rows, [[BPR-1363]]'s steady columns, [[BPR-1089]]'s
chips and Not on a file row and [[BPR-1529]]'s folder ink unchanged.

## Acceptance

- AC1: File rows alternate between the table's ground and a quiet band
  derived from the theme, counted over the rows on screen within each
  folder: the first file under a folder header is on the ground, the second
  on the band, and so on. Folder header rows and the Not on a file row are
  never banded, so every group starts the same way. Collapsing or expanding
  a folder, a sort and an incremental refresh keep the rule, and a hidden row
  takes no turn. A render check samples the band on visible rows across
  collapse and sort, with a negative control that fails on Qt's
  whole-tree alternation.
- AC2: In Light, Dark, High Contrast Light and High Contrast Dark the band
  is visible against the ground and quiet (a luminance ratio between 1.03
  and 1.25), file text, line counts and chip digits stay at 4.5:1 or more on
  it, and the operation badges, read marks and comment marks at 3:1 or more.
  Selection, keyboard focus and the chip ring draw over the band exactly as
  over the ground. Any ink that is below these floors on the ground today is
  named on this card with its value on both.
- AC3: The Not on a file row draws only its name and its Comments chips: no
  operation badge, Changed mark or read mark, which it drew from empty roles
  as an Added "A", a "?" and an unread ring.
- AC4: Focused Qt tests, one full `ctest --preset dev` on the merged tree,
  format, tidy and the documentation gate pass.
- AC5 (human): The owner accepts the frozen candidate, Light and Dark
  before and after: "The files table's quiet alternating band helps follow a
  row across, and folders still read as groups."

## Implementation

Load frontend-design:frontend-design first. Qt presents: the band is the
row delegate's (`overview_row_delegate`), as the cell's background brush and
as the ground the cell's marks mix with; no QSS, no new appearance role, no
core, host, provider or storage change. Only the Files table is touched;
the Overview's toggle, loading states, disclosure colours, avatars and side
panel belong to other cards. Never drive or capture the owner's desktop;
renders are offscreen and the owner checks the frozen build themselves.
