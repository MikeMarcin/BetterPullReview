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
**11**.

Review Cycles keep their own file, `state/v1/review-cycles.sqlite3`
([RFC-BPR-0053](../rfcs/RFC-BPR-0053-review-cycles-domain-and-local-authority.md)),
for the same reason. A cycle's notes, reviewed marks, rounds, responses and
verifications are records of that store, not ledger receipts: the ledger has no
cycle table and no ledger row names a cycle, and a pull request a cycle
published or linked starts with the ledger state any other pull request has.
Profile purge lists the two files separately.

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
Only the shown file's receipt can withhold it, so leaving a read or excluded
file for another returns it to the order and the Files drawer at once
([[BPR-PENDING-Q]]).

The review bar's named Accept button ([[BPR-1507]]) presents the same acceptance
`QAction` as the private-progress menu. Its command, enablement, checked state
and inverse label come from the accepted receipt projection. The direct click
adds no policy, automatic acceptance or persistence path; the state icon and
alternate actions remain in the adjacent menu. Its Accept command has
Option-A on macOS and Alt+A elsewhere ([[BPR-1515]], replacing
Command+Shift+A in [[BPR-1531]]), shown in the menu and direct button's
tooltip. The binding is absent when the projected command is Revoke
acceptance or a widget that takes typed text has focus, and such a widget
claims the key when it arrives, so Option-A types `å` there; key repeat is
disabled. The key therefore uses the same exact-material eligibility and
never turns a repeated Accept into Revoke.

If the SQLite store cannot open, the host answers every operation with that
typed opening failure. GitHub content, epoch selection, and diff rendering
continue, while the receipt actions are disabled and the workbench states that
private review progress is unavailable.

### Off the decision lane

[[BPR-1211]]. Every store operation runs on the profile's review-ledger effect
lane, including opening the store and closing it. That lane is one worker thread
shared by the profile's windows. Each host queues its operations and runs them
one at a time, in the order they were asked for. A receipt command is decided by
the core on the decision lane when it reaches the head of that queue, against
the durable state the operations before it left. Its typed completion returns to
the decision lane, and only then is anything retained or published. A host
holds at most 64 operations; a gesture beyond that is refused `unavailable` and
presented, rather than growing a queue behind slow storage.

No host lock is held across storage. Reads answer from what the host has already
loaded or committed and never touch the store: the retained receipt, durable
receipts, conversation receipts and the private observation. A cache miss is an
explicit asynchronous load that republishes when it answers. The overview reads
all missing receipts in one operation, and the read state loads the
conversation receipts once. The private records load once when the store opens
and again when a sibling changes them, and this host's own writes update them
as they become durable. A read that began before a sibling's invalidation
retains nothing, so it cannot restore what that invalidation removed.

Retiring a host, on authority replacement or when its window closes, answers
queued operations `cancelled`. A completion that returns afterwards is refused:
the store may have committed it, but nothing is retained or published into the
successor, which reads the same root and so recovers what was committed. The
store then closes on the effect lane, after the operation running there. Agent
sessions and promoted agent notes go through the same host, and their answers
arrive later on the decision lane. The private review store opens the same file
after this ledger's opening has answered, as it did when that opening was
synchronous.

[[BPR-1254]]. The ledger is one file per profile, and it is not this lane's
alone: every window's private review store reads and writes it too, from the
mutation lane and from agent turns. Opening validates a copy of the file and
refuses the file as `unavailable` if its main, WAL or SHM bytes changed
meanwhile, so a private read or write during a reopen (a moved head reopens the
ledger) refused an ordinary opening, and the agent's session-catalog read after
it. The profile therefore holds one review-ledger file exclusion. Every task on
the review-ledger lane runs under it, and so does every private review store
operation, so only one of them touches the file at a time. This is the one lock
held across storage, and no product path takes it on the decision lane.

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
size and a page-count ceiling. The ledger then raises only its SQLite row/string
ceiling to 2 MiB so a complete 1 MiB assistant body and bounded metadata fit in
one row. Per-field and per-scope private-content bounds remain stricter; other
databases retain their existing connection ceiling.

The build sets `SQLITE_OMIT_AUTOINIT`, so `sqlite3_initialize` must run before
any other call. Both stores go through one `sqlite_runtime_ready`. A store that
opened a database without it did not fail cleanly — it jumped through a null
pointer.

## Schema

Version 1 introduced `ledger_meta`, `review_scope`, and `file_receipt`. Version
2 added `conversation_receipt`. Version 3 adds `local_note`,
`last_reviewed_head`, and `local_defect`. Version 4 adds
`agent_session_sequence` and `agent_session`. Version 5 normalizes the
historically one-based conversation-defect line to the core's zero-based
coordinate and marks every migrated defect anchor outdated. The legacy row did
not retain enough facts to prove whether it came from a current right thread,
a left thread, an outdated original line, or an unrelated retained comparison;
arithmetic alone cannot make it trustworthy. Application reconciliation may
restore availability only when the accepted current thread and rendered
comparison prove the exact file, right-side current line, comparison digest,
accepted-head endpoint, and in-bounds source row.

Every table is `STRICT`, so a value of the wrong type is refused rather than
coerced. The two 256-bit digest columns — `file_receipt.file_identity` and
`file_receipt.material_id` — additionally carry `length(...) = 32` checks, so a
truncated digest fails at the row that caused it instead of becoming a puzzle
read back later. Comment and conversation identities are integers and need no
such check. Opening supported versions 3 through 11 compares every non-internal `sqlite_schema`
object with the canonical table definition. A same-name table with changed
columns, checks, foreign keys, uniqueness, or without `STRICT` is a schema
mismatch rather than a partially understood ledger.

`agent_session_sequence` is the non-pruned high-water mark for one review
scope. `agent_session` holds only immutable session/profile, review identity,
base/head OIDs, and epoch metadata. Admission uses `BEGIN IMMEDIATE`: reserving
the next ID, inserting its row, advancing the sequence, and pruning beyond the
newest 128 records commit atomically. The high-water row is never derived from
the retained maximum, so pruning, restart, and two simultaneously opened stores
cannot reuse an ID. Signed 64-bit exhaustion is a typed quota refusal.

Version 6 added note revision/CAS and its non-pruned revision sequence.
Version 7 replaces `local_note` and `local_note_sequence` with the private
review authority below. The temporary `local_note_record` facade maps each
legacy note to its original turn in a canonical thread. Reads project that
turn's current revision; writes append reviewer revisions. Canonical thread
updates invalidate the facade's CAS token as well. The mapping holds no text.
Legacy attachments remain either absent or an exact file/comparison pair.

Version 11 (RFC-BPR-0089 M2) adds `generated_mark`: one row per file the
reviewer made a generated-file statement about in that review scope, keyed by
`file_identity`, with `mark` (1 Show as normal, 2 Treat as generated) and
`review_anyway`. A row that says nothing (`mark = 0` and not reviewed anyway)
is refused by the table's own check, so a missing row and "no mark" mean the
same thing and forgetting a mark erases its row. The private observation loads
the marks in identity order, the order the core keeps them in. A generated
mark is not the receipt's Excluded and changes no receipt: generated status is
derived from the mark and the head's `linguist-generated`, and only the
reviewer's own Excluded is a lasting statement about a file's review.

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

### Private review authority (version 7)

The application `private_review_store_port` and SQLite factory expose bounded
storage operations; they have no agent, GitHub or publish capability. The
existing shared draft host remains the owner of draft decisions and fan-out.
Its persistent value uses the existing `revisioned_review_draft`; restoring
bytes does not restore runtime scope or publication grants. The M6/M7 hosts and
M8 workbench now use this authority. Local public-path evidence is recorded in
[RFC-BPR-0023](../verification/RFC-BPR-0023.md); designated GitHub readback is
still pending.

| Tables | Durable responsibility |
|---|---|
| `private_store_identity`, `private_scope_state` | Ledger incarnation, stable review binding, non-pruned allocation/CAS high waters, catalog generation, count and byte counters. |
| `private_thread`, `private_turn`, `private_content` | Thread/version/binding, turn tip and immutable turn, finding or candidate content with digest and typed metadata. |
| `private_run`, `private_run_source` | Run admission and terminal provenance independent of the pruned session catalog; exact selected source pins. |
| `private_edge` | Scope-qualified immutable derivation edges; source order precedes destination order. |
| `private_composition`, `private_composition_source` | Versioned composition and ordered exact source selection. |
| `private_draft` | Existing draft state and exact target; linked drafts reference candidate bodies instead of duplicating them. |
| `private_publication` | Local pending join and receipt correlation, including separate approved-prose and wire-body digests. |
| `private_legacy_note` | Bodyless old-note mapping and original revision/CAS compatibility. |

A write contains at most 256 records. New objects consume consecutive IDs from
one scope frontier under `BEGIN IMMEDIATE`; edits carry expected versions.
Source existence/digest/tip checks, accepted composition and draft correlation,
quota counters and the allocation frontier commit together. IDs never come
from retained maxima and never wrap at signed 64-bit exhaustion. Immutable run
terminals keep their admitted source set even when a later reviewer edit has
advanced a source tip; the retained predecessor remains pinned.

Catalog pages hold at most 64 headers/64 KiB metadata. Transcript pages hold
64 revisions/512 KiB body text. An oversized body returns its exact identity,
digest, full byte count and a continuation; each chunk advances by at most
512 KiB and never changes the full body's completion or eligibility. Cursors
bind the scope/incarnation and catalog generation or thread version; stale
cursors refuse. Revision pages use a tested scope/thread/order index and no
OFFSET scan. Run detail requests hold at most 16 runs/256 KiB; source requests
hold at most 256 exact references. Publication and dependency pages have at
most 64 entries. The bounded draft collection retains at most 4,096 records, with at most 64 active authored drafts. Empty revision-one placeholders from older builds do not consume the active allowance; published and discarded records remain historical evidence. The existing 64 MiB private-payload budget still applies.

Schema 9 rebuilds only the scope-counter table, copying every counter, binding, generation and allocation frontier while widening its retained-draft bound. It leaves all draft bytes, composition references and publication lineage in place. Browsing a source line creates one runtime-only empty selection per window consumer and writes no record or durable identity. The first edit creates the normal draft and applies that edit in one transaction before its projection can publish.

An untouched sibling selection at the same complete runtime scope, exact anchor and reply target follows that committed identity. A sibling with its own candidate or save in flight keeps its own identity and acknowledgement; simultaneous authors retain separate saved comments rather than overwriting either body. Other anchors and authorities do not follow the first save.

There is no automatic expiry of authored text or publication history. Explicit cleanup removes only empty, never-edited revision-one ordinary placeholders, checks for publication references and rechecks eligibility transactionally. A racing accepted edit refuses the whole cleanup; a retained allocation frontier prevents a late restore from resurrecting removed placeholders. Written, intentionally cleared, composed, reply, pending and uncertain material is preserved.

Per scope, the store admits 4,096 threads, 65,536 turn revisions, 65,536 runs,
65,536 findings, 4,096 compositions, 65,536 candidate revisions and 65,536
publication records. Reviewer/candidate text is at most 16 KiB, assistant text
1 MiB and ordinary draft text 64 KiB. Thread bodies have a 64 MiB aggregate
budget; other private payloads have a separate 64 MiB budget. Admission refuses
the entire transaction without truncation or implicit pruning.

Migration traverses every supported v1–v6 origin atomically. Each old note
becomes one thread/turn/revision with exact bytes, attachment and
`legacy_unknown` actor; v6 note revision and the deleted-highest revision
sequence survive. `local_note` and its sequence are dropped only within the
successful migration transaction. Receipt, defect and session migrations keep
their existing behavior. Canonical validation includes all v7 tables and
constraints.

### Identity scoping

`review_scope` is unique on `(host, account, repository, pull_request)`.
Scope-owned rows use scope-qualified keys and foreign keys. The v7 private
store uses ordered deletion inside one transaction, preserving the hardened
connection's trigger-depth limit and refusing external references. Older
receipt tables retain their existing schema. All four scope fields are
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
names the review ledger. A prior-version file stamped for another
application is refused before the migration transaction begins; its database
bytes, version, metadata row, schema, and receipts remain exact.

Existing main, WAL, and SHM files are captured byte for byte before SQLite opens
the supplied path. The main file and any retained WAL are materialized in an
owner-private temporary directory; SQLite classifies that isolated clone against
application ID, user/schema versions, metadata row, and canonical schema. The
clone owns hard-linked identity and main/WAL authorities. Cleanup truncates the
sensitive authorities first, deletes only the four known SQLite children plus
the identity child, verifies the captured directory identity and emptiness, and
surfaces substitution, an unknown child, or incomplete deletion as a refusal.
It never recursively deletes a temporary path.

The originals are recaptured once after clone cleanup and again at the bounded
preflight-to-open application boundary. Both captures must match before the
original path reaches `sqlite3_open_v2` or connection PRAGMAs. Deterministic
tests replace all three namespace entries at that boundary with wrong-app,
future-version, and tampered retained-WAL files; both displaced originals and
substitutes remain exact. This is a bounded application ordering check, not a
claim of impossible atomic protection against a hostile filesystem racing after
the final comparison. The v3-to-v4 course preserves prior records, while every
incompatible refusal preserves exact main/WAL/SHM existence and bytes.

The whole advance runs in one transaction, so a database is never left between
two versions; a partially migrated ledger would be indistinguishable from a
corrupt one.

The current version is deliberately not 1. A migration mechanism whose only
version is the initial one has never migrated anything and would first be
exercised on a user's database. A real version-1 database traverses every
migration, an exact version-2 database traverses the 2-to-3 and 3-to-4 steps,
and an exact version-3 database traverses the 3-to-4 step. Prior receipts,
notes, and reviewed head are asserted to survive before session metadata is
accepted.

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
but it stays legal and reports no change. Only an explicit command moves it
back ([[BPR-1701]]): `mark_unread` (Mark unseen, Seen's inverse) returns it
to `unread`, and `unmark_read` (Mark unread, Read's inverse) takes a read file
back to `seen` and leaves any other file as it is. Read implies Seen, so taking
Seen off clears Read too. Neither changes acceptance or exclusion, just as
revoking acceptance leaves the progression where it was.

Opening a pull-request file records Seen automatically after its exact selected
comparison is successfully presented and its material receipt is observed
([[BPR-1506]]). The completion rechecks the file, comparison and request generation
before submitting the ordinary core-decided durable command. Failed or superseded
loading records nothing; preparing nearby files in the background records nothing.
Material loaded under Home or Overview stays unread until the comparison is shown
in Review. An explicit Mark unseen stays until the next open, and an
explicit Mark unread leaves the file seen.
The accepted projection follows persistence. Reopening an already Seen or Read
material needs no write, and opening never accepts or verifies a file.

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

[[BPR-1601]] represents each pull-request comment and review summary as a
single-post conversation. The existing scope and receipt table remain unchanged;
`issue-comment:`, `review:` and `review-thread:` projection identities occupy
distinct namespaces before deriving the durable keys. Core read/unread and
own-post rules apply equally to these singleton conversations. A saved receipt
with a zero seen cursor (Mark unread) still counts as seen before. The queue
counts unread posts only in previously seen conversations; Discuss, Threads
and the Status drawer show every unread post, including a never-read one.
The host publishes the pathless count after the receipt commits, using the
same incremental cache as thread reads.

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

Live review-comment write recovery is deliberately not part of this ledger.
Its separate owner-private `review-mutation-recovery.sqlite3` database stores an
exact-scope canonical write fingerprint, resolved anchor/body, state/receipt,
and at most 100 sorted exact-match comment node IDs observed before admission.
Schema version two keeps those baseline bytes in a keyed side table so a
version-one migration can retain successful receipts while representing old
unresolved rows as baseline-incomplete. Such rows cannot resolve or POST. New
unresolved rows, baseline bytes, SHA-256 integrity digest, and prepared
transmission phase are committed in one transaction. Only a transport-owned
callback after native POST-reply creation can durably mark an attempt possibly
transmitted. Prepared attempts perform no recovery transport; possibly-
transmitted restart reconciliation is GET-only and accepts exactly one current
match absent from that baseline. The store also refuses to resolve any direct
receipt already in the baseline. The digest detects stored corruption under
the owner-private database assumption; it is not a same-user tamper-proof
authority.

## Backups, retention, and purge

Removing `review-ledger.sqlite3` with its `-wal` and `-shm` companions destroys
all ledger state. The next open creates a new private incarnation, so retained
references cannot attach to reused numeric IDs. The neighboring epoch catalog
and separate mutation-recovery journal are untouched.

The v7 private-store scope purge is narrower: after producer quiescence it
removes that scope's threads, runs, findings, compositions, drafts and local
publication records in one transaction, retaining incarnation and high-water
tombstones. It leaves receipt/defect/session data and external publication
recovery authority to their existing owners. Individual thread deletion refuses
while another retained object or run needs it; bounded dependency pages support
explicit cleanup. Neither operation invokes a GitHub capability.

**No backup is taken, and no time-based retention policy runs.** Receipts,
notes, reviewed heads, and defects are not pruned. Only the agent-session
metadata catalog has its explicit per-review bound of 128 newest rows; its
separate sequence survives pruning. No other scheduled deletion should be
inferred from that narrow bound.

## Privacy

The ledger is private to the person using this machine and performs no upload,
synchronization or GitHub action. Later host operations may send only explicitly
selected private sources to an agent or separately approved candidate prose to
GitHub through their established capabilities. Local acceptance of file material
is never a GitHub approval.

The v7 private authority retains explicitly authored reviewer and assistant
turns, structured finding snapshots, composition candidates, draft text and
bounded run metadata. Typed actor, requested/configured/reported provenance and
exact source lineage remain inspectable. Finding snapshots can contain their
reported repository-relative path and evidence; the store does not duplicate
source files or shared GitHub threads. Selection lists retain exact references,
not copied transcripts. Credentials, argv, environment, workspace roots and
presentation coordinates are excluded. A reader of the owner-private database
can read retained private prose as well as progress identities; this is not an
encrypted store or a same-user tamper-proof authority.

Removing it is a file deletion and needs no application feature to be
trustworthy.

## Private invocation retention

The private host commits reviewer prompt and admitted run atomically, then
uses a source-use claim through private input acknowledgement. The SQLite claim
is an immediate transaction on the effect lane: a competing writer receives
`busy`, and write/refusal/timeout releases the claim. Run terminal updates are
one-way from admitted to a typed terminal. Complete assistant text is retained
as a turn under that exact run; findings retain run/ordinal/digest and an explicit
finding-linked thread retains a dependency edge to its snapshot.

Run payload version 2 stores ordered assistant-item byte lengths alongside the
output digest. Version 1 payloads still read exactly with boundaries absent;
this is an opaque-record extension inside unchanged ledger schema 7. Text is
not duplicated in run metadata. Bounded run pages (16 records/256 KiB) support
recovery after producer quiescence; pending admitted runs become interrupted
without launching an agent. The session catalog's 128-row pruning does not
remove these separately retained runs or their transcript lineage.


## Composition publication lineage

Ledger schema 7 retains the private publication join beside durable drafts and
immutable composition revisions. Publication payload version 2 adds the exact
idempotency key, actor node/database/login, path, accepted head and receipt-space
reservation. Version 1 payloads remain readable with these formerly unrecorded
facts absent; no actor or recovery authority is invented. The remote logical
journal remains in the recovery database (schema 6 since [[BPR-1540]]) and retains ownership of send/reconcile state.

Pending joins reserve 4 KiB in the existing payload quota before remote
admission. Completion consumes this reserve, atomically attaches the canonical
receipt and advances the exact publishing draft; later quota growth cannot
strand that bounded receipt. The indexed logical-ID lookup returns one row;
server-ID and draft/advisory views remain cursor-bounded pages. A failed local
commit is repairable from the existing remote journal and exact server reread,
including after a head advance, without another POST. Missing, ambiguous or
mismatched viewer, PR, journal, body or comment identity leaves the join pending.
Purge and lineage retention still obey the existing
producer-quiescence and dependency rules.

The Saved comments viewer can explicitly discard a still-unpublished draft, including one on an earlier head. This changes its local state and frees an active slot while retaining its exact text in history. The command checks the listed revision and cannot discard an uncertain publishing draft or a confirmed server comment.

Saved-comment recovery actions carry the full private scope and the selected draft revision. A head update can retire an editable projection while the same scoped history remains inspectable; that update does not silently swallow a confirmed local discard. An account, PR, profile or private-incarnation change refuses the action.
