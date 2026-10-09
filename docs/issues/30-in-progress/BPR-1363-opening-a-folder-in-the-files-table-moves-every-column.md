---
schema: "implement-rfc.card.v1"
id: "BPR-1363"
title: "Opening a folder in the Files table moves every column"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"owner, chat 2026-10-09 (Overview Files tree, nested-folders fixture)","reservation":"docs/issues/_reservations/BPR-1363--BPR-1363--b87c15b2f6df.json","related_cards":["BPR-1213","BPR-1089"]}
acceptance: []
---

# BPR-1363: Opening a folder in the Files table moves every column

## Finding

Owner, 2026-10-09: "I dont like that opening this disclosure shift the
column widths". On the Overview with the nested-folders fixture and every
folder closed, opening `docs/api` to show `reference.md` moved the File,
Added, Removed, Changed, Read, Comments and Note columns right by a few
pixels. Every column of the Overview Files table is sized to the contents of
the rows on screen (`ResizeToContents` with precision 0), so a row that
appears or disappears, or a scroll, can change a width.

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
- The owner accepts the result on a frozen build that opens on the Overview
  with the nested-folders fixture.
