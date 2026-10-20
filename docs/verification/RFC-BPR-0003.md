# RFC-BPR-0003 Verification Plan and Ledger

RFC base: `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`

RFC-BPR-0003 closed `Auto-Verified` on 2026-08-28 through BPR-0054, in
canonical order behind RFC-BPR-0002 (closed the same day via
BPR-0035/BPR-0036). The final closeout replay section below records the
fresh macOS, Linux, and consumed Windows evidence, the keyed per-criterion
record, and the independent capability-critical review's cured findings.

BPR-0056 completes the bounded history-page core/host/projection slice, and
BPR-0046 consumes it through the reusable incremental Qt selector model
embedded in the interim loaded-content surface. BPR-0066 closes the later
residency finding by retaining exactly one 100-epoch/200-anchor page, making
newest/older navigation core-authorized, rejecting stale catalog windows, and
proving ordinal-one/newest traversal through core, host, SQLite, Qt, and the
rendered composition. BPR-0052 completes the conservative simplification and
post-change replay. BPR-0047 selects the production Git/cache/catalog/epoch
path and proves two-process persistence; BPR-0048 completes the broad combined
matrix, BPR-0049 supplies available native UX verification, and BPR-0059
completes the owner-activated macOS focus proof.

BPR-0409 adds one bounded, redacted installed-Git diagnostic without changing
core policy or effect ownership. The same exact text now crosses process/cache
construction, cache execution, workspace materialization, the agent terminal,
and the visible Qt panel. Its test-only diagnostic wrapper forwards ordinary
commands to installed Git and fails only one marker-authorized workspace
`pack-objects --revs` call after the real selected diff is ready.

## Architecture allocation

| Region | Selected responsibility | Direct evidence |
|---|---|---|
| Functional core | Exact active PR revision, epoch identity/cause policy, named and explicit comparison selection, alias legality, bounded history-page cursor/limit policy, request/generation acceptance, compact status/commands/deltas | Qt-free transition tables, replay/property tests, state-size assertion, forbidden-dependency scan |
| Application host | Source-free/clock-free accepted-PR handoff, bounded Git/epoch observations, OS entropy and system clock, Git/catalog jobs, revision-bound history pages, staging, serialized completion, cancellation, persistence, cache authority, lifecycle, and incremental projection mapping | Runtime-port contracts plus fake-port/manual-executor tests with reordered, stale, duplicate, failed, canceled, identity-switch, restart, commit-failure, decision-lane projection, and quiescence paths |
| Installed-Git local-process adapter | Validated installed-Git image and exec path, isolated `QProcess`, fixed local initialization/pack/object/ancestry/ref operations, secure completed-spool consumption, exact output and pack/index validation, process containment | BPR-0042 real local Git fixtures, hostile configuration/process/path canaries, exact child inspection, and architecture scans |
| Cache and retained-ref adapter | Authority-partitioned app-owned bare repositories, cache manifest/locking/reuse, completed-spool production, pre-import observations, object residency/connectivity, and paired private-ref retention | BPR-0043 real cache and local protocol fixtures, force-push residency, cross-authority isolation, quota, and exact ref inspection |
| Native Git smart transport | Minimal app-owned protocol-v2 discovery/fetch client and credential-bearing HTTPS requests under the exact RFC-BPR-0002 profile/account authority; exact refs/wants, authority-scoped haves, streaming bounds, and no redirects/cookies | Loopback TLS/proxy/protocol fixtures and exact request/body/header/authority counters |
| Epoch catalog adapter | Versioned transactional catalog, ordinal chronology, availability, pending commit/recovery, exact pruning/deletion state, no source or credential content | BPR-0044 disposable SQLite storage/behavior fixtures, transaction/restart/replay failures, exact schema/identity checks, Release seam scan, and guarded-Windows cross-build |
| Qt presentation adapter | Provider/Git-free stable epoch model, independent selectors, one 100-epoch/200-anchor resident page, exact OIDs, aliases, unavailable/stale states, focus/accessibility, incremental updates, native appearance inheritance | More-than-1,000-page stable-ID model traversal, Qt signal/state/accessibility tests, and deterministic/native rendered automation |
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
authority has one FIFO actor and one exact PR cache session at a time. BPR-0068
makes the retained `Jobs` vector the global admission ledger: 64 jobs by
default and at most 1,024 by contract. Every queued, running, or
decision-completion-pending actor closure, including diff blob materialization,
owns one slot; the slot retires before its terminal can admit dependent work.
Switching PRs closes and reopens the exact session, while unrelated repositories
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

BPR-0068 separately caps ordinary reentrant accept/dispatch ingress at 64 by
default and 1,024 at most. Overflow is rejected before core acceptance, smart
leases are revoked, and one diagnostic is retained for the turn. Matching
queued offers are revoked by invalidation; two fixed priority slots retain the
only simultaneously relevant active/pending invalidations, and a separate
shutdown flag supersedes ordinary work. The similar GitHub-host ingress queue
is explicitly outside this epoch-host slice.

The blob bridge has a separate fail-closed authority guard in addition to
cache-session identity. Only the current application-session and pull-request
authority may materialize through the retained actor. Authority replacement,
matching invalidation, and shutdown revoke that guard synchronously, and the
host checks it before admission and on the actor both before and after cache
access. Returned bytes are discarded if an advisory-cancellation-ignoring
cache crosses revocation. A non-consumable actor-exit reservation keeps result
publication and waiter notification inside the global residency count. The
authorization generation is checked while holding the same mutex through
successful result publication. Matching invalidation performs cancellation
and revocation before its ingress call returns; shutdown closes blob ingress
and cancels jobs before its call returns, including reentrant requests whose
semantic transitions drain at the end of the current decision turn.
All cancellation sweeps use a fixed 1,024-entry control snapshot. They mark
and select jobs under the ledger mutex but invoke `request_stop` only after
releasing it. Nested sweeps exclude already-issued controls, so synchronous
stop callbacks can reenter shutdown, `materialize()`, or counter inspection
without mutex recursion or one-frame-per-job stack growth.

Active append preparation also has explicit replacement admission. The host
pre-admits cleanup and requires its actor submission to succeed before a
different authority is invalidated or the new offer is submitted. A
same-authority success queues the preadmitted cleanup before core acceptance,
leaving the offer terminal reservation for the core-emitted ingestion job.
Empty and observation-only preparation require no actor job. Capacity or
executor refusal therefore preserves the active authority instead of losing
durable staging cleanup. Replacement cancellation skips the protected cleanup
control; direct same- and different-authority append cases record a live stop
token at `abort_recovered_operation` and a durable `aborted` transition.
For a cross-repository replacement, both independent actor submissions must
succeed before active cancellation or invalidation. A sequenced executor that
accepts cleanup and rejects the new repository actor proves the old authority
remains selected while mandatory cleanup can still finish.

The focused component command is:

```sh
cmake --build --preset dev --target \
  test_better_pull_review_application_epoch_contract_tests \
  test_better_pull_review_application_epoch_host_tests
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
terminal cases. BPR-0068 adds small injected limits plus refresh, blob-bridge,
executor-refusal, FIFO, invalidation-priority, and mixed shutdown floods. The
manual executors directly expose job, actor-queue, decision-completion, and
ordinary-ingress counts while no Qt or provider process is involved. Adjacent
real installed-Git, cache, and SQLite
contracts verify the physical and durable port implementations. BPR-0047
selects those implementations through the public executable and proves
second-process cache/catalog reuse for GitHub.com and GHES; BPR-0048 supplies
the broad combined fault/reordering matrix; BPR-0055
completes maintenance authorization and recovery; BPR-0046 completes the Qt
history trigger/model and interim loaded-window control. BPR-0066 replaces
page accumulation with one stable-ID-reconciled resident page, adds separate
core-authorized older/newest requests, retains exact out-of-window selected
OIDs, and invalidates stale catalog windows without accepting reordered
terminals. BPR-0053 retains target-native Windows evidence.

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

## Owner-activated macOS appearance-probe provenance

The BPR-0059 native evidence is a single owner-activated run. It is bound to
the source it executed, not to the current tree, and its rasters were
deliberately never checked in. This section records what a reader needs to
judge and reproduce it.

| Field | Recorded value |
|---|---|
| Card | [BPR-0059](../issues/60-done/BPR-0059-rfc-0003-macos-active-focus-verification.md) |
| Recording commit | `63df236f3732903c55d3ba9b386ed9d83e467234`, 2026-08-01 |
| Probe source identity | `src/app/main.cpp` blob `787ba735278b4f22834fab0e3b8e20f7a9df5515` at that commit |
| Executable | `build/dev/src/app/better_pull_review` from the Qt-enabled tests-enabled `dev` preset |
| Invocation and environment | The rollback-safe shell in [build and test](../development/build-and-test-reference.md#native-appearance-verification): `env -u QT_QPA_PLATFORM -u QT_STYLE_OVERRIDE` plus `--github-fixture github-com --native-appearance-probe --render-evidence <light> --changed-render-evidence <dark>`, with the evidence directory created by `mktemp -d "${TMPDIR:-/tmp}/bpr-native-appearance.XXXXXX"` |
| Selected platform and style | `GITHUB_FIXTURE_QT_PLATFORM:cocoa`, `GITHUB_FIXTURE_QT_STYLE:QMacStyle` |
| Accepted markers | `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK`, both local and global `Light->Dark` transitions, and the signal, semantic-state, pixel-difference, render, and changed-render success markers |
| Raster geometry | Two images at 2200x1440 and the same device-pixel ratio |
| Light raster SHA-256 | `66c1e75d69b8656de8cccde876cec98e0fe865420d41ae085ffda5361c40b2bb` |
| Dark raster SHA-256 | `4d1b79b3237475f63b6dd19e3fdc2c302cb9e7eb7c2a207ba1eb384a7b058354` |
| System appearance | Original System Dark `true`; the shell trap restored `true` after the run |
| Host identity | macOS 15.7.7 build 24G720, arm64, Qt 6.11.1 |

Two limits are deliberate rather than accidental.

First, the host identity above was confirmed on 2026-08-03 from the same
machine and build tree. The run log itself did not capture operating-system or
Qt version strings, so that row is a post-run confirmation of the probe host,
not a value read out of the run. Everything else in the table comes from the
run record or from Git.

Second, the two rasters were retained only in the transient local evidence
directory and are not checked-in artifacts. The attestation that both frames
show the same loaded pull request, the same epoch identities and complete
OIDs, the same `Whole PR` selection, retained focus, and readable but distinct
native palettes was made by the primary reviewer who inspected them during the
BPR-0059 run. No later reader can re-inspect those exact images; the durable
substitutes are the recorded SHA-256 values, the marker set, and the
deterministic checked-in `native_appearance_probe_contract` rejection guard.

A rerun at the current HEAD does not reproduce this run. `src/app/main.cpp`
has changed in five later commits, and the loaded window now also composes the
RFC-BPR-0004 selected-file diff workbench, so a fresh probe renders different
content and yields different raster hashes. Owner-authorized native
System-theme evidence for the current composition belongs to BPR-0083 under
RFC-BPR-0004; it is not a reproduction of BPR-0059 and does not retroactively
extend or invalidate it.

## Branches no local compiler reads

Parameters here are named `aXxx`. A rename to that convention was applied only
to the code the macOS compiler builds. Every `#if defined(_WIN32)` and
`#elif defined(__linux__)` branch in the affected functions kept referring to
the old names, which are not declared there.

**The Windows and Linux builds did not compile at HEAD**, and the whole local
gate stayed green throughout, because the preprocessor removes those branches
before any compiler on this host sees them. BPR-0155 found it and repaired it.

That fact reaches back into this ledger. Two entries below record platform
evidence gathered before the rename:

- **BPR-0044's MinGW compile/link evidence** — 47/47 steps, PE32+ outputs.
- **BPR-0050's Ubuntu 26.04 x86_64 GCC 15.2 matrix** — process, protocol,
  Secret Service, cache, catalog, maintenance, public Qt, dependency, and
  runtime.

Both runs happened. Neither was reproducible at HEAD before BPR-0155. The
Linux one is reproducible again as of 2026-08-17: BPR-0156 ran the matrix on
an Ubuntu 26.04 x86_64 host matching BPR-0050's GCC 15.2, Git 2.53, libsecret
0.21.7, and gnome-keyring 50.

### What BPR-0155 established

| Surface | Result |
|---|---|
| Sources repaired | 9 files, 114 lines: `catalog_database.cpp`, `gh_cli_transport.cpp`, `epoch_runtime_ports.cpp`, `installed_git_process.cpp`, `installed_git_cache.cpp`, and four test files |
| MinGW x86-64 syntax check | 47 non-Qt translation units clean; 8 unreachable without Qt for Windows; 7 unreachable without libsecret, CoreFoundation, or KSyntaxHighlighting headers |
| macOS `dev` build | Unchanged — none of these branches is compiled here |

Compiling is not running. The Windows runtime claims stay in BPR-0053.

### What BPR-0156 re-established

Compiling was indeed not running, and it was not compiling either. Seven of
BPR-0155's nine files are gated behind `BPR_BUILD_QT`, so `core-only` never
reads them; only the `dev` preset proves they build. It did not build until
BPR-0346 repaired a `QJsonValue` initializer that is ambiguous wherever
`std::int64_t` is `long` rather than `long long`. That line sits in no
conditional branch, so `verify_conditional_branches.py` cannot reach it, and
BPR-0155's gate never could have.

| Surface | Result |
|---|---|
| `core-only` build | 358 of 358 edges, warnings as errors |
| `dev` build | 671 of 671 edges; `better_pull_review` links |
| Tests-disabled Release build | Fails under GCC at `-O3`; BPR-0343 |
| BPR-0155's nine files | All compile, seven only under `dev` |
| `getrandom`, `renameat2`, `/proc/self/fd` | Re-established through the epoch, catalog, cache, and process suites |
| Secret Service vault | Re-established against a real gnome-keyring 50.0 owning `org.freedesktop.secrets`; all four `linux_secret_service_*` tests pass |
| `gh` CLI Linux environment | Re-established; `gh_cli_transport` fails only inside the isolation namespace, for the harness reason in BPR-0342 |
| `core-only` suite | 82 of 84 |
| `dev` suite | 175 of 183 |

The two `core-only` failures are pre-existing on `main` and are not
Linux-specific: `platform_configuration_contract` and
`dependency_modernization_contract` assert strings that e5797f3 and fbf0f18
changed without updating them, and they fail on macOS too. e5797f3 also moved
`github_application.cpp` without updating the round-trip contract's pinned
digests, which BPR-0346 re-pinned after reading the full diff since they last
matched.

The remaining `dev` failures are BPR-0341 through BPR-0345, one slow-host
budget overrun of under 0.05%, and the absent WSLg system tray. None of them
contradicts a BPR-0050 claim; they are properties of code and harness written
after it.

`coding-format-check` could not be reproduced: no clang-format Ubuntu 26.04
offers agrees with the tree, and the gate pins no version. BPR-0346's edit was
checked directly instead and introduced no violation.

### The gate

`scripts/verify_conditional_branches.py`, wired as `conditional_branch_identifiers`.

It looks only where the local compiler cannot: inside preprocessor branches
that are false for the host running it. Code the compiler already checks is
left to the compiler. Within those branches it reports a bare `xxx` only where
the enclosing function has a parameter `aXxx` and the branch declares no local
`xxx` — the exact shape a half-finished rename leaves behind. An unrecognised
condition is treated as compiled here, which is the safe direction: the
compiler is then the check.

The division of labour was measured on one file, `epoch_runtime_ports.cpp`,
whose `fill_entropy` has a branch per platform:

| Break | Gate | macOS compiler |
|---|---|---|
| `aBytes` → `bytes` at line 39, inside `#if defined(_WIN32)` | names the line | silent |
| `aBytes` → `bytes` at line 54, inside `#elif defined(__linux__)` | names the line | silent |
| `aBytes` → `bytes` at line 45, inside `#elif defined(__APPLE__)` | silent | `error: use of undeclared identifier 'bytes'` |

Neither covers the other, and together they cover the function. That is the
whole design: the gate reads what the compiler cannot, and defers to the
compiler everywhere else.

The gate proves that a fixed set of drift shapes is absent from the source. It
is not platform verification and must not be read as any.

BPR-0157 added two more shapes after the first repair left the Linux build
broken. A local renamed `finalPath` was still `final_path` in the `__linux__`
branch, which the parameter rule could not see; and a chain declaring
`requested_vfs` in one branch beside `requestedVfs` in another compiled
wherever the second branch was taken, so nothing appeared undefined at all.
Eleven more sites in five files.

None of the three rules relates a name to a *different* name. Had `finalPath`
become `catalogPath`, only a compiler for that platform would object.

### The same rot in the architecture contract

Repairing the sources broke `architecture_boundaries`. Its Installed-Git runner
contract required the pattern `stop\.stop_requested\(\)`, and after the rename
nothing matched.

The runner calls `aStop.stop_requested()` in twelve places. The only line that
still spelled it `stop.` was inside `#elif defined(Q_OS_WIN)`. The contract's
evidence that the runner honours cancellation was one line no compiler on this
host built, while eleven compiled ones counted for nothing.

Every other cancellation pattern in that file already accepted `(stop|aStop)`.
This one now does too.

## Automated-run bounds

Every end-to-end test starts the real application and waits for markers. When
a stage never becomes ready the run has to say so itself, because a process
killed by CTest prints nothing: no stage, no verdict, no evidence. BPR-0119
hit exactly that — a forced-never-ready condition produced a bare 45-second
timeout — and BPR-0131 closes it.

The bound is wall clock, from the moment the run is scheduled to the moment it
gives up. It is not a tick count. A tick budget is only comparable to a CTest
`TIMEOUT` if every tick costs what it costs on an idle machine, and under load
the event loop stretches exactly when the harness clock does not; the previous
6,000-tick budget was nominally 60 seconds and outlasted four of the timeouts
containing it.

`bpr_bound_automated_run` in `tests/end_to_end/CMakeLists.txt` is the only way
these tests get a `TIMEOUT`, so a timeout cannot be changed without restating
the budget beside it. It refuses any pair where the worst case the application
permits itself exceeds three quarters of the harness allowance, and prints the
arithmetic at configure time.

| Test | Timeout | Budgeted runs | Budget | Reserve | Worst case | Three-quarter share | Observed |
|---|---|---|---|---|---|---|---|
| `application_public_path` | 15 s | 1 | 8 s | 2 s | 10 s | 11.25 s | 0.46 s |
| `application_public_path_ghes_cli_unavailable` | 15 s | 1 | 8 s | 2 s | 10 s | 11.25 s | 0.20 s |
| `application_render` | 15 s | 1 | 8 s | 2 s | 10 s | 11.25 s | 0.23 s |
| `application_automated_run_budget` | 30 s | 1 | 5 s | 2 s | 7 s | 22.5 s | 6.39 s |
| `application_github_public_path_github_com` | 60 s | 1 | 40 s | 2 s | 42 s | 45 s | 12.26 s |
| `application_github_public_path_ghes_3_18` | 60 s | 1 | 40 s | 2 s | 42 s | 45 s | 12.18 s |
| `application_diff_workbench_public_path` | 60 s | 1 | 40 s | 2 s | 42 s | 45 s | 13.10 s |
| `application_diff_workbench_shutdown_public_path` | 60 s | 1 | 40 s | 2 s | 42 s | 45 s | 12.13 s |
| `application_native_appearance_probe_contract` | 60 s | 1 | 30 s | 10 s | 40 s | 45 s | 12.95 s |
| `application_epoch_persistence_github-com` | 240 s | 2 | 60 s | 4 s | 124 s | 180 s | 50.04 s |
| `application_epoch_persistence_ghes-3_18` | 240 s | 2 | 60 s | 4 s | 124 s | 180 s | 50.72 s |
| `application_epoch_maintenance_public_path` | 360 s | 4 | 60 s | 8 s | 248 s | 270 s | 54.47 s |

Reserve is everything the budget does not cover: process start, shutdown,
writing evidence, and — for the appearance contract — eight
configuration-failure invocations that exit during argument parsing without
ever scheduling a run. Observed times are whole-test wall clock on macOS
15.7.7 arm64, Qt 6.11.1, idle.

The smallest ratio between a budget and what the run actually takes is 2.4x
(`application_epoch_persistence_*`: 60 s against 25 s per phase). The largest
is 40x (`application_render`). The margin left to CTest after the application
has spent its whole budget is never less than 5 seconds and never less than a
quarter of the timeout.

Driver scripts derive their per-invocation `execute_process` timeouts from the
same budget, so an inner backstop cannot fire before the run's own limit
either.

BPR-0266 restores the focused epoch-refresh public-path assertion after the
conversation drawer began following the selected file. The open projection is
still asserted as five review-wide roots with nine exact identities. Once
`review.cpp` is focused, the refreshed projection must contain exactly three
thread roots and their four comment children, while the same host, repository,
pull request, Base-to-HeadTwo pair, ordinal-two `Force-push/rebase` cause, and
zero whole-model resets remain required. GitHub.com and GHES two-process
persistence both pass this stronger gate. The maintenance course also reaches
its public epoch-prune verdict. BPR-0269 removed its stale receipt-load marker
expectations: all four processes still share one persistent fixture root, while
exact review-ledger seed/reopen load evidence remains owned by the dedicated
review-ledger composition contract.

BPR-0447 corrected what the maintenance course does with a condition that has
not become true yet. `stage::epoch_maintenance_history` called
`fail_automated_run` on the first refusal from `begin_epoch_maintenance_probe`,
while `stage::open` calls the same predicate and discards a false return. Every
condition inside that predicate is established asynchronously, so a refusal on
one tick means "not yet". Under a loaded matrix the restarted pruned-delete
process could have its comparison-resolve cache open cancelled, leaving no
exact comparison accepted; `request_fixture_diff` then refuses, and the run
ended 163 ms into a 60-second budget it never touched — about one run in three
under 40 spinning processes on a 14-core host, and never in isolation. The
stage now polls, so the budget is what bounds a condition that never becomes
true, and the run reports `PUBLIC_PATH_MAINTENANCE_GATE:<name>` beside its
maintenance evidence so an expiry names which of the ten conditions it was
waiting on rather than reporting only `PUBLIC_PATH_STAGE:14`. Measured: three
of six loaded phase-three runs entered that wait for 462, 463 and 466 ticks and
all six passed, and the contract passed ten of ten under the load that had
failed three of seven before.

`application_automated_run_budget` exercises the bound. `--automated-stall-probe`
holds the `open` stage forever, which is the shape of a hang: nothing crashes,
nothing errors, a readiness condition simply never becomes true. The run
reports `PUBLIC_PATH_STAGE:2`, the verdict conjunction, the narrowing groups,
and `PUBLIC_PATH_BUDGET:expired=1 budget_ms=5000 elapsed_ms=5928
stage_ticks=505`, then exits 1 — 6.39 seconds inside a 30-second timeout.

`stage_ticks` is reported because it separates two failures that otherwise
look identical: 505 ticks in 5.9 seconds is a run that was waiting, and a
handful of ticks in the same interval is a run whose event loop was blocked.

Four independent breaks show the assertions are not vacuous. Each was built
successfully before being run:

| Break | Result |
|---|---|
| Budget guard disabled | CTest killed the process at 30.04 s with no markers at all — the original failure, reproduced |
| Expiry recorded but never flagged | `PUBLIC_PATH_BUDGET:expired=0`, regular expression not found |
| Stall moved from `open` to `connect` | `PUBLIC_PATH_STAGE:1`, regular expression not found |
| Fixture budget raised to 50 s under a 60 s timeout | Configure failed: 52,000 ms exceeds the 45,000 ms three-quarter share |

## Claim matrix

| Claim | Direct surface | Proof | Result |
|---|---|---|---|
| Core policy is deterministic and compact | Qt-free epoch reducer | Exhaust cause, alias, pair, idempotence, overflow, replay, and mismatch tables; inspect state size/dependencies | Verified in BPR-0041: core-only 20/20, focused core/architecture 2/2, explicit catalog-incarnation/projection-clear/session canaries, independent final review |
| PR/epoch authority is exact | Core/host/cache/catalog contexts | Cross profile revision, host, account, repository, PR, snapshot, generation, catalog incarnation, and catalog revision | Verified on available component/public surfaces: BPR-0041 covers deterministic core identity; `d903290` and `edd762e` cover accepted refs/OIDs and native lease authority; `2801241` covers physical cache/PR-ref scope isolation; BPR-0044 persists and rejects mismatched durable identities; BPR-0045 verifies source-free handoff binding, host-minted identity, exact PR-session switching, and stale host rejection. BPR-0047 crosses the accepted GitHub decision lane into the exact public epoch authority for both GitHub.com and GHES. BPR-0048 directly acquires the bound handoff transport and proves an authority switch revokes both the lease and issued transport before new provider work |
| App-owned bare cache is created and reused safely | Real Git adapter and temporary app-data root | Open fixture twice under one authority, then cross account/profile revision; inspect exact manifest/config/path/object format, reuse, and physical isolation | Verified for the BPR-0043 component in `2801241`: `git_cache_contract` passed 1/1 in 162.58 seconds with SHA-1/SHA-256 reuse, physical authority isolation, exact identity and topology drift checks, cross-process locking, quota refusal, and substituted-state preservation. BPR-0047 additionally proves public two-process reuse with zero smart calls on restart; BPR-0048 supplies cross-authority combined cases |
| Installed Git receives no credential or network authority | Process and app-owned smart-transport canaries | Inspect exact protocol-v2 requests, fixed `index-pack` image/argv, environment, cwd, config, streams, descendants, sockets, native-HTTPS counters, bounded same-scope haves, and rejected alternate retrieval capabilities | Verified on available targets across BPR-0042/BPR-0043/BPR-0045/BPR-0047/BPR-0050: the fixed child has no credential/network authority, `d903290` admits only the closed protocol acquisition, `edd762e` proves the native HTTPS wire/lease boundary in 19 GHES plus 3 GitHub.com scenarios, `2801241` directly exercises the validated spool/inventory/cache handoff, the host requires direct commit inspection before haves or comparisons, and the public composition selects those pieces. BPR-0050 repeats the process, environment, descendant/socket, smart-protocol, cache, public-runtime, and secret scans on macOS and Ubuntu x86_64. Windows executed in BPR-0053's matrix-10/11 record with `git_process_contract` green target-native, and the 2026-08-28 final replay re-ran the process/protocol selectors fresh on macOS and WSL2 Linux |
| A failing installed-Git step explains itself locally and safely | Process/cache/workspace/application/Qt boundaries | Emit one URL-bearing nonzero-exit diagnostic, require the exact bounded redacted text at every hop, preserve refs/local paths, reject a second text member, and prove the selected-diff Qt terminal appears before any agent process starts | Verified by BPR-0409 on macOS: the augmented selector passed 8/8 in 384.98 seconds, including process and cache factories, cache and workspace mappings, agent `StandardError`, the visible `Finished: workspace_unavailable` terminal, architecture mutations, one workspace start, zero agent-process starts, and quiescence. Success, malformed output, and the stderr-limit outcome remain textless. The Windows wrapper branch is source/conditional-branch checked but is not claimed as target-native Windows execution |
| Old observed head survives force-push | Local smart remote, cache, refs, catalog, public selector | Observe and retain the old head, then remove/prune the remote object, import the replacement, restart, and compare retained old head to latest | Verified on the available public path: `2801241` retains and directly inspects both old and replacement cache revisions after remote pruning; BPR-0044 proves durable multi-epoch chronology/anchor restart round trips; BPR-0045 verifies append/recovery orchestration and direct commit inspection. BPR-0048 adds a manifest-bound sibling-head public graph whose self-contained replacement pack omits the old head, requires the rendered second cause to be `Force-push/rebase`, and proves GitHub.com/GHES seed/reuse reopens and compares the retained old head with zero smart calls |
| Repository behavior cannot execute | Hostile Git/config/repository fixtures | Arm hooks/helpers/askpass/filters/LFS/submodule/textconv/ext-diff/protocol canaries; require zero invocation | Verified on the directly exercised macOS and Ubuntu x86_64 surfaces through hostile real-Git/config state plus exact child image/argv/environment/cwd/port, descendant/socket, cache, smart-transport, and public-composition inspection. The canaries remain absent after BPR-0050's target-native matrix; Windows executed the hostile-fixture process and cache suites green in BPR-0053's matrix-10/11 record, and the 2026-08-28 final replay re-ran them fresh on macOS and WSL2 Linux |
| Epoch causes remain truthful | Core cause table with real Git facts | Replay initial, descendant upload, non-fast-forward rewrite, base-only, branch-change, simultaneous-change, equivalent, unknown-lineage, and retention-rehydration cases | Verified on available surfaces: BPR-0041/BPR-0044 exhaust deterministic core/catalog policy, including one-shot rehydration. BPR-0045 supplies bounded directly inspected Git observations. BPR-0048 maps the remaining scenarios to those exact tables and adds installed-Git proof that the public replacement is non-fast-forward plus a stable Qt-role assertion that ordinal two is `Force-push/rebase` |
| Catalog/ref commit and recovery agree | Fault-injected catalog and Git refs | Fail or interrupt every phase; restart; require the catalog visibility authority to expose one exact committed epoch or an unavailable/recoverable pending state, never a ref-derived false success | Verified on macOS arm64 and Ubuntu 26.04 x86_64: BPR-0044 proves exact typed operation facts, phase/evidence/revision CAS, and idempotent catalog replay. BPR-0045 verifies every recorded append phase, continuation after restart, mandatory-success resolution, and physical/catalog postconditions. BPR-0055 covers every prune and cache-delete durable phase with injected lookup, transition, ref, quarantine, finalization, and cleanup failures, exact authorization replay, and real Git/SQLite composed cleanup. BPR-0048 supplies the broad combined fault matrix; BPR-0047 and BPR-0067 reopen durable state across separate processes through the public composition. Target-native Windows executed in BPR-0053's matrix-10/11 record, and the 2026-08-28 final replay re-ran the composed recovery selectors fresh on macOS and WSL2 Linux at the head carrying BPR-0459's cancellation reclassification |
| Defaults and arbitrary pairs use exact OIDs | Core + Qt + public application | Verify Whole PR and selected IDs/OIDs/context agree; accept same-authority reverse and identical pairs, with identical OIDs producing empty comparison | Verified on the available public path: BPR-0041 proves exact core pair/alias policy, BPR-0044 restores exact anchor IDs/OIDs, BPR-0046 verifies typed Qt behavior, and BPR-0047 drives Whole PR, arbitrary, reverse, and identical selections with complete OIDs through the real composition and after restart |
| Named aliases resolve exactly | Catalog/core/public application | Verify Whole PR, nearest preceding distinct live head, newest ordered current-viewer review, and an association-only review update on an unchanged head; make the exact alias unavailable instead of falling back when its candidate is unavailable | Verified on macOS arm64 and Ubuntu 26.04 x86_64 through core, catalog, Qt, and the public application: BPR-0041 proves semantic candidate selection and the no-fallback policy, and BPR-0044 persists chronology, availability, review order, and association-only updates. BPR-0046 proves Qt dispatch and unavailable handling. BPR-0047 proves Whole PR, Previous upload, and My last reviewed agree with stable rendered OIDs through the public composition and after restart; BPR-0048 supplies the combined association-only and unavailable-candidate cases. Target-native Windows executed in BPR-0053's matrix-10/11 record, and the 2026-08-28 final replay re-ran the core/catalog/Qt/public alias selectors fresh on macOS and WSL2 Linux |
| Pruned and never-observed history is honest | Catalog/core/Qt | Explicit prune/delete and unreachable unknown head stay visible/unavailable and never silently remap | Verified on available targets: BPR-0044 proves durable exact prune/cache-delete facts and idempotent finalization; BPR-0055 proves core authorization, exact paired-ref deletion, whole-cache cleanup, restart recovery, and matching incremental availability projection through fake-port and real Git/SQLite composition. BPR-0046 directly renders pruned, cache-deleted, unavailable, and never-observed states without remapping. BPR-0067 adds the selected executable's real QAction/confirmation/core/host/installed-Git/SQLite/projection path and proves durable `pruned` then `cache_deleted` results across four processes with exact targets, no reset, and no extra provider traffic. Windows executed in BPR-0053's matrix-10/11 record, and the 2026-08-28 final replay re-ran the maintenance integration and public path fresh on macOS and WSL2 Linux |
| Stale/canceled work cannot publish | Manual executor and real process cancellation | Reorder/duplicate/cancel Git, transport, catalog, and identity-switch completions | Verified on macOS arm64 and Ubuntu 26.04 x86_64 at both component and composed boundaries: earlier protocol/process/cache slices reject canceled work and exact lease drift, and `git_process_contract` cancels a blocked real child and kills its descendant process group; BPR-0045 directly verifies pre/post-PONR cancellation policy and rejects stale load, preparation, and comparison terminals before core/projection publication. BPR-0056 rejects mismatched authority/source/catalog/cursor, cancels on revision or identity replacement, and proves a deliberately reordered loaded page cannot publish rows. BPR-0066 additionally rejects late older/newest page terminals across file authority, epoch generation, catalog revision, and request identity while retaining a stale visible page until an exact newest load succeeds. BPR-0048 supplies the broad combined fault matrix and BPR-0047 the public composition. Target-native Windows executed in BPR-0053's matrix-10/11 record, whose head carries BPR-0390/BPR-0397's bounded waits and BPR-0459's cancellation reclassification, and the 2026-08-28 final replay re-ran the cancellation-bearing selectors fresh on macOS and WSL2 Linux |
| Qt projection is incremental, accessible, and system-native | Stable-ID model and real widgets | Assert exact signals, persistent indexes, focus, keyboard, OID text, announcements, responsive layout, bounded resident rows, zero routine resets, and live native palette/style/font inheritance | Verified on available targets: BPR-0046 and refreshed BPR-0049 three-scale actual-window cases pass with exact insert/remove/move/data signals, persistent anchor indexes, selector and QAction translation, 40/64-character OIDs, explicit states, focus/accessibility, deduplicated bounded announcements, live font invalidation, and zero routine resets. BPR-0066 repeatedly replaces more than 1,000 pages while retaining at most 100 epochs/200 anchors, reaches ordinal one and newest, preserves or legally invalidates persistent indexes by stable identity, and renders the real 100/200-row composition at 1.0x/1.25x/2.0x. BPR-0049 directly renders the current real application through unforced Ubuntu XCB/QFusionStyle. BPR-0059's owner-activated Cocoa/QMacStyle run adds the required active-window/existing-focus proof: exact epoch identities, OIDs, Whole PR selection, accessibility, readable palettes, and zero resets survive the same-process System Light-to-Dark transition in distinct inspected 2200x1440 rasters. Real Windows UI execution landed in BPR-0053's matrix-10/11 record — the three-scale Qt selectors and `qt_appearance_profile` green target-native — and the 2026-08-28 final replay re-ran the three-scale cases fresh on macOS (matrix and sanitizer trees) and WSL2 Linux |
| Public composition selects production pieces | Real executable | Recorded GitHub plus local Git protocol fixture reaches exact visible pair; second invocation proves persistence; Release excludes seams | Verified on available targets: BPR-0047 covers GitHub.com/GHES acquisition, retained comparisons, restart, quiescence, and tests-disabled seam exclusion. BPR-0067 drives the selected executable's public maintenance QAction and confirmation through the same core/host/installed-Git/SQLite/Qt composition, verifies prune and cache-delete effects after separate restarts, and observes zero additional provider/smart calls during maintenance. Windows executed in BPR-0053's complete matrix-10/11 record, and the 2026-08-28 final replay re-ran the public composition contracts fresh on macOS and WSL2 Linux |
| Resource use is bounded without silent retention loss | Protocol/process/cache/catalog/history fixtures | Exceed packet, stream, pack, disk, ref, row/page, host job/completion/ingress, output, time, and process limits; require bounded failure and no automatic prune. Where no admission flood exists, name the exact enforcement site that refuses admission instead of claiming an executed overflow | Verified on the focused BPR-0068 host surface and prior available-platform surfaces: earlier slices cover process/protocol/cache bounds; BPR-0044 covers database/SQLite/catalog/epoch/revision/checkpoint quotas; BPR-0045 bounds source-identity retry, prepared observations, recovery scanning, and repository-actor retention; BPR-0056 covers 1..100 revision-bound page requests with no core row retention. BPR-0066 proves a strict 100-epoch/200-anchor Qt residency cap through more than 1,000 page replacements and a real 100,000-epoch SQLite catalog without losing ordinal-one or newest reachability. BPR-0068 directly exceeds small injected job and ordinary-ingress capacities while effects/decisions are withheld, includes blob materialization in the same ledger, proves core-request quota and blob generic-host failure outcomes, FIFO/terminal priority, lease cleanup, fail-closed retained-cache authorization, projection depth one, reserved cleanup admission, and shutdown quiescence. Its final sequential component and ASan/UBSan replays pass with zero application launches. Exactly two epoch-host capacities carry executed overflow evidence, both through `epoch_host_limits`: the default-64/maximum-1,024 job ledger and the default-64/maximum-1,024 ordinary reentrant ingress FIFO. The 100 prepared observations, 1,000 retained repository actors, 1,000-operation recovery scan in pages of 100, and 32-attempt source-identity retry are enforced admission refusals with direct source and enforcement-site evidence rather than executed floods; one source-identity collision retry is executed by `source_collision_retries_before_core_acceptance`. BPR-0086 adds the deterministic allocation-failure seam, which drives each of the three recoverable admission allocation boundaries to fail and requires zero partial admission, lease revocation, unchanged active authority, exactly one `unavailable` diagnostic, and shutdown quiescence; it also rejects zero deferred-ingress capacity and values above both hard maxima. That seam compiles only under `BUILD_TESTING` and its symbols are in the tests-disabled Release forbidden-byte contract. The explicitly unexecuted production 30-second elapsed idle case remains outside this host-residency proof |
| Platform claims are target-native | macOS, Linux, and Windows | Run configure/build/tests, Git/process/network/cache/catalog/Qt/runtime/dependency evidence on each claimed target | Verified on all three targets. macOS arm64: prior slices and BPR-0050 provide direct process/protocol/native-loopback/cache/catalog/Keychain/public-runtime evidence, BPR-0045 the guarded production CSPRNG/system-clock ports, BPR-0058 the retained-directory canary and `/proc/self/fd` syscall proof, and the 2026-08-28 final replay adds fresh 218/218 dev, 98/98 core-only, and 8/8 sanitizer matrices. Linux x86_64: BPR-0050's full-system QEMU matrix, BPR-0156's re-establishment, and the 2026-08-28 WSL2 replay at `17c8950d` — 208 of 212 dev with every failure attributed to BPR-0310 or BPR-0472 and none to the post-`82c7d1c1` epoch commits, 96/96 core-only, `BINARY_DEPENDENCIES_OK`, `RUNTIME_SURFACE_OK`; WSL2 is not a full-system kernel, so process-lifetime, tray, and filesystem-semantics claims rest on the QEMU provenance. Windows x64: BPR-0036/BPR-0053's target-native record — complete matrix-10 at `8e952928` and matrix-11 at `0d347481` with installed-Git, DACL/junction/long-path, Credential Manager, Job Object, cache/catalog, Qt, PE-import, and runtime evidence — current at closeout per the final-replay delta analysis; MinGW compile/link evidence is superseded by real MSVC execution |
| GitHub Actions is absent | Repository and documentation | Tracked/on-disk workflow scans plus documentation validator | Verified again after BPR-0052: tracked and on-disk workflow counts are 0/0, no source/build integration selects GitHub Actions, and every macOS/Linux build and verification command is local or target-native |

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
| BPR-0409 local Git diagnostics | System architecture, security boundary, issue board, and this verification ledger | Updated: exactly one bounded/redacted `Detail` crosses process/cache construction, cache execution, workspace materialization/cleanup, agent result, and visible Qt terminal. Success and non-diagnostic failures remain textless; refs/local paths remain exact; test-only wrapper authority cannot enter production composition | Eight-test focused selector, exact process/cache/workspace/application/Qt mutations, real selected-diff workspace-failure course, architecture boundary mutations, positive composition, format/documentation gates, and independent RFC re-review |
| Core epoch state and typed flow | Domain/state model and Git/epoch architecture | Updated: exact revision/incarnation, composite cause facts, exact alias/pair policy, request/generation acceptance, BPR-0056 bounded page/cursor/delta policy, BPR-0066 explicit older/newest accepted-window navigation and invalidation, and BPR-0055 digest-bound maintenance intent/request/terminal policy; no page rows or maintenance target vectors are retained | Core paging/cause/selection/maintenance tables, more-than-1,000-page replay, state-size, and dependency scans |
| Host/Git/transport boundary | System architecture, Git/epoch implementation, security model | Updated through BPR-0068: earlier slices harden installed Git, smart transport, cache/ref consumption, and durable catalog facts; BPR-0045 adds the source-free handoff/runtime ports, per-repository FIFO host, exact append/recovery sequencing, physical commit inspection, stale/cancel rejection, and projection/quiescence containment. BPR-0055 composes exact prune/cache-delete authorization and every durable restart phase; BPR-0056 adds revision-bound catalog page execution and sanitized incremental projection; BPR-0066 adds explicit accepted-window replacement/invalidation and prevents new-catalog inserts from mixing into an older snapshot. BPR-0068 makes jobs the global admission ledger, includes the blob bridge with a revocable active-authority guard, separately caps ordinary reentrant ingress, gives invalidation/shutdown fixed terminal priority, and makes smart-transport revocation allocation-free. BPR-0047 adds the narrow accepted-snapshot outlet, revocable account capability, concrete public composition, and quiescent shutdown order | `application_epoch_contract`, `application_epoch_host`, `application_github_host`, `epoch_maintenance_integration`, adjacent real Git/cache/SQLite contracts, and both public persistence cases |
| Cache privacy and operations | Git/epoch architecture, [build/test operator guidance](../development/build-and-test-reference.md#retained-git-history-and-epoch-storage), and product entrypoints | Updated through BPR-0067: `2801241` implements physical authority isolation and exact cache/ref cleanup; BPR-0044 implements durable append/prune/cache-delete facts; BPR-0045 composes append authorization/recovery; BPR-0055 composes core-authorized paired-ref and whole-cache maintenance; BPR-0067 adds the secondary public maintenance menu, default-Cancel confirmations, exact captured targets, durable public restart proof, and cache-delete supersession of prior prune tombstones. README and troubleshooting state exact destructive consequences and prohibit manual storage editing | Hostile cache fixtures, catalog restart/corruption/quota contracts, manual-executor maintenance failures/restarts, real `git_cache_contract`, real Git/SQLite maintenance composition, and `application_epoch_maintenance_public_path` |
| Epoch semantics | [Product contract](../architecture/product-contract.md), product/UX reference, and verification ledger | Updated through BPR-0051: retained observed revisions are a distinct private authority; composite causes, unknown lineage, chronology, equivalence, exact anchors, aliases without fallback, reverse/identical pairs, unavailable history, and GitHub's continued authority over shared current facts remain explicit | Core/host/public fixture matrix and immutable-base authority review |
| Qt presentation | Workbench and appearance/accessibility documents | Updated through BPR-0067: reusable base/head selectors, named actions, exact copyable OIDs, explicit retained/unavailable/stale states, separate older/newest actions, one stable-ID-reconciled 100-epoch/200-anchor page, accessibility, bounded announcements, native System inheritance, owner-active Cocoa/QMacStyle focus retention, and one secondary accessible local-history menu with default-Cancel confirmations; no final-workbench claim | More-than-1,000-page model traversal, real 100/200-row three-scale raster cases, BPR-0049/BPR-0059 native appearance evidence, and BPR-0067 Qt/public maintenance action tests |
| Build and packaging | Build/test guide and dependency inventory | Updated for repository-pinned static SQLite 3.53.4, the exact 18-table/16-index schema, platform storage/runtime-port boundaries, and dedicated core/host/history/maintenance/Qt commands. BPR-0047 selects those existing dependencies in the public executable and adds test-only two-process orchestration; it adds no GitHub Actions, dependency, schema, package format, or runtime option | Focused application/core/Git/SQLite/Qt commands, tests-disabled Release graph/binary inspection, and dependency/architecture scans; no Windows execution is claimed |
| Tests and fixtures | Fixture catalog and test guide | Updated through BPR-0068: prior component, paging, public persistence, force-push, sanitizer, and timing fixtures remain; BPR-0067 adds the four-process selected-executable maintenance contract and exact public prune/delete restart markers; BPR-0068 adds deterministic job/actor/completion/blob/ordinary-ingress floods and executor refusal with explicit counters and terminal-priority proof | `epoch_history_fixture_contract`, `core_epoch_policy`, `application_epoch_contract`, `application_epoch_host`, `application_github_host`, `epoch_maintenance_integration`, `application_epoch_persistence_*`, `application_epoch_maintenance_public_path`, `sqlite_epoch_catalog_behavior_contract`, `qt_epoch_comparison_scale_*`, native smart transport, Release composition, architecture scans, documentation gate, and simplification review |
| No-impact/deferred regions | RFC ledger, architecture, issue board | Revalidated by BPR-0051/BPR-0059: BPR-0047 wires the interim BPR-0046 control but makes no final visual-polish or selectable-theme claim. BPR-0048 supplies the broad combined matrix, BPR-0049/BPR-0059 complete available native UX including owner-active macOS focus, and BPR-0053 retains real Windows evidence. No GitHub mutation, local review-receipt/actionability policy, RFC4 diff/line mapping, RFC5 final workbench/polish, RFC6 themes/syntax/fonts, queue/AI/notification, CLI enablement, or package-format/signing/installer implementation is claimed | Immutable-base path/target audit, target/dependency scans, BPR-0059 rendered native inspection, and documentation anchors reviewed by BPR-0051 |
| GitHub Actions | Build/test/package/release documentation | Updated and verified through completed BPR-0044: absolute prohibition and no workflow files | Post-BPR-0052 documentation validator and tracked/on-disk workflow scans are 0/0 |

### Immutable-base changed-path classification

BPR-0051 classified 186 paths. BPR-0069 reclassifies the complete
`a600ab2c29d2b4088a3f2598dbb817c35af2eafd..HEAD` set, which is 374 paths: 198
added and 176 modified. Every path routes to a tracked `AGENTS.md`, and the
counts below were produced by resolving each changed path to its nearest
ancestor instruction file.

| Owning `AGENTS.md` | Changed paths | Disposition |
|---|---|---|
| `docs/` | 86 | Updated. These are the reviewed documents themselves: architecture, UX, security, verification, RFC text, operator guidance, and the issue board, carried by BPR-0051, BPR-0066, BPR-0067, BPR-0068, BPR-0081, BPR-0084, BPR-0085, and this card |
| `tests/` | 79 | Updated. Fixture catalog and test guidance cover the added epoch, maintenance, residency, diff, and coding-standard suites; `tests/AGENTS.md` itself was updated by BPR-0085 |
| repository root | 67 | Updated. Build, toolchain, coding-standard, vendored-dependency, and agent-instruction paths owned by BPR-0084 and BPR-0085. The deleted tracked SQLite amalgamation is a build-input change reflected in the dependency inventory; it does not change the epoch catalog schema or storage contract |
| `src/application/` | 36 | Updated through BPR-0068 for the epoch host, ports, and limits. The diff host and diff runtime ports added in the same directory are RFC-BPR-0004 surfaces recorded by BPR-0081 |
| `src/adapters/github/` | 29 | Updated. Four commits touch this scope in the range: `edd762e` adds the native Git smart transport and its lease boundary, `15e7842` adds the accepted-snapshot outlet for BPR-0047, `bcde3b9` is BPR-0084 build modernization, and `0ec1136` is BPR-0085 formatting and naming. The first two are RFC-BPR-0003 surfaces documented by the Git/epoch model and security model; the provider, credential, and REST/GraphQL surfaces in the same directory remain RFC-BPR-0002's |
| `src/adapters/qt/` | 26 | Updated for the epoch comparison control and history model. The alignment axis, scroll controller, link map, and diff workbench are RFC-BPR-0004 surfaces recorded by BPR-0081 |
| `src/core/` | 26 | Updated for the epoch model and transition. The diff core modules are RFC-BPR-0004 surfaces recorded by BPR-0081 |
| `src/adapters/` | 12 | Updated. Installed-Git cache/process and the SQLite epoch catalog remain documented by the Git/epoch architecture and security model. The RFC-BPR-0004 blob-materialization path added by `da39b3a` reaches the installed-Git cache, and its 16 MiB ceiling and authority guard are recorded by BPR-0068. `bcde3b9` changed only `src/adapters/sqlite/CMakeLists.txt`; the catalog schema, VFS, and storage code are untouched at the pinned 3.53.4 version |
| `src/app/` | 12 | Updated. Composition, worker order, and the public fixture entry points remain documented by the system architecture and operator guidance |
| `packaging/` | 1 | No-impact. `packaging/README.md` still declares that package formats, signing, and installers remain RFC-BPR-0014 |

The classification is a routing lower bound. The semantic obligations it
produced are recorded in the ledger table above and in the claim matrix, not
in the path counts.

## Performance and projection plan

- Core state remains fixed-size apart from bounded transition storage and
  retains no catalog, ref list, Git object, pack, blob, or path.
- The catalog port and BPR-0056 core/host path expose at most 100 history rows
  per revision-bound request. Core state retains only the accepted catalog,
  navigation, and continuation facts and never a page vector. BPR-0066 keeps
  one accepted page in Qt, so presentation residency is at most 100 epochs/200
  anchors regardless of catalog size or traversal depth.
- BPR-0045 retains at most 1,000 repository actors. BPR-0068 bounds all queued,
  running, and completion-pending epoch-host work with one default-64,
  maximum-1,024 job ledger, including the diff blob bridge. Each exact
  repository authority has one FIFO and one PR cache session at a time, while
  unrelated repositories can progress independently. Ordinary reentrant
  accept/dispatch ingress has its own default-64, maximum-1,024 FIFO; two
  fixed invalidation slots and one shutdown flag remain terminal-priority.
- Source collision retry is bounded to 32 attempts, prepared observations to
  100, and one recovery scan to 1,000 operations in pages of 100.
- Only the job-ledger and ordinary-ingress capacities are injected small and
  actually overflowed by a test. The repository-actor, prepared-observation,
  recovery-scan, and identity-retry bounds are compile-time constants enforced
  by an admission refusal at their single call site; their evidence is source
  and enforcement-site inspection, not an executed flood.
- BPR-0086 adds the deterministic allocation-failure seam. Each of the three
  recoverable allocation boundaries inside job admission is now driven to fail
  directly, and each leaves no partial job, actor entry, or scheduled effect,
  revokes the rejected offer's lease, preserves the active authority, reports
  exactly one `unavailable` diagnostic, and still reaches shutdown quiescence.
  The limit-validation table also rejects zero deferred-ingress capacity and
  values above both hard maxima. The seam is compiled only under
  `BUILD_TESTING` and its symbols are in the tests-disabled Release
  forbidden-byte contract.
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
  paths, including exact destructive recovery. BPR-0067 adds the selected
  public application's secondary confirmation/command surface and four-process
  durable maintenance proof. BPR-0056 completes history paging
  through core, host, and projection; BPR-0046 owns the Qt trigger and row
  model that consumes it, and BPR-0066 bounds that model to one page with
  explicit older/newest navigation.
- Package formats, signing, notarization, installers, and distribution
  certification remain RFC-BPR-0014, while accurate new dependency/runtime
  accounting is required now.
- No QML, WebEngine, browser, product-owned listener, plugin, telemetry,
  updater, or GitHub Actions implementation is introduced.

## Current closeout replay

The BPR-0069 replay supersedes the earlier post-BPR-0066/BPR-0052 record. It
ran serially on macOS 15.7.7 build 24G720 arm64 with Qt 6.11.1 and Git 2.39.5,
from `build/dev` and `build/core-only` removed and reconfigured after BPR-0084
replaced the tracked SQLite amalgamation with verified CPM source acquisition.
The verified source cache was preserved, so the configure resolved SQLite
3.53.4 without a download. It records:

- a clean Qt-enabled developer matrix passing 88/88 in 684.92 seconds and a
  clean Qt-free `core-only` matrix passing 33/33 in 79.36 seconds. Both suites
  are larger than the 67 and 27 of the earlier snapshot because the
  RFC-BPR-0004 pack, BPR-0067, BPR-0068, and BPR-0085 added cases. The earlier
  counts are superseded rather than contradicted;
- the architecture-boundary, architecture-boundary-contract,
  documentation-structure, documentation-issue-graph, and
  platform-configuration gates passing inside both matrices;
- the repository format gate passing across 181 files and the strict
  first-party `clang-tidy` audit passing;
- `DOCUMENTATION_OK`, `BINARY_DEPENDENCIES_OK` and `RUNTIME_SURFACE_OK` against
  the freshly built executable, the original pack validator result `OK: 14
  RFCs, 17 normative documents, architecture v2 enforced`, tracked/on-disk
  GitHub Actions workflow counts of 0/0, and a clean `git diff --check`;
- a focused AddressSanitizer/UndefinedBehaviorSanitizer matrix passing 8/8 in
  7.06 seconds over `core_epoch_policy`, `application_epoch_host`, and the
  three-scale `qt_epoch_comparison` and `qt_diff_workbench` cases. The
  sanitizer was confirmed active rather than silently dropped: the cache
  carries `-fsanitize=address,undefined -fno-omit-frame-pointer`, the test
  binary links `libclang_rt.asan_osx_dynamic.dylib`, and it imports 42
  `__asan` interceptor symbols. This covers the BPR-0070 command projection,
  the BPR-0086 admission allocation-failure seam that deliberately throws
  through `admit_job`, and the BPR-0087 harness changes;
- the tests-disabled Qt-enabled Release composition, which
  `application_composition_contract` builds fresh with `BUILD_TESTING=OFF` and
  `CMAKE_BUILD_TYPE=Release` and scans for forbidden bytes; it passes at HEAD;
- the instrumented 100,000-epoch SQLite catalog work, replayed at HEAD by
  running `sqlite_epoch_catalog_behavior_contract` directly out of the
  sanitizer tree rather than through CTest. It reports
  `sqlite-epoch-catalog-behavior-tests:pass` in 45.82 seconds under
  AddressSanitizer and UndefinedBehaviorSanitizer. Its uninstrumented run
  passes inside the ordinary `dev` matrix in 37.77 seconds;
- BPR-0059's owner-activated Cocoa/QMacStyle run, recorded at its own commit
  and not re-executed here. Its markers, raster hashes, host identity, and the
  exact limits of its attestation are in the probe-provenance section above;
- no real Windows execution: BPR-0036 and BPR-0053 remain intentionally open,
  so their affected claims and the canonical RFC-BPR-0002/RFC-BPR-0003 status
  transitions remain unverified.

## Final closeout replay — 2026-08-28

The BPR-0054 closeout replayed every closeout matrix from clean state on two
live hosts and consumed the retained Windows record with a commit-exact delta
analysis. Code head for the fresh matrices: `27e50ded` (equal to `17c8950d`;
the run's own commits up to the matrices were docs/tracker-only, verified by
`git diff --stat`). One compiled change landed after the matrices:
`196a9926` (BPR-0473) edits the non-Windows branch of one test translation
unit, and its single owning selector, `git_process_contract`, re-ran green in
77.28 s against the rebuilt binary rather than being carried forward. The
independent critical review re-ran the documentation, diff-hygiene,
workflow-absence, conditional-branch, binary-dependency, and runtime-surface
gates at the review head `812487ec`, all green.

### macOS final replay

Host: macOS 15.7.7 build 24G720 arm64, Apple Git 2.39.5, CMake 4.4.2,
clang-format 22.1.8, and the repo-pinned Qt 6.10.3 resolved from the verified
CPM source cache — this replay's fresh trees select the BPR-0084-era pinned
providers, superseding the system Qt 6.11.1 the pre-provider records name.
`build/dev`, `build/core-only`, and the sanitizer tree were removed and
reconfigured; `build/.cpm-cache` was preserved, so SQLite 3.53.4, Qt 6.10.3,
and KSyntaxHighlighting resolved without downloads. Clean here means the
complete first-party graph rebuilt from an empty tree; Qt itself is the
prebuilt pinned provider package from the verified cache, not a
from-source rebuild.

Two failed first attempts preceded passing phases and are disclosed rather
than implied away: the sanitizer configure failed twice — a manual
invocation missing `CPM_SOURCE_CACHE` sent the Qt provider to an absolute
`/bprqt` path, and a retry over the resulting half-configured tree failed at
`CPMAddPackage` — before the tree was removed and a preset-equivalent fresh
configure succeeded in 23.3 s; and the first direct sanitizer-tree catalog
invocation failed to locate its binary (the runner searched for the
registration name where the executable is
`better_pull_review_sqlite_behavior_contract_tests`) before the direct run
passed deterministically. The per-phase runner reuses fixed log filenames,
so failing-attempt output was overwritten by the passing reruns; the phase
status file retains the attempt sequence, and future replays should retain
per-attempt logs.

- `ctest --preset dev`: **100% of 218 passed in 1756.78 s**. A prior same-day
  run passed 217 of 218: `documentation_structure` correctly rejected the
  transient board state in which BPR-0054 had been moved to `30-in-progress/`
  while depending on in-progress BPR-0053 — the closeout card works from
  backlog, the lane move was reverted at `eb59567d`, and the complete matrix
  re-ran green. `verify_documentation.py` re-verified `DOCUMENTATION_OK`
  standalone at `70e5d8c5` after the BPR-0472 tracker slice landed mid-run.
- `ctest --preset core-only`: **100% of 98 passed in 282.04 s**.
- Per-criterion selectors inside the green dev matrix:
  `core_dependency_boundary` 0.21 s; `architecture_boundaries` 7.04 s;
  `architecture_boundary_contract` 63.95 s (130 cases after BPR-0471);
  `conditional_branch_identifiers` 1.06 s; `platform_configuration_contract`
  0.09 s; `core_epoch_policy` 0.06 s; `application_epoch_catalog_contract`
  0.06 s; `application_epoch_contract` 0.06 s; `application_epoch_host`
  0.20 s; `sqlite_epoch_catalog_storage_contract` 0.18 s;
  `sqlite_epoch_catalog_behavior_contract` 39.63 s;
  `qt_epoch_comparison_scale_100/125/200` 0.29/0.28/0.32 s;
  `qt_diff_workbench_scale_100/125/200` 1.62/1.65/1.81 s;
  `epoch_history_fixture_contract` 0.47 s; `git_process_contract` 77.07 s;
  `git_cache_contract` 246.44 s; `epoch_maintenance_integration` 28.69 s;
  `application_composition_contract` 107.15 s, building the tests-disabled
  Qt-enabled Release composition fresh and scanning it for forbidden bytes;
  `application_native_appearance_probe_contract` 26.34 s;
  `qt_appearance_profile` 0.30 s;
  `application_epoch_persistence_github-com` 57.07 s;
  `application_epoch_persistence_ghes-3_18` 56.21 s;
  `application_epoch_maintenance_public_path` 57.50 s;
  `application_review_ledger_composition_contract` 43.88 s;
  `runtime_process_surface` 0.66 s; `application_render` 0.35 s;
  `documentation_structure` 0.54 s.
- Sanitizer matrix, from a freshly removed and reconfigured
  AddressSanitizer/UndefinedBehaviorSanitizer tree
  (`build/rfc3-closeout-sanitize`, Debug, Qt on, the same preset cache
  variables plus `-fsanitize=address,undefined -fno-omit-frame-pointer`):
  **8 of 8 passed in 18.98 s** — `core_epoch_policy` 0.36 s,
  `application_epoch_host` 0.64 s, `qt_epoch_comparison_scale_100/125/200`
  0.86/0.47/0.53 s, `qt_diff_workbench_scale_100/125/200` 5.23/5.41/5.47 s.
  The sanitizer is confirmed active rather than silently dropped: the cache
  carries the flags, the epoch-host test binary links
  `libclang_rt.asan_osx_dynamic.dylib`, and it exposes 1,363 `__asan`
  symbols. The instrumented 100,000-epoch catalog work re-ran directly out of
  the same fresh tree: `better_pull_review_sqlite_behavior_contract_tests`
  reports `sqlite-epoch-catalog-behavior-tests:pass` in 57.46 s under
  ASan/UBSan, and the uninstrumented registration passes inside the dev
  matrix in 39.63 s.
- Gates: `coding-format-check` passes (clang-format 22.1.8), and
  `format_check.py` re-verified the whole tree after the BPR-0473 repair. The
  strict first-party `clang-tidy` audit ran partitioned
  (`tidy.py --warnings-as-errors --paths`, chunk TU counts summed against the
  compile database): 127 of 127 unique `src/` and 119 of 119 unique `tests/`
  translation units, all green after one finding — BPR-0473's redundant empty
  initializer in the non-Windows branch of
  `run_isolation_policy_file_contract`, introduced by `4761afe4` after the
  last full audit; the fix is behavior-identical, and the macOS
  `git_process_contract` row re-ran green in 77.28 s against the rebuilt
  binary rather than being carried forward. `verify_documentation.py` reports
  `DOCUMENTATION_OK`; `verify_binary_dependencies.py` reports
  `BINARY_DEPENDENCIES_OK` and `verify_runtime_surface.py` reports
  `RUNTIME_SURFACE_OK` against the freshly built dev executable.
- The original source-pack validator (repository-external by BPR-0001's
  recorded exclusion) reports `OK: 14 RFCs, 17 normative documents,
  architecture v2 enforced`, re-run this session.
- Workflow-absence and diff hygiene: tracked and on-disk GitHub Actions
  workflow counts are 0/0 and `git diff --check` is clean, verified at the
  evidence head and re-verified at the closeout head.

Final replay record keyed by each closeout criterion, every selector from the
fresh green macOS dev matrix unless stated, with the Linux and Windows
platform records above carrying the same selectors:

- "Opening a fixture PR creates/reuses the correct app-owned bare
  repository." — `git_cache_contract` 246.44 s and `application_epoch_host`
  0.20 s, both Passed.
- "Observed heads receive durable private refs and remain available after the
  simulated remote force-push removes them." — `git_cache_contract` 246.44 s,
  `application_epoch_persistence_github-com` 57.07 s,
  `application_epoch_persistence_ghes-3_18` 56.21 s, all Passed.
- "Base update, author upload, force-push, and base-branch-change cases
  receive distinct cause labels." — `core_epoch_policy` 0.06 s,
  `application_epoch_contract` 0.06 s, `application_epoch_catalog_contract`
  0.06 s, all Passed.
- "Left and right selectors reach all retained epochs through core-authorized
  bounded pages and show exact OIDs even when the selected anchors leave the
  resident page." — `core_epoch_policy` 0.06 s and
  `qt_epoch_comparison_scale_100/125/200` 0.29/0.28/0.32 s, all Passed.
- "`Previous upload -> latest` and `My last reviewed -> latest` resolve to
  expected OIDs." — `core_epoch_policy` 0.06 s, Passed.
- "Git process tests prove hooks, textconv, external diff, submodules,
  filters, and worktree execution are disabled." — target-native:
  `git_process_contract` green on the Windows host in matrix-10 (29.06 s) and
  matrix-11 per BPR-0053's record, consumed with the delta analysis below;
  fresh: 77.07 s in this matrix and 77.28 s re-run against the rebuilt binary
  after BPR-0473, plus the Linux replay's green run, all Passed.
- "Repository cache deletion and ref pruning are explicit user/admin
  operations with documented consequences." — `epoch_maintenance_integration`
  28.69 s and `application_epoch_maintenance_public_path` 57.50 s, both
  Passed.
- "Core epoch-selection tests run without Git or Qt; host tests prove
  Git/ref/time effects and stale completion rejection; Qt tests prove
  selector event translation and projection only." — `core_epoch_policy`
  0.06 s, `application_epoch_host` 0.20 s,
  `qt_epoch_comparison_scale_100/125/200` 0.29/0.28/0.32 s,
  `core_dependency_boundary` 0.21 s, and `architecture_boundaries` 7.04 s,
  all Passed, with the Qt-free `core-only` matrix 98/98.

### Windows delta analysis — the retained record stands

Windows evidence is BPR-0053's and BPR-0036's target-native record on the
designated Windows 11 x64 host against Git for Windows 2.55.0.windows.3:
complete matrix-10 at `8e952928` (2026-08-27, 205 of 208, this card's four
rows green: `git_process_contract` 29.06 s, `git_cache_contract` 34.42 s,
`sqlite_epoch_catalog_storage_contract` 1.20 s,
`sqlite_epoch_catalog_behavior_contract` 85.28 s) and complete matrix-11 at
`0d347481` (2026-08-27, 202 of 208 in-run; every non-passing row attributed
to Smart App Control reputation verdicts on freshly relinked images or to
RFC-BPR-0016 surfaces — no RFC-BPR-0003 suite among them).

All five epoch-surface commits that postdate the last full Linux matrix —
`e355895f`, `4af48e02`, `8666fb54`, `5cb734d4`, `6df4a2aa` — are ancestors of
`0d347481`, so matrix-11 executed them on Windows. The complete non-docs commit set after
`0d347481` is seven commits, classified exactly: `8e175d8a` (BPR-0469) is the
one Windows-compiled production change — it removes a dead file-local
accessor from the Qt diff workbench, and the RFC-BPR-0002 closeout rebuilt
both Windows presets at the merged head `76d18e60`, proving the tree compiles
under MSVC, and reran its twenty-seven compiled selectors, which link the
same Qt adapter library, green against the head binaries. `3cdaba81` and
`27e50ded` change `linux_secret_service.cpp`, which the CMake platform gate
compiles only on Linux, plus the boundary-guard cmake/script; `76d18e60`
renames identifiers in three Linux-only test sources; `ade350dc` and
`3eb2ec35` change the platform-independent boundary-guard and
composed-hosts verification scripts, whose changed behavior this replay's
fresh macOS and Linux matrices execute; and `196a9926` (BPR-0473) edits the
`#else`-of-`Q_OS_WIN` branch of one test translation unit, which the Windows
preprocessor removes. Residual recorded exactly: no RFC-BPR-0003 selector
has executed on Windows after `0d347481`.

### Linux final replay

The concurrent Linux session ran the closeout matrices on 2026-08-28 at
`17c8950d` in the ext4 worktree `/home/mmarcin/bpr-linux`: Ubuntu 26.04 LTS
on WSL2 (kernel 6.18.33.2-microsoft-standard-WSL2 x86_64), GCC 15.2.0, Git
2.53.0, CMake 4.4.2, Qt 6.10.3, libsecret 0.21.7, gnome-keyring 50.0.
`build/dev` and `build/core-only` removed and reconfigured with the source
cache preserved; both graphs built completely (741 of 741 and 392 of 392
targets). This is the first complete Linux matrix since `82c7d1c1`
(2026-08-17, BPR-0156) and therefore the first to execute `e355895f`,
`4af48e02`, `8666fb54`, and `5cb734d4` on Linux — and the first possible one
at all since BPR-0469 showed the Linux `dev` build did not compile at
`13fbc9df`.

- `ctest --preset dev`: **208 of 212 passed in 996 s**. Every failure is
  attributed and none is attributable to the four epoch commits above:
  the three `linux_secret_service_native_*` rows fail on this host's locked
  default collection, owned by BPR-0310 and untested here rather than
  passing — no RFC-BPR-0003 claim rests on the Secret Service path; and
  `application_review_ledger_composition_contract` fails on its
  forced-obstruction course with `epoch_handoff_failed=1`, reproduced
  identically at BPR-0156's own baseline `82c7d1c1` under both 40000 ms and
  150000 ms budgets, now owned by BPR-0472 with BPR-0156's "slower host"
  attribution corrected by measurement. Every selector an RFC-BPR-0003 claim
  row cites passed.
- `ctest --preset core-only`: **96 of 96 passed in 439 s**.
- `verify_binary_dependencies.py` reports `BINARY_DEPENDENCIES_OK` and
  `verify_runtime_surface.py` reports `RUNTIME_SURFACE_OK` against the built
  dev executable (115,643,304 bytes, SHA-256
  `a221637c41e391da68f2c37124e75a8890766aea1cfc9b2b02cbada470f422e5`).
- Four rows BPR-0156 recorded as failing now pass on this host:
  `application_agent_invocation_composition_contract` (was BPR-0341),
  `application_queue_composition_contract` (was BPR-0344),
  `application_epoch_maintenance_public_path` (was BPR-0345), and
  `application_native_notification_attended_contract` (the WSLg tray row) —
  the tray conversion is treated as the environment having changed, not as
  proof the code did.
- Limits carried into the platform row: WSL2 is not a full-system kernel, so
  this run is weaker than BPR-0050's QEMU provenance for process-lifetime,
  tray, and filesystem-semantics claims, and the run logs were transient
  session scratch — the durable evidence is this record and BPR-0472's
  tables.
- Accepted risk, recorded explicitly rather than only by card reference:
  BPR-0472's failing course raises `epoch_handoff_failed` around the
  `deliver_epoch_handoff`/`Epochs->accept` path in
  `github_application_host.cpp`, and the epoch handoff is an RFC-BPR-0003
  surface. Its non-causality table exonerates only the four
  post-`82c7d1c1` epoch commits, not epoch-host code generally: the failure
  is pre-existing on every measured Linux run back to BPR-0156's own
  baseline, occurs only on the forced-obstruction course of an RFC-BPR-0007
  composition no claim row cites, and has never reproduced on macOS or
  Windows. If BPR-0472's root cause lands in RFC-BPR-0003-owned code, the
  affected Linux claim reopens through that card; the integrator closes over
  that named question knowingly.

## Clean closeout blockers

Every blocker below was satisfied at the 2026-08-28 closeout; the
final-closeout-replay section carries the per-item evidence. The list is
retained as the requirements it was:

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

RFC-BPR-0029 governs the later portable update-occurrence layer beside this
closed local epoch model. Its [verification matrix](../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md#verification-matrix)
separately tests two-catalog identity, partial history, lossless migration and
event association; those claims do not retroactively redefine RFC-BPR-0003's
local ordinal or retained-revision evidence.
