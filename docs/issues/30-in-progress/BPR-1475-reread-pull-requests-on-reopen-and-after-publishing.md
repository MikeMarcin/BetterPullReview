---
schema: "implement-rfc.card.v1"
id: "BPR-1475"
title: "Reread a pull request on every reopen and after publishing to it"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1472"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1475--BPR-1475--f4fade45d6e3.json","origin":"owner decision 2026-10-13"}
---

# Reread a pull request on every reopen and after publishing to it

BPR-1472 found that a pull request reopened from Home's Recently viewed list
could show a read taken before this window published a review to it. It
fixed that narrowly: the shown read was reused unless this window had
published over it. The owner then decided two questions BPR-1472 raised
(2026-10-13).

1. **Reread on every reopen.** Opening a pull request from Home's Recently
   viewed, or from a pasted link, always reads it fresh from GitHub, even when
   the window already shows that pull request. This replaces BPR-0987's "no
   second read" rule for that case. A paged read per reopen is accepted.
2. **Reread after publishing.** After a successful publish from the window (a
   review, a comment or a reply), BPR rereads the pull request automatically,
   so the window immediately shows what was posted.

## Acceptance

- AC1: Opening the pull request the window already shows, from its Recently
  viewed row or from a pasted link to it, reads it fresh from GitHub (one new
  snapshot read) and lands where an ordinary open lands; nothing reuses the
  shown read.
- AC2: A successful review submission from the window rereads the pull
  request by itself, so the submitted review shows without a manual refresh.
  A comment or reply published from the window does the same. A read already
  in flight is not doubled.
- AC3: The reread after publishing keeps the user's place: the window stays
  in the modality it was in, the selected file and its diff stay selected,
  and an open note is not closed.
- AC4: The tests that enforced BPR-0987's "no second read" for a reopen are
  updated to the owner's decision and pass:
  `application_profile_window_review_restoration_public_path` and
  `application_setup_destination_public_path`.
