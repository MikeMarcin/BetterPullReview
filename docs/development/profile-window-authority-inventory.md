# Profile and window authority inventory

Planning for RFC-BPR-0024, inspected at `e354b45e`; setup destination work
BPR-0689 is concurrent. This inventory records design and source ownership,
not implemented behavior. BPR-0595 is the acceptance ledger.

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
| Lifetime | Startup creates one application object; last-window shutdown terminates its services. There is no profile admission owner, stable window identity, or distinct close-versus-quit restoration path. |

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

The public process course uses two real fixture desktop processes and checks the
existing owner's durable window rows after a competing launch and a synchronous
Open retry. Cross-process activation/new-window transport is still absent. The
global registry's existing compare-and-set conflicts still require explicit Reload
Profiles before retry; this foundation does not add automatic cross-process
preference synchronization, two-account acceptance or native package evidence.
