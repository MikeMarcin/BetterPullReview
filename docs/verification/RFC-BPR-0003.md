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
| Git adapter | Validated installed-Git image, isolated `QProcess`, app-owned bare repositories, controlled pack indexing, exact refs/OIDs, object residency, ancestry/material facts, process containment | Real local Git fixtures, hostile configuration/process/path canaries, binary/runtime inspection |
| Native Git smart transport | Minimal app-owned protocol-v2 discovery/fetch client and credential-bearing HTTPS requests under the exact RFC-BPR-0002 profile/account authority; exact refs/wants, authority-scoped haves, streaming bounds, and no redirects/cookies | Loopback TLS/proxy/protocol fixtures and exact request/body/header/authority counters |
| Epoch catalog adapter | Versioned transactional catalog, ordinal chronology, availability, pending commit/recovery, exact pruning/deletion state, no source or credential content | Disposable catalog fixtures, transaction-failure and crash-recovery tests, exact cleanup |
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
(`initial_observation`, `live_observation`, or
`explicit_historical_import`), base transition (`not_applicable`,
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

The separate streaming transport implements a minimal app-owned protocol-v2
client. Its `ls-refs` request names exactly the accepted base-branch and
pull-request-head prefixes, requires exactly those two advertised refs and
provider-approved OIDs, and admits only the deduplicated accepted base/head
`want` set plus bounded haves derived from the same physical authority scope.
Known optional capabilities may be advertised and ignored; unrequested
capability use and alternate retrieval paths are rejected. Pack bytes enter
installed Git only through this fixed local operation:

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

Private refs are retention roots. A hard quota refuses new material rather
than silently deleting history. Pruning and repository deletion are explicit
exact-target operations that first record pending durable intent, enumerate
affected history, update refs/catalog availability, and recover or finish
after interruption. No glob or broad credential/cache deletion is permitted.

## Claim matrix

| Claim | Direct surface | Proof | Result |
|---|---|---|---|
| Core policy is deterministic and compact | Qt-free epoch reducer | Exhaust cause, alias, pair, idempotence, overflow, replay, and mismatch tables; inspect state size/dependencies | Verified in BPR-0041: core-only 20/20, focused core/architecture 2/2, explicit catalog-incarnation/projection-clear/session canaries, independent final review |
| PR/epoch authority is exact | Core/host/cache/catalog contexts | Cross profile revision, host, account, repository, PR, snapshot, generation, catalog incarnation, and catalog revision | Planned |
| App-owned bare cache is created and reused safely | Real Git adapter and temporary app-data root | Open fixture twice under one authority, then cross account/profile revision; inspect exact manifest/config/path/object format, reuse, and physical isolation | Planned |
| Installed Git receives no credential or network authority | Process and app-owned smart-transport canaries | Inspect exact protocol-v2 requests, fixed `index-pack` image/argv, environment, cwd, config, streams, descendants, sockets, native-HTTPS counters, bounded same-scope haves, and rejected alternate retrieval capabilities | Planned |
| Old observed head survives force-push | Local smart remote, cache, refs, catalog, public selector | Remove/prune old remote object, restart, and compare retained old head to latest | Planned |
| Repository behavior cannot execute | Hostile Git/config/repository fixtures | Arm hooks/helpers/askpass/filters/LFS/submodule/textconv/ext-diff/protocol canaries; require zero invocation | Planned |
| Epoch causes remain truthful | Core cause table with real Git facts | Replay initial, descendant upload, non-fast-forward rewrite, base-only, branch-change, simultaneous-change, equivalent, and unknown-lineage cases | Planned |
| Catalog/ref commit and recovery agree | Fault-injected catalog and Git refs | Fail or interrupt every phase; restart; require the catalog visibility authority to expose one exact committed epoch or an unavailable/recoverable pending state, never a ref-derived false success | Planned |
| Defaults and arbitrary pairs use exact OIDs | Core + Qt + public application | Verify Whole PR and selected IDs/OIDs/context agree; accept same-authority reverse and identical pairs, with identical OIDs producing empty comparison | Planned |
| Named aliases resolve exactly | Catalog/core/public application | Verify Whole PR, nearest preceding distinct live head, newest ordered current-viewer review, and an association-only review update on an unchanged head; make the exact alias unavailable instead of falling back when its candidate is unavailable | Planned |
| Pruned and never-observed history is honest | Catalog/core/Qt | Explicit prune/delete and unreachable unknown head stay visible/unavailable and never silently remap | Planned |
| Stale/canceled work cannot publish | Manual executor and real process cancellation | Reorder/duplicate/cancel Git, transport, catalog, and identity-switch completions | Planned |
| Qt projection is incremental, accessible, and system-native | Stable-ID model and real widgets | Assert exact signals, persistent indexes, focus, keyboard, OID text, announcements, responsive layout, zero routine resets, and live native palette/style/font inheritance | Planned |
| Public composition selects production pieces | Real executable | Recorded GitHub plus local Git protocol fixture reaches exact visible pair; second invocation proves persistence; Release excludes seams | Planned |
| Resource use is bounded without silent retention loss | Protocol/process/cache/catalog/history fixtures | Exceed packet, stream, pack, disk, ref, row/page, output, time, and process limits; require bounded failure and no automatic prune | Planned |
| Platform claims are target-native | macOS, Linux, and Windows | Run configure/build/tests, Git/process/network/cache/catalog/Qt/runtime/dependency evidence on each claimed target | Planned |
| GitHub Actions is absent | Repository and documentation | Tracked/on-disk workflow scans plus documentation validator | Planned |

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
| Host/Git/transport boundary | System architecture, Git/epoch implementation, security model | Selected: minimal app-owned protocol-v2 client, native smart HTTPS, fixed credential-free Git pack indexing, cache/ref/catalog authority, staging, cancellation, recovery, and lifecycle | Fake and real process/transport/catalog fixtures plus boundary/runtime scans |
| Cache privacy and operations | Git/epoch architecture, build/test and operator guidance | Selected: account/profile-revision physical isolation, opaque scopes, permissions, quotas, retention, explicit prune/delete, and recoverable crash states | Hostile path/config/recovery tests and exact filesystem/ref/catalog inspection |
| Epoch semantics | Product/UX reference and verification ledger | Selected: composite causes, unknown lineage, chronology, equivalence, exact anchors, aliases without fallback, reverse/identical pairs, and unavailable history | Core/host/public fixture matrix |
| Qt presentation | Workbench and appearance/accessibility documents | Selected: reusable selectors, exact OIDs, states, incremental model, accessibility, and native System inheritance; no final-workbench claim | Qt signal/state/raster/native appearance evidence |
| Build and packaging | Build/test guide and dependency inventory | Planned: installed Git, Qt Core/QProcess, native transport, catalog/runtime dependencies per platform | Target graph and binary/runtime inspection |
| Tests and fixtures | Fixture catalog and test guide | Planned: deterministic Git histories, loopback protocol, hostile canaries, retained evidence limits | Fixture validator and focused/full/sanitizer matrices |
| No-impact/deferred regions | RFC ledger, architecture, issue board | Planned: no GitHub mutation, receipt/actionability, RFC4 diff/line mapping, RFC5 final workbench, RFC6 themes/syntax/fonts, queue/AI/notification, CLI enablement, package-format/signing/installer implementation | Immutable-base path/target audit and boundary scans |
| GitHub Actions | Build/test/package/release documentation | Planned: absolute prohibition and no workflow files | Documentation validator and tracked/on-disk absence scans |

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
