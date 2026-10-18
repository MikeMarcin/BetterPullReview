---
schema: "implement-rfc.card.v1"
id: "BPR-1601"
title: "Pull request comments get read state"
status: "In Progress"
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

- AC1: Stable singleton comment/review identities reuse the core read/unread and
  own-post rules, preserve seen-before across manual unread and restart, and
  coexist with existing thread receipts without a schema migration.
- AC2: The ordinary composition shows the new markers in Discuss, the new
  count in Threads' Not on a file group and the Status drawer, opens Discuss
  from Go to them, clears all three after read, and restores them after unread
  or a body edit. Next new comment visits them after file threads in Discuss
  order, widening file scope; both Discuss and Threads menus persist reads.
- AC3: Focused ledger, storage, routing, Qt and public-path courses pass, along
  with core-only architecture checks, both composition contracts, format22 and
  the documentation gate on the current-main landing tree.

## Boundaries

Lean data-model work after [[BPR-1589]] lands; no visual design beyond the
existing new-comment markers. The core decides read state, the host persists
it, Qt presents.

## Implementation

PR comments and review summaries each form a singleton conversation, using the
existing prefixed projection identity for the durable conversation and post
keys. No table, schema version or legacy receipt interpretation changes. The
core still decides unread posts and the inverse action; the host commits a
receipt before publishing an incremental update. A receipt with a zero seen
cursor remains seen before. Queue counts keep that rule while surface markers
and the Status count include all unread posts.

The host's cache is bound to the accepted snapshot and distinguishes native
thread indices from the appended singleton observations. The public test caught
an initial missing bound in the existing native-thread lookup; that bound now
prevents singleton keys from indexing the native thread vector. Navigation keeps
its file-thread cache and appends the pathless roots in retained Discuss order.
The finder uses its existing group, row, marker and menu presentation.

`frontend-design:frontend-design` was loaded. The offscreen composed renders
showed Status's two-comment line, the final Not on a file group with two new
posts, and Discuss's matching markers; the viewer's own comment has none. The
owner's live desktop was not used. The handoff requires no owner look for this
extension of existing markers.

## Verification in progress

Core, application-host, storage and architecture tests pass. The updated Qt
cards/finder courses and Overview course pass; finder runs cover 100%, 125%
and 200%. The final public-path course passes in 4.47 s, including restart,
edit, scope widening and a read through the Threads menu. Core-only ledger,
routing and architecture checks pass (19.17 s). Composition checks and the
final landing record are pending. No full suite runs for this card; handoff
item 9 supplies the single integrated full suite.
