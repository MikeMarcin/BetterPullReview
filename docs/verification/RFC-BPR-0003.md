# RFC-BPR-0003 Verification Plan and Ledger

RFC base: `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`

RFC-BPR-0003 remains `Proposed`. RFC-BPR-0002 is still the canonical closure
frontier because BPR-0036 retains real Windows x64 evidence and BPR-0035
retains its final transition. By product-owner direction, that hardware-only
gate does not block RFC-BPR-0003 planning or implementation on available
targets. It does block RFC-BPR-0003 final closeout indirectly: BPR-0054
depends on BPR-0035, so no RFC can become `Auto-Verified` out of order.

## Architecture allocation

| Region | Selected responsibility | Direct evidence |
|---|---|---|
| Functional core | Exact active PR revision, epoch identity/cause policy, named and explicit comparison selection, alias legality, request/generation acceptance, compact status/commands/deltas | Qt-free transition tables, replay/property tests, state-size assertion, forbidden-dependency scan |
| Application host | Typed accepted-PR handoff, bounded Git/epoch observations, Git/catalog jobs, explicit time, staging, serialized completion, cancellation, persistence, cache authority, lifecycle, incremental projection mapping | Fake-port/manual-executor tests with reordered, stale, duplicate, failed, canceled, identity-switch, restart, and commit-failure paths |
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
Git host + canonical hostname
authenticated GitHub account
repository + pull request
provider snapshot + GitHub identity/PR generation
epoch request + epoch generation
catalog incarnation + expected catalog revision
source observation identity
```

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
  -> typed application handoff to epoch host
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

This is component evidence only. BPR-0044 owns durable operation facts,
BPR-0045 owns journal-before-`index-pack`, ref/catalog sequencing, stale host
publication, and restart recovery, BPR-0047 owns selection through the public
BetterPullReview composition, and BPR-0053 owns target-native Windows
execution. No evidence above proves durable authorization, crash recovery,
public composition, Windows behavior, or RFC-BPR-0003 completion.

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

BPR-0044 implements the BPEC v1 durable component: a closed 18-table/15-index
schema, authority/incarnation/revision facts, epoch/review/anchor state, and
typed append, prune, and cache-delete operation records. It persists a
host-issued nonzero 256-bit source-observation identity exactly and rejects
reuse among published epochs in one catalog incarnation. Its deterministic
fixtures prove persistence, freshness relative to prior stored identities,
and collision rejection, not entropy generation. BPR-0045 must mint every
accepted observation identity from operating-system entropy and retry a
catalog collision.

Private refs are retention roots. A hard quota refuses new material rather
than silently deleting history. Pruning and repository deletion are explicit
exact-target operations that first record pending durable intent, enumerate
affected history, update refs/catalog availability, and recover or finish
after interruption. BPR-0044 prune facts admit only available, unavailable,
or corrupt starting anchors and retain exact expected availability, OIDs, ref
identity, and revision through begin/replay/finalize/restart. Catalog
finalization atomically marks both anchors and linked non-destructive review
states pruned and releases logical quota exactly once. BPR-0045 still owns the
real paired-ref deletion. No glob or broad credential/cache deletion is
permitted.

## Claim matrix

| Claim | Direct surface | Proof | Result |
|---|---|---|---|
| Core policy is deterministic and compact | Qt-free epoch reducer | Exhaust cause, alias, pair, idempotence, overflow, replay, and mismatch tables; inspect state size/dependencies | Verified in BPR-0041: core-only 20/20, focused core/architecture 2/2, explicit catalog-incarnation/projection-clear/session canaries, independent final review |
| PR/epoch authority is exact | Core/host/cache/catalog contexts | Cross profile revision, host, account, repository, PR, snapshot, generation, catalog incarnation, and catalog revision | Partially verified: BPR-0041 covers deterministic core identity; `d903290` and `edd762e` cover accepted refs/OIDs and native lease authority; `2801241` covers physical cache/PR-ref scope isolation; BPR-0044 directly persists and rejects mismatched repository-cache/PR authority, incarnation, revision, ordinal, operation, ref, and source-observation identities. Host publication and public selection remain BPR-0045/BPR-0047 |
| App-owned bare cache is created and reused safely | Real Git adapter and temporary app-data root | Open fixture twice under one authority, then cross account/profile revision; inspect exact manifest/config/path/object format, reuse, and physical isolation | Verified for the BPR-0043 component in `2801241`: `git_cache_contract` passed 1/1 in 162.58 seconds with SHA-1/SHA-256 reuse, physical authority isolation, exact identity and topology drift checks, cross-process locking, quota refusal, and substituted-state preservation. Durable catalog authorization and public composition remain later slices |
| Installed Git receives no credential or network authority | Process and app-owned smart-transport canaries | Inspect exact protocol-v2 requests, fixed `index-pack` image/argv, environment, cwd, config, streams, descendants, sockets, native-HTTPS counters, bounded same-scope haves, and rejected alternate retrieval capabilities | Partially verified across BPR-0042/BPR-0043: the fixed child has no credential/network authority, `d903290` admits only the closed protocol acquisition, `edd762e` proves the native HTTPS wire/lease boundary in 19 GHES plus 3 GitHub.com scenarios, and `2801241` directly exercises the validated spool/inventory/cache handoff. Public composition remains open |
| Old observed head survives force-push | Local smart remote, cache, refs, catalog, public selector | Observe and retain the old head, then remove/prune the remote object, import the replacement, restart, and compare retained old head to latest | Partially verified: `2801241` retains and directly inspects both old and replacement cache revisions after remote pruning; BPR-0044 proves exact durable multi-epoch chronology/anchor restart round trips. BPR-0045 must compose those facts with real refs and BPR-0047 must expose the public selector |
| Repository behavior cannot execute | Hostile Git/config/repository fixtures | Arm hooks/helpers/askpass/filters/LFS/submodule/textconv/ext-diff/protocol canaries; require zero invocation | Verified for the directly exercised macOS BPR-0042 local-process surface through hostile real-Git state plus exact child argv/environment/cwd/port inspection; Linux/Windows and end-to-end transport/cache evidence remain open |
| Epoch causes remain truthful | Core cause table with real Git facts | Replay initial, descendant upload, non-fast-forward rewrite, base-only, branch-change, simultaneous-change, equivalent, unknown-lineage, and retention-rehydration cases | Verified for deterministic core/catalog-component policy in BPR-0041/BPR-0044, including the one-shot durable rehydration requirement and explicit cause; BPR-0045 still must supply real accepted Git facts |
| Catalog/ref commit and recovery agree | Fault-injected catalog and Git refs | Fail or interrupt every phase; restart; require the catalog visibility authority to expose one exact committed epoch or an unavailable/recoverable pending state, never a ref-derived false success | Partially verified in BPR-0044: exact typed operation facts, phase/evidence/revision CAS, abort/repair/finalized replay, restart paging, and idempotent catalog finalization passed. BPR-0045 retains real ref mutation, two-resource ordering, artifact cleanup, and stale publication |
| Defaults and arbitrary pairs use exact OIDs | Core + Qt + public application | Verify Whole PR and selected IDs/OIDs/context agree; accept same-authority reverse and identical pairs, with identical OIDs producing empty comparison | Partially verified: BPR-0041 proves exact core pair/alias policy and BPR-0044 restores exact anchor IDs/OIDs; BPR-0046/BPR-0047 retain Qt and public application evidence |
| Named aliases resolve exactly | Catalog/core/public application | Verify Whole PR, nearest preceding distinct live head, newest ordered current-viewer review, and an association-only review update on an unchanged head; make the exact alias unavailable instead of falling back when its candidate is unavailable | Partially verified: BPR-0041 proves semantic candidate/no-fallback policy, and BPR-0044 persists chronology, availability, review order, and association-only updates. BPR-0046/BPR-0047 retain rendered/public agreement |
| Pruned and never-observed history is honest | Catalog/core/Qt | Explicit prune/delete and unreachable unknown head stay visible/unavailable and never silently remap | Partially verified in BPR-0044: exact available/unavailable/corrupt prune facts survive replay/restart; finalization atomically prunes both anchors and linked review state and decrements quota once. Real ref deletion and Qt/public presentation remain BPR-0045/BPR-0046/BPR-0047 |
| Stale/canceled work cannot publish | Manual executor and real process cancellation | Reorder/duplicate/cancel Git, transport, catalog, and identity-switch completions | Partially verified at component boundaries: `d903290` aborts its pack sink on canceled/failed acquisition, `edd762e` verifies cancellation and exact lease revocation/isolation, BPR-0042/`b65b0a9` cover process cancellation/cleanup, and `2801241` covers pre-canceled cache operations plus abort/finish and import-terminal races without resurrection. BPR-0045 still owns stale-result rejection across host publication |
| Qt projection is incremental, accessible, and system-native | Stable-ID model and real widgets | Assert exact signals, persistent indexes, focus, keyboard, OID text, announcements, responsive layout, zero routine resets, and live native palette/style/font inheritance | Planned |
| Public composition selects production pieces | Real executable | Recorded GitHub plus local Git protocol fixture reaches exact visible pair; second invocation proves persistence; Release excludes seams | Planned |
| Resource use is bounded without silent retention loss | Protocol/process/cache/catalog/history fixtures | Exceed packet, stream, pack, disk, ref, row/page, output, time, and process limits; require bounded failure and no automatic prune | Partially verified: BPR-0042/`b65b0a9`, `d903290`, `edd762e`, and `2801241` cover process/protocol/cache bounds; BPR-0044 covers the 1-GiB main-database limit, fixed SQLite limits, bounded history/recovery/anchor/prune/cache-delete requests, 100,000 epochs per PR, checked revision reserve, sustained WAL checkpointing, and zero-mutation quota failures. The production 30-second elapsed idle case remains open |
| Platform claims are target-native | macOS, Linux, and Windows | Run configure/build/tests, Git/process/network/cache/catalog/Qt/runtime/dependency evidence on each claimed target | Partially verified: prior slices have direct macOS process/protocol/native-loopback/cache evidence; BPR-0044 adds direct macOS catalog storage/behavior, ASan/UBSan, and Release dependency evidence. Its clean MinGW PE cross-build is compile/link evidence only. Linux remains planned and every affected Windows runtime claim remains open in BPR-0053 |
| GitHub Actions is absent | Repository and documentation | Tracked/on-disk workflow scans plus documentation validator | Verified through completed BPR-0044: tracked and on-disk workflow counts are 0/0 and no source/build integration selects GitHub Actions; repeat at RFC closeout |

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
| Core epoch state and typed flow | Domain/state model and Git/epoch architecture | Selected: exact revision/incarnation, composite cause facts, exact alias/pair policy, request/generation acceptance, and deltas | Core tables, replay/property tests, state/dependency scans |
| Host/Git/transport boundary | System architecture, Git/epoch implementation, security model | Updated through committed component work: `b65b0a9` hardens installed Git, `d903290`/`edd762e` implement closed protocol-v2/native smart HTTPS, `2801241` implements the authority cache/ref consumer, and BPR-0044 supplies exact durable operation/catalog facts. BPR-0045 retains sequencing/lifecycle and BPR-0047 public composition | BPR-0044 fresh dev 58/58, focused repeat 20/20, core-only 25/25, and ASan/UBSan 4/4; composed runtime remains pending |
| Cache privacy and operations | Git/epoch architecture, build/test and operator guidance | Updated: `2801241` implements physical authority isolation and exact cache/ref cleanup; BPR-0044 implements durable append/prune/cache-delete intent, exact replay, repair provenance, tombstones, one-shot retention rehydration, and logical quota release. BPR-0045 retains real two-resource orchestration | Hostile cache fixtures plus BPR-0044 storage/behavior restart, corruption, quota, prune, delete, and rehydration contracts |
| Epoch semantics | Product/UX reference and verification ledger | Selected: composite causes, unknown lineage, chronology, equivalence, exact anchors, aliases without fallback, reverse/identical pairs, and unavailable history | Core/host/public fixture matrix |
| Qt presentation | Workbench and appearance/accessibility documents | Selected: reusable selectors, exact OIDs, states, incremental model, accessibility, and native System inheritance; no final-workbench claim | Qt signal/state/raster/native appearance evidence |
| Build and packaging | Build/test guide and dependency inventory | Updated for repository-pinned static SQLite 3.53.4, exact compile/runtime policy, BPEC schema, platform storage boundary, and isolation of Git-cache fault injection in its dedicated contract target. Public composed runtime and platform-wide inspection remain later work | Fresh tests-disabled Release 49/49, zero registered tests, zero catalog seam symbols/strings, static SQLite/no dynamic libsqlite; tests-on to tests-off reconfigure leaves zero Git-cache fault definitions; MinGW cross-build 47/47 with zero Windows execution |
| Tests and fixtures | Fixture catalog and test guide | Updated for the BPR-0044 storage and behavior contracts, deterministic source-ID persistence/collision evidence, exact prune replay, cache-delete rehydration, schema/bootstrap hostility, and cross-vs-native Windows boundary | Fresh dev 58/58; focused repeat 20/20; core-only 25/25; ASan/UBSan 4/4; architecture mutations 104/104; independent correctness and simplification reviews clean |
| No-impact/deferred regions | RFC ledger, architecture, issue board | Recorded: BPR-0044 supplies durable facts; BPR-0045 owns OS-entropy observation IDs and journal-before-import/ref orchestration; BPR-0047 owns public composition; BPR-0053 retains target-native Windows evidence. No GitHub mutation, RFC4 diff/line mapping, RFC5 final workbench, RFC6 themes/syntax/fonts, queue/AI/notification, CLI enablement, or package-format/signing/installer implementation is claimed | Immutable-base path/target audit and boundary scans |
| GitHub Actions | Build/test/package/release documentation | Updated and verified through completed BPR-0044: absolute prohibition and no workflow files | Documentation validator and tracked/on-disk workflow scans are 0/0; repeat at RFC closeout |

## Performance and projection plan

- Core state remains fixed-size apart from bounded transition storage and
  retains no catalog, ref list, Git object, pack, blob, or path.
- Catalog history is paged/incremental; core sees only active pair and bounded
  alias candidates.
- Fetch/pack/object work is keyed by exact authority and coalesced per
  repository/revision. One writer serializes durable mutation without
  serializing unrelated repositories.
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
