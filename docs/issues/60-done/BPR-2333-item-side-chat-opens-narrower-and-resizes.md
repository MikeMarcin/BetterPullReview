---
schema: "implement-rfc.card.v1"
id: "BPR-2333"
title: "The item side chat opens narrower, resizes by its seam and keeps its width per window"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"reservation":"docs/issues/_reservations/BPR-2333--BPR-2333--133e96076872.json","origin":"owner's attended review bbbd62f881b93ad3315947522fe41e0d (frozen from main 7aeba8bdf), Reject on BPR-2322","related_cards":["BPR-2322","BPR-1334","BPR-1588"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2333: The item side chat opens narrower, resizes by its seam and keeps its width per window

## The owner's words

Attended review session `bbbd62f881b93ad3315947522fe41e0d`, frozen from main
`7aeba8bdf`, rejected [[BPR-2322]]'s report side chat:

"By default its taking up too much space and I need to be able to resize it."

In the look's window the column took 380 pixels of a comparison page of
about 630, so the report beside it had about 250 and wrapped its verdict a
few words a line. Nothing resized it.

## What changed

| | Before | After |
|---|---|---|
| Default width | `sizeHint` 380, minimum 300 | 320 beside a page at least 1,067 wide; three tenths of a narrower page; never under 260 |
| Resize | None | The seam at the column's left edge (`itemSideChatResizeHandle`, a horizontal resize cursor held by `hold_resize_cursor`, a tooltip and an accessible name) drags the column between 260 and six tenths of the page; a double-click returns it to the default |
| Remembered | No | The width a drag chose, or Reset, goes to the workbench host (`workbench_application_host::resize_side_chat`), which holds it until the window's record loads, validates it (1 to 16,384) and stores it in that window's row (`window_layout.side_chat_width`, workbench settings schema 38; a version-37 store migrates at the default width). A restart projects it (`workbench_layout_delta::SideChatWidth`) and the column adopts it without reporting it back |
| A stored width wider than the page allows | – | Shown at the bound; the stored width is kept for a wider window |

The window's composition forwards the column's width sink to the host as it
does Line details' height, so the production composition digest is
resealed with a note.

## Acceptance

- AC1. Beside a wide page the column opens at 320 pixels; beside an 800-pixel
  page at 260.
- AC2. Dragging the seam resizes the column, bounded by 260 and six tenths
  of the page; a double-click returns it to the default; each gesture
  reports its width (or Reset) once.
- AC3. The width is kept per window: the host holds a width given before
  the record loads, merges it on an orderly close, projects it on a restart,
  refuses zero or past the bound, and Reset persists the default.
- AC4. The SQLite store keeps each window's width in its own column,
  refuses zero or past the bound, clears one window's on Reset, and
  migrates a version-37 file with every other choice kept.
- AC5. The side chat's public path still passes.

## Decisions

- The frontend-design skill was loaded for the column. The seam reuses the
  column's existing accent line (`side_chat::paint_seam`) as the grip, so
  nothing new is drawn.
- The handle is a child of the column at its left edge, not a `QSplitter`:
  the report, the code and the column keep the comparison page as their
  parent, which [[BPR-2322]] relied on.
- Like Line details' seam, the handle has no keyboard focus of its own.

## Evidence

See the Verification section appended at closeout.
