# Deterministic Functional Core, Imperative Host, and Qt Presentation Adapter

> A practical architecture for building testable, high-performance Qt applications without turning the UI layer into the application.

## Executive definition

Use three responsibilities, arranged as two architectural regions:

```text
Deterministic functional core
    decides what the application means and what should happen next

Imperative shell
    ├── application host
    │   performs I/O, owns services, coordinates concurrency, and manages caches
    │
    └── Qt presentation adapter
        owns widgets, windows, Qt models, actions, focus, layout, and event translation
```

The governing rule is:

> **The core decides. The host performs. Qt presents.**

A user action begins as a Qt event. The Qt adapter translates it into a semantic intent. The application host supplies any bounded observations the decision requires and invokes the deterministic core. The core updates compact interaction state and emits semantic deltas, diagnostics, and requests. The host performs the requests and feeds their results back as events. The Qt adapter applies the resulting projections and deltas to the interface.

This is compatible with MVC, but stricter than ordinary MVC. MVC divides user-interface responsibilities; this pattern also specifies effect ownership, dependency direction, determinism, concurrency boundaries, and performance behavior.

---

## 1. Goals

This pattern is intended to produce applications that are:

- straightforward to unit test without launching a GUI;
- responsive under large data sets and high-frequency input;
- explicit about authoritative state and side effects;
- resistant to business logic accumulating in `QWidget` subclasses;
- able to use Qt fully without making the entire application Qt-shaped;
- safe around asynchronous work, cancellation, and stale results;
- incrementally extensible without requiring a single giant controller;
- practical in C++, including in-place mutation, caches, scratch storage, and parallel work where appropriate.

### Non-goals

This pattern does **not** require:

- immutable data structures everywhere;
- copying the complete application state for every event;
- one global Redux-style reducer;
- serializing in-process calls through JSON or RPC;
- hiding Qt behind a fake universal widget abstraction;
- moving every visual decision into the functional core;
- prohibiting caches, arenas, object pools, or retained scratch memory;
- creating a general application framework before the first feature exists.

---

## 2. Design philosophy

### 2.1 Determinism matters more than ideological purity

For a small application, a value-returning reducer may be ideal:

```cpp
[[nodiscard]] auto reduce(
    const application_state& aState,
    const application_event& aEvent) -> transition_result;
```

For a large or latency-sensitive application, deterministic in-place transitions are usually a better fit:

```cpp
[[nodiscard]] auto transition(
    application_state& aState,
    const application_event& aEvent,
    const observation_view& aObservations,
    transition_storage& aStorage) -> transition_result;
```

The important property is:

```text
same prior state
+ same event
+ same observations
= same resulting state, deltas, requests, and diagnostics
```

The core may mutate memory explicitly passed to it. It may use preallocated output buffers and scratch storage. What it must not do is perform hidden I/O, consult hidden globals, read the wall clock implicitly, invoke Qt, start threads, or mutate external services.

BPR's owner-approved SHA-256 implementation (RFC-BPR-0080 r4, 2026-10-18)
may call CommonCrypto on macOS or CNG on Windows, with HACL as the portable
fallback. Determinism concerns observable digest bytes for the same input.
CPU-feature dispatch, allocation and internal synchronization in the system
implementation do not themselves change those bytes. This exception admits
only unkeyed SHA-256 computation, with native types private to its wrapper;
it grants no clock, entropy, credential, filesystem or other OS capability.

BPR's pinned RE2/Abseil source matcher has a separate scoped exception
(BPR-1618, owner 2026-10-18). It starts no worker threads. Its private caches,
hash seeds, synchronization and contention clocks may vary timing and layout,
while spans, ordering and diagnostics remain externally deterministic. Expected
error logging is disabled; upstream invariant-failure diagnostics remain possible.
This does not grant first-party core clock, thread, entropy or logging APIs.

Duration and time-point arithmetic may use `std::chrono` without reading a
clock. Caller-owned PRNG state may use an explicit seed. Implicit entropy
and global random state remain effects. A reproducible cross-platform value
also requires a specified algorithm and mapping; standard distributions can
vary across standard-library implementations.

### 2.2 Keep the core small in ownership and substantial in decision-making

The core should own the state and policies needed to make application decisions. It should not automatically own every byte the application can access.

A major application may have gigabytes of documents, indexes, thumbnails, render resources, database pages, or network state. Copying or mirroring those into a functional state tree is unnecessary. The core can operate on stable identities and bounded observations supplied by the host.

### 2.3 One authority per category of state

For every category of state, identify one authority:

- the canonical document;
- the current user selection;
- the persisted settings;
- the active asynchronous request generation;
- the presentation model;
- the cache entry;
- the external service session.

Do not let a widget, a domain document, and a cache each become competing mutable authorities for the same fact.

### 2.4 Separate semantic decisions from presentation mechanics

The core may decide:

- whether deletion is legal;
- which item becomes active after deletion;
- whether a command is enabled;
- whether an asynchronous result is stale;
- how a multi-selection operation is interpreted;
- which semantic operation should be committed.

The Qt adapter may decide:

- whether the command appears in a menu, toolbar, or context menu;
- how text is elided;
- whether a notification is a toast or status-bar message;
- how rows are painted;
- which widget receives focus;
- how a popup is positioned;
- which animation or transition presents a state change.

### 2.5 Optimize around the active set and changed set

Routine work should scale approximately with:

```text
active set + changed set
```

not:

```text
total document size + total project size
```

A selection change should not rebuild an entire tree. A pointer move should not rewrite the canonical document. A search completion should not replace unrelated models. A toolbar toggle should not scan every object in the application.

---

## 3. Architecture

```text
┌──────────────────────────────────────────────────────────────┐
│ Imperative shell                                             │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Qt presentation adapter                                │  │
│  │                                                        │  │
│  │ QApplication · windows · docks · widgets · actions     │  │
│  │ QAbstractItemModel · delegates · focus · drag/drop     │  │
│  │ accessibility · themes · native events · rendering UI  │  │
│  └───────────────┬───────────────────────────▲────────────┘  │
│                  │ semantic intents          │ projections    │
│                  ▼                           │ and deltas      │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Application host                                       │  │
│  │                                                        │  │
│  │ composition · authority sessions · persistence         │  │
│  │ files · network · clocks · jobs · caches · diagnostics │  │
│  │ request execution · cancellation · lifecycle           │  │
│  └───────────────┬───────────────────────────▲────────────┘  │
│                  │ bounded observations      │ requests       │
└──────────────────┼───────────────────────────┼─────────────────┘
                   ▼                           │
        ┌─────────────────────────────────────────────┐
        │ Deterministic functional core               │
        │                                             │
        │ interaction state · policy · planners       │
        │ validation · command legality · transitions │
        │ semantic deltas · request generations       │
        └─────────────────────────────────────────────┘
```

The Qt presentation adapter is part of the imperative shell. It is not a third architectural peer outside the shell.

---

## 4. State taxonomy

A reliable implementation starts by classifying state before writing controllers.

| State category | Typical owner | Examples |
|---|---|---|
| Authoritative domain state | Domain service or application host | Documents, records, database rows, accepted transactions |
| Interaction state | Functional core | Selection, active tool, current mode, pending gesture, navigation context |
| Bounded observations | Host, borrowed by core | Selected records, camera facts, permission facts, query results, snapshots |
| Mutable caches and indexes | Application host | Search index, decoded records, thumbnails, bounds, row mappings |
| Presentation projections | Host or Qt adapter | Visible rows, inspector cards, command state, status summaries |
| Qt-local state | Qt adapter | Focus, expanded branches, scroll position, geometry, hover, animations |
| External resource state | Application host and adapters | Files, sockets, database handles, worker pools, renderer sessions |
| Pending-work identity | Core and host | Request IDs, generations, revisions, cancellation state |

### 4.1 When domain state may live in the core

It is reasonable for the domain model to live in the core when it is:

- small enough to copy or update cheaply;
- naturally in-memory;
- free of external ownership or transactional constraints;
- useful to exercise directly in deterministic tests.

Keep domain state outside the core when it is:

- very large or partially resident;
- owned by a database, document engine, server, or other authority;
- concurrently accessed through snapshots;
- expensive to duplicate;
- already governed by an independent transaction or revision model.

In that case, the core stores stable identities and receives only the observations required for the current decision.

---

## 5. Functional core

### 5.1 Responsibilities

The functional core should own deterministic application decisions such as:

- interaction state transitions;
- selection rules;
- command legality and enablement;
- validation and policy decisions;
- mode and tool transitions;
- gesture interpretation;
- deterministic transformation and planning algorithms;
- construction of semantic command proposals;
- request generation and correlation state;
- acceptance or rejection of asynchronous completions;
- derivation of semantic dirty domains and compact projections;
- reason-coded diagnostics for rejected actions.

### 5.2 Prohibited dependencies

The core should normally contain no dependency on:

- `QObject`, `QWidget`, `QString`, `QVariant`, `QModelIndex`, or Qt event types;
- the filesystem;
- sockets or HTTP clients;
- database connections;
- wall-clock access that is not passed explicitly;
- random-number generation that is not passed explicitly or seeded explicitly;
- threads, executors, futures, timers, or event loops;
- dialogs, notifications, or clipboard APIs;
- mutable service locators or process-wide singletons.

Using a Qt-free core is not about disliking Qt. It creates a clean semantic boundary and makes the core usable in tests, command-line tools, alternate frontends, and headless automation.

### 5.3 Core vocabulary

Use a small set of explicit concepts.

#### Intent

A semantic statement of what the user or another actor is trying to do.

```cpp
struct delete_selection_intent {};
struct rename_item_intent
{
    item_id Item;
    std::string NewName;
};
struct query_changed_intent
{
    std::string Text;
};
```

An intent should not be a raw `QMouseEvent` or a string command such as `"delete"`.

#### Event

A fact that has occurred and may require a state transition.

```cpp
struct search_completed_event
{
    request_id Request;
    request_generation Generation;
    std::vector<search_hit> Hits;
};

struct save_failed_event
{
    request_id Request;
    save_error Error;
};
```

User intents may be represented as one event family, but it is useful to preserve the semantic distinction between “requested” and “completed.”

#### Observation

An immutable, bounded fact supplied by the host for the current transition.

```cpp
struct selection_observation
{
    std::span<const selected_item_basis> Items;
    source_revision Revision;
};

struct permission_observation
{
    bool CanDelete;
    bool CanRename;
};
```

Observations should expose the information the core needs, not the host service that can be queried for anything.

#### Request

A semantic external action the host must attempt.

```cpp
struct load_document_request
{
    request_id Id;
    document_locator Locator;
};

struct run_search_request
{
    request_id Id;
    request_generation Generation;
    std::string Query;
};

struct choose_save_destination_request
{
    request_id Id;
    suggested_destination Suggestion;
};
```

A request should say “choose a save destination,” not “show a `QFileDialog`.” The host and presentation adapter decide how to fulfill it.

#### Delta

A compact description of semantic changes.

```cpp
enum class changed_domain : std::uint32_t
{
    None         = 0,
    Selection    = 1u << 0,
    Commands     = 1u << 1,
    Search       = 1u << 2,
    Document     = 1u << 3,
    Status       = 1u << 4
};

struct selection_delta
{
    std::span<const item_id> Added;
    std::span<const item_id> Removed;
    std::optional<item_id> NewActive;
};
```

Deltas let the host and Qt adapter update only what changed.

### 5.4 Example core API

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

`transition_storage` can own retained vectors, inline buffers, arenas, or scratch memory. This permits allocation-free hot paths without hiding effects.

### 5.5 Compose feature cores instead of building one giant reducer

```text
application_core
├── session
├── selection
├── commands
├── navigation
├── search
├── editing
├── history
└── validation
```

The top-level dispatcher should mostly route events and coordinate cross-feature invariants. Feature-specific logic belongs in focused transition functions and planners.

Avoid a single source file that knows every event, every field, every panel, and every external request in the application.

### 5.6 The core should not emit widget commands

Avoid outputs such as:

```text
set_line_edit_text
show_popup
hide_popup
select_tree_row
repaint_widget
focus_button
```

Those commands encode a specific widget implementation and turn the core into an abstract Qt controller.

Prefer semantic output:

```text
query text accepted
suggestions changed
active suggestion changed
selection changed
validation failed
save destination required
```

The Qt adapter decides how those facts appear.

---

## 6. Application host

### 6.1 Responsibilities

The application host is the imperative orchestrator. It should own:

- the composition root for application services;
- authoritative document or session handles;
- database, filesystem, network, renderer, and platform adapters;
- caches, indexes, and residency;
- asynchronous scheduling and cancellation;
- request execution;
- transaction submission and result handling;
- persistence, recovery, and settings;
- logging, metrics, and operational diagnostics;
- lifecycle and shutdown ordering;
- conversion between external records and bounded core observations;
- publication of projections to the Qt adapter.

The host may be mutable and concurrent. Its discipline comes from explicit ownership and serialized decision application, not from purity.

### 6.2 The host is not a second business-logic layer

The host should decide operational matters:

- which provider executes a request;
- which cache can satisfy an observation;
- whether work runs synchronously or asynchronously;
- how to retry an I/O failure;
- how to map a native error into a typed application error;
- how to schedule publication back to the UI thread.

The host should not duplicate semantic policy already owned by the core:

- whether deletion is allowed;
- which item becomes selected after a move;
- whether a result belongs to the current query;
- how a multi-item edit is interpreted;
- whether a user action represents one or several semantic commands.

### 6.3 Capability-specific ports

Define narrow, stable ports around capabilities:

```cpp
class document_port
{
public:
    virtual ~document_port() = default;

    virtual auto load(document_locator aLocator) -> load_result = 0;
    virtual auto submit(const semantic_command& aCommand) -> submit_result = 0;
    virtual auto observe(const observation_query& aQuery) -> observation_result = 0;
};

class search_port
{
public:
    virtual ~search_port() = default;

    virtual void submit(search_work aWork, search_completion aCompletion) = 0;
    virtual void cancel(request_id aRequest) = 0;
};
```

Do not create one universal `application_services` object with hundreds of unrelated methods. Do not create a one-method interface for every trivial function. Group operations by ownership, lifetime, threading model, and replacement boundary.

### 6.4 Direct typed calls in-process

Inside one process, prefer:

- typed C++ records;
- direct calls;
- spans and views;
- explicit result types;
- compile-time command variants when useful.

Do not add JSON, sockets, generic RPC envelopes, or string-based dispatch merely because a future process boundary is possible. Add serialization at the actual process boundary.

### 6.5 Authority and transactions

When a domain service already owns canonical state, the host should submit semantic commands to that authority and wait for acceptance or rejection.

```text
preview
    local, transient, reversible, not canonical

commit proposal
    deterministic semantic command prepared by the core

accepted result
    canonical authority advances revision

rejected result
    core receives a reason-coded event and discards or repairs preview state
```

Do not let the UI mutate a local mirror and later “save whatever it currently looks like” unless the application is intentionally built around that model.

---

## 7. Qt presentation adapter

### 7.1 Responsibilities

Use Qt directly and confidently at the presentation edge. The Qt adapter owns:

- `QApplication` and application event integration;
- windows, dialogs, docks, tabs, splitters, widgets, and layouts;
- `QAction`, menus, toolbars, command shortcuts, and context menus;
- `QAbstractItemModel`, proxy models, delegates, and selection models;
- focus, hover, drag-and-drop, clipboard, accessibility, and input methods;
- theme, palette, icons, high-DPI behavior, and platform conventions;
- native presentation surfaces and visual-only rendering state;
- translation of Qt events into semantic intents;
- application of projections and deltas to Qt objects;
- visual-only state such as expanded rows, scroll positions, and animation progress.

### 7.2 What the Qt adapter should not own

Avoid placing these responsibilities in widgets or models:

- canonical business data;
- file or database persistence;
- domain command legality;
- asynchronous job orchestration;
- stale-result policy;
- transaction history;
- cross-window application workflow;
- retry policy;
- long-lived service discovery;
- hidden caches that become alternate authorities.

### 7.3 Signals and slots are transport, not architecture

A signal may carry a semantic intent from a widget to an adapter or host. A slot may apply a projection. That does not mean the semantic workflow should be implemented as an untraceable web of signal-to-signal connections.

Prefer a visible path:

```text
QAction::triggered
    → adapter constructs rename_item_intent
    → host dispatches intent
    → core transitions
    → host executes requests
    → adapter applies command-state and model deltas
```

### 7.4 Use `QAction` as the presentation of command identity

Each application command should have one semantic identity. The core or application layer supplies command state:

```cpp
struct command_projection
{
    command_id Id;
    bool Enabled;
    bool Checked;
    std::optional<reason_code> DisabledReason;
};
```

The Qt adapter binds that identity to one or more `QAction` instances. Menus, toolbars, shortcuts, and context menus reuse the same command state instead of each reimplementing enablement logic.

### 7.5 Qt models are projections, not canonical domain models

A `QAbstractItemModel` should adapt stable application identities into Qt indexing and role data. It may cache row mappings and presentation strings. It should not silently become the only owner of the domain object graph.

Guidelines:

- retain stable item identities separate from row numbers;
- maintain identity-to-row or identity-to-node mappings;
- apply insert, remove, move, and data-change deltas incrementally;
- avoid `beginResetModel()` for selection, focus, or small field changes;
- preserve persistent indexes when reasonably possible;
- keep selection in semantic IDs and synchronize `QItemSelectionModel` with `QSignalBlocker`;
- do not retain raw pointers to domain objects across revisions unless their lifetime is guaranteed;
- let Qt virtualize large views rather than constructing one widget per record.

### 7.6 Keep host-owned UI policy out of reusable feature widgets

Reusable widgets should not directly call:

- `QFileDialog` for application persistence;
- application-specific modal wrappers;
- global error reporters;
- process-wide service registries;
- unrelated top-level windows.

A feature may emit a semantic request or use a small host-facing contract. The application composition root decides which dialog, persistence provider, and error policy fulfills it.

### 7.7 Do not overabstract Qt

The goal is not to make Qt disappear.

Bad abstraction:

```text
IWidget
IButton
ILayout
IString
IMenu
```

This creates a second, weaker GUI toolkit and makes ordinary Qt work difficult.

Better boundary:

- core and application records use standard or domain-specific types;
- conversion occurs at the Qt adapter boundary;
- the adapter uses Qt idiomatically;
- Qt-specific tests verify the adapter;
- most semantic tests never instantiate Qt.

---

## 8. End-to-end data flow

### 8.1 Synchronous action

```text
1. User triggers a QAction.
2. Qt adapter emits a semantic intent.
3. Host obtains any required bounded observations.
4. Core applies a deterministic transition.
5. Core returns state changes, deltas, requests, and diagnostics.
6. Host executes immediate requests or schedules work.
7. Host publishes changed projections.
8. Qt adapter updates only affected actions, models, and widgets.
```

### 8.2 Asynchronous action

```text
1. Core emits a request with request_id and generation.
2. Host captures immutable input and schedules work.
3. Worker performs the operation without touching Qt or core state.
4. Completion is marshalled onto the serialized application lane.
5. Completion becomes a typed event.
6. Core accepts it only if document/session/revision/generation still match.
7. Host updates caches and publishes deltas.
8. Qt adapter applies the result.
```

### 8.3 High-frequency gesture

```text
pointer down
    capture stable gesture basis and reserve buffers

pointer move
    coalesce to the latest useful sample
    update transient preview from the captured basis
    publish only dirty presentation data

pointer up
    construct one semantic commit proposal
    submit once to the authoritative domain service

cancel or capture loss
    discard preview
    produce no canonical command
```

---

## 9. Asynchrony, concurrency, and stale work

### 9.1 Use a serialized decision lane

Application decisions should normally be applied in a single ordered lane, often the GUI thread or a dedicated application thread. Heavy work may run in parallel, but completions re-enter through typed events.

This avoids concurrent mutation of interaction state while still allowing expensive searches, parsing, validation, indexing, and computation to scale.

### 9.2 Version every asynchronous request

A request should carry enough identity to prove that its result is still relevant:

```cpp
struct request_context
{
    request_id Request;
    session_id Session;
    source_revision Revision;
    request_generation Generation;
};
```

The exact fields depend on the product. Common dimensions include:

- application session;
- document identity;
- document revision;
- active query generation;
- viewport or panel identity;
- gesture identity;
- user account or workspace identity.

Reject stale completions in constant time. Do not attempt to repair them by applying them to whichever widget happens to still exist.

### 9.3 Cancellation is cooperative and semantic

Cancellation may mean:

- stop work immediately if possible;
- stop publishing results;
- allow the worker to finish but discard its completion;
- supersede an earlier generation.

Treat cancellation as a normal outcome, not automatically as an error.

### 9.4 Workers never mutate Qt

Worker code should not:

- access widgets;
- retain `QModelIndex` values;
- emit direct calls into destroyed windows;
- mutate the functional core;
- update application caches without the cache’s declared synchronization protocol.

Marshal completion records to the host, then update Qt on the appropriate thread.

### 9.5 Deterministic executor for tests

Provide a test scheduler that can:

- queue submitted work;
- complete tasks in chosen order;
- delay selected completions;
- inject failures;
- deliver stale completions deliberately;
- drain one task or all tasks deterministically.

This turns race-prone workflows into reproducible unit and integration tests.

---

## 10. Performance engineering

### 10.1 Hot-path rules

For latency-sensitive interactions:

- reserve storage when the gesture begins;
- avoid heap allocation during established high-frequency updates;
- operate over contiguous spans where possible;
- recompute from captured starting state rather than accumulating drift;
- coalesce pointer, scroll, resize, and text-change events when intermediate samples cannot be presented;
- publish dirty domains instead of complete application snapshots;
- use direct typed calls on the local decision lane;
- defer expensive materialization, parsing, and validation;
- avoid whole-document scans during routine commands;
- do not persist or transact on every pointer move.

### 10.2 Mutable caches are expected

The host may need:

- decoded-record caches;
- hierarchy or graph indexes;
- search indexes;
- thumbnail caches;
- source-to-view mappings;
- formatting caches;
- geometry or bounds caches;
- network response caches;
- schema and metadata caches.

These are not a violation of the pattern. They are imperative implementation resources. Keep their authority subordinate to the canonical source and make invalidation explicit.

### 10.3 Incremental projections

Do not derive every panel from scratch after every event.

Prefer:

```cpp
struct projection_update
{
    changed_domain Changed;
    std::optional<selection_delta> Selection;
    std::optional<row_delta_batch> Rows;
    std::optional<command_delta_batch> Commands;
    std::optional<status_projection> Status;
};
```

The adapter then touches only affected views.

### 10.4 Avoid accidental distributed-system costs

Do not serialize or queue every call just to make components look “decoupled.” In-process boundaries can use ordinary function calls. Use process protocols only where process isolation, remote execution, crash containment, or independent deployment provides real value.

### 10.5 Measure representative workflows

Build benchmarks around actual interaction shapes:

| Family | Representative cases |
|---|---|
| Core transitions | tool/mode switch, selection change, command enablement |
| High-frequency preview | 1, 100, 1,000, and stress-case items |
| Large models | insert, remove, move, selection, filter, and partial refresh |
| Search | query update, cancellation, stale completion, large result set |
| Persistence | clean save, dirty save, failure, recovery, large document |
| Async | completion ordering, cancellation, supersession, shutdown |
| Qt adapter | action updates, model deltas, focus flow, high-DPI layout |

Use structural gates before arbitrary universal timing claims:

- no whole-document scan;
- no model reset for a small change;
- no allocation in an established hot loop within retained capacity;
- no blocking I/O on the GUI thread;
- no continuous repaint while idle;
- no stale completion applied to current state.

Then record platform-specific latency baselines and regression thresholds.

---

## 11. Error and diagnostic model

### 11.1 Separate semantic rejection from operational failure

Semantic rejection:

```text
rename rejected because the name is invalid
command disabled because selection is locked
operation conflicts with a newer source revision
```

Operational failure:

```text
file write failed
network request timed out
database connection was lost
provider returned malformed data
```

The core usually owns semantic rejection. The host owns operational failure classification and retry mechanics. The Qt adapter owns presentation.

### 11.2 Use typed reason codes

```cpp
enum class rename_rejection
{
    Empty,
    Duplicate,
    Reserved,
    ReadOnly,
    StaleRevision
};
```

Keep localized or user-facing prose outside low-level logic. Preserve structured cause information for logs, telemetry, and tests.

### 11.3 Diagnostics are not automatically dialogs

The core should emit a diagnostic fact. The presentation policy decides whether it becomes:

- inline validation;
- a status-bar message;
- a toast;
- a problems panel entry;
- a modal dialog;
- a log-only record.

### 11.4 Contain exceptions at imperative boundaries

If external libraries throw, catch and translate exceptions in the host or adapter boundary. Do not allow arbitrary exceptions to unwind through Qt event delivery or cross worker-thread boundaries without policy.

---

## 12. Undo, redo, preview, and commit

### 12.1 Separate preview from canonical state

Interactive previews are usually:

- transient;
- local;
- cancellable;
- high frequency;
- not yet accepted by the domain authority.

Canonical commits are:

- semantic;
- validated;
- revisioned;
- recorded in history;
- usually one command per completed user gesture.

### 12.2 One gesture should normally produce one history entry

A drag containing 500 pointer events should not create 500 undo entries. Capture the starting basis, update preview locally, then commit one semantic operation on completion.

### 12.3 Put history beside the canonical mutation model

If the domain layer has transactions or semantic commands, build undo/redo there or in the application host coordinating it. Avoid storing complete GUI snapshots as history unless the application is genuinely small enough that this is the simplest correct design.

### 12.4 Interaction state is not automatically undoable

Selection, focus, expanded rows, hover, and temporary filters are usually workspace state, not domain history. Make that distinction explicit.

---

## 13. Lifecycle and ownership

### 13.1 Use a composition root

Construct the application in one visible place:

```text
main
    creates providers and adapters
    creates application host
    creates deterministic core state
    creates Qt presentation adapter
    wires typed callbacks and publications
    starts the event loop
```

Avoid hidden singleton initialization and service discovery through global registries.

### 13.2 Keep QObject ownership in the Qt adapter

Let Qt parent-child ownership manage Qt objects. The application host should not need to understand widget lifetimes beyond explicit presentation interfaces or weak registration tokens.

### 13.3 Define shutdown ordering

A robust shutdown sequence is typically:

```text
1. Stop accepting new user and external ingress.
2. Mark sessions as closing.
3. Cancel or supersede outstanding work.
4. Prevent new publications to destroyed presentations.
5. Drain or detach workers according to policy.
6. Flush or abandon persistence according to explicit rules.
7. Destroy Qt presentation objects.
8. Destroy host adapters and external services.
9. Destroy core state.
```

Do not rely on `QPointer` or weak pointers to compensate for unclear ownership and unrestricted callbacks.

---

## 14. Testing strategy

### 14.1 Functional core tests

Core tests should require no `QApplication`.

Use:

- table-driven transition tests;
- invariant and property tests;
- event-sequence replay tests;
- fuzz tests for parsers and state machines;
- deterministic randomized tests with recorded seeds;
- direct algorithm benchmarks;
- snapshot tests for structured projections when stable and useful.

Example:

```cpp
TEST(selection, deleting_active_item_selects_next_visible_item)
{
    application_state state = fixture_with_selection(item_id{ 12 });
    observation_fixture observations = visible_items({ 10, 11, 12, 13 });
    transition_storage storage;

    const auto result = transition(
        state,
        delete_selection_intent{},
        observations.view(),
        storage);

    EXPECT_EQ(state.Selection.Active, item_id{ 13 });
    EXPECT_TRUE(has_request<submit_delete_command>(result));
}
```

### 14.2 Host tests

Test the imperative host with fake or in-memory ports:

- successful request execution;
- each relevant failure mode;
- cancellation;
- completion ordering;
- stale-result rejection;
- retry and backoff policy;
- temporary filesystem or database integration;
- cache invalidation;
- shutdown while work is pending;
- recovery after partial operations.

Use a manual executor so tests control when work completes.

### 14.3 Qt adapter tests

Qt tests should focus on the adapter contract:

- a Qt event produces the correct semantic intent;
- a command projection updates `QAction` state;
- a row delta produces the correct `QAbstractItemModel` notifications;
- selection synchronization does not recurse;
- focus and escape behavior are correct;
- drag/drop payloads translate correctly;
- dialogs return typed results;
- accessibility names and keyboard navigation exist;
- themes and high-DPI layouts remain usable.

Do not retest every business rule through widget clicks when the core already has direct deterministic coverage.

### 14.4 End-to-end tests

Keep a smaller number of full workflow tests for:

- launch and composition;
- open/edit/save/reopen;
- major error recovery;
- representative asynchronous workflows;
- multi-window lifecycle;
- packaging and platform integration.

### 14.5 Replay is a first-class diagnostic tool

Record semantic events and observations for difficult interaction failures. A replay fixture should be able to reproduce the core transition sequence without the original GUI timing.

---

## 15. Recommended module layout

A medium or large application can use:

```text
src/
├── core/
│   ├── state/
│   ├── events/
│   ├── transitions/
│   ├── planners/
│   └── projections/
│
├── application/
│   ├── host/
│   ├── ports/
│   ├── requests/
│   ├── sessions/
│   └── caches/
│
├── adapters/
│   ├── qt/
│   │   ├── windows/
│   │   ├── widgets/
│   │   ├── models/
│   │   ├── actions/
│   │   └── dialogs/
│   ├── filesystem/
│   ├── database/
│   └── network/
│
└── app/
    └── main.cpp

tests/
├── core/
├── application/
├── adapters_qt/
└── end_to_end/
```

A small application should use fewer directories:

```text
feature/
├── core/
├── host/
└── qt/
```

Do not fragment a feature into dozens of targets before its ownership boundaries are real.

### 15.1 Dependency direction

```text
core
    depends only on standard/domain vocabulary

application host
    depends on core and port contracts

Qt adapter
    depends on application-facing contracts and Qt

external adapters
    depend on their port contracts and provider libraries

composition root
    depends on all concrete pieces and wires them together
```

The core must never depend upward on the host or Qt adapter.

---

## 16. Worked example: asynchronous search

### 16.1 Qt input

```cpp
connect(SearchEdit, &QLineEdit::textChanged, this, [this](const QString& aText) {
    Host.dispatch(query_changed_intent{ aText.toStdString() });
});
```

The adapter translates text. It does not run the search or decide whether an old result remains valid.

### 16.2 Core transition

```text
query_changed("alpha")
    update query text
    increment query generation
    clear active result if policy requires
    emit run_search_request(request_id, generation, "alpha")
    mark Search and Commands dirty
```

### 16.3 Host execution

```text
capture request identity and immutable query
schedule search on worker
optionally cancel superseded work
marshal completion to application lane
```

### 16.4 Completion

```text
search_completed(request, generation, hits)
    if generation != current generation:
        ignore
    else:
        replace semantic result projection
        preserve or repair active selection deterministically
        mark Search and Commands dirty
```

### 16.5 Qt update

The model adapter applies row deltas and restores semantic selection by stable identity. It does not reset unrelated panels.

---

## 17. Worked example: Save As

```text
User triggers Save As
    ↓
Qt adapter dispatches save_as_intent
    ↓
Core verifies command legality and enters AwaitingDestination
    ↓
Core emits choose_save_destination_request
    ↓
Host asks the Qt presentation adapter to collect a destination
    ↓
User cancels
    → destination_cancelled_event
    → core returns to Idle with no error

User accepts
    → destination_chosen_event
    → core emits persist_document_request
    → host saves through persistence port
    → save_completed_event or save_failed_event
    → core updates dirty/checkpoint state
    → Qt updates title, actions, and diagnostics
```

The core never invokes `QFileDialog`. The reusable feature widget never owns filesystem policy. Cancellation is a normal event.

---

## 18. Worked example: drag or canvas manipulation

```text
Begin
    Qt translates pointer-down and hit information
    Host supplies selected-item bases and coordinate observations
    Core captures gesture basis and active constraint

Update
    Qt coalesces pointer movement
    Core computes preview from original basis into retained output storage
    Host updates presentation cache
    Qt or renderer displays preview

Commit
    Core produces one semantic operation proposal
    Host submits it to canonical authority
    Accepted result advances revision and history

Cancel
    Core discards gesture state
    Host drops preview
    Canonical state is unchanged
```

This pattern applies to reordering, resizing, timeline scrubbing, graph editing, diagram manipulation, and spatial transforms.

---

## 19. Decision table: where should this logic live?

| Question | Owner |
|---|---|
| Is the Delete command legal? | Functional core |
| Which item becomes active after deletion? | Functional core |
| What does a multi-selection edit mean? | Functional core |
| Is an async result stale? | Functional core, using explicit identities |
| Which provider performs the save? | Application host |
| How is the document serialized? | Domain or persistence adapter |
| Should a failed save retry? | Application host policy |
| Which directory does a file dialog initially show? | Host plus Qt adapter |
| Is the error inline, modal, or a toast? | Qt presentation policy |
| What color is the selected row? | Qt presentation adapter |
| How is a large list virtualized? | Qt presentation adapter |
| How is a domain item mapped to a row? | Qt model adapter/cache |
| How is search ranked? | Core algorithm or deterministic domain service |
| Where is the search index stored? | Application host cache/service |
| How is debounce timed? | Host or Qt adapter; the core tracks semantic generation |
| Who owns worker threads? | Application host |
| Who owns `QObject` lifetime? | Qt presentation adapter |
| Who owns canonical undo history? | Domain authority or application host |
| Who owns hover and animation progress? | Qt presentation adapter |

---

## 20. Common failure modes

| Smell | Consequence | Correction |
|---|---|---|
| A `QWidget` owns the document, controller, persistence, services, selection, and caches | Hard-to-test monolith and unclear authority | Move decisions to feature cores and effects to the host |
| Core headers include Qt types | Semantic layer becomes toolkit-bound | Translate Qt types at the adapter boundary |
| Core emits `show_popup` or `set_text` effects | Core encodes widget implementation | Emit semantic state, requests, and projections |
| Every event rebuilds all widgets | Latency, flicker, lost selection | Publish dirty domains and incremental deltas |
| `beginResetModel()` is used for small changes | Lost persistent indexes and expensive redraw | Use insert/remove/move/data-change notifications |
| Worker completion mutates a widget directly | Lifetime races and stale UI | Return typed completion to the host’s serialized lane |
| One global reducer handles every feature | High coupling and unreviewable transitions | Compose bounded feature cores |
| Every state transition copies the whole document | Poor scaling | Keep compact interaction state and borrow observations |
| Purity rules prohibit caches or scratch storage | Needless allocation and recomputation | Keep mutable implementation resources in host/storage |
| A universal effect bus uses strings or maps | Weak contracts and hidden coupling | Use typed requests and result events |
| A giant service locator is passed everywhere | Any code can perform any effect | Use capability-specific ports and bounded observations |
| Feature widgets invoke file dialogs and global error wrappers | Reuse and host policy become coupled | Route semantic requests through the host |
| Command enablement is duplicated in slots and menus | Inconsistent behavior | Publish one command projection per semantic command |
| Async work lacks generation or revision identity | Old results overwrite current state | Version requests and reject stale completions |
| Serialization is used for every in-process call | Distributed-system overhead without benefit | Use direct typed calls until a real process boundary exists |
| Preview mutates canonical state continuously | Huge history, cancellation complexity, I/O pressure | Keep preview transient and commit once |

---

## 21. Relationship to MVC and related patterns

This architecture can be described as a disciplined form of MVC:

- Qt widgets and models provide the View and input adaptation;
- deterministic feature cores contain much of the Controller logic and interaction model;
- authoritative domain services and core state form different parts of the Model;
- the application host provides application services and effect orchestration.

The useful distinction is that ordinary MVC does not require:

- deterministic transitions;
- explicit observations and requests;
- a prohibition on hidden effects;
- toolkit-free decision logic;
- versioned asynchronous completions;
- incremental publication;
- one authority per state category.

It also combines ideas from:

- functional core / imperative shell;
- ports and adapters;
- unidirectional data flow;
- command processing;
- read-model or projection patterns;
- event-driven application design.

Do not spend effort inventing a proprietary pattern name. The value is in the enforced boundaries, not the label.

---

## 22. Adoption sequence

### Step 1: Choose one complete workflow

Start with a narrow vertical slice such as:

- open, edit, save;
- asynchronous search and selection;
- list reorder with undo;
- property editing with validation;
- a canvas gesture with preview and commit.

### Step 2: Identify authorities

Write down who owns:

- canonical data;
- interaction state;
- history;
- cache entries;
- settings;
- external sessions;
- Qt object lifetime.

### Step 3: Define semantic intents and results

Translate raw UI events into application vocabulary. Avoid premature generic event buses.

### Step 4: Extract deterministic decisions

Move policy, transitions, and planners into Qt-free functions. Leave visual mechanics in Qt.

### Step 5: Introduce explicit host requests

Represent I/O and asynchronous work as typed semantic requests and result events.

### Step 6: Build fake ports and a manual scheduler

Prove workflows without real files, networks, databases, or timers.

### Step 7: Add incremental projections

Replace full refreshes with dirty domains and stable-identity deltas where scale requires it.

### Step 8: Measure real hot paths

Benchmark representative data sizes. Add preallocation, coalescing, caches, and parallelism only where evidence supports them.

### Step 9: Enforce dependency boundaries

Use build targets and automated checks so the core cannot accidentally depend on Qt or provider libraries.

### Step 10: Expand feature by feature

Do not halt product development to construct a universal framework. Generalize only after two or more real features demonstrate the same stable shape.

---

## 23. Design review checklist

### Core

- [ ] Can core tests run without `QApplication`?
- [ ] Are all non-deterministic inputs explicit observations?
- [ ] Does the core avoid filesystem, network, database, clock, thread, and Qt calls?
- [ ] Is interaction state compact relative to total application data?
- [ ] Are feature transitions bounded rather than centralized in one giant reducer?
- [ ] Are outputs semantic rather than widget commands?
- [ ] Can stale asynchronous results be rejected using explicit identity?
- [ ] Does a completed gesture produce one semantic commit where appropriate?

### Host

- [ ] Is there one authority for each category of canonical state?
- [ ] Are ports grouped by real capabilities and ownership boundaries?
- [ ] Are caches explicitly invalidated and subordinate to authority?
- [ ] Is expensive work kept off the GUI thread?
- [ ] Do completions re-enter through a serialized application lane?
- [ ] Are cancellation and shutdown behaviors defined?
- [ ] Are operational failures translated into typed results?
- [ ] Are in-process calls direct and typed rather than serialized without need?

### Qt adapter

- [ ] Do widgets translate events rather than own business workflow?
- [ ] Are `QAction` states driven by one semantic command projection?
- [ ] Are Qt models incremental and identity-based?
- [ ] Is semantic selection separate from row/index identity?
- [ ] Are dialogs and error presentation host policies rather than feature persistence?
- [ ] Is Qt-local state clearly distinguished from domain and interaction state?
- [ ] Are worker callbacks prevented from touching Qt directly?
- [ ] Are accessibility, focus, keyboard, and high-DPI behaviors tested?

### Performance

- [ ] Does routine work scale with the active or changed set?
- [ ] Are high-frequency events coalesced where intermediate samples are useless?
- [ ] Are established hot paths allocation-free within retained capacity where justified?
- [ ] Are complete model resets and document scans absent from routine operations?
- [ ] Are idle views actually idle?
- [ ] Are representative benchmarks and structural performance gates present?

---

## 24. When to use this pattern

It is especially useful for:

- long-lived desktop applications;
- editors and authoring tools;
- data browsers with large models;
- applications with asynchronous search or analysis;
- applications with multiple windows or frontends;
- tools that need headless automation;
- interfaces with complex gestures, preview, commit, and undo;
- systems where the canonical model is owned by a database, service, or document engine.

Use a lighter form for:

- small single-screen utilities;
- simple CRUD tools with little policy;
- throwaway prototypes;
- interfaces whose only purpose is to invoke a few independent commands.

Even then, the smallest useful rule remains:

> Keep business decisions out of widget callbacks, and keep effects explicit.

---

## 25. Compact reference

```text
CORE
    Owns decisions, compact interaction state, policy, planners, generations.
    Receives semantic events and bounded observations.
    Emits semantic deltas, requests, and diagnostics.
    Has no Qt, I/O, threads, clocks, dialogs, or hidden globals.

HOST
    Owns effects, services, authority sessions, caches, jobs, persistence,
    cancellation, lifecycle, and operational error policy.
    Converts provider data into observations and completions into events.

QT ADAPTER
    Owns windows, widgets, actions, models, focus, layout, accessibility,
    drag/drop, dialogs, themes, and event translation.
    Applies projections incrementally.
    Does not own canonical business state or semantic policy.

PERFORMANCE
    Scale with active set + changed set.
    Use deterministic in-place transitions when appropriate.
    Preallocate hot paths, coalesce high-frequency input, retain caches in host,
    and use direct typed calls in-process.

ASYNC
    Every request carries identity and generation/revision context.
    Workers never touch Qt or core state.
    Completions return to one ordered application lane.
    Stale completions are ignored deterministically.

TESTING
    Core: unit, property, replay, fuzz, benchmark.
    Host: fake ports, manual executor, failure and lifecycle tests.
    Qt: event translation, model deltas, actions, focus, accessibility.
    End-to-end: a small set of complete workflows.
```

---

## 26. Final guidance

Implement the architecture as a set of constraints, not as a ceremonial framework.

The successful version is not the one with the most interfaces or the purest terminology. It is the one in which:

- important decisions are deterministic and directly testable;
- effects and authority are explicit;
- Qt remains an excellent presentation toolkit rather than becoming the domain model;
- large data stays in the services and caches designed to manage it;
- hot interactions remain direct, incremental, and allocation-conscious;
- asynchronous failures and stale results are reproducible rather than mysterious;
- a future alternate frontend can reuse application decisions without pretending every Qt widget is portable.

The concise design philosophy is:

> **Make decisions deterministic, effects explicit, authority singular, presentation adaptable, and work proportional to what actually changed.**
