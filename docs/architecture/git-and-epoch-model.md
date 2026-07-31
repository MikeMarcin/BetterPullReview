# Git Cache and Review Epoch Implementation Architecture

This document specializes the
[system architecture](system-architecture.md) for RFC-BPR-0003. The governing
rule remains:

> **The core decides. The host performs. Qt presents.**

RFC-BPR-0003 adds local Git-object residency and private review history. It
does not create a second GitHub authentication path, operate on a developer
worktree, make cached content available without fresh authentication, compute
the RFC-BPR-0004 diff, or claim the final RFC-BPR-0005 workbench.

## Selected boundary

```text
accepted RFC-BPR-0002 pull-request revision
    exact authority, snapshot/generations, base/head, review order,
    and a revocable native-HTTPS account capability
        |
        v
dedicated epoch application host
    bounded catalog observations, Git/cache/catalog jobs, explicit time,
    staging, serialized completion, cancellation, recovery, projections
        |
        +--> dedicated deterministic epoch core
        |       cause/alias/selection policy, exact acceptance, deltas
        |
        +--> git_cache_port
        |       app-owned bare repository and installed-Git pack/object/ref engine
        |
        +--> bounded Git protocol-v2 client
        |           |
        |           +--> git_smart_transport_port
        |                    exact native-HTTPS discovery/upload-pack
        |
        +--> epoch_catalog_port
                SQLite catalog, journal, paging, and recovery
        |
        v
Qt epoch presentation
    stable rows, independent selectors, exact OIDs, accessibility
```

The epoch host is separate from the GitHub host and reducer. The GitHub host
publishes a typed application handoff only after an
`accepted_snapshot_delta`; the epoch path never observes a Qt projection as
an internal command bus.

The application-owned protocol-v2 client depends on the narrow smart-transport
port. The GitHub adapter implements that port with the same validated
native-HTTPS account authority used for API reads. The Git adapter receives
only an already completed, validated, bounded private spool request and typed
local object/ref commands. It never
receives credential bytes, a credential-bearing URL, a remote ref
advertisement, proxy configuration, or permission to open a network
connection. Installed Git remains a local pack/object/ref engine.

## Exact identity model

### Git object IDs

Commit and tree IDs are bounded, algorithm-tagged value types:

```text
git_hash_algorithm = sha1 | sha256
git_commit_oid = algorithm + exact 20-byte or 32-byte digest
git_tree_oid = algorithm + exact 20-byte or 32-byte digest
```

Input accepts exactly 40 or 64 hexadecimal characters, normalizes them to
lowercase bytes, rejects an all-zero digest, and infers no other algorithm.
Base, head, review, advertised, cache, and repository object-format facts must
agree. Presentation performs the only conversion back to complete lowercase
hexadecimal text.

An OID alone is not pull-request, account, or epoch authority.

### Pull-request authority and revision

The application handoff carries:

```text
application_session_id
host_profile_id + github_profile_revision
git_host_id + canonical hostname + server contract
github_identity_generation
github_account_id + exact viewer actor ID
repository_id + validated repository locator
pull_request_id + pull_request_number + pull_request_generation
provider_snapshot_id
validated base-branch identity
base git_commit_oid + head git_commit_oid
bounded newest-current-viewer review candidate with typed review order
revocable native-HTTPS account capability
```

The base-branch identity is a versioned fixed digest of the repository
identity and validated full `refs/heads/...` name. Raw branch text never
becomes a core identity or Git argument. The Git adapter receives the
separately validated ref-name value needed to check the remote advertisement.
`HeadRef` is display metadata only and never becomes fetch authority.

The account capability is opaque and revocable. GitHub identity invalidation
revokes it before clearing the accepted GitHub session. Revocation prevents
new smart requests and cancels in-flight requests; retaining a C++ handle
cannot extend authentication authority after the matching identity generation
ends. The GitHub host publishes a typed
`pull_request_authority_invalidated` application message before it clears that
authority. The epoch host consumes the accepted-revision and invalidation
messages directly; neither message is reconstructed from a projection or a
public snapshot accessor.

### Cache and epoch scope

Milestone one does not share a physical object database across accounts or
profile revisions. A repository cache partition is the exact tuple:

```text
profile + profile revision + host + account + repository + object format
```

The catalog allocates a 256-bit random `repository_cache_scope_id` from an
operating-system entropy port after authenticated authority is proven. The
directory name is the lowercase 64-character encoding of that opaque ID:

```text
repositories/v1/<repository-cache-scope-id>.git
```

The catalog separately allocates a 256-bit random
`pull_request_authority_scope_id` for the complete cache/PR authority. It
qualifies every ordinal, catalog row, retained ref, comparison, page cursor,
and deletion target. Neither scope ID is derived from raw provider text, and
the catalog enforces an exact unique authority mapping and collision retry.

Core identities are strongly typed and nonzero:

```text
epoch_catalog_incarnation_id     random identity of one catalog lifetime
review_epoch_id                  PR authority + incarnation + ordinal
epoch_ordinal                    monotonic inside PR authority scope
epoch_catalog_revision           monotonic inside PR authority scope
epoch_request_id                 feature-qualified asynchronous request
epoch_generation                 active-authority generation
comparison_generation            resolved-comparison generation
epoch_operation_id               exact durable operation
material_equivalence_id          opaque equality fact
```

The catalog incarnation is a nonzero random 256-bit value created with the
catalog. Deleting and recreating a catalog creates a different incarnation,
so a stale ID cannot collide with a reused authority or ordinal.

Ordinals and catalog revisions are in `0..INT64_MAX`, with committed ordinals
starting at one. Review order is in `1..10,000`. Observed UTC milliseconds,
when present, are in `0..253402300799999`. Every increment uses a checked
helper. Overflow fails closed before state, storage, ref, or projection
mutation; an aborted or recovered reserved ordinal is never reused.
`review_epoch_id` is never used without its authority and catalog
incarnation.

## Compact core state

The dedicated epoch core retains only:

```text
application session
optional active PR authority and accepted revision identity
epoch generation and next epoch request
accepted catalog revision
optional pending prepare/commit/prune/delete context
selection mode: named alias or exact explicit pair
resolved left/right anchors and availability
comparison generation
compact commands, status, and diagnostics
```

It does not retain catalog pages, review collections, Git paths, refs,
objects, packs, blobs, SQLite rows, timestamps used for scheduling, transport
handles, or presentation strings.

The production type has an enforced upper bound:

```cpp
static_assert(sizeof(epoch_state) <= 2048);
```

A comparison endpoint is exact:

```text
comparison_anchor
    authority scope
    catalog incarnation
    kind: epoch_base | epoch_head | review_commit
    exact epoch or review-association identity
    git_commit_oid
    availability
```

The friendly `Branch base` label resolves to the latest live epoch's accepted
base-tip anchor. It is not a computed merge base. A review anchor remains
semantically distinct from an epoch head even when both resolve to the same
OID.

## Typed transition path

### Intents

```text
select_named_comparison_intent(alias)
select_comparison_anchors_intent(left, right, expected catalog revision)
request_epoch_history_page_intent(cursor, page limit)
prune_review_epochs_intent(exact epoch IDs, expected catalog revision)
delete_repository_cache_intent(exact cache scope, expected revisions)
```

Prune and delete are explicit destructive commands. A UI confirmation is
presentation mechanics; the exact target, legality, and expected revision are
semantic.

### Bounded observations

```text
accepted_pull_request_revision_observation
epoch_preparation_observation
    exact request/authority/revision context
    latest live epoch summary
    preceding distinct-head live epoch summary
    base/head equality and validated lineage
    material relation
    exact latest-current-viewer review anchor or absence
    expected catalog revision and proposed ordinal
epoch_selection_observation
    summaries only for requested anchors and named-alias candidates
epoch_destructive_operation_observation
    exact affected IDs/counts and expected revisions
```

The full history remains host/catalog-owned and paged. The core receives no
unbounded vector.

### Requests

```text
prepare_pull_request_epoch_request
commit_epoch_catalog_request
    append_epoch | associate_review
discard_epoch_preparation_request
load_epoch_history_page_request
resolve_comparison_request
prune_retained_epochs_request
delete_repository_cache_request
cancel_epoch_work_request
```

Preparation may fetch objects, stage refs, read catalog facts, obtain explicit
observed time, and compute ancestry/tree facts. Only a matching core commit
request may promote prepared state to retained refs and a committed catalog
revision. An `associate_review` mutation carries no Git/ref promotion and uses
only an expected-revision SQLite transaction.

### Completion events

```text
pull_request_revision_accepted_event
pull_request_revision_invalidated_event
epoch_catalog_loaded | epoch_catalog_load_failed
    | epoch_catalog_load_cancelled
epoch_revision_prepared | epoch_revision_prepare_failed
    | epoch_revision_prepare_cancelled
epoch_catalog_committed | epoch_catalog_commit_failed
    | epoch_catalog_commit_cancelled
comparison_resolved | comparison_resolve_failed
    | comparison_resolve_cancelled
maintenance_completed | maintenance_failed | maintenance_cancelled
```

The catalog-load algebra covers both the initial window and a requested page.
The catalog-commit algebra identifies `append_epoch` versus
`associate_review`; the maintenance algebra identifies prune versus cache
delete. Every terminal carries the complete application/profile/host/account/
repository/PR/snapshot/generation/request/catalog context applicable at
submission. Duplicate, stale, mismatched, malformed, and overflow terminals
produce no accepted durable or visible state.

### Deltas

```text
accepted_epoch_delta
epoch_catalog_delta
    optional catalog identity; absence explicitly clears catalog projection
    inserted, removed, and availability-changed stable IDs
comparison_selection_delta
    semantic selection
comparison_resolution_delta
    semantic selection plus optional exact resolved anchors/OIDs
epoch_command_delta
epoch_status_delta
epoch_diagnostic_delta
```

Authority loss, authority switch, and catalog-conflict reload publish explicit
catalog, semantic-selection, and resolved-comparison clears before any
replacement catalog can become visible. Catalog incarnation is therefore a
hard boundary: a one-sided selection cannot combine a newly loaded anchor
with an accepted pair from an earlier incarnation.

Selecting a different resolved anchor pair, or changing availability of the
selected pair, advances `comparison_generation` and cancels dependent work.
Selecting the same exact pair is idempotent.

## Epoch facts and cause policy

An epoch stores exact base/head OIDs, base-branch identity, ordinal, observed
time, origin, independent base/head transition facts, primary display cause,
material relation, availability, and provider/review association.
`observed_time` is explicit host input for display and diagnostics only.
Ordinal and catalog revision define ordering.

The fact model is composite:

```text
origin:
    initial_observation | live_observation | explicit_historical_import

base transition:
    not_applicable | unchanged | base_oid_changed | base_branch_changed

head transition:
    not_applicable | unchanged | descendant | non_fast_forward
    | lineage_unknown

material relation:
    same | different | unknown
```

The core derives one primary label without discarding simultaneous facts:

1. first live observation;
2. explicit historical import;
3. base-branch change;
4. proven rewritten/non-fast-forward head (`Force-push/rebase`);
5. proven descendant head (`Author upload`);
6. changed head with unavailable lineage (`Head change; lineage unavailable`);
7. head-stable base change (`Base update`);
8. no base, head, or branch change: no new epoch.

When base and head both change, the head label applies unless the branch
identity changed; the independent base transition remains visible. Unknown
lineage is never guessed as an author upload or force-push. It still creates
an honest retained epoch so an observed head is not discarded.

Material equality never suppresses an epoch whose live revision changed. The
host may derive an opaque material-equivalence fact only from the ordered
exact base-tree OID, head-tree OID, and comparison-policy revision. Unknown
tree identity is never equal. RFC-BPR-0007 may later use that fact when
deciding receipt reuse; RFC-BPR-0003 does not migrate receipts, and RFC-BPR-0004
does not receive reuse authority from equality alone.

An explicit historical import is legal only when its exact base, head,
branch identity, and provenance are known. A GitHub review record supplies a
review commit but not the exact base viewed at submission, so it creates a
review anchor or association, not an invented historical epoch.
Historical imports are selectable exact history, but their insertion ordinal
is not evidence of observed live-tip chronology. They never become `Latest`
or the candidate for `Previous upload`.

## Review ordering and `My last reviewed`

GitHub's complete REST review collection is chronological. The RFC-BPR-0002
normalizer therefore assigns each accepted review a bounded 1-based
`github_review_order` after all pages have been validated and concatenated.
The order is a provider fact, not wall-clock inference. Only after the
complete collection is accepted does the GitHub host derive the one bounded
current-viewer candidate for the epoch handoff.

The latest current-viewer review is the highest review order whose exact actor
matches the authenticated viewer, whose state is not `pending`, and whose
commit OID is present. The newest matching record remains authoritative even
when its object was never retained; in that case `My last reviewed` is shown
as unavailable. The application never silently falls back to an older,
resident review. Submitted time may be retained for display but is not the
ordering key.

Review association is idempotent by exact authority plus review identity. If
the review commit equals an existing epoch head, the association points at
that epoch. Otherwise it remains a distinct review anchor and becomes usable
only when the exact commit object is resident.

A same-base/head/branch accepted snapshot creates no epoch, but a new or
changed newest-current-viewer review fact still requires durable catalog
progress. The core emits an `associate_review` catalog mutation containing the
exact accepted review identity/order/OID, availability observation, catalog
incarnation, and expected revision. One SQLite compare-and-swap transaction
inserts or updates that association and advances the catalog revision once;
it touches no Git object or ref. An identical association is an idempotent
no-op. Its accepted terminal re-resolves semantic review aliases just like an
epoch append, and a crash-recovered catalog commit is visible on the next
exact-authority load even though the former application session receives no
terminal.

## Comparison policy

The default selection mode is the semantic `Whole PR` alias:

```text
left  = latest live epoch's base anchor
right = latest live epoch's head anchor
```

Named aliases are:

- `Whole PR`;
- `Previous upload -> latest`: the nearest earlier live epoch with a head OID
  different from the latest live epoch, including a force-push/rebase epoch;
- `My last reviewed -> latest`: the exact newest current-viewer review commit
  to the latest live head.

Historical imports and base-only epochs do not displace the preceding
distinct-head rule. Alias resolution first chooses the semantic candidate and
then checks its availability. If that exact previous-upload or newest-review
candidate is pruned, cache-deleted, corrupt, or never observed, the alias is
unavailable; it never skips to an older resident candidate.

A named selection stays semantic and re-resolves when the accepted review
facts or catalog revision change. An explicit pair stays on the exact anchor
IDs/OIDs selected; if an object is pruned or a cache is deleted, it becomes
visibly unavailable and never jumps to another epoch. Changing one side of a
resolved alias converts the whole selection to an explicit pair using the
other last-resolved exact anchor.

Any two resident anchors in the same exact PR authority are legal, including
reverse and identical comparisons. Identical OIDs represent an empty
comparison. Cross-profile, cross-account, cross-repository, cross-PR,
cross-incarnation, mixed-object-format, mismatched-catalog, and unavailable
pairs are rejected with reason-coded diagnostics.

## App-owned bare repository

Every cache is created beneath the canonical application-data root with:

- one immutable versioned identity manifest;
- exact private owner permissions;
- a cross-process, exact-cache writer lock;
- `core.bare=true`, `core.logAllRefUpdates=false`, the expected object format,
  and no remote;
- app-owned empty system/global configuration and template directories;
- no alternates, grafts, shallow state, worktree state, replace refs, hooks,
  maintenance, or automatic garbage collection.

Before every use, the adapter revalidates the root and every ancestor/child
identity, rejects symlinks and platform reparse/junction escapes, verifies the
manifest and exact Git configuration, and checks repository/object format.
Validation and use occur while the exact cache writer lock is held. Unrelated
cache scopes may run concurrently.

Only two advertised refs are eligible for a live observation:

```text
refs/heads/<validated base ref>
refs/pull/<pull request number>/head
```

Both advertised OIDs must equal the already accepted provider snapshot. The
adapter never adds a remote, fetches a wildcard or tag, accepts a provider
clone URL, or uses `HeadRef` as a refspec.

Request-scoped staging refs and authority-qualified final refs are:

```text
refs/betterpullreview/v1/stage/<64-lowercase-hex-operation-id>/base
refs/betterpullreview/v1/stage/<64-lowercase-hex-operation-id>/head

refs/betterpullreview/v1/scope/<64-lowercase-hex-pr-scope-id>/
    epoch/<16-lowercase-hex-ordinal>/base
refs/betterpullreview/v1/scope/<64-lowercase-hex-pr-scope-id>/
    epoch/<16-lowercase-hex-ordinal>/head
```

The line wrapping above is explanatory; each real ref is one continuous
string. The ordinal encoding is fixed-width, nonzero, and no greater than
`INT64_MAX`. Ref names are constructed only by these typed versioned
constructors. Catalog text, provider text, arbitrary OIDs, and caller-supplied
ref text cannot become a ref argument. Ref creation and deletion use
`git update-ref --stdin -z --no-deref` start/prepare/commit transactions with
exact expected old values and no reflogs; no glob reaches Git.

## Native smart transport and credential-free pack ingestion

The public implementation requires Git protocol v2. The GitHub adapter
derives the only two routes from validated authority and repository locator:

```text
GET  /<owner>/<repository>.git/info/refs?service=git-upload-pack
POST /<owner>/<repository>.git/git-upload-pack
```

GitHub.com uses the exact `github.com` Git origin. GHES uses the exact
validated enterprise hostname. The transport sends the protocol-v2 header,
requires the exact advertisement/request/result media types, uses the
RFC-BPR-0002 native credential and system trust/proxy policy, disables cookies
and caching, and rejects every redirect. The mandatory discovery query is an
original validated request, not a redirected target.

The current JSON transport and its 4 MiB body ceiling are unchanged. A
separate streaming smart-transport implementation bounds discovery, packet
lines, request rounds, request bytes, response/pack bytes, headers, idle and
total duration, disk growth, and cancellation while applying backpressure.
It emits fixed typed failures and never retains raw server, TLS, proxy, Git,
path, or credential text in diagnostics.

The bounded application protocol client validates the discovery capability
advertisement, then constructs—not rewrites—an exact protocol-v2 `ls-refs`
request with only these two arguments:

```text
ref-prefix refs/heads/<validated accepted base branch>
ref-prefix refs/pull/<validated pull-request number>/head
```

The returned advertisement must contain exactly one result for each prefix,
no result outside them, and OIDs exactly equal to the already accepted API
revision. Only then may a fetch request proceed. Its `want` set is the
deduplicated accepted base/head OID set; its bounded `have` set may expose
only catalog-visible, object-verified tips from the same
profile-revision/account/repository cache partition. The request terminates
negotiation explicitly with `done`; it never permits an arbitrary cached or
review OID to become a `want`.

The client uses only the base `ls-refs` and `fetch` commands plus the exact
repository object-format declaration. A bounded, syntactically valid
advertisement may name known GitHub/GHES features such as `shallow`,
`wait-for-done`, `filter`, or `server-option`; unused advertised features are
ignored, not treated as exercised behavior. The client never requests,
echoes, or exercises bundle URIs, `packfile-uris`, object filters, shallow
history, `want-ref`, `sideband-all`, server options, session identifiers,
thin packs, tags, or any capability that introduces another retrieval or
execution path. Any unrequested feature in a request or response fails closed.
The fetch response must contain one well-formed `packfile` section and no
unrequested section. Sideband 1 is streamed with backpressure into the exact
private request-owned spool
`incoming/v1/<64-lowercase-hex-operation-id>.pack.part`; bounded sideband 2 is
discarded; sideband 3 is a fixed typed failure. Packet counts, payloads,
progress, and complete response bytes are bounded before disk admission.

The host validates the pack header, repository-format trailer checksum, and
operation byte limit; durably renames the completed spool; and records the
exact pack identity, a bounded pre-import pack-directory inventory, and
whether that final pack already existed in the typed catalog operation. Only
then is the validated spool streamed to installed Git. The fixed ingestion
command is equivalent to:

```text
git -c core.fsync=all
    -c core.fsyncMethod=fsync
    --git-dir=<validated bare cache>
    index-pack
    --stdin
    --strict
    --check-self-contained-and-connected
    --max-input-size=<fixed-decimal-operation-bound>
    --threads=1
    --no-rev-index
```

The client never requests a thin pack, so `--fix-thin` is absent. `--keep`,
`fetch-pack`, `unpack-objects`, and loose-object ingestion are forbidden. The
exclusive cache lock and disabled repack/maintenance/GC eliminate the race
for which `index-pack --keep` is intended. Standard output must be exactly one
validated `pack` result for the repository's object format; standard error is
bounded and reduced to fixed diagnostics. The host records the exact pack
identity agreement and immediately creates the paired staging refs after
independently verifying the accepted commits and object closure. It then
exact-deletes the request spool. A crash cannot create an unowned final pack:
the durable operation already identifies the expected pack and preexisting
state before `index-pack` starts.

The adapter performs a fixed version/capability probe and supports Git 2.39 or
newer only when this exact `index-pack`, object-inspection, and ref-transaction
contract passes on a sanitized fixture. A version string alone is not
evidence. It resolves and binds an absolute executable and matching
`git --exec-path`, launches no shell, constructs the child environment from
an empty set plus fixed platform/runtime and app-owned Git variables, and
supplies an app-owned empty home/config/template root. Network, proxy,
credential-helper, askpass, SSH, URL-rewrite, alternate-object, replace-ref,
trace, editor, pager, locale, dynamic-loader, and arbitrary parent variables
are absent.

The same fixed `core.fsync=all` and `core.fsyncMethod=fsync` policy applies to
every object and ref mutation. The probe directly proves it for received
pack/index data and refs. The adapter never relies on a platform or Git
default such as macOS writeout-only flushing. A Git build for which this
durability cannot be configured and directly exercised is unsupported, and
no catalog-visible epoch may be committed with it.

No command performs checkout, merge, submodule, LFS, attribute filter, hook,
textconv, external diff, worktree, maintenance, or automatic GC behavior.
Later ancestry, object, and tree checks use only fixed verbs and validated
OIDs under the same repository and environment policy.

The implemented process worker serializes calls. Unix children enter a new
session/process group; guarded Windows code assigns children to a Job Object
and closes inherited handles, with target-native execution retained in
BPR-0053. Input, retained output, time, cancellation, descendant termination,
and two-stage reaping are bounded. If the adapter still cannot prove a child
reaped after both stages, it permanently quarantines the lane and deliberately
retains the process wrapper instead of invoking a potentially unbounded Qt
destructor wait. It does not spawn an unbounded background reaper or admit
later work onto the lane, and the application must not free repository or
transport resources underneath that ownership. BPR-0045 owns final composed
lifecycle ordering. No privileged helper, daemon, service, listener, or
weakened platform-security setting is introduced.

BPR-0042 implements only this credential-free local process boundary and the
exact secure consumption of
`incoming/v1/<64-lowercase-hex-operation-id>.pack.part`. BPR-0043 owns the
app-owned protocol-v2/native-HTTPS path, cache creation and locking, spool
creation and completed-pack validation, pre-import pack inventory, and
retained refs. BPR-0044 owns the durable operation record containing the
spool identity/size, expected pack, inventory, and preexisting-pack fact.
BPR-0045 must persist that record before invoking `index-pack` and owns the
remaining import/ref/catalog/recovery sequence. Passing the local
BPR-0042 contract is not evidence that those later surfaces exist.

## Catalog and recoverable commit protocol

RFC-BPR-0003 introduces the versioned app-owned SQLite database:

```text
state/v1/epoch-catalog.sqlite3
```

It is the dedicated epoch catalog. RFC-BPR-0007's private review ledger has a
separate database, schema version, and lifecycle authority; the two features
may reuse the narrow SQLite adapter library but do not add tables to each
other's database.

The catalog has `application_id = 0x42504543` (`BPEC`),
`user_version = 1`, and a nonzero random 256-bit catalog incarnation. The
initial schema uses typed `STRICT` tables, with `WITHOUT ROWID` where the
composite primary key is the row identity, for:

- the singleton catalog incarnation and schema facts;
- repository cache scopes bound one-to-one to the exact
  profile-revision/host/account/repository/object-format manifest;
- PR authority scopes, checked next ordinal, and catalog revisions;
- epochs and exact base/head retained-ref identities;
- review anchors/associations and chronological review order;
- explicit availability and deletion state;
- typed operation and pending-detail tables for append, prune, and cache
  deletion.

OIDs are stored as algorithm plus binary digest. IDs, enums, counts, times,
and fixed digests use typed integer/blob columns. The database contains no
credential, source body, diff, comment/review body, login, hostname, owner,
repository name, URL, raw process output, or raw provider error.
There are no generic JSON payload rows, views, triggers, caller-provided SQL,
or `ATTACH`.

The adapter is built against one repository-pinned SQLite amalgamation with
no silent system-library fallback; BPR-0044 records the selected release and
source hash when the dependency is added. Extension loading and double-quoted
string compatibility are omitted. Defensive mode and foreign keys are on;
trusted schema is off. One serialized writer uses prepared allowlisted SQL,
bounded read transactions, WAL, and `synchronous=FULL`. Catalog and parent
directory creation, WAL checkpoints needed by explicit lifecycle operations,
and rename/delete operations use directly verified platform durability.
Private owner permissions and no-follow/reparse validation gate every open.
Schema/version mismatch, integrity failure, unexpected schema objects, or an
unexpected path/permission state makes the catalog unavailable rather than
recreating over evidence.

Git refs and SQLite cannot share one physical transaction, so publication
uses SQLite as the durable coordinator and the catalog as BetterPullReview's
visibility authority:

1. Under the exact cache writer lock, `BEGIN IMMEDIATE` verifies the authority,
   catalog incarnation/revision, and quota; reserves a never-reused ordinal;
   and records a typed `preparing` operation with exact generated staging and
   final refs. The SQLite transaction commits before network work.
2. Stream the bounded response into the request-owned spool. After validating
   its pack identity, a short catalog transaction records that identity, the
   bounded pre-import pack inventory, and preexisting-pack fact before
   `index-pack` starts.
3. Import and verify the exact accepted objects, create the paired staging
   refs with one expected-value ref transaction and the selected full
   durability policy, then exact-delete the spool.
4. Return only bounded lineage, tree, alias, and catalog facts to the
   serialized decision lane.
5. After an exact core commit authorization, a catalog transaction rechecks
   every generation/revision and records the complete hidden candidate as
   `commit_authorized`. Before `ref_commit_in_progress`, cancellation may
   abort the hidden candidate and exact staging state.
6. The writer durably records `ref_commit_in_progress`, then one
   `git update-ref --stdin -z` transaction verifies both staging OIDs and both
   absent final refs, creates the paired final refs, and deletes the paired
   staging refs. From this phase onward cancellation cannot report a false
   cancellation; recovery must resolve the operation.
7. A catalog transaction checks host-supplied exact ref/object inspection,
   inserts the visible epoch and review associations, advances the catalog
   revision exactly once, and marks cleanup. Its commit is the durable-success
   and visibility linearization point. A reserved ordinal is never reassigned
   even if the candidate aborts.
8. Only the matching committed terminal may update core selection and publish
   Qt deltas.

An association-only mutation skips the spool, Git, and ref phases. One
`BEGIN IMMEDIATE` transaction checks authority/incarnation/revision, applies
the idempotent review association, advances the revision only for a change,
and becomes its visibility/durable-success point.

Git ref transactions lock all queued updates, but a raw concurrent reader may
observe only part of a transaction. Every BetterPullReview reader therefore
uses the exact repository lock and catalog visibility gate. Refs alone never
authorize presentation or downstream comparison.

Recovery holds the same writer lock and uses only typed operation state:

- an exact incomplete or complete request spool is resumed or exact-deleted
  only through its matching operation;
- an interrupted import compares the recorded bounded pack inventory and
  expected pack identity. It may resume, or exact-delete only a nonpreexisting
  pack/index and import-temporary files created by that operation when no
  staging/final/catalog ref can reach them;
- `preparing` or `commit_authorized`, no final refs, and the exact staging
  pair: either resume or exact-abort according to the recorded phase and
  cancellation decision;
- `ref_commit_in_progress`, no final refs, and the exact staging pair: retry
  the fixed ref transaction and finalize;
- the exact final pair: verify objects and finalize the catalog exactly once;
- a committed catalog row with exact staging residue: exact-clean the residue;
- a mixed, missing, unexpected, or mismatched final/staging/object state:
  mark `repair_required`, make the affected scope unavailable, and never
  guess;
- a committed epoch with missing or mismatched refs: retain its identity and
  tombstone it as corrupt; never silently refetch mutable history.

An unreferenced, well-formed staging pair may be removed under the lock because
it was never retained history. A preexisting pack, an unrecorded complete
pack, or any object whose operation ownership/reachability is uncertain is
never deleted; ambiguity becomes `repair_required` or remains quota-counted
until explicit whole-cache deletion. Recovery never deletes any other ref or
object.

Prune uses a typed pending intent with exact authority, incarnation, ordinal,
OIDs, and expected revision; blocks new comparison work for that target;
deletes the paired refs with exact expected values; and then finalizes a
`pruned` tombstone and one catalog-revision advance. The row and exact anchor
identity remain. Cache deletion enumerates every PR scope in the one
account/profile cache partition, records that exact set durably, takes the
exclusive cache lock, atomically renames the validated repository to a
transaction-named sibling quarantine path, finalizes `cache_deleted`
tombstones and revisions, and then exact-deletes the quarantine directory.
Failed physical cleanup remains a retryable tombstone and is never reopened.
Secure physical erasure from WAL/journal files or SSD media is not claimed.

## Retention, quotas, and deletion

Private refs, not remote reachability, root every retained epoch. There is no
automatic epoch pruning or Git GC. Validated typed limits cover:

- repository and global cache bytes;
- pack/object growth per operation;
- retained epochs per PR and catalog rows per page;
- request/response/packet/output bytes;
- child and network time;
- concurrent cache writers and pending operations.

A hard limit fails before publication, cleans only request-scoped staging
and provably owned import residue, and preserves all committed history. The UI
names the exhausted limit. Epoch pruning can free the logical retained-epoch
limit, but because milestone one runs no automatic Git GC, it does not claim
immediate physical-byte reclamation. Exact whole-cache deletion is the only
operation guaranteed to reclaim repository bytes.

Pruning requires exact epoch IDs and expected catalog revision. Repository
cache deletion requires the exact authenticated cache scope and reports its
affected PR/epoch counts before confirmation. Deleting one account/profile
partition cannot touch another partition. Unavailable historical rows remain
honest metadata until explicitly removed; they never remap to a surviving
OID.

## Catalog paging and acceptance

History paging is revision-bound:

```text
authority scope + catalog incarnation + expected catalog revision
before_ordinal exclusive
limit in 1..100
order by ordinal descending
```

Every page repeats the exact revision and returns an opaque continuation. A
revision mismatch invalidates the page and requires refresh; the host and Qt
model never merge pages from different revisions. Core retains only the
active pair, one latest live summary, one preceding distinct-head candidate,
one current-viewer review candidate, and bounded pending contexts. Host owns
pages; Qt owns only loaded projection rows.

Every asynchronous context includes the exact application session,
profile/revision, host/account/repository/PR authority, source snapshot and
GitHub generations, epoch request/generation, catalog incarnation/revision,
and operation or selection identity. A completion with no matching pending
request or from an older generation is `stale_completion`. The same
request/generation with any other field different is
`completion_identity_mismatch`. Neither rejection clears current work or
emits content, catalog, availability, or selection deltas.
Duplicate terminals are stale. Catalog compare-and-swap conflict commits
nothing and requests a fresh bounded observation.

## Concurrency and lifecycle

- The application/GUI lane serializes core transitions and accepted
  projections.
- A storage actor serializes journal/ref/catalog mutations for one cache and
  preserves the decision-lane enqueue order. Unrelated caches run in
  parallel.
- Network and Git preparation run off the serialized lane with immutable
  inputs and explicit cancellation.
- Identity switching advances the epoch generation before cancellation and
  clears active comparison state. Earlier terminals cannot publish.
- Same-authority refresh retains the last accepted exact pair marked
  refreshing. Failure or cancellation does not replace it; a newly requested
  unresolved pair never authorizes downstream diff work.
- Cancellation is advisory and each normally accepted in-process submission
  during the live host lifetime produces one terminal. Abrupt process death
  instead leaves one typed durable operation outcome for recovery and makes
  no terminal-delivery promise to the dead session. Before
  `ref_commit_in_progress` work may cancel with no committed epoch. That phase
  is the point of no cancellation, not proof of success: recovery resolves
  success or a typed repair-required failure and never reports a false
  cancellation. Once the catalog visibility transaction commits, success is
  mandatory.
- A durable result that becomes stale after identity switching remains valid
  only in its old authority catalog and is never published into the new
  scope. A later exact-authority load may reveal it.
- Shutdown closes Qt and epoch ingress; the GitHub host invalidates account
  authority and revokes every smart-HTTPS lease; the epoch host then cancels
  and drains transport/Git/catalog jobs while leaving every durable operation
  finalized or recovery-safe. Local Git/SQLite handles close next, exact
  credential cleanup completes while its vault remains alive, smart/native
  transport and provider/vault teardown follow, and the serialized decision
  lane is destroyed last.

## Qt projection

The application publishes provider/Git/SQLite-free records:

```text
epoch_row_projection
    stable authority/epoch ID, ordinal, primary/secondary cause facts,
    observed time, exact base/head OIDs, availability

comparison_projection
    selection mode, exact left/right anchor IDs and OIDs,
    alias/reason, availability, comparison generation

epoch_catalog_changed_projection
    inserted/removed/changed stable IDs and catalog revision

epoch_status/command/diagnostic projections
```

History is paged with revision-bound cursors. Routine append, prune,
availability change, and selection apply exact insert/remove/data-change
signals; they do not reset the model or rebuild all rows. A complete authority
replacement may replace the model, while persistent indexes from an old
authority are never reused.

The reusable RFC-BPR-0003 control contains two independent selectors, named
comparison actions, exact non-elided/copyable OID fields, explicit unavailable
states, keyboard operation, visible native focus, accessible relationships,
and one bounded deduplicated announcement. Qt dispatches typed IDs/aliases and
never compares OIDs, resolves aliases, classifies causes, queries SQLite,
reads the cache, or launches Git.

The control inherits the current system palette and native style. It adds no
custom theme or final-workbench visual claim. RFC-BPR-0005 retains its
distinct integrated-workbench presentation-polish and native
visual-acceptance slice. RFC-BPR-0006 owns selectable appearance, fonts, and
syntax presentation, with fresh installation defaulting to System.

## Platform and no-impact boundaries

macOS and Linux require target-native process, native-HTTPS, cache/catalog,
recovery, Qt, runtime, and dependency evidence. Windows source and contracts
are implemented now; real Windows Git, Job Object, DACL/junction, Credential
Manager, cache/catalog, rendering, runtime, and cleanup evidence remains
BPR-0053. That open hardware task does not block available-platform
implementation or later RFC work, but it blocks affected Windows claims and
RFC closeout.

RFC-BPR-0003 does not add:

- GitHub mutations;
- diff computation or line mapping;
- receipt/actionability policy;
- final workbench layout or selectable themes;
- AI, queue, observer, or notification behavior;
- positive GitHub CLI enablement;
- QML, WebEngine, browser, listener, plugin, telemetry, or updater surfaces;
- package formats, signing, notarization, or installers;
- GitHub Actions build, test, package, release, or verification workflows.
