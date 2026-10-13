# Architecture Conformance Plan

This document turns the deterministic-functional-core / imperative-host / Qt-presentation-adapter architecture into direct implementation and closeout checks. Every RFC must select the applicable rows and add feature-specific evidence.

## Structural checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-S1 | Core is Qt- and provider-free | Build/link the core with Qt/provider packages unavailable; include/dependency scan | `QString`, `QObject`, SQLite, GitHub, Git, process, clock, thread, or AI headers reachable from core |
| ARCH-S2 | Dependency direction is inward | CMake target graph and forbidden-dependency test | Core depends on host/Qt; provider adapter reaches into widgets |
| ARCH-S3 | Composition is explicit | Inspect and test the composition root | Global service locator, hidden singleton initialization, widget-created providers |
| ARCH-S4 | Capability ports are narrow | Interface review and fake-port tests | One giant `application_services` object or one-method interface explosion |

## Deterministic-core checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-C1 | Same state + event + observations produces the same transition | Event-sequence replay and deterministic randomized/property tests | Hidden clock, random source, global cache, or I/O changes result |
| ARCH-C2 | Core emits semantic requests/deltas | Inspect transition outputs and compile-time types | `show_popup`, `set_widget_text`, raw SQL, URL, or shell command emitted by core |
| ARCH-C3 | Core state is compact | State-size/report and large-PR test | Full Git blobs, all comments, all queue rows, or Qt objects retained in core |
| ARCH-C4 | Command legality and stale-result policy live in core | Direct transition tests | Widget slot or host retry code decides semantic legality |

## Host checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-H1 | All external effects are host-owned | Fake-port integration and effect trace | Widget calls GitHub/Git/SQLite/agent directly |
| ARCH-H2 | Host does not duplicate semantic policy | Core/host decision audit and mutation-path tests | Host independently chooses next file, verifies defects, or reinterprets multi-selection |
| ARCH-H3 | Async work is identity-tagged and serialized on completion | Manual-executor tests with reordered/stale completions | Untagged completion overwrites a new PR/file/epoch |
| ARCH-H4 | Caches remain subordinate to authority | Invalidation/reconciliation tests | Cache value overrides GitHub or durable ledger fact |
| ARCH-H5 | Lifecycle and shutdown are explicit | Pending-work shutdown tests | Callback reaches destroyed UI; process or DB teardown races |

## Strict CLI transient credential checks (RFC-BPR-0078)

The architecture gate permits only the runner's closed four-name token table and
adapter-owned sensitive acquisition/environment ports. Generic requests cannot
carry a token variable; no application/core/Qt projection or persistent store
owns credential bytes. Mutation tests reject expanded tables, misplaced token
names and removal of ownership gates. Native qualification is separate from
structural permission: [the strict ledger](RFC-BPR-0078.md) binds actual stock-gh
lookup/helper outcomes, persistence negatives and descendant lifetime evidence.
Production runner factories supply no test seam; the explicit post-start seam
only forces a stalled worker while its real credential-bearing child is alive.

## RFC-BPR-0003 epoch checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-E1 | Git object, PR authority, source revision, catalog incarnation, epoch, anchor, request, generation, and comparison identities are strongly typed and bounded | Compile-time type distinctions plus SHA-1/SHA-256, zero, mixed-format, range, and checked-increment tables | Raw OID/ref/label string becomes identity; counter wraps; catalog recreation reuses an incarnation |
| ARCH-E2 | Dedicated epoch core state is compact and independent of the RFC-BPR-0002 GitHub reducer | `sizeof(epoch_state) <= 2112`, Qt-free/core-only build, allocation/dependency scan, and large-history test | Epoch history enters `github_state`; strings, vectors, catalog pages, refs, or Git objects are retained in `epoch_state`; provider, clock, process, or database APIs enter the epoch core |
| ARCH-E3 | The accepted GitHub-to-epoch handoff is exact and post-acceptance | Cross every profile revision, host, account, repository, PR, snapshot, GitHub generation, base branch, base OID, and head OID; reorder acceptance/invalidation | Epoch work begins from a staged/rejected snapshot, login/ref text selects authority, or a new identity sees old catalog state |
| ARCH-E4 | Epoch cause preserves independent origin/base/head facts | Exhaust initial, historical, base-only, branch-change, descendant, non-fast-forward, unknown-lineage, simultaneous, and unchanged tables | One enum loses a simultaneous change; unknown lineage is called force-push; timestamp determines chronology |
| ARCH-E5 | Material equivalence is exact and non-destructive | Compare ordered base-tree/head-tree/policy identities across equal, unequal, unknown, cross-authority, and repeated-revision cases | Approximate diff text declares equivalence; equivalent material suppresses/reorders an epoch or migrates a receipt |
| ARCH-E6 | Alias and explicit-pair policy remains core-owned | Whole-PR, previous-distinct-head, current-viewer-review, force-push, base-only, historical-import, reverse, identical, pruned, and cross-authority tables | Host/Qt chooses fallback; a pruned alias skips to older history; an explicit pair jumps to Latest |
| ARCH-E7 | `My last reviewed` uses complete typed GitHub review chronology and persists association-only changes | Multi-page fixtures with exact viewer actor, review order `1..10,000`, pending/dismissed/missing-author/missing-commit cases, rejected incomplete pagination, and a new review on an unchanged base/head/branch | Login, review ID, timestamp, incidental container order, partial response, or an older available commit selects the alias; an unchanged head drops the new review fact because no epoch was appended |
| ARCH-E8 | Catalog/ref publication is authorized, compare-and-swap guarded, failure-atomic, and recoverable | Fault inject every stage and restart; require exact expected revision and either one durable committed epoch or no accepted epoch | Prepared objects become visible; partial refs/catalog are reported as success; revision conflict overwrites a concurrent catalog |
| ARCH-E9 | Stale, mismatched, duplicate, failed, and cancelled terminals cannot publish | Manual-executor cross-product over request/session/authority/source/incarnation/revision/generation plus cancellation before/after the durable commit point | Wrong completion clears current pending work; post-commit cancellation reports false failure; stale success enters another account |
| ARCH-E10 | Epoch host lifecycle keeps the decision lane, Git transport/process, catalog, and projections alive through quiescence | Pending load/prepare/commit/resolve/maintenance shutdown tests and destruction-order instrumentation | Decision executor stops before a terminal, staged refs leak without recovery, or a callback reaches a destroyed projection |
| ARCH-E11 | Full epoch history remains catalog-owned while core-authorized Qt paging stays bounded and reachable | Traverse more than 1,000 exact newest/older pages, reach ordinal one, restart newest, retain exact out-of-window OIDs, and assert a 100-epoch/200-anchor Qt cap with stable row signals and no routine reset | Qt accumulates pages, invents cursors, pins synthetic rows, loses selected OIDs, mixes catalog revisions, or cannot return from old history |

## RFC-BPR-0004 diff and alignment checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-D1 | One semantic alignment model drives mapping, groups, navigation, detail, ribbons, and scroll | Stable-ID trace across core and composed UI | Separate link/detail mapping, Qt regrouping, or selected surfaces disagree |
| ARCH-D2 | Diff cache/completions use exact authority and comparison identity | Cache hit/miss plus reordered manual-executor matrix | Same path crosses repository/PR/blob/options/algorithm identity; stale result publishes |
| ARCH-D3 | Qt alone owns measured geometry and the live comparison position | Type/state audit and core-only build | `qreal`, pane origins, scrollbar values, or live `q` enter core, worker, or durable state |
| ARCH-D4 | One shared controller owns linked vertical scrolling | Rendered left/right/divider input trace | Active/follower coupling, normalized pane scrollbars, handoff jump, or fake alignment rows |
| ARCH-D5 | Link-map work is custom-painted and visible-set bounded | Large sparse-change counters and rendered hit tests | One widget per ribbon, permanent crossing move ribbons, or full-segment scan per paint |

## Qt-adapter checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-Q1 | Qt events translate into semantic intents | Qt adapter tests | Business workflow implemented in a widget callback |
| ARCH-Q2 | `QAction` reflects semantic command projections | Action-state tests across selections/capabilities | Duplicate enablement logic in toolbar/menu/context menu |
| ARCH-Q3 | Qt models update incrementally by stable identity | Model-signal assertions and persistent-index tests | `beginResetModel()` for one receipt/comment/selection change |
| ARCH-Q4 | Qt owns visual-only mechanics | Focus/layout/accessibility tests | Core emits focus/popup/animation/widget commands |
| ARCH-Q5 | Workers never touch Qt | Worker-executor Qt/provider boundary contract plus thread-affinity instrumentation | Worker retains or mutates a widget, Qt model, or provider object |

## RFC-BPR-0014 maintenance and package checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-R1 | Compact core owns maintenance legality and complete request/selection identity | `core_release_maintenance` and core-only graph | File/vault API in core; stale target exports or deletes; wrapped request generation |
| ARCH-R2 | Host owns one physical job, frozen preview, bounded inventory and serialized commit/terminal delivery | `application_release_maintenance`, `sqlite_release_maintenance_files` | Reordered success precedes commit boundary; changed object is deleted; callback outlives owner |
| ARCH-R3 | Destructive preparation drains the selected profile while retaining lease and independent vault worker | `application_release_maintenance_public_path` plus native vault/restart course | Store/logger writes after preparation; metadata erased before credentials; cancellation silently restarts producers |
| ARCH-R4 | Qt presents exact seven commands/preview/confirmations incrementally | `qt_release_maintenance_scale_100/125/200`, actual Settings public path | Widget performs an effect; stale dialog grants a new target; status resets review/preview state |
| ARCH-R5 | Tests-disabled package contains required composition/helpers and exact dependency/source closure | `verify_release_package.py`, `verify_release_security.py`, hostile package course and independent native trust | Missing MCP/supervisor/update helper; fixture seam; untrusted plugin; mismatched SPDX or source sidecar |

These are check definitions, not a full-matrix verdict. The
[RFC-0014 ledger](RFC-BPR-0014.md) binds executed candidates and pending native,
trust, vulnerability and crash criteria. Core isolation requires no separately
distributed SDK. The [maintenance contract](../architecture/release-maintenance.md)
defines the typed intent/prepared/commit/terminal/delta path and fixed limits;
the [corporate packet](../security/corporate-security-overview.md) maps its effects.

## Performance checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-P1 | Routine work scales with active set + changed set | Instrumented workflows and structural counters | One comment or selection change scans/rebuilds the whole PR |
| ARCH-P2 | High-frequency work is coalesced and allocation-conscious | Scroll/resize/typing/selection benchmarks | Per-event full projection or unbounded allocation |
| ARCH-P3 | In-process calls are typed and direct | Dependency/protocol inspection | JSON/RPC/event-bus serialization between ordinary local modules |

## Test-layer checks

- Core tests run without `QApplication`.
- Host tests use fake/in-memory ports and a deterministic manual executor.
- Qt tests focus on event translation, actions, models, focus, accessibility, rendering, and platform behavior rather than retesting every business rule.
- End-to-end tests run the real composition root for representative workflows.
- Event/observation replay is available for difficult state and concurrency failures.

## Per-RFC closeout evidence

Every RFC closeout ledger must state:

1. which semantic decisions were added or changed in the core;
2. which effects, authorities, caches, or jobs were added or changed in the host;
3. which event translations, projections, actions, models, or visual mechanics changed in Qt;
4. which region had no impact and the direct evidence for that conclusion;
5. the typed intent/event/observation/request/delta path exercised by the public application;
6. the stale-result/cancellation identity used by any asynchronous work;
7. the incremental-update and active/changed-set proof for routine interactions.

A static architecture diagram, target name, mockup, or view-model-only test cannot by itself satisfy these rows.
