# RFC-BPR-0007 Verification Plan and Ledger

RFC base: `ea3a3b8223c00f9e0999bf0ab2882496abf65843`

Status: implementation in progress. The core receipt policy has landed
(BPR-0121). RFC-BPR-0007 stays `Proposed`.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0003 and RFC-BPR-0005. Both are implemented and
verified on available targets, and both remain `Proposed` because closure is
strictly ordered and RFC-BPR-0002 still owes real Windows x64 evidence in
BPR-0036. Done BPR-0088 records the product owner's direction to advance the
implementation frontier on available targets. Closure order is unchanged: this
RFC cannot become `Auto-Verified` before RFC-BPR-0005 does.

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
| Receipt invalidation is precise | Core plus SQLite | Advance a fixture with mixed changed and unchanged material | Only receipts whose material identity changed are invalidated; unchanged ones survive the epoch advance | Planned |
| Read, accepted, excluded, and seen survive restart | Host persistence | Set each state, restart, re-read | Every state returns identical after a clean restart | Planned |
| New or edited comments reopen the right conversation | Core plus host | Add a comment, then edit one in place | Both reopen unread, and only the affected conversation | Core half verified. BPR-0121 proves an in-place edit reopens unread and outdates acceptance while carrying the same comment identity; the host half lands with BPR-0125 |
| Crash recovery is sound | Process-kill integration | Kill during a transaction and during a refresh, then restart | Committed state survives and incomplete transactions are discarded | Planned |
| Migrations are forward-only and complete | Host migration tests | Open a database at every supported prior schema | Each migrates to current and no migration is reversible | Planned |
| No cross-identity leak | Persistence integration | Reuse PR numbers and paths across hosts, accounts, and repositories | No receipt is visible outside the identity that wrote it | Planned |
| GitHub remains unchanged | Database-loss test | Delete the ledger and inspect an independent GitHub fixture | Only private state is lost and a clean ledger is recreated | Planned |
| Next actionable is deterministic and explains itself | Core | Rank a fixture queue twice and read the explanation | The order is identical and the explanation names the deciding facts | Planned |
| The ledger decision and effect split holds | Core, fake store, and Qt | Drive a receipt intent end to end | Intent transition, persistence request, typed completion, and UI delta are each separately observable | Planned |
| The core stays free of storage and presentation | Dependency scan | Scan the core target | No SQLite, Qt, filesystem, clock, thread, or credential identifier appears | Verified for the receipt policy. BPR-0121's transitions build and run under `core-only`, which links neither Qt nor SQLite; later slices re-assert it as they land |
| The ledger persists no presentation coordinate | Schema scan | Inspect every column of the created schema | No Qt index, row number, ribbon path, y-coordinate, or scroll position is stored | Planned |

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

## Closeout blockers

RFC-BPR-0007 cannot become `Auto-Verified` until RFC-BPR-0005 is, which
requires RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002, and therefore real
Windows x64 evidence in BPR-0036 and BPR-0053. BPR-0130 will make no closure
claim.
