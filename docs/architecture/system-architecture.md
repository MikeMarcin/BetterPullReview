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
- Native HTTPS as the sole milestone-one production GitHub authentication
  transport
- Visible GitHub CLI profile choices fixed to typed `cli_unavailable` with
  zero CLI executable/configuration discovery or CLI process launch; the
  bounded `QProcess` broker remains inactive scaffolding until RFC-BPR-0015
- REST-first GitHub provider with narrow GraphQL use
- KSyntaxHighlighting through its low-level highlighter interface
- No Qt WebEngine, QML runtime, Electron, Tauri, Node runtime, local HTTP service, or browser-extension requirement

The concrete RFC-BPR-0002 allocation is defined in [GitHub Host
Implementation Architecture](github-host-implementation.md).

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

    the GitHub adapter may depend on Qt Core and Network for its designated
    process, HTTPS, and native-integration effects, but never on Qt Widgets

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
- `QProcess`, `QNetworkAccessManager`, and credential-vault APIs are confined
  to designated external adapter sources; their presence in a provider adapter
  does not authorize their use in the application host or Qt presentation.
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
| Staged provider results | Application host, pending a core acceptance decision | immutable normalized snapshots keyed by complete request context |
| Caches and indexes | Application host | accepted GitHub snapshots, Git objects, normalized API cache, diff/syntax cache, row maps, search indexes |
| Epoch catalog | Application host and transactional catalog adapter | authority-scoped catalog incarnation/revision, durable epoch/anchor rows, availability, and pending recovery records |
| Staged Git/epoch results | Application host until a core commit request and durable terminal | request-scoped objects/refs and one bounded epoch candidate keyed by complete authority/source/catalog context |
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
    -> host commits only an explicitly accepted staged result
    -> host publishes bounded changed projections
    -> Qt adapter incrementally updates affected actions, models, and views
```

No widget may call GitHub, Git, SQLite, or an agent directly. No provider completion may mutate a widget or core state directly.
For GitHub reads, an `accepted_snapshot_delta` is the explicit commit signal;
a broad changed-domain bit cannot substitute for acceptance.

RFC-BPR-0003 begins only after that signal and host commit. The GitHub host
then emits one provider-free `accepted_pull_request_handoff` containing the
exact application session, profile/profile-revision, host/authority-fingerprint/
hostname/server contract, account/viewer, repository/validated locator, pull
request/number, provider snapshot, typed revision origin, GitHub identity/
request generations, object format, base-branch identity, validated typed base
ref, algorithm-tagged base/head OIDs, bounded current-viewer review candidate,
and optional native smart-Git lease.
The handoff deliberately contains neither a source-observation identity nor
observed time.
After exact catalog recovery, the epoch host mints the source identity from
the operating-system CSPRNG; it observes UTC time only when preparing a
candidate. The GitHub host emits an invalidation before a previous identity
can be replaced. This narrow handoff performs no Git, persistence, clock,
entropy, or Qt work and does not make the RFC-BPR-0002 GitHub host an epoch
controller.

## Concurrency and stale work

- Application decisions are applied on one serialized decision lane, normally the GUI/application lane.
- Blocking network, vault, Git, diff, syntax, indexing, persistence
  preparation, and AI work runs through host-owned effect executors, separate
  from the serialized decision lane, and may run in parallel.
- Every asynchronous request carries the identities required to prove relevance: request, session, host/account, repository, PR, revision/epoch, and generation as applicable.
- Workers operate on immutable inputs and never touch Qt objects or mutable core state.
- Completions return as typed events to the serialized lane.
- Stale completions are rejected deterministically in constant time.
- Cancellation may stop work, suppress publication, or supersede a generation; it is a normal outcome rather than automatically an error.
- Tests may use one deterministic manual executor for both interfaces while
  still preserving their separate effect-execution and decision-serialization
  roles.

Epoch ingestion and comparison resolution use separate typed generations and
at most one semantic pending operation in each core lane. Their contexts
include the request, application session, complete pull-request authority,
accepted pull-request revision, catalog incarnation/revision, and the lane
generation. The host may temporarily retain up to 100 exact prepared
operations so superseded or rejected preparation can be discarded by identity;
that bounded effect state is not a second semantic lane. BPR-0055 adds one
compact core-authorized maintenance lane whose pending state retains only the
exact catalog/cache identity, target count, and deterministic target digest;
the bounded target batch remains in the request. Authority replacement
advances every applicable generation before cancellation and clears
identity-sensitive epoch projections. Same-authority refresh may retain the
last accepted exact pair as visibly refreshing.

Epoch cache/catalog effects are serialized by a host-lifetime registry of at
most 1,000 actors, one per exact repository cache authority. Each actor owns
one FIFO and at most one live cache session. Switching pull requests in the
same repository first runs the old context's exact cleanup under its old PR
session, closes that session, and opens the full new PR request; unrelated
repository actors may run in parallel. Reentrant ingress is deferred until the
current decision turn completes. Projection delivery is synchronous and
observational: its exceptions are contained and cannot veto core acceptance,
durable success, cleanup, or actor progress. All ingress and state/quiescence
inspection remain decision-lane confined. Shutdown is quiescent only after
jobs, deferred ingress, pending offers, decision turns, and every actor queue
are idle.

Every normally accepted Git/catalog submission during the live host lifetime
produces exactly one typed success, failure, or cancellation terminal. An
abrupt process death instead leaves one typed durable operation outcome for
recovery, not a terminal-delivery promise to the dead session. Cancellation
is advisory. Before the recorded point of no cancellation it may produce
cancellation with no accepted epoch. After that phase, recovery must return
success or a typed failure, not cancellation. The catalog visibility
transaction is the durable success and linearization point; after it commits
the terminal is success so durable state and the core cannot disagree. Stale,
duplicate, mismatched, failed, or cancelled terminals emit no
epoch/catalog/comparison acceptance delta. A durable success that is stale
because the active identity changed remains private to its own authority
catalog and is not published into the replacement identity.

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
| GitHub/GHES | host/account identity policy, command legality, completion acceptance | provider capability port, session/staging/cache lifecycle; external GitHub adapter implements authentication and transports | account/host selection, errors, capability presentation |
| Git/epochs | epoch-selection rules, comparison intent, stale acceptance | Git process, bare cache, refs, clocks, persistence | epoch selectors, labels, unavailable states |
| Diff/navigation | deterministic mapping/group planning, active change, navigation policy | blob residency, jobs, diff/syntax caches | virtualized panes, connectors, lower detail, focus |
| Review ledger | receipt/actionability transitions | SQLite authority, queries, migrations | badges, filters, commands, explanations |
| GitHub mutations | semantic proposal, target/head validation policy | mutation execution, idempotency, reconciliation | draft editor, confirmation, progress, diagnostics |
| Queue/observers | rule matching and attention policy | refresh timers, API/cache, notification request execution | queue models, filters, native notification presentation |
| AI | provider-host policy, context-scope decisions, staleness, finding promotion | process/session/workspace management and protocols | explicit invocation, approvals, chat/findings presentation |
| Appearance | no color/font/widget authority | settings persistence and cache scheduling | system mode, fonts, colors, syntax presentation, accessibility |

## RFC-BPR-0003 dedicated epoch vertical slice

RFC-BPR-0003 is implemented as a dedicated epoch core and application host.
It does not grow the GitHub reducer/host into a cross-feature controller and
does not use a Qt projection as an internal event bus.

```text
accepted RFC-BPR-0002 pull-request handoff
    -> dedicated epoch host
    -> bounded catalog and Git observations
    -> deterministic epoch core
    -> semantic prepare/commit/resolve requests
    -> Git transport/process and catalog adapters
    -> typed terminals on the serialized decision lane
    -> exact catalog/comparison deltas
    -> provider/Git-free incremental Qt projection
```

The core uses fixed algorithm-tagged commit/tree identities, a complete
pull-request authority and source revision, a catalog incarnation plus
monotonic revision, composite epoch cause, exact base/head or review anchors,
and checked request/epoch/comparison generations. Its state retains no catalog
page or Git object and is bounded to 2,048 bytes. The host owns refs, object
residency, ancestry/material observations, time, persistence, append recovery,
and cancellation. The catalog owns full durable history. BPR-0056 adds the
bounded core/host page algebra that exposes at most 100 sanitized rows at a
time; core state retains only the accepted catalog and continuation, and the
projection never requests a whole-model reset.

BPR-0046 consumes that projection in one provider/Git/filesystem-free Qt
control embedded in the interim loaded pull-request window. Its shared stable
model exposes each epoch's typed base and head anchors to two independent
selectors, applies routine page/catalog changes with insert/remove/move/data
signals, and preserves exact selection and copyable SHA-1/SHA-256 OIDs. Named
actions and selector gestures dispatch only semantic core intents. The
control owns focus, accessibility, announcements, and live palette/font/style
invalidation; it does not resolve aliases, compare OIDs, inspect availability,
or perform an effect.

An epoch ID is scoped by exact authority, catalog incarnation, and a nonzero
ordinal no greater than `INT64_MAX`. Recreating a deleted catalog creates a
new incarnation before ordinal reuse. The host presents at most one latest
live epoch, one preceding distinct-head candidate, one current-viewer review
candidate, one material-equivalence candidate, and two requested anchor facts
to a core transition.

Cause retains independent origin, base transition, and head transition. The
primary label precedence is initial observation, explicit historical import,
base-branch change, proven non-fast-forward head, descendant head,
unknown-lineage head, then base-only update. Simultaneous base/head changes
preserve both axes, and unknown lineage is never relabeled as
force-push/rebase.
Material equivalence is exact equality of ordered
base-tree/head-tree/policy-revision tuples; it annotates but never removes or
reorders an epoch.

Named comparisons remain semantic. `Whole PR` resolves latest live base to
latest live head. `Previous upload -> latest` selects the greatest earlier
live epoch with a different head, including a force-push; a missing/pruned
candidate makes the alias unavailable rather than causing a fallback.
`My last reviewed -> latest` uses the maximum typed chronological review
order for the exact current-viewer actor among non-pending, commit-bearing
reviews. Historical imports never become Latest or Previous upload merely
because they were imported later.

Explicit pairs retain exact anchor identities and never remap. Same-authority,
same-incarnation available anchors are legal in either order, including
reverse and identical-OID comparisons. Cross-authority, cross-incarnation,
missing, pruned, or unavailable pairs are illegal. Every later diff request
uses the accepted anchor identities, exact OIDs, catalog revision, and
comparison revision rather than selector labels.

Catalog availability is necessary but not physical proof. Before admitting a
same-cache `have` or resolving a named or explicit comparison, the epoch host
directly inspects each candidate commit through the credential-free Git-cache
port. A missing or mismatched object is omitted from negotiation or projected
as unavailable; the host never upgrades catalog metadata into object residency.

The host stages request-scoped Git objects/refs and a bounded candidate before
the core issues `CommitEpochCatalogRequest`. The request carries the expected
catalog revision. Ref/catalog publication is failure-atomic and recoverable:
the core changes accepted epoch/catalog/comparison state only after an exact
durable success terminal. A compare-and-swap conflict commits nothing and
requires a fresh bounded catalog observation.

The BPR-0045 component implements append orchestration and restart recovery.
It journals exact spool/inventory facts before `index-pack`, records
`ref_commit_in_progress` before ref promotion, and treats that phase as the
point of no cancellation. Any error or exception after that point is resolved
from exact durable and physical postconditions. Catalog visibility remains
mandatory success even when cleanup is left for recovery. BPR-0055 applies
the same rule to core-authorized prune/cache-delete orchestration: cancellation
may abort only before durable ref deletion or quarantine begins, and restart
must resume each later phase with the original operation authorization.
BPR-0047 retains
selection of the complete host path by the public application.

When base, head, and branch are unchanged but the accepted
newest-current-viewer review fact changes, the core issues the
`associate review` variant instead. It performs one expected-revision
SQLite-only catalog mutation, advances the revision only for a changed
association, and creates no epoch, Git object import, or ref.

## Performance contract

Routine work must scale approximately with:

```text
active set + changed set
```

not total PR history, total repository size, or all rows in every panel. The implementation must use incremental projections, bounded prefetch, retained caches, cancellation, event coalescing, and allocation-conscious hot paths. There is no JSON/RPC bus between in-process regions.

Provider refresh follows the same rule: normalization is bounded before
staging, accepted snapshots are compared by stable identity, and Qt receives
only affected metadata and conversation deltas rather than a routine model or
application reset.

## Testing contract

- **Core:** no `QApplication`; table-driven transitions, invariants, property tests, replay, fuzzing, and deterministic benchmarks.
- **Host:** fake/in-memory ports, manual executor, completion ordering, cancellation, stale rejection, cache invalidation, persistence, failure, recovery, and shutdown tests.
- **Qt adapter:** event-to-intent translation, `QAction` projection, incremental model notifications, selection synchronization, focus, keyboard, accessibility, high-DPI, and rendered interaction tests.
- **End to end:** a smaller set of complete workflows through the real composition root and packaged application.

Build, test, package, release, and RFC verification evidence is produced by
local or target-native tools. GitHub Actions has zero role in any of those
surfaces.

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
