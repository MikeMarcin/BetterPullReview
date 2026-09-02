---
id: BPR-0532
title: Retain epoch history so old-revision continuations can anchor
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0521]
---

# Retain epoch history so old-revision continuations can anchor

## Why

[[BPR-0521]] composes a side-chat turn against the kept session's old
revision: its supplement, the helper's pin, the frozen snapshot's binding,
and the disposable workspace are all the old revision's. What it cannot
supply is the old alignment. `core::epoch_state` holds one accepted revision
and one accepted comparison, and the workbench holds one alignment, so the
comparison digest, line counts, segments, groups, and moves of the revision
being continued against are retained nowhere once the head moves. Rather
than send the current comparison's identities beneath an older binding, the
continued turn freezes no per-path identities, and every finding it returns
arrives unanchored.

On 2026-09-02 the product owner accepted that as the shipped behaviour. This
card holds the alternative for whenever anchored continuations are wanted;
it is not part of RFC-BPR-0012's closeout and [[BPR-0507]] does not depend
on it.

## Required final state

- Either retained epoch history (the accepted comparison of a revision a live
  chat session still binds survives the head moving, bounded and evicted
  with the session) or a re-alignment request against the kept binding's
  epoch pair, chosen with a recorded reason.
- A turn continued against an old revision freezes that revision's own
  per-path identities and returns anchored findings validated against them;
  the silent re-anchoring [[BPR-0521]] refuses stays refused.
- Storage, memory, and eviction bounds are explicit and tested; core, host,
  and composition boundaries are unchanged in kind.
- `application_agent_chat_continued_contract` gains the anchored-continuation
  course and the empty-freeze course stays as the documented fallback when
  history is gone.

## Non-goals

- Any change to how a fresh session at the new head anchors — unchanged.
- Retaining history for sessions that are no longer live.
