---
schema: "implement-rfc.card.v1"
id: "BPR-1582"
title: "Threads rows select on a click, open on a double click, and offer Review, Discuss and Mark read"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner decision 2026-10-17, relayed by the coordinator","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-0656","BPR-1293","BPR-1356","BPR-1567","BPR-1581"],"reservation":"docs/issues/_reservations/BPR-1582--BPR-1582--9ef6743b0873.json"}
---

# BPR-1582: Threads rows select on a click, open on a double click

## Owner decision

Relayed by the coordinator on 2026-10-17, with [[BPR-1581]]'s request: in
the Threads view a single click selects the thread, like the rest of the
interface, with no navigation; a double click opens it in Review; a
Shift+double click opens it in Discuss; the selected or hovered row carries a
**Review** chip next to the existing **Discuss** chip, styled alike, keyboard
reachable and named. Return opens in Review and Shift+Return in Discuss, as
they do. Then: a right-click menu on a row with **Mark as read** (**Mark as
unread** once it is read), from the existing read state, the core deciding,
plus Open in Review and Open in Discuss; Copy link and Open on GitHub only if
they exist elsewhere; a multi-selection if the list has one; the menu key,
Shift+F10.

Today ([[BPR-1293]], [[BPR-1356]]) a press on a row picks it into Review at
once, a press on a post's bead picks that post, the Discuss pill shows on the
hovered and cursor rows, and the row menu holds Open in Review (default) and
Open in Discuss.

## Design

Designed with the `frontend-design` skill. The two pills are one pair at the
end of the row's words: **Review** then **Discuss**, the same quiet pill
(hairline, panel tint, ink on the pointer), so the people never move. Each
names its key in an anchored tooltip ("Open in Review (↩)", "Open in Discuss
(⇧↩)"). A press on a pill opens at once; a press anywhere else on the row,
or on a bead, selects that row and post and moves the cursor there. A double
click opens the selection in Review, Shift with it in Discuss. The pills are
reached by keyboard through their keys on the cursor's row, which the row's
accessible description says; the row menu lists both.

The row menu: Open in Review (default, bold), Open in Discuss, then Mark as
read or Mark as unread. Mark as read is the drawer card's Mark read, through
the same ledger path ([[BPR-0656]]). Mark as unread is new: the core moves the
reader's seen cursor back to the post before the latest post by someone else,
so that post is new again; a thread whose every post is mine offers neither.
No per-thread Copy link or Open on GitHub exists elsewhere in the
application, so the menu does not add them. The list is single-selection, so
the menu acts on its row.

Navigation is [[BPR-1581]]'s shared route: one core decision for Threads,
Review and Discuss.

## Acceptance

- `BPR-1582:core`: `core_review_ledger` covers mark unread (latest other's
  post new again, my posts skipped, nothing to unread), and
  `core_workbench_interaction` the shared route from Threads.
- `BPR-1582:finder`: `qt_conversation_finder_scale_*` cover a click
  selecting without a pick (row and bead), double click to Review, Shift+double
  click to Discuss, both pills' placement on the hovered and cursor rows and
  never on a header, the Review pill's press, Return and Shift+Return, the
  row menu by right click, the menu key and Shift+F10 with Mark as read or
  unread enabled as the state says.
- `BPR-1582:window`: the window course opens through each gesture into the
  right surface and marks a thread read and unread through the ledger.
- `BPR-1582:negative`: a negative control (a single click that picks, a
  double click that lands elsewhere) is reported.
- `BPR-1582:gates`: format, tidy, documentation gate, the full `dev` ctest
  shared with [[BPR-1581]].
- `BPR-1582:owner-look` (human): the owner accepts renders of the Threads
  rows (hover and selected, both pills, the row menu) in Light and Dark, in
  the same short session as [[BPR-1581]], one question.
