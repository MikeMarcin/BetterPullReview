# GitHub Provider Contract

The concrete RFC-BPR-0002 flow and lifecycle are defined in [GitHub Host
Implementation Architecture](github-host-implementation.md).

## Architectural allocation

- The functional core receives normalized host/account/capability observations, decides semantic command legality and request identity, and accepts or rejects completion events.
- The application host owns the narrow provider capability port, selected
  session policy, staged-snapshot acceptance protocol, cancellation, cache
  invalidation, and serialized completion delivery.
- The GitHub adapter implements profile validation, normalization,
  REST/GraphQL contracts, CLI and native HTTPS transports, credentials,
  proxy/trust enforcement, bounded retry/rate-limit handling, and future
  mutation execution.
- The Qt presentation adapter owns host/account controls, authentication/error presentation, and command projections; it never receives credentials or calls the provider directly.

For read completions, the host stages a bounded immutable normalized snapshot,
the core accepts or rejects its complete identity context, and only an exact
`accepted_snapshot_delta` permits cache commit and Qt publication.

## Host support

- GitHub.com
- GitHub Enterprise Server 3.18 and later, subject to capability probing

Host profiles are explicit and never inferred from untrusted content. Each profile stores hostname, product kind, API roots, authentication mode, proxy/trust behavior, REST version, and a profile revision. Capabilities are separately probed observations keyed by profile revision, host, account, server/contract facts, and probe-contract revision; capability revision is generated for the accepted cache value.

The selected v1 profile validator admits canonical lowercase ASCII FQDNs
only. GitHub.com is exactly `github.com` with REST
`https://api.github.com` and GraphQL
`https://api.github.com/graphql`; GHES 3.18 is exactly
`https://HOST/api/v3` and `https://HOST/api/graphql`. Both use REST version
`2022-11-28`. Profiles derived from provider content or redirects, hostname
aliases, IP literals, ports, non-HTTPS roots, and root drift fail before a
request can be compiled. GraphQL redirects are forbidden, and REST redirects
remain on the exact validated authority and API namespace.

The functional core sees an opaque profile identity rather than hostname, URL,
proxy, trust, or authentication configuration. The active host and account are
always visible to the user, while credential material is never projected.

## API strategy

REST is the baseline for pull-request metadata, commits, files, reviews, comments, checks, search, and notifications. GraphQL is used narrowly for review-thread grouping, file viewed state, thread resolution, and other features whose REST representation is insufficient.

GHES requests pin the supported REST API version selected by the host adapter. The initial floor is `2022-11-28` for GHES 3.18. Capability probing and contract fixtures remain authoritative over naming assumptions.

Each capability is `Unknown`, `Supported`, or `Unsupported`. Lack of probe
evidence produces `Unknown`; it does not silently enable a feature or claim
that the server does not support it.

One bounded, static GraphQL introspection query observes the exact
review-thread, viewed-state, mark/unmark, and resolve/unresolve schema
contracts. It never executes a mutation. Only a complete authenticated
schema observation may produce `Supported` or `Unsupported`; every
operational failure, GraphQL error, partial response, malformed shape, or
limit breach produces explicit all-`Unknown` unavailability. Schema support
is distinct from target authorization: normalized
`viewerCanResolve`/`viewerCanUnresolve` flags and viewed-file state describe
the selected thread/file, while actual mutation permission remains
fail-closed until RFC-BPR-0008.

## Authentication transports

### GitHub CLI broker

- Invoke an explicitly resolved `gh` executable.
- Never request or print the token.
- Pass request bodies through standard input.
- Disable interactive prompts for application jobs.
- Preserve the agent or GitHub CLI's own credential storage boundary.
- Use an argument allowlist, a canonical working directory currently outside
  repository ancestry, and no shell.
- Construct an explicit minimal environment allowlist rather than inheriting
  the parent environment. Token, repository/host override, debug, pager,
  editor/browser, color, TTY, update, telemetry, spinner, and prompt
  influences are not admitted and their values are never read or logged.
- Inject an unknown canary environment variable during contract tests and
  prove it is not inherited by the child.
- Bound input, output, error, duration, and pagination; fail atomically when a
  limit is reached.
- Accept a result only when the broker's effective authority, active
  credential identity, ambient configuration, and redirect/certificate
  behavior have been proven to satisfy the selected profile and correlated
  identity generation. If the selected `gh` contract cannot prove those
  requirements, the broker path remains unavailable and RFC closeout is
  blocked.

BPR-0025 implements the closed invocation compiler, Qt-free injected process
port, bounded sole-`QProcess` runner, correlation-preserving wire terminal,
and fixed unavailability gate. The compiler translates the already-validated
GitHub.com/GHES REST and GraphQL routes into the exact relative form expected
by `gh api`; callers cannot contribute arguments or environment names.

The runner owns one serialized process worker. On Unix it requests a new
session, closed inherited descriptors, disabled core dumps, and reset signal
handlers; the child is the new session's initial process-group leader.
Compiled child observation proves those flag/session/group facts. Compiled
lifecycle cases prove clean exit, failure, timeout, and cancellation signal
descendants that remain in that initial process group while the direct child
is reaped; an escape case proves a descendant that moves to another process
group survives before explicit fixture cleanup. A compiled seam proves
completion-watchdog quarantine and recovery. Source and hostile architecture
gates additionally enforce shutdown and group-resolution paths and the rule
that an actual bounded-reap failure retains the live `QProcess` on the
quarantined worker for continued reaping; the runtime suite does not force
that live-process failed-reap path.
Allocation-free request-shape validation and pre-cancellation occur before the
worker allocates or copies a request.
Fixed-size reads bound bytes accepted and retained by the adapter, but Qt
exposes no hard cap for its private in-flight pipe allocation. Raw macOS
inspection admits only Darwin's deterministic `__CF_USER_TEXT_ENCODING`
addition to the requested environment. The Linux raw-runner fixture requests
its locale explicitly; this is a fixture fact, not an additional production
broker variable. Windows Job Object containment, private pipe bounds, and
target-native behavior remain BPR-0036 work.

Canonical/currently non-repository working-directory validation does not
establish owner-controlled permissions or bind directory identity through
launch. BPR-0037 owns that validation-to-launch race together with Unix
process-group escape and PID/process-group identity reuse across cleanup
signals before production CLI enablement. Initial-group signaling is not
claimed race-free.

An owner-private, digest-stable copy of the selected `gh` 2.95.0 bytes
produced a fixed bad-certificate TLS alert before HTTP but followed a
cross-authority `302` loopback redirect. The selected origin received the
synthetic authorization and exact body; the distinct sink received neither.
POST `307` and `308` sink absence is recorded as unproven rather than
rejection. The ephemeral loopback authority cannot be supplied as the CLI
startup hostname, so this source-backed characterization is not exact
production-authority proof. Ordinary CLI output cannot prove the final
authority.

Three independent executable, configuration, and identity gaps also prevent
enablement. The trust probe binds its observations to exact copied bytes, but
production currently validates only a canonical path and executable name; it
cannot carry that digest identity through a path replacement or update race to
the image actually launched.
Official CLI configuration permits a persisted `http_unix_socket` value, and
the production environment must supply the selected platform home or
application-data facts used by the CLI's credential/account state. Merely
omitting `GH_CONFIG_DIR` therefore does not exclude ambient transport
redirection. Canonical `SSL_CERT_FILE`/`SSL_CERT_DIR` paths also do not bind
the selected trust-material identity or contents through the CLI's later
open, leaving replacement/content mutation unresolved. In addition,
`gh api --hostname` selects the account currently active for that host and
exposes no per-invocation account selector.
`gh auth switch` can change that selection independently of the request's
already-established identity generation, so the child cannot prove it used
the correlated credential identity.

Production CLI reads expose all four typed unavailability reasons and return
`cli_unavailable` without starting a process. The synthetic authentication
config used by the isolated trust probe is verification-only; token variables
and every other credential-bearing environment surface are absent from the
production broker. A future CLI path must eliminate or directly bind every
one of these gaps; satisfying only redirect behavior is insufficient.
BPR-0037 owns that production-enablement work.

### Native HTTPS

BPR-0026 implements the internal wire port with one dedicated Qt worker,
event loop, and `QNetworkAccessManager` per transport. The Qt objects do not
cross the adapter, and one transport rejects a second concurrent operation.
The complete wire request and correlation are revalidated before the worker
uses a credential or performs network work. This surface is not yet the
application-facing provider: BPR-0027 still owns pagination, normalization,
body-aware failure interpretation, staged snapshot delivery, and public
composition.

Production uses the system CA set and
`QNetworkProxyFactory::systemProxyForQuery` for every hop. At most 16 proxy
candidates are admitted; supported direct, HTTP, and SOCKS5 entries preserve
system order, with fallback only after retryable proxy/connection failure.
Unsupported or empty candidate sets fail closed. A manager-local proxy is
selected for each attempt, with no global proxy mutation. Qt's generic Unix
system-proxy implementation can use libproxy/desktop sources or lowercase
`http_proxy`, `https_proxy`, and `no_proxy`. Production code itself does not
call `getenv`, so this contract means the selected platform/Qt system policy,
not environment independence. Proxy-authentication failure may try a later
candidate; origin authentication, TLS, HTTP, malformed-response, and policy
failures may not.

TLS is configured for `VerifyPeer`, the explicit operating-system root set,
TLS 1.2 or later, and HTTP/1.1 ALPN, without client identity, session reuse,
HTTP/2, or a certificate-error bypass. `sslErrors` aborts the hop. A terminal
requires the `QNetworkReply::encrypted` completed-initial-handshake signal, a
non-null peer certificate, no accepted `sslErrors`, the exact requested reply
URL, no HTTP/2 use, and the configured verification and minimum-protocol
policy. Qt's scheme-associated
`ConnectionEncryptedAttribute` is checked only as an additional consistency
fact, not as standalone negotiated-session proof. The meaningful evidence is
the completed-handshake signal and peer certificate under enforced
`VerifyPeer`/TLS-1.2-or-later policy, together with the trusted/untrusted
fixture outcomes; the transport does not directly record the negotiated TLS
version.

Qt automatic redirects are disabled. At most three manual redirects may
follow a REST `GET`, and each target must be query-free HTTPS on the exact
validated REST authority and inside its selected API namespace. GraphQL,
cross-authority, looping, malformed, and over-limit redirects fail as typed
`redirect`. Every accepted hop reconstructs its URL, proxy selection, TLS
configuration, request, and Authorization header.

The fixed adapter bounds are 256 KiB per request body, 4 MiB per response
body, 64 response headers, 64 KiB aggregate response-header text, 8 KiB per
header field, 16 proxy candidates, three redirects, and a 30-second absolute
operation deadline. Synchronous discovery may extend wall-clock completion;
only the remaining deadline applies when it returns. Successful pages must be
HTTP 200 JSON with no content encoding, a consistent content length, and
bounded Link metadata.

HTTP and transport outcomes map to typed, redacted failures. HTTP 403 is a
rate limit only with zero remaining or valid bounded `Retry-After`; HTTP 429
is rate limited. GraphQL HTTP-200 error bodies and other body-defined
secondary-rate-limit responses require BPR-0027's body-aware host path.

System/PAC discovery and system-root enumeration are synchronous Qt calls.
Cancellation queues a worker abort and the event loop polls the stop token,
but neither prompt cancellation nor the 30-second deadline can preempt a
blocked discovery call. The deadline begins when `perform` admits the
operation, is rechecked after synchronous calls, and only its remainder is
applied once control returns.

Likewise, the public limits cap bytes accepted and retained by this adapter,
not Qt's private HTTP parser, socket buffers, or pre-metadata allocations.
`QNetworkReply` may normalize or collapse duplicate headers, especially
`Location`, before exposing raw header pairs and redirect attributes. The
adapter enforces the metadata Qt exposes, not an exact count of on-wire
Location fields.

The explicit mutable Authorization buffer and credential owner are
overwritten, but Qt may hold internal Authorization copies for which it
exposes no exact wipe operation.

Bootstrap material is exact-session-bound, can authorize only one viewer
identity request, and is destroyed once claimed. Stored-account material is
accepted only for the exact profile, profile revision, host, account, server
contract, application session, and identity generation. Store credentials
only in Windows Credential Manager, macOS Keychain, or Linux Secret Service.
Never store tokens in SQLite, settings, environment files, logs, crash
reports, or command lines. Locked, unavailable, access-denied, corrupt, and
failed-delete states remain typed; there is no plaintext, settings,
environment, or SQLite fallback.

The test-enabled build exposes an adapter-private loopback proxy and
single-fixture-CA seam; `BPR_BUILD_TESTS=OFF` omits those factories and
symbols. A disposable TLS-over-CONNECT harness exercises 15 cases: success,
ordered proxy fallback, same-authority redirect, cross-authority redirect,
GraphQL redirect, untrusted certificate, response and header limits,
in-flight cancellation, timeout, pre-cancellation, HTTP 401/403/429, and
content-encoding rejection. It verifies the exact CONNECT authority, SNI,
Host, request target, method/body, and Authorization placement without
contacting GitHub. Runtime facts report TLS support/backend and the first raw
system-proxy kind/count without addresses or secret material; they do not
prove that every returned candidate is reachable.

The 15-case fixture passed on macOS arm64 with Qt Network 6.11.1 and the
OpenSSL runtime backend; the observed system-proxy result was one direct
candidate. A test-disabled production build contained no test-only native
HTTPS factory symbols and retained the expected Security/CoreFoundation
adapter dependencies and current ad-hoc linker signature. This is
development build/runtime evidence, not delivered-package certification.

Linux target-native verification passed in an Ubuntu 26.04 x86_64 full-system
QEMU guest with Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. The frozen
`native_https_transport.cpp` SHA-256 was
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`;
the frozen Python harness SHA-256 was
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The focused contract and complete 15-case fixture passed 2/2 in
0.12/11.29 seconds, and both architecture tests passed 2/2 in
5.00/144.35 seconds with all 92 hostile mutations. The uninstrumented
focused suite passed repeatedly. Runtime
facts were
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
The test ELF directly needs Qt6Network and Qt6Core; a controlled trace of the
single trusted-success case under `strace` opened
`qt6/plugins/tls/libqopensslbackend.so`, `libssl.so.3`, and `libcrypto.so.3`.
A full-suite `strace` is not functional evidence because tracing perturbs
cancellation timing; dependency tracing was intentionally restricted to one
success, while behavior comes from the repeated uninstrumented suite.
Neither source nor generated build contained workflow files or GitHub Actions
references.

Windows execution remains BPR-0036 work and does not block other
RFC-BPR-0002 implementation slices; it blocks only affected Windows claims
and final RFC closeout.

The portable vault capability accepts only an exact profile/profile-revision/
host/account/server-contract authority and exact request/session/identity
generation. Its credential owner is noncopyable, caps nonempty material at
2,560 bytes so every supported native vault can represent it, exposes only
read-only bytes, and securely overwrites storage before release. Store is an
ownership-transferring atomic upsert; retrieve, absence, delete, failure, and
cancellation are distinct typed outcomes. No raw operating-system error text
or credential bytes enter provider errors or projections.

On macOS, the BPR-0022 adapter explicitly selects the file-based Keychain
implementation for the current ad hoc linker-signed, unbundled executable,
which has no team identity, bound `Info.plist`, or provisioning profile. It
scopes every generic-password operation to the exact default keychain, fixed
v1 production service, and 66-character fixed-width hexadecimal encoding of
the complete portable authority; stable explicit bytes distinguish the
GitHub.com and GHES contracts without serializing a C++ enum ordinal. Requests
fail rather than display authentication UI. The dedicated native-test service
is distinct from the production service. The adapter does not set the
data-protection, synchronization, access-group, or accessibility attributes,
and it never probes or falls back between Keychain implementations. A future
signed data-protection Keychain migration is package lifecycle work under
RFC-BPR-0014, not an alternate path in this adapter.

Synchronous Security framework calls run only through a host effect executor,
never through the serialized decision executor or a GUI lane. Native-operation
terminals arrive on the effect lane, while immediate rejection and explicit
or destructor cancellation may complete synchronously on their originating
lane. A consuming application host must marshal every typed terminal onto its
decision lane.

On Windows, the BPR-0023 adapter selects only wide-character generic
Credential Manager operations. A fixed production target prefix and a
disjoint integration-test prefix precede the same complete 66-character
authority encoding. Store is one `CredWriteW` create-or-replace operation;
retrieve and delete are exact-target `CredReadW` and `CredDeleteW`
operations. Native results are bounded to the documented Windows 10/11
2,560-byte limit, wiped only over a structurally valid extent, and released
with `CredFree`. The adapter neither deletes before replacement nor selects
Credential UI, enumeration, Credential Locker, DPAPI, registry, settings,
SQLite, environment, file, or another storage implementation.

Generic Credential Manager storage is scoped to the current user rather than
to the BetterPullReview executable. Its target prefix is a namespace, not an
ACL. `CRED_PERSIST_LOCAL_MACHINE` retains the item for that same user's later
logons on the same computer; it is not machine-wide, hardware-backed, or
roaming. No documented Credential Manager CRUD status is mapped to `locked`.
The prepared fake and cross-compile evidence does not verify native behavior;
BPR-0023 closes only the implementation slice. BPR-0036 retains the direct
Windows x64 lifecycle, cleanup, and runtime claim as open work.

On Linux, the BPR-0024 adapter is compiled only for the Linux target and
requires stable `libsecret-1>=0.20.0`. It opens a derived `SecretService` with
the literal canonical `org.freedesktop.secrets` argument and
`SECRET_SERVICE_NONE`, validates that the resulting D-Bus proxy has that exact
name, and only then establishes a Secret Service session. Dependency-level
service redirection therefore fails as typed `unavailable` before session or
item work; production code neither reads nor mutates routing environment. A
live hostile owner observed only proxy-property inspection and no
`OpenSession`, collection, or credential-item method. The derived service
denies all prompt vfuncs. When libsecret supplies a prompt, the adapter makes
one bounded, no-auto-start `Dismiss` call and never displays a prompt, unlocks
a collection, or creates a collection.

The Linux adapter uses only the default collection. Its exact disjoint v1
production and integration-test schemas each have one explicit `authority`
attribute containing the complete 66-character portable authority; libsecret
adds the exact `xdg:schema` attribute. Store uses atomic replacement and
`application/octet-stream`. Retrieve accepts only that type or exact
`text/plain`: gnome-keyring 50 with libsecret 0.21.7 was directly observed
normalizing the stored content-type metadata to `text/plain` while preserving
the opaque bytes. Locked and unavailable service, access denial, corrupt
items, invalid requests, allocation failure, ordinary operation failure,
absence, replacement, deletion, and cancellation remain typed. Synchronous
libsecret operations run only through the host effect executor.

The Linux vault contract, resource-locked native fixture, and cleanup passed
in an Ubuntu 26.04 x86_64 full-system QEMU VM with an emulated CPU, Linux 7.0,
Qt 6.10.2, libsecret 0.21.7, and gnome-keyring 50. The fixture ran under Xvfb,
a desktop D-Bus Secret Service session, and a dedicated real user. It proved
73-byte storage followed by 2,560-byte replacement, retrieval, deletion, and
absence for both runtime canaries and opaque values containing NUL, high-bit,
and invalid UTF-8 bytes. Checked-in cleanup proved the exact fixture authority
absent, and a separate D-Bus audit proved integration-schema absence. Bounded
runtime-surface probes, hostile service-name rejection, a truly locked default
collection, and no-session-bus unavailability also passed. Access-denied and
failed-delete terminals are covered by injected effects plus direct
native-status classification rather than an operating-system policy fixture.
This is direct Linux vault evidence from a full-system VM; it is not
bare-metal evidence. BPR-0026 separately verified the internal Linux native
HTTPS wire surface with the 15-case loopback fixture, runtime observation,
ELF dependencies, and a controlled single-success dependency trace. Neither
the vault evidence nor the internal wire evidence proves the public
composition surface.

Windows target-native evidence remains open under BPR-0036 and is nonblocking
for implementation on available platforms. The public provider path remains
BPR-0027 regardless of target. A mock, cross-compile, vault-only run,
macOS-only run, or internal wire fixture cannot prove that composed path or
the retained Windows claim.

## Normalized application boundary

The application-facing `github_provider_port` accepts semantic session and
read requests and returns standard/domain C++ normalized records or typed
failures. It does not expose:

- `QString`, `QJsonValue`, or other JSON/library records;
- `QNetworkRequest`, `QNetworkReply`, or `QProcess`;
- raw REST/GraphQL response structures;
- tokens, authorization headers, raw stderr, or unrestricted response text.

Normalized records include stable viewer, repository, pull-request, commit,
review, thread, comment, file, check, capability, rate-limit, and snapshot
identities as applicable. Count, byte, page, nesting, string, and allocation
limits are enforced before a result can be staged.

Actor identity uses GitHub's opaque global node ID plus a bounded login. The
node ID is case-sensitive and scoped by the selected profile and host; it is
never decoded or compared across GitHub installations. The authenticated
numeric account ID remains part of request authority, while the viewer's node
ID and login seed author consistency within each snapshot. A present author
must have both facts, and a deleted or unavailable author is explicitly null.
Entity, actor, and non-target search node IDs share one collision domain;
only an exactly correlated reference to the selected pull request may reuse
an existing node ID.

The GraphQL review-thread/viewed-state projection includes repository and
pull-request node IDs and must match the corresponding REST records. Each
GraphQL file path maps exactly once to the REST file collection and carries
`DISMISSED`, `UNVIEWED`, or `VIEWED`. Each thread carries an explicit `LINE`
or `FILE` subject plus per-target resolve/unresolve availability: line
threads require line anchors, file threads prohibit them, and every comment
path must match its thread path. REST and GraphQL records therefore cannot be
replayed across repository, pull-request, file, actor, or thread authority
without the complete candidate failing atomically.

The v1 selected contract admits at most 4 MiB per response page, 100 records
per page, JSON nesting depth 32, 64 members per object, 3,000 files, 250
pull-request commits, 1,000 search results, 50,000 aggregate records, 256 KiB
per body, and 16 MiB of normalized text. Callers may tighten these ceilings
but cannot relax them. Overflow, duplicate identity, malformed relationship,
unknown enum, incomplete GraphQL page, or allocation failure rejects the
entire candidate snapshot.

Recorded fixtures retain an exact sanitized field projection. They are not
complete raw GitHub payloads. Raw transports own status, content type,
pagination, header, and envelope validation and must discard additive
provider fields before the exact selected projection is normalized; no raw
response type or unbounded provider field crosses the adapter.

The BPR-0019 recorded facade executes one selected page for each operation.
Every transport response must explicitly certify that the selected page is
complete; unknown completeness or a declared continuation rejects the whole
candidate. The facade also rejects incompleteness in GraphQL `pageInfo` and
projected check/search totals. It is not a production pagination
implementation: BPR-0027 must exhaust REST and GraphQL pages under the
aggregate limits before submitting one complete snapshot. Search results use
the pull request's global node ID and repository-scoped number, never the
Search Issues numeric issue ID. Before projecting away raw URLs, the
production transport must also prove each result belongs to the canonical
repository-scoped request. No one-page fixture result may be used to claim
that a larger live pull request was loaded completely.

The BPR-0026 wire transport revalidates the canonical `ListChecks` URL and
wire request, but the application-host request context does not yet bind the
returned checks page to its expected head OID. BPR-0027 must carry and verify
that `HeadOid` relationship before normalized checks can enter a staged
snapshot.

Provider callbacks return immutable completions and never mutate the active
cache, core, Qt model, or widget. The host posts each completion to the
serialized application lane, stages it under the exact request context, and
commits it only after the core emits the matching acceptance delta.

## Normalized provider surface

```text
GetViewerIdentity
GetRepository
GetPullRequest
ListPullRequestFiles
ListPullRequestCommits
ListReviews
ListReviewThreads
ListIssueComments
ListChecks
SearchPullRequests
MarkFileViewed / Unviewed
CreateReview / SubmitReview
CreateThread / ReplyToThread
ResolveThread / UnresolveThread
```

Unsupported capabilities produce explicit unavailable states and safe fallbacks; they do not silently approximate a mutation.

RFC-BPR-0002 composes only the read operations needed for viewer, repository,
PR metadata, files, commits, reviews, threads, comments, checks, search, and
capability probing. The mutation operations above describe the stable future
provider contract but are not callable through the RFC-BPR-0002 composition.

A no-mutation verification sink parses every REST method/route and the GraphQL
document's selected operation type. It rejects every GraphQL `mutation`
operation rather than relying on query names, endpoint names, or fixture
intent.

## Failure and diagnostic boundary

Transport and normalization failures are classified into bounded categories
such as authentication, permission, network, API version, unsupported
capability, rate limit, certificate, redirect, cancellation, timeout, process
startup, malformed response, and response limit. Provider-owned details are
redacted before a safe typed event crosses inward. Qt owns user-facing prose.

Raw remote content and credential-bearing diagnostics are never retained as
RFC evidence.

## Cache isolation

Provider sessions, capabilities, retries, rate limits, staged results, and
accepted normalized snapshots are keyed by profile revision, host,
authenticated account, repository/PR identity, and contract revision as
applicable. Switching profile or authenticated account cancels or suppresses
old work, clears visible shared projections, discards stages, and prevents
cache reuse across the old identity.

## Mutation safety

Every mutation requires an immutable target descriptor containing host, repository identity, PR number, expected head OID, authenticated user identity, and idempotency key where supported. Tests default to recorded contracts and a fake mutation sink. Live mutations require a user-designated safe PR.

RFC-BPR-0002 performs no mutation. Mutation safety becomes executable product
scope in RFC-BPR-0008.

## Build and distribution boundary

GitHub Actions has no role in building, testing, packaging, releasing, or
verifying the provider. Local and target-native tools produce the evidence.

The build and packaging records must nevertheless report the adapter's actual
Qt Network and platform-vault compile, link, and runtime dependencies.
RFC-BPR-0014 owns delivered-package signing and distribution certification; it
does not defer accurate dependency accounting from RFC-BPR-0002.
