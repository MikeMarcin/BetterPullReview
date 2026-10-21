---
id: "RFC-BPR-0007"
title: "Local Review Ledger, File and Conversation Receipts, and Actionability"
status: "Auto-Verified"
area: "BPR"
milestone: "M2 Personal Review Workflow"
depends_on: ["RFC-BPR-0003","RFC-BPR-0005"]
pack_version: 2
---

# RFC-BPR-0007: Local Review Ledger, File and Conversation Receipts, and Actionability

## Summary

Persist private review progress and derive unread, accepted, changed-since-review, needs-verification, and next-actionable states without competing with GitHub shared state.


## Motivation

GitHub does not represent the personal inspection ledger required to answer what is unread, what was accepted, what changed after review, and what should be inspected next.

## Goals

- Add a versioned SQLite review ledger.
- Persist file receipts against immutable file material.
- Persist conversation read/acceptance cursors against comment versions.
- Persist local notes, exclusions, last-reviewed heads, and local defect metadata.
- Derive changed-since-read/accepted, unread, needs-verification, and next-actionable projections.
- Recover safely after restart and schema migration.

## Non-goals

- Synchronizing private state across machines.
- Treating local acceptance as a GitHub approval.
- Hiding GitHub state behind local state.

## Architectural allocation

- **Core:** owns receipt and acceptance transitions, actionability policy, next-actionable navigation, semantic command legality, and invalidation decisions based on file/comment material observations.
- **Host:** owns the SQLite ledger as durable authority, migrations, transactions, queries, cache invalidation, crash recovery, and construction of bounded receipt/conversation observations. The core does not hold the entire ledger.
- **Qt adapter:** owns badges, filters, explanations, actions, queue/file-tree projection, and event translation. It cannot mark state by mutating a Qt model directly.
- A receipt command produces a semantic persistence request; the host commits it and returns a typed completion before the core publishes the accepted projection.

## Storage design

SQLite contains identities, receipts, rules, notes, projections, and migration metadata. Git objects remain in the Git cache. Credentials and duplicated source bodies are prohibited. One serialized writer and bounded readers use prepared statements, foreign keys, transactions, and WAL.

**Owner amendment, 2026-10-06 ([[BPR-1210]]).** A private note on a file
identified that file only by opaque file and comparison identities, never by a
path. Once a later update dropped the file from both accepted snapshots, the
note could no longer name it. The owner accepted keeping the path:

- A file note now keeps its file's path as it was when the note was written,
  in ledger schema 8's `private_note_path`, one row per note thread.
- The path is the pull request's own file name, kept only in the private
  local ledger, and is never guessed or reconstructed.
- Notes written before schema 8 keep no path. They say "File comparison from
  an earlier update" when their file has left the pull request.
- Deleting a note deletes its path, and profile purge removes the ledger file
  as before.

**Owner decision, 2026-10-17 ([[BPR-1591]], RFC-BPR-0085 D3).** Private notes
may hang at a line, as threads do:

- A note written on a selected line keeps its side, zero-based line and the
  blob that side showed, in ledger schema 10's `private_note_line`, one row
  per note thread. Notes on the whole file or the pull request, and notes
  written before schema 10, have no row and keep their anchors.
- `core::place_note` decides where a note shows: at its line in the
  comparison it was written on, or in any other whose side shows the same
  blob; otherwise outdated with its reason (the side changed, the side is
  absent, or no blob was kept). A note is never moved to a nearby line, and an
  edit cannot move it.
- Deleting a note deletes its line.

## Receipt semantics

File receipts attach to `FileMaterialId`. A new epoch preserves a receipt only when the compared material identity remains equivalent. Conversation receipts record the latest seen/accepted comment identity and edit version. New or edited comments reopen unread state.

Amended 2026-10-08 by the owner's rule on [[BPR-1339]], verbatim: "Rule gap,
marking it read updates it to read. Accepted is stronger than read or unread".
Read, accepted and excluded stay independent facts on the receipt: accepting a
file does not mark it read, and revoking acceptance shows its read state again.
Where one state is shown or counted, accepted outranks read, seen and unread:
the file's mark is `core::progress_mark` (excluded, then accepted, then the
read progression, [[BPR-1331]]), and a file is done in my review progress once
it is read or accepted (`core::progress_done`). Overview's read column and the
Files drawer show the mark; "Files read", the "not read" count and Continue
review's next file count done files.

Amended 2026-10-21 by the owner's request on [[BPR-1699]], verbatim: "We
have some automated review bots that are very spammy on reviews. I think I
want the ability to Mark all comments as read and Mark all comments from a
particular user as read." `core::mark_conversations_read` decides a bulk read
over a pull request's conversations at once: every post, or one author's,
named by `core::review_author_of` from the login. The conversation cursor is
unchanged, so an author's mark moves each cursor over that author's (and my)
new posts and stops at the first new post by anyone else; what it leaves new
it counts. The receipts it changes are committed in one transaction through
`store_conversation_receipts`: all or none. It is a one-shot mark of the posts
observed; no author identity is stored and nothing is muted.

Amended 2026-10-21 by the owner's request on [[BPR-PENDING-P]], verbatim: "In
the toolbar I can uncheck accepted by clicking the accepted button. I should
be able to do the same for read and seen." Seen and Read are toggles, as
acceptance is. Read implies Seen, so taking Read off (`unmark_read`) leaves a
read file seen, and taking Seen off (`mark_unread`) leaves it unread. Neither
changes acceptance or exclusion, just as revoking acceptance leaves the read
progression where it was. An accepted file stays accepted and done. Opening a
file still records Seen: a file marked unseen stays unseen while it is shown
and is seen again the next time it is opened.

Where review state targets a diff location, it uses RFC-BPR-0004's stable file,
epoch, source-line/boundary, change-group, or move-relation identity. The ledger
never persists Qt indexes, visual row numbers, ribbon paths, measured
y-coordinates, or comparison scroll positions.

## Actionability

The domain derives an ordered actionable queue from GitHub state, local receipts, defect state, and current filters. The UI can explain why an item is next.

## Acceptance criteria

- Read, accepted, excluded, and seen states survive restart.
- Changed material invalidates only affected receipts.
- New or edited comment versions reopen the correct conversation.
- Database deletion loses only private state and recreates a clean ledger without changing GitHub.
- Abrupt termination and WAL recovery preserve committed state and discard incomplete transactions.
- Migrations are forward-only, versioned, backed up where policy requires, and tested from every supported prior schema.
- Strong identity prevents state reuse across host, account, repository, PR, epoch, and material boundaries.
- `Next actionable` is deterministic and explains its ranking.
- Receipt/actionability transitions are directly replay-tested without SQLite/Qt, while host persistence tests and Qt projection tests prove the complete request/completion/delta path.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Receipt invalidation is precise | Domain plus SQLite | Advance fixture with mixed changed/unchanged material |
| Crash recovery is sound | Process-kill integration | Kill during transaction/refresh and restart |
| No cross-identity leak | Persistence integration | Reuse PR numbers and paths across hosts/accounts/repos |
| GitHub remains unchanged | Database-loss test | Delete ledger and inspect independent GitHub fixture state |
| Ledger decision/effect split holds | Core + fake store + Qt tests | Receipt intent transitions, persistence request, completion, and UI delta are all explicit |

## Documentation impact

Schema, migration, identities, receipt semantics, actionability, backups, retention, purge, crash recovery, and privacy are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
