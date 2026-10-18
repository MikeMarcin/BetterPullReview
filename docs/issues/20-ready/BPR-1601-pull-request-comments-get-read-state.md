---
schema: "implement-rfc.card.v1"
id: "BPR-1601"
title: "Pull request comments get read state"
status: "Ready"
rfcs: []
depends_on: ["BPR-1589"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decision relayed by the coordinator during BPR-1589, 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1601--BPR-1601--d8c573df027e.json", "queue": "Data-model work, not taste: a candidate for the Codex queue"}
---
# BPR-1601: Pull request comments get read state

The owner, 2026-10-17, while [[BPR-1589]] was in progress: comments that are
not on a file (pull-request conversation comments and review summaries) should
be read and unread as review threads are. Today the review ledger keeps post
read state only for review threads ([[BPR-0648]], [[BPR-0656]]), so Discuss
marks nothing new outside threads and the Overview's Status drawer cannot count
the conversation's new comments.

## Scope

- Core: the review ledger's read state covers pull-request comments and review
  summaries, keyed by their stable comment identity, with the same seen-before
  rule threads use.
- Persistence: the ledger stores it, backward compatible with existing ledgers.
- Discuss draws the new markers on those comments; Threads' "Not on a file" row
  carries their new count.
- Mark as read and Mark as unread work on them ([[BPR-1582]]); ⌥N, next new
  comment, visits them in Discuss's order after the threads.
- The host publishes the count, and the window calls the Overview page's
  `set_new_conversation_comments`, so [[BPR-1589]]'s hidden line, "N new
  comments in the conversation" with Go to them (Discuss), appears.

## Acceptance

- Core and storage courses for read, unread, seen-before and restart.
- Public-path courses: a new conversation comment shows in Discuss, in Threads'
  "Not on a file" row and on the Status drawer's line; Go to them opens Discuss;
  reading it clears all three; ⌥N reaches it.

## Boundaries

Lean data-model work after [[BPR-1589]] lands; no visual design beyond the
existing new-comment markers. The core decides read state, the host persists
it, Qt presents.
