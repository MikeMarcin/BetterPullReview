# Review Ledger

The review ledger holds one reviewer's private progress: which file material
has been read, which has been accepted, which conversations have been seen,
which files are excluded, private notes, the last reviewed head, and local
defect classification and anchor history. It is private state. It is never a
GitHub approval, it is never synchronised between machines, and it never hides
GitHub state behind a local opinion.

Shared review state — comments, threads, replies, and reviews — is
[Review Round Trip](review-round-trip.md)'s. The two share no table and no
authority: this ledger is local and private, that one mirrors GitHub.

## Where it lives, and why it is its own file

```text
state/v1/review-ledger.sqlite3
```

It sits beside `state/v1/epoch-catalog.sqlite3` and is a **separate database**.
That separation is load-bearing rather than tidy: deleting private review state
must lose only private review state, and if the two shared a file, discarding a
reviewer's progress would also discard the derived git cache. A test asserts
the independence directly and fails if the two are ever pointed at one path.

`application_id` is `0x42505244` (`BPRD`) and the current `user_version` is
**3**.

## Ownership

| Region | Owns |
|---|---|
| Functional core | Receipt and acceptance transitions, invalidation across an epoch advance, actionability policy and its ranking, and the reason each ranked item carries |
| Application host | The database as durable authority, migrations, transactions, WAL and recovery, bounded observation construction, the persistence request and its typed completion |
| Qt adapter | Badges, markers, explanations, and the queue projection |

The shipped composition creates one `review_ledger_application_host` only
after accepting an exact host/account/repository/pull-request handoff. The
selected-diff publication supplies the current file/material observation;
unknown or unselected material remains conservatively actionable. Seen, read,
accepted, and excluded `QAction` gestures carry the projected file and material
back through the host. Notes, reviewed heads, and defects use that same host,
never a fixture-owned store call. Exact authority invalidation retires the host
and store together. Receipt-driven filtering reobserves actionability without
replacing the workbench authority or its selected comparison. When selected
material changes, the core reopens its receipt and the same-process refresh
returns that file to the actionable order without detaching the rendered diff.

If the SQLite store cannot open, the application installs one typed unavailable
store behind the same host. GitHub content, epoch selection, and diff rendering
continue, while the receipt actions are disabled and the workbench states that
private review progress is unavailable.

The core holds no part of the ledger and takes no SQLite dependency; its
receipt policy builds and runs under the `core-only` preset, which links
neither Qt nor SQLite. The adapter holds no policy: its queue model has no
mutating entry point at all, so "the presentation cannot mark state by mutating
a model" is true by construction rather than by convention.

## Typed vocabulary

- **Command** — `core::file_receipt_command`, carrying the file, the material
  it was issued against, and the kind.
- **Decision** — `core::file_receipt_outcome`, carrying the resulting receipt,
  a typed rejection, and whether anything changed.
- **Observation** — `core::file_material_observation` for an epoch advance, and
  `core::conversation_observation` for a conversation. Both are bounded records
  the host constructs; the core never asks for more.
- **Request** — `application::receipt_persistence_request`.
- **Completion** — the store's `std::expected<void, ledger_store_failure>`.
- **Delta** — `application::receipt_projection`, or
  `receipt_failure_publication` carrying exactly one of a core rejection or a
  store failure.
- **Private records** — `application::local_note_record`, an optional exact
  file/comparison attachment, one `core::git_commit_oid` last-reviewed head,
  and `application::durable_defect_record` binding a local defect history to
  its file and ordinary GitHub conversation identity.
- **Private observation** — `application::private_ledger_observation`, bounded
  to 4,096 notes and 4,096 defects and ordered by stable record identity.

Private writes and deletes are narrow store operations on those values. The
application host validates them before the port, and the SQLite adapter repeats
the validation at the authority boundary. Invalid records have their own typed
`invalid_record` outcome, and a full bounded scope reports `quota_exceeded`;
neither is folded into storage unavailability or a write conflict.

## Authority and ordering

The database is the authority. The order is: the core decides, the decision
becomes a persistence request, the store answers with a typed completion, and
only then is the projection published.

A projection published before the commit would show a reader a state the
database does not hold, and it would survive exactly until the next restart
contradicted it. The host tests make this observable rather than assumed: the
fake store and the fake projection append to one shared log, so a success reads
`request, committed, published`.

A command that is legal but changes nothing produces no request, no commit, and
no publication. Persisting it would turn every repeated click into a write.

## Concurrency

One serialized writer and bounded readers, through the same hardened connection
policy the epoch catalog uses — `configure_connection`, shared rather than
copied, because a copied policy drifts until the two databases are hardened
differently. It sets defensive mode, disables extensions, views, and triggers,
enables foreign keys, selects WAL with `synchronous = FULL`, and pins the page
size and a page-count ceiling.

The build sets `SQLITE_OMIT_AUTOINIT`, so `sqlite3_initialize` must run before
any other call. Both stores go through one `sqlite_runtime_ready`. A store that
opened a database without it did not fail cleanly — it jumped through a null
pointer.

## Schema

Version 1 introduced `ledger_meta`, `review_scope`, and `file_receipt`. Version
2 added `conversation_receipt`. Version 3 adds `local_note`,
`last_reviewed_head`, and `local_defect`.

Every table is `STRICT`, so a value of the wrong type is refused rather than
coerced. The two 256-bit digest columns — `file_receipt.file_identity` and
`file_receipt.material_id` — additionally carry `length(...) = 32` checks, so a
truncated digest fails at the row that caused it instead of becoming a puzzle
read back later. Comment and conversation identities are integers and need no
such check. Opening version 3 compares every non-internal `sqlite_schema`
object with the canonical table definition. A same-name table with changed
columns, checks, foreign keys, uniqueness, or without `STRICT` is a schema
mismatch rather than a partially understood ledger.

`local_note` has either no attachment or both a 256-bit file identity and a
256-bit comparison identity. The note identifier is unique within the review
scope, and text bytes are refused when empty or beyond 16 KiB. Notes store the
reviewer's own private text; they never store a GitHub comment or source body.

`last_reviewed_head` holds exactly one algorithm-tagged, padded Git object ID
per review scope. Replacing it changes no receipt, note, or defect row, so
changed-since-review can compare the current accepted head with the exact prior
head without retargeting any other state.

`local_defect` binds one defect ID to one file identity and one ordinary review
conversation. It stores severity, category, lifecycle, anchor availability,
and both the current and original RFC-BPR-0004 source-line identity. Current
policy requires those anchors to be equal, and both columns remain immutable
after insertion: the second copy is retained audit history, not a
substitute-location channel. Reusing a defect ID may update only severity,
category, lifecycle, and anchor availability. Any offered change to its file,
conversation, current anchor, or original anchor is a typed conflict and leaves
the row unchanged. `(scope, conversation)` is unique, so a thread cannot
silently acquire two competing local classifications.

### Identity scoping

`review_scope` is unique on `(host, account, repository, pull_request)` and
every other row references it with `ON DELETE CASCADE`. All four fields are
required: a pull request number is unique only within a repository, a
repository name only within an account, and an account only within a host.
Reusing pull request 7 on a second host must not surface the first host's
receipts or private records, and tests resolve that number across identities
differing in one field each. Material identity remains inside file receipts and
note attachments; last-reviewed state retains the exact commit; defects retain
the file, comparison, source side/line, and conversation identities.

### What the ledger refuses to store

- **Credentials.** Ever.
- **Duplicated source bodies.** Git objects stay in the git cache.
- **Presentation coordinates.** No Qt index, visual row number, ribbon path,
  measured y-coordinate, or comparison scroll position.

This is enforced against the schema the build actually creates rather than
against the insert statements someone remembered to read: a test enumerates
every column of every table and matches each against a forbidden substring set.
It also names a column it did inspect, so an empty schema cannot pass
vacuously.

## Migrations

Migration is **forward-only**. There is no downgrade path, because a downgrade
would have to discard data the newer version added.

A database written by a newer build is refused with a schema mismatch **before
anything is written to it**. That distinction matters and was nearly lost: an
earlier version of the test asserted only that the open was refused, and it
passed with the forward-only guard deleted, because validation rejected the
version afterwards and produced the same failure. The test now reopens the
refused file and asserts its recorded schema format is untouched.

A nonzero prior version is also migrated only when `application_id` already
names the review ledger. A version-1 or version-2 file stamped for another
application is refused before the migration transaction begins; its database
bytes, version, metadata row, schema, and receipts remain exact.

The whole advance runs in one transaction, so a database is never left between
two versions; a partially migrated ledger would be indistinguishable from a
corrupt one.

The current version is deliberately not 1. A migration mechanism whose only
version is the initial one has never migrated anything and would first be
exercised on a user's database. A real version-1 database traverses both
migrations, and an exact version-2 database traverses the 2-to-3 step. File and
conversation receipt rows written under the prior versions are asserted to
survive before any new private record is accepted.

## Receipt semantics

A file receipt attaches to **material**, never to a path and never to an epoch.
A path is renamed and an epoch advances while the bytes stay the same;
conversely the same path under a new epoch may be entirely different content.
Only the material answers "is this the thing I already read". The material is
RFC-BPR-0004's comparison digest rather than a second identity invented here.

Reading and accepting are **independent facts**. Marking a file read is not
approval, and accepting it is not a claim to have read it. Folding them into
one state machine would have forced a meaning onto "accepted but unread", which
has none.

The read progression is `unread → seen → read`. A weaker claim is absorbed
rather than treated as a regression — marking a read file seen leaves it read —
but it stays legal and reports no change. Only an explicit mark-unread reopens.

Every command carries the material it was issued against, so a command formed
while the reader was looking at one comparison cannot be applied to the
comparison that has since replaced it.

### Across an epoch advance

A receipt survives exactly when the compared material identity is unchanged.
When the material moved, the read progression and the acceptance are both
reopened and the receipt re-attaches to the new material.

**Exclusion survives.** Excluding a file is a standing statement about the file
— a generated artefact stays uninteresting when it is regenerated — not a claim
about particular bytes. Reopening it on every advance would make the setting
useless for exactly the files it exists for.

A file the advance no longer compares is reported absent rather than retained,
because retaining it would leave a receipt attached to material nothing
compares.

### Conversations

Both cursors record a comment identity **and** an edit version. An in-place
edit changes no identity, so an identity-only cursor would report a
conversation as read while its text had been rewritten.

## Actionability

The queue is ranked by reason first and file identity second. The reason
enumerators are declared in priority order and the ranking reads that order
directly rather than restating it in a comparison function, because two
statements of one order drift apart.

The identity tie-break is not decoration. Without it, items sharing a reason
keep whatever order the caller supplied, which looks stable on one run and is
not deterministic.

Exclusion sits outside the actionable range and is checked before every other
reason, so an excluded file with an open defect and unread conversations is
still excluded. Filters change which items appear and never how the remaining
ones are ordered.

The reason is a core value the adapter renders. An adapter that composed its
own explanation text would be deciding policy while looking like presentation.

## Crash recovery

Committed state survives abrupt termination and incomplete transactions are
discarded. This is proven by killing real processes: a forked child opens the
ledger, begins a transaction, writes, and announces readiness only once the
rows are inside the open transaction, then the parent sends `SIGKILL` and
asserts the child died from the signal rather than exiting gracefully. The
receipt committed beforehand returns byte for byte; the one inside the
abandoned transaction is absent. A second kill repeats the contract for a
note, reviewed head, and defect in one transaction: all three committed values
survive and all three abandoned replacements disappear together.

That test is POSIX-only and compiles out on Windows, where `fork` does not
exist. The Windows equivalent is unwritten rather than silently passing.

## Backups, retention, and purge

**Purge is deletion.** Removing `review-ledger.sqlite3` with its `-wal` and
`-shm` companions destroys all private review state; the next launch recreates
a clean ledger and the epoch catalog beside it is untouched. Both halves are
tested.

**No backup is taken, and no retention policy runs.** The ledger grows with the
reviews a reviewer touches and nothing prunes it. This is the current position
stated plainly rather than a policy: neither has been designed, and neither
should be inferred from this document. A ledger that quietly discarded a
reviewer's history on a schedule nobody chose would be worse than one that
grows.

## Privacy

Everything in the ledger is private to the person using this machine. It is
never uploaded, never synchronised, and never turned into a GitHub action.
Local acceptance is not an approval; a reviewer who accepts a file here has
told themselves something, not GitHub.

Apart from the reviewer's explicitly created private-note text, the ledger
records identities and progress rather than content: file and material
digests, comment identities and edit versions, defect classification and
semantic anchors, and the flags above. It does not record file paths, source
text, GitHub comment text, credentials, or anything about who wrote what. A
person reading the database can see the private notes and which opaque material
a reviewer marked read, but not the source or shared discussion those
identities refer to.

Removing it is a file deletion and needs no application feature to be
trustworthy.
