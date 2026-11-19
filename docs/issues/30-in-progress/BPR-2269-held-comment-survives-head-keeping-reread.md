---
schema: "implement-rfc.card.v1"
id: "BPR-2269"
title: "An Approve whose line comment was written before a reread that kept the head stops at the pending-comment hold"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2269--BPR-2269--724289100633.json","found_by":"BPR-2265's implementer, 2026-11-18, building its reread fixture course","related_cards":["BPR-2265","BPR-2263","BPR-1345"]}
---

# BPR-2269: An Approve whose line comment was written before a reread that kept the head stops at the pending-comment hold

## Observed

In BPR-2265's fixture course, an Approve carrying a line comment written
before two rereads of the same pull request (same head and account) stopped
at "Not submitted: a pending comment changed or could not be held for this
review." The log shows `review_submit_wait submit_subject=pending_comments`
and then the refusal, before any request. It may be part of the owner's real
Submit failures on 2026-11-18.

## Acceptance

- AC1. After rereads that keep the head, an Approve carrying a line comment
  written before them publishes with the comment, from one Submit.
- AC2. BPR-2265's reread course carries the line comment again.
- AC3. Focused tests, format and tidy on the changed files, and
  `python3 scripts/verify_documentation.py --root .` pass.
