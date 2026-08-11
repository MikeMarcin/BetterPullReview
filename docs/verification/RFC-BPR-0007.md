# RFC-BPR-0007 Verification Plan and Ledger

RFC base: `ea3a3b8223c00f9e0999bf0ab2882496abf65843`

Status: **the durable authority and selected-file product composition are
complete; broader queue and mutation workflows remain open.** BPR-0121 through BPR-0128 delivered file and conversation
receipts, actionability policy, Qt projection, and the first two schema
versions. BPR-0238 adds schema version 3 and bounded ports for private notes,
the existing receipt-owned exclusions, last-reviewed heads, and defect
metadata/history. BPR-0228 composes one `review_ledger_application_host` under
the exact accepted pull-request authority, routes receipt actions and private
records through it, and derives selected-file actionability from exact material
facts. Unknown material remains conservatively actionable. BPR-0229 through
BPR-0231 own the broader queue, mutation, and notification workflows.

RFC-BPR-0007 stays `Proposed`. No card here makes a closure claim.

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
| Receipt invalidation is precise | Core plus SQLite | Advance a fixture with mixed changed and unchanged material | Only receipts whose material identity changed are invalidated; unchanged ones survive the epoch advance | Core half verified. BPR-0122 asserts retention per file across a mixed advance rather than by count, and distinguishes an absent file from a changed one; the SQLite half lands with BPR-0125 |
| Read, accepted, excluded, and seen survive restart | Host persistence | Set each state, restart, re-read | Every state returns identical after a clean restart | Verified. BPR-0125 writes each of the four states to its own file identity in a real database, reopens, and compares byte for byte; a file never written reads back absent rather than defaulted |
| New or edited comments reopen the right conversation | Core plus host | Add a comment, then edit one in place | Both reopen unread, and only the affected conversation | Core half verified. BPR-0121 proves an in-place edit reopens unread and outdates acceptance while carrying the same comment identity; the host half lands with BPR-0125 |
| Crash recovery is sound | Process-kill integration | Kill during a transaction and during a refresh, then restart | Committed state survives and incomplete transactions are discarded | Transaction persistence verified. BPR-0126 SIGKILLs a child holding an open receipt transaction. BPR-0238 repeats the real kill with note, reviewed-head, and defect writes in one transaction: all committed values survive and all abandoned values disappear together. Both assert `WTERMSIG == SIGKILL`; the POSIX-only tests compile out on Windows. The composed refresh path remains BPR-0228 |
| Migrations are forward-only and complete | Host migration tests | Open a database at every supported prior schema | Each migrates to current and no migration is reversible | Verified through schema version 3. A real version-1 database with nonzero valid identities and legal acceptance state traverses 1→2→3; an exact version-2 fixture traverses 2→3 and retains both file and conversation receipt rows. A newer database is refused *without being written to*. Separate wrong-application v1 and v2 fixtures are refused before migration and preserve the whole database byte-for-byte, application/user/schema metadata, exact `sqlite_schema` definitions, the exact file receipt, and the v2 conversation receipt |
| No cross-identity leak | Persistence integration | Reuse PR numbers and paths across hosts, accounts, and repositories | No receipt is visible outside the identity that wrote it | Verified. BPR-0124 resolves pull request 7 on four identities differing in one field each to four distinct scopes, and BPR-0125 shows the same file identity written under one host carries no receipt under another |
| GitHub remains unchanged | Database-loss test | Delete the ledger and inspect an independent GitHub fixture | Only private state is lost and a clean ledger is recreated | Verified for store independence. BPR-0126 deletes the ledger with its `-wal` and `-shm`, recreates a clean one, and shows a second database beside it untouched; the test fails if the two ever share a file. Inspecting a live GitHub fixture belongs to BPR-0128's public path |
| Next actionable is deterministic and explains itself | Core | Rank a fixture queue twice and read the explanation | The order is identical and the explanation names the deciding facts | Verified. BPR-0123 ranks a fixture whose top reason is tied and whose supplied order the tie-break must overturn, and re-ranks the reversed input to the identical result; the reason is a core value the adapter renders rather than composes |
| The ledger decision and effect split holds | Core, fake store, Qt, and shipped composition | Drive a receipt intent end to end | Intent transition, persistence request, typed completion, and UI delta are each separately observable | Verified. BPR-0125's fake store and fake projection share one log, and BPR-0228 drives seven inverse receipt actions through the shipped `QAction` surface, requiring one command, durable completion, and changed projection per gesture |
| The core stays free of storage and presentation | Dependency scan | Scan the core target | No SQLite, Qt, filesystem, clock, thread, or credential identifier appears | Verified. The whole ledger policy — receipts (BPR-0121), carry-over (BPR-0122), and ranking (BPR-0123) — builds and runs under `core-only`, which links neither Qt nor SQLite, so the separation is proven by the build rather than by a scan |
| The application-owned store persists a receipt | Shipped action/host/store composition | Run `application_review_ledger_composition_contract` | A real selected-file gesture supplies exact material, receipt actions persist through the host, and a later process loads and carries the receipt forward without direct fixture-store access | Verified by BPR-0228. Seed and reopen use one private root; the seed proves filtering retains the selected rendered comparison and same-process changed material re-enters actionability, while the newer-head reopen proves a receipt load and one core-decided carry-over write. Notes, the prior reviewed head, and defect history remain exact. A source guard rejects direct fixture calls to the concrete store |
| Notes, exclusions, last-reviewed heads, and defect metadata persist | Host persistence, migration, and composition | `ctest --preset dev -R '^(application_review_ledger_host\|sqlite_review_ledger_storage\|application_review_ledger_composition_contract)$' --output-on-failure` | Every record survives clean/crash restart and cannot leak across host, account, repository, PR, epoch, or file/conversation identity | Verified. BPR-0238 supplies schema-v3 isolation, bounds, migration, conflict, and SIGKILL evidence. BPR-0228 persists note/head/defect records through the one composed host and reloads them in a second real process. Public defect mutation remains BPR-0230 |
| The ledger persists no presentation coordinate | Schema scan | Inspect every column of the created schema | No Qt index, row number, ribbon path, y-coordinate, or scroll position is stored | Verified. BPR-0124 enumerates every column of the created schema and matches each against a forbidden substring set, and names a column it did inspect so an empty schema cannot pass |

## Standing composed-host linkage audit

BPR-0248 requires the exact demangled
`bpr::application::review_ledger_application_host` constructor in both the
development-fixture and production binaries. This is linkage evidence only;
the receipt actions, persistence completion, restart, and projection assertions
in `application_review_ledger_composition_contract` remain the construction
and runtime evidence.

The review-mutation linkage inventory is now profile-specific. The
development-fixture profile requires the review-mutation coordinator
constructor, the exact test-only GitHub public-path fixture provider boundary,
and `bpr::application::reconcile`, while forbidding the live provider type and
factory. Tests-disabled production requires the exact
`bpr::adapters::github::live_review_mutation_provider` type and
`make_live_review_mutation_provider` factory, while forbidding the coordinator
constructor and all fixture/reconciliation symbols. This is linkage evidence:
the fixture executable marker and dormant-selection contract prove fixture
selection, and the production cut proves the live provider has no coordinator
and calls neither observation nor submission. Missing binaries, missing
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

RFC-BPR-0007 cannot become `Auto-Verified` until RFC-BPR-0005 is, which
requires RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002, and therefore real
Windows x64 evidence in BPR-0036 and BPR-0053. BPR-0130 will make no closure
claim.
