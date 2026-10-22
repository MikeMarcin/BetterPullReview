---
schema: "implement-rfc.card.v1"
id: "BPR-1737"
title: "Confirm on GitHub Enterprise Server 3.20 that a file comment is taken and the three-line rule holds"
status: "Backlog"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-1735"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1737--BPR-1737--b0d2bd329c00.json","kind":"verification","origin":"BPR-1735 (owner decision 2026-10-22): the remaining GHES assumptions of RFC-BPR-0022 amendment r5","related_cards":["BPR-1735","BPR-1716","BPR-1715","BPR-1706"],"needs":"the owner's GitHub Enterprise Server 3.20.8 host; no designated target serves GHES"}
---

# BPR-1737: Confirm on GitHub Enterprise Server 3.20 that a file comment is taken and the three-line rule holds

[[BPR-1735]] rests the off-hunk file comment on two facts measured on
GitHub.com (BPR-Test #116 to #119 and #124) and never observed on the owner's
GitHub Enterprise Server 3.20.8:

1. `POST /repos/{o}/{r}/pulls/{n}/comments` with `subject_type: file`,
   `commit_id` the head and the head's path creates a file comment. GitHub.com
   added it to REST in 2023; GHES 3.20 is newer than that, so it is very likely.
   If GHES refuses it, the refusal is a fully received 4xx: the file child is
   rejected, the draft kept and the reason shown, and nothing is published
   twice.
2. A line comment resolves only within three lines of a change of the merge
   base to the head on its side, as GitHub.com does. If GHES resolves fewer
   lines, a comment the core placed in a hunk is refused (a review is refused
   whole and settled by BPR-1715; a single comment is rejected and kept); if it
   resolves more, a placeable line goes as a file comment, which is honest.

## Acceptance

- AC1: On the owner's GHES 3.20 host, the BPR-1735 build publishes a held
  off-hunk comment of a review as a file comment, and GitHub's answer has
  `subject_type: file`.
- AC2: On the same host a line comment three lines from a change is taken and
  one four lines away is refused with 422, as on GitHub.com.

Only the owner can reach that host. The owner's own course on the new build
(the review refused on 2026-10-21: publish its held comment as a file comment,
then submit the other eight) answers AC1; the diagnostics export of that run
records the file child's status class.
