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

## Qt-adapter checks

| ID | Requirement | Direct proof | Failure examples |
|---|---|---|---|
| ARCH-Q1 | Qt events translate into semantic intents | Qt adapter tests | Business workflow implemented in a widget callback |
| ARCH-Q2 | `QAction` reflects semantic command projections | Action-state tests across selections/capabilities | Duplicate enablement logic in toolbar/menu/context menu |
| ARCH-Q3 | Qt models update incrementally by stable identity | Model-signal assertions and persistent-index tests | `beginResetModel()` for one receipt/comment/selection change |
| ARCH-Q4 | Qt owns visual-only mechanics | Focus/layout/accessibility tests | Core emits focus/popup/animation/widget commands |
| ARCH-Q5 | Workers never touch Qt | Thread-affinity instrumentation | Network/diff/AI worker mutates a widget or Qt model |

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
