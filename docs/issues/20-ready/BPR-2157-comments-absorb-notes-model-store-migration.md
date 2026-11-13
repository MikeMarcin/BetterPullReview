---
schema: "implement-rfc.card.v1"
id: "BPR-2157"
title: "One comment in three states: model, store and the notes migration"
status: "Ready"
rfcs: ["RFC-BPR-0127"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":["AC03","AC11","AC13"],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
---

# BPR-2157: One comment in three states: model, store and the notes migration

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `local-comment-model` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

Core: the Local, Draft and Published states and the two moves (Make draft, Keep local) as revisioned draft commands with typed refusals; one anchor admitting a line, a range, a reply, a whole file and the pull request; placement generalised from `core::place_note`; the roster's Local count, with `drafts_outside_review` never listing Local; no send path admits a Local comment. Host: one store port for unsent comments (`store_note`, `erase_note` and `private_ledger_observation::Notes` removed, RFC-BPR-0023's one authority); the ledger schema step inside `migrate_private_review` from schema 11 (text bytes, anchor facts and order kept, legacy identity kept so Go to note arrives; repeatable after interruption); Local bounded by the note bound of 4,096 a review, outside the 64 active drafts. The Notes tab reads the new store; nothing else looks different.

## Acceptance

- **AC03:** With Local comments present on lines, in threads, on a file and on the pull request, Add single comment, the review's Submit, Add all, Send and Publish response write nothing of them: the fixture provider receives no Local body, and each is still Local afterwards.
- **AC11:** A profile whose ledger holds notes written before and after the path and line side tables opens after migration with one Local comment for each: equal text bytes, equal anchor facts, equal order; a note-era Go to note arrives at its comment; a migration interrupted at any statement and run again gives the same result.
- **AC13:** Label counters, thread counts, the Comments tab's new count and Home are unchanged by creating, moving or deleting Local comments.
