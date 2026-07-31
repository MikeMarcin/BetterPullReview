# RFC-BPR-0003 Verification Plan and Ledger

RFC base: `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`

RFC-BPR-0003 remains `Proposed`. RFC-BPR-0002 is still the canonical closure
frontier because BPR-0036 retains real Windows x64 evidence and BPR-0035
retains its final transition. By product-owner direction, that hardware-only
gate does not block RFC-BPR-0003 planning or implementation on available
targets. It does block RFC-BPR-0003 final closeout indirectly: BPR-0054
depends on BPR-0035, so no RFC can become `Auto-Verified` out of order.

## Architecture allocation

| Region | Planned responsibility | Direct evidence |
|---|---|---|
| Functional core | Exact active PR revision, epoch identity/cause policy, named and explicit comparison selection, alias legality, request/generation acceptance, compact status/commands/deltas | Qt-free transition tables, replay/property tests, state-size assertion, forbidden-dependency scan |
| Application host | Typed accepted-PR handoff, bounded Git/epoch observations, Git/catalog jobs, explicit time, staging, serialized completion, cancellation, persistence, cache authority, lifecycle, incremental projection mapping | Fake-port/manual-executor tests with reordered, stale, duplicate, failed, canceled, identity-switch, restart, and commit-failure paths |
| Git adapter | Validated installed-Git image, isolated `QProcess`, app-owned bare repositories, local pack/protocol operations, exact refs/OIDs, object residency, ancestry/material facts, process containment | Real local Git fixtures, hostile configuration/process/path canaries, binary/runtime inspection |
| Native Git smart transport | Credential-bearing Git discovery/upload-pack requests under the exact RFC-BPR-0002 profile/account authority; streaming protocol bounds; no redirects/cookies | Loopback TLS/proxy/protocol fixtures and exact request/body/header/authority counters |
| Epoch catalog adapter | Versioned transactional catalog, ordinal chronology, availability, pending commit/recovery, exact pruning/deletion state, no source or credential content | Disposable catalog fixtures, transaction-failure and crash-recovery tests, exact cleanup |
| Qt presentation adapter | Provider/Git-free stable epoch model, independent selectors, exact OIDs, aliases, unavailable states, focus/accessibility, incremental updates, native appearance inheritance | Qt signal/state/accessibility tests plus deterministic and native rendered automation |
| Composition root | Explicit Git/transport/catalog/host/core/Qt wiring and worker/shutdown order | Test-enabled recorded/local fixture trace and tests-disabled Release graph/binary contract |

RFC-BPR-0003 uses a dedicated epoch core and application host. It does not
expand the RFC-BPR-0002 GitHub reducer/host into a cross-feature controller,
and it does not use a Qt projection as an internal event bus.

## Authority and identity plan

The complete asynchronous context includes:

```text
application session
host profile + profile revision
Git host + canonical hostname
authenticated GitHub account
repository + pull request
provider snapshot + GitHub identity/PR generation
epoch request + epoch generation + expected catalog revision
```

Physical Git objects may be stored once per validated host/repository
partition only if BPR-0040 proves safe authorization and privacy behavior.
Catalog visibility, alias resolution, selection, and every publication remain
bound to the current profile/account/PR authority. There is no offline-open
fallback that makes cached private content visible to an unauthenticated or
different account. Every catalog partition and retained ref uses a validated
opaque authority-scope identity derived from the complete profile/account/
repository/PR authority. A shared physical object store never implies a
shared ref name, ordinal, catalog row, or epoch ID.

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
  -> native-HTTPS smart discovery/upload-pack + isolated installed Git
  -> request-scoped staged objects/refs and epoch candidate
  -> serialized completion with exact context
  -> core acceptance/record decision
  -> failure-atomic private-ref + catalog commit
  -> exact catalog/comparison/status deltas
  -> incremental Qt rows and independent selectors
```

No callback, fetched object, staging ref, catalog row, label, or Qt selection
becomes active merely because it exists. BPR-0040 must specify the commit
protocol so a disk/catalog/ref failure cannot leave the core, durable store,
and visible selection claiming different accepted epochs.

## Deterministic epoch policy plan

The core receives explicit branch identity, prior/current base/head OIDs,
head-lineage facts, material-equivalence identity, optional historical-review
association, and host-supplied observed time. Ordinal/catalog revision, not
wall time, defines chronology.

Planned primary causes are:

1. initial observation;
2. explicit historical import;
3. base-branch change;
4. descendant head change (`Author upload`);
5. proven non-fast-forward head change (`Force-push/rebase`);
6. head-stable base change (`Base update`);
7. unchanged revision (no epoch).

BPR-0040 must resolve simultaneous base/head change representation and unknown
lineage without guessing. Material equivalence never erases a distinct
observed revision; it only provides an opaque reuse relationship for later
receipt/diff work.

Named comparisons resolve to exact anchors:

- `Whole PR`: current branch base to latest retained head;
- `Previous upload -> latest`: immediately preceding retained epoch with a
  different head, including a force-push/rebase epoch, to latest;
- `My last reviewed -> latest`: newest provably ordered current-viewer review
  commit to latest;
- arbitrary retained anchor to arbitrary retained anchor.

A named alias remains semantic and may re-resolve when the catalog advances.
An explicit pair remains exact and becomes visibly unavailable if its object
is pruned; it never silently jumps to Latest.

## Git process and transport plan

Installed Git is a local object/protocol engine, not a second production
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

The installed-Git adapter resolves an absolute executable/exec path, builds an
environment from nothing, uses app-owned empty system/global configuration and
templates, operates only on canonical validated bare repositories, and admits
fixed commands. It neither inherits nor invokes user/system aliases, URL
rewrites, credentials, askpass, hooks, filters, LFS, submodules, textconv,
external diff, alternates, replace objects, SSH/file/ext transports, editors,
pagers, tracing, fsmonitor, worktrees, auto-GC, or maintenance.

## Cache, retention, and persistence plan

The planned storage shape is versioned and contains no raw hostname, owner,
repository name, account login, URL, or credential:

```text
repositories/v1/<keyed-host-digest>/<repository-id>.git
```

An immutable identity manifest, canonical-descendant check, private
permissions, one-writer lock, exact bare/config/object-format validation, and
absence of alternates/shallow/graft/worktree state gate every use.

Only the accepted base branch and `refs/pull/<number>/head` advertisements are
eligible. Both advertised OIDs must equal the accepted provider snapshot.
Raw `HeadRef` text and provider clone URLs never become Git authority.
Request-scoped staging refs precede atomic compare-and-swap retention refs:

```text
refs/betterpullreview/authority/<authority-scope-id>/epoch/<ordinal>/base
refs/betterpullreview/authority/<authority-scope-id>/epoch/<ordinal>/head
```

`authority-scope-id` is a validated fixed-width nonreversible identifier for
the exact profile revision, host, account, repository, and pull request. It
contains no raw provider text and cannot collide merely because two accounts
observe the same PR number. BPR-0040 must select its derivation and manifest
binding; an absent or mismatched binding fails closed.

Private refs are retention roots. A hard quota refuses new material rather
than silently deleting history. Pruning and repository deletion are explicit
exact-target operations that first record pending durable intent, enumerate
affected history, update refs/catalog availability, and recover or finish
after interruption. No glob or broad credential/cache deletion is permitted.

## Claim matrix

| Claim | Direct surface | Proof | Result |
|---|---|---|---|
| Core policy is deterministic and compact | Qt-free epoch reducer | Exhaust cause, alias, pair, idempotence, overflow, replay, and mismatch tables; inspect state size/dependencies | Planned |
| PR/epoch authority is exact | Core/host/cache/catalog contexts | Cross profile, host, account, repository, PR, snapshot, generation, and catalog revisions | Planned |
| App-owned bare cache is created and reused safely | Real Git adapter and temporary app-data root | Open fixture twice; inspect exact manifest/config/path/object format and reuse counters | Planned |
| Installed Git receives no credential or network authority | Process and smart-transport canaries | Inspect image, argv, environment, cwd, config, streams, descendants, sockets, and native-HTTPS counters | Planned |
| Old observed head survives force-push | Local smart remote, cache, refs, catalog, public selector | Remove/prune old remote object, restart, and compare retained old head to latest | Planned |
| Repository behavior cannot execute | Hostile Git/config/repository fixtures | Arm hooks/helpers/askpass/filters/LFS/submodule/textconv/ext-diff/protocol canaries; require zero invocation | Planned |
| Epoch causes remain truthful | Core cause table with real Git facts | Replay initial, descendant upload, non-fast-forward rewrite, base-only, branch-change, simultaneous-change, equivalent, and unknown-lineage cases | Planned |
| Catalog/ref commit and recovery agree | Fault-injected catalog and Git refs | Fail or interrupt every phase; restart; require one exact committed epoch or an unavailable/recoverable pending state, never a false visible success | Planned |
| Defaults and arbitrary pairs use exact OIDs | Core + Qt + public application | Verify Branch-base-to-Latest and selected IDs/OIDs/request context agree; allow reverse/identical pairs per final architecture | Planned |
| Named aliases resolve exactly | Catalog/core/public application | Verify Whole PR, preceding distinct head, and provably newest current-viewer review; show reason when unavailable | Planned |
| Pruned and never-observed history is honest | Catalog/core/Qt | Explicit prune/delete and unreachable unknown head stay visible/unavailable and never silently remap | Planned |
| Stale/canceled work cannot publish | Manual executor and real process cancellation | Reorder/duplicate/cancel Git, transport, catalog, and identity-switch completions | Planned |
| Qt projection is incremental and accessible | Stable-ID model and real widgets | Assert exact signals, persistent indexes, focus, keyboard, OID text, announcements, responsive layout, and zero routine resets | Planned |
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
comparisons expose stable IDs, full copyable OIDs, ordinal/cause/availability,
and explicit loading, refreshing, retained-error/cancel, empty, pruned,
cache-deleted, and never-observed states. Qt dispatches IDs/aliases and never
infers cause, fallback, or legality.

Deterministic renders cover supported widths and 1.0x, 1.25x, and 2.0x.
Native macOS evidence covers actual system Light and Dark plus retained pair,
focus, model identity, exact OIDs, geometry, and zero resets through a live
appearance transition. Offscreen forced-style evidence remains geometry-only.

RFC-BPR-0005 must still allocate and complete its distinct final integrated
workbench presentation-polish/native visual-acceptance card. RFC-BPR-0006 owns
selectable System/Light/Dark/high-contrast modes, with fresh install defaulting
to System, plus fonts and syntax presentation. RFC-BPR-0003 makes no final
visual-polish or custom-theme claim.

## Documentation ledger

| Scope | Required documents | Disposition | Planned evidence |
|---|---|---|---|
| Core epoch state and typed flow | Domain/state model and Git/epoch architecture | Planned: exact revision, anchor, cause, alias, request/generation, acceptance, and deltas | Core tables, replay/property tests, state/dependency scans |
| Host/Git/transport boundary | System architecture, Git/epoch implementation, security model | Planned: native smart transport, isolated installed Git, cache/ref/catalog authority, staging, cancellation, recovery, lifecycle | Fake and real process/transport/catalog fixtures plus boundary/runtime scans |
| Cache privacy and operations | Git/epoch architecture, build/test and operator guidance | Planned: location, identity, permissions, quotas, retention, force-push limit, explicit prune/delete, crash consequences | Hostile path/config/recovery tests and exact filesystem/ref/catalog inspection |
| Epoch semantics | Product/UX reference and verification ledger | Planned: causes, chronology, equivalence, exact anchors, defaults, aliases, unavailable history | Core/host/public fixture matrix |
| Qt presentation | Workbench and appearance/accessibility documents | Planned: reusable selectors, exact OIDs, states, incremental model, accessibility, native inheritance; no final-workbench claim | Qt signal/state/raster/native appearance evidence |
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
