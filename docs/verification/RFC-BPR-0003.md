# RFC-BPR-0003 Verification Plan and Ledger

RFC base: `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`

RFC-BPR-0003 remains `Proposed`. RFC-BPR-0002 is still the canonical closure
frontier because BPR-0036 retains real Windows x64 evidence and BPR-0035
retains its final transition. By product-owner direction, that hardware-only
gate does not block RFC-BPR-0003 planning or implementation on available
targets. It does block RFC-BPR-0003 final closeout indirectly: BPR-0054
depends on BPR-0035, so no RFC can become `Auto-Verified` out of order.

BPR-0056 completes the bounded history-page core/host/projection slice, and
BPR-0046 consumes it through the reusable incremental Qt selector model
embedded in the interim loaded-content surface. BPR-0047 now selects the
production Git/cache/catalog/epoch path and proves two-process persistence;
BPR-0048 completes the broad combined matrix, BPR-0049 supplies available
native UX verification, and BPR-0059 retains the owner-activated macOS focus
proof.

## Architecture allocation

| Region | Selected responsibility | Direct evidence |
|---|---|---|
| Functional core | Exact active PR revision, epoch identity/cause policy, named and explicit comparison selection, alias legality, bounded history-page cursor/limit policy, request/generation acceptance, compact status/commands/deltas | Qt-free transition tables, replay/property tests, state-size assertion, forbidden-dependency scan |
| Application host | Source-free/clock-free accepted-PR handoff, bounded Git/epoch observations, OS entropy and system clock, Git/catalog jobs, revision-bound history pages, staging, serialized completion, cancellation, persistence, cache authority, lifecycle, and incremental projection mapping | Runtime-port contracts plus fake-port/manual-executor tests with reordered, stale, duplicate, failed, canceled, identity-switch, restart, commit-failure, decision-lane projection, and quiescence paths |
| Installed-Git local-process adapter | Validated installed-Git image and exec path, isolated `QProcess`, fixed local initialization/pack/object/ancestry/ref operations, secure completed-spool consumption, exact output and pack/index validation, process containment | BPR-0042 real local Git fixtures, hostile configuration/process/path canaries, exact child inspection, and architecture scans |
| Cache and retained-ref adapter | Authority-partitioned app-owned bare repositories, cache manifest/locking/reuse, completed-spool production, pre-import observations, object residency/connectivity, and paired private-ref retention | BPR-0043 real cache and local protocol fixtures, force-push residency, cross-authority isolation, quota, and exact ref inspection |
| Native Git smart transport | Minimal app-owned protocol-v2 discovery/fetch client and credential-bearing HTTPS requests under the exact RFC-BPR-0002 profile/account authority; exact refs/wants, authority-scoped haves, streaming bounds, and no redirects/cookies | Loopback TLS/proxy/protocol fixtures and exact request/body/header/authority counters |
| Epoch catalog adapter | Versioned transactional catalog, ordinal chronology, availability, pending commit/recovery, exact pruning/deletion state, no source or credential content | BPR-0044 disposable SQLite storage/behavior fixtures, transaction/restart/replay failures, exact schema/identity checks, Release seam scan, and guarded-Windows cross-build |
| Qt presentation adapter | Provider/Git-free stable epoch model, independent selectors, exact OIDs, aliases, unavailable states, focus/accessibility, incremental updates, native appearance inheritance | Qt signal/state/accessibility tests plus deterministic and native rendered automation |
| Composition root | Explicit Git/transport/catalog/host/core/Qt wiring and worker/shutdown order | Test-enabled recorded/local fixture trace and tests-disabled Release graph/binary contract |

RFC-BPR-0003 uses a dedicated epoch core and application host. It does not
expand the RFC-BPR-0002 GitHub reducer/host into a cross-feature controller,
and it does not use a Qt projection as an internal event bus.

## Authority and identity

The complete asynchronous context includes:

```text
application session
host profile + profile revision
Git host + authority fingerprint + canonical hostname + server contract
authenticated GitHub account + exact viewer actor
repository + validated locator + pull request/number
provider snapshot + typed revision origin + GitHub identity/PR generation
object format + base branch/ref + exact base/head OIDs
epoch request + epoch generation
catalog incarnation + expected catalog revision
source observation identity
```

The accepted handoff carries the provider-side context through the PR
generation, but carries neither the source-observation identity nor UTC. The
BPR-0045 host performs recovery first, then mints the source identity and
observes time only when the deterministic core plan requires them.

Physical Git objects are never shared across accounts or profile revisions.
Each bare repository partition is bound one-to-one to the exact profile,
profile revision, host, authenticated account, repository, and object format.
The catalog allocates an opaque random repository-cache scope only after that
authority is authenticated, and separately allocates an opaque random
pull-request authority scope. Neither ID is derived from provider text.

Catalog visibility, alias resolution, selection, and every publication remain
bound to the current profile/account/PR authority. There is no offline-open
fallback that makes cached private content visible to an unauthenticated or
different account. Every review epoch ID also includes the nonzero random
catalog incarnation and its never-reused ordinal. Recreating the catalog
creates a new incarnation, so stale IDs cannot collide with a recreated
authority or ordinal.

Commit and object OIDs are bounded algorithm-tagged value types. Raw provider
text, labels, branch names, and filesystem paths are never identity. A
`review_epoch_id` is unique only inside its exact pull-request authority.
Comparison endpoints distinguish an epoch's base/head anchor from its friendly
label, so `Branch base`, epoch heads, review-associated commits, and arbitrary
pairs always resolve to exact OIDs.

## Typed vertical slice

```text
accepted GitHub pull-request revision
  -> source-free/clock-free typed handoff to epoch host
  -> bounded durable recovery before source mint or core acceptance
  -> fresh host-minted source identity + host-observed UTC when required
  -> bounded prior-catalog + ancestry/material observations
  -> deterministic core request with exact authority/generation
  -> native-HTTPS protocol-v2 discovery/fetch + isolated Git pack indexing
  -> request-scoped staged objects/refs and epoch candidate
  -> serialized completion with exact context
  -> core acceptance/record decision
  -> failure-atomic private-ref + catalog commit
  -> exact catalog/comparison/status deltas
  -> incremental Qt rows and independent selectors
```

No callback, fetched object, staging ref, catalog row, label, or Qt selection
becomes active merely because it exists. SQLite is the durable coordinator
and catalog visibility authority. Exact pending operation state coordinates
the staged refs, final refs, and catalog row; only the matching committed
terminal may publish. Recovery either completes the exact recorded operation,
removes only unretained staging state, or marks the affected scope
unavailable/repair-required. It never guesses from a ref or exposes a false
visible success.

An unchanged base/head/branch observation creates no epoch. A new or changed
newest-current-viewer review fact uses an expected-revision SQLite-only
association commit, advances the catalog revision only when the durable fact
changes, and re-resolves `My last reviewed` without inventing a Git epoch.

## Deterministic epoch policy

The core receives explicit branch identity, prior/current base/head OIDs,
head-lineage facts, material-equivalence identity, optional historical-review
association, and host-supplied observed time. Ordinal/catalog revision, not
wall time, defines chronology.

Epoch cause is composite. The stored facts independently record origin
(`initial_observation`, `live_observation`, `explicit_historical_import`, or
`retention_rehydration`), base transition (`not_applicable`,
`unchanged`, `base_oid_changed`, or `base_branch_changed`), head transition
(`not_applicable`, `unchanged`, `descendant`, `non_fast_forward`, or
`lineage_unknown`), and material relation (`same`, `different`, or
`unknown`).

The primary label preserves those facts: initial observation, historical
import, base-branch change, proven `Force-push/rebase`, proven
`Author upload`, `Head change; lineage unavailable`, or head-stable
`Base update`. When base and head both change, the head label applies unless
the branch identity changed, while the independent base transition remains
visible. Unknown lineage is never guessed. Material equivalence never erases
a distinct observed revision; it only provides an opaque reuse relationship
for later receipt/diff work.

Named comparisons resolve to exact anchors:

- `Whole PR`: latest live epoch's exact base anchor to its exact head anchor;
- `Previous upload -> latest`: nearest earlier live epoch with a different
  head, including a force-push/rebase epoch, to the latest live head;
- `My last reviewed -> latest`: newest chronologically ordered, non-pending
  current-viewer review commit to the latest live head;
- arbitrary retained anchor to arbitrary retained anchor.

A named alias remains semantic and may re-resolve when accepted review facts
or the catalog revision advances. Resolution chooses the semantic candidate
before checking residency: an unavailable previous-upload or newest-review
candidate makes the alias unavailable and never falls back to an older
resident candidate. An explicit pair remains exact and becomes visibly
unavailable if its object is pruned; it never silently jumps to Latest. Any
two resident anchors in the same exact PR authority and catalog incarnation
are legal, including reverse and identical pairs; identical OIDs produce an
empty comparison.

## Git process and transport

Installed Git is a local pack/object/ref engine, not a second production
authentication transport. Credentials, credential-bearing URLs, proxy
authority, cookies, and GitHub trust decisions never enter its argv,
environment, configuration, standard streams, or child tree.

The native Git smart transport admits only:

- exact `GET /<owner>/<repository>.git/info/refs?service=git-upload-pack`;
- exact `POST /<owner>/<repository>.git/git-upload-pack`;
- the validated profile/account/hostname authority and expected Git content
  types;
- bounded packet framing, request batches, streamed pack bytes, duration,
  disk, headers, and fixed diagnostics.

The protocol-mandated discovery query is an original validated request. Every
redirect remains forbidden, including query-bearing redirects; RFC-BPR-0002's
redirect policy is not relaxed. The current JSON-oriented native transport and
its 4 MiB response contract are not relabeled as pack-stream evidence.

GitHub.com smart Git uses `github.com`, while `api.github.com` remains its
REST/GraphQL authority. GHES smart Git uses the exact validated enterprise
hostname. Discovery and `ls-refs` are bounded control rounds: their replies
are read in fixed chunks and retained only within a 64-KiB whole-reply
ceiling. Fetch response bytes are read in chunks of at most 16 KiB and
forwarded directly to the incremental protocol consumer rather than retained
as one body. Production source policy declares a 15-minute total deadline and
a 30-second no-progress idle deadline.

The app-owned protocol-v2 client names exactly the accepted base-branch and
pull-request-head prefixes in `ls-refs`, requires exactly those two advertised
refs and provider-approved OIDs, and admits only the deduplicated accepted
base/head `want` set plus bounded haves derived from the same physical
authority scope. Known optional capabilities may be advertised and ignored;
unrequested capability use and alternate retrieval paths are rejected. Pack
bytes enter installed Git only through this fixed local operation:

```text
git -c core.fsync=all
    -c core.fsyncMethod=fsync
    --git-dir=<validated bare cache>
    index-pack
    --stdin
    --strict
    --check-self-contained-and-connected
    --max-input-size=<bound>
    --threads=1
    --no-rev-index
```

Production never invokes `fetch-pack`; installed Git receives no network or
credential authority.

The installed-Git adapter resolves an absolute executable/exec path, builds an
environment from nothing, uses app-owned empty system/global configuration and
templates, operates only on canonical validated bare repositories, and admits
fixed commands. It neither inherits nor invokes user/system aliases, URL
rewrites, credentials, askpass, hooks, filters, LFS, submodules, textconv,
external diff, alternates, replace objects, SSH/file/ext transports, editors,
pagers, tracing, fsmonitor, worktrees, auto-GC, or maintenance.

BPR-0042 directly implements and verifies that local child boundary and
secure consumption of an already completed
`incoming/v1/<operation64>.pack.part` spool. The evidence is carried by
commits `0059fbc`, `6e2e7fc`, `a0a92cf`, and `251be4b`; the final focused
matrix covers `git_process_contract`, `architecture_boundaries`, and
`architecture_boundary_contract`, and the standalone architecture mutation
suite covers 104 hostile mutations. Commit `b65b0a9` further binds cleanup to
exact filesystem identities and serializes process-boundary mutation across
adapter instances.

Commit `d903290` implements the closed application protocol-v2 acquisition,
and commit `edd762e` implements the native smart-HTTPS wire and lease
boundary. Their focused replay passed 5/5:
`git_protocol_boundary`, `application_git_protocol_v2`,
`native_git_smart_transport_contract`,
`native_git_smart_transport_fixture`, and `git_process_contract`. The native
fixture runs 19 isolated GHES scenarios plus the GitHub.com discovery,
`ls-refs`, and fetch success scenarios, for 22 child runs without contacting
GitHub.com or a GHES service. The timeout fixture uses a reduced test-only
absolute deadline; the 30-second idle value remains source-policy evidence,
not independently elapsed fixture evidence.

Commit `2801241` completes the BPR-0043 authority-partitioned bare-cache and
retained-private-ref component. `git_cache_contract` passed 1/1 in 162.58
seconds. Its real SHA-1/SHA-256 repositories cover exact authority isolation
and reuse, immutable manifest/topology/configuration validation,
cross-process locking, identity-bound spool/import/cleanup, quotas,
cancellation and operation races, paired ref promotion/prune, deletion
tombstones, and chronological old-head residency after the fixture remote
removes and prunes the old object. The adjacent protocol/native/process/
architecture replay passed 7/7 in 92.95 seconds; direct protocol and
architecture mutation scripts passed 3/3 and 104/104. A tests-disabled
Release build contained zero cache fault-injection markers. The corrected
snapshot received a clean independent review, and the simplification pass
made no change because the explicit guards and cleanup paths encode the
fail-closed invariants. The documentation gate and `git diff --check` passed,
and tracked/on-disk GitHub Actions workflow counts were 0/0.

The cache application port now carries versioned fixed-size no-path artifact
identities, roles, and sizes for the completed spool and installed pack/index,
plus canonical bounded pre-import inventory facts. Those are serializable
inputs for BPR-0044; the accompanying opaque authorization tokens remain
in-memory state and are not durable-operation evidence.

This earlier evidence is component evidence only. BPR-0044 supplies durable
operation facts, and the BPR-0045 component described below composes them with
journal-before-`index-pack`, ref/catalog sequencing, stale host rejection, and
restart recovery. Completed BPR-0047 supplies selection through the public
BetterPullReview composition, and BPR-0053 retains target-native Windows
execution. The evidence above alone does not prove those later component or
public claims, Windows behavior, or RFC-BPR-0003 completion.

## Cache, retention, and persistence

The selected storage shape is versioned and contains no raw hostname, owner,
repository name, account login, URL, or credential:

```text
repositories/v1/<64-lowercase-hex-repository-cache-scope-id>.git
```

The catalog assigns that random scope ID to exactly one profile-revision,
host, account, repository, and object-format tuple. An immutable identity
manifest, canonical-descendant check, private permissions, one-writer lock,
exact bare/config/object-format validation, and absence of
alternates/shallow/graft/worktree state gate every use.

Only the accepted base branch and `refs/pull/<number>/head` advertisements are
eligible. Both advertised OIDs must equal the accepted provider snapshot.
Raw `HeadRef` text and provider clone URLs never become Git authority.
Request-scoped staging refs precede atomic compare-and-swap retention refs:

```text
refs/betterpullreview/v1/stage/<64-lowercase-hex-operation-id>/base
refs/betterpullreview/v1/stage/<64-lowercase-hex-operation-id>/head
refs/betterpullreview/v1/scope/<64-lowercase-hex-pr-scope-id>/epoch/<16-lowercase-hex-ordinal>/base
refs/betterpullreview/v1/scope/<64-lowercase-hex-pr-scope-id>/epoch/<16-lowercase-hex-ordinal>/head
```

The catalog allocates the PR scope ID for the complete cache/PR authority and
binds it one-to-one to that authority. It contains no raw provider text and
cannot collide merely because two accounts observe the same PR number. An
absent, colliding, or mismatched scope/manifest binding fails closed.

BPR-0044 implements the BPEC v1 durable component: a closed 18-table/16-index
schema, authority/incarnation/revision facts, epoch/review/anchor state, and
typed append, prune, and cache-delete operation records. It persists a
host-issued nonzero 256-bit source-observation identity exactly and rejects
reuse among published epochs in one catalog incarnation. Its deterministic
fixtures prove persistence, freshness relative to prior stored identities,
and collision rejection, not entropy generation. BPR-0045 adds recovery before
minting, production operating-system CSPRNG ports, and a fresh retry after a
catalog-reported collision.

Private refs are retention roots. A hard quota refuses new material rather
than silently deleting history. Pruning and repository deletion are explicit
exact-target operations that first record pending durable intent, enumerate
affected history, update refs/catalog availability, and recover or finish
after interruption. BPR-0044 prune facts admit only available, unavailable,
or corrupt starting anchors and retain exact expected availability, OIDs, ref
identity, and revision through begin/replay/finalize/restart. Catalog
finalization atomically marks both anchors and linked non-destructive review
states pruned and releases logical quota exactly once. BPR-0055 implements the
core-authorized paired-ref deletion and cache-maintenance host paths. No glob
or broad credential/cache deletion is permitted.

## Application-host component

BPR-0045 implements the source-free/clock-free
`accepted_pull_request_handoff`, production runtime ports, and epoch host
component. The production entropy port uses `getentropy` on macOS,
`getrandom` on Linux, and system-preferred `BCryptGenRandom` on Windows; the
system-clock port rejects values outside the core UTC domain. Recovery runs
before source mint or core acceptance, and a catalog-reported identity
collision causes a fresh CSPRNG sample, bounded to 32 attempts.

The host retains at most 100 prepared observations and 1,000 repository actors,
and scans at most 1,000 recovery operations in pages of 100. Each repository
authority has one bounded FIFO actor and one exact PR cache session at a time;
switching PRs closes and reopens that session, while unrelated repositories
can progress independently. Before offering haves or comparison endpoints, the
host requires catalog availability and direct commit-object inspection. It
does not treat a catalog row as proof that an object still exists.

Append work records `preparing` before import and advances through exact durable
spool, import, staging, commit-authorization, ref-commit, visibility/cleanup,
and terminal phases. Restart recovery resumes the recorded phase rather than
guessing. Failures and cancellation before the point of no return use exact
abort; after durable commit authorization, the host must resolve the operation
to success or an explicit repair state. Applied-then-error results are resolved
through physical/catalog postconditions before the host publishes an outcome.

Effect work never publishes directly. Decision-lane completion rechecks its
exact pending authority, accepted provider revision, and generation before
core acceptance or projection. The host source defers reentrant ingress behind
the current decision and contains projection exceptions. Quiescence waits for
actors, preparations, decisions, and deferred work to drain.

The focused component command is:

```sh
cmake --build --preset dev --target \
  better_pull_review_application_epoch_contract_tests \
  better_pull_review_application_epoch_host_tests
ctest --preset dev --output-on-failure \
  -R '^(application_epoch_contract|application_epoch_host)$'
```

`application_epoch_contract` directly verifies the handoff and production
runtime-port boundary. `application_epoch_host` uses deterministic capability
fakes and manual effect/decision executors to verify exact ordering, all append
restart phases, mandatory-success/postcondition resolution, repository FIFO and
PR-session switching, stale/cancel rejection, decision-lane-only projection,
projection-exception containment, deferred reentrant ingress without recursive
publication, quiescence, and bounded history paging. Its page matrix covers
empty, one-row, multi-page, exact-bound, invalid/malformed, catalog-revision,
identity-switch, failure, cancellation, duplicate, and deliberately reordered
terminal cases. Adjacent real installed-Git, cache, and SQLite
contracts verify the physical and durable port implementations. BPR-0047
selects those implementations through the public executable and proves
second-process cache/catalog reuse for GitHub.com and GHES; BPR-0048 supplies
the broad combined fault/reordering matrix; BPR-0055
completes maintenance authorization and recovery; BPR-0046 completes the Qt
history trigger/model and interim loaded-window control; and BPR-0053 retains
target-native Windows evidence.

BPR-0055 adds one deterministic maintenance lane to the compact core. A prune
request contains a strictly ordered unique batch of at most 100 exact epoch
IDs; the pending state retains only its count and a domain-separated SHA-256
target digest. Whole-cache deletion binds the exact active cache scope. Every
terminal revalidates authority, accepted source, generation, catalog, kind,
cache, target count, and digest. Duplicate, stale, cross-authority, and
tampered terminals are inert. Success publishes exact stable-ID availability
changes for prune or one bulk `cache_deleted` change, then re-resolves the
semantic comparison.

The host converts only that core request into durable action. It loads exact
anchors before beginning prune, seals the complete bounded cache impact before
deletion, and uses the durable operation ID as the replay-stable Git
authorization token. Cancellation aborts only before `ref_delete_in_progress`
or `quarantine_rename_in_progress`; every later phase resumes mandatory exact
completion. Mixed ref/cache identity or phase ambiguity becomes durable
`repair_required`. The manual-executor matrix covers every maintenance restart
phase and injected lookup, transition, ref, quarantine, finalization, and
cleanup failure. The real `epoch_maintenance_integration` composition drives
core, host, SQLite, and installed Git together and reopens durable state after
exact prune and cache deletion.

## Available-platform security verification

BPR-0050 completed the current security matrix on native macOS arm64 and an
Ubuntu 26.04 x86_64 full-system QEMU guest. The Linux guest directly selected
GCC 15.2, Git 2.53, Qt 6.10.2, libsecret 0.21.7, and gnome-keyring 50. The
broad process, smart-protocol, native-HTTPS, cache, catalog, recovery,
maintenance, cancellation, authority, hostile-canary, dependency, runtime,
and public-composition groups pass after the separately tracked BPR-0060
through BPR-0064 remediations. A fresh tests-enabled GCC Release tree also
executes the optimized protocol/transport paths 4/4; tests-off Release graph,
binary-byte, dependency, runtime, and ELF inspections select the production
surface without fixture or dormant CLI execution seams.

The current Linux Secret Service v2 lifecycle passes against an isolated real
session service and disposable keyring. Separate direct sessions prove no-bus
unavailability and a D-Bus-observed locked default collection. A live hostile
dependency-selected owner receives exactly one property inspection and no
session, alias, search, create, secret-read, or delete method while the
canonical owner remains present. Native macOS retains the corresponding
installed-Git, Keychain v2, public composition, dependency, runtime, and
cleanup evidence, plus remediation-specific sanitizer replays.

Checked-in evidence contains no private-key artifact, private-key body,
GitHub token body, credential file, private provider response, or runtime
canary. The only credential-bearing URL syntax is an intentional
`proxy.example.invalid` rejection fixture. GitHub Actions remains absent from
both the tracked tree and on-disk workflow directory. Real Windows execution
is intentionally still open in BPR-0053 and is not inferred from the macOS or
Linux results. The critical security review also required the live guest to be
bound to the host by an identical 240-file build-input manifest, then stopped
and removed with its SSH key, VM images, build logs, and port forward; the
final workspace scan retains none of those credential/image artifacts.

## Claim matrix

| Claim | Direct surface | Proof | Result |
|---|---|---|---|
| Core policy is deterministic and compact | Qt-free epoch reducer | Exhaust cause, alias, pair, idempotence, overflow, replay, and mismatch tables; inspect state size/dependencies | Verified in BPR-0041: core-only 20/20, focused core/architecture 2/2, explicit catalog-incarnation/projection-clear/session canaries, independent final review |
| PR/epoch authority is exact | Core/host/cache/catalog contexts | Cross profile revision, host, account, repository, PR, snapshot, generation, catalog incarnation, and catalog revision | Verified on available component/public surfaces: BPR-0041 covers deterministic core identity; `d903290` and `edd762e` cover accepted refs/OIDs and native lease authority; `2801241` covers physical cache/PR-ref scope isolation; BPR-0044 persists and rejects mismatched durable identities; BPR-0045 verifies source-free handoff binding, host-minted identity, exact PR-session switching, and stale host rejection. BPR-0047 crosses the accepted GitHub decision lane into the exact public epoch authority for both GitHub.com and GHES. BPR-0048 directly acquires the bound handoff transport and proves an authority switch revokes both the lease and issued transport before new provider work |
| App-owned bare cache is created and reused safely | Real Git adapter and temporary app-data root | Open fixture twice under one authority, then cross account/profile revision; inspect exact manifest/config/path/object format, reuse, and physical isolation | Verified for the BPR-0043 component in `2801241`: `git_cache_contract` passed 1/1 in 162.58 seconds with SHA-1/SHA-256 reuse, physical authority isolation, exact identity and topology drift checks, cross-process locking, quota refusal, and substituted-state preservation. BPR-0047 additionally proves public two-process reuse with zero smart calls on restart; BPR-0048 supplies cross-authority combined cases |
| Installed Git receives no credential or network authority | Process and app-owned smart-transport canaries | Inspect exact protocol-v2 requests, fixed `index-pack` image/argv, environment, cwd, config, streams, descendants, sockets, native-HTTPS counters, bounded same-scope haves, and rejected alternate retrieval capabilities | Verified on available targets across BPR-0042/BPR-0043/BPR-0045/BPR-0047/BPR-0050: the fixed child has no credential/network authority, `d903290` admits only the closed protocol acquisition, `edd762e` proves the native HTTPS wire/lease boundary in 19 GHES plus 3 GitHub.com scenarios, `2801241` directly exercises the validated spool/inventory/cache handoff, the host requires direct commit inspection before haves or comparisons, and the public composition selects those pieces. BPR-0050 repeats the process, environment, descendant/socket, smart-protocol, cache, public-runtime, and secret scans on macOS and Ubuntu x86_64; real Windows remains BPR-0053 |
| Old observed head survives force-push | Local smart remote, cache, refs, catalog, public selector | Observe and retain the old head, then remove/prune the remote object, import the replacement, restart, and compare retained old head to latest | Verified on the available public path: `2801241` retains and directly inspects both old and replacement cache revisions after remote pruning; BPR-0044 proves durable multi-epoch chronology/anchor restart round trips; BPR-0045 verifies append/recovery orchestration and direct commit inspection. BPR-0048 adds a manifest-bound sibling-head public graph whose self-contained replacement pack omits the old head, requires the rendered second cause to be `Force-push/rebase`, and proves GitHub.com/GHES seed/reuse reopens and compares the retained old head with zero smart calls |
| Repository behavior cannot execute | Hostile Git/config/repository fixtures | Arm hooks/helpers/askpass/filters/LFS/submodule/textconv/ext-diff/protocol canaries; require zero invocation | Verified on the directly exercised macOS and Ubuntu x86_64 surfaces through hostile real-Git/config state plus exact child image/argv/environment/cwd/port, descendant/socket, cache, smart-transport, and public-composition inspection. The canaries remain absent after BPR-0050's target-native matrix; real Windows execution remains BPR-0053 |
| Epoch causes remain truthful | Core cause table with real Git facts | Replay initial, descendant upload, non-fast-forward rewrite, base-only, branch-change, simultaneous-change, equivalent, unknown-lineage, and retention-rehydration cases | Verified on available surfaces: BPR-0041/BPR-0044 exhaust deterministic core/catalog policy, including one-shot rehydration. BPR-0045 supplies bounded directly inspected Git observations. BPR-0048 maps the remaining scenarios to those exact tables and adds installed-Git proof that the public replacement is non-fast-forward plus a stable Qt-role assertion that ordinal two is `Force-push/rebase` |
| Catalog/ref commit and recovery agree | Fault-injected catalog and Git refs | Fail or interrupt every phase; restart; require the catalog visibility authority to expose one exact committed epoch or an unavailable/recoverable pending state, never a ref-derived false success | Partially verified: BPR-0044 proves exact typed operation facts, phase/evidence/revision CAS, and idempotent catalog replay. BPR-0045 verifies append-phase continuation and postconditions. BPR-0055 covers every prune/cache-delete durable phase, failure injection, exact authorization replay, and real Git/SQLite composed cleanup. BPR-0048 supplies the broad combined matrix and BPR-0047 public composition |
| Defaults and arbitrary pairs use exact OIDs | Core + Qt + public application | Verify Whole PR and selected IDs/OIDs/context agree; accept same-authority reverse and identical pairs, with identical OIDs producing empty comparison | Verified on the available public path: BPR-0041 proves exact core pair/alias policy, BPR-0044 restores exact anchor IDs/OIDs, BPR-0046 verifies typed Qt behavior, and BPR-0047 drives Whole PR, arbitrary, reverse, and identical selections with complete OIDs through the real composition and after restart |
| Named aliases resolve exactly | Catalog/core/public application | Verify Whole PR, nearest preceding distinct live head, newest ordered current-viewer review, and an association-only review update on an unchanged head; make the exact alias unavailable instead of falling back when its candidate is unavailable | Partially verified: BPR-0041 proves semantic candidate/no-fallback policy, and BPR-0044 persists chronology, availability, review order, and association-only updates. BPR-0046 proves Qt dispatch and unavailable handling. BPR-0047 proves Whole PR, Previous upload, and My last reviewed agree with stable rendered OIDs through the public composition and restart; BPR-0048 supplies the combined association-only/unavailable cases |
| Pruned and never-observed history is honest | Catalog/core/Qt | Explicit prune/delete and unreachable unknown head stay visible/unavailable and never silently remap | Partially verified: BPR-0044 proves durable exact prune/cache-delete facts and idempotent finalization; BPR-0055 proves core authorization, exact paired-ref deletion, whole-cache cleanup, restart recovery, and matching incremental availability projection through fake-port and real Git/SQLite composition. BPR-0046 adds the missing exact resulting-availability projection fact and directly renders pruned, cache-deleted, unavailable, and never-observed states without remapping. BPR-0047 supplies production public presentation |
| Stale/canceled work cannot publish | Manual executor and real process cancellation | Reorder/duplicate/cancel Git, transport, catalog, and identity-switch completions | Partially verified at component boundaries: earlier protocol/process/cache slices reject canceled work and exact lease drift; BPR-0045 directly verifies pre/post-PONR cancellation policy and rejects stale load, preparation, and comparison terminals before core/projection publication. BPR-0056 rejects mismatched authority/source/catalog/cursor, cancels on revision or identity replacement, and proves a deliberately reordered loaded page cannot publish rows. BPR-0048 supplies the broad combined fault matrix and BPR-0047 public composition |
| Qt projection is incremental, accessible, and system-native | Stable-ID model and real widgets | Assert exact signals, persistent indexes, focus, keyboard, OID text, announcements, responsive layout, zero routine resets, and live native palette/style/font inheritance | Partially verified: BPR-0046 and refreshed BPR-0049 three-scale actual-window cases pass with exact insert/remove/move/data signals, persistent anchor indexes, selector and QAction translation, 40/64-character OIDs, explicit states, focus/accessibility, deduplicated bounded announcements, live font invalidation, and zero routine resets. BPR-0049 directly renders the current real application through unforced Ubuntu XCB/QFusionStyle and has inspected Cocoa/QMacStyle System Light/Dark palette, pixel, selection, OID, identity, and zero-reset evidence. The prior macOS process was inactive with null focus; BPR-0059 retains the required active-window/existing-focus proof rather than inferring it from offscreen geometry or inactive native rasters |
| Public composition selects production pieces | Real executable | Recorded GitHub plus local Git protocol fixture reaches exact visible pair; second invocation proves persistence; Release excludes seams | Verified in BPR-0047 for GitHub.com and GHES: the real QAction/provider/accepted-handoff/smart-protocol/installed-Git/cache/SQLite/core/Qt path creates two epochs, renders every required named/explicit pair, reopens them in a second process with zero smart calls, drains both hosts, and the tests-disabled Release graph/binary excludes all fixture arguments, markers, sources, and factories |
| Resource use is bounded without silent retention loss | Protocol/process/cache/catalog/history fixtures | Exceed packet, stream, pack, disk, ref, row/page, output, time, and process limits; require bounded failure and no automatic prune | Verified except for the explicitly unexecuted production 30-second elapsed idle case: earlier slices cover process/protocol/cache bounds; BPR-0044 covers database/SQLite/catalog/epoch/revision/checkpoint quotas; BPR-0045 covers bounded source/preparation/recovery/actor work; BPR-0056 covers 1..100 revision-bound pages with no core row retention or model reset. BPR-0048 hardens the target-native reduced-deadline fixture against proxy/instrumentation startup races and reruns it normally and under Address/UndefinedBehavior Sanitizers without changing production deadlines |
| Platform claims are target-native | macOS, Linux, and Windows | Run configure/build/tests, Git/process/network/cache/catalog/Qt/runtime/dependency evidence on each claimed target | Partially verified: prior slices and BPR-0050 provide direct macOS process/protocol/native-loopback/cache/catalog/Keychain/public-runtime evidence; BPR-0045 adds guarded production CSPRNG/system-clock ports and current-host native contracts. BPR-0058 adds the retained-directory replacement canary and syscall proof that SQLite main/bootstrap/sidecar opens remain under `/proc/self/fd` with final-component no-follow. BPR-0050 completes the broader Ubuntu 26.04 x86_64 GCC 15.2 process, protocol, Secret Service, cache, catalog, maintenance, public Qt, dependency, and runtime matrix. MinGW remains compile/link evidence only, and every affected Windows runtime claim remains open in BPR-0053 |
| GitHub Actions is absent | Repository and documentation | Tracked/on-disk workflow scans plus documentation validator | Verified through BPR-0050: tracked and on-disk workflow counts are 0/0, no source/build integration selects GitHub Actions, and every macOS/Linux build and verification command is local or target-native; repeat at RFC closeout |

## Platform and safe-target plan

| Surface | Plan |
|---|---|
| macOS arm64 | Direct installed-Git, native smart transport, Keychain authority, cache/catalog, process cancellation, public fixture, native Light/Dark Qt, dependency/runtime, and cleanup evidence |
| Linux x64 desktop | Local full-system VM/QEMU build and target-native Git, native smart transport, Secret Service authority, cache/catalog, process cancellation, native Qt, dependency/runtime, and cleanup evidence |
| Windows x64 | Implement guarded source/contracts now; retain DACL, junction/long-path, installed-Git, Credential Manager, native transport, Job Object/process tree, cache/catalog, public/Qt, PE/runtime, and cleanup execution in BPR-0053 |
| GitHub.com/GHES | Recorded GitHub transport plus local smart-protocol fixtures by default; no private response or credential retained |
| GitHub mutations | None; only read-only smart Git discovery/upload-pack is in scope |
| Build ownership | Local and target-native CMake/CTest/scripts only; GitHub Actions has no role |

Windows BPR-0053 is nonblocking for implementation and later RFC work, but
blocks affected Windows claims and RFC-BPR-0003 closeout. Cross-compilation,
mocks, another operating system, or hosted workflow output cannot substitute
for its target-native evidence.

## UX and presentation plan

RFC-BPR-0003 adds a reusable comparison group to the interim loaded-content
surface, not the final workbench. Two independent selectors and named
comparisons expose stable authority/incarnation/epoch IDs, full copyable OIDs,
ordinal, composite cause facts, availability, and explicit loading,
refreshing, retained-error/cancel, empty, pruned, cache-deleted, and
never-observed states. An unavailable named candidate remains selected with
its reason and never falls back. Reverse and identical exact pairs remain
representable. Qt dispatches IDs/aliases and never infers cause, fallback,
residency, or legality.

Deterministic renders cover supported widths and 1.0x, 1.25x, and 2.0x.
Native macOS evidence covers actual system Light and Dark plus retained pair,
focus, model identity, exact OIDs, geometry, and zero resets through a live
appearance transition. Routine catalog and selection changes use exact
insert/remove/data-change signals rather than resetting the model. The
interim control inherits the active system palette, native style, and
interface font and applies no forced theme. Offscreen forced-style evidence
remains geometry-only.

RFC-BPR-0005 must still allocate and complete its distinct final integrated
workbench presentation-polish/native visual-acceptance card. RFC-BPR-0006 owns
selectable System/Light/Dark/high-contrast modes, with fresh install defaulting
to System, plus fonts and syntax presentation. RFC-BPR-0003 makes no final
visual-polish or custom-theme claim.

## Documentation ledger

| Scope | Required documents | Disposition | Planned evidence |
|---|---|---|---|
| Core epoch state and typed flow | Domain/state model and Git/epoch architecture | Updated: exact revision/incarnation, composite cause facts, exact alias/pair policy, request/generation acceptance, BPR-0056 bounded page/cursor/delta policy, and BPR-0055 digest-bound maintenance intent/request/terminal policy; no page rows or maintenance target vectors are retained | Core paging/cause/selection/maintenance tables, replay tests, state-size and dependency scans |
| Host/Git/transport boundary | System architecture, Git/epoch implementation, security model | Updated through component work: earlier slices harden installed Git, smart transport, cache/ref consumption, and durable catalog facts; BPR-0045 adds the source-free handoff/runtime ports, per-repository FIFO host, exact append/recovery sequencing, physical commit inspection, stale/cancel rejection, and projection/quiescence containment. BPR-0055 composes exact prune/cache-delete authorization and every durable restart phase; BPR-0056 adds revision-bound catalog page execution and sanitized incremental projection. BPR-0047 adds the narrow accepted-snapshot outlet, revocable account capability, concrete public composition, and quiescent shutdown order | `application_epoch_contract`, `application_epoch_host`, `application_github_host`, `epoch_maintenance_integration`, adjacent real Git/cache/SQLite contracts, and both public persistence cases |
| Cache privacy and operations | Git/epoch architecture, [build/test operator guidance](../development/build-and-test.md#retained-git-history-and-epoch-storage), and product entrypoints | Updated through BPR-0051: `2801241` implements physical authority isolation and exact cache/ref cleanup; BPR-0044 implements durable append/prune/cache-delete facts; BPR-0045 composes append authorization/recovery; BPR-0055 composes core-authorized paired-ref and whole-cache maintenance with exact durable tokens, cancellation boundary, recovery, repair, and incremental availability. README and troubleshooting now state installed-Git admission, opaque private storage, force-push/never-observed limits, quota refusal, restart/repair behavior, exact destructive consequences, and the absence of a public maintenance command | Hostile cache fixtures, catalog restart/corruption/quota contracts, manual-executor maintenance failures/restarts, real `git_cache_contract`, real Git/SQLite maintenance composition, immutable-base documentation impact review, and BPR-0051 gate reruns |
| Epoch semantics | [Product contract](../architecture/product-contract.md), product/UX reference, and verification ledger | Updated through BPR-0051: retained observed revisions are a distinct private authority; composite causes, unknown lineage, chronology, equivalence, exact anchors, aliases without fallback, reverse/identical pairs, unavailable history, and GitHub's continued authority over shared current facts remain explicit | Core/host/public fixture matrix and immutable-base authority review |
| Qt presentation | Workbench and appearance/accessibility documents | Updated in BPR-0046: reusable base/head selectors, named actions, exact copyable OIDs, explicit retained/unavailable states, incremental stable model, accessibility, bounded announcements, and native System inheritance; no final-workbench or target-native appearance claim | Three scale-specific Qt signal/state/interaction/actual-window raster cases; BPR-0049 retains available native appearance evidence and BPR-0059 retains the owner-activated macOS focus transition |
| Build and packaging | Build/test guide and dependency inventory | Updated for repository-pinned static SQLite 3.53.4, the exact 18-table/16-index schema, platform storage/runtime-port boundaries, and dedicated core/host/history/maintenance/Qt commands. BPR-0047 selects those existing dependencies in the public executable and adds test-only two-process orchestration; it adds no GitHub Actions, dependency, schema, package format, or runtime option | Focused application/core/Git/SQLite/Qt commands, tests-disabled Release graph/binary inspection, and dependency/architecture scans; no Windows execution is claimed |
| Tests and fixtures | Fixture catalog and test guide | Updated through BPR-0048: BPR-0045 handoff/runtime, BPR-0055 maintenance, BPR-0056 bounded pages, BPR-0046 stable Qt model/selector/render coverage, BPR-0047 public seed/reuse, and BPR-0048's manifest-bound sibling-head force-push, exact rendered cause, issued-transport revocation, sanitizer matrix, and slower-host fixture timing disposition | `epoch_history_fixture_contract`, `core_epoch_policy`, `application_epoch_contract`, `application_epoch_host`, `application_github_host`, `epoch_maintenance_integration`, `application_epoch_persistence_*`, `qt_epoch_comparison_scale_*`, native smart transport, Release composition, architecture scans, documentation gate, and simplification review |
| No-impact/deferred regions | RFC ledger, architecture, issue board | Revalidated by BPR-0051: BPR-0047 wires the interim BPR-0046 control but makes no final visual-polish or selectable-theme claim. BPR-0048 supplies the broad combined matrix, BPR-0049 available native UX, BPR-0059 the owner-activated macOS focus transition, and BPR-0053 real Windows evidence. No GitHub mutation, local review-receipt/actionability policy, RFC4 diff/line mapping, RFC5 final workbench/polish, RFC6 themes/syntax/fonts, queue/AI/notification, CLI enablement, or package-format/signing/installer implementation is claimed | Immutable-base path/target audit, target/dependency scans, and documentation anchors reviewed by BPR-0051 |
| GitHub Actions | Build/test/package/release documentation | Updated and verified through completed BPR-0044: absolute prohibition and no workflow files | Documentation validator and tracked/on-disk workflow scans are 0/0; repeat at RFC closeout |

## Performance and projection plan

- Core state remains fixed-size apart from bounded transition storage and
  retains no catalog, ref list, Git object, pack, blob, or path.
- The catalog port and BPR-0056 core/host path expose at most 100 history rows
  per revision-bound request. Core state retains only the accepted
  catalog/continuation and never a page vector.
- BPR-0045 retains at most 1,000 repository actors. Each exact repository
  authority has one bounded FIFO and one PR cache session at a time, while
  unrelated repositories can progress independently.
- Source collision retry is bounded to 32 attempts, prepared observations to
  100, and one recovery scan to 1,000 operations in pages of 100.
- Routine epoch append or availability change performs changed-set catalog and
  Qt work, not a whole-PR scan or model reset.
- Smart protocol and process streams use bounded buffers/backpressure and
  direct typed in-process calls rather than JSON/RPC.
- Hard quotas fail explicitly. They never silently prune a retained epoch.

## Explicit deferrals and no-impact regions

- RFC-BPR-0004 owns diff computation, line mapping, change groups, blob-to-line
  presentation, and external diff replacement. RFC-BPR-0003 only ensures exact
  Git object residency and comparison identity.
- RFC-BPR-0005 owns the final workbench and virtualized review surface,
  including its mandatory dedicated presentation-polish/native visual gate.
- RFC-BPR-0006 owns selectable themes, fonts, syntax, and high-contrast
  profiles. Fresh install System-default behavior is a mandatory planning
  requirement there.
- RFC-BPR-0007 owns private receipts/actionability. Material equivalence is an
  opaque RFC-BPR-0003 fact and does not migrate receipts.
- GitHub writes remain RFC-BPR-0008. Queue, observers, notifications, and AI
  remain their later RFCs.
- Positive GitHub CLI enablement remains deferred RFC-BPR-0015. Installed Git
  is a different local object/protocol tool and receives no credential or
  network authority.
- BPR-0055 completes core-authorized prune and cache-delete maintenance host
  paths, including exact destructive recovery; it adds no Qt confirmation or
  command surface. BPR-0056 completes history paging
  through core, host, and projection; BPR-0046 owns the Qt trigger and row
  model that consume it.
- Package formats, signing, notarization, installers, and distribution
  certification remain RFC-BPR-0014, while accurate new dependency/runtime
  accounting is required now.
- No QML, WebEngine, browser, product-owned listener, plugin, telemetry,
  updater, or GitHub Actions implementation is introduced.

## Clean closeout blockers

RFC-BPR-0003 cannot become `Auto-Verified` until:

- RFC-BPR-0002 is `Auto-Verified` and every RFC-BPR-0003 card is in `done/`;
- clean Qt-enabled `dev`, Qt-free `core-only`, sanitizer, and tests-disabled
  Release matrices pass;
- core/host/Git/transport/catalog/Qt/composition and architecture boundaries
  pass from the immutable base;
- exact force-push retention and restart recovery succeed without the old
  object remaining remotely reachable;
- target-native macOS, Linux, and real Windows process/network/cache/catalog/
  runtime/dependency/Qt evidence is complete;
- every claim and documentation-ledger row is resolved with reproducible,
  falsifiable, secret-free evidence;
- the original source-pack validator, workflow-absence scans, and
  `git diff --check` pass.
