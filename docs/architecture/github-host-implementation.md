# RFC-BPR-0002 GitHub Host Implementation Architecture

This document specializes the [GitHub provider
contract](github-provider-contract.md) for the RFC-BPR-0002 implementation.
The governing rule remains:

> **The core decides. The host performs. Qt presents.**

RFC-BPR-0002 adds a read-only GitHub.com and GitHub Enterprise Server 3.18
vertical slice. It does not add a second source of shared truth, a GitHub
mutation path, or a widget-to-provider shortcut.

## Selected boundary

```text
Qt presentation adapter
    host/profile controls, PR locator input, QActions, metadata,
    conversation model, errors, focus, accessibility
        |
        | typed semantic intent
        v
application host
    bounded observations, provider session, staged normalized snapshots,
    capability/cache authority, cancellation, lifecycle
        |
        | deterministic transition
        v
functional core
    active opaque identity, command legality, request/generation identity,
    completion acceptance, semantic deltas and diagnostics
        |
        | typed semantic request
        v
application host
        |
        | narrow github_provider_port
        v
GitHub adapter
    explicit profiles, REST/GraphQL request contracts, normalization,
    GitHub CLI broker, native HTTPS, response limits and redaction
        |
        +-- installed gh through a designated QProcess runner
        |
        +-- QNetworkAccessManager plus a platform credential vault
```

Provider, transport, process, network, vault, JSON, and Qt types do not enter
the functional core. `QProcess` and `QNetworkAccessManager` are permitted only
inside their designated external adapters. Qt Widgets presentation code
receives application projections and has no provider capability.

## Authority allocation

| State | Authority | Core representation | Qt representation |
|---|---|---|---|
| Host profile configuration | Application host profile registry | Opaque `host_profile_id` | Selectable non-secret profile facts |
| Authenticated account | Selected provider session | Opaque host/account identity and generation | Visible hostname and account login |
| Credentials | GitHub CLI storage or native platform vault | None | None |
| Capability detail and evidence | Host capability cache | Compact tri-state capability set and revision | Supported, unsupported, or unknown presentation |
| Shared PR and conversation state | GitHub, observed through the provider session | Stable identity, accepted snapshot identity, bounded summary | Immutable metadata and conversation projections |
| Normalized response cache | Application host | Snapshot identity only | Incremental rows/cards derived from accepted snapshots |
| Command legality and stale-result policy | Functional core | Interaction state and typed transition policy | `QAction` projections |
| Focus, input text, geometry, and row indexes | Qt presentation adapter | None unless semantically required | Native Qt state |

An accepted normalized snapshot is a cache of a GitHub observation. It is
never a competing shared authority. Refresh replaces or repairs it from
GitHub; local disagreement cannot overwrite GitHub facts.

## Identity model

The functional core uses opaque, strongly typed identities:

```text
host_profile_id
git_host_id
github_account_id
repository_id
pull_request_id
provider_snapshot_id
capability_revision
request_id
application_session_id
request_generation
```

Host profile configuration contains hostname, product kind, REST and GraphQL
roots, authentication transport, trust/proxy mode, REST API version, and a
profile revision. Those operational facts remain in the host and provider
adapter. The core sees only the profile identity and normalized observations
required for deterministic policy.

Session establishment occurs before the provider has proven the account
identity, so it uses:

```cpp
struct github_session_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    host_profile_id Profile;
    request_generation Generation;
};
```

A pull-request request carries every identity already known at submission:

```cpp
struct pull_request_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    host_profile_id Profile;
    git_host_id Host;
    github_account_id Account;
    repository_locator Repository;
    pull_request_number Number;
    request_generation Generation;
};
```

The completion adds canonical repository, pull-request, and snapshot
identities. Acceptance compares the entire applicable context, not only the
request number. Host/profile/account switching increments the generation,
clears active shared projections, and makes every earlier completion stale in
constant time.

## Validated host authority

A host-profile candidate becomes usable only through the GitHub adapter's
validated-profile type. The candidate must have trusted `explicit_user`,
`builtin`, or `managed` provenance, nonzero opaque profile/host identity and revision,
system proxy/trust modes, a selected authentication transport, and one of the
two enum-backed server contracts. Pull-request/provider content and redirects
are never profile provenance.

The initial contracts admit only canonical lowercase ASCII FQDNs, with DNS
label and total-length bounds and no IP literal, custom port, trailing dot,
userinfo, IDNA alias, URL syntax, query, or fragment. Their roots are exact:

| Contract | Hostname | REST root | GraphQL root |
|---|---|---|---|
| GitHub.com | `github.com` | `https://api.github.com` | `https://api.github.com/graphql` |
| GHES 3.18 | configured FQDN | `https://HOST/api/v3` | `https://HOST/api/graphql` |

Both select REST version `2022-11-28`. Product kind, routes, roots, and
version are derived from the validated contract rather than accepted again
as request strings. A redirect is only a target-validation input: it cannot
create or mutate a profile. GraphQL redirects are rejected; a REST redirect
may proceed only when it remains HTTPS, on the exact REST authority, and
inside the selected API namespace.

Profiles contain no login, account, capability, or credential material.
Authenticated account identity is a later `/user` observation, and
capabilities are separately observed and cached facts.

## Capability model

Every probed capability has three states:

```text
Unknown
Supported
Unsupported
```

`Unknown` means the selected session has not produced sufficient evidence.
It is not treated as supported, and it is not mislabeled as unsupported.
RFC-BPR-0002 records at least:

- review-thread reads;
- viewed-file mutation support;
- resolve-thread support;
- unresolve-thread support.

The latter mutation capabilities are observed for future command legality but
are not exposed as executable mutations in RFC-BPR-0002. A capability snapshot
is keyed by profile revision, host, authenticated account, product/server
facts, and probe-contract revision. The core receives the compact tri-state
set plus `capability_revision`; the host retains detailed probe evidence.

The adapter sends one statically selected, bounded GraphQL introspection
`query` to the exact validated GraphQL root. It inspects the mutation root and
the pull-request, changed-file, review-thread, viewed-state enum, and four
mutation input/payload contracts without executing a mutation. Names and type
signatures are case-sensitive and order-independent; duplicate names,
partial aliases, GraphQL errors, malformed wrappers, or a limit breach do not
form evidence.

`Supported` means a complete authenticated schema observation matches the
recorded call contract. It does not assert that the current account may
mutate every target. `Unsupported` is emitted only by a complete observation
that conclusively lacks or incompatibly changes a required fact.
Authentication, permission, network, API-version, rate-limit, certificate,
redirect, timeout, cancellation, malformed, partial, and oversized results
remain explicitly unavailable with all four core states `Unknown`.

Review-thread support is a dependency of resolve and unresolve support.
Viewed-file mutation support requires both mark and unmark contracts.
Per-thread `viewerCanResolve`/`viewerCanUnresolve` and per-file
`DISMISSED`/`UNVIEWED`/`VIEWED` state are normalized target facts, not host
capability overrides. GitHub exposes no equivalent read-only per-file
mutation-permission fact, so mutation authorization remains fail-closed until
RFC-BPR-0008 exercises a designated safe target.

The application cache accepts only four canonical, conclusive recorded or
observed facts. Its exact key is profile, profile revision, host,
authenticated account, server contract, and probe-contract revision. It uses
an injected monotonic logical epoch rather than a clock, generates a new
nonzero capability revision for every successful re-probe, reuses that
revision on a hit, expires at a half-open boundary, never caches unavailable
results, never evicts fresh authority, and exposes exact/profile/identity
invalidation. Capacity and freshness are bounded (defaults 16 and 300 epochs;
ceilings 64 and 900).

## Typed vocabulary

### Intents

```text
activate_host_profile_intent(profile)
open_pull_request_intent(repository locator, PR number)
refresh_pull_request_intent
```

Profile creation or editing is a typed application-host settings operation.
It validates and persists non-secret operational configuration before the core
is offered an opaque profile identity. The core does not validate URLs or
handle credential material.

### Bounded observations

```text
current_github_identity_observation
current_host_capabilities_observation
current_pull_request_summary_observation
```

Observations carry their complete source identity and revision. They expose
facts, not provider, cache, process, network, or vault objects.

### Host requests

```text
establish_github_session_request
fetch_pull_request_request
invalidate_github_identity_request
```

`fetch_pull_request_request` is semantic. It does not contain a URL, REST
route, GraphQL text, `gh` argument, HTTP header, or credential.

### Completion events

```text
github_session_ready_event
github_session_failed_event
pull_request_loaded_event
pull_request_load_failed_event
request_cancelled_event
```

A loaded event carries correlation and normalized summary facts plus a
`provider_snapshot_id`; the full normalized snapshot remains host-owned.

### Deltas

```text
github_identity_delta
capability_delta
accepted_snapshot_delta
pull_request_summary_delta
command_delta
status_delta
diagnostic_delta
```

`accepted_snapshot_delta` is the only authority for committing a staged
provider result to the active host cache. A changed-domain flag by itself is
not sufficient.

## Session, open, and refresh flows

### Establish a session

```text
Qt profile selection
    -> activate_host_profile_intent
    -> core clears the prior active identity, advances generation,
       disables dependent commands, and emits:
           invalidate_github_identity_request
           establish_github_session_request
    -> host cancels/supersedes old work and calls github_provider_port
    -> provider resolves viewer identity and probes capabilities
    -> immutable completion returns to the serialized decision lane
    -> github_session_ready_event
    -> core validates request/application/profile/generation
    -> identity, capability, command, and status deltas
    -> host publishes bounded projections
```

Authentication, permission, trust, network, API-version, rate-limit, malformed
response, and output-limit failures return as typed failure events. Raw
provider text is never a core diagnostic or UI string.

### Open a pull request

```text
Qt Open QAction
    -> open_pull_request_intent(repository locator, PR number)
    -> core validates semantic readiness from bounded identity/capability facts
    -> fetch_pull_request_request with complete correlation context
    -> host calls the selected normalized provider
    -> provider performs REST-first reads and narrow GraphQL thread reads
    -> provider returns an immutable bounded normalized snapshot
    -> host posts the completion to the serialized decision lane
    -> host stages the snapshot by request context
    -> pull_request_loaded_event(snapshot identity and summary)
    -> core either:
         accepts and emits accepted_snapshot_delta, or
         rejects as stale and emits no acceptance delta
    -> host commits and publishes only an explicitly accepted snapshot
```

### Refresh

Refresh submits the active immutable pull-request key under a new request
generation. The old accepted snapshot may remain visibly marked as refreshing,
but an incomplete, failed, canceled, or stale replacement never partially
changes it. A successful accepted snapshot is compared by stable identity and
published as bounded metadata and conversation changes.

## Staged snapshot acceptance

Provider callbacks never mutate an active cache, core state, Qt model, or
widget. The callback captures an immutable normalized result and posts it to
the decision executor.

On the serialized lane the host:

1. stages the result under its exact request context;
2. dispatches the corresponding typed event to the core;
3. inspects the transition for an exact `accepted_snapshot_delta`;
4. commits the matching staged snapshot to the active cache;
5. derives and publishes bounded application projections;
6. discards every unaccepted or superseded staged result.

This order is mandatory. Committing before the core decision would allow a
late result to become an alternate authority even if Qt had not displayed it.
Publishing merely because `changed_domain` contains a broad content bit would
make acceptance ambiguous and is therefore prohibited.

## Normalized provider boundary

The application-facing `github_provider_port` accepts semantic session and
fetch requests and completes with normalized records or typed failures. Its
read model includes bounded forms of:

- viewer identity;
- repository identity;
- pull-request identity and metadata;
- base and head commit identities;
- reviews;
- review threads and comments;
- issue comments;
- files, commits, and checks required by the provider contract;
- capability and rate-limit observations.

Provider-library and transport types stop at the adapter:

```text
not allowed inward:
    QString / QJsonValue / QJsonDocument
    QNetworkRequest / QNetworkReply
    QProcess / process exit records
    raw REST or GraphQL response records
    Authorization headers or tokens

allowed inward:
    standard/domain C++ normalized records
    stable identities and revisions
    bounded plain text
    typed failure categories
    redacted metadata-only operational trace facts
```

Normalization validates count, byte, nesting, page, string, and allocation
limits before a snapshot can be staged. Failure is atomic: a rejected response
does not partially update the accepted snapshot.

## Transport ownership

The provider adapter selects one transport from the explicit host profile.

### GitHub CLI broker

The broker uses a canonical absolute `gh` executable through a designated
`QProcess` runner. It invokes argument arrays without a shell, supplies
request bodies through standard input, disables prompting and incidental
output, uses a neutral non-repository working directory, bounds all streams and
duration, and redacts failures. It neither asks for nor extracts a token.

Arguments come from an operation-specific allowlist. The child environment is
constructed from an explicit minimal allowlist rather than inherited and then
scrubbed. It contains only the platform facts required for the CLI's own
credential store and the selected trust/proxy policy plus forced noninteractive
settings. Token, repository/host override, debug, config-directory override,
pager, editor/browser, color, TTY, update, telemetry, spinner, and prompt
influences are absent without reading or logging their values. Contract tests
inject a previously unknown canary environment name/value and prove that it is
not inherited.

The configured hostname is explicit. A broker result may be normalized only
when the selected adapter can establish that the request stayed within its
approved authority policy. A redirect-requiring or otherwise unverifiable
contract, cross-authority redirect, or certificate failure fails closed; a
configured hostname is not by itself redirect or certificate evidence. If the
installed CLI contract cannot prove this behavior, the broker mode is
unavailable and RFC-BPR-0002 closeout remains blocked.

### Native HTTPS

The native transport uses `QNetworkAccessManager`, operating-system
trust/proxy behavior, and the selected platform vault. Redirects are handled
under an HTTPS and exact-authority allowlist. Certificate failures abort the
request; there is no call or option that ignores TLS errors.

Native credentials are consumed inside the credential-bearing transport and
are never placed in SQLite, settings, environment files, process arguments,
logs, diagnostics, core events, application projections, or Qt objects.
Locked, unavailable, access-denied, corrupt, and credential-deletion failure
states are typed failures. They never cause plaintext, settings, environment,
or SQLite fallback storage.

The portable application contract keys every native credential by profile,
profile revision, host, authenticated account, and server contract. Every
store, retrieve, delete, cancellation, and terminal result also carries the
request, application session, and identity generation. Login text, repository
content, redirects, capability revisions, and caller-provided provider facts
cannot form credential authority.

Credential material is a bounded nonempty byte owner rather than a string.
It is noncopyable, move-safe, exposes only a read-only byte span, overwrites
owned storage before release, and has no comparison, hashing, formatting,
streaming, or serialization surface. Store takes ownership immediately and
acts as an atomic upsert. Retrieve distinguishes material from absence;
delete distinguishes deletion from prior absence; every operation has typed
failure and cancellation alternatives. A winning cancellation suppresses
secret-bearing success and destroys transient material, and teardown does the
same for pending work.

The contract does not itself discover an account or prove a native vault.
Production enrollment must bind a validated profile to the authenticated
account observed through `/user`, then hand the move-only material directly
to the selected platform adapter. Its caller remains responsible for
overwriting any mutable input buffer used to construct the owned material.
Fake-vault tests prove portable replacement, isolation, result, and
cancellation semantics only; BPR-0022, BPR-0023, and BPR-0024 retain the
target-native lifecycle gates.

The native Windows and Linux matrices exercise HTTPS TLS validation, system
proxy selection, cross-authority redirect rejection, certificate rejection,
runtime network/process scope, and binary dependencies in addition to the
platform vault lifecycle. Mock transports, cross-compilation, and macOS
evidence cannot substitute for those target-native claims.

## Read-only enforcement

The RFC-BPR-0002 composition exposes provider reads only. Its verification
transport inventory is backed by a rejecting no-mutation sink:

- REST operations are classified by method and route;
- GraphQL documents are parsed for their selected operation type;
- every GraphQL `mutation` is rejected regardless of operation name;
- a query name, endpoint name, or fixture label is never treated as proof that
  an operation is read-only.

Mutation capability probing may observe schema support without executing the
mutation. Executable GitHub mutations remain RFC-BPR-0008.

## Cache and invalidation rules

Every identity-sensitive cache key includes:

```text
profile revision
+ host identity
+ authenticated account identity
+ repository identity or unresolved locator
+ pull-request identity/number
+ capability or provider-contract revision
```

Selecting a different profile or resolving a different authenticated account:

- increments the core identity generation;
- disables old-identity commands immediately;
- cancels or suppresses pending provider work;
- discards staged results;
- removes active metadata and conversation projections;
- invalidates capability, normalized response, rate-limit, and retry state for
  the old active session;
- prevents reuse solely because repository text or PR number matches.

Shared records are refreshed from GitHub. A cache hit can satisfy a bounded
observation only when its entire authority key and freshness policy match.

## Application projection and Qt rules

The application host publishes an envelope containing semantic core deltas and
optional immutable host-owned read projections. Qt never receives a provider
or credential object.

The RFC-BPR-0002 presentation exposes:

- active hostname, authenticated account, and selected transport;
- profile selection/configuration controls;
- repository and pull-request locator input;
- Open and Refresh `QAction` state from command projections;
- explicit loading, authentication, permission, network, API-version,
  capability, rate-limit, malformed, oversized, canceled, and stale states;
- normalized PR metadata and read-only conversations.

Conversation models map stable GitHub identities to rows. Accepted refreshes
produce insert, remove, move, and data-change deltas for affected records.
They do not call `beginResetModel()` for a bounded comment, capability, status,
or selection change. Presentation owns wording, focus, accessibility, layout,
and safe plain-text rendering.

## Composition and lifecycle

The composition root is the only scope that knows all concrete pieces. Its
visible ownership order is:

```text
non-secret profile store
platform credential vault
process runner and native network transport
normalized GitHub provider
host snapshot/capability caches
decision executor
Qt window and models
application host
```

Destruction reverses that order. Shutdown first closes ingress, advances or
invalidates the active generation, cancels provider work, disables completion
publication, and discards staged snapshots. Only then are Qt projections,
transports, vault handles, and caches destroyed.

Recorded GitHub.com and GHES fixtures may replace the transport behind the same
provider port in tests. They must use the same provider, host, core, Qt, and
composition path as the real application; a direct normalizer helper or static
mockup is not public-path evidence.

## Build and dependency allocation

The selected dependency direction is:

```text
better_pull_review_core
    standard/domain C++ only

better_pull_review_application
    core + normalized GitHub port/read models

better_pull_review_adapter_github
    application + Qt Core/Network
    owns provider contracts, QProcess broker, HTTPS, and vault bindings

better_pull_review_adapter_qt
    application-facing contracts + Qt Core/Gui/Widgets
    no provider/process/network/vault dependency

better_pull_review_composition
    wires concrete adapters; owns no hidden service locator
```

The Qt-free preset compiles and links neither effect-backed provider
implementations nor presentation targets and continues to configure, build,
and test without Qt or provider packages. Boundary checks must forbid
provider/process/network/vault types from core, forbid effects from Qt
presentation, and allow effect APIs only in their designated external adapter
sources.

GitHub Actions has zero role in configuring, building, testing, packaging,
releasing, or verifying this feature. Evidence comes from local CMake/CTest,
recorded contracts, real-process inspection, and target-native vault/network
tests.

Build and packaging records still report the actual Qt Network and
platform-vault compile, link, and runtime dependencies selected by the
provider. RFC-BPR-0014 owns delivered-package signing and distribution
certification; it does not defer accurate RFC-BPR-0002 dependency accounting.

## No-impact and deferred regions

RFC-BPR-0002 has no implementation impact on:

- Git object caching and review epochs;
- diff computation, line mapping, and syntax highlighting;
- the private SQLite review ledger;
- GitHub review/comment/viewed/resolve mutation execution;
- queue, observer, AI, and notification workflows;
- QML, WebEngine, browser, listener, plugin, telemetry, or updater surfaces;
- package formats, signing, notarization, or installer behavior.

Git cache and epoch work remains RFC-BPR-0003. GitHub mutations remain
RFC-BPR-0008. Delivered-package certification remains RFC-BPR-0014.

Within RFC-BPR-0002, the CLI broker, native HTTPS transport, macOS Keychain,
Windows Credential Manager, Linux Secret Service, Qt flow, and target-native
credential evidence are later issue slices, not deferred acceptance criteria.
RFC-BPR-0002 cannot close while any of them remains unverified.
