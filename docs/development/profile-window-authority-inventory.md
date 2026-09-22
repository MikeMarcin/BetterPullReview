# Profile and window authority inventory

Planning for RFC-BPR-0024, inspected at `e354b45e`; setup destination work
BPR-0689 is concurrent. The ownership table and foundation decisions below
record design and source ownership rather than implemented behavior; the
closing section on the empty desktop records shipped behavior and says so.
BPR-0595 is the acceptance ledger.

## RFC-0024 r2 implementation continuation

BPR-0976 now shares fresh validated server conversation snapshots through the
profile/read hosts to exact-authority siblings. A content-only projection
preserves their navigation and retained comparison; no Qt event bus or second
provider read is involved. BPR-0977 adds bounded window-local native geometry
to the existing layout row and serialized write lane. Its composed restart
course checks distinct observed placements, immediate Quit, exact stable IDs,
explicit Close, defaults isolation and unavailable storage; offscreen checks
are not native focus/lifetime acceptance. Native gaps remain on its live card.

## Existing ownership

| Surface | Current owner and consequence |
|---|---|
| Application composition | `src/app/src/github_application.cpp`, `github_application::implementation`: one window captures one set of hosts, providers, executors, stores and effects. Constructing it twice against one root would duplicate owners. |
| Durable roots | `native_application_data_root` and `prepare_application_data_root` supply one root to SQLite domain stores, caches, epochs, diagnostic log and workspace ownership. Individual domain schemas must remain independently owned. |
| Credentials | `github_credential_authority_key` includes host-profile identity/revision, server, account and contract, but no BPR profile. Native vault namespace is shared. A root partition alone cannot isolate credentials. |
| Authentication and active PR | `github_application_host` contains both active authentication and selected PR state. Authentication must be coordinated within a profile; active PR reads belong to each window. Sharing the complete host would move both windows together. |
| Review and edit records | Ledger store is one durable authority. Notes lack expected revisions; inline draft revision is in composition memory. Shared edits need compare-and-set outcomes retaining the rejected candidate; no second ledger. |
| Reading state | `workbench_application_host` owns selected diff, cursor, file order and generation per instance. Keep these per window. Layout persistence currently uses one singleton record and mixes durable preference defaults with current drawer state. |
| Settings | Each workbench owns a Settings child today. Move singleton ownership to profile runtime; accepted preferences project to every window of that profile. |
| Lifetime | Startup creates one application object. At this audit, last-window shutdown terminated its services, and there was no profile admission owner, stable window identity, or distinct close-versus-quit restoration path. RFC-BPR-0033 supersedes the first of those and the core catalog the last: an injected policy decides what an empty desktop does, and an explicit close and Quit now keep different restore records. |

The read-only scout verified these ownership facts. BPR-0689 adds a shared,
synchronized provider catalog for live server registration, but does not create
BPR profiles or make provider host IDs a durable BPR identity.

## Foundation decisions

Use distinct opaque `bpr_profile_id` and `bpr_window_id` values in core. Immutable
Default identity adopts the legacy environment in place: domain databases,
cache objects and epoch IDs are not moved or recreated. A transactionally
recorded registry adoption marker makes repeated and interrupted startup
idempotent. Newly created profiles use derived private child roots and standard
empty defaults. Labels are display data and never paths or authority keys.

The registry is bounded, versioned and failure-closed. Only Default may request
legacy vault adoption, through an explicitly profile-bound compatibility namespace; a future or failed
registry never falls back to another profile. Root selection belongs to the
application/adapter boundary and must retain owner-private and canonical-path
checks already used by domain stores.

One profile runtime owns shared durable services, auth generations and accepted
review records. Window sessions own selected PR reads, comparison/epoch state,
reading position and layout. Each completion carries its profile identity and
applicable window/request identity. Closing a window retires only its consumers;
profile quit retains all open-window records before service shutdown. UI is not
the inter-window event bus.

## Acceptance map and planned direct checks

| Criteria | Bounded implementation responsibility | Direct check |
|---|---|---|
| AC-01, AC-09 | BPR-0690 typed identity/lifecycle policy; later composition/menu slice | Core deterministic identity/rename/activation/close tests; shipped profile menu course after composition |
| AC-02, AC-05, AC-09 | BPR-0691 bounded registry and Default adoption; later vault/root partition slice | Prior environment adoption twice and interrupted transaction; future/malformed refusal; fake-vault two-profile same-server isolation |
| AC-03, AC-08, AC-09 | Later shared profile runtime and per-window session extraction | Two composed windows share accepted preferences/data while selected PR, epoch and cursor stay independent; stale completions after sign-out |
| AC-04 | Later revision-aware shared draft/note slice | Expected revision race preserves accepted record and rejected candidate; existing publication authority checks remain required |
| AC-06 | Later runtime admission and consumer lifecycle slice | Repeated launch activates owner; explicit new-window creates; contention never starts competing owner; close one leaves others usable |
| AC-07 | Later bounded window-session persistence/restoration slice | Quit versus explicit close, restart, missing epoch/file, malformed optional record and explicit restoration error |
| AC-01, AC-02, AC-03, AC-06, AC-08, AC-09 | Final public multi-window composition | Two fixture profiles/accounts and two windows; Settings singleton, independent review positions, cross-profile sign-out isolation |

Only the first two foundation IDs are allocated now. Subsequent bounded cards
are allocated when their source contracts are established; BPR-0595 must acquire
each required dependency. Core-only and architecture checks accompany new core
boundaries. Native window behavior and subjective interaction remain explicit
native/attended evidence, not conclusions from offscreen unit tests.

BPR-0693 refines credential adoption: immutable Default keeps the legacy native
namespace/codec as canonical in place. Non-Default vault instances use new
versioned namespaces containing profile identity and reject all foreign profile
keys before native effects. Schema2 binding metadata scopes rows and permits
schema1 adoption only by Default. No secret-copy/promotion transaction is needed.

BPR-0711 adds native runtime exclusion after exact registry/root selection and
before queue loading or profile service construction. The startup host's effect
phase obtains the lease; composition transfers its lifetime to the profile owner,
ahead of worker/service members in destruction order. Registry bootstrap remains
a separately serialized global store operation and does not authorize a profile
runtime. Opening a not-yet-owned profile first admits its runtime, reloads the
global allocator, then requests the existing core window lifecycle. Closing or
failed-start entries cannot bypass admission merely because their map key remains.
Startup refusal consumes its callback exactly once, including shutdown reentry.

## The empty desktop and the chooser (RFC-BPR-0033)

This section records shipped behavior rather than planned ownership. Closing a
window no longer presents the management window. `profile_desktop`'s
last-runtime branch asks an `empty_desktop_policy` the composition injects, and
that function — not a platform test inside the desktop — decides between
staying resident and quitting; the outcome it last gave is observable, so both
answers are drivable on one host. `native_empty_desktop_policy` answers
`stay_resident` on macOS and `quit` everywhere else. A resident desktop keeps
its own menu bar while no window exists (File › Quit; Profiles listing the
profiles by name with the current one checked, then New Window and Manage
Profiles…), and application activation, New Window or choosing a profile
dispatches the existing open intent for the most recently used profile — the
last profile whose window was opened or focused in this process, which persists
nothing new. A quitting desktop uses the existing Quit path, so profile records
are saved exactly as an explicit Quit saves them; it retries while the registry
is still saving the close that emptied the desktop and forces shutdown only if
that request is persistently refused. The registry and scratch lease are
unchanged from [[BPR-0696]], whose last-window presentation this supersedes.

The restoration paths are distinct in the core rather than by convention:
`profile_window_catalog::close` permanently retires exactly that window from
automatic restoration, while `quit` retains every live window in focus order
before their runtime contexts are retired. A profile operation that fails while
a window exists is reported in that window's Profiles menu status item; the
chooser is presented for a failure only when no window exists to carry it,
which is the startup case RFC-BPR-0024 already describes.

The admission foundation's public process course checks the existing owner's
durable window rows after contention. BPR-0712 adds the lease-bound POSIX local
activation transport, carrying only stable profile identity and activate/new-window.
The owner's management host remains the sole window allocator. A sender has no
runtime or window; bounded queued/refused/indeterminate results never authorize
retry or lock stealing. Exact MRU reactivation, new-window admission, different
profiles, lost endpoints and owner death use independent real fixture processes.
Windows transport, oversized native socket addresses and packaged focus evidence
remain unavailable/unverified. The global registry's existing compare-and-set conflicts still require explicit Reload
Profiles before retry; this foundation does not add automatic cross-process
preference synchronization, two-account acceptance or native package evidence.
