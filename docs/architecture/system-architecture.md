# BetterPullReview System Architecture

## Normative pattern

BetterPullReview must be implemented using the pack's [Deterministic Functional Core, Imperative Host, and Qt Presentation Adapter](deterministic-functional-core-imperative-host-qt-presentation-adapter.md) architecture. This document specializes that architecture for the review workbench.

The application has two architectural regions:

```text
Deterministic functional core
    decides what BetterPullReview means and what should happen next

Imperative shell
    ├── application host
    │   owns effects, authority sessions, services, jobs, caches, and lifecycle
    │
    └── Qt presentation adapter
        owns windows, widgets, Qt models, actions, focus, layout, and event translation
```

The governing rule is:

> **The core decides. The host performs. Qt presents.**

The Qt presentation adapter is part of the imperative shell. It is not an architectural peer that bypasses the host or the core.

## Technology baseline

- C++23
- Qt 6 Widgets, Core, Gui, Network, and required platform integration
- CMake and Ninja
- SQLite through a pinned, directly linked C API
- Installed Git executable through `QProcess`
- REST-first GitHub provider with narrow GraphQL use
- KSyntaxHighlighting through its low-level highlighter interface
- No Qt WebEngine, QML runtime, Electron, Tauri, Node runtime, local HTTP service, or browser-extension requirement

## BetterPullReview component graph

```text
┌──────────────────────────────────────────────────────────────────┐
│ Imperative shell                                                 │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ Qt presentation adapter                                  │  │
│  │ windows · widgets · QActions · Qt models · diff painting │  │
│  │ focus · accessibility · themes · native interaction      │  │
│  └─────────────────┬──────────────────────────▲───────────────┘  │
│                    │ semantic intents          │ projections      │
│                    ▼                           │ and deltas        │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ Application host                                         │  │
│  │ composition · GitHub/Git/SQLite/AI ports · jobs · caches │  │
│  │ request execution · persistence · cancellation · lifecycle│  │
│  └─────────────────┬──────────────────────────▲───────────────┘  │
│                    │ bounded observations      │ requests         │
└────────────────────┼───────────────────────────┼───────────────────┘
                     ▼                           │
          ┌──────────────────────────────────────────────┐
          │ Deterministic functional core                │
          │ review interaction state · policy · planners │
          │ validation · transitions · semantic deltas   │
          │ request identity/generation · diagnostics    │
          └──────────────────────────────────────────────┘
```

## Build-target and dependency direction

The initial target shape should be equivalent to:

```text
better_pull_review_core
    standard/domain C++ only

better_pull_review_application
    application host, ports, requests, sessions, caches
    depends on core

better_pull_review_adapter_github
better_pull_review_adapter_git
better_pull_review_adapter_sqlite
better_pull_review_adapter_ai
    implement application-host ports

better_pull_review_adapter_qt
    Qt presentation adapter
    depends on application-facing contracts and Qt

better_pull_review_app
    composition root; wires concrete adapters, host, core state, and Qt presentation
```

The live repository may use different target names, but it must enforce these rules:

- The core imports no Qt, SQLite, GitHub, Git, operating-system, AI, clock, thread, future, timer, dialog, or process APIs.
- The host depends on core and capability-specific port contracts. It may be mutable and concurrent, but semantic decisions remain in the core.
- Provider adapters implement narrow host ports. They do not expose provider response types to the core or Qt.
- The Qt adapter uses Qt idiomatically. It does not own canonical GitHub state, review policy, persistence, asynchronous orchestration, or stale-result policy.
- The composition root is the only place that knows all concrete pieces.
- In-process communication uses typed C++ calls and records. JSON/RPC is added only at a real process boundary such as an agent protocol.

## Repository shape

```text
BetterPullReview/
  CMakeLists.txt
  cmake/
  src/
    core/
      state/
      events/
      transitions/
      planners/
      projections/
    application/
      host/
      ports/
      requests/
      sessions/
      caches/
    adapters/
      qt/
        windows/
        widgets/
        models/
        actions/
        dialogs/
        diff/
      github/
      git/
      sqlite/
      ai/
    app/
      main.cpp
  tests/
    core/
    application/
    adapters_qt/
    contract/
    end_to_end/
    fixtures/
    benchmarks/
    fuzz/
  docs/
  packaging/
```

Do not fragment a feature into unnecessary targets before the ownership boundary is real. Do not collapse the core, host, and Qt adapter into a main-window controller.

## State authority map

| State category | BetterPullReview authority | Examples |
|---|---|---|
| Shared collaborative state | GitHub, accessed through host/provider sessions | PR commits, comments, replies, reviews, thread resolution, checks |
| Private durable review state | Host-owned SQLite ledger | receipts, acceptance, observer rules, local defect classification, AI drafts |
| Interaction state | Functional core | active PR/file/change, epoch selection, navigation context, command state, pending semantic operation |
| Bounded observations | Host, borrowed by core | current GitHub snapshot, selected file material, permissions, capability facts, visible queue records |
| Caches and indexes | Application host | Git objects, normalized API cache, diff/syntax cache, row maps, search indexes |
| Presentation projections | Host and Qt adapter | visible file rows, conversation cards, command projections, status summaries |
| Qt-local state | Qt presentation adapter | focus, hover, expanded branches, scroll positions, drawer geometry, animation progress |
| External resource state | Application host/adapters | sockets, child processes, credential-vault handles, SQLite connection, worker pool |
| Pending-work identity | Core and host | request IDs, session IDs, revisions, generations, cancellation/supersession |

There must be one authority per category. A `QAbstractItemModel`, cache, and widget may project a fact but may not become a competing canonical authority.

## Semantic vocabulary

Every feature uses explicit typed concepts:

- **Intent:** what the user or another actor is trying to do, such as `select_comparison_epochs_intent` or `publish_review_intent`.
- **Event:** a fact that occurred, such as `pull_request_loaded_event`, `git_diff_completed_event`, or `review_publish_failed_event`.
- **Observation:** an immutable bounded fact supplied by the host, such as the selected file material, current permissions, current head OID, or visible review records.
- **Request:** a semantic external action the host must attempt, such as `fetch_pull_request_request`, `compute_diff_request`, `persist_receipt_request`, or `publish_review_request`.
- **Delta:** a compact semantic change, such as changed command state, selected rows, review receipt state, queue entries, or status.
- **Diagnostic:** a structured semantic rejection or operational fact; presentation is decided at the Qt edge.

The core must not emit widget commands such as `show_popup`, `focus_button`, or `set_tree_row`. The Qt adapter must not dispatch string commands or mutate policy fields directly.

A representative core API is:

```cpp
struct transition_result
{
    changed_domain Changed = changed_domain::None;
    std::span<const host_request> Requests;
    std::span<const diagnostic> Diagnostics;
};

[[nodiscard]] auto transition(
    application_state& aState,
    const application_event& aEvent,
    const observation_view& aObservations,
    transition_storage& aStorage) -> transition_result;
```

Deterministic in-place mutation and retained storage are permitted. Hidden I/O, hidden globals, implicit wall-clock reads, thread creation, and Qt access are not.

## End-to-end flow

```text
Qt event or QAction
    -> Qt adapter constructs semantic intent
    -> application host obtains bounded observations
    -> core transitions interaction state
    -> core emits semantic deltas, requests, and diagnostics
    -> host performs immediate or asynchronous requests
    -> host converts completion into a typed event
    -> core accepts/rejects it using identity and generation
    -> host publishes changed projections
    -> Qt adapter incrementally updates affected actions, models, and views
```

No widget may call GitHub, Git, SQLite, or an agent directly. No provider completion may mutate a widget or core state directly.

## Concurrency and stale work

- Application decisions are applied on one serialized decision lane, normally the GUI/application lane.
- Network, Git, diff, syntax, indexing, persistence preparation, and AI work may run in parallel.
- Every asynchronous request carries the identities required to prove relevance: request, session, host/account, repository, PR, revision/epoch, and generation as applicable.
- Workers operate on immutable inputs and never touch Qt objects or mutable core state.
- Completions return as typed events to the serialized lane.
- Stale completions are rejected deterministically in constant time.
- Cancellation may stop work, suppress publication, or supersede a generation; it is a normal outcome rather than automatically an error.
- Tests use a deterministic manual executor that can reorder, delay, fail, cancel, and deliberately deliver stale completions.

## Qt presentation rules

- `QAction` is the presentation of semantic command identity. Enabled, checked, and disabled-reason state comes from command projections.
- `QAbstractItemModel` instances adapt stable identities into Qt indexes. They apply insert/remove/move/data-change deltas incrementally.
- `beginResetModel()` is not acceptable for a selection change, comment arrival, single receipt update, or other bounded change.
- Semantic selection uses stable IDs and synchronizes with `QItemSelectionModel` without recursive event loops.
- Custom diff widgets paint visible rows only and retain layout caches; comments remain in the conversation drawer.
- Dialogs, notifications, native menus, focus, accessibility, themes, layout, and high-DPI behavior belong to the Qt adapter.
- Reusable widgets do not invoke application persistence, global service registries, or unrelated windows.

## Feature allocation

| Feature family | Functional core | Application host | Qt presentation adapter |
|---|---|---|---|
| GitHub/GHES | host/account identity policy, command legality, completion acceptance | authentication, REST/GraphQL, retries, capability cache | account/host selection, errors, capability presentation |
| Git/epochs | epoch-selection rules, comparison intent, stale acceptance | Git process, bare cache, refs, clocks, persistence | epoch selectors, labels, unavailable states |
| Diff/navigation | deterministic mapping/group planning, active change, navigation policy | blob residency, jobs, diff/syntax caches | virtualized panes, connectors, lower detail, focus |
| Review ledger | receipt/actionability transitions | SQLite authority, queries, migrations | badges, filters, commands, explanations |
| GitHub mutations | semantic proposal, target/head validation policy | mutation execution, idempotency, reconciliation | draft editor, confirmation, progress, diagnostics |
| Queue/observers | rule matching and attention policy | refresh timers, API/cache, notification request execution | queue models, filters, native notification presentation |
| AI | provider-host policy, context-scope decisions, staleness, finding promotion | process/session/workspace management and protocols | explicit invocation, approvals, chat/findings presentation |
| Appearance | no color/font/widget authority | settings persistence and cache scheduling | system mode, fonts, colors, syntax presentation, accessibility |

## Performance contract

Routine work must scale approximately with:

```text
active set + changed set
```

not total PR history, total repository size, or all rows in every panel. The implementation must use incremental projections, bounded prefetch, retained caches, cancellation, event coalescing, and allocation-conscious hot paths. There is no JSON/RPC bus between in-process regions.

## Testing contract

- **Core:** no `QApplication`; table-driven transitions, invariants, property tests, replay, fuzzing, and deterministic benchmarks.
- **Host:** fake/in-memory ports, manual executor, completion ordering, cancellation, stale rejection, cache invalidation, persistence, failure, recovery, and shutdown tests.
- **Qt adapter:** event-to-intent translation, `QAction` projection, incremental model notifications, selection synchronization, focus, keyboard, accessibility, high-DPI, and rendered interaction tests.
- **End to end:** a smaller set of complete workflows through the real composition root and packaged application.

## Prohibited implementation shortcuts

- Business workflow in `QWidget` or `QAbstractItemModel` subclasses.
- A main-window object that owns canonical state, network, Git, persistence, selection, and caches.
- Semantic policy duplicated in the host and core.
- Workers updating widgets or core state directly.
- Untagged asynchronous results.
- A universal string/map effect bus or process-wide service locator.
- Whole-application model resets for routine changes.
- Full-state copies or full-PR scans for ordinary navigation and receipt updates.
- Fake `IWidget`/`IButton` abstractions that hide Qt instead of containing it at the adapter boundary.
