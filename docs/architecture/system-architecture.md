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
- CMake 4.4.2 and Ninja
- Centralized package discovery for prebuilt Qt, libsecret, Python, and Git
- SQLite 3.53.4 through a verified CPM-acquired, directly linked C API
- Installed Git executable through `QProcess`
- Native HTTPS as the sole milestone-one production GitHub authentication
  transport
- Visible GitHub CLI profile choices fixed to typed `cli_unavailable` with
  zero CLI executable/configuration discovery or CLI process launch; the
  bounded `QProcess` broker remains inactive scaffolding until RFC-BPR-0015
- REST-first GitHub provider with narrow GraphQL use
- KSyntaxHighlighting 6.28.1 through its low-level highlighter interface,
  acquired as verified CPM source with extra-cmake-modules 6.28.0 as a pinned
  transitive build input; it requires Qt 6.9 or newer
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
- The BPR-0235 prelaunch public-identity check is one narrower macOS exception:
  a dedicated GitHub adapter owns an absolute, Apple-code-validated
  `/usr/bin/curl` child because Qt and its native HTTPS manager must not exist
  yet. Its production API accepts only cancellation, its target and wire
  profile are compiled constants, its environment is empty, and it has no
  authentication or mutation operation. Exact static ownership keeps this
  exception out of the application host, Qt adapter, and ordinary provider;
  authenticated GitHub traffic still uses native HTTPS only.
- The Qt adapter uses Qt idiomatically. It does not own canonical GitHub state, review policy, persistence, asynchronous orchestration, or stale-result policy.
- The composition root is the only place that knows all concrete pieces.
- In-process communication uses typed C++ calls and records. JSON/RPC is added only at a real process boundary such as an agent protocol.

Build dependency ownership is centralized before these product targets are
declared. Caller-installed Qt is required in CMake package mode and is never
fetched; Python, Git, and Linux libsecret use the standard CMake or
`pkg-config` providers only when the selected graph needs them. The centralized
provider verifies the resulting `Qt6::*`, `LibSecret::LibSecret`,
`Python3::Interpreter`, and `Git::Git` targets before product subdirectories
are evaluated and rejects any predeclared collision with those owned names.
Standard tool discovery uses CMake's built-in modules and rejects package
redirects for provider-owned names. The source tree configures only as the
top-level project, rejects top-level dependency-provider hooks, and loads its
repository and built-in modules by trusted absolute path; install/export
composition remains RFC-BPR-0014 work. CPM owns verified source acquisition
rather than prebuilt platform-package discovery.
`Threads::Threads` and native frameworks remain explicit toolchain/platform
capabilities. SQLite is download-only CPM source that becomes the first-party
static `BetterPullReview::SQLite3` target and is never replaced by a system
library. Only its verified C translation unit and public header are staged into
the target's include/source directory; unrelated cache siblings never enter
the compiler search path.

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
| Private durable review state | Host-owned SQLite ledger | receipts, acceptance, observer rules, local defect classification/lifecycle/anchor history, AI drafts |
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
one FIFO and at most one live cache session. One public `epoch_host_limits`
record caps the global retained job ledger (64 by default, at most 1,024) and
ordinary reentrant ingress (64 by default, at most 1,024). The same record
bounds how long a synchronous caller may wait on that FIFO: `BlobWait` (90
seconds by default, at most 10 minutes) for the two reads — blob
materialization and selected-diff resolution — and `WorkspaceWait` (10 minutes
by default, at most 60) for the two agent-workspace calls, which write or
remove a whole tree rather than read an object the cache already holds. Both
are validated non-zero at composition. They are backstops, not latency
targets: what releases an abandoned call *promptly* is the caller's stop
token, which every one of these waits honours, and the deadline is what
releases a call whose actor never reaches the work at all. Before either
existed the waits had neither, and only shutdown ended them. Every queued,
running, or decision-completion-pending actor operation—including synchronous
diff blob materialization—retains one ledger entry. The global job cap
therefore bounds the sum of actor FIFOs and each individual FIFO. Decision
completion tasks are allocated at admission, and retirement reserves the freed
slot for one core-emitted dependent request until that terminal returns;
concurrent blob calls cannot steal mandatory cleanup capacity. Core epoch
requests receive their typed `quota_exceeded` terminal when ordinary capacity
is full. The existing blob vocabulary has no host-capacity reason, so blob
admission returns generic host failure rather than misreporting a file-size
materialization limit. A replacement that has append preparation to retire
must pre-admit and successfully schedule that cleanup before mutating the
active authority or submitting the new offer. Insufficient capacity produces
an exact quota refusal; allocation or executor refusal produces unavailable
while preserving the active context. The admitted cleanup control is protected
from the replacement's ordinary authority-cancellation sweep. Switching pull
requests in the same repository therefore queues old-context cleanup under its
old PR session before the new offer can open the full new PR request. For
unrelated repository actors, the cleanup post and new-offer post must both
succeed before the host cancels or invalidates the active authority. Refusal of
the second post leaves the old authority active while the already-admitted
cleanup finishes safely.

Reentrant accept/dispatch ingress remains FIFO and is rejected before core
acceptance when its separate capacity is full. One pending diagnostic reports
an overflow turn. Matching queued offers are revoked by invalidation, while
two fixed priority slots retain the only simultaneously relevant active and
pending invalidations; shutdown uses a separate priority flag and supersedes
ordinary work. Neither terminal event can be crowded out. Projection delivery
is synchronous and observational: its exceptions are contained and cannot
veto core acceptance, durable success, cleanup, or actor progress. Semantic
ingress and state/quiescence inspection remain decision-lane confined; the
synchronous `materialize()` port is the sole concurrent exception. Blob calls
enter through a close-aware counter, share the job ledger, combine caller and
host cancellation, and remain part of quiescence until their callers wake. A
retained cache actor grants blob access only to the exact active application
session and pull-request authority. Replacement, matching invalidation, and
shutdown synchronously revoke that grant; materialization checks it before
admission and on the serialized actor both before and after cache access. A
cache that returns bytes despite advisory cancellation cannot publish them
after revocation. Blob result delivery moves its job into a non-consumable,
counted actor-exit reservation through waiter notification, so no running
closure falls outside the residency bound. Matching invalidation performs the
cancel-and-revoke portion when the request is admitted, before a reentrant
call returns; shutdown similarly closes blob ingress and cancels admitted
jobs before returning, even when semantic teardown must wait for the current
decision turn. A generation-checked authorization critical section makes
successful result publication linearizable with revocation. Cancellation
marks and copies matching controls into fixed maximum-capacity storage while
holding the job mutex, then releases that mutex before `request_stop`.
Synchronous stop callbacks may reenter shutdown, the public blob seam, or
counter inspection without deadlock or recursive cancellation depth.
Shutdown is quiescent only after the job ledger, entered blob calls, ordinary
and priority ingress, pending offers, decision turns, and every actor queue are
idle. The similar GitHub-host reentrant-ingress surface remains separately
scoped future work. Its smart-Git authority revocation path does participate
in this lifecycle: it swaps the retained weak transport set under lock without
allocating before it cancels transports.

The selected diff host has its own effect executor but obtains blob bytes only
through the epoch host. Its synchronous blob seam queues onto the matching
repository actor and requires the actor's already retained complete cache
request to equal the diff request authority. It never opens a competing cache
session and cannot switch a same-repository actor back to a stale pull request.
Installed Git checks blob type and declared size before retaining content, and
the cache adapter revalidates the owned repository boundary after the process
returns. The 16 MiB diff-computation materialization ceiling is therefore
preserved across the full composition.

Selected-file resolution uses the same retained actor before blob
materialization. A bounded workbench observation keeps an opaque semantic file
identity, presentation path, provider-free current/previous repository paths,
and change kind. The core still receives only cursor and order facts. On
selection, the workbench host obtains the epoch host's exact accepted
authority, anchors, comparison revision, normalized options/digest, and
algorithm, then queues `commit + path -> optional blob OID` lookups on the
already-open cache session. Normal, add, remove, and copy facts query the
current path independently at both selected endpoints. Rename facts query the
current and previous candidates per endpoint. An unbound or ambiguous endpoint
may use one bounded `diff-tree` rename record for the exact selected commit
pair, but only when one record matches accepted lineage and every endpoint
already resolved by the cheap lookup. The fixed raw scan bound is larger than
the fixed retained-candidate bound, and unrelated records are filtered before
the latter applies. Reverse pairs use reversed commits; multiple, malformed,
and unbound candidate records fail closed without history traversal.
The actor revalidates that complete selected authority before and after Git
work; one absent endpoint is legal, while two absent endpoints and non-blob
tree entries fail closed. Qt retains the opaque file identity in its list model
and emits a semantic selection without parsing row text or running Git on the
presentation lane.

Composition forwards every accepted epoch projection unchanged to Qt, then
replays the retained workbench selection only after an available comparison is
accepted. A new workbench authority/generation retires selected work, clears
the host rows/selection/core cursor, and publishes that clear before starting
replacement observation. Thus unavailable or malformed replacement data cannot
leave prior-authority rows selectable.

Composition shutdown closes diff ingress first and epoch blob ingress next.
The Qt projection remains alive until the diff host, epoch host, GitHub host,
and cache actors are quiescent; the diff effect lane drains before the epoch
effect lane. A changed accepted semantic revision, epoch/file/options
selection, authority invalidation, and whole-cache deletion revoke matching
diff state and Qt geometry before stale work can publish. A metadata-only
accepted refresh with unchanged authority and anchor-driving revision facts
retains the exact diff and semantic selection.

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
- Custom diff widgets paint visible rows only and retain layout caches; comments remain in the conversation drawer. A conversation selection may reuse the semantic line transition and paint a narrow margin marker only after the application proves the selected comparison contains that exact endpoint revision. Source-line gestures reverse-filter the retained thread forest by exact file, side, and line; reply selection resolves through its root. An interactive non-line gesture clears the selected conversation with its marker rather than leaving a selected root that no longer owns the diff selection.
- Dialogs, notifications, native menus, focus, accessibility, themes, layout, and high-DPI behavior belong to the Qt adapter.
- Reusable widgets do not invoke application persistence, global service registries, or unrelated windows.

The RFC-BPR-0003 secondary local-history menu presents core-projected prune,
whole-cache-delete, and maintenance-cancel commands. Qt captures exact stable
epoch IDs from the accepted pair, or the exact accepted catalog/cache scope,
before opening a destructive confirmation whose default and Escape choice are
Cancel. Confirmation does not relax authority: the core and host revalidate
the captured identity and revision before the existing typed maintenance path
can act. Qt never derives a maintenance target from visible page residency or
performs the storage operation itself.

The core disables prune unless at least one anchor of the accepted pair is an
epoch anchor whose availability can enter a prune operation. `core::prunable`
is the single definition of that eligibility, and the catalog port's
`valid_prune_anchor_availability` defers to it so the projection and the
durable authority cannot drift apart. An already pruned, cache-deleted, or
never-observed selection therefore never reaches a destructive confirmation.
This is a presentation pre-check rather than a new authorization boundary: the
core transition and the host catalog remain the fail-closed decision point, and
an intent injected directly still travels to them.

### RFC-BPR-0005 workbench allocation

The workbench shell follows the same three-region split as every other feature.

The core owns file and change navigation order, selection identity, drawer
semantic state, and the command projections with their disabled reasons. It
stays compact: it holds the selected file identity, a bounded navigation cursor
carrying the ordinal, actionable count, and neighbour facts, and the two drawer
states. It never holds the file list, a path, or file content. A selection that
has left the filtered actionable order has no ordinal to step from, so the core
reports that only the host can name its neighbour rather than inventing one.

The host owns the ordered actionable set, assembles the bounded cursor facts
the core decides from, publishes only the projection domains whose meaning
changed, and persists drawer widths. Its observation port reports a limit
rather than truncating, because a truncated order would hand the core false
neighbour facts. Staleness is scoped per domain rather than by a global
generation, so a fresh observation cannot silently invalidate an in-flight
width persist whose payload was still correct. Cancellation and supersession
stamp an outstanding record rather than erasing it, because a cancelled request
is still owed its terminal.

The Qt adapter composes rather than rebuilds. The centre hosts RFC-BPR-0003's
epoch control and RFC-BPR-0004's comparison surface unchanged, so the workbench
adds no second epoch authority, no second mapping, no presentation-side
regrouping, no follower-scroll coupling, and no synthetic alignment rows. The
source panes are `QPlainTextEdit`, which virtualizes its own text layout, so
widget count is bounded by visible UI structure rather than source length; a
sixty-thousand-line comparison creates no widget that a sixty-line one does
not.

The composition root binds the workbench to one review-ledger host per accepted
pull-request authority. A real selected-diff publication supplies its exact
file/material fact before the four receipt actions are enabled. Accepted
receipt projections update the compact action surface and re-observe the
workbench when actionability changes; files without an exact material fact stay
conservatively actionable. This actionability-only refresh retains the semantic
selection and rendered comparison even when the selected file leaves the
filtered order; changed material reopens the receipt and returns the file to
that order. A failed ledger open disables only private-progress actions and
leaves GitHub, epoch, and diff authorities live.

### RFC-BPR-0006 appearance allocation

The core owns appearance-mode resolution, typography legality, the closed
semantic role vocabulary, theme exchange, and contrast measurement. It holds no
font, colour, Qt, or KSyntaxHighlighting type; a colour is four named sRGB
channels, and typography is integer fixed point so every legal value is exactly
representable and comparable.

The host owns settings persistence and the token-span cache. That cache is
keyed by blob identity and definition version with the theme absent from the
key, which makes "switching theme does not re-run lexical parsing" structural
rather than behavioural. A persisted record the core rejects is refused whole
rather than clamped, because a clamped value would tell a settings surface its
rejected input was accepted.

The syntax adapter implements the host's classification port through
KSyntaxHighlighting's low-level interface. It classifies a complete blob in one
lexical pass and projects a hunk from that pass; highlighting a range on its own
would lose the state that opened a multiline construct earlier. It produces core
roles, never colours, and consults no standard path.

The Qt adapter turns a core-resolved variant plus a palette into applied
presentation without installing a forced style or QSS, preserving the contract
that the application inherits the native style and interface font.

The Qt-free `core-only` preset acquires neither KSyntaxHighlighting nor the
syntax adapter; its resolved package set remains SQLite alone.

## Dif asset ownership boundary

RFC-BPR-0019 places Dif's editable source frames, manifest, provenance, and
committed generated products under `src/adapters/qt/assets/dif/`. The offline
`scripts/dif_assets.py` tool validates and regenerates that tree. It is a
maintainer tool, not a product dependency: ordinary developer, core-only, and
tests-disabled application graphs do not select or execute it.

The generated catalog is a fixed C++ descriptor with Qt resource names, but
RFC-BPR-0019 does not bind those resources to a target. Core and application
state contain no mascot, image, clip, frame, timing, visibility, or motion
preference. The application binary likewise contains no Dif asset until the
downstream composition RFC selects it. This keeps creative source ownership
inside the Qt adapter tree without turning asset policy into core policy or
runtime JSON parsing.

RFC-BPR-0020 may consume only an Auto-Verified asset revision. It owns the Qt
resource binding, one decoded cache, sequencer, widget, lifecycle, motion
preference integration, and real landing composition. The accepted asset note
also requires the plus/minus prop to remain separable from Dif's core base
representation. If satisfying that note changes any current raster, the asset
revision returns through RFC-BPR-0019's deterministic and attended gates before
the application selects it.

## Feature allocation

| Feature family | Functional core | Application host | Qt presentation adapter |
|---|---|---|---|
| GitHub/GHES | host/account identity policy, command legality, completion acceptance | provider capability port, session/staging/cache lifecycle; external GitHub adapter implements authentication and transports | account/host selection, errors, capability presentation |
| Device authorization | stage, command legality, request/generation identity, stale rejection, terminal reason | device-code request, bounded polling, token exchange, refresh before expiry, handoff authority check, refresh-only vault slot | user code, address, validity, handoff outcome, buttons enabled from the core's command projection |
| Git/epochs | epoch-selection rules, comparison intent, stale acceptance | Git process, bare cache, refs, clocks, persistence | epoch selectors, labels, unavailable states |
| Diff/navigation | exact comparison identity, byte-faithful text decoding, deterministic alignment/pairing/intraline policy, stable mapping/group/move identities, active change, navigation policy | blob residency, immutable worker inputs, jobs, exact diff caches, cancellation, stale-result rejection | natural-layout panes, measured max-extent axis, retained comparison position, shared scrollbar, `DiffLinkMap`, lower detail, focus |
| Review ledger | receipt/actionability transitions | SQLite authority, queries, migrations | badges, filters, commands, explanations |
| GitHub mutations | semantic proposal, target/head validation policy | mutation execution, idempotency, reconciliation | draft editor, confirmation, progress, diagnostics |
| Queue/observers | rule matching and attention policy | refresh timers, API/cache, notification request execution | queue models, filters, native notification presentation |
| AI | provider-host policy, context-scope decisions, staleness, finding promotion | process/session/workspace management and protocols | explicit invocation, approvals, chat/findings presentation |
| Appearance | no color/font/widget authority | settings persistence and cache scheduling | system mode, fonts, colors, syntax presentation, accessibility |
| Dif mascot | no asset, frame, timer, or mascot state | no asset generation, decode, or animation state | RFC-BPR-0019 owns reviewed asset bytes; RFC-BPR-0020 later owns fixed resources, sequencing, painting, and lifecycle |

The shipped agent route preserves this allocation. Qt exposes provider,
disclosure, working-copy choice, start/cancel, stream, and terminal state, but
owns no process. The application host reruns core policy before effects, adapts
only accepted authority/private observations, and serializes lifecycle and
cleanup. The retained cache actor materializes an owner-private exact-head
workspace outside its cache; the agent adapter receives that directory only
after materialization. App composition is the only region that knows the real
panel, host, cache, launcher, and built-in executable profiles together.

The shipped review-mutation composition preserves this allocation. Qt emits the
diff workbench's exact semantic selection and draft commands, then presents
typed outcomes. It owns no provider. The core decides whether the draft may
publish. The application mutation host binds the selected comparison, file,
workbench generation, selected-diff identity, side, line, resolved repository
path, epoch-issued endpoint provenance, commit, designated head, and acting
identity before queuing any effect. Only the exact right/current-head endpoint
is supported; left, absent, stale, or mismatched paths fail before provider
work. One host-owned drain serializes coordinator/provider effects independently
of executor concurrency. The coordinator then fixes the destination,
revalidates target and head, derives the idempotency key, and refreshes server
comments before presentation.

The live provider captures a bounded, sorted set of exact matching GitHub
comment node IDs before durable admission. The exact-scope SQLite store commits
that baseline and the canonical write fingerprint atomically, then viewer and
target/head observations remain the final authority sequence immediately before
POST. Admission is only a prepared row: the native transport advances it to
possibly-transmitted through a one-shot durable callback after the Qt network
manager returns a POST reply. A failed witness aborts the reply; pre-dispatch
failure and death before that callback remain prepared and perform no recovery
transport. Native proxy fallback cannot create a second mutation POST after
the first candidate creates its reply, even when its terminal error would be
retryable for a GET. A possibly-transmitted originating or restarted process
performs only GET reconciliation: current exact IDs minus the durable baseline
must contain exactly one identity to resolve. Zero or multiple candidates,
pagination, capacity overflow, digest-invalid baseline bytes, and migrated
unresolved v1 rows with no baseline fail closed without a second POST. The
encoded baseline and SHA-256 digest commit together. An exact successful POST
response is its own direct receipt only when its ID was absent from the durable
baseline.

The fixture build supplies a reusable testing grant. Tests-disabled production
constructs the same asynchronous host only when the accepted handoff and
snapshot match the compiled designated host, repository, pull request, base,
and head tuple. The acting database ID, node ID, and login are not compiled
constants: they come from the exact authenticated session, are cross-bound to
its opaque accepted-snapshot transport lease, and are re-observed before a
write. An ordinary production launch supplies no grant. The sole production
path has an outer stage-zero image owner around two authority stages. Stage
zero captures the producer-bound launcher bytes under the fixed system Python,
attaches the exact literal-digest image read-only below an owner-private mount
root, and retains the image, mount, inventory, verifier, supervisor,
application, and nested runtime identities through child exit. The mounted
signed-capsule supervisor verifies the complete dependency/resource inventory
and performs one unauthenticated, redirect-free public target observation
before it creates the GUI process. A private in-application bootstrap then
validates and atomically consumes one owner-private approval bound to the
actual shipped executable, source digest, Release/tests-disabled build
identity, exact designated tuple, and random one-use approval ID. Credentials
remain in the later Qt sensitive prompt and never enter that approval, stage
zero, or the supervisor.

Stage zero owns stage one as a bounded process group. It revalidates the image
and mounted runtime after every normal, failed, timed-out, or handled-signal
exit, reaps the complete group, and detaches without force. This is a read-only
mounted-filesystem guarantee, not an immutable-backing-file guarantee: the
owner can still write the DMG while it is attached, and post-child digest
checks detect persistent drift only after execution. `hdiutil attach` remains
pathname-based, and the generated CTest manifest/cache remains the trusted
same-UID execution plan. Each reviewed packet therefore binds its exact stored
DMG; separate image builds are not expected to be byte-identical.

The production carrier is private and one-use. It first binds the application
session, workbench generation, exact target, and authenticated actor. The host
then validates comparison, file, right-side semantic anchor, resolved path,
and current head before atomically consuming the carrier immediately before
effect enqueue. Invalid requests do not consume or supersede; the first exact
request consumes, every later request refuses, and shared cancellation,
invalidation, expiry, and shutdown revoke unused authority. No boolean,
environment variable, credential, raw nonce, persisted value, IPC message, or
ordinary Qt gesture can mint the capability.

Each accepted fixture request has its own monotonic stop source. Supersession,
explicit cancellation, authority replacement, and shutdown request stop, and
the token propagates through coordinator observation, the provider session, and
native HTTPS. Terminal delivery is serialized, exactly once for the current
request, and withheld after supersession or shutdown. The host remains alive
until its owned effects and completions are quiescent, then the composition root
releases provider and lease authority. A UI selection made while a request is
pending updates one latest-selection slot rather than replacing the publishing
draft. Returning from A to B to A before the terminal therefore adopts A. The
matching terminal always retires pending state and reconciles server truth
before that latest selection is presented. An unavailable terminal may retain
one exact authority-bound retry identity; different anchors or authorities
cannot restore it, and successful retry, invalidation, composition retirement,
and shutdown clear it.

The epoch host owns the only production mint seam for sealed selected-diff
endpoint provenance. The fixture issuer exists only under testing macros.
Likewise, the testing-grant friendship is absent from production-flags
translation units, so an external definition cannot acquire constructor
access. A full LF-normalized digest locks the composition translation unit.
A second deterministic manifest hashes every production application source and
header as its sorted relative path plus LF-normalized content digest. The exact
file inventory and the composition TU's semantic inventories remain separate,
so whitespace, comment, token-fragment, or second-TU changes cannot bypass the
binding.

For RFC-BPR-0004, interaction state in the core includes the active semantic
change and selected source line, boundary, or mapping target. Host caches may
retain immutable file-diff values and indexes only under exact blob, option,
comparison, and algorithm identity. Qt-local state includes measured line
boundaries, the measured alignment axis, the retained subpixel comparison
position, shared-scrollbar projection, hover paths, and splitter geometry.
Measured pixels and the live scroll position are not core or durable state.
The center map is the sole wide `QSplitterHandle`: it retains ordered changed
interval geometry, derives cached visible paths from both pane origins, rejects
frames for a different comparison digest, and resolves pointer hits back to
stable segment/group identity. Frame, measurement, and size changes recompute
stationary-pointer hover and publish any resulting clear or replacement. Move
markers and temporary overlays attach to the exact source-left and destination-
right interval midpoints, not to a whole-ribbon bounding box. Fills, outlines,
focus, and move markers derive from the active Qt palette and style; no per-
ribbon widget, forced palette, QSS, or permanent crossing move path is part of
the design.

The live core `diff_semantics` boundary consumes only a validated
`computed_file_diff`. A retained `diff_semantic_index` binds sorted stable-ID,
line, move, and intraline lookup records to that immutable value after one
full validation. Exact/similar line correspondence, opposing spans and
boundaries, canonical group projection/order, previous/next stop-or-wrap
navigation, semantic selection, detail input, and optional move annotation all
resolve from the same alignment IDs. Navigation does not accept a mutable
presentation projection, and rejected caller-supplied mappings or selections
cannot carry fabricated identities or diagnostics forward.

The default linked view prohibits copying or normalizing independent pane
scrollbar values, active/follower chase heuristics, synthetic alignment rows,
separate mappings for ribbons/detail/navigation/scroll, full semantic
recomputation during wheel/resize/paint, and permanent crossing move ribbons.
High-frequency input rebuilds neither semantic diff nor unchanged measured
geometry and does not rescan pane text. The center map binary-searches its two
ordered changed-entry ranges and merges them without a per-frame sort, so
visible ribbon selection and painting scale with visible or near-visible
changed segments. Capacity-growth and visited-record counters make that hot
path directly falsifiable in the composed Qt fixture.

Focused review is a Qt-local presentation choice, not durable review state.
The shipped Files and Conversations actions retain their host-decided drawer
semantics. Source-pane line-number visibility is deliberately *not* in that
Qt-local class: it is a durable reader preference, so it is a field of
`core::workbench_interaction_state`, an intent on the workbench variant, a
column of the SQLite `workbench_layout` table, and a published delta the View
menu's action reads its checked state from. A Qt-local boolean on the pane
would have satisfied the same user-visible request while violating both the
adapter boundary and the one-command-projection rule. A separate always-visible, keyboard-addressable Review details
action hides or restores the secondary session/queue/pull-request header,
epoch/history controls, and selected-change detail together. Hiding them does
not clear their values or semantic selection; it returns their vertical extent
to the two source panes and center map. At the 1280 × 1000 evidence viewport,
the selected-diff contract requires the native comparison splitter to occupy
at least 75% of the client width and 85% of its height while every hidden
surface remains reopenable through the shipped toolbar. That height floor
reflects the product-owner requirement that all secondary upper and lower
detail chrome collapse together.

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
time. BPR-0066 makes page direction explicit: core authorizes either an exact
revision-bound `older` continuation or a cursor-free `newest` restart. Core
state retains only the accepted catalog, navigation kind, and continuation;
it never retains page rows or complete history.

BPR-0046 consumes that projection in one provider/Git/filesystem-free Qt
control embedded in the interim loaded pull-request window. Its shared stable
model retains exactly one accepted page, capped at 100 epochs and 200 anchor
rows, and reconciles page replacement by stable-ID insert/remove/move/data
signals without a routine reset. `Load earlier epochs` dispatches only the
accepted continuation; `Return to newest epochs` dispatches a cursor-free
semantic restart. A catalog revision retains the old page visibly but marks it
read-only and stale until that newest request succeeds. The exact selected pair
and complete SHA-1/SHA-256 OIDs remain in the independent comparison projection
when either anchor leaves the resident page; Qt does not fabricate pinned rows.
The control owns focus, accessibility, announcements, and live
palette/font/style invalidation; it does not resolve aliases, compare OIDs,
inspect availability, choose a paging cursor, or perform an effect.

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
BPR-0047 selects that complete path in the public application. The composition
root resolves the owner-private application-data directory and installed Git,
constructs the authority-partitioned cache and SQLite catalog, supplies native
entropy and UTC ports, and wires the dedicated epoch host between the accepted
GitHub handoff and the existing Qt comparison control. Provider, vault, and
epoch effects remain on separate owned workers; the Qt event loop remains the
single decision lane. Orderly shutdown revokes the smart authority, drains
both hosts to quiescence, then closes and joins epoch, provider, and vault
workers without letting a worker touch Qt.

When base, head, and branch are unchanged but the accepted
newest-current-viewer review fact changes, the core issues the
`associate review` variant instead. It performs one expected-revision
SQLite-only catalog mutation, advances the revision only for a changed
association, and creates no epoch, Git object import, or ref.

## RFC-BPR-0016 device authorization allocation

Device authorization is a second credential-acquisition path over
RFC-BPR-0002's native HTTPS transport. It does not replace the manual token
prompt, which stays first-class on every profile, and it relaxes no transport
rule. Like the epoch slice above it is a dedicated core policy and application
host rather than growth in the GitHub reducer, and no Qt projection is used as
an internal bus.

```text
begin/cancel intent from Qt
    -> device authorization core (stage, legality, request identity)
    -> typed host request
    -> device authorization application host (code, poll, exchange, renew)
    -> GitHub device-authorization contract adapter (form body, parser)
    -> native HTTPS transport (the two /login routes)
    -> one typed observation, accepted by the core
    -> revisioned presentation published outward
    -> authorized token -> the existing enrollment -> the platform vault
```

**The core decides.** `core::device_authorization_state` carries the stage,
the pending request context, the generation, and the terminal reason;
`core::transition` returns the changed domains, the host requests, the status
delta, the diagnostics, and which of `begin_device_authorization` and
`cancel_device_authorization` is legal — and, when one is not, the reason code
that says why — as `device_authorization_command_projection` values. The core
names no client identifier, endpoint, device code, token, or poll interval
([[BPR-0277]], `core_device_authorization`).

**The host performs.** `application::device_authorization_application_host`
owns the device-code request, the poll schedule under the server's interval
and its requested `slow_down` backoff, the total wall-clock bound and the
maximum poll count, cancellation from the UI and at shutdown, renewal before
expiry, and the erasure that follows every terminal including the successful
one. Active polling and renewal deadlines use a monotonic clock, because a wall
clock step must not extend polling or shorten a live interval. BPR-0541 adds
wall-clock issuance and expiry facts only to bridge application lifetimes;
backward or expired facts refuse restart. A lazy SQLite adapter persists exact
device-binding metadata through a serialized effect lane, and the GitHub host
revalidates the vault-backed account before composition arms deferred renewal.
No token enters SQLite ([[BPR-0279]], [[BPR-0541]],
`application_device_authorization_host`,
`application_device_authorization_refresh`).

Two narrowings keep that host from reaching further than the flow needs.
It takes `github_credential_refresh_vault_port` — `store_refresh`,
`retrieve_refresh`, `delete_refresh`, `cancel` — rather than the whole vault,
so naming the base-token slot from the device host is a compile error rather
than a rule somebody has to keep; the full `github_credential_vault_port`
derives from it, so composition still hands one platform object to both. And
authorized material passes through once: the access token becomes a vault
entry through the existing enrollment boundary and the host keeps no copy,
while refresh material is read back from the vault when it is needed
([[BPR-0283]], `application_device_authorization_refresh`,
`application_github_credential_vault`).

**The GitHub adapter speaks the wire.** It builds the exact form bodies,
parses each recorded response into one typed completion under a bounded
response limit, and refuses a `verification_uri` that does not sit under the
expected authority before the address can reach a host at all. That authority
is declared once, as
`application::default_device_verification_authority`, and checked twice —
at the wire by the parser and again in the host against the authority
composition handed it — so a registration that moved the device endpoints
cannot leave half the product trusting the old one ([[BPR-0278]],
[[BPR-0283]], `github_device_authorization_contract`,
`github_device_authorization_refresh_contract`).

**Native HTTPS carries it unchanged.** Both endpoints are the same transport
under the same rules: system trust and proxy configuration, no
certificate-error bypass, unknown-authority redirects rejected before any
credential or body is forwarded, and every query-bearing redirect failing
closed. URL construction admits the `github_com_2022_11_28` server contract,
no query, and exactly the two `/login` paths; anything else yields no request
at all ([[BPR-0278]], `native_https_device_authorization_contract`,
`native_https_device_authorization_fixture`).

**Qt presents, and decides nothing.** `qt::device_authorization_panel` renders
the user code, the verification address, the remaining validity, and the fate
of the last browser handoff, and enables its begin and cancel buttons by
looking each command up in the core's projection. An absent command is a
disabled command, so a projection that lost one fails closed rather than
falling back to a rule of the panel's own. Copy and open stay derived from
what is on screen, because neither moves the flow ([[BPR-0280]],
[[BPR-0283]], `qt_device_authorization_scale_100`,
`qt_device_authorization_scale_125`, `qt_device_authorization_scale_200`).

**The browser capability is composition's, not Qt's.**
`VerifyArchitectureBoundaries.cmake` refuses `QUrl`, `QDesktopServices`, and a
literal scheme anywhere under `src/adapters/qt/`, so the presentation adapter
cannot hold an address and cannot acquire an opener. `bpr::app::system_browser_opener`
is the one place in the shipped graph that hands an address to the platform's
own launch service: no embedded web view, no browser engine, no network of its
own, no listener, and no supervised child. A shipped graph binds it; a
tests-enabled graph binds a recording opener instead, so no suite in this
repository can start a browser. Composing none at all is the construction cut,
and only then does the action answer `unavailable` ([[BPR-0516]],
`application_device_authorization_browser_handoff`,
`application_device_authorization_browser_absent`, `composed_application_hosts`).

**Composition selects the identity and the platform.** The device path is
composed on every platform, registered or not, because the visible choice and
its typed answer are the product behaviour: selecting device sign-in on an
unregistered platform must say so rather than do nothing. A registered
platform composes the credential-free wire bound to the recorded distributable
client identifier; an unregistered one composes a refusing transport whose
reached-count proves the capability gate held. A composition failure falls
closed to the refusal ([[BPR-0281]], [[BPR-0318]],
`application_device_authorization_public_path`,
`application_device_authorization_release_contract`).

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

## Anonymous public-preview allocation

RFC-BPR-0017 uses the existing provider, pager, normalizer, epoch host, and diff
host. Core owns explicit access mode, operation legality, unavailable facts,
rate refusal, and identity generations. The application host selects a deliberate
anonymous authority, owns effects and invalidation, and publishes the narrow
`public_preview_projection.hpp` record. Qt receives that record and semantic
gestures; it receives no credential, provider, or transport.

The GitHub adapter supplies separate credential-free native REST and smart-HTTP
factories with the existing TLS/proxy/redirect bounds. An invalid authenticated
credential never selects them as a fallback. Composition enables this authority
only on macOS. Cache and catalog keys include access mode, so anonymous objects
and epochs cannot borrow an authenticated account's namespace.
