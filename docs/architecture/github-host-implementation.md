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
    inactive GitHub CLI boundary, native HTTPS, response limits and redaction
        |
        +-- retained gh/QProcess scaffolding, not public-composed in milestone one
        |
        +-- production QNetworkAccessManager plus a platform credential vault
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
| Credentials | Native platform vault in milestone one; CLI storage only under a future enabled contract | None | None |
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

Provider failure mapping is total and intentionally bounded. A table-driven
application-host contract enumerates all 15 `github_provider_failure` values,
drives each through a pull-request completion, and requires the corresponding
typed core reason with no accepted host/core snapshot, projection payload, or
provider-owned detail. Process-startup and generic-unavailable failures both
map to the safe network reason; retry policy remains separately typed and
bounded.

The production provider, not the recorded one-page facade, owns continuation
exhaustion. REST pages follow only a validated `Link` continuation on the
selected authority; GraphQL connections advance only through nonempty,
changing `pageInfo.endCursor` values while `hasNextPage` is true. Page count,
per-page bytes and records, aggregate bytes and records, cursor size, expected
GitHub totals, repository identity, pull-request identity, and expected head
OID are checked before one snapshot is returned. A repeated or missing cursor,
unknown completeness, truncated count, authority drift, or failed page
rejects the complete read.

Rate-limit observations are resource-scoped. Core REST, Search, and GraphQL
may update only their corresponding `core`, `search`, and `graphql` buckets.
GitHub.com requires the applicable exact four-header tuple; GHES may omit an
entire tuple, but a partial, duplicate, malformed, unknown, or wrong-resource
tuple is invalid. The provider and host merge independent buckets across
pages and retries. A primary, secondary, or structured GraphQL `RATE_LIMITED`
terminal retains its exact causal record; a bounded `Retry-After` remains a
separate scheduling observation. Raw provider messages never cross inward.

## Transport ownership

The provider adapter validates the explicit host profile. Milestone-one native
profiles construct native HTTPS; CLI profiles return `cli_unavailable` before
transport construction.

### GitHub CLI broker

Native HTTPS is the sole milestone-one production authentication transport.
CLI profiles remain visible, but the public provider returns fixed typed
`cli_unavailable` before transport construction, CLI
executable/configuration discovery, or a process-port call. The broker uses a
canonical absolute `gh` executable through a designated `QProcess` runner
only as retained, tested scaffolding for a possible future transport. It
invokes argument arrays without a shell, supplies request bodies through
standard input, disables prompting and
incidental output, uses a canonical directory currently outside repository
ancestry, bounds all streams and duration, and redacts failures. It neither
asks for nor extracts a token.

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
approved authority policy, used the correlated credential identity, and was
not rerouted by ambient configuration. A redirect-requiring or otherwise
unverifiable contract, cross-authority redirect, certificate failure,
uncontrolled persisted transport setting, or unbound active-account
selection fails closed. A configured hostname is not by itself authority,
identity, redirect, or certificate evidence. If the installed CLI contract
cannot prove these properties, broker mode is unavailable.

BPR-0025 implements that boundary as two adapter-private, standard-C++ ports.
The wire port retains the complete parent request, profile revision, host,
operation, and page correlation on every success, typed failure, or
cancellation terminal. The lower process port receives only one validated
absolute executable, an element-wise argument vector, a freshly constructed
environment, a canonical working directory currently outside any worktree or
bare-repository ancestry, bounded standard input, and fixed stream/time/reap
limits. Its production runner is the only GitHub source that uses `QProcess`;
it returns bounded standard output only on a clean success and otherwise
discards every partial stream and returns a fixed failure category. This
directory validation does not prove owner-controlled permissions or bind the
directory identity through launch.

The runner owns one serialized worker and creates, uses, and destroys each
`QProcess` there. An absolute deadline covers queueing and is checked again
immediately before launch. Allocation-free shape validation and
pre-cancellation precede work-item allocation and request copying. On Unix the
runner requests closed inherited descriptors, disabled core dumps, reset
signal handlers, and `CreateNewSession`; the child is the new session's
initial process-group leader. A compiled child directly observes those four
facts. Compiled lifecycle cases prove cleanup on clean exit, failure, timeout,
and cancellation signals descendants that remain in that initial process
group and reaps the direct child; a separate escape case proves a descendant
that moves to another group can survive before the fixture removes it. A
compiled seam also proves completion-watchdog quarantine, zero launch while
quarantined, and recovery. Source and hostile architecture gates enforce
shutdown cleanup and process-group resolution and separately require an actual
bounded-reap failure to return fixed `reap_failure`, quarantine the slot, and
retain the live object for continued reaping on the same worker rather than
invoking Qt's destructor wait or creating another reaper. The compiled runtime
suite does not force that live-process failed-reap path.

Public reads use a fixed buffer and request no more than the remaining
accepted bound plus one byte. That bounds retained and adapter-accepted bytes,
not Qt's private in-flight pipe allocation, for which Qt exposes no cap. Raw
macOS inspection observes the requested environment plus only Darwin's
deterministic `__CF_USER_TEXT_ENCODING` addition. The Linux raw-runner fixture
requests its locale explicitly; that is not an extra production broker
variable. Windows Job Object descendant containment, private pipe bounds, and
target-native CLI runner behavior are deferred to RFC-BPR-0015 and are not
inferred from the Unix evidence. BPR-0036 retains the Windows native-HTTPS,
vault, and public-path checks.

The invocation compiler revalidates the complete canonical wire request. It
emits only `gh api`, one translated relative endpoint, explicit
`--hostname`/`--method` pairs, canonical noncredential headers, and
`--input -` for the selected GraphQL `POST`. GitHub.com REST drops its leading
slash, GHES REST drops the already-validated `/api/v3/` prefix, and both
GraphQL contracts become the CLI's literal `graphql` endpoint. Absolute URLs,
repository placeholders, file expansion, caller-supplied flags, status or
debug output, response caching, pagination, templating, fields, previews, and
credential-bearing arguments are not representable.

The current direct audit used the official macOS arm64 `gh` 2.96.0 release
dated 2026-07-02. Its downloaded ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`,
and its extracted binary SHA-256 was
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`.
An owner-private copy produced one fixed bad-certificate TLS alert and no HTTP
request for a self-signed loopback certificate. A controlled cross-authority
`302` received the exact 47-byte request body and synthetic authorization at
its selected origin, then reached the distinct sink without either.
Equivalent `307` and `308` POST probes reached their selected origins but did
not produce an observed sink request; that absence remains unproven rather
than accepted as redirect rejection. The installed `gh` 2.95.0 binary,
SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun with the same material outcomes. Each private executable digest was
checked again after its probes.

The observed `302` is sufficient to falsify exact-authority enforcement. The
CLI's ordinary success output provides neither a stable redirect chain nor
the effective final authority, and the available debug/status surfaces are
outside the broker allowlist. The direct loopback probe uses a full URL only
to select a disposable local fixture from an owner-private canonical
non-repository directory.
Because `gh` rejects an ephemeral `host:port` as `--hostname`, the probe keeps
the synthetic `github.com` startup hostname and never treats the loopback run
as exact production-authority proof. Source inspection confirms that relative
production endpoints use the same default Go redirect client. A
verification-only owner-private `GH_CONFIG_DIR` holds the random synthetic
token solely to pass the CLI startup authentication gate. That override is
absent from production.

Omitting that override does not eliminate ambient configuration.
`http_unix_socket` is an active CLI transport setting, not a telemetry-only
fact: for both selected binaries, the isolated probe loaded the setting and
the exact `gh api` invocation made one authenticated HTTP request through an
AF_UNIX sentinel with the expected synthetic Authorization value. The
production broker would need the selected platform home or application-data
paths for CLI account state. Canonical `SSL_CERT_FILE`/`SSL_CERT_DIR` paths
likewise do not bind selected trust-material identity or contents through the
CLI's later open.

The current `gh api` command exposes no per-invocation `--user`.
`gh auth token --user` prints the token and is prohibited;
`gh auth switch --user` mutates persisted active-account state and is racy
with BetterPullReview's correlated identity generation. Current selected
source resolves `ActiveToken` per round trip, falls back from a keyring error
to a blank-host token, and uses a macOS keyring path that launches
`/usr/bin/security`.

Consequently the retained CLI transport exposes eight typed reasons:
milestone-one product-scope deferral, executable-image identity,
cross-authority redirect behavior, ambient configuration, credential/account
identity, working-directory identity, descendant-process containment, and
numeric PID/process-group identity reuse. It returns `cli_unavailable` before
starting a child; the public composition rejects CLI earlier and makes zero
process launches. A fake process cannot create trust, and native HTTPS
evidence cannot be borrowed. RFC-BPR-0015 owns any positive CLI enablement and
must bind or eliminate every blocker without a privileged helper, weakened
security, or GitHub Actions.

### Native HTTPS

BPR-0026 implements the adapter-private `github_wire_transport_port` with one
dedicated `QThread`, event loop, and `QNetworkAccessManager` per transport
instance. The manager and every request/reply object stay on that worker.
`perform` admits one operation at a time, revalidates the complete correlation
and canonical wire request before network work, and returns only the original
correlation plus a bounded success, typed error, or cancellation. BPR-0027
adds pagination, normalization, host staging, and the application-facing
provider path. BPR-0029 composes that provider and transport in the public
executable for explicit GitHub.com and GHES profiles.

The production transport asks `QNetworkProxyFactory::systemProxyForQuery` for
each initial or redirected URL. It accepts at most 16 returned candidates,
retains supported direct, HTTP, and SOCKS5 candidates in system order, and
tries the next candidate only after a retryable connection or proxy failure.
Proxy-authentication failure is retryable to a later system candidate; origin
authentication, TLS, HTTP, malformed-response, and policy failures are not.
Each attempt clears manager-local access and connection caches and selects
only that candidate; no process-global proxy is mutated. On generic Unix,
Qt's system policy may use libproxy/desktop sources or lowercase
`http_proxy`, `https_proxy`, and `no_proxy`. Production code does not call
`getenv`, but “system proxy” is not a claim of environment independence.

System/PAC proxy discovery is synchronous inside Qt. System-root enumeration
and request configuration are also synchronous. The absolute 30-second
deadline is checked before and after those calls and bounds subsequent Qt
transfer/event-loop work, but it cannot preempt a blocked discovery call.
Prompt cancellation sets a stop flag and queues an abort on the worker, with
a five-millisecond in-loop poll as a backstop; completion can still be delayed
until synchronous discovery returns. Shutdown can wait for the same worker.

Every hop receives a fresh request with manual redirect policy, no cookie,
cache, or authentication reuse, HTTP/2 disabled, `Accept-Encoding: identity`,
and HTTP/1.1 ALPN. TLS configuration starts from an explicit copy of the
operating-system CA set, removes local identity, session-ticket, backend, and
pre-shared-key state, selects `VerifyPeer`, and permits TLS 1.2 or later.
Certificate errors and pre-shared-key challenges abort; there is no
certificate-error bypass or process-global TLS mutation. A terminal requires
the `QNetworkReply::encrypted` completed-initial-handshake signal, a non-null
peer certificate, no accepted `sslErrors`, the configured peer-verification
and TLS-1.2-or-later policy, no HTTP/2 use, and the exact requested reply URL.
Qt's scheme-associated
`ConnectionEncryptedAttribute` is an additional terminal consistency check,
not by itself proof of a negotiated encrypted session. The meaningful
handshake/trust evidence is the completed-handshake signal and peer
certificate under the enforced verification policy, together with the
trusted/untrusted fixture outcomes. Qt's public reply surface used here does
not directly record the negotiated TLS version, so the implementation does
not claim that stronger fact.

Qt automatic redirects are disabled. The transport permits at most three
manual redirects, only for REST `GET`, only to a canonical query-free HTTPS
path on the exact validated REST authority and within its API namespace.
GraphQL redirects, cross-authority targets, loops, malformed targets, and a
fourth redirect return typed `redirect`. A newly validated URL, proxy
candidate list, TLS configuration, request, and Authorization header are
created for each accepted hop.

Adapter-visible bounds are fixed: 256 KiB per request body, 4 MiB per response
body, 64 response headers, 64 KiB of aggregate response-header text, 8 KiB per
header field, 16 system proxy candidates, three redirects, and one
30-second absolute operation deadline. Synchronous discovery may extend
wall-clock completion because it cannot be preempted; the request uses only
the remaining deadline after discovery returns. Reads use fixed 16 KiB chunks
and request no more than the remaining response limit plus one byte.
Successful responses must be HTTP 200 JSON with no content encoding, a
consistent content length, and bounded pagination metadata. Authentication,
permission, network, API-version, rate-limit, certificate, redirect, timeout,
cancellation, malformed-response, response-limit, and unavailable outcomes
remain typed and redact remote detail.

Those are adapter acceptance and retention bounds, not exact bounds on Qt's
private HTTP parser, socket buffers, or other in-flight allocations. Qt can
parse or retain bytes before exposing metadata through `QNetworkReply`, and it
may normalize or collapse duplicate headers, notably `Location`, before raw
header pairs and redirect attributes become visible. The adapter enforces the
metadata Qt exposes, not an exact count of on-wire Location fields. Qt also
does not provide a wipe API for the copies of an Authorization header held by
`QNetworkRequest`, `QNetworkAccessManager`, TLS, or socket internals. The
transport overwrites its explicit mutable Authorization buffer and the
credential owner overwrites its storage, but exact erasure of Qt-owned copies
is not claimed.

Bootstrap material is bound to one exact session context and may authorize
only one `GetViewerIdentity` operation; once claimed, it is destroyed after
that terminal path and cannot be replayed. An account credential must match
the exact profile, profile revision, host identity, canonical hostname,
account, and server contract, and each request must match its application
session and identity generation. Mismatches fail before network execution.

The target-native test seam exists only when `BPR_BUILD_TESTS=ON`. It keeps
the production request, TLS, redirect, cancellation, response, and
credential-binding path but substitutes an ordered loopback proxy list and
may add one disposable fixture CA to the system roots. The 15-case
TLS-over-CONNECT fixture covers success, ordered proxy fallback, accepted
same-authority REST redirect, rejected cross-authority and GraphQL redirects,
untrusted certificate, response/header limits, in-flight and pre-start
cancellation, timeout, HTTP 401/403/429 classification, and content-encoding
rejection. Runtime facts separately report whether Qt supports TLS, its active
TLS backend category, the first raw system-proxy candidate category, and raw
candidate count; they do not contain credentials, proxy addresses, CA
contents, or remote data, and they do not prove every candidate is reachable.

On the directly exercised macOS arm64 development host, the complete 15-case
fixture passed. Runtime observation reported Qt Network 6.11.1 with the
OpenSSL TLS backend, TLS supported, and one direct system-proxy candidate.
A separate `BPR_BUILD_TESTS=OFF` build/dependency audit found no test-only
factory symbols, retained the adapter's direct Security and CoreFoundation
dependencies, and observed the current ad-hoc linker signature. These are
development build and runtime facts, not package signing or distribution
certification.

Direct Linux evidence used an Ubuntu 26.04 x86_64 full-system QEMU guest with
Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. The guest inputs were frozen to
SHA-256
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`
for `native_https_transport.cpp` and
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`
for the Python harness. The contract and 15-case fixture passed 2/2 in
0.12/11.29 seconds, and the two architecture gates passed 2/2 in
5.00/144.35 seconds. The final hostile suite covered all 92 mutations.
The uninstrumented focused suite passed repeatedly. Runtime
observation reported
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
The ELF directly requires Qt6Network and Qt6Core. A controlled trace of only
the trusted-success case under `strace` observed the Qt OpenSSL backend plugin,
`libssl.so.3`, and `libcrypto.so.3` being opened. A full fixture-suite
`strace` is deliberately not evidence because tracing perturbs cancellation
timing; behavioral evidence comes from the uninstrumented passes, while the
single-success trace is dependency evidence only. Source and the generated
build contained no workflow files or GitHub Actions references.

Windows 10/11 x64 execution and dependency inspection remain BPR-0036 work;
that platform-specific evidence is nonblocking for the other implementation,
test, documentation, security-review, simplification, and later-RFC work. It
blocks only affected Windows claims and final RFC-BPR-0002 Auto-Verified
closeout.

Native credentials are consumed inside the credential-bearing transport.
They never enter process arguments, environment, logs, diagnostics,
persistence, core events, application projections, or a wrong-authority wire
request. The transport necessarily places Authorization in transient
`QNetworkRequest`/network-backend storage; those Qt-held copies cannot be
precisely wiped. Locked, unavailable, access-denied, corrupt, and
credential-deletion failure states are typed failures. They never cause
plaintext, settings, environment, or SQLite fallback storage.

The portable application contract keys every native credential by profile,
profile revision, host identity, canonical hostname, authenticated account,
and server contract. Every store, retrieve, delete, cancellation, and terminal
result also carries the request, application session, and identity generation.
Login text, repository content, redirects, capability revisions, and
caller-provided provider facts cannot form credential authority.

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

### macOS Keychain selection

BPR-0022's macOS adapter is an explicit file-based Keychain
implementation. It uses the modern `SecItem` operations with a generic
password item, but deliberately does not set
`kSecUseDataProtectionKeychain`, `kSecAttrSynchronizable`, an access group, or
an accessibility class. This is a scoped choice for the current ad hoc
linker-signed, unbundled development executable, which has no team identity,
bound `Info.plist`, or provisioning profile. It is not a claim that a future
signed application bundle should remain on the legacy implementation.

File-based Keychain searches normally cover every keychain in the process
search list even though additions go to the default keychain. The adapter
therefore resolves the exact default `SecKeychainRef`, uses it as
`kSecUseKeychain` for additions, and supplies a singleton
`kSecMatchSearchList` for retrieval, update, and deletion. A matching
service/account item in another keychain cannot be retrieved, replaced, or
deleted by this adapter.

The production service is fixed as
`org.betterpullreview.github-native-credential.file-keychain.v2`; native
tests use the separate
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v2`
service and cannot touch production items. The account attribute is 574
lowercase hexadecimal characters: four fixed-width 64-bit fields for profile,
profile revision, host, and account; an explicit server-contract discriminator
(`01` for GitHub.com and `02` for GHES 3.18); one byte of hostname length; and
253 fixed hostname-byte slots encoded as canonical ASCII bytes or `00`
padding. The discriminators do not depend on C++ enum ordinals. The key
contains no login, API URL, repository, redirect, provider response, or
credential material.
Every query matches the generic-password class, exact service, exact account,
and exact default keychain and requests failure instead of authentication UI.

Store is an in-place upsert: add first, update only on duplicate, and retry add
once only if that update reports absence. It never deletes an old item before
replacement. Retrieve asks for one data result; delete reports deletion or
prior absence distinctly. Native status values are mapped to the portable
locked, unavailable, access-denied, corrupt, operation-failed,
allocation-failure, or invalid-request categories without retaining a raw
status number or operating-system error string.

The adapter queues exact portable operation contexts through an injected host
effect executor. Synchronous `SecItem` work never runs on the serialized
decision executor or a GUI lane. Cancellation can win only before native
execution starts; once a synchronous `SecItem` operation begins, its terminal
result wins. A terminal produced after native execution arrives on the effect
lane. Immediate rejection and explicit or destructor cancellation may complete
synchronously on the originating lane. Every callback runs outside adapter
locks, and a consuming application host must marshal every typed completion
onto the serialized decision lane. Destruction atomically claims and cancels
all still-queued work and destroys any owned store or retrieved material.

There is no runtime probe, migration, or fallback between the file-based and
data-protection Keychains. A future data-protection selection requires a
signed app-like bundle, explicit entitlement/access-group policy, migration
and rollback behavior, and target-native package evidence under
RFC-BPR-0014. Until that work is complete, the two stores must never be
silently mixed.

### Windows Credential Manager selection

BPR-0023's Windows adapter selects the wide-character Win32 Credential
Manager API and one `CRED_TYPE_GENERIC` credential with
`CRED_PERSIST_LOCAL_MACHINE`. The production target prefix is fixed as
`org.betterpullreview.github-native-credential.windows-credential-manager.v2/`;
native tests use the disjoint
`org.betterpullreview.github-native-credential.windows-credential-manager.integration-test.v2/`
prefix. The same 574-character fixed-width lowercase hexadecimal authority
encoding used by the macOS adapter follows the prefix. Production and test
targets are therefore 650 and 667 UTF-16 code units respectively.

The target prefix is an application namespace, not an operating-system access
control list. Generic credentials are available to another process running as
the same user if that process knows the target. `CRED_PERSIST_LOCAL_MACHINE`
means that the credential remains available to later logon sessions of that
same user on the same computer. It does not make the credential machine-wide,
hardware-backed, application-isolated, or roaming.

The portable 2,560-byte limit exactly matches the documented Windows 10/11
generic-credential bound of `5 * 512` bytes. The adapter names that supported
platform contract directly because some MinGW headers retain the obsolete
512-byte macro. It does not reduce the portable contract according to the
compiler header. Returned native blobs are accepted and overwritten before
`CredFree` only when both their pointer and their extent satisfy the stable
bound; malformed or oversized native results fail as corrupt without
dereferencing an untrusted extent.

Store constructs one zero-initialized `CREDENTIALW` and makes one
`CredWriteW(..., 0)` create-or-replace call. It never deletes the old
credential before replacement. Retrieve and delete use exact-target
`CredReadW` and `CredDeleteW` calls, and native ownership is released only
through `CredFree`. `GetLastError` is captured immediately after a failed
call. Documented absence, unavailable-session, denial, corrupt-data,
allocation, and invalid-request statuses map to portable outcomes; all
unselected statuses become `operation_failed`. Credential Manager documents
no read/write/delete status that honestly means a locked vault, so this
adapter does not invent a native `locked` mapping.

Synchronous Credential Manager calls use the host effect executor with the
same queued-work, cancellation, callback, and atomic teardown rules as the
macOS adapter. The source has no Credential UI, enumeration, Credential
Locker, DPAPI, registry, file, settings, SQLite, environment, process,
network, logging, or alternate-vault path.

The adapter, fake-backed contract suite, native fixture, hostile source
boundaries, and x64 PE cross-compile are prepared. None is a substitute for
running the checked-in `windows-dev` build and disposable lifecycle on a
native Windows x64 environment. BPR-0023 closes the implementation slice;
BPR-0036 keeps the native claim open until that target-native build,
execution, import inspection, and exact cleanup pass.

### Linux Secret Service selection

BPR-0024's Linux adapter is selected only when `CMAKE_SYSTEM_NAME` is
`Linux`. That target requires and links the stable
`libsecret-1>=0.20.0` interface; the Qt-free target graph does not discover
or link libsecret.

The production schema is fixed as
`org.betterpullreview.github-native-credential.secret-service.v2`; native
tests use the disjoint
`org.betterpullreview.github-native-credential.secret-service.integration-test.v2`
schema. Each schema declares only one string attribute, `authority`, whose
value is the same 574-character fixed-width lowercase hexadecimal encoding
used by the other native adapters. A matching item must have exactly that
authority and the `xdg:schema` value added by libsecret. Multiple matches,
additional attributes, a schema mismatch, or an invalid value is corrupt.

The v1 and v2 namespaces are deliberately disjoint, and the adapters do not
read, enumerate, migrate, or fall back to v1. The pre-release v1 production
vaults were never composed into the public executable. Every directly
executed v1 integration fixture completed exact cleanup; the Windows v1
fixture remained preparatory and created no target-native item. This
intentional format break prevents an old hostname-free key from aliasing a
hostname-bound authority.

The adapter opens a derived `SecretService` with the literal
`org.freedesktop.secrets` argument and `SECRET_SERVICE_NONE`, then checks
`g_dbus_proxy_get_name()` for that exact value before establishing a Secret
Service session or resolving a collection or item. Only a canonical proxy may
continue through `secret_service_ensure_session_sync()`. This second check is
mandatory because the selected libsecret can be redirected at the dependency
layer. A redirected proxy fails as typed `unavailable`; production code never
reads, copies, changes, or removes a service-routing environment variable.

Only the default collection alias is admitted. The adapter does not unlock
the collection, create a collection, search an alternate vault, or silently
start an interactive authentication flow. Its derived `SecretService`
overrides synchronous, asynchronous, and finish prompt vfuncs. If libsecret
hands it a prompt, the adapter issues exactly one `Dismiss` D-Bus call with
no auto-start and a positive one-second bound, then returns a locked failure.
It never calls `Prompt`.

Store creates a binary `SecretValue` with
`application/octet-stream` and uses one
`SECRET_ITEM_CREATE_REPLACE` operation in the default collection. Retrieve
searches all exact-schema matches without loading or unlocking them, requires
zero or one exact item, checks its lock state, and then loads that item
explicitly. It accepts only `application/octet-stream` or exact `text/plain`.
The latter is narrowly admitted because gnome-keyring 50 through libsecret
0.21.7 was directly observed changing the content-type metadata to
`text/plain` while retaining arbitrary opaque bytes, including a complete
2,560-byte replacement. Delete acts on the same one exact item.

Secret Service, D-Bus, and I/O statuses map conservatively to the portable
locked, unavailable, access-denied, corrupt, invalid-request, allocation, or
operation-failed categories. No raw status text crosses inward. Synchronous
libsecret work is serialized by the backend and dispatched through the
injected host effect executor. Queuing, cancellation, completion, callback,
and teardown follow the same portable ownership rules as the other native
adapters. There is no prompt display, collection creation, unlock, alternate
vault, plaintext, settings, SQLite, environment, file, process, network,
logging, or UI fallback.

Direct Linux vault evidence passed in an Ubuntu 26.04 x86_64 full-system QEMU
VM with an emulated CPU, Linux 7.0, Qt 6.10.2, libsecret 0.21.7, and
gnome-keyring 50. A dedicated real user ran the fake contract and the
resource-locked setup/lifecycle/cleanup fixture under Xvfb plus a desktop
D-Bus Secret Service session. The four focused CTest cases passed, including
73-byte storage, 2,560-byte replacement, retrieval, deletion, repeated
absence, and canary probes over arguments, environment, fixed output,
executable, source, and isolated scratch. A second lifecycle round-tripped
opaque values containing NUL, high-bit, and invalid UTF-8 bytes at the same
73-byte and 2,560-byte sizes. Checked-in setup and cleanup proved the exact
fixture authority absent; a separate controlled D-Bus audit proved the entire
integration schema absent. Other controlled runs proved hostile
`SECRET_SERVICE_BUS_NAME` rejection, a truly locked default collection, and
no-session-bus unavailability. A live hostile D-Bus owner observed only
proxy-property inspection and no `OpenSession`, collection, or
credential-item method before rejection. Fake-backed effects and direct
native-status classification separately cover access-denied and failed-delete
terminals without claiming those as desktop-policy fixtures.

That full-system VM result proves the selected Linux vault behavior against a
real Linux kernel and user-space API stack, but it is not bare-metal evidence.
The vault fixture itself does not verify HTTPS. BPR-0026 separately verified
the Linux internal native HTTPS wire path in the same class of full-system
Ubuntu 26.04 QEMU environment: all 15 TLS/proxy/redirect/limit/cancellation
scenarios passed, runtime selected the OpenSSL backend and one direct proxy
candidate, and ELF plus a controlled single-success trace selected Qt6Network,
Qt6Core, the Qt OpenSSL backend plugin, `libssl.so.3`, and `libcrypto.so.3`.
That separate BPR-0026 evidence is not by itself composed-provider evidence.
BPR-0029 now supplies the public executable trace on macOS and in an Ubuntu
26.04 x86_64 full-system QEMU guest. The current Linux v2 run passed the
Secret Service setup/lifecycle/cleanup cases 3/3, independent exact cleanup
and hostile/unavailable/locked cases, both exact-host rendered public
fixtures, fresh 81/81 tests-on and 36/36 tests-off Release build targets,
fixture-seam absence, and dependency/runtime/ELF audits. Windows target-native
evidence remains in BPR-0036. Mock transports, cross-compilation, and
vault-only evidence cannot substitute for a retained platform claim.

## Read-only enforcement

The RFC-BPR-0002 composition exposes provider reads only. Its verification
transport inventory is backed by a rejecting no-mutation sink:

- recorded methods must parse as exact uppercase `GET` or `POST`, with no
  fallback for any other value;
- REST catalog operations must use typed `GET`;
- GraphQL `POST` bodies and the capability-probe document share one bounded
  full-document scanner and must contain exactly one explicit `query` plus
  bounded fragments;
- every top-level `mutation` or `subscription`, including one after a valid
  query, is rejected, as are multiple operations, malformed scanned
  structure or token framing, and byte, nesting, or definition-limit
  exhaustion;
- comments, strings, block strings, field names, operation names, endpoint
  names, and fixture labels are never treated as operation-type evidence.

The scanner is a bounded structural classifier, not a complete GraphQL grammar
parser. The outbound read contract also requires exact equality with the
canonical compiled request body before an operation can execute. Every
legacy/current GraphQL shape has a direct case where the scanner approves an
added read selection but canonical equality rejects the changed body.

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

The production application host constructs and owns an initially empty
capability cache. Composition cannot inject a populated cache, and a caller's
syntactically valid capability facts are not observations. Only the exact
authenticated provider probe can stage an entry; the core's accepted-session
delta precedes cache commit.

Native credential resume accepts only an opaque profile identity. A
noncopyable host-owned binding catalog selects the complete
profile/profile-revision/host/hostname/account/server authority previously
established by strict credential-authenticated `/user`. Login text, profile
text, settings, SQLite, and caller-provided account values cannot select a
vault item. Switching identity exact-deletes the prior retained authority
before the next session begins; authoritative absence forgets the stale
binding; failed cleanup retains an exact catalog tombstone for remediation.

This catalog is process-local. Sharing it across host instances proves
same-process recreation only. The public composition selects
`delete_retained_on_shutdown`; its exact vault delete terminal is part of host
quiescence, and provider/vault workers are not drained until that terminal has
returned. A delete failure is reported and leaves the exact catalog tombstone.
There is no serialized or integrity-backed selector, enumeration, v1 fallback,
or process-restart recovery. An abrupt process or host crash can therefore
leave a native-vault item that BetterPullReview cannot discover or resume.

## Application projection and Qt rules

The application host publishes an envelope containing semantic core deltas and
optional immutable host-owned read projections. Qt never receives a provider
or credential object.

The public presentation boundary is
`bpr/application/github_projection.hpp`. Its intent callback admits only
profile activation, pull-request open, and pull-request refresh intents; it
cannot construct provider completion events. The update envelope strips core
host requests and accepted-completion internals before delivery. The host
projects accepted session and pull-request records into provider-free value
types before publishing them atomically with the core transition that accepted
them.

Composition supplies Qt with immutable non-secret profile choices derived
from the same validated profile configuration: opaque profile identity,
display label, canonical hostname, product kind, and selected transport.
Profile selection is the only identity-authority input. Login and account
identity remain read-only output of the authenticated `/user` observation and
cannot select a credential or vault binding.

Native-HTTPS activation obtains credential text through a Qt-owned
password-echo input with character and UTF-8 byte bounds. Composition sees
only the move-only sensitive buffer, immediately converts it to the
application credential-material owner, and resets the temporary on every
terminal path. The prompt is presentation behavior; it does not perform a
vault, provider, filesystem, process, or network effect.

The RFC-BPR-0002 presentation exposes:

- active hostname, authenticated account, and selected transport;
- profile selection/configuration controls;
- repository and pull-request locator input;
- Open and Refresh `QAction` state from command projections;
- explicit loading, authentication, permission, network, API-version,
  capability, rate-limit, malformed, oversized, canceled, and stale states;
- normalized PR metadata and read-only conversations.

The BPR-0031 presentation arranges connection and locator controls into
compact responsive rows. At narrow widths the locator actions move below
their inputs and the description/conversation splitter changes from
horizontal to vertical. Host, transport, account, pull-request identity, and
review content remain primary; capability/rate observations and exact commit
identities are reachable through separate Connection Details and Technical
Details disclosures.

Presentation state is explicit: `disconnected`, `connecting`, `empty`,
`loading`, `error`, `cancelled`, `loaded`, `refreshing`, `loaded-error`, and
`loaded-cancelled`. The last three nonterminal/failure variants retain
accepted content. A new-target load does not imply that prior
identity-sensitive content is current, while refresh failure or cancellation
does not discard an accepted projection.

Conversation models map stable GitHub identities to rows. Accepted refreshes
produce insert, remove, move, and data-change deltas for affected records.
They do not call `beginResetModel()` for a bounded comment, capability, status,
or selection change. A custom delegate wraps remote literal text without
elision while leaving native selection and focus painting to the active
style. The first accepted projection may establish an initial expansion;
subsequent accepted refreshes preserve Qt-local expansion and restore the
current selection when its stable identity remains. Presentation owns
wording, focus, accessibility, layout, and safe plain-text rendering.

The normalized presentation contains separate stable identities for reviews,
review threads, nested review comments, and pull-request comments. Qt derives
incremental model operations from those identities; it does not receive
provider records or row indexes from the host. A new pull-request open clears
the prior metadata and conversation projection immediately, while a refresh
keeps the last accepted projection through loading, failure, cancellation, or
stale completion.

The window inherits the application and operating-system palette, style, and
font. It does not force a palette or style, apply QSS, or install a replacement
application font. Qt application palette, font, style, and theme events refresh
the derived presentation fonts, native icons, delegate geometry, and
responsive layout without changing semantic state. Each published
status/diagnostic presentation update emits at most one combined polite
accessible announcement, and identical announcement text is suppressed.

Direct macOS evidence records system-default Dark startup under QMacStyle and
a safe loaded-window application-local Dark-to-Light plus 13-to-17-point font
exercise. The latter proves that the live event path responds; it is not
evidence of a real operating-system-global appearance toggle. Deterministic
offscreen forced-style renders remain layout and scaling evidence only.
The test-enabled real-application fixture includes a fail-closed native
appearance probe for the remaining gate. It reports readiness only after a
system-Light Cocoa/QMacStyle startup has loaded the public GitHub.com fixture,
selected and collapsed a stable conversation, focused the conversation view,
and retained an in-memory initial render. Qt's color-scheme signal alone is not
accepted because an application-local override can emit it. On macOS the probe
also reads the current-user global appearance domain through a read-only,
test-only CoreFoundation seam and requires an independent Light-to-Dark global
transition. Only after the Dark palette has settled with the same platform,
style, selection, focus, collapse state, and model identity does it retain the
second render. The captures must have the same geometry and device-pixel ratio
but different pixels. The probe then creates two fresh non-aliased PNGs from
those exact captures and reloads both before reporting success; failed probes
leave no render artifacts. The probe observes appearance and never changes the
operating-system setting. The global preference key is evidence plumbing, not
production appearance logic. Incomplete arguments, a non-GitHub.com fixture,
aliased or existing paths, offscreen execution, the wrong startup scheme, a
missing signal or global transition, identical pixels, or state drift discard
the claim and fail.
User-selectable System, Light, and Dark profiles remain RFC-BPR-0006 work.

## Composition and lifecycle

The composition root is the only scope that knows all concrete pieces. Its
production profile set is explicit:

- built-in GitHub.com native HTTPS;
- built-in GitHub.com CLI, visibly labeled unavailable;
- optional GHES 3.18 native HTTPS from an explicitly supplied canonical
  hostname;
- optional GHES 3.18 CLI, visibly labeled unavailable.

The provider owns immutable copies of every validated profile and routes by
exact profile identity. GitHub.com profiles have built-in provenance; GHES
profiles have explicit-user provenance. CLI activation reaches the same
semantic host path but returns `cli_unavailable` before the process port,
which keeps the unavailable state visible and produces zero child launches.
The tests-disabled composition contract also rejects CLI runner-factory
symbols in the public application, while its self-test selects the built-in
GitHub.com CLI profile and requires the bounded unavailable presentation.
Provider and transport counter tests independently require zero factory,
effect, and process-port calls. Optional GHES CLI selection follows the same
source path but is not claimed as a separate end-to-end self-test case.

The visible ownership order is:

```text
validated non-secret profile store
provider and vault effect executors
platform credential vault
native network transport
normalized GitHub provider
decision executor
Qt window and models
host-owned credential binding catalog
application host
```

Provider and vault effects use separate worker executors. The Qt event-loop
decision executor is a required live service through host quiescence. A
successful post is retained exactly once for eventual non-inline execution.
Provider and vault submissions that return normally produce exactly one
terminal; cancellation is advisory and does not suppress it. Composition
keeps every referenced port, projection, decision lane, and binding catalog
alive until the host reports quiescence.

Projection delivery is synchronous but observational. An exception sets a
diagnostic latch and cannot veto a core transition, effect, or cleanup.
Reentrant public ingress is deferred into FIFO generations; only the
generation present at turn entry runs in that turn, preventing projection
feedback from starving queued work. New ingress cannot overtake an older
continuation, and shutdown closes ingress with terminal priority.

Destruction reverses ownership. Public shutdown first closes ingress,
advances or invalidates the active generation, cancels provider work, requires
an exact delete terminal for the retained v2 credential authority, and
discards staged snapshots. Only after host quiescence does composition stop
the provider, close and join both workers, and permit Qt projections,
transports, vault handles, the binding catalog, and the decision lane to be
destroyed. A failed exact credential deletion is surfaced and leaves the
catalog tombstone; it is not silently treated as successful cleanup.

Recorded GitHub.com and GHES fixtures may replace the transport behind the same
provider port in tests. They must use the same provider, host, core, Qt, and
composition path as the real application; a direct normalizer helper or static
mockup is not public-path evidence.

The fixture transport, fixture vault, fixture command-line choices, native
appearance-probe choices and evidence, and fixture evidence types are compiled
only into test-enabled targets. The public application header is
test-definition-free. A clean
`BPR_BUILD_TESTS=OFF` Release configure/build inspects compile commands, the
Ninja graph, symbols, and raw binary bytes to prove that the fixture
translation unit, test macros, paths, arguments, labels, native appearance
markers, and evidence strings are absent.

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

Within RFC-BPR-0002, the Linux Secret Service vault lifecycle, the inactive
CLI broker's process/invocation boundary, and the native HTTPS internal wire
path on macOS and Linux are directly verified. Positive CLI enablement is
outside milestone-one scope under RFC-BPR-0015; the public profile is visibly
unavailable and launches zero processes.
BPR-0028 verifies the safe Qt projection and presentation. BPR-0029 composes
the explicit profiles, native HTTPS, platform vault, provider, host, core, and
Qt path and exact-deletes retained credentials during orderly shutdown; it
makes no crash/restart recovery claim. Its current-tree Linux v2 execution
passed in an Ubuntu 26.04 x86_64 full-system QEMU guest with exact shutdown
cleanup and tests-disabled Release selection/seam evidence. Retained Windows
target-native credential/HTTPS/public-fixture evidence remains BPR-0036 work.
Query-bearing REST redirects remain rejected by the closed BPR-0038 design
decision; any future relaxation requires new direct public-contract evidence
and separately scoped work. The open Windows execution is nonblocking for all
other slices and later-RFC work; it blocks only its affected platform claim
and final RFC-BPR-0002 Auto-Verified closeout.
