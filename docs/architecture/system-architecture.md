# BetterPullReview System Architecture

Review Cycles ([RFC-BPR-0053 through RFC-BPR-0061](../rfcs/RFC-BPR-0053-review-cycles-domain-and-local-authority.md))
is built into the desktop and `bpr cycle` under the same allocation as a pull
request; the [product contract](product-contract.md) states its narrow scope and
the [execution ledger](../verification/RFC-BPR-0053-0061.md) what is qualified
and owed. Those RFCs stay Proposed until their closeout cards judge each
criterion. See [Review Cycles](#review-cycles) below.

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

RFC-BPR-0023's private review follows the same split: core proposals bind
immutable thread/revision/run/composition identities; application hosts own
SQLite transactions, exact source admission, serialized effects, and publication
recovery; Qt owns the Private history panel and incremental projection. The
[state model](domain-and-state-model.md), [private ledger](review-ledger.md),
[round trip](review-round-trip.md), and [agent boundary](ai-agent-boundary.md)
define the ports and limits. The [operator workflow](../ux/private-review-workflow.md)
describes the separate local draft and public Publish actions.

The Qt presentation adapter is part of the imperative shell. It is not an architectural peer that bypasses the host or the core.

## Technology baseline

- C++23
- Qt 6 Widgets, Core, Gui, Network, and required platform integration
- CMake 4.4.2 and Ninja
- Centralized package discovery for prebuilt Qt, libsecret, libxcb, Python, and Git
- SQLite 3.53.4 through a verified CPM-acquired, directly linked C API
- Installed Git executable through `QProcess`
- Native HTTPS as the default production GitHub authentication transport
- Explicit macOS CLI read opt-in under RFC-BPR-0015 (Auto-Verified 2026-09-23);
  unsupported platforms retain typed `cli_unavailable`
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

## Profile and window ownership

Startup acquires a native exclusive lease for the exact validated private profile
root before queue storage or runtime services are constructed. The desktop then
lets the management host commit its first core window context. Open Profile also
acquires admission before changing durable window generations. Contention returns
`owner_busy` and preserves the other owner's windows; retries during failed-start
cleanup cannot bypass admission. The lease outlives profile services and effect
executors. POSIX uses a non-inherited directory-inode lock; Windows retains private
validated directory handles and an exclusive non-inherited lock-file handle.
Native process exit releases ownership without a PID-age or timeout override.
Distinct profile roots may own independent runtimes. On supported POSIX roots,
the desktop binds an owner-private local socket while retaining the native lease.
A competing startup sends the stable profile identity and exactly one activate
or new-window intent. Effect-lane polling authenticates the peer user and bounded
versioned frame; the decision lane routes it through existing management policy.
Acknowledgement follows the actual activation or successfully opened window.
Busy/closing owners refuse, and uncertain delivery never causes an automatic
retry or competing runtime. The sender changes no window registry state.
On Windows the desktop binds the first instance of an owner-private message-mode
named pipe under the same lease ([[BPR-0979]]): the name is digested from the
canonical root, the descriptor names only this user (and, contained, the
package), the peer process's token user is checked on both ends, one 16-byte
message is the whole request and the acknowledgement is the same two bytes.
A second instance keeps the name bound while a delivered request is pending, so
the name is never free while an owner lives, and nothing is left behind when it
exits. POSIX socket paths beyond the native address bound remain unavailable;
those roots retain exclusive ownership and explicit contention. The Windows
adapter contract and public activation course pass on the Windows station
([[BPR-0979]]). Windows lets the process a person launched take the foreground,
not the owner it hands off to, so before a request the sender passes that right
to the authenticated owner process, and only that process, with
`AllowSetForegroundWindow` ([[BPR-0980]]). Without it, a repeated launch on the
native desktop reached the owner's most recently used window but left it behind
the launching shell.

The profile runtime owns private storage roots,
authentication, provider capabilities, queue refresh, preferences, logging,
epoch cache actors, shared drafts and workspace recovery. A window session owns
its read host, epoch and diff state, selected review, navigation, Qt surface,
mutation jobs and agent session. Closing a window drains its consumers; quitting
drains all windows before profile services. The first window has no special
ownership of shared services or projection delivery.

Window read requests retain their exact application session. Authenticated
provider requests additionally carry the accepted profile authentication session
as separate provenance, checked against the opaque account transport lease.
This lets sibling windows share a sign-in without changing request or completion
identity. Replacing the accepted account retires the old session before existing
windows can adopt the new authority. Acceptance is presented when it happens, so
a window opened later presents the account session its read host adopted as soon
as it is admitted, before any read and without one ([[BPR-1017]]). A
public-preview session keeps the profile host's own presentation.

Settings has one presentation per profile and mounts the invoking window's
controls over shared accepted preferences. Window geometry remains independent.
Its ten task pages aggregate the existing typed domain projections and
intents; there is no shared key/value settings authority. The appearance,
workbench, queue, authentication, agent and history hosts keep their own
validation, generations, persistence and effect lanes. Closing Settings cancels
uncommitted appearance preview/import work while accepted durable operations
finish through their host. The application-wide RFC-BPR-0035 updater is a
separate, explicit service mounted in General. The [Settings authority
inventory](../development/settings-authority-inventory.md) records each store,
reset and failure scope; the [Settings guide](../development/settings.md)
describes the corresponding user behavior.

The workbench profile also owns center ribbon width ([[BPR-1512]]): 16–256
logical pixels, default 28. Schema 20 adds it to the profile row while carrying
all earlier profile and window choices forward. Accepted durable behaviour
publication updates Qt’s actual splitter handle and map size hint together;
Qt retains ribbon geometry, hit testing and splitter mechanics.

The global profile registry retains bounded stable window identities and focus
order. The management host saves core lifecycle decisions before starting or
closing windows, and before orderly Quit drains services. Failed saves preserve
the usable windows and offer explicit registry reload. Restart restores retained
identities with fresh generations in focus order; explicit Close removes only
that window, while Quit retains survivors. Restored windows start on Home while
optional restoration loads. Each desktop window restores its own
workbench drawer states and widths and line-number visibility from the
profile-private window-keyed layout store. RFC0063 adds an optional summary fraction to that row (schema 15), validated and coalesced by the same host, with startup choices merged after load and drained on orderly Quit. Older rows default to content-aware sizing; profile defaults and new windows carry no summary preference. RFC-BPR-0085 adds the Overview's Status drawer width and collapsed state to the row on the same terms (schema 23, [[BPR-1588]]); it is the Overview's own column, not one of the core's drawers, which the Overview keeps collapsed. No PR text or reading state is persisted as layout. Fresh windows use profile defaults.
The same row holds bounded logical native position, normal size and maximized
state, never profile defaults. Qt applies it once after load, fits obsolete
display coordinates to an available screen, and reports later move/resize
observations to the host's coalesced persistence lane. Schema-nine migration
preserves older Home/drawer rows without inventing a native placement.
Orderly Quit drains the latest accepted layout write; explicit Close erases its
row after writers drain. The layout host closes ingress, retains at most one
active store plus one latest coalesced layout per window, and waits for their
decision terminals without retrying a refused store. Storage uses the existing
two-second SQLite busy timeout; there is no additional layout shutdown timer
or guarantee against an operating-system I/O stall. The port reports the last
finished store's refusal separately from its in-flight count. After host
quiescence, cleanup names only a real `cancelled` or `unavailable` result;
BPR-0875's attempted/successful-store counters retain their diagnostic meaning.
A later successful store clears the prior refusal (BPR-0899). Startup reconciliation removes orphan layout rows
using the complete accepted live set without changing profile defaults or the
global allocator. Optional layout failure is visible and leaves windows usable.
BPR-0709 adds accepted PR destinations in the separate profile-private
`state/v1/window-restoration.sqlite3`. Each of at most 32 records contains only
stable window identity, accepted read mode, configured canonical hostname,
repository owner/name and PR number. BPR-0710's version-two record optionally
adds the accepted exact comparison anchors and commit OIDs, stable file identity
and semantic selection. Version-one destinations migrate transactionally without
inventing a reading position. The core selects one shared startup
authority in retained focus order; incompatible destinations fail visibly.
The host loads and coalesces revision-aware writes on the effect lane, flushes
the latest accepted destinations on Quit and erases explicit Close rows after
writers drain. Failed attempted navigation never replaces an accepted record.
Restoration uses the exact window read host after the accepted session matches;
authenticated startup may reuse only a remembered grant, while public preview
uses the credential-free path. Home and new navigation cancel pending restore
requests by exact window/request identity. Broken optional storage leaves usable
Home and an explicit error without resetting the global window allocator.
The core advances reading restoration only after the retained exact pair, file
and semantic material resolve through the existing epoch and diff hosts. Missing
anchors, files or material leave an explicit unavailable result, not an alias or
nearby line. Accepted positions coalesce independently per window; failed
restoration preserves the saved record. Interactive reading gestures cancel the
pending continuation, and a superseded comparison request is retired by exact
request and generation. Qt restores the verified semantic selection and reveals
its line without reporting a new user gesture.
Actual-process destination acceptance is recorded by BPR-0709; BPR-0710 records
the bounded exact-reading course and its fixture limitations. Native window
geometry and target-native packaged activation remain open RFC-BPR-0024 work.

BPR-0969's version-five restoration store adds the accepted numeric account ID
to every saved destination and Recent entry. The core admits authenticated
metadata and saved reading only for that exact account, host and access mode;
new authenticated reads cannot record unknown ownership. The composition
checks the current core identity during sign-out as well as the accepted session,
filters Recent before Qt receives it, and rechecks Open and Resume admission.
Versions one through four migrate transactionally with account zero (unknown).
Their authenticated titles, destinations and saved reading remain private in the
store but are hidden until a newly accepted read creates an account-bound entry;
the current login never claims the old rows. Anonymous rows keep their anonymous
identity. Comparison anchors, source/cache scopes, positions and document-tab
payloads are unchanged. The ten-entry bound remains shared by the profile.
The [bounded isolation evidence](../verification/BPR-0969-recent-identity-2026-09-25.md)
does not establish durable PR snapshots or offline account authority.

The subsequent BPR-0969 overview store retains at most ten accepted historical
PR overviews, partitioned by complete profile-revision/host authority, exact
account and destination. The application host performs bounded SQLite effects
on its serial worker and the core admits a cached observation only after a
current authenticated read fails for transport loss. The same atomic historical
record optionally retains a bounded changed-file catalogue with current/previous
paths and provider change kinds/counts at those exact accepted base/head IDs.
Qt labels freshness unknown and preserves unavailable versus observed-empty
collections. The same atomic observation retains bounded historical thread roots
and replies, general comments and review summaries. Plain-text readers expose
prior review state and inert anchors without granting current conversation,
epoch, receipt or mutation authority.

Cached file activation uses a separate historical source port after the core
rechecks the current authenticated account, destination and request generation.
The epoch host opens only existing catalogue scopes, requires the exact retained
base/head pair, and reads bounded blobs on the profile's existing Git actor.
It neither ingests epochs nor obtains a live comparison. Missing material and
unsupported source are explicit. A fresh server observation removes all cached
readers. Startup with `/user` unreachable still cannot authenticate an account
or expose authenticated cache content. The
[GitHub host contract](github-host-implementation.md) describes these boundaries;
[BPR-0969 integrated evidence](../verification/BPR-0969-cached-review-2026-09-25.md)
records the supported Mac fixture course and the unsupported cases.

[[BPR-1131]] adds RFC-BPR-0052's document tabs to the same authority. The
core's `document_tabs_state` holds one window's tabs for the pull request it
shows: the window's comparison, which new tabs take, and each tab's identity
and exact view (pair, file and reading position), at most sixteen. Its typed
transitions open a file in the active tab or a new background tab, duplicate,
close with the neighbour rule, close others, activate, step to the next or
previous tab, return a tab to the window's comparison and make a tab's pair
the window's. A comparison chosen for the active tab carries the window with
it only while that tab shares the window's pair; sibling tabs keep their exact
pairs. Only the active tab loads. Each load has a fresh request identity, and
`application::document_tab_host` accepts a completion or a reading only for
the load in force, so a late answer for another tab, a closed tab or an
earlier load of the same tab is refused. A view the host cannot present stays
on its tab as unavailable and is never retargeted. The restoration store's
version four keeps a window's collection with its record and with the
recently viewed entry, so Resume brings the tabs back; tab identities are not
durable and a restored collection mints fresh ones in order. Version-three
files gain the two empty tab tables transactionally. [[BPR-1132]] composes one
`document_tab_host` per window. Its exact loads use the existing epoch and
selected-diff hosts and the reading-restoration stages; a tab is ready only
when its requested view is presented. File and comparison gestures are observed
back into the active tab. With no active tab, accepted comparison gestures
update the window's pair in every modality, so the first file inherits the
pair the reviewer selected ([[BPR-1196]]). A background tab creates no load. Qt receives only
`document_tabs_projection.hpp`, sends typed tab intents, and presents the
header, menus and older-pair notice. Tabs persist through the existing
restoration host, and shutdown drains their posted terminals on the decision
lane before releasing the window.

RFC-BPR-0076 routes the four existing file/change QAction presses through normal
Review entry before interpreting source navigation. Core still projects legal
commands and exact bounds and decides the remembered drawer transition. The
composition keeps one latest continuation bound to the accepted PR authority,
workbench generation, active tab and exact file/pair. Its ordinary tab driver
restores that view before Qt applies the existing command projection and sends
its file/change intent. Completion consumes the continuation once; later
file/tab/comparison/modality/Home/composer gestures cancel it synchronously,
and identity replacement is refused by the bound context. The view it waits for
is the active tab's current one: a reading position observed after the press
reaches the tab through the decision lane, and each tab publication
re-evaluates the continuation, so a press made just after a selection is not
held waiting for the superseded position. Unavailable tabs
retain their ordinary explanation without newer-pair fallback. No source tab
is created by entry alone and no full-review scan/model reset is introduced.

## Feedback body digest port

BPR-1447 (RFC-BPR-0080) adds `application::body_digest_port`, a batch SHA-256
of many bodies with no OS type in its signature. The window read host calls it
in the provider's completion callback, on the GitHub provider's effect worker,
before the fetched snapshot is posted to the decision lane, and the digests
travel with the snapshot (`github_feedback_body_digests`, one entry per
feedback post, keyed by post identity and body length). The feedback adapter
hands each matching digest to the core as `feedback_post_observation::BodyDigest`;
the core uses it instead of hashing, and hashes as before when a digest is
absent or does not match. Snapshots compare without the digests, which are a
derived cache of the bodies.

The concrete backend is `adapters::github::platform_body_digest`: CommonCrypto
on macOS and the core's SHA-256 elsewhere (CNG on Windows, HACL on other
platforms after RFC-BPR-0080 r4) (`application::core_body_digest`), still off the window
thread. Composition owns one per profile and passes it to each window read
host. `architecture_boundaries` admits CommonCrypto SHA-2 only in the native
core wrapper, this adapter and the existing designated test-tool owners; CNG
SHA-256 belongs only to the native core wrapper. Application contracts
remain free of OS types. The core still owns every identity it derives; only
this classifier cache key is supplied by the host.

## Diagnostic logging boundary

BPR-0602 adds a bounded `application::diagnostic_log_port`. Composition constructs
and injects the concrete logger and closes it after producers quiesce. The logging
adapter alone owns Quill and file effects; core and Qt presentation have no logger
or Quill dependency. The GitHub native adapter sanitizes only static capability
error message/type/code fields while it owns the active credential. BPR-0971
reconstructs a closed explanation grammar, retains only compiled public-query
symbols and known codes, and explicitly omits unknown content. Provider
callbacks submit that bounded summary and fixed operation facts without changing
request acceptance, retries or terminal delivery. Quill is pinned centrally and
excluded from core-only builds. BPR-1215 frames those same admitted fields as bounded JSONL before Quill
receives them, with adapter-owned clocks, opaque per-logger session, sequence,
closed severity/category and source/toolchain build identity. Existing metadata
export admits canonical v2/v3 and legacy v1 while discarding every free-text or
identity field. BPR-1216 adds the adapter-owned native session-file owner:
exclusive creation, current-file lifetime locks, a shared retention transaction,
5 MiB segments, 50 MiB aggregate, 64 files and 30-day inactive age retention.
Normal record inventory/pruning/writes run on the backend. Producer contention and
backend lock contention drop with counters; I/O failure stays local. Maintenance
recognizes the same closed filenames, separates incomplete file tails and leaves
the coordination inode in place.

BPR-1217 adds profile-owned diagnostic settings. The application host serializes
preference storage and logger barriers on an effect executor, then acknowledges
accepted state on its decision lane. SQLite owns versioned private preferences;
Qt presents the projection in signed-out Settings and Help → Diagnostics.
Composition loads preferences before the first event and clears account context
on Sign out. Standard/Verbose detail is independent of identity privacy. JSONL
v3 uses typed opaque request/window labels by default; explicit Include locally
also retains those numeric identities. No new provider names or content enter
the contract. Warnings/errors use separately reserved bounded Quill queues.
Recent sanitized intermediate context is capped at 512 events, 2 MiB and three
minutes, checked before exposure and retired by bounded idle backend maintenance.
Off, privacy/detail changes and Clear invalidate queued generations and reset
transient context before acknowledgement. Clear coordinates native owned files
without retiring the profile; busy/unsafe/partial failures are distinct. Rich
scope-aware export is described below. RFC-0027 native developer courses and
attended controls are accepted; its verification ledger retains the separate
package/release limitations. RFC-0027 is Auto-Verified under its explicit
dependencies and owner amendment r3.

Diagnostic refresh requests made while a capture is in flight coalesce into one
fresh observation after its completion. Reopening Settings after Sign out therefore
cannot leave a prior capture as the final displayed recent-context count. Queued
epoch effects retain the logger generation observed when they were submitted, so
later protocol steps cannot repopulate a revoked account context.

BPR-1218 carries export policy, session scope and explicit recent-context selection
through the existing maintenance core's exact preparation identity. The logging
control port flushes and returns a sequence cutoff plus selected sanitized context
on the effect lane. Native storage reads only admitted profile files; current-session
selection filters filenames before reading. Application classification rebuilds
bounded JSONL, validates every field and known explanation, and gives each export
fresh opaque session/request/window joins. The core requires separate confirmation
for identifying export, supplied by the explicit Save action in its disclosure
dialog. Direct Help exports use the same profile host without opening Settings.
Preview and the native exclusive local writer consume the
same digest-bound bytes; neither path composes a network capability.

The entry point also owns one bounded terminal service before Qt startup and
after producer quiescence. Its 64 slots hold at most 2 KiB each; ingress drops
on contention/fullness and a saturating count accompanies the next admitted
record. An owned writer handles OS output, then ordinary shutdown cancels and
joins stalled delivery. Qt diagnostics retain severity alone; application
producers admit fixed labels/numbers and classified startup causes. Raw epoch
Git stderr, workspace paths and arbitrary exception strings have no terminal
bypass. Native Windows/Linux delivery remains unqualified; OS crash collection
and uninterruptible device/kernel stalls remain outside this bound.

## Release maintenance and packaged ownership

RFC-BPR-0014 composes `release_maintenance_application_host` through
`src/app/src/release_maintenance_controls.cpp` and the profile desktop. The
core owns one compact pending operation, exact prepared selection, command
legality and stale/cancellation decisions. The host owns frozen diagnostic
bytes, bounded inventory, effect/decision delivery and recovery; the Qt panel
receives seven command projections and the exact preview, never a file/vault port.

`release_maintenance_authority` binds BPR profile, runtime session and admitted
root identity. Requests/events additionally carry request, generation,
operation and complete selection descriptor. Destructive preparation retires
all windows/producers of that profile before storage inventory, while a desktop
holder retains the native root lease, maintenance worker and separate vault
worker. Stores and logger close before cleanup. Cancellation does not restart
the old runtime; startup recovery runs before any replacement stores open.
The [maintenance contract](release-maintenance.md) owns the detailed typed
intents, prepared/commit/terminal events, deltas, limits and direct tests.

The tests-disabled package binds the actual application, agent supervisor,
stdio MCP helper and macOS updater helper to its file inventory and selected
build graph. [[BPR-1179]] makes two component families removable at build
time: `BPR_BUILD_AGENT_INTEGRATIONS=OFF` compiles out the agent launch adapter,
the application's agent hosts, the provider protocol decoders, the MCP
helper and its supervisor and the cycle's Send to Agent adapter
([[BPR-1180]]), and the window withdraws its agent controls;
`BPR_BUILD_LOCAL_AUTOMATION=OFF` binds no owner command endpoint and builds no
`bpr`. The composition decides which family it holds; stored reviews and
cycles never depend on either. The supervisor stays in every package, because
guarded Git launches run inside it. Core remains independently buildable without Qt/providers; no
standalone SDK is implied. The [package guide](../development/release-packages.md)
and [corporate packet](../security/corporate-security-overview.md) distinguish
these mechanisms from pending native, trust, vulnerability and crash acceptance.

## Build-target and dependency direction

The initial target shape should be equivalent to:

```text
better_pull_review_core
    standard/domain C++, plus only the owner-approved pinned, pure
    libraries (glaze, utf8proc, zlib, fmt, sha1collisiondetection,
    HACL* SHA-2, rapidfuzz-cpp, dtl; owner 2026-10-13), and the narrow
    deterministic platform SHA-256 exception (CommonCrypto/CNG, 2026-10-18)

better_pull_review_application
    application host, ports, requests, sessions, caches
    depends on core

better_pull_review_platform
    header-only platform definitions several adapters share and none owns
    (the Windows owner-private object descriptor and its check, BPR-0825;
    the Windows standard-handles-only launch restriction the Git and gh CLI
    runners take, BPR-1115 and BPR-1114)
    depends on nothing first-party

better_pull_review_process_support
    shared adapter support: the QProcess supervision the gh and Git runners
    share (one serialized lane with its quarantine, and the process-group
    wait, kill, resolution, bounded kill-and-reap, background reap and
    abandonment, BPR-1436)
    depends on Qt Core only, nothing first-party; decides nothing about what
    a process runs

better_pull_review_adapter_github
better_pull_review_adapter_git
better_pull_review_adapter_sqlite
better_pull_review_adapter_logging
better_pull_review_adapter_ai
better_pull_review_adapter_regex
    implement application-host ports

    the GitHub adapter may depend on Qt Core and Network for its designated
    process, HTTPS, and native-integration effects, but never on Qt Widgets

    the regex adapter implements core's source_regex_engine interface with
    pinned RE2 and Abseil (BPR-1431) and links only core

better_pull_review_adapter_qt
    Qt presentation adapter
    depends on application-facing contracts and Qt

better_pull_review_app
    composition root; wires concrete adapters, host, core state, and Qt presentation
```

The live repository may use different target names, but it must enforce these rules:

- The core imports no Qt, SQLite, GitHub, Git, AI, thread, future, timer,
  dialog or process APIs. OS APIs are limited to deterministic unkeyed SHA-256
  in `sha256_commoncrypto.cpp` and `sha256_cng.cpp` (RFC-BPR-0080 r4).
  Identical inputs yield identical digest bytes; platform CPU dispatch and
  internal state are implementation details, not semantic observations.
  Core may use chrono duration/time-point arithmetic and caller-owned
  explicitly seeded PRNG state; it must not read a clock, implicit entropy
  or global random state. Cross-platform random identities also need a
  specified algorithm and mapping.
- The core links only owner-approved pinned, pure libraries (no I/O, threads,
  clocks or globals): glaze, utf8proc, zlib, fmt, sha1collisiondetection,
  HACL*'s generated SHA-2, rapidfuzz-cpp and dtl (owner, 2026-10-13;
  sha1collisiondetection and HACL* through RFC-BPR-0080, rapidfuzz-cpp and
  dtl through RFC-BPR-0082; dtl only for per-line intraline comparison and
  the Review Cycle suggestion hunks' line edit bounded at 512 changes, never
  as the diff engine's whole-file line matcher).
  Windows additionally links the system `bcrypt` only in its SHA-256 branch;
  macOS CommonCrypto comes from libSystem. HACL remains the portable backend.
  `cmake/VerifyCoreBoundary.cmake` refuses other unapproved libraries. Before
  hand-writing a general-purpose system in core, ask the owner whether a
  library should be allowed instead.
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
- A definition two adapters must agree on, and neither owns, lives in the
  header-only platform target rather than behind an adapter-to-adapter edge.
  Only its header builds the Windows owner-private descriptor; the storage
  roots and the agent and Git workspace objects all use it ([[BPR-0825]]).
- Code two adapters must share that needs Qt, and that neither owns, lives in
  a shared adapter support target rather than behind an adapter-to-adapter
  edge. `better_pull_review_process_support` is the one such target: the gh
  and Git runners supervise their children through its serialized lane and
  bounded reap ([[BPR-1436]]). It links Qt Core only, launches nothing, reads
  no environment and builds no Windows descriptor, handle list or job; those
  stay with each runner and the platform header.
- The composition root is the only place that knows all concrete pieces.
- In-process communication uses typed C++ calls and records. JSON/RPC is added only at a real process boundary such as an agent protocol.

Build dependency ownership is centralized before these product targets are
declared. Qt is acquired by the pinned provider by default or supplied through
an explicit caller-installed root, then resolved in CMake package mode;
Python, Git, and Linux libsecret and libxcb use the standard CMake or
`pkg-config` providers only when the selected graph needs them. The centralized
provider verifies the resulting `Qt6::*`, `LibSecret::LibSecret`,
`PkgConfig::BPR_XCB`, `Python3::Interpreter`, and `Git::Git` targets before
product subdirectories
are evaluated and rejects any predeclared collision with those owned names.
Standard tool discovery uses CMake's built-in modules and rejects package
redirects for provider-owned names. The source tree configures only as the
top-level project, rejects top-level dependency-provider hooks, and loads its
repository and built-in modules by trusted absolute path. Application installation
and package qualification remain RFC-BPR-0014 work; its core-isolation requirement
does not imply a separate SDK or general CMake embedding support. The
[release-readiness allocation](release-readiness.md) records the current
dependency/helper inventory and the remaining package/security ownership.
CPM owns verified source acquisition
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
    platform/
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
| Local review cycles | Host-owned profile-private cycle store and its content objects | captured revisions and bytes, notes, suggestions, round drafts, frozen rounds, deliveries, responses, verifications, publication plans and journals |
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
Home's profile-owned personal discovery scheduler refreshes retained PR metadata
before running the saved-repository searches ([[BPR-1379]]). It reads at most 30
enrolled PRs per cycle, prioritizing missing titles, with two reads in flight.
Each retained read obtains the title, author, current head, requested reviewers
and at most four pages of submitted review summaries (400 reviews); it reads no
diff, changed files, commit history or conversations. Complete review evidence
updates the existing core attention projection, and a submitted viewer review
retains discovery membership before the old request is withdrawn. Existing
unseen-comment and defect observations remain intact. Exact accepted account,
session, request and enrollment checks reject late or foreign metadata; failed
or limited reads preserve prior facts and do not trigger a projection retry loop.
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

PR Open and Refresh share `github_read_admission` across the windows of one
profile runtime. The host keys its bounded cooldown records by canonical
hostname, viewer node and rate resource, independent of the connection profile
and authentication generation. Every provider submission, including a transient
retry, checks the resources its read shape uses: REST core/search, plus GraphQL
when that capability is used. An already-running request can finish, but neither
an older success nor replacing the session clears a live cooldown. Cancellation,
retirement and shutdown retain their exact-request publication checks.

`core::github_read_backoff` decides the delay from explicit clock observations.
Primary exhaustion waits for the later of Retry-After and the primary reset;
secondary throttling uses Retry-After and does not confuse the ordinary primary
reset with a secondary delay. Missing, zero or expired minimums use 60, 120,
240, 480, then 900 seconds. The fallback cap never shortens a server minimum.
The host translates reset time once to a monotonic deadline and rounds remaining
seconds upward for presentation. Arithmetic saturates instead of wrapping.

Rate refusals make **zero automatic retries**. Open or Refresh before the
deadline reports the retained cause and remaining wait without spending a wire
request; after the deadline a new gesture may start one read. No timer, deferred
callback or retry queue survives cancellation. Closing the window cancels its
active read; later terminals cannot publish into another window or account.
Ordinary network/timeout/unavailable failures retain the existing three-attempt
ceiling, and each retry rechecks shared rate admission. The registry retains at
most 256 resource records, evicts only expired records, and refuses unknown
keys at capacity; a late overflow observation conservatively holds all PR reads
for its minimum. This is runtime-local admission, not a persistent or
cross-process account budget, and does not govern personal discovery or mutations.
Account → Details presents distinct primary/secondary advice. Same-head refresh
retains the exact accepted comparison, semantic selection and Qt scroll position.

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
work; one absent endpoint is legal for add/remove/copy facts. Modified,
type-changed and unchanged facts require both selected endpoints; a mismatch
reports the missing side explicitly, without fabricating empty source or
substituting a merge base. Two absent endpoints and non-blob tree entries fail
closed. A present zero-byte blob remains material, distinct from an absent tree
entry. The current missing-side presentation clears both panes and displays
the typed explanation above them. BPR-0944 separately tracks the owner-deferred
partial-pane design (retain readable right material and explain missing earlier
material in the left pane) and comparison-basis decision. That follow-up changes
no current source or anchor authority. Qt retains the opaque file identity in its list model
and emits a semantic selection without parsing row text or running Git on the
presentation lane.

[[BPR-1327]] prepares the files a reviewer is about to open before they are
opened. `core::prepared_comparison_order` decides which: the file under the
cursor, the files after it in order, then the files before it, bounded by a
window of 16 (`core::prepared_comparison_window`). A review with that many
changed files or fewer is prepared whole; a larger one is prepared as the
window around the cursor and moves with it. After an available comparison, a
file-list change or a cursor move, composition coalesces one preparation per
turn and starts it only when the window changed. The selected file is left out,
because the reviewer's own request is already opening it. Preparation has
three steps, each inside an existing bound:

- **Resolution.** On a dedicated `PrepareEffects` worker, the epoch host
  resolves each file on the repository actor exactly as an open would, under
  the same held selected-comparison authority, and keeps each result as a
  prepared grant (at most 64, dropped with the selected grant on any
  authority, comparison or revocation change). Opening a prepared file reuses
  its grant without running Git, and a prepared grant authorizes only the
  blobs its own tree lookup found.
- **Blobs.** For a sparse review (the connected GitHub CLI loads every review
  whose shape is consistent with `--filter=blob:none`), the prepared files'
  missing blobs are read in batches of at most 16, each blob at most 512 KiB,
  through `git_smart_transport_port::read_exact_blobs`, after the actor proves
  each endpoint durably sparse exactly as a single exact read must. The round
  trip runs on the preparing thread, never on the actor. The host checks every
  answer's SHA-1 against its object ID before it keeps it, through the core's
  collision-detecting SHA-1 (sha1collisiondetection, [[BPR-1450]]), as the
  transport does; a detected input is refused, never retained. Verified bytes,
  including those of single exact reads, are retained per actor (at most 256
  blobs and 16 MiB) and dropped on authority replacement or revocation, so the
  classifier's second read of a blob and a revisit do not cross the network
  again. A transport that cannot batch reads nothing ahead.
- **Diffs.** `diff_application_host::prepare` computes the prepared comparisons
  one at a time on the diff lane, only while no requested comparison is
  outstanding, and stores them unpublished in the diff cache. They may hold at
  most a quarter of its entries and an eighth of its bytes (16 entries and
  8 MiB by default), read no side larger than a quarter of that byte share, use
  only room the reviewer's comparisons leave free, and give way first when the
  reviewer needs room. A request for the comparison being prepared waits for
  it; a request for any other stops it. Opening a prepared file is a cache hit:
  one decision turn, no Git, no network and no computation. It is presented
  as the reviewer's first look at the file, not as a cached revisit.
- **Pausing.** [[BPR-1366]] A prepared computation that the reviewer's request
  stops returns within milliseconds instead of running to its end: within the
  engine's check interval (64 Ki steps), or within a millisecond while the
  line matcher runs, whose thread is abandoned ([[BPR-1485]]). Restart-only
  cancellation keeps nothing: the stopped comparison, if the window still
  wants it, is prepared again from the start, first, once the reviewer is
  idle, and a request for it computes it from the start as the reviewer's own
  work, which preparation never stops. A superseded one (new window,
  invalidation, cancellation, shutdown) is not prepared again. A reviewer job
  polls only its own cancellation.

A new window cancels the previous preparation; invalidation of the pull
request or account cancels it and discards its queue and cache entries; window
close stops it, and shutdown waits for it.

[[BPR-1360]] A cold open reads its two sides at once. When both blobs of the
opened comparison belong to the reviewer's selected grant, the diff worker asks
for them together (`diff_blob_port::materialize_both`), and the epoch host does
both sides' cache and grant work in one job on the repository actor. When both
must be read exactly from the network, the actor lends the left side's read to
the diff worker, which is only waiting for the result, and makes the right
side's read itself. The open then waits for the longer round trip rather than
both, without another thread. The lent read touches no cache session. Both
answers are rechecked and published under the actor's authority lock, and the
diff worker checks them left first, as before. Both reads are the reviewer's
([[BPR-1359]]): the job stops a prepared exact read in flight, and none is
admitted while the job is queued or running. Only the reviewer's own stop (a
superseded selection) or shutdown stops either read. Any other pair, including
preparation's, is read one side at a time.

[[BPR-1370]] Through the connected GitHub CLI, an exact read of a text blob is
one `gh` process: a GraphQL read whose answer also names the account that made
it, held to the lease's bound account as an account observation is. A blob
that GraphQL cannot carry as text keeps the REST read between its account
observations ([GitHub host implementation](github-host-implementation.md#connected-cli-destination-routing)).

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
- Overview sizes its automatic table columns from visible rows. Every file remains in the incremental model; initial native presentation does not measure the entire review for column widths.
- Conversation detail selects its stable root and hides unrelated roots before native item layout measures rows. The complete incremental model remains available for navigation and later selections.
- Compact conversation cards reuse identical label/action geometry within one layout, keyed by complete display text and action flags. Each card keeps its own identity, source and unread state; width, font and policy changes receive a fresh layout cache.
- Unified source text and line geometry are populated only while the one-pane presentation is active. Switching presentation captures the exact semantic reading anchor before rebuilding or clearing that content; two-pane publication leaves the hidden source document empty.
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

Each profile observes successful ledger-store mutations, including private notes
written by agent finding delivery, and coalesces receipt, conversation and private
record invalidations onto its decision lane. Matching sibling windows reload
accepted private state under their current host/account/repository/PR authority
and authentication generation. The signal carries no candidate text or reading
position; window sessions and comparison identities remain independent. A receipt
reload carries durable state to the receiving window's selected material in
memory only, so observing another comparison cannot create a write feedback loop.
Conversation invalidation clears the retained receipt cache and republishes
read-state projections. Private-record invalidation reloads accepted durable
defects without correlating or persisting against the recipient's selected
file or epoch; a failed reload retains the previous accepted projection. Note refresh
preserves rejected edit candidates. Closing/replaced windows and retired account
generations refuse delivery. This projection path grants no publication authority.

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

Syntax delivery binds to the comparison surface's exact loading or displayed
context, rather than the diff host's previous accepted result. Cached spans may
arrive before the replacement diff and stay attached through the bounded Qt
turns that install a large document. Leaving that context clears them, and a
publication for neither current blob is discarded. [[BPR-1382]] exercises both
panes through Next/Previous navigation, including cached large-file revisits.
A publication colours every side whose blob it names, so a pure rename, whose
two sides share one coalesced classification, colours both panes whichever of
the classification and the diff answers first. Leaving a comparison retires its
in-flight classifications with the blob reads it stops, so a later request for
the same blob is submitted afresh rather than joined to a stopped read
([[BPR-1558]]).

The syntax adapter implements the host's classification port through
KSyntaxHighlighting's low-level interface. It classifies a complete blob in one
grammar pass and projects a hunk from the complete result; highlighting a range
on its own would lose the state that opened a multiline construct earlier. It produces core
roles, never colours, and consults no standard path. RFC-BPR-0072 adds bounded,
cancellable C++ refinement over grammar-protected spans: blob-local namespace,
enum, scope and delimiter facts recognize declarations and unambiguous qualified
uses. Aliases, shadows and unresolved scopes stay conservative; no compiler or
cross-file lookup is composed. C++ identifier revision 6 retires prior spans; C
and Resource retain revision 5. Core appends stable namespace and enum-member
role identities; Qt owns their four-variant defaults and independent controls.

The Qt adapter turns a core-resolved variant plus a palette into applied
presentation without installing a forced style or QSS, preserving the contract
that the application inherits the native style and interface font. Each variant
keeps its own customization: the durable record holds one palette document and
one stored flag per variant (appearance schema format 7, RFC-BPR-0034), and the
adapter's override table is keyed by variant, so it reads each variant's record
as overrides against that variant's built-in profile and applies them only
while that variant resolves. A light customization therefore never colours the
dark profile when the desktop scheme changes, and committing one theme leaves
the other three exactly as they were. Before format 7 there was one palette
tagged with the variant it belonged to (`palette_variant`, format 2,
BPR-0645), so tuning a second theme discarded the first; `palette_variant` had
no meaning left once every variant owns a slot and is migration input rather
than a column. The stored flag, not the document, says whether a slot is a
decision: every slot holds a *complete* document, because the reader imports
each one through the core parser. Every original role is required; additive
label, attention, namespace and enum-member roles omitted by older documents
resolve to that variant's built-in defaults. Existing values and stored flags
survive without a palette reset. Scoped palette reset removes one variant's
overrides without deleting other appearance preferences.

An exported theme is a version-2 document whose `base` line names the variant
it customises ([[BPR-1479]]). The store's per-slot documents stay version 1,
because the slot is their base. Theme import fills the additive roles a
document omits from its base's built-in palette before checking contrast, so
what is previewed is what Keep changes stores and a later export writes. A
version-1 document's base is inferred from its code background (Light or Dark,
never high contrast). The core parses and decides the base; the composition
hands the host the four built-in palettes, which Qt owns, and the host
performs the fill. The theme on screen plays no part.

Migration files the version-6 palette under exactly the variant that row
recorded, keeping its document bytes, and leaves the other three uncustomized,
which is the same "no customization" a fresh installation has. It runs inside
the transaction that stamps the new version, admits the legacy row through the
version-6 reader first — so a tampered document, an unpublished mode or variant
name, an out-of-range flag or illegal typography refuses the migration rather
than being repaired into the new shape — and binds the variant names as
parameters rather than interpolating them into SQL. A database stamped newer
than this build is refused rather than opened, and a stamped file's application
identity is checked before the first write, so another application's database
leaves untouched. Formats 1 to 6 migrate with customization retained, except
the deliberate one-time clear format 4 performs for [[BPR-0720]]'s changed
built-in dark palette. Theme export runs through the appearance host and an
atomic bounded document writer; it exports the accepted palette of the theme on
screen, which the composition supplies, and excludes unsaved previews.

The Qt-free `core-only` preset acquires neither KSyntaxHighlighting nor the
syntax adapter; its resolved package set remains SQLite alone.

## Dif asset ownership boundary

RFC-BPR-0030's accepted production revision places Dif's editable source
frames, manifest, provenance, and committed generated products under
`src/adapters/qt/assets/dif/`. The offline `scripts/dif_assets.py` tool validates
and regenerates that tree. It is a maintainer tool, not a product dependency:
ordinary developer and tests-disabled builds consume committed outputs and do
not execute it; `core-only` selects neither the outputs nor Qt.

The Qt adapter binds the fixed generated descriptor and ten accepted strips as
resources for the tests-enabled and tests-disabled applications. It owns the
lazy shared decoded cache, elapsed-time sequencer, widget, painting, window
eligibility, native motion observation, and presentation-only cue latches. The
application host owns persisted preference effects and offers typed accepted
presentation facts to Qt after authoritative content has landed. The core owns
only deterministic appearance preferences and bounded observation resolution;
it contains no mascot asset, clip, frame, timer, widget, decode, or cue state.

This allocation preserves RFC-BPR-0019/0020 as historical design context while
RFC-BPR-0030/0031 supply the current ten-clip revision and composition. Dif's
base remains prop-free. Any raster change returns through deterministic
regeneration and a digest-bound attended decision before runtime selection.

One static Dif image lives outside the catalog: the macOS menu bar status item
([[BPR-1250]], RFC-BPR-0031's 2026-09-13 owner amendment). Its 18 and 36 pixel
template images are in `src/adapters/qt/assets/status-item/`, made from the
neutral master by the offline `scripts/dif_status_item_template.py`, and
compiled into macOS graphs only. `qt::menu_bar_status_icon` marks them a mask,
so the menu bar tints Dif. The desktop composition owns the one status item
(`native_menu_bar_status_item`) for the life of the process and lends it to
each profile's notification sink, so it stays in the menu bar when the last
window closes. Linux and Windows keep a tray per profile.

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
| Review Cycles | cycle, revision, item and round identities and transitions, capture-path admission, exact comparisons, suggestion conflicts, round assembly and freezing, the disclosure decision (`authorize`), response admission, publication plans and journals, wire names | cycle runtime and store, read-only workspace source, capture lane, exports, owner command channel, harness grants, publication catalog and GitHub publication session, Send to Agent through the existing launcher | cycle page, drawer, code view, notes, Suggest change, Compose feedback and Share notes sheets, Publish as Pull Request sheet, Updates line |
| Appearance | no color/font/widget authority | settings persistence and cache scheduling | system mode, fonts, colors, syntax presentation, accessibility |
| Dif mascot | appearance preferences and bounded native-observation resolution only; no asset, clip, frame, timer, widget, decode, or cue state | persisted preference effects and typed accepted presentation facts; no art generation, decode, painting, or animation state | RFC-BPR-0030/0031 fixed resources, shared decoded cache, elapsed-time sequencing, painting, window lifecycle, native observation, and bounded presentation-only cues |

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

Normal production inline publication composes the asynchronous host from the
accepted authenticated handoff and snapshot. Acting identity and the native
transport lease remain bound to that exact authority. BPR-0641 adds a private
user-publish issuer: after the core accepts Publish, composition passes a
five-minute, one-use grant bound to the entire publication request, session,
generation, target and actor. Host validation still checks retained endpoints
before consuming it. Test builds refuse this issuer; the real editor fixture
uses exact request-bound testing grants. A subsequent deliberate gesture needs
a fresh grant. Ordinary launches do not load an attended approval.

The separately selected designated attended validation
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

The attended source-line carrier is private and one-use. It first binds the application
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
comparison, and algorithm identity. The algorithm revision is 2 since linked
xdiff replaced the first-party matcher (BPR-1497); a key or result at another
revision is refused and recomputed. The persisted comparison digest and file
material leave the revision out and keep the bytes revision 1 wrote, because
receipts, defects, drafts, private notes and threads, tours and window
restoration store them beside line coordinates that do not depend on the
alignment. Qt-local state includes measured line
boundaries, the measured alignment axis, the retained subpixel comparison
position, shared-scrollbar projection, hover paths, and splitter geometry.
Measured pixels and the live scroll position are not core or durable state.
RFC-BPR-0073 applies the containing native row and the remaining within-line
pixels as one origin: the native viewport is placed and clipped, so its text,
selection and pointer coordinates move together. The fixed overscan is bounded
by font metrics; fractional events do not resize the viewport or remeasure the
file. Static gutters and link-map bands use that same applied origin. Qt-local
pixel-gesture state suppresses perpendicular drift on the predominant axis before
scrollbar clamping. Recent directional evidence can release or change that
preference; discarded motion is never banked, and momentum retains the established
axis. Phase/device/idle and comparison resets clear context. Keyboard reveals
place a clipped row flush with the nearest displayed edge within one device
pixel through the receiving pane's existing authority. The row is placed from
the first visible row by the measured axis's line tops (in unified view, its row
stride and the top margin row 0 alone shows), not by Qt's block geometry, which
stops walking about two viewport heights from the first visible block, so a row
selected far out of view is revealed at that edge too ([[BPR-1476]]). Subsequent reveals scroll
one source row per arrow; the flush origin may retain a fraction. Page destinations
remain line-aligned. In-view cursor navigation retains fractional placement.
Source horizontal bars use Qt's ordinary native controls and transient feedback,
including Qt's normal flash after a vertical row value changes. The owner withdrew
the request to suppress that flash; no custom style-option or axis-notification
hook remains. The platform style owns visibility and fading. No scrollbar policy
or origin changes.
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
action hides or restores the secondary session/queue/pull-request header and
epoch/history controls together. Since [[BPR-1334]] the lower pane is Line
details, a profile preference with its own View menu action, shown by default. Hiding them does
not clear their values or semantic selection; it returns their vertical extent
to the two source panes and center map. At the 1280 × 1000 evidence viewport,
the selected-diff contract requires the native comparison splitter to occupy
at least 75% of the client width and 85% of its height while every hidden
surface remains reopenable through the shipped toolbar. That height floor
reflects the product-owner requirement that all secondary upper and lower
detail chrome collapse together.

## Review Cycles

A review cycle is a second review subject beside the pull request, under the
same allocation. The core (`bpr/core/review_cycle*.hpp`) owns the cycle,
revision, item, round, response, verification and publication identities and
their transitions; capture-path admission and byte classification; exact
comparisons; suggestion conflicts; round assembly and the frozen canonical
payload; the disclosure decision `authorize` that every channel asks; response
admission; publication plans and journals; and the wire names (`RC-3`, `R2`,
`I7`, `F1`) the desktop and the terminal share. It names no host, account,
filesystem or process.

The application host composes `review_cycle_runtime` over the profile's
`review_cycle_store` with a decision lane, an effect lane and, in the desktop, a
capture lane of its own, and nothing else: no account, provider, vault, agent
or executable lookup. Its ports are the read-only workspace source
(`bpr/adapters/workspace`, which never runs Git), the SQLite cycle store, the
local export destination, the installed-Git publication catalog
(`publication_candidate_port`, in a BPR-owned store) and the GitHub publication
session (`github_publication_port`) minted by the accepted account's transport.
`review_cycle_workbench` turns them into the page's steps. Qt presents: the
cycle page (`review_cycle_view`), its Compose feedback and Share notes sheet
(`review_cycle_share_sheet`), Publish as Pull Request (`review_cycle_publish_sheet`)
and Add Notes to Review Tour (`review_cycle_promote_sheet`), with no store,
file, Git or network effect of their own.

`src/app` composes the desktop's cycle host (`review_cycle_desktop`) and the
terminal (`cycle_cli`, `cycle_commands`, `cycle_session`) over the same runtime.
A profile has one writer, the process holding its runtime lease; a terminal
command meets a running desktop through `cycle_owner_service` on the profile's
same-user owner channel and runs there on the desktop's store, so both
interfaces give the same answer. Send to Agent (`review_cycle_agent_sender`)
hands the frozen round to the existing agent launcher and records what it saw;
it is compiled only with agent integrations, and the owner channel and `bpr`
only with local automation (see [Release maintenance and packaged ownership](#release-maintenance-and-packaged-ownership)).
Stored cycles depend on neither family. The
[domain model](domain-and-state-model.md#review-cycle-identities-rfc-bpr-0053),
[terminal guide](../development/cycle-cli.md) and
[threat model](security-threat-model.md#review-cycles) carry the contracts.

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
page or Git object and is bounded to 2,112 bytes. The host owns refs, object
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
cannot construct an address or acquire an opener. RFC-BPR-0036 adds one guarded,
inert exception: the unnamed address parameter required by the private Markdown
document's resource-denial override. That function returns empty bytes without
reading the address. Markdown destinations remain untrusted text gestures for
application admission; [the reader contract](native-markdown.md) describes this
component boundary. `bpr::app::system_browser_opener`
is the one place in the shipped graph that hands an address to the platform's
own launch service: no embedded web view, no browser engine, no network of its
own, no listener, and no supervised child. Both graphs bind it for a person's
run. A run inside the automated test boundary binds a recording opener
instead, so no suite in this repository can start a browser; that boundary is
a run-time fact, not a build-time one ([[BPR-0797]]): the marker
`BPR_AUTOMATED_TEST_BOUNDARY` that `bpr_add_network_isolated_test` injects
into every registered row, or a selected fixture. `BUILD_TESTING` is on by
default, so a build-time gate had put the recorder into the application a
developer runs and let it report a handoff nothing had opened. One exception
is a person's again: an attended look (`--attended-launch` outside the marker)
binds the real opener for the window's own opens, Open on GitHub and body
links, while its fixture device sign-in keeps the recorder ([[BPR-1338]]: the
owner pressed Open on GitHub in a frozen attended build and nothing opened,
because the selected fixture had bound the recorder). Composing none
at all is the construction cut, and only then does the action answer
`unavailable` ([[BPR-0516]],
`application_device_authorization_browser_handoff`,
`application_device_authorization_browser_absent`,
`application_composition_capabilities_inside_boundary`,
`application_composition_capabilities_product_run`, `composed_application_hosts`).
The marker has one reader, `bpr::app::inside_automated_test_boundary` in the
composition, compiled only into tests-enabled graphs: a profile's composition,
the release updater (which installs nothing and opens no browser inside the
boundary) and the update lifetime all ask it, and a tests-disabled binary
neither carries the name nor answers to it ([[BPR-0802]], [[BPR-0983]]).

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

## Release updater discovery, staging and installation

RFC-BPR-0035 milestones 1 and 2 add deterministic stable-version/asset/scheduling
policy and one desktop-wide application service. Menus and General Settings
project the same host; SQLite persists only preferences and an opaque account
binding. Explicitly selected native GitHub.com authority supplies a release-only
lease, the designated HTTPS adapter streams private assets, and the macOS update
adapter performs bounded ZIP extraction and native signature/trust verification.
Core and Qt gain no effect APIs. LibArchive 3.8.9 is pinned by the central
dependency provider for macOS staging, with system zlib and bundled upstream
notices. Milestone 3 adds a bundled helper, cooperative process-lifetime locks,
desktop-wide draft preservation/shutdown, a durable replacement journal and an
exact healthy-start receipt. Its platform adapter owns fixed signature/trust
commands and exact helper/application launches; private SQLite stores inert draft
recovery text and Qt presents it read-only. Cleanup is journal/inode-authorized.
The startup transaction binds the canonical private state-root inode before any
store opens. Disposable signed/notarized upgrade and recovery qualification permits
installer selection for eligible trusted macOS installations; explicit verified
candidate gestures still gate replacement. No developer application was replaced
or relaunched, and no release was published. The
[release updater contract](release-updater.md) binds the fixed distribution
source, separate native authority, limits and successor obligations.

## Performance contract

RFC-BPR-0077's [source search session foundation](source-search.md) follows
the same allocation: compact core scope/generation/count/cursor policy, bounded
host-owned query packets, off-lane effects and serialized typed terminals.
The shared domain matcher, source plans, path filters and exact record acceptance
are supplied by M2; M3 adds exact retained-tree membership, epoch-actor
acquisition, immutable read reuse and bounded progress. The independent native pane and PR controllers compose View > Find actions,
selection/reveal and the incremental Files/Search drawer through these same
ports. Exact scope/query and monotonic reading identities reject late results
and reveals. Search has its own profile effect lane so selected navigation can
yield background acquisition, then resume the original full corpus. Search
shares the existing actor material cache without enlarging it; native models
borrow immutable records and own bounded numeric indexes. Public performance
records distinguish acquisition, matching, process RSS and component admission.

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

## GitHub CLI read milestone

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a chosen-account connection question,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; account discovery and API use require `gh` 2.96 or later
(including later majors)
because the account inventory uses `gh auth status --json hosts`
(every account on each host; `--active` only for the default choice).
Older versions are refused before account status runs and the UI requires an
upgrade. BetterPullReview never passes `--show-token`.
This is a version check, not proof of an official executable's provenance.
The r2 trust-boundary amendment permits a bounded exact-repository `gh` Git
operation for objects; `gh` may internally launch Git and its shell credential
helper. Every macOS `gh` launch must run under OS-enforced file-write denial,
with a narrowly owned temporary object-store exception only for the bounded
clone; an unavailable boundary refuses the CLI profile. This covers `gh`'s
possible first-use global configuration migration, which a post-request check
cannot prevent. BetterPullReview must not read the helper's token exchange or
change global authentication. Full Git comparison requires `gh` 2.96 or later
(RFC-BPR-0015's account and API floor) and Git 2.49 or later
(`clone --revision`), not one exact vendor or banner. Before any credentialed
`gh` use, bounded write-denied local `gh api --help`, `gh repo clone --help`
and `gh auth git-credential --help` probes must show the flags and usage the
app invokes, and `git clone -h` must show the required clone options. A failed
check and an unsupported tool reach the comparison as distinct typed reasons.
Capability admission retains actual clone and exact-object validation. Native
qualification evidence remains installation-specific. The 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold
remain; the aggregate threshold is not a hard disk quota. Local refs reflect the accepted
API snapshot, not fresh remote advertisement. A tests-disabled macOS fixture
now renders a full PR diff through the CLI profile; an isolated build with
the process adapter removed from composition visibly refuses the same healthy
fixture. A macOS native read-only probe now verifies one official `gh` HTTPS clone of a
small owner-controlled private repository through `gh auth git-credential`;
`gh` reports a keyring token source and a matching Keychain item exists.
The probe does not bind the precise Security API calls, credential generation,
or concurrent account selection. The private repository name and objects
were not retained. On macOS, the opted-in CLI profile now routes whole-review and
right-side inline-comment publication through the existing mutation admission,
strict exact-account/head/content admission and one-use grants from the ordinary
review/comment gesture, plus
server reconciliation. The acquired credential stays fixed across checks and dispatch; an ambiguous
attempted write is not posted twice. Fixture actions
exercise this path; no live PR was written. Windows and Linux remain
unavailable. Mac qualification is recorded by BPR-0787. RFC-BPR-0014 is now
Auto-Verified, and RFC-BPR-0015 became Auto-Verified on 2026-09-23 through
BPR-0788. The product owner approved the pending review of the CLI
authority and protocol changes made after that qualification. That
approval is an owner decision, not an independent reviewer's report.

Tour Publish is a bounded grouped-confirmation extension: the preview binds one
immutable plan to the exact actor, head, sources, prose and allowed returned-link
slots. Each journaled write consumes its own one-use child grant. Stop or an
identity/draft change revokes remaining authority; restart reconciles read-only
and needs an explicit preview and Resume. Compatibility comments are explicitly
non-atomic, and an optional description edit is a separately named operation.
These Tour operations remain unavailable on an unqualified host/transport.

RFC-BPR-0078's `strict-cli-publication-r2` amendment permits one transient
credential inside the GitHub adapter for each chosen-account operation. Stock
`gh auth token --hostname HOST --user LOGIN` runs under write denial and a private,
token-free configuration; its bounded sensitive output never enters a generic
process result. Missing or inaccessible named credentials share the fixed
`credential_unavailable` refusal before any API or network Git launch. Tool
failure remains distinct, and a possibly transmitted write remains indeterminate.
One immutable owner supplies `GH_TOKEN` or `GH_ENTERPRISE_TOKEN`, selected by
stock-gh hostname classification, to that operation's identity validation, API
calls and gh-owned Git/helper descendants. Subsequent operations acquire afresh;
there is no ambient-account or native-transport fallback. Login and retained
numeric account checks still refuse a misbound credential before a write.

BPR creates no durable credential copy: no token enters arguments, retained
outputs, SQLite, private configuration, object stores, recovery records or a BPR
vault. Owned buffers are overwritten after queued work and descendants drain,
including quarantine. Qt, OS and stock-child copies, swap and crash dumps are
outside precise-erasure claims. Removing the named item after acquisition cannot
replace bytes already held; server revocation may still reject them. Global gh
account/configuration is unchanged. The configured executable, system trust,
redirect behavior and helper processes remain trusted surfaces; private per-call
configuration excludes the owner's ambient socket/configuration settings.
[Strict qualification](../verification/RFC-BPR-0078.md) records native scope and
negative controls. Read opt-in grants no publishing authority. RFC-BPR-0013 keeps a 250-record limit for commits returned by the pull-request
endpoint. A larger reported total is accepted only when that endpoint
returns exactly 250 records ending at the accepted head. The UI names the
reported total and returned count separately; this does not establish that
any particular private pull request can render.

## Semantic source-line publication allocation

RFC-BPR-0022 uses the existing publication pipeline. Core
`review_publication_policy` owns representation and legal durable transitions.
The mutation host supplies exact admitted source and actor facts, then the
serialized GitHub session owns bounded write-capability observations and the
SQLite logical journal, retained by the current schema-5 store. The adapter owns the canonical permalink and
exact wire classifier. Application `review_comment_placement` decodes changed
FILE roots against sealed source, caches by root identity/body and source, and
projects semantic anchors separately from remote subjects. Qt renders the
projection and indexes accepted anchors; it performs no provider work or link
parsing on paint, hover or scroll.

The [owning round-trip contract](review-round-trip.md#semantic-source-line-publication-policy-rfc-bpr-0022)
defines the single state machine, limits, migration and cancellation rules.
Its successful receipt selects the exact server comment only after an ordinary
accepted refresh and source material are available. No widget capability,
second mutation lane, or full model reset is introduced.

## Authored Home section allocation (RFC-BPR-0066)

The core's `home_section_for` classifies each accepted queue row from its own author-role/authored-discovery membership and unchanged attention bucket. Waiting-only authored rows stay in Waiting on others; other authored rows go to My pull requests. The Qt landing model projects this decision separately from its attention color role, and the existing window publication restores exact selected identity after accepted refresh. No new provider work, enrollment, notification policy or repository-drawer attention is introduced.

The host's window-owned `home_view_state` adds bit 16 for My pull requests without changing the original collapse bits. Workbench settings schema 19 widens the private window row's collapse bound to 31 and transactionally rebuilds older tables with all existing fields copied. The new bit starts zero on upgrade; named group/band collapse remains window-session state. Profile defaults carry no Home view.

## Draft Home metadata allocation (RFC-BPR-0067)

Home’s Draft chip projects an optional accepted GitHub metadata fact. Personal
searches and retained reads carry the field without another request or full PR
hydration; accepted ordinary PR reads carry it through the existing queue and
Recent paths. Missing metadata remains unknown and cannot erase a retained fact.
Queue enrollment schema 8 retains the optional flag and its accepted numeric
account under the existing profile/host/login/PR key; the session binds that
numeric account before reuse. Older rows migrate to unknown. Recent schema 6
adds a nullable flag to the existing complete destination (including access mode
and numeric account), preserving positions and document tabs transactionally.
Accepted queue publication updates an existing matching Recent destination
without changing its viewing order or granting reading authority. Qt paints the
leading neutral chip and draft-first accessible wording through the shared row
delegate. Attention, membership, sorting, notifications and opening are unchanged.

## Read-only merge requirements allocation (RFC-BPR-0069)

The GitHub adapter compiles separate bounded read queries for the current review
request connection (`asCodeOwner` and typed user/team identity) and the PR's
merge/review/auto-merge assessment. It exhausts requests at 1,000 records/10 pages,
selects active applicable target-branch rules through the branch-rules endpoint,
and independently reads classic protection. Optional explanations share the
selected account transport and resource rate observations but have a separate
8 MiB/32-page aggregate budget after essential PR reads. Authentication or account
binding loss fails the entire read; optional policy/roster failure cannot erase
other readable observations. Native HTTPS and the closed CLI compiler select the
same GraphQL/REST surfaces. No team membership or local CODEOWNERS evaluator is
introduced.

Mandatory snapshot content and search finish before check runs and commit statuses
spend the remaining shared page/byte budget. Exhaustion qualifies only those
optional results; it cannot revoke a completed mandatory observation. A complete
REST fallback carries actor-roster observation separately from the richer
connection's availability, including an observed empty roster. The window and
Home replace prior requests with those current actors while retaining the actual
code-owner availability qualification.

The application validates bounded nested requirements and field provenance,
retains same-authority/head/base observations as explicitly stale on failure,
and projects indexed context/app matches, waiting identities and truthful
requirement text. Missing app identity cannot establish optionality against an
app-restricted context. Missing expected results do not enter observed totals.
Qt presents native wrapped labels and disclosures with literal, accessible
wording; its existing exact check-link intent remains the only external action.

## Home waiting allocation (RFC-BPR-0070)

The core's typed `queue_waiting_facts` keeps outstanding requests and server review
requirements separate from check/merge blockers and local unread activity. The
application maps the existing read observations, merges only observed fields under
profile-wide dispatch order, establishes exact actor authorship for existing
enrollment and persists bounded account-bound metadata in queue schema 9. The
GitHub adapter reuses closed read operations for background summaries under shared
budgets. Qt presents names, blocker text, accessible reasons and filtered counts
through the existing native Home models and delegate. The
[queue contract](review-queue.md#home-waiting-observations-rfc-bpr-0070) names limits,
restart/freshness behavior and qualification boundaries.

## Remembered connection allocation (RFC-BPR-0071)

The core owns bounded selected-connection state, ordering, replacement and stale-result admission. A profile-owned application capability supplies serialized metadata effects; SQLite owns token-free transactional snapshots under the existing private root. The startup effect loads metadata before authentication selection. Composition reconstructs stable transport/account identities above retained allocator counters and shares one validation across windows. The profile-owned remembered connection host keeps logical selections separate from the last committed snapshot; a single writer rebases each admitted operation on that durable revision after a refused write. Successful authenticated selection triggers a prompt save independently of window layout; orderly Quit drains admitted saves. A disabled-reuse row suppresses old device resume. No storage record is an authenticated authority. Accepted-session projections alone may trigger remembering; an invalidation projection cannot reuse the retiring model. Exact-host setup supplies a remembered-revalidation fact to core, which selects authentication before private reading. BPR-1409 integrates these runtime paths; [BPR-1410 qualification](../verification/RFC-BPR-0071.md) covers fresh processes, replacement, retirement and persistence refusal.

A native sign-in begins on a profile that composition mints before the account is known. Once the host's bootstrap `/user` names the account, and before any vault write, core's `adopt_native_connection` decides whether this BPR profile already holds that account's own native identity on the same server: the same host ID, hostname, server, numeric account and login, selected or retired. Composition re-binds the enrollment to that identity when its profile is registered in the running process, and the host restarts the same material there; `/user` must then name the same account. The authority-scoped private state saved under the identity, such as an exact retained comparison, therefore resumes. The snapshot retains up to sixteen retired native identities that another account's native selection displaced. The update check admits a returning identity only exactly as it was retired. A CLI selection or Forget drops the server's retired identities, so a native sign-in that replaces CLI, and every different account, still receive a fresh identity. Startup rebuilds only the selected connection's profile, so after a restart an earlier retired profile is not re-bound and that sign-in keeps a fresh identity ([BPR-1494](../issues/60-done/BPR-1494-a-returning-account-does-not-resume-its-retained-comparison.md)).

## Inline Markdown image allocation (RFC-BPR-0084)

The core decides (`bpr/core/markdown_image.hpp`): which image references of a
rendered body are admitted, the bounded `<img>`/`<picture>`/`<a>` recognizer,
how a reference matches the address GitHub's own rendering serves for it (a
camo `data-canonical-src`, an attachment's UUID, or the exact address), whether
a fetch and each redirect hop are admitted and may carry the profile's
credential, the displayed size, the bounds, and the cache identity. The
application host performs: `markdown_image_window` (one per window read host)
admits a body's references against the accepted Markdown context, batches the
bodies a turn asks for into one GraphQL resolution through the GitHub provider
(`markdown_image_resolution_port`), and loads each served address through the
profile's `markdown_image_service`, which runs at most four fetches on serial
image lanes (`markdown_image_fetch_port`), decodes on its own lane
(`markdown_image_decoder_port`) and keeps a 256 MiB least-recently-used decoded
cache keyed by host, account and canonical source. Bytes that decoded are also
kept on disk (`markdown_image_disk_cache_port`, the SQLite adapter's
owner-private `state/v1/markdown-images.sqlite3`): 128 MiB least recently used,
an entry expiring seven days after its last use, keyed only by a SHA-256 digest
of the cache identity and removed by profile purge. Before a body is resolved,
each absolute or attachment reference is looked up there on the decode lane, so
a restarted application shows a cached image without a request. A changed scope (window,
session, identity, account or pull request) cancels earlier work; a body's
repeated request keeps what it still asks for. The GitHub adapter resolves
through the exact account transport (`resolve_body_images`, a closed GraphQL
`nodes` query of `bodyHTML` scoped to the accepted repository and pull request)
and fetches through native HTTPS: a credential-free media binding per lane and
profile, or, for a GHES credential origin, the account's own transport on the
provider's serial lane. The Qt adapter decodes with Qt's PNG reader and the
shipped `qgif`, `qjpeg` and `qwebp` plugins, and rasterizes core-admitted SVG
with QtSvg's renderer (`qt::markdown_image_decoder`, signature chosen, bounds
before allocation) and presents through the reader
([Images](native-markdown.md#images)). Qt never receives an address, signed
address or credential. Composition builds the lanes, decoder, service and
dispatch per profile and window; a fixture run serves `markdown-images-v1`'s
recorded rendering and images and never the network. Whether images load at all is the profile's workbench
preference `review_markdown_images` (schema 21, Load by default): the Qt window
applies a published change to its readers, and Don't load sends no image
intent, so the host resolves and fetches nothing.

## Insights window and storage allocation (RFC-BPR-0086, first milestone)

One `qt::insights_window` belongs to each `github_profile_runtime`, alongside
Settings. Its menu action and central shortcut registry dispatch to composition;
Qt receives only `application::insights_snapshot` and semantic callbacks.
The window owns visibility and presentation timers. `application::insights_host`
permits one pending snapshot, performs measurements on the profile’s dedicated
effect lane, throttles storage to sixty seconds, and posts exactly one terminal
to the decision lane. Shutdown stops the window’s timers and drains initialization,
measurement and completion before closing that lane.

The SQLite adapter initializes owner-private `state/v1/insights.sqlite3` at the
profile’s first launch. Schema 1 stores the start time, last measured aggregate
bytes and local repository display labels; lifetime counts and history remain
later milestones. Storage sums file lengths and SQLite sidecars without reading
other stores’ contents. Git repository bytes use the same bounded file inventory
helper as the existing cache adapter; no cache session or Git process is opened.
A changing or refused entry can make an observation unavailable. This is a local
snapshot, not a transactional filesystem-wide snapshot across active writers.

The existing maintenance inventory recognizes Insights as profile state. Core
classifies it as statistics, removed only by Reset everything among the four
Troubleshooting scopes. Full profile purge also removes it. An anonymized frozen
export reads only its last byte aggregates and recomputes the frozen descriptor’s
size and digest; identifying and metadata exports add nothing from Insights.
The [window contract](../ux/insights.md) records the currently delivered surface.
