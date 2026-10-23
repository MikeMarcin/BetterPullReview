# Git Cache and Review Epoch Implementation Architecture

This document specializes the
[system architecture](system-architecture.md) for RFC-BPR-0003. The governing
rule remains:

> **The core decides. The host performs. Qt presents.**

RFC-BPR-0003 adds local Git-object residency and private review history. It
does not create a second GitHub authentication path, operate on a developer
worktree, make authenticated cached content available without fresh authentication, compute
the RFC-BPR-0004 diff, or claim the final RFC-BPR-0005 workbench.

## Selected boundary

```text
accepted_pull_request_handoff from RFC-BPR-0002
    exact authority, snapshot/generations, base/head, review order,
    and a revocable native-HTTPS account or anonymous-public capability;
    no source-observation identity or observed time
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
an internal command bus. The host finishes exact append recovery before it
mints and collision-probes a source-observation identity or submits the
accepted revision to the epoch core. It obtains UTC only when a preparation
requires an observed-time fact.

The production runtime ports have no ambient configuration. They obtain each
256-bit source/operation identity from the operating-system CSPRNG
(`getentropy` on macOS, `getrandom` on Linux, and
`BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG)` on Windows) and obtain time
from the bounded system UTC clock. An all-zero or failed entropy result and an
out-of-domain clock result fail closed. A source- or operation-identity
collision consumes a fresh sample, with at most 32 attempts for each bounded
retry loop. Windows source/compile evidence is not target-native execution.

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

The provider-free `accepted_pull_request_handoff` carries:

```text
application_session_id
host_profile_id + github_profile_revision
git_host_id + host authority fingerprint + canonical hostname + server contract
github_identity_generation
github_account_id + exact viewer actor ID
repository_id + validated repository locator
pull_request_id + pull_request_number + pull_request_generation
provider_snapshot_id + typed revision origin
git object format
validated base-branch identity + validated typed base ref
base git_commit_oid + head git_commit_oid
bounded newest-current-viewer review candidate with typed review order
revocable native-HTTPS account capability
```

It deliberately carries neither the core's `pull_request_source_revision` nor
observed UTC. Those host-owned observations cannot be supplied by the GitHub
host, a provider record, or a Qt projection.

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
authority. The epoch host consumes the accepted handoff and invalidation
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
epoch_occurrence_id              digest of canonical host + numeric PR + typed update evidence
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

The occurrence id is a separate portable name. A numeric GitHub force-push
event id and its after-head OID distinguish repeated A→B→A heads; a live
head transition without an event has typed observed-head evidence. Neither
local incarnation nor ordinal enters the occurrence digest. The SQLite v6
sidecar and append journal retain occurrences and their optional local epoch associations while
preserving every legacy epoch row, key, incarnation, and ordinal. A late event
can supersede an observed-head occurrence only through an exact durable
compare-and-swap; old evidence is archived. Provider event order and local
observation order are separate facts, so an uncertain association remains
ambiguous rather than being inferred from equal heads or mixed clocks. A
head-only historical occurrence starts without an epoch anchor or presumed
original base. A retained import binds the exact event to a local epoch in the
same transaction as the epoch append and labels its derived comparison base
separately from the unknown original base.

Active imported comparison associations belong to the latest live base
selection. The core identifies a change to its base OID or branch identity;
the catalogue atomically unlinks the affected occurrence associations when
it commits that live epoch. Every old epoch, anchor and retained ref remains
intact, so explicit comparisons and ledger, window, MCP and agent bindings
continue to resolve their original material. The host presents affected
occurrences as comparison unavailable while the new live append is pending,
including when it fails. Without an epoch-origin lookup, this pending projection
conservatively masks any force-push event link with an unknown historical base,
even when its derived base is absent; it preserves observed-head links. An
ambiguous event linked to a live epoch can therefore be briefly unavailable
until the host reloads the exact durable associations. The bounded historical
import path can then create
a new comparison epoch under the new base selection. A missing or non-unique
merge base leaves the occurrence unavailable and its historical base unknown.

Generic timeline reconciliation cannot restore an imported comparison link;
only journaled historical completion can establish it. Migration from v5 or
earlier expires active imported comparison associations whose selected-base
provenance cannot be established. It preserves catalogue incarnation and
revision, every retained epoch and anchor, and pending recovery journals.
Restart recovery applies the same atomic invalidation to a pending live base
replacement. This invariant does not require a per-occurrence selected-base
column; independently selected historical comparisons would require one.

## Compact core state

The dedicated epoch core retains only:

```text
application session
optional active PR authority and accepted revision identity
epoch generation and next epoch request
accepted catalog revision
optional pending prepare/commit/prune/delete context
optional pending history-page context and last accepted catalog/cursor
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
static_assert(sizeof(epoch_state) <= 2112);
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
base anchor. [[BPR-1560]], [[BPR-1572]]: a live epoch's base is the merge base
GitHub's compare API names for the pull request's recorded base `base.sha` and
its head: the commit GitHub's Files changed compares from, and so the left side
of every file row the review lists. Measured on github.com (2026-10-17,
stacked pull requests): GitHub's Files changed, its commit list and the lines a
`LEFT` comment is numbered in follow that merge base, not the base branch
tip's. GitHub keeps `base.sha` while the base branch only moves forward, also
when it fast-forwards to, or merges, commits the head has, and replaces it with
the branch's new tip one to six seconds after the branch is force-pushed (onto
any commit, a descendant of `base.sha` included).

The provider binds the answer to its snapshot. It asks compare for `base.sha`
and the head (an exact pair, remembered with its `ahead_by`). For an open pull
request it asks compare for the base branch and the head as well. When both
name one merge base it is accepted. When the branch names another, the
recorded base's is accepted only if GitHub's commit count for the pull request
equals that pair's `ahead_by` and compare names `base.sha` itself as the merge
base of `base.sha` and the exact tip the branch answer resolved (the branch
only moved forward from it); otherwise the merge base is refused. Last, the
pull request is read again and must still have the snapshot's head,
`base.sha`, base branch, state, changed-file count and commit count, or the
merge base is refused. A refusal hands the recorded base over under
`comparison_base_basis::disputed_base`, and Whole PR says GitHub's answers
disagree (`merge_base_disputed`); a read that failed hands it over under
`comparison_base_basis::recorded_base`, and Whole PR says GitHub named none
(`merge_base_unavailable`). In both, Whole PR resolves with its base
unavailable, and the recorded base is retained so updates stay comparable but
is never compared from: GitHub keeps it ahead of the merge base whenever the
base branch moved before the pull request opened or before a force push, and a
two-dot comparison from it reverts the base branch's own changes and hides the
pull request's changes the base branch already carries. The application never
computes this merge base itself. GitHub's compare names one merge base even
where Git has several (criss-cross histories); the review compares from that
one, the one GitHub names. Measured on a criss-cross pull
request (BPR-Test #106, 2026-10-17: two merge bases X1 and Y1 whose
comparisons with the head change different lines of one file, and a virtual
merge of both that changes neither): compare named X1, and GitHub's Files
changed, patches included, was exactly the comparison from X1, not from Y1 nor
from Git's recursive virtual base; the shipped composition matched it line for
line. GitHub's choice among several merge bases is not documented; the review
follows the one compare names, the same answer the file list was measured to
follow.

Before a `LEFT` line is published the same rule is asked again from GitHub:
the base branch's merge base, and when it is not the comparison base, the
recorded base's (its `ahead_by` must equal the commit count the pull request
reports) and the recorded base's against the exact tip the first answer
resolved; then the pull request is read again and must be unchanged, commit
count included, so a recorded base GitHub replaced while these were asked is
not taken for the one they describe. The pull request is read again after an
agreeing answer too, and it is the last read before every write on every
publication path (a single comment, a review, each tour write, first or
resumed), through every transport: the GitHub CLI account wrapper checks the
account with reads of its own before each write, so for every witnessed write
bound to a pull request it reads the pull request once more after that check and refuses
the write when its head or recorded base moved (a moved recorded base, with or
without the head, is `target_moved`; a moved head alone `stale_head`). The
native HTTPS transport (github.com and GHES alike) adds no read before a write,
and a signed-out session cannot write. That narrows but cannot close the window: a read and the POST that
follows are not atomic, and GitHub's review and review-comment requests carry
the head (`commit_id`) and the coordinates but no precondition on the base, so
a recorded base GitHub replaces after the last read and before the POST
arrives cannot be ruled out. The publications are then reconciled from what
GitHub stored.

A window keeps a merge base GitHub named for the same head, recorded base and
base branch across a read that failed, never across answers that refuse it. A
merge from the base branch, a rebase or a squash onto another base commit
moves the merge base and so appends an epoch whose cause is a base change; an
ordinary push does not. A change of base alone, with the head unchanged (the
base branch rebased, GitHub replacing `base.sha`), is an observation with no
head lineage on a selected-objects review as on a complete one.
Catalogues written before [[BPR-1560]] hold the recorded base on their earlier
epochs; their next live observation appends one base-change epoch. A review
anchor remains semantically distinct from an epoch head even when both resolve
to the same OID.

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

The full history remains host/catalog-owned and paged. One transition may
receive and emit at most 100 summaries, but `epoch_state` retains no page rows;
it remembers only the exact accepted catalog and continuation cursor needed to
validate the next request.

### Requests

```text
prepare_pull_request_epoch_request
commit_epoch_catalog_request
    append_epoch | associate_review
discard_epoch_preparation_request
load_epoch_history_page_request
resolve_comparison_request
perform_epoch_maintenance_request
    prune exact epoch IDs | delete exact repository-cache scope
    expected catalog revision and deterministic target digest
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
epoch_history_page_loaded | epoch_history_page_load_failed
    | epoch_history_page_load_cancelled
maintenance_completed | maintenance_failed | maintenance_cancelled
```

The history-page algebra is distinct from catalog activation and carries its
requested cursor and limit. The catalog-commit algebra identifies `append_epoch` versus
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
epoch_history_page_delta
    loading/loaded/failed/cancelled, requested cursor and limit,
    at most 100 transient rows, and the exact continuation
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
    | retention_rehydration

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
3. retention rehydration with a fresh observation identity after completed
   cache deletion;
4. base-branch change;
5. proven rewritten/non-fast-forward head (`Force-push/rebase`);
6. proven descendant head (`Author upload`);
7. changed head with unavailable lineage (`Head change; lineage unavailable`);
8. head-stable base change (`Base update`);
9. no base, head, or branch change: no new epoch unless the catalog carries
   the exact durable retention-rehydration requirement described below.

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

Whole-cache deletion is the one explicit exception to ordinary unchanged-
source idempotence. When the stable PR scope is reparented into a new physical
cache and its latest live epoch has both anchors durably marked
`cache_deleted`, the catalog exposes one typed `retention_rehydration_required`
fact. The core may then create one higher-ordinal epoch whose accepted provider
revision, base branch, base OID, and head OID correspond to the deleted latest
epoch, but the replacement must carry a nonzero host-issued 256-bit source-
observation identity not previously published in the catalog incarnation. It
receives `retention_rehydration` origin and
primary cause. A successful append clears the requirement atomically and
leaves the old cache-delete tombstone intact. A changed semantic revision or
changed branch/base/head uses its ordinary live cause, and an ordinary
unchanged observation without the durable requirement remains a no-op.
BPR-0044 persists and restores the supplied identity and rejects reuse of an
already published identity within the catalog incarnation, including the
deleted epoch's identity. BPR-0045 implements the production CSPRNG port,
mints a new identity for every accepted observation only after recovery, and
retries both the indexed pre-dispatch probe and a reservation-time collision
with a fresh value. Deterministic catalog/host fixtures prove persistence,
collision classification, and retry order; the native runtime-port contract
directly exercises the current platform source.

An evidenced historical after-head may be fetched under the accepted pull
request's Git authority. Import requires verified head closure and one derived
comparison base against the selected live base; that derived anchor and the
accepted branch context do not establish the original historical base or
branch. An unavailable head or non-unique comparison leaves the event
unbound. A GitHub review record supplies a review commit but not the exact
base viewed at submission, so it creates a review anchor or association, not
an invented historical epoch.
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

Catalog availability does not substitute for physical residency. Before
admitting a same-cache `have` or returning named/explicit comparison
candidates, the BPR-0045 host directly inspects each commit object through the
credential-free cache port. A missing or mismatched object is omitted from
negotiation or downgraded to unavailable before the core resolves the pair.

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

GitHub.com uses the exact `github.com` Git origin, not the
`api.github.com` REST/GraphQL authority. GHES uses the exact validated
enterprise hostname for both its API and smart-Git origins. The transport
sends the protocol-v2 header, requires the exact
advertisement/request/result media types, uses the RFC-BPR-0002 native
credential and system trust/proxy policy, disables cookies and caching, and
rejects every redirect. The mandatory discovery query is an original
validated request, not a redirected target.

The current JSON transport and its 4 MiB body ceiling are unchanged. A
separate smart transport reads response data in fixed chunks. Discovery and
`ls-refs` are control rounds: each complete reply is bounded to 64 KiB and is
retained before the application protocol parser validates it. Fetch is
different: each native read is at most 16 KiB and is forwarded immediately to
the incremental pkt-line/sideband consumer rather than accumulated as a
complete response body. The production smart-operation deadline is 15
minutes, with a 30-second no-progress idle deadline reset by admitted upload
or download progress. Request rounds, request bytes, response/pack bytes,
headers, packet counts, progress, disk growth, and cancellation are
independently bounded while fetch applies backpressure. These bounds describe
bytes accepted and retained by BetterPullReview, not an exact cap on Qt's
private TLS, socket, or HTTP-parser allocations. The transport emits fixed
typed failures and never retains raw server, TLS, proxy, Git, path, or
credential text in diagnostics.

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

The BPR-0043 cache/ref contract validates the pack header,
repository-format trailer checksum, and operation byte limit; publishes the
completed spool; and exposes opaque receipt and pre-import-observation values
for the exact pack identity, bounded pack-directory inventory, repository
size, and preexisting-pack fact. It does not make those facts durable.
BPR-0044 records them in the typed catalog operation. The BPR-0045 host now
requires the exact returned durable record before the validated spool can be
streamed to installed Git. The fixed ingestion command is equivalent to:

```text
git -c core.fsync=all
    -c core.fsyncMethod=fsync
    --git-dir=<validated bare cache>
    index-pack
    --stdin
    --strict=<closed warning-and-information-class ignore list>
    --check-self-contained-and-connected
    --max-input-size=<fixed-decimal-operation-bound>
    --threads=1
    --no-rev-index
```

BPR-0880 fixes that severity list, and the sparse variant passes the same list
to `--fsck-objects`. It names exactly the message identifiers whose default
severity in Git's own fsck table is warning or information: `badFilemode`,
`badTagName`, `emptyName`, `fullPathname`, `gitattributesSymlink`,
`gitignoreSymlink`, `gitmodulesParse`, `hasDot`, `hasDotdot`, `hasDotgit`,
`mailmapSymlink`, `missingTaggerEntry`, `nulInCommit`, `nullSha1` and
`zeroPaddedFilemode`. Every other identifier stays fatal.

Each identifier carries the oldest Git measured to recognise it, and is offered
only to an installation at or above that version. Git answers an identifier it
does not know with `fatal: Unhandled message id` and exits 128, so shipping one
an installation has never heard of would fail every import on it; a version
recorded too high only leaves that Git as strict as it is today. Every
identifier in the list above was measured against Git 2.34.1, Git 2.39.5 and
Git 2.53.0, and all fifteen are recorded at the admitted floor.

The reason is that `index-pack --strict` promotes a warning to a fatal error,
so a repository whose history carries one of these — an old tree with a
zero-padded file mode, say — could never be opened at all, while GitHub serves
it and `git clone` accepts it. An information-class finding never made
`index-pack` exit nonzero, but the `warning:` line it wrote to standard error
reached the same dead end through the rule below. Git's own transfer path leaves
`fetch.fsckObjects` false by default, and this cache verifies the full object
closure separately with `verify_connectivity`, so refusing a repository Git
itself accepts costs the reviewer the entire pull request for integrity the
product does not rely on. The severity is `ignore` rather than `warn` because
`warn` still writes the finding to standard error, and an accepted exit that
wrote to standard error is malformed output here: the repository would stay
unopenable and BPR-0846's exact-output rule would be the thing that had moved.

`largePathname` is the one warning-class identifier left out: it is unknown to
Git 2.39.5 and its introduction could not be dated, so it is recorded against
2.53.0 and reaches only an installation that new. The two `fatal`-severity
identifiers cannot be demoted at all — Git answers
`fatal: Cannot demote ... to ignore`. `--fsck-objects` only learned to take a
severity parameter in Git 2.44, so an older installation keeps the bare option
and the stricter sparse behaviour rather than being handed an argument it would
reject. `fsck.<msg-id>` configuration is not an alternative on either path:
`index-pack` does not read it. The `fsck_severity_fixture_contract` test proves,
against the installed Git on each platform, that every shipped identifier is one
that Git knows.

Ignoring `nullSha1` lets fsck accept a tree entry that names the null object,
but the strict path still refuses such a pack: `--check-self-contained-and-connected`
answers `fatal: did not receive expected object 0000...`, which is a closure
failure rather than an fsck finding and so arrives unnamed. The sparse path
accepts it, consistently with `sparse_commit_trees` residency, which already
tolerates objects the pack does not carry and does not run `verify_connectivity`.

An object Git refuses for an error-class finding is reported as
`git_cache_failure::unsupported_object`, whose detail names the fsck message
class, rather than as a bare process failure. The cache is not broken and no
retry helps, so the epoch path carries `epoch_failure::unsupported_object`
instead of `repair_required`; as in BPR-0361, the code says what cannot be
accepted and the text Git wrote stays in the adapter.

The client never requests a thin pack, so `--fix-thin` is absent. `--keep`,
`fetch-pack`, `unpack-objects`, and loose-object ingestion are forbidden. The
exclusive cache lock and disabled repack/maintenance/GC eliminate the race
for which `index-pack --keep` is intended. Standard output must be exactly one
validated `pack` result for the repository's object format; standard error is
bounded and reduced to fixed diagnostics. The cache/ref contract can verify
exact pack-identity agreement, accepted commits, object closure, and paired
staging/ref transactions only after receiving typed authorizations. BPR-0045
supplies those authorizations, journals before `index-pack`, and orders the
staging/ref/catalog transitions so an interrupted append is recoverable. The
public application selects this component through the completed BPR-0047
composition.

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
BPR-0053. Every unguarded Windows Git inherits only its three standard
handles, through the platform target's launch restriction; a launch whose
handle list cannot be built is refused by process creation itself, so no Git
is created and the request fails as `process_start` (BPR-1104, BPR-1115).
Input, retained output, time, cancellation, descendant termination,
and two-stage reaping are bounded. If the adapter still cannot prove a child
reaped after both stages, it permanently quarantines the lane and deliberately
retains the process wrapper instead of invoking a potentially unbounded Qt
destructor wait. It does not spawn an unbounded background reaper or admit
later work onto the lane, and the application must not free repository or
transport resources underneath that ownership. BPR-0045 implements the epoch
host's close-ingress, cancel, actor-drain, and quiescence contract; completed
BPR-0047 supplies the final composition-root destruction order. No privileged
helper, daemon, service, listener, or weakened platform-security setting is
introduced.

BPR-0042 implements the credential-free local process boundary and exact
secure consumption of
`incoming/v1/<64-lowercase-hex-operation-id>.pack.part`; commit `b65b0a9`
additionally binds cleanup to exact filesystem identities and serializes
process-boundary mutation. BPR-0043 has committed the bounded application
protocol client in `d903290`, native smart-HTTPS transport in `edd762e`, and
authority-partitioned bare cache/private-ref adapter in `2801241`. The cache
contract passed 1/1 in 162.58 seconds and directly exercises SHA-1/SHA-256
creation and reuse, exact authority and filesystem identity binding,
cross-process locking, bounded spool/inventory/import behavior, quota refusal,
cancellation, paired ref promotion/prune, deletion tombstones, and
chronological force-push retention after the old remote object is pruned. The
adjacent protocol/native/process/architecture replay passed 7/7 in 92.95
seconds; the protocol and architecture mutation scripts passed 3/3 and
104/104. A clean independent corrected-snapshot review found no remaining
issue, and the conservative simplification pass made no change because the
explicit guards and cleanup branches encode fail-closed invariants.

The BPR-0043 application boundary now exposes versioned, scheme-tagged,
fixed-size artifact identities without paths or provider/platform enums. A
completed receipt binds the exact spool identity, role, and byte count; a
prepared result binds the installed pack and index identities, roles, and
sizes plus the validated head relationship to the prior retained head; and the
pre-import inventory is bounded, ordered, unique, and explicit about a
preexisting expected pack. The opaque authorization tokens remain in-memory
state and are not persistence evidence.

BPR-0044 now supplies the durable operation record containing those artifact,
expected-pack, inventory, preexisting-pack, and installed head-lineage facts.
BPR-0045 composes that record with the import/ref/catalog/recovery sequence and
directly verifies every returned identity and phase transition before using
it. Completed BPR-0047 supplies public composition and BPR-0048 supplies the
broader cross-component fault matrix. BPR-0053 retains target-native Windows
execution. Component-only evidence does not establish Windows runtime
behavior.

### The reviewer's clone ([RFC-BPR-0092](../rfcs/RFC-BPR-0092-agent-first-run-setup.md))

No Git command runs in a reviewer's own clone. Discovery reads a candidate's
`.git/config` (at most 64 KiB, `include` not followed) and `.git/HEAD` through
the workspace adapter, follows no link and writes nothing; the core parses the
remotes and drops any user information from their URLs. A linked clone enters
an agent workspace only as the one `objects/info/alternates` line Git reads
from the workspace's own `.git`, so Git reads the clone's objects without
writing to it; the clone's refs, index, stashes, configuration and checkout
are unchanged, and no `git worktree` entry is made. The cache itself is
unchanged: the head's missing objects are packed out of it into the workspace,
never fetched into the clone.

## Catalog and recoverable commit protocol

RFC-BPR-0003 introduces the versioned app-owned SQLite database:

```text
state/v1/epoch-catalog.sqlite3
```

It is the dedicated epoch catalog. RFC-BPR-0007's private review ledger has a
separate database, schema version, and lifecycle authority; the two features
may reuse the narrow SQLite adapter library but do not add tables to each
other's database. The ledger is documented in
[Review Ledger](review-ledger.md), and the reuse is literal: both stores go
through one `configure_connection` and one `sqlite_runtime_ready` rather than
carrying copies that could drift apart.

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

BPEC v1 is a closed schema of 18 `STRICT, WITHOUT ROWID` tables and 16
indexes. Every open compares the complete non-SQLite object set by type, name,
and canonical SQL, verifies the singleton incarnation, runs
`integrity_check(1)` and `foreign_key_check`, and rejects additions or drift.
Operation detail rows carry a fixed operation kind and a composite foreign key
to the matching parent kind, so an append, prune, or cache-delete detail cannot
be attached to another operation shape. Each epoch stores the exact nonzero
256-bit source-observation identity supplied by the host, and the catalog
enforces uniqueness across published epochs and durable append operations
within one catalog incarnation. The application can probe that indexed
identity set before dispatching any accepted observation, and append
reservation repeats the check inside the writer transaction.
This is a pre-closeout amendment to the not-yet-released v1 contract. Earlier
interim development catalogs whose canonical operation-source or install
schema predates these amendments fail the closed-schema check and must be
recreated; there is no production migration surface yet.
Retention rehydration must not reuse an identity even when its semantic provider
revision and Git anchors correspond to the cache-deleted latest epoch; it is
not an exception to source-observation uniqueness. BPR-0045 implements
operating-system-entropy generation, recovery-before-mint ordering, indexed
collision probing, and bounded fresh-identity retry at the accepted-observation
boundary.

OIDs are stored as algorithm plus binary digest. IDs, enums, counts, times,
and fixed digests use typed integer/blob columns. The database contains no
credential, source body, diff, comment/review body, login, hostname, owner,
repository name, URL, raw process output, or raw provider error.
There are no generic JSON payload rows, views, triggers, caller-provided SQL,
or `ATTACH`.

The adapter builds verified, CPM-acquired SQLite 3.53.4 source as one static
library with no system-library lookup or fallback. Its release, source
identity, and SHA3-256 measurements are recorded in
[`third_party/sqlite/PROVENANCE.md`](../../third_party/sqlite/PROVENANCE.md),
and its compile/runtime contract is recorded in
[`COMPILE_OPTIONS.md`](../../third_party/sqlite/COMPILE_OPTIONS.md). Extension
loading, shared cache, URI filenames, double-quoted string compatibility, and
attachments are disabled. Defensive mode, foreign keys, and the query-planner
stability guarantee are on; trusted schema, views, triggers, writable schema,
memory mapping, and SQLite worker threads are off. Runtime statements are
prepared from an allowlist and a connection authorizer admits data access only
to the 18 known tables.

The catalog uses 4,096-byte pages and an exact 262,144-page limit, bounding the
main database at 1 GiB. It uses local WAL with `synchronous=FULL` and a
256-page automatic-checkpoint threshold; the threshold is not represented as
a hard WAL-file-size guarantee. History pages, recovery pages, exact-anchor
lookups, and prune requests are each bounded to 100 rows; a cache-delete set is
bounded to 10,000 PR scopes; and one PR may retain at most 100,000 epochs.
Operation revisions leave the final checked increment available for a terminal
or `repair_required` transition. `SQLITE_FULL` maps to the typed
`quota_exceeded` result before the caller can claim publication.

On POSIX targets, the storage root must be canonical, local, owned by the
effective user, and mode `0700`; catalog files are owner `0600`, regular, and
single-link. Linux admits only the configured ext4, XFS, Btrfs, tmpfs, F2FS,
and overlay local-filesystem set, while macOS requires `MNT_LOCAL`. Bootstrap
uses OS entropy, an exact private temporary name, no-follow handles, an
identity check, file/directory synchronization, and atomic no-replace
publication. On Windows, guarded source requires a canonical drive path on a
fixed NTFS/ReFS volume, exact `TokenUser` ownership, a protected single-ACE
owner DACL, non-reparse single-link files, retained no-delete-share ancestor
handles, and a per-catalog VFS that admits only the main, WAL, SHM, and journal
leaves and verifies their native file identities. These Windows statements
describe implemented source/contracts, not target-native execution.

Schema/version mismatch, integrity failure, unexpected schema objects, or an
unexpected path, identity, permission, or filesystem state makes the catalog
unavailable rather than recreating over evidence.

Git refs and SQLite cannot share one physical transaction, so publication
uses SQLite as the durable coordinator and the catalog as BetterPullReview's
visibility authority:

Catalog schema v3 adds a typed `append_receiving` ownership row. The v2 upgrade
validates the old schema, adds this table and updates version metadata in one
transaction, preserving existing authority, epoch and operation identities.
The earlier v1-to-v2 migration remains a separate atomic step. Legacy unfinished
operations gain no invented ownership. Terminal compaction and whole-cache
cleanup remove the receiving fact with the other recovery inputs.

When death occurs after receiving ownership is recorded but before a completed
receipt is journaled, recovery may exact-delete the receiving or completed file
under that ownership and size limit. It does not import the unrecorded pack.
A substituted file or an unknown spool containing payload remains ambiguous
and is not deleted by filename. An empty, private, regular receiving file with
no committed receiving/spool/install fact is `unowned_empty`. With no staging
or final refs, abort leaves that file untouched and abandons the operation;
it cannot claim ownership, reuse the filename or read payload from it. This
also preserves an unrelated empty replacement at that name. New spool
admission scans at most 4096 entries in the shared incoming directory and
refuses at that count without deleting anything to make space. Every entry
counts, including unowned leftovers and unrelated entries. This bounds
admission work and prevents repeated single-writer crashes from accumulating
unlimited empty files. The normal repository byte/pack/ref limits still apply.

If an installation transaction rolls back, the committed spool receipt may
still describe a complete preimport inventory plus exactly one newly installed
pack/index pair. Recovery labels that observation `inventory_only`, distinct
from journaled file ownership. With no staging or final refs, matching spool
size, unchanged prior inventory and independently verified accepted object
closure, operation abort can leave the pack/index intact and exact-clean its
owned spool. It neither deletes installed files nor publishes an epoch from
this observation. A subsequent ordinary import accounts for the retained pack
in its inventory and quotas. Missing pairs, unexpected inventory and a
substitution against an existing installation identity still refuse recovery.

1. Under the exact cache writer lock, `BEGIN IMMEDIATE` verifies the authority,
   catalog incarnation/revision, and quota; reserves a never-reused ordinal;
   and records a typed `preparing` operation with exact generated staging and
   final refs. The SQLite transaction commits before network work.
2. Create the empty receiving file and offer its exact opaque identity,
   operation and payload limit to the host's synchronous witness. The host
   persists this ownership with `record_receiving` before payload acquisition.
   Stream the bounded response into that spool. Publication preserves the
   file identity when changing its receiving name to the completed name.
   After validating its pack identity, a short catalog transaction records
   that identity, the bounded pre-import pack inventory, and preexisting-pack
   fact before `index-pack` starts. The receipt must match the persisted file
   identity and payload limit.
3. Import and verify the exact accepted objects. Before creating paired
   staging refs or deleting the spool, the cache synchronously offers its exact
   installation facts to the host's witness on the effect worker. The host
   validates and persists those facts with `record_install`; preparation
   proceeds only after the witness accepts. A refusal preserves possibly
   journaled artifacts for recovery. The cache then creates the paired staging
   refs with one expected-value ref transaction and the selected full
   durability policy, and exact-deletes the spool. The host requires the final
   prepared result to match the persisted installation.
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

The BPR-0045 append host implements this as an exact physical observation over
operation identity, spool, installed pack/index, accepted commit objects, and
staging/final ref topology. Restart aborts `preparing`, `spool_recorded`,
`imported`, `staging_ready`, or `abort_requested` residue only through the
matching recovery request. A durable `commit_authorized` operation resumes
toward promotion; `ref_commit_in_progress` with exact staging retries the fixed
promotion, while exact final refs resume catalog finalization. Every promotion,
association, or catalog error that may have applied is followed by exact
postcondition inspection. After `ref_commit_in_progress`, cancellation cannot
replace success or repair with a false cancelled terminal; after catalog
visibility, success is mandatory even when cleanup remains pending.

An unreferenced, well-formed staging pair may be removed under the lock because
it was never retained history. A preexisting pack, an unrecorded complete
pack, or any object whose operation ownership/reachability is uncertain is
never deleted; ambiguity becomes `repair_required` or remains quota-counted
until explicit whole-cache deletion. Recovery never deletes any other ref or
object.

BPR-0044 implements the durable catalog half of this protocol: exact operation
facts, phase/evidence compare-and-swap transitions, restart paging, idempotent
catalog finalization, repair provenance, and append/prune/cache-delete
tombstones. BPR-0045 now orders the append calls around real Git/cache port
mutations, resumes or cleans exact artifacts after restart, and rejects stale
host publication. Its manual-executor fixture composes fake ports, while the
adjacent Git/cache and catalog contracts directly exercise the physical and
durable component operations. Completed BPR-0048 supplies the broad combined
fault matrix, and BPR-0047 supplies selection through the public composition.

The BPR-0044 prune fact records exact authority, incarnation, ordinal, base and
head OIDs, retained-ref identities, expected catalog revision, and the
expected availability of each anchor. Only `available`, `unavailable`, and
`corrupt` are legal starting availability states. Begin, pre-finalization
replay, finalization, finalized replay, and restart revalidate those persisted
facts. Finalization atomically marks both anchors and every linked
non-destructive review association `pruned`, advances the catalog revision,
and releases the retained-epoch quota exactly once; replay performs no second
decrement or mutation.

The complete BPR-0055 prune protocol blocks new comparison work, binds the
strictly ordered target IDs into one domain-separated SHA-256 digest held in
compact core state, loads their exact base/head anchors, and durably records
the expected catalog revision, availability, OIDs, and paired refs before the
point of no cancellation. The host then deletes each pair with its exact old
values and the durable operation ID as the replay-stable authorization token,
and gives their exact absence back to the catalog for one tombstone
finalization. Adapter methods alone do not authorize the operation. The row
and exact anchor identity remain.

BPR-0055 cache deletion enumerates every PR scope in the one account/profile
cache partition and seals that complete bounded set before the point of no
cancellation. The host closes its live cache session, uses the same durable
operation authorization to quarantine and exact-delete the validated
repository, finalizes `cache_deleted` tombstones and revisions, then retries
the same physical cleanup idempotently before marking the operation complete.
Restart aborts only pre-point-of-no-return intent; every later phase resumes
mandatory exact completion. Ambiguous physical identity becomes durable
`repair_required`. Failed physical cleanup remains a retryable tombstone and
the deleted cache scope is never reopened.
Secure physical erasure from WAL/journal files or SSD media is not claimed.
When the same authenticated PR is later opened in a new cache lifetime, its
stable PR scope and history are reparented rather than rewritten. The durable
retention-rehydration requirement is set only when the latest live epoch's two
anchors are both cache-deleted; a successful replacement clears it exactly
once, while a PR with no latest live epoch cannot claim rehydration.

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
cache deletion requires the exact authenticated cache scope and identifies
that every retained pull request in that repository scope is affected before
confirmation. Deleting one account/profile partition cannot touch another
partition. Unavailable historical rows remain honest metadata until explicitly
removed; they never remap to a surviving OID.

## Catalog paging and acceptance

The catalog adapter's history paging is revision-bound:

```text
authority scope + catalog incarnation + expected catalog revision
before_ordinal exclusive
limit in 1..100
order by ordinal descending
```

Every page repeats the exact revision and returns an opaque continuation. A
revision mismatch invalidates the page and requires refresh. Core retains only
the active pair, bounded pending contexts, and the last accepted
catalog/continuation needed to authorize the next page. BPR-0056 implements
one `request_epoch_history_page_intent`, exact cursor/limit validation, one
typed host request, loaded/failed/cancelled terminals, and a sanitized
incremental page projection. Empty, short, exact-bound, and continued pages
all use the same path. Duplicate initial or in-flight requests, skipped or
rewritten cursors, non-descending/oversized rows, and authority/source/
generation/incarnation/revision drift fail closed. Catalog conflict retires
the page and starts a bounded catalog reload.

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
- A host-lifetime registry retains at most 1,000 repository-authority actors.
  Each actor serializes its complete offer/recovery/load/prepare/commit/
  discard/compare FIFO and owns at most one exact PR-scoped cache session.
  Switching PRs closes the old session before opening the new exact request;
  unrelated repository actors run in parallel.
- The public epoch-host residency record admits 64 retained jobs by default
  (hard maximum 1,024). Every queued, running, or serialized-completion-pending
  actor operation, including the synchronous diff blob bridge, owns exactly
  one ledger slot. Core epoch-request capacity refusal produces the
  request-specific `quota_exceeded` terminal; blob capacity refusal uses its
  existing generic host-failure observation rather than the unrelated
  file-size materialization-limit reason. Recoverable admission/type-erasure
  allocation or effect-executor refusal rolls back the slot before
  `unavailable`. Decision-completion callables are allocated at admission, and
  terminal retirement reserves the freed slot for one core-emitted dependent
  request so concurrent blob work cannot crowd out cleanup. Cancellation never
  silently removes an actor FIFO entry.
- Replacing an active context that retains append preparation pre-admits its
  cleanup as a distinct bounded job. Cleanup scheduling must succeed before a
  different authority is invalidated or its new offer is submitted. For a
  same-authority replacement, the preadmitted cleanup is queued before core
  acceptance. Its job control is exempt from ordinary authority cancellation,
  while shutdown can still stop it for durable restart recovery. An empty or
  observation-only preparation set is retired on the decision lane and
  consumes no actor slot. For different repository actors, active cancellation
  and invalidation occur only after both the cleanup and replacement offer
  posts succeed; failure of the second post preserves the old authority.
- Ordinary reentrant accept/dispatch ingress has a separate default capacity
  of 64 (hard maximum 1,024), retains FIFO order, and rejects overflow before
  core acceptance with exact lease cleanup and one pending diagnostic per
  turn. Matching invalidation revokes queued offers and uses two fixed
  active/pending priority slots; shutdown uses a separate priority flag. Those
  terminal events cannot be crowded out by ordinary ingress.
- One authority context retains at most 100 exact prepared operations, and one
  accepted recovery pass admits at most 1,000 operations in pages of 100.
- Network and Git preparation run off the serialized lane with immutable
  inputs and explicit cancellation.
- `materialize()` is the only concurrently callable epoch-host ingress. Its
  close-aware in-flight count participates in quiescence, its caller stop is
  forwarded into the host job control, and host shutdown closes admission and
  stops an active cache read under the same lifecycle boundary. The retained
  actor grants blob access only to the active application-session and
  pull-request authority, rechecks that grant and stop state on the actor both
  before and after cache access, and revokes it synchronously on authority
  replacement, matching invalidation, or shutdown. Result publication retains
  a counted, non-consumable actor-exit reservation through waiter notification.
  Authorization generation and result publication share the revocation mutex.
  Matching invalidation cancels and revokes at request admission; shutdown
  closes blob ingress and cancels jobs at request admission, even when their
  semantic transitions are deferred until the current decision turn ends.
  Cancellation marks and snapshots matching controls into fixed bounded
  storage, then invokes synchronous stop callbacks after releasing the job
  mutex; nested sweeps exclude controls whose cancellation was already issued.
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
- The effect executor either retains one task or throws before retaining it;
  the decision executor remains live and nonthrowing through quiescence.
  Reentrant ingress is deferred to the next decision turn. Projection
  callbacks are synchronous observations whose exceptions are contained, so
  presentation cannot veto accepted core state, durable success, cleanup, or
  FIFO progress.
- The GitHub application host has a distinct reentrant-ingress queue. Its
  resource audit remains separate future scope and is not silently covered by
  the RFC-BPR-0003 epoch-host limits.
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
    inserted/removed/changed stable IDs, exact resulting availability,
    and catalog revision

epoch_history_page_projection
    requested cursor/limit, phase/reason, stable sanitized rows,
    exact continuation; never a whole-model reset

epoch_status/command/diagnostic projections
```

The projection contract can express revision-bound page delivery and
stable-ID changes without provider, Git, SQLite, credential, or host-request
records. BPR-0045 emits bounded append/authority/comparison/status updates;
BPR-0056 emits bounded history-page updates, and BPR-0055 emits maintenance
changes. The maintenance delta carries both the exact changed epoch IDs and
their resulting availability, so Qt never infers `pruned` from a prior command;
whole-cache deletion remains one explicit bulk `cache_deleted` value. Routine
changes use exact insert/remove/move/data-change signals rather than resetting
the model or rebuilding all rows. A complete authority replacement may replace
the model, while persistent indexes from an old authority are never reused.

The core projects six epoch command identities: refresh, comparison selection,
selected-epoch prune, repository-cache delete, refresh cancellation, and
maintenance cancellation. General availability comes from the current
authority, catalog, request counters, accepted comparison, and pending lanes.
For prune, Qt may capture only epoch anchors from the accepted pair, then sorts
and deduplicates their stable epoch IDs; it cannot infer targets from visible
history rows. For whole-cache deletion it captures the exact accepted catalog
and cache authority. The existing transition still rejects stale confirmation
targets or changed authority, revision, availability, and operation identity.

The reusable RFC-BPR-0003 control contains two independent selectors, named
comparison actions, exact non-elided/copyable OID fields, explicit unavailable
states, keyboard operation, visible native focus, accessible relationships,
and one bounded deduplicated announcement. Qt dispatches typed IDs/aliases and
never compares OIDs, resolves aliases, classifies causes, queries SQLite,
reads the cache, or launches Git.

The live model retains only pages delivered to Qt and exposes each epoch's
base and head as distinct typed anchor choices while keeping one stable epoch
row projection as their source. A resolved current-viewer review anchor may
remain selected and expose its exact OID without being invented as an epoch
row. The control is embedded in the interim loaded pull-request surface;
completed BPR-0047 supplies production epoch-host wiring and second-process
public composition.

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

For the BPR-0044 component, a fresh macOS `dev` matrix passed 58/58, the focused
core/application/storage/behavior matrix passed 20/20 across five repetitions,
the AddressSanitizer/UndefinedBehaviorSanitizer matrix passed 4/4, and the
Qt-free `core-only` matrix passed 25/25. A tests-disabled Release build retained
the pinned SQLite library as a static dependency and exposed none of the
catalog fault-injection seam. Tracked and on-disk GitHub Actions workflow counts
were 0/0.

A clean MinGW cross-build compiled and linked 47/47 targets. Its CTest
inventory contained 25 tests, exactly two labeled `BPR-0053` and
`target-native-windows`. The inventory was inspected but no Windows executable
was run, so this proves source and link compatibility only and is not evidence
for a BPR-0053 target-native claim. BPR-0045 supplies the dedicated host,
native runtime ports, append/recovery orchestration, and component tests;
completed BPR-0047 demonstrates catalog durability through the selected
public end-to-end application path on the available verified targets.

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

## Public-preview cache authority

RFC-BPR-0017 adds an explicit access-mode discriminator to handoff, cache
manifest, and SQLite authority keys. Anonymous public epochs have no current
viewer or account-relative review association. Schema version 2 migrates prior
rows into authenticated scope; the anonymous scope cannot enumerate those rows
or satisfy a Git acquisition with their objects. Installed Git remains the
local object engine and receives no network or credential capability.

A selected anonymous file resolves exact repository path, base, head, and content
through the same retained epoch session and diff host. Switching modes invalidates
that session and its projection before another authority is accepted. This is
public reading, not unauthenticated access to previously cached private data.

## Review Cycles beside epochs

A Review Cycle ([RFC-BPR-0053 through RFC-BPR-0061](../rfcs/RFC-BPR-0053-review-cycles-domain-and-local-authority.md))
is not an epoch and has no pull request authority. Its revisions are captures
the reviewer asked for, kept in the profile's own `review-cycles.sqlite3` with
their bytes as content-addressed objects; none is an epoch catalog row, a
retained cache object or a claim about any remote head, and a cycle never reads
the epoch catalog or the retained cache (`review_material_bridge` answers each
subject from its own authority and never falls back to the other).

Capturing a Git checkout reads it as data. The workspace adapter parses the
index, `HEAD`, references and `.gitignore` files as bytes and never starts Git,
so hooks, filters, `fsmonitor`, `textconv` and lazy fetch never run, and nothing
in the checkout or its `.git` changes (see the
[domain model](domain-and-state-model.md#review-cycle-identities-rfc-bpr-0053)).

Publishing a cycle as a new pull request is the one cycle path that runs Git,
and it is a separate finite catalog (`publication_candidate_port`,
`bpr/adapters/git/installed_git_publication.hpp`). It shares the isolated runner
described above but none of its repositories, references or operations: every
command names a BPR-owned bare store in the profile's own `publication-git-v1`
isolation root, with no system configuration, credential helper, protocol or
lazy fetch, and sees the checkout's objects only as a per-command alternate. It
reads the starting commit and its tree, writes the candidate's blobs, tree and
commit (`commit-tree --no-gpg-sign`) in that store, and admits each object only
when Git names it as the core computed (`bpr/core/git_objects.hpp`). It never
pushes. The objects, branch and pull request reach GitHub through the native
REST publication session instead (see the
[threat model](security-threat-model.md#review-cycles)). A pull request created
or linked this way is then reviewed through the ordinary epoch path, like any
other.
