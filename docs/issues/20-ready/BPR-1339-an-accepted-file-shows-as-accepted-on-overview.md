---
schema: "implement-rfc.card.v1"
id: "BPR-1339"
title: "An accepted file shows as accepted on Overview, and counts as done"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1339--BPR-1339--66d5f024f602.json","related_cards":["BPR-1331","BPR-0647","BPR-0656","BPR-0972","BPR-1211"],"acceptance":["BPR-1339:reproduced","BPR-1339:rule","BPR-1339:overview-mark","BPR-1339:counts","BPR-1339:owner-look"]}
---

# BPR-1339: An accepted file shows as accepted on Overview, and counts as done

## Owner notes

Owner, chat, 2026-10-08, verbatim: "I accepted a file in the review but it
still shows as unread in the overview".

The rule, the owner's answer the same day, verbatim: "Rule gap, marking it read
updates it to read. Accepted is stronger than read or unread".

## What exists

- The receipt (`core::file_receipt`, RFC-BPR-0007) holds three independent
  facts: the read progression (unread, seen, read), accepted, and excluded.
  Accepting a file does not mark it read, and with the default "mark read by
  hand" behaviour opening a file does not either.
- [[BPR-1331]]'s `core::progress_mark` already sums a receipt up as one mark:
  excluded, then accepted, then the read progression. Only the review bar's
  private progress button uses it.
- Overview ([[BPR-0647]], RFC-BPR-0028's "my read state") projects only the read
  fact: `overview_file_row::Read`, the read column's dot, "Files read N of M",
  the heading's "N not read", and Continue review's "first file I have not
  read". Acceptance never reaches the row. `overview_receipt_index::apply`
  ([[BPR-0972]]) also drops a receipt change whose read state did not move, so
  an Accept published no row update at all.
- The Files drawer ([[BPR-0656]]) draws its faint unread ring from the same
  read fact, so an accepted unread file kept that ring too.

The refresh path itself works: marking a file read in Review moves its
Overview row at once. The fault is the rule gap, not a stale projection.

## Scope

- **Rule.** Accepted is stronger than read, seen and unread. A file's row shows
  the core's `progress_mark`, so an accepted file shows accepted whatever its
  read state. Acceptance still does not set the read fact: revoking it returns
  the row to the file's read state.
- **Counts.** A file is done when it is read or accepted
  (`core::progress_done`). "Files read N of M", the heading's "N not read" and
  Continue review's next file all count done files. Exclusion's counting is
  unchanged (by its read state).
- **Overview mark.** The read column draws the mark in the BPR-1331 family:
  hollow unread, half seen, filled read as before; accepted a filled disc with
  a tick cut out; excluded a ring struck through. Its accessible text and
  tooltip say "accepted" or "excluded".
- **Files drawer.** The faint unread ring shows only while the mark is unread.
- **Live.** An Accept, a Revoke acceptance, an Exclude and an Include move the
  row, the counts and the next file through the same incremental update a read
  change uses.

## Acceptance

- `BPR-1339:reproduced`: a composed course on the nested-folders fixture
  accepts one file through the private progress control's menu and another
  through its key and mnemonic, then shows Overview; against the code before
  the change it fails with the accepted file still "unread".
- `BPR-1339:rule`: the owner's rule is recorded verbatim here and in
  RFC-BPR-0007; `core_review_ledger` pins `progress_done`.
- `BPR-1339:overview-mark`: application and Qt tests pin the row's mark, the
  incremental update on accept and revoke, and the read column's text; each
  fails against the code before the change.
- `BPR-1339:counts`: the summary, the heading and Continue review count an
  accepted file as done, in the projection and in the incremental update.
- `BPR-1339:owner-look`: the owner accepts the Overview's accepted mark
  (`overview-accepted-mark-v001`) in one short attended session on a frozen
  build.
