---
schema: "implement-rfc.card.v1"
id: "BPR-1601"
title: "Pull request comments get read state"
status: "Done"
rfcs: []
depends_on: ["BPR-1589"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decision relayed by the coordinator during BPR-1589, 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1601--BPR-1601--d8c573df027e.json", "queue": "Data-model work, not taste: a candidate for the Codex queue"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4c2d600f3","evidence":{"kind":"observation","detail":"Core and host singleton courses pass for read, unread, edits, own posts and seen-before across restart. SQLite stores read and zero-cursor singleton receipts beside unchanged existing thread receipts and reopens them. Existing migration courses pass; no schema change."}},{"criterion":"AC2","outcome":"pass","revision":"4c2d600f3","evidence":{"kind":"observation","detail":"The real composed synthetic course passes through Go to them, Discuss and Threads menus, next-new after file threads, file-scope widening, all three markers/counts, manual unread, restart and an edited stable summary. Final public course passes; offscreen renders inspected. Overview reset and finder accessibility counts pass."}},{"criterion":"AC3","outcome":"pass","revision":"4c2d600f3","evidence":{"kind":"observation","detail":"Fourteen focused native rows pass across the affected targets, including both composition contracts (664.58 s), with round-trip rerun after final app relink (5.10 s). Three core-only rows pass in 19.17 s. Format22 passes 21 C++ files; documentation gate, 67 documentation regression tests and diff check pass. Main 887feb0ed rechecked and merged; composition pins resealed, mutation digest unchanged."}}]
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

## Verification — 2026-10-17

Implementation `39d056d37`, followed by `4c2d600f3` for singular accessibility
wording and clearing the count with the PR. Main `887feb0ed` is an ancestor;
fetch/recheck and merge reported up to date. All 21 changed C++ files pass
format22; the documentation gate, its 67 regression tests and diff check pass.

Fourteen native focused rows pass across the affected checks: core ledger and
workbench routing, ledger host, SQLite ledger storage, architecture boundaries,
Qt cards, finder at 100/125/200%, Overview page, composed Overview, the new
conversation read course and both composition contracts. The public course
uses only the synthetic conversations fixture and actual actions/menus. It
covers both singleton kinds, the viewer's own post, file-to-Discuss navigation
and scope widening, both read menus, all three counts/markers, restart and a
same-identity body edit. The final count-reset/page and public-path checks pass
in 33.66 s. Core-only ledger, routing and architecture checks pass in 19.17 s.

The composition pair passes in 664.58 s, including its separate production
Release build. The full application and manifest pins were resealed with a
dated scope review; the production mutation branch remains unchanged. The
round-trip contract passes again after the final native app relink (5.10 s).
The initial focused tidy batch (11 translation units) and the new public test
pass; handoff item 9 supplies the required full tidy gate on final main.

The initial UI failures identified old file-only row-count expectations and
a test fixture without threads. The first complete public course exposed the
singleton/native-thread cache-index crash described above; it passes with the
bound in place. No native Windows or Linux run is claimed. No full suite ran
per card; item 9 supplies the single integrated run after this landing.
