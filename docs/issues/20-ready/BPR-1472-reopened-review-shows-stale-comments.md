---
schema: "implement-rfc.card.v1"
id: "BPR-1472"
title: "Reopening a pull request from Recently viewed shows comments published since"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1472--BPR-1472--a0d64e912270.json","origin":"owner report 2026-10-13"}
---

# Reopening a pull request from Recently viewed shows comments published since

The owner published a review from BetterPullReview one evening: Approve, with
two comments. The comments appeared on github.com straight away, so the
publication worked. The next morning the owner opened the same pull request
from Home's Recently viewed list, and the two comments were missing.
Refreshing Home first did not help. The comments appeared only after a
manual refresh of the open pull request.

## Acceptance

- AC1: After this window publishes to a pull request (a submitted review, or
  a single comment or reply), reopening that pull request from Recently
  viewed reads it again and shows the published comments without a manual
  refresh. A pasted link to that pull request does the same.
- AC2: A regression test drives the composed application: it submits a review
  with two comments, goes Home, and opens the pull request from Recently
  viewed. It requires one new snapshot read. The test fails on the code
  before the fix.
- AC3: If nothing was published since the shown read, reopening it from
  Recently viewed still reads nothing (BPR-0987 unchanged). The existing
  restoration and link-location courses still pass.
