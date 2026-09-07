---
schema: "implement-rfc.card.v1"
id: "BPR-0656"
title: "Post read state through conversation receipts"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Post read state through conversation receipts

RFC-BPR-0028's Read and Discuss surfaces draw unread posts with a blue dot
and full-ink names, step read posts back, offer Mark read, mark a new
thread's line blue in the gutter and walk the new threads from the strip's
"next new comment" control. None of that has an authority today:
`core::conversation_receipt` (the cursor of the last comment I saw in a
thread) and `core::unread(receipt, observation)` exist, and the SQLite
ledger has a `conversation_receipt` table since schema version 2, but no
port method, host or projection carries it, so [BPR-0648](../60-done/BPR-0648-aligned-comment-cards-in-read.md)
draws every post in the same ink and offers no Mark read.

## Scope

- `review_ledger_store_port`: load and commit conversation receipts for the
  review's scope; the SQLite store implements them over the existing table.
- `review_ledger_application_host`: observe the conversations of the accepted
  snapshot against their receipts, decide per post whether it is newer than
  my cursor, publish a per-thread projection (unread count, first unread
  post) with the conversation projection, and accept a Mark read command
  that advances the cursor to the latest post.
- Qt: the cards' unread dot and dimming, Mark read on a card, the blue gutter
  mark for a thread whose root is unread, the strip's "next new comment"
  group with its count, and the same states in Discuss (BPR-0649).
- Owned paths: `src/application` ledger port and host, `src/adapters/sqlite`
  store, `src/app` composition, `src/adapters/qt` cards, strip and diff
  marks, their focused tests and the ledger contract.

## Acceptance

- A thread with posts after my cursor reads as unread in the cards and the
  gutter; Mark read advances the cursor durably and the states clear; the
  strip's control steps through unread threads in file order.
- `core_review_ledger`, `application_review_ledger_host`,
  `application_review_ledger_composition_contract` and the Qt selectors for
  the cards, strip and diff marks pass.
