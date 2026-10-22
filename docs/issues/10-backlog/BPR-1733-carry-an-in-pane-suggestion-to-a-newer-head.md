---
schema: "implement-rfc.card.v1"
id: "BPR-1733"
title: "Carry a kept in-pane suggestion to a newer head, or copy its change"
status: "Backlog"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1728"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 M3, 2026-10-21: split from BPR-1728 because it needs host facts the window does not hold","related_cards":["BPR-1723","BPR-1726","BPR-1728"],"milestone":"RFC-BPR-0090 M3"}
---

# BPR-1733: Carry a kept in-pane suggestion to a newer head, or copy its change

RFC-BPR-0090 "A new head while editing" ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#a-new-head-while-editing)).

## Where it stands

- Built: the edit ends when a new publication arrives, and its draft stays
  on its comparison, read-only, as the established rule says (AH3). The
  core's decision is `core::decide_suggestion_carry` (AH1): every original
  line of the range unchanged at the new head, exactly, from the old-to-new
  head correspondence.
- Not built: the window has no old-to-new head correspondence for a kept
  draft's file. The host computes `line_correspondence` values for filtered
  diffs (RFC-BPR-0089), and would have to read the old head's and the new
  head's blob of the draft's file and supply theirs.

## Scope

- The host supplies the old-to-new head correspondence of a kept suggestion
  draft's file when the head moves.
- The read-only draft offers **Carry to the new head** exactly when
  `decide_suggestion_carry` maps its lines; carrying opens the same edit on
  the new lines with the same block, and deletes the old draft once the new
  one is kept.
- Otherwise it says the file changed where it was edited and offers **Copy
  change**, which copies the block's text.

## Acceptance criteria

- AC1: RFC AC08: with a new head arriving mid-edit, the edit ends and the
  draft stays on its comparison; Carry is offered exactly when every
  original line of its range is unchanged at the new head, and Copy change
  otherwise, on fixtures and on BPR-Test.
