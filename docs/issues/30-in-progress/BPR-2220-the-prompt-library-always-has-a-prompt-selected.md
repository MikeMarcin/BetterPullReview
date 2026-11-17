---
schema: "implement-rfc.card.v1"
id: "BPR-2220"
title: "The prompt library always has a prompt selected"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2179"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2220--BPR-2220--ed478f0bfc51.json","origin":"owner, 2026-11-16, Settings › Prompts"}
---

# The prompt library always has a prompt selected

## Origin

The owner, 2026-11-16: "Also the bottom of prompt library looks like a broken
menu when no prompt is selected... either always have one selected or hide
that bottom area until one is."

The bottom area is Settings › Prompts' editor ([[BPR-2179]]): beside the
library in a wide window, under it in a narrow one. With nothing selected it
showed every field disabled and empty.

Coordinator's decision: always have a prompt selected; hide the editor only
when the list is truly empty.

## Scope and acceptance

- **AC-a.** When the library opens, its first prompt is selected and the
  editor shows it.
- **AC-b.** When the selected prompt is deleted, its neighbour in the list is
  selected (the next row, else the one before).
- **AC-c.** A filter or search that still shows the selected prompt keeps it;
  one that hides it selects the first prompt shown.
- **AC-d.** A filter or search that shows nothing hides the editor and the
  list says so; clearing it shows the editor again with the selection it had.
- **AC-e.** Presentation only: the selection policy lives in the Qt page; the
  core's prompt editor is unchanged.
- **AC-f.** The owner, 2026-11-16, on the same page: "we can probably move the
  restore built-ins and new prompt to the same row as the filter for the
  prompt library." New prompt and Restore built-ins sit on the filter's row,
  at its right; the filter takes the room left. The row fits the page's
  narrowest width at 100, 125 and 200 percent without clipping: as the page
  narrows the title, then the filter choices, go on their own lines.

The owner's look on a frozen build (Fixture agent only, a fresh short session,
one Accept/Reject for each of AC-a to AC-d and AC-f) is filed, not waited on.

## Intended verification

`qt_prompts_page` asserts AC-a to AC-d and AC-f (run at 100, 125 and 200
percent); native-look renders light and dark, before and after.
