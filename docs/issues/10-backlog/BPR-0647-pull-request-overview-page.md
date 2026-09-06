---
schema: "implement-rfc.card.v1"
id: "BPR-0647"
title: "Pull request overview page"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: ["BPR-0646"]
priority: "P1"
---

# Pull request overview page

RFC-BPR-0028 mockup state 1. The Overview modality's content: title, author
and branches, description, the files table grouped by folder (operation,
added and removed counts, changed since my last review, my read state, unread
and total comment badges, my note), participants with sign-off state, the
since-my-last-review summary, my review progress, and Continue review (Start
review on first visit) entering Read at the first unseen change. Data comes
from the accepted snapshot, the epoch host and the private ledger projections
the host already publishes; any missing projection is added in the host, not
computed in Qt.

## Acceptance

- RFC-BPR-0028 AC-04 with the mockup's columns and groups.
- Continue review lands on the first file marked changed since my last review,
  at its first hunk; Start review lands on the first file.
- Focused Qt tests for the table model and the summary; a composed course
  through the fixture that opens a pull request on Overview.
