# Review Ledger

The review ledger holds one reviewer's private progress: which file material
has been read, which has been accepted, which conversations have been seen, and
which files are excluded. It is private state. It is never a GitHub approval,
it is never synchronised between machines, and it never hides GitHub state
behind a local opinion.

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
**2**.

## Ownership

| Region | Owns |
|---|---|
| Functional core | Receipt and acceptance transitions, invalidation across an epoch advance, actionability policy and its ranking, and the reason each ranked item carries |
| Application host | The database as durable authority, migrations, transactions, WAL and recovery, bounded observation construction, the persistence request and its typed completion |
| Qt adapter | Badges, markers, explanations, and the queue projection |

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
2 added `conversation_receipt`.

Every table is `STRICT`, so a value of the wrong type is refused rather than
coerced. The two 256-bit digest columns — `file_receipt.file_identity` and
`file_receipt.material_id` — additionally carry `length(...) = 32` checks, so a
truncated digest fails at the row that caused it instead of becoming a puzzle
read back later. Comment and conversation identities are integers and need no
such check.

### Identity scoping

`review_scope` is unique on `(host, account, repository, pull_request)` and
every other row references it with `ON DELETE CASCADE`. All four fields are
required: a pull request number is unique only within a repository, a
repository name only within an account, and an account only within a host.
Reusing pull request 7 on a second host must not surface the first host's
receipts, and a test resolves that number across four identities differing in
one field each.

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

The whole advance runs in one transaction, so a database is never left between
two versions; a partially migrated ledger would be indistinguishable from a
corrupt one.

The current version is deliberately not 1. A migration mechanism whose only
version is the initial one has never migrated anything and would first be
exercised on a user's database, so the 1-to-2 step is tested against a database
really written by the older schema, and a row written under version 1 is
asserted to survive.

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
discarded. This is proven by killing a real process: a forked child opens the
ledger, begins a transaction, writes, and announces readiness only once the row
is inside the open transaction, then the parent sends `SIGKILL` and asserts the
child died from the signal rather than exiting gracefully. The receipt
committed beforehand returns byte for byte; the one inside the abandoned
transaction is absent.

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

The ledger records identities and progress, not content: file and material
digests, comment identities and edit versions, and the flags above. It does not
record file paths, source text, comment text, or anything about who wrote what.
A person reading the database learns which opaque material a reviewer marked
read, not what that material said.

Removing it is a file deletion and needs no application feature to be
trustworthy.
