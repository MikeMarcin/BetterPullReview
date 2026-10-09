# RFC-BPR-0007 Verification Plan and Ledger

RFC base: `ea3a3b8223c00f9e0999bf0ab2882496abf65843`

Status: **RFC-BPR-0007 is `Auto-Verified` as of 2026-09-02; the closeout
record is on BPR-0241 and in the Closeout section at the end of this ledger.**
Before that: BPR-0121 through BPR-0128 delivered file and conversation
receipts, actionability policy, Qt projection, and the first two schema
versions; BPR-0238 added schema version 3 and bounded ports for private notes,
exclusions, last-reviewed heads, and defect metadata/history; BPR-0228
composed one `review_ledger_application_host` under the exact accepted
pull-request authority; BPR-0229 through BPR-0231 composed the round trip,
the defect workflow, and honest queue enrollment; BPR-0472 removed the last
platform defect (a directory in the ledger's place escaped the store as an
exception on libstdc++). Unknown material remains conservatively actionable.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0003 and RFC-BPR-0005. Both remain incomplete and
`Proposed`; their live cards name the available-target, target-native, and
attended work still open. Done BPR-0088 records the product owner's direction
to advance the implementation frontier despite that open work. Closure order
is unchanged: this RFC cannot become `Auto-Verified` before RFC-BPR-0005 does.

## Architecture allocation

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Receipt and acceptance transitions, invalidation decisions from material observations, actionability policy, next-actionable ordering and its explanation, semantic command legality | Qt-free and SQLite-free replay tests over transition tables; forbidden-dependency scan |
| Application host | The SQLite ledger as durable authority, forward-only migrations, transactions, WAL and crash recovery, bounded observation construction, persistence request and typed completion, cache invalidation | Host tests against a real database file, process-kill recovery, migration-from-every-prior-schema tests |
| Qt adapter | Badges, filters, explanations, actions, queue and file-tree projection, event translation | Qt adapter tests asserting the adapter never marks state by mutating a model directly |
| Composition root | Wiring the ledger into the real application | Selected public-path evidence through the real executable |

The core holds no part of the ledger and takes no SQLite, Qt, filesystem,
clock, thread, or credential dependency. A receipt command produces a semantic
persistence request; the host commits it and returns a typed completion before
the core publishes the accepted projection. The adapter cannot mark state by
mutating a Qt model.

## Reuse rather than reimplementation

The pieces below already exist and are composed rather than rebuilt.
Introducing a second database connection policy, a second migration mechanism,
or a second identity vocabulary is a defect, not an implementation choice.

- `src/adapters/sqlite/src/catalog_database.cpp` — the connection, `PRAGMA
  user_version` schema versioning, WAL, foreign keys, prepared statements, and
  the serialized-writer policy. The ledger uses the same policy.
- RFC-BPR-0004's stable identities — file, epoch, source-line and boundary,
  change-group, and move-relation. The ledger persists these and never a Qt
  index, visual row number, ribbon path, measured y-coordinate, or comparison
  scroll position. A test asserts that prohibition against the schema rather
  than trusting review.
- RFC-BPR-0003's epoch and material identity, which is what makes "the compared
  material identity remains equivalent" decidable.

## Claim matrix

A row is claimed only when its card lands with direct evidence. Rows still
reading `Planned` have none yet, and a partially claimed row says which half.

| Claim | Direct surface | Command or interaction | What proves it | Result |
|---|---|---|---|---|
| Receipt invalidation is precise | Core plus SQLite | Advance a fixture with mixed changed and unchanged material | Only receipts whose material identity changed are invalidated; unchanged ones survive the epoch advance | Verified. BPR-0122 asserts retention per file across a mixed advance rather than by count, and distinguishes an absent file from a changed one; the persisted half is BPR-0125's host carry-over (`test_material_observation_loads_carries_and_publishes_in_order` in `application_review_ledger_host`) and BPR-0228's composition contract, whose newer-head reopen proves a receipt load and one core-decided carry-over write. [[BPR-1331]] found the composed application keyed each receipt by the comparison digest, which names the epoch, so a push reopened unchanged files too; the receipt now attaches to `core::file_material`, and `application_review_progress_push_contract` pins a changed file reopening unread, an unchanged one staying accepted and an excluded one staying excluded across a real push |
| Read, accepted, excluded, and seen survive restart | Host persistence | Set each state, restart, re-read | Every state returns identical after a clean restart | Verified. BPR-0125 writes each of the four states to its own file identity in a real database, reopens, and compares byte for byte; a file never written reads back absent rather than defaulted |
| New or edited comments reopen the right conversation | Core plus host | Add a comment, then edit one in place | Both reopen unread, and only the affected conversation | Verified. BPR-0121 proves an in-place edit reopens unread and outdates acceptance while carrying the same comment identity; the host builds the bounded conversation observations the core decides on (BPR-0125) and the conversation receipts persist and migrate with their identity (BPR-0124, BPR-0238, `sqlite_review_ledger_storage`) |
| Crash recovery is sound | Process-kill integration | Kill during a transaction and during a refresh, then restart | Committed state survives and incomplete transactions are discarded | Transaction persistence verified. BPR-0126 SIGKILLs a child holding an open receipt transaction. BPR-0238 repeats the real kill with note, reviewed-head, and defect writes in one transaction: all committed values survive and all abandoned values disappear together. Both assert `WTERMSIG == SIGKILL`; the POSIX-only tests compile out on Windows. The composed refresh path is BPR-0228's composition contract, replayed at closeout |
| Migrations are forward-only and complete | Host migration tests | Open a database at every supported prior schema | Each migrates to current and no migration is reversible | Verified through schema version 3. A real version-1 database with nonzero valid identities and legal acceptance state traverses 1→2→3; an exact version-2 fixture traverses 2→3 and retains both file and conversation receipt rows. A newer database is refused *without being written to*. Separate wrong-application v1 and v2 fixtures are refused before migration and preserve the whole database byte-for-byte, application/user/schema metadata, exact `sqlite_schema` definitions, the exact file receipt, and the v2 conversation receipt |
| No cross-identity leak | Persistence integration | Reuse PR numbers and paths across hosts, accounts, and repositories | No receipt is visible outside the identity that wrote it | Verified. BPR-0124 resolves pull request 7 on four identities differing in one field each to four distinct scopes, and BPR-0125 shows the same file identity written under one host carries no receipt under another |
| GitHub remains unchanged | Database-loss test | Delete the ledger and inspect an independent GitHub fixture | Only private state is lost and a clean ledger is recreated | Verified for store independence. BPR-0126 deletes the ledger with its `-wal` and `-shm`, recreates a clean one, and shows a second database beside it untouched; the test fails if the two ever share a file. Inspecting the independent GitHub fixture is BPR-0128's public path, terminal, and the composition contract's seed/reopen course runs against that fixture |
| Next actionable is deterministic and explains itself | Core | Rank a fixture queue twice and read the explanation | The order is identical and the explanation names the deciding facts | Verified. BPR-0123 ranks a fixture whose top reason is tied and whose supplied order the tie-break must overturn, and re-ranks the reversed input to the identical result; the reason is a core value the adapter renders rather than composes |
| The ledger decision and effect split holds | Core, fake store, Qt, and shipped composition | Drive a receipt intent end to end | Intent transition, persistence request, typed completion, and UI delta are each separately observable | Verified. BPR-0125's fake store and fake projection share one log, and BPR-0228 drives seven inverse receipt actions through the shipped `QAction` surface, requiring one command, durable completion, and changed projection per gesture |
| The core stays free of storage and presentation | Dependency scan | Scan the core target | No SQLite, Qt, filesystem, clock, thread, or credential identifier appears | Verified. The whole ledger policy — receipts (BPR-0121), carry-over (BPR-0122), and ranking (BPR-0123) — builds and runs under `core-only`, which links neither Qt nor SQLite, so the separation is proven by the build rather than by a scan |
| The application-owned store persists a receipt | Shipped action/host/store composition | Run `application_review_ledger_composition_contract` | A real selected-file gesture supplies exact material, receipt actions persist through the host, and a later process loads and carries the receipt forward without direct fixture-store access | Verified by BPR-0228. Seed and reopen use one private root; the seed proves filtering retains the selected rendered comparison and same-process changed material re-enters actionability, while the newer-head reopen proves a receipt load and one core-decided carry-over write. Notes, the prior reviewed head, and defect history remain exact. A source guard rejects direct fixture calls to the concrete store. This course owns `MaterialReceiptHits` evidence: the counter records a receipt hit by the selected-material observation, not review-ledger database existence, so the epoch-maintenance course does not substitute for it. BPR-0824 moved the course from `ReceiptLoadHits`, which also counts the window's own `refresh_shared_ledger` re-read of its carry-over commit; on Linux that re-read usually ran before the probe sampled |
| Notes, exclusions, last-reviewed heads, and defect metadata persist | Host persistence, migration, and composition | `ctest --preset dev -R '^(application_review_ledger_host\|sqlite_review_ledger_storage\|application_review_ledger_composition_contract)$' --output-on-failure` | Every record survives clean/crash restart and cannot leak across host, account, repository, PR, epoch, or file/conversation identity | Verified. BPR-0238 supplies schema-v3 isolation, bounds, migration, conflict, and SIGKILL evidence. BPR-0228 persists note/head/defect records through the one composed host and reloads them in a second real process. Public defect mutation remains BPR-0230 |
| The ledger persists no presentation coordinate | Schema scan | Inspect every column of the created schema | No Qt index, row number, ribbon path, y-coordinate, or scroll position is stored | Verified. BPR-0124 enumerates every column of the created schema and matches each against a forbidden substring set, and names a column it did inspect so an empty schema cannot pass |

## Standing composed-host linkage audit

BPR-0248 requires the exact demangled
`bpr::application::review_ledger_application_host` constructor in both the
development-fixture and production binaries. This is linkage evidence only;
the receipt actions, persistence completion, restart, and projection assertions
in `application_review_ledger_composition_contract` remain the construction
and runtime evidence.

The review-mutation linkage inventory is profile-specific. Both profiles
require the asynchronous application-host and coordinator constructors plus
the generic `bpr::application::reconcile` decision used after mutation. The
development-fixture profile additionally requires the exact test-only GitHub
public-path provider boundary and testing-grant factory while forbidding the
live provider and factory. Tests-disabled production requires the exact
live-provider type and factory while forbidding the fixture component type and
testing-grant factory. This is linkage evidence; independent construction,
grant, selection, and host-dispatch cuts prove the executable route, and the
empty production grant proves zero provider work. Missing binaries, missing
supported demangling tools, nonzero tools, failed demanglers, and malformed or
zero-defined symbol tables all fail closed. Only parsed defined records count;
`U`/`UND` references and anchors embedded in surrounding template text do not.
Windows declaration prefixes are accepted only through the first allowlisted
calling convention outside balanced template, parameter, and array delimiters;
nested conventions do not select the anchor boundary and unbalanced
declarations fail closed. Absolute, common, debug, and unknown symbol kinds or
sections do not count. CTest treats the verifier's exit status as authoritative.

## Deferred gates

RFC-BPR-0007's own text names no owner-attended requirement, no Windows-native
requirement, no GitHub mutation, no credential, and no network service. Its
non-goals exclude cross-machine synchronisation, so no remote surface is
involved and no safe test target is required.

The process-kill recovery criterion runs on this host. It kills a child process
this project starts; it needs no elevated privilege and no operator.

## Card allocation

| Card | Slice |
|---|---|
| BPR-0120 | This planning slice: verification plan, allocation, and card set |
| BPR-0121 | Core receipt and acceptance transitions against material identity |
| BPR-0122 | Core invalidation decisions from bounded material observations |
| BPR-0123 | Core actionability policy, deterministic ordering, and explanation |
| BPR-0124 | Ledger schema, strong identity scoping, and forward-only migrations |
| BPR-0125 | Host persistence request, typed completion, transactions, and WAL |
| BPR-0126 | Crash recovery and database-loss behaviour |
| BPR-0127 | Qt badges, filters, explanations, and queue projection |
| BPR-0128 | Composition-root wiring and public-path evidence |
| BPR-0129 | Documentation: schema, migrations, identity, privacy, retention, purge |
| BPR-0130 | Simplification and closeout preparation |

## Documentation ledger

| Scope | Required documents | Disposition |
|---|---|---|
| Schema, migrations, identity, receipt semantics, actionability, crash recovery, backups, retention, purge, privacy | [Review Ledger](../architecture/review-ledger.md) | Updated through schema version 3. Records the bounded note/head/defect vocabulary and identity, all prior migrations, exact retained anchors, note privacy, caps, deletion, and private-record SIGKILL recovery in addition to the receipt contract |
| Epoch catalog boundary | Git and epoch model | Updated. The existing statement that the ledger is a separate database now links the ledger document and records that the connection policy and runtime initialisation are literally shared rather than duplicated |
| Build and test | Build and test guide | Updated by BPR-0121 for the core suite; the storage, host, and Qt suites run under the standard presets and need no separate invocation |

**Backups and retention are documented as absent rather than described.**
Neither has been designed. Stating a policy that does not run would be worse
than stating none, so the document says plainly that the ledger grows and that
purge is file deletion.

## Closeout blockers

Discharged 2026-09-02. RFC-BPR-0002 through RFC-BPR-0005 are `Auto-Verified`,
BPR-0036 and BPR-0053 supplied the real Windows x64 evidence, and BPR-0130
made no closure claim as it said; BPR-0241 makes it.

## Closeout — 2026-09-02

Recorded on [BPR-0241](../issues/60-done/BPR-0241-close-rfc-0007.md) under
the light closeout contract. The automated rows were replayed at `6dd7240a`
with `ctest --preset dev --no-tests=error -R '^(core_review_ledger|application_review_ledger_host|sqlite_review_ledger_storage|qt_review_queue|application_review_ledger_composition_contract|composed_application_hosts)$' --output-on-failure`
— 6/6, `ninja: no work to do` beforehand — and the same three ledger
selectors passed on the WSL2 station at `8948d61a` under BPR-0472. The
final replay record, keyed by criterion:

| Criterion | Selectors | Result | Commit |
|---|---|---|---|
| Read, accepted, excluded, and seen states survive restart. | `application_review_ledger_host`, `sqlite_review_ledger_storage`, `application_review_ledger_composition_contract` | pass | `6dd7240a` |
| Changed material invalidates only affected receipts. | `core_review_ledger`, `application_review_ledger_host`, `sqlite_review_ledger_storage`, `application_review_ledger_composition_contract` | pass | `6dd7240a` |
| New or edited comment versions reopen the correct conversation. | `core_review_ledger`, `sqlite_review_ledger_storage`, `application_review_ledger_composition_contract` | pass | `6dd7240a` |
| Database deletion loses only private state and recreates a clean ledger without changing GitHub. | `sqlite_review_ledger_storage`, `application_review_ledger_host`, `application_review_ledger_composition_contract` | pass | `6dd7240a` |
| Abrupt termination and WAL recovery preserve committed state and discard incomplete transactions. | `sqlite_review_ledger_storage` | pass | `6dd7240a` |
| Migrations are forward-only, versioned, backed up where policy requires, and tested from every supported prior schema. | `sqlite_review_ledger_storage` | pass | `6dd7240a` |
| Strong identity prevents state reuse across host, account, repository, PR, epoch, and material boundaries. | `core_review_ledger`, `sqlite_review_ledger_storage` | pass | `6dd7240a` |
| `Next actionable` is deterministic and explains its ranking. | `core_review_ledger`, `application_review_ledger_host`, `application_review_ledger_composition_contract` | pass | `6dd7240a` |
| Receipt/actionability transitions are directly replay-tested without SQLite/Qt, while host persistence tests and Qt projection tests prove the complete request/completion/delta path. | `core_review_ledger`, `application_review_ledger_host`, `qt_review_queue`, `application_review_ledger_composition_contract` | pass | `6dd7240a` |

`composed_application_hosts` passed in the same replay, which is the standing
linkage audit above. RFC-BPR-0007 is `Auto-Verified`.
