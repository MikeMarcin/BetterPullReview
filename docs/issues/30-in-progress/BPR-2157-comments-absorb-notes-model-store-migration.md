---
schema: "implement-rfc.card.v1"
id: "BPR-2157"
title: "One comment in three states: model, store and the notes migration"
status: "In Progress"
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

## Progress (2026-11-13)

Lane commits `e270bbfa4` (the schema 11 fixture), `25d98b02a` and `84cde1c7f` (model, store, migration and tests), plus the AC03 course and documentation commit that follows.

**Design.**
- **Core** (`bpr/core/local_comment.hpp`): `comment_state` (Local, Draft, Published; a draft record is never Local) and `local_comment_anchor` (a note's anchor plus a range start and a reply thread). `decide_keep_local` keeps the text, line or range and reply target, records the shown blob, and is refused as `held_by_review`, `publishing`, `published` or `discarded`. `decide_make_draft` returns a draft of the same kind at the same place, or one at the selected line (`Reanchored`, "Make draft on line N"). It is refused as `select_a_line` or `draft_stands`, and nothing is merged.
- **Host:** the ledger port's `store_note`, `erase_note` and `private_ledger_observation::Notes` are gone. They are now `store_local_comment`, `erase_local_comment` and `LocalComments` over `local_comment_record`. Its body (`Body`) has the 64 KiB draft bound, and it gains `StartLine` and `ReplyToThread`.
- **SQLite:** schema 12, table `private_local_comment`, documented in [review-ledger.md](../../architecture/review-ledger.md). It keeps the note bound of 4,096 a review, counted outside the 64 active drafts, and the 64 KiB body bound. Revisions continue from `note_revision`.
- **Migration:** inside the ledger's one migration transaction. Each note keeps its identity, revision, bytes, attachment, path and line, in note order. The note's own thread is removed unless referenced, and the three note tables are dropped. A test-only seam interrupts it after N statements.

**Identity.** A Local comment keeps the note identity space: migrated comments keep their note identity, so finding-card memory, which is kept in another store, still arrives. Drafts draw from the private frontier. Make draft and Keep local therefore move a comment between two identity spaces in one transaction. How a move keeps its identity is milestone 2's to settle with the moves' store operation.

**Evidence** (dev, Debug, macOS):
- AC11: `sqlite_review_ledger_storage` passes. It opens `tests/fixtures/review-ledger/v11-notes.sqlite`, written by the shipped code at `d6ccfb8b3`, and finds every note as one Local comment with equal identity, revision, bytes, anchor and order. The note threads are gone and the note identity edits its comment. An interruption after each of the migration's statements (at least 10) leaves schema 11 untouched, and the retry gives the same result.
- AC03: `application_review_submission_composition_public_path` writes a Local comment on a line and one on the pull request. Through every submission, single comment and reply that course makes, no body the fixture server received carries their words, and both are still Local afterwards. Local comments in threads and on a file, and Publish response, have no surface until milestones 2 to 4. The type keeps them out regardless: a Local comment is never a `review_comment_draft`.
- AC13: label counters exclude the `private_note` origin (`core_feedback_partition`), and a Local comment never enters a conversation observation. Composed evidence waits for the Comments drawer (milestone 2).
- `core_local_comment` covers the moves and refusals, and `sqlite_private_review_storage` the store and purge.

Changes outside the core of this card:
- The Overview's Note column now shows the file's comment. It read `supplement_note::File`, which no store filled. Milestone 3 retires the column.
- A selected Local comment longer than the agent supplement's 16 KiB refuses the agent request today. Milestone 4 (agent opt-ins) settles it.
