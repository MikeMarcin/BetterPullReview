# GitHub Provider Contract

The concrete RFC-BPR-0002 flow and lifecycle are defined in [GitHub Host
Implementation Architecture](github-host-implementation.md).

## Architectural allocation

- The functional core receives normalized host/account/capability observations, decides semantic command legality and request identity, and accepts or rejects completion events.
- The application host owns the narrow provider capability port, selected
  session policy, staged-snapshot acceptance protocol, cancellation, cache
  invalidation, and serialized completion delivery.
- The GitHub adapter implements profile validation, normalization,
  REST/GraphQL contracts, the in-progress opt-in CLI read boundary, the production native
  HTTPS transport, credentials, proxy/trust enforcement, bounded
  retry/rate-limit handling, and future mutation execution.
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
remain query-free on the exact validated authority and inside its API
namespace.

The public composition provides built-in-provenance GitHub.com native-HTTPS
and CLI profiles. An optional GHES 3.18 hostname supplied through the
application command line creates explicit-user-provenance native-HTTPS and CLI
profiles only after the same validation. The provider owns immutable copies
of all selected profiles, rejects duplicate profile identities, and routes
each operation by the exact validated profile.

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

## Anonymous read authority

The existing provider and pager admit nine REST operation kinds under explicit
public-preview authority: repository, pull request, files, commits, reviews,
inline review comments, issue comments, checks, and search. `list_review_comments`
uses `/pulls/{n}/comments` and exhausts its bounded REST pagination. Reply linkage
forms comment groups; it supplies no GraphQL thread identity or resolution fact.
Viewer identity and GraphQL each have a distinct typed pre-request refusal.
Every GHES contract is refused.

Anonymous native HTTPS accepts no credential parameter and sends no Authorization
header. It performs no vault lookup or capability probe. The host supplies an
explicit empty viewer, policy-unsupported capabilities, and unobserved capability
evidence; those values cannot stand in for an authenticated schema observation.
Rate observations preserve their resource and reset facts, exhausted reads do not
retry, and 404 diagnostics disclose neither existence nor privacy. Full bounded
pagination must complete before the existing normalizer can publish a snapshot.

## Authentication transports

### GitHub CLI broker

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
A stop edited while its review is pending goes by the fixed GraphQL mutation
`UpdateReviewComment` (`updatePullRequestReviewComment`, by the comment's node
ID), because GitHub.com answers the REST `PATCH /pulls/comments/{id}` 404 for a
pending comment ([[BPR-1368]]); a submitted stop is still edited through REST.
These Tour operations remain unavailable on an unqualified host/transport.
`gh api graphql` also carries a thread resolved or reopened
(`ResolveThread`, `UnresolveThread`) and a reaction added or removed
(`AddReaction`, `RemoveReaction`), each after the read that binds its subject
to the pull request (`ObserveThread`, `ObserveReactionSubject`)
([[BPR-1741]]). Those are the only GraphQL writes it admits; each body
must equal its compiled form, and a reply is still refused there.

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

#### Historical RFC-BPR-0002 boundary and evidence

The following records the original zero-launch milestone and its strict trust
contract. The RFC-BPR-0015 amendment supersedes that enablement prohibition;
the recorded risks remain unresolved, not passing containment evidence.

Native HTTPS is the sole milestone-one production authentication transport.
The public CLI profiles remain visible, but the provider returns fixed typed
`cli_unavailable` before transport construction, CLI executable/configuration
discovery, or a process-port call. The broker below is retained, tested
scaffolding and a constraint on any future implementation; it is not an
enabled milestone-one transport. The public self-test enumerates the visible
rows and reports `CLI_UNAVAILABLE_COUNT:1` for the default profile set and
`CLI_UNAVAILABLE_COUNT:2` with explicit GHES. Public row selection performs one
bounded noncredential profile-resolution effect per row from clean start. An
interactive switch from authenticated native HTTPS may first exact-delete the
predecessor credential binding; it does not enroll or retrieve a credential
for the CLI profile. Provider rejection schedules no transport, wire,
capability, or CLI-specific effect.

That milestone-one description is retained as history. RFC-BPR-0015 later
enabled opt-in macOS CLI reads, and [[BPR-0909]] replaced the precomposed CLI
rows with the discovered Use GitHub CLI entry; the self-test now reports
`CLI_ENTRY_ABSENT_OK` or `CLI_ENTRY_OFFERED_OK` with `CLI_ENTRY_CARDS`
([[BPR-0982]]).

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
  requirements, the broker path remains unavailable.

BPR-0025 implements the closed invocation compiler, Qt-free injected process
port, bounded sole-`QProcess` runner, correlation-preserving wire terminal,
and fixed unavailability gate. The compiler translates the already-validated
GitHub.com/GHES REST and GraphQL routes into the exact relative form expected
by `gh api`; callers cannot contribute arguments or environment names.

The runner owns one serialized process worker. On Unix it requests a new
session, closed inherited descriptors, disabled core dumps, and reset signal
handlers; the child is the new session's initial process-group leader.
Compiled child observation proves those flag/session/group facts. On Windows
every child inherits only its three standard handles, and a launch whose
handle list cannot be built is refused at creation (BPR-1114). Compiled
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
target-native CLI runner behavior are deferred with positive CLI enablement to
RFC-BPR-0015; BPR-0036 retains only Windows native-HTTPS, vault, and public-path
verification.

Canonical/currently non-repository working-directory validation does not
establish owner-controlled permissions or bind directory identity through
launch. Unix process-group escape and PID/process-group identity reuse across
cleanup signals are likewise unresolved; initial-group signaling is not
claimed race-free. They remain explicit future-enable blockers rather than
milestone-one closeout requirements.

The current direct audit used the official `gh` 2.96.0 release dated
2026-07-02. Its downloaded macOS arm64 ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`,
and the extracted binary SHA-256 was
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`.
An owner-private copy produced a fixed bad-certificate TLS alert before HTTP
but followed a cross-authority `302` loopback redirect. The selected origin
received the synthetic authorization and exact body; the distinct sink
received neither. POST `307` and `308` sink absence remains unproven rather
than accepted as rejection. The installed `gh` 2.95.0 binary, SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun and produced the same material outcomes.

Persisted `http_unix_socket` is an active ambient transport input, not a
telemetry-only setting. For both selected binaries, the isolated probe loaded
that setting, and exact `gh api` execution made one authenticated HTTP request
through an AF_UNIX sentinel with the expected synthetic Authorization value.
Production would need the platform home or application-data facts used by the
CLI's account state, so merely omitting `GH_CONFIG_DIR` cannot exclude that
redirection. Canonical `SSL_CERT_FILE`/`SSL_CERT_DIR` paths also do not bind
selected trust-material identity or contents through the CLI's later open.

The current `gh api` command has no per-invocation `--user` selector.
`gh auth token --user` reveals the token and is prohibited;
`gh auth switch --user` mutates persisted active-account state and is racy
with an established request identity. The selected source resolves
`ActiveToken` for each round trip, falls back from a keyring error to a
blank-host token, and the macOS keyring implementation launches
`/usr/bin/security`. None of those mechanisms binds the child to
BetterPullReview's correlated credential generation.

Historical per-account pinning ([[BPR-1373]], RFC-BPR-0015's 2026-10-09 owner amendment)
selects an account with neither command. A CLI profile chosen for one login
runs each `gh` call with `GH_CONFIG_DIR` naming a private directory that the
app writes for that call alone and removes afterwards: owner-only, read-only,
holding `config.yml` and a `hosts.yml` that names only that host and login.
`gh` reads that login's keychain token itself, so no token passes through
BetterPullReview, and no `GH_TOKEN`-family variable is ever set. When the
login has no keychain entry, `gh` falls back silently to the blank-host
(active) token. The [[BPR-1370]] viewer check, held to the profile's host,
login and first-observed account ID, is therefore the proof: any other viewer
refuses the read and the lease, and the profile shows the account signed out
in `gh`. This binds the account by observation, not by credential generation.
The window between `gh`'s keychain read and its request remains.

Milestone-one CLI reads expose all eight typed reasons:
`milestone_one_product_scope_deferred`,
`executable_identity_unverifiable`,
`cross_authority_redirect_unverifiable`,
`ambient_configuration_unverifiable`,
`credential_identity_binding_unverifiable`,
`working_directory_identity_unverifiable`,
`descendant_process_containment_unverifiable`, and
`numeric_process_identity_reuse_unverifiable`. They return
`cli_unavailable` with zero process launches. The synthetic authentication
config is verification-only; credential-bearing environment surfaces remain
absent from the broker. RFC-BPR-0015 owns any positive CLI enablement and must
directly close every blocker; satisfying only redirect behavior is
insufficient.

### Native HTTPS

BPR-0026 implements the internal wire port with one dedicated Qt worker,
event loop, and `QNetworkAccessManager` per transport. The Qt objects do not
cross the adapter, and one transport rejects a second concurrent operation.
The complete wire request and correlation are revalidated before the worker
uses a credential or performs network work. BPR-0027 consumes it through the
application-facing provider and owns pagination, normalization, body-aware
failure interpretation, and staged snapshot delivery. BPR-0029 selects this
native transport in the public executable for GitHub.com and explicitly
configured GHES profiles. Its recorded public QAction fixtures preserve that
profile/provider/host/core/Qt composition but substitute a fixture transport
factory; they are not real-network native-HTTPS runs. The direct native wire
fixtures below verify the request, TLS, proxy, redirect, and credential-bearing
transport separately.

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

The validated GitHub profile governs the origin authority and every origin
request. Actual connections may instead terminate at bounded HTTP or SOCKS5
proxy endpoints returned by the selected Qt system-proxy policy. Those routing
intermediaries are not GitHub profile authorities and never receive the GitHub
`Authorization` header as proxy authentication.

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
rate limit only when exact metadata proves exhausted quota, a valid bounded
`Retry-After` is present, or a bounded strict provider payload identifies a
secondary or GraphQL `RATE_LIMITED` response. HTTP 429 is rate limited only
with an exact causal rate-limit record. GraphQL HTTP-200 and non-200 error
envelopes use the same bounded structured classification; message matching is
limited to the documented secondary-limit form and cannot launder a generic
permission failure.

GitHub.com responses require the complete
`X-RateLimit-Limit`/`Remaining`/`Reset`/`Resource` tuple. GHES may omit that
tuple completely, but a partial, duplicate, malformed, unknown-resource, or
wrong-resource tuple is rejected. Core REST, Search, and GraphQL operations
must respectively report `core`, `search`, and `graphql`. The provider
accumulates those independent observations across every exhausted page and
preserves them on later local, wire, or remote failure. A rate-limit terminal
also identifies its exact causal resource record and whether it is a
secondary limit; `Retry-After` remains a separate bounded scheduling fact.

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
accepted only for the exact profile, profile revision, host identity,
canonical hostname, account, server contract, application session, and
identity generation. Store credentials only in Windows Credential Manager,
macOS Keychain, or Linux Secret Service.
Never store tokens in SQLite, settings, environment files, logs, crash
reports, or command lines. Locked, unavailable, access-denied, corrupt, and
failed-delete states remain typed; there is no plaintext, settings,
environment, or SQLite fallback.

The test-enabled build exposes an adapter-private loopback proxy and
single-fixture-CA seam; `BUILD_TESTING=OFF` omits those factories and
symbols. A disposable TLS-over-CONNECT harness exercises 15 cases: success,
ordered proxy fallback, same-authority redirect, cross-authority redirect,
GraphQL redirect, untrusted certificate, a response-body limit, one
8-KiB-plus-one response-header-field limit, in-flight cancellation, timeout,
pre-cancellation, HTTP 401/403/429, and content-encoding rejection. It does not
contain separate 65-field or aggregate-header-only runtime scenarios; those
adapter checks remain source/contract evidence. The harness verifies the exact
CONNECT authority, SNI, Host, request target, method/body, and Authorization
placement without contacting GitHub. Runtime facts report TLS support/backend
and the first raw system-proxy kind/count without addresses or secret
material; they do not prove that every returned candidate is reachable.

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
Tracked and on-disk scans found no GitHub Actions workflow files, and the
source/build configuration selected no GitHub Actions integration.

Windows native-HTTPS execution remains BPR-0036 work. It does not block other
implementation, security, documentation, simplification, or later-RFC work;
it blocks only affected Windows claims and final RFC-BPR-0002 Auto-Verified
closeout.

The portable vault capability accepts only an exact profile/profile-revision/
host/hostname/account/server-contract authority and exact
request/session/identity generation. Its credential owner is noncopyable,
caps nonempty material at
2,560 bytes so every supported native vault can represent it, exposes only
read-only bytes, and securely overwrites storage before release. Store is an
ownership-transferring atomic upsert; retrieve, absence, delete, failure, and
cancellation are distinct typed outcomes. No raw operating-system error text
or credential bytes enter provider errors or projections.

On macOS, the current adapter retains BPR-0022's explicit file-based Keychain
selection for the ad hoc linker-signed, unbundled executable, which has no
team identity, bound `Info.plist`, or provisioning profile. BPR-0029 advances
its fixed production and integration namespaces from the historically tested
v1 format to v2 and scopes every generic-password operation to the exact
default keychain and 574-character fixed-width hexadecimal encoding of the
complete hostname-bound portable authority. Stable explicit bytes distinguish
the GitHub.com and GHES contracts without serializing a C++ enum ordinal.
Requests fail rather than display authentication UI. The dedicated
native-test service is distinct from the production service. The adapter does
not set the
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

On Windows, the current adapter retains BPR-0023's wide-character generic
Credential Manager operations while BPR-0029 advances the fixed production
and integration target prefixes from the prepared v1 format to hostname-bound
v2. Each prefix precedes the same complete 574-character authority encoding.
Store is one `CredWriteW`
create-or-replace operation;
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

On Linux, the current adapter retains BPR-0024's Linux-only
`libsecret-1>=0.20.0` selection while BPR-0029 advances the schemas from the
historically tested v1 format to hostname-bound v2. It opens a derived
`SecretService` with
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

The Linux adapter uses only the default collection. Its exact disjoint v2
production and integration-test schemas each have one explicit `authority`
attribute containing the complete 574-character hostname-bound portable
authority; libsecret adds the exact `xdg:schema` attribute. Store uses atomic
replacement and `application/octet-stream`. Retrieve accepts only that type
or exact
`text/plain`: gnome-keyring 50 with libsecret 0.21.7 was directly observed
normalizing the stored content-type metadata to `text/plain` while preserving
the opaque bytes. Locked and unavailable service, access denial, corrupt
items, invalid requests, allocation failure, ordinary operation failure,
absence, replacement, deletion, and cancellation remain typed. Synchronous
libsecret operations run only through the host effect executor.

The v2 service, target, and schema namespaces are disjoint from the historical
hostname-free v1 namespaces on every platform. Production does not read,
enumerate, migrate, or fall back to v1. The v1 production adapters were never
selected by the public executable. Every directly executed v1 integration
fixture completed exact cleanup; the Windows v1 fixture remained preparatory
and created no target-native item. This is an intentional pre-release format
break, not a credential migration.

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
This is historical v1 Linux vault evidence from a full-system VM; it is not
bare-metal evidence. BPR-0026 separately verified the internal Linux native
HTTPS wire surface with the 15-case loopback fixture, runtime observation,
ELF dependencies, and a controlled single-success dependency trace.

BPR-0029 separately verified the current v2 fixture and recorded public
composition path
in an Ubuntu 26.04 x86_64 full-system QEMU guest. The Secret Service
setup/lifecycle/cleanup cases passed 3/3 along with independent exact cleanup
and hostile, unavailable, and locked cases. Both GitHub.com and GHES public
fixtures passed with exact host and rendered state through the substituted
fixture transport; fresh tests-on and
tests-off Release builds completed 81/81 and 36/36 targets respectively, the
fixture seam was absent, native platform-vault selection was reported, and
dependency, runtime, and ELF audits passed.

Windows target-native evidence remains open under BPR-0036 and is nonblocking
for implementation on available platforms. BPR-0029 directly verifies the
composed path on macOS and Linux; BPR-0036 retains the Windows public-fixture
run. A mock, cross-compile, vault-only run, or internal wire fixture cannot
prove a retained platform claim.

### RFC-BPR-0003 Git smart-HTTPS lease

RFC-BPR-0003 adds a separate adapter-private wire contract for Git smart HTTP.
It does not add an operation to `github_provider_port`, reuse the JSON
`github_wire_transport_port`, or authorize installed Git to perform network
authentication. The GitHub application host may mint an opaque noncopyable
lease only after accepting a native-HTTPS pull-request snapshot. The lease is
bound to the exact application session, profile and profile revision, host
identity, canonical hostname and server contract, authenticated account,
canonical repository identity, and GitHub identity generation.

The lease admits only:

- service discovery by exact `GET` of the compiled
  `/OWNER/REPOSITORY.git/info/refs?service=git-upload-pack` target; and
- stateless RPC by exact `POST` of the compiled
  `/OWNER/REPOSITORY.git/git-upload-pack` target.

The adapter, not the caller, compiles the canonical path and owns
Authorization, TLS, trust, proxy, cookies, caching, headers, and response
decoding policy. There is no generic URL/request API. The discovery query is
valid only on the original request. Automatic redirects are disabled and all
redirects fail closed, including same-authority and query-bearing redirects.
The Git smart path therefore does not weaken the RFC-BPR-0002 REST rule.

Discovery and `ls-refs` are bounded control rounds, not streamed pack
surfaces. Their replies are read in fixed chunks and retained only within a
64-KiB whole-reply ceiling before parsing. Fetch response bytes are read in
chunks of at most 16 KiB and forwarded directly to the incremental
pkt-line/sideband consumer instead of being accumulated as a complete body.
The production smart path has a 15-minute total deadline and a 30-second
no-progress idle deadline. Exact service/content types, object format,
advertised base/head identities, framing, and final object connectivity must
all agree with the accepted request before cache staging can succeed.

GitHub.com smart Git uses `github.com`; it never substitutes the
`api.github.com` REST/GraphQL authority. GHES smart Git uses the exact
validated enterprise hostname. The loopback contract exercises the complete
19-scenario GHES matrix and the GitHub.com discovery, `ls-refs`, and fetch
origin subset without contacting either service.

Profile, account, repository, pull-request, or identity-generation
invalidation revokes the lease before old-authority epoch work can publish.
Shutdown does the same before native transport and credential teardown.
Revocation rejects new calls, attempts bounded cancellation of admitted work,
and preserves one typed terminal with the complete original context for every
normally accepted submission. CLI profiles expose no native smart-HTTPS Git-object lease. Their opted-in
macOS path uses a separate supervised gh Git-object bridge for reads and a
separately confirmed mutation lease for review/comment publication.

An app-owned minimal protocol-v2 client, not installed Git, constructs the
closed `ls-refs` and `fetch` requests for the validated accepted base and
fixed pull-request-head refs and validates every advertisement and response.
Its `want` set is exactly the deduplicated accepted base/head pair; any bounded
`have` is both catalog-visible and directly commit-object-verified in the same
repository cache scope.
The fixed installed-Git `index-pack` child receives the bounded pack stream
and validated bare-repository path but no protocol advertisement, remote ref,
credential, Authorization header, URL, proxy, trust, cookie, provider
payload, or ambient account authority. Separate fixed ref operations receive
only generated private BetterPullReview refs and exact expected OIDs. Native
HTTPS remains the only credential-bearing network implementation.

Commits `d903290` and `edd762e` implement and directly verify these
application-protocol and native-wire component boundaries. Commit `2801241`
implements their authority-partitioned cache/ref consumer; its direct cache
contract passed 1/1 in 162.58 seconds, while the adjacent protocol,
native-transport, process, and architecture replay passed 7/7 in 92.95
seconds. BPR-0044 adds the durable catalog state machine, and BPR-0045 adds the
dedicated host's journal-before-import sequencing, exact append recovery, and
stale-publication boundary. Those component results still do not prove
selection by the public executable; that remains BPR-0047 work.

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

GitHub.com and GHES 3.18 document `ListReviews` results in chronological
order. The normalized provider preserves the fully exhausted sequence and
assigns each review a bounded one-based chronology ordinal across pages.
It does not reconstruct order from timestamps or identifiers. A present
`submitted_at` value is parsed and bounded; pending reviews may omit it.
Current-viewer association uses the exact scoped actor/node identity, never
login text.

The GitHub host uses that observation to derive the one handoff candidate for
`My last reviewed`: the highest-order exact-current-viewer review whose state
is not pending and whose algorithm-tagged commit identity is present.
Dismissed is a submitted state. Once selected, the exact record remains
authoritative even if its commit object was never retained; the epoch alias
is then unavailable. Neither provider nor host substitutes an older resident
review, current head, local time, or guessed commit.

The GraphQL review-thread/viewed-state projection includes repository and
pull-request node IDs and must match the corresponding REST records. Every
pull-request-bearing GraphQL page also repeats non-null `headRefOid`; it must
equal the REST head OID, so a head change between endpoints or pages rejects
the whole candidate. Each
GraphQL file path maps exactly once to the REST file collection and carries
`DISMISSED`, `UNVIEWED`, or `VIEWED`. Each thread carries an explicit `LINE`
or `FILE` subject, non-null `LEFT` or `RIGHT` diff side, separate nullable
current and original lines, and per-target resolve/unresolve availability. A
`LINE` thread with neither coordinate remains a line-subject conversation with
an unavailable source-line anchor. A current root with only an original line
is refused rather than treating that original line as current. An outdated
root may retain only its original line. File threads prohibit both coordinates.
A line-thread reply may omit both of its own nullable coordinates; any reply
coordinate that is present remains independently typed and bounded. The
normalizer never substitutes an original line for a missing current one or
derives a missing thread anchor from a comment. Every comment path must match
its thread path. REST and GraphQL
records therefore cannot be replayed across repository, pull-request, file,
actor, or thread authority without the complete candidate failing atomically.

The v1 selected contract admits at most 4 MiB per response page, 100 selected
records per page, JSON nesting depth 32, 256 raw members per object, and
10,000 elements per raw array. A raw string is bounded by the configured page
body ceiling so ignored GitHub fields such as a documented 500 KiB file patch
can be discarded safely; projected fields retain their narrower semantic
limits. Aggregate normalized ceilings remain 3,000 files, 250 pull-request
commits, 1,000 search results, 50,000 records, 256 KiB per selected body, and
16 MiB of text. The pull-request summary's `changed_files` and `commits`
counts are required, bounded by those ceilings, and must equal the completely
exhausted REST collections even when GraphQL file viewed state is unavailable.
Callers may tighten these ceilings but cannot relax them. Overflow, duplicate
identity, malformed relationship, unknown enum, incomplete GraphQL page, or
allocation failure rejects the entire candidate snapshot.

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
implementation. The BPR-0027 provider exhausts REST and GraphQL pages under
the aggregate limits before submitting one complete snapshot. Search results use
the pull request's global node ID and repository-scoped number, never the
Search Issues numeric issue ID. Before projecting away raw URLs, the
production transport requires the Search item `pull_request` marker and
proves its REST URL, `repository_url`, and web `html_url` exactly bind the
server-canonical repository owner/name and the result number under the
validated profile authority. The caller's locator casing cannot replace that
observed canonical repository identity. No one-page fixture result may be
used to claim that a larger live pull request was loaded completely.

The BPR-0026 wire transport revalidates the canonical `ListChecks` URL and
wire request. BPR-0027 additionally carries the expected head OID through the
application-host request and rejects a returned checks page whose canonical
commit relationship does not match before normalization or staging.

Provider callbacks return immutable completions and never mutate the active
cache, core, Qt model, or widget. The host posts each completion to the
serialized application lane, stages it under the exact request context, and
commits it only after the core emits the matching acceptance delta. A
submission that returns normally produces exactly one terminal even after
advisory cancellation; a throwing submission retained no operation. The
decision lane remains available through host quiescence and invokes every
accepted post exactly once without inline execution.

Only after that exact accepted snapshot commit may the GitHub host emit the
provider-free `accepted_pull_request_handoff` to the dedicated epoch host. It
carries the application session; profile identity/revision; Git host,
authority fingerprint, canonical hostname, and server contract; GitHub
identity generation; authenticated account and exact viewer actor; repository
identity and validated locator; pull-request identity/number/generation;
provider snapshot identity and typed revision origin; object format and
algorithm-tagged base/head object identities; opaque base-branch identity plus
its validated typed base ref; the one bounded newest-current-viewer review
candidate with typed provider order; and the optional native smart-HTTPS lease.
Source-observation identity and observed time do not cross:
the epoch host obtains those from its operating-system entropy and clock ports
after recovery. Source identity is minted and collision-probed before the
handoff reaches the epoch core; UTC is observed only for a preparation that
requires it. Provider records, unvalidated branch/ref text, URLs, credentials,
and Qt projections do not cross.

The GitHub host revokes the lease, sends the matching
`pull_request_authority_invalidated` handoff, and only then clears the old
accepted authority and identity-sensitive projection. Epoch work retains the
original context and cannot regain authority by reading the GitHub host's
current snapshot. Handoff delivery is observational with respect to the
already accepted GitHub transition; the epoch host stages, decides, persists,
and publishes through its own core and serialized completion path.

The public composition uses separate provider and vault worker executors and
the Qt event loop as the serialized decision lane. Native credential input is
owned by a Qt password-echo prompt, converted immediately to the application
move-only material type, and cleared from the Qt-owned temporary. The public
host selects exact deletion of its retained credential on orderly shutdown;
the delete terminal is required for quiescence before workers are closed and
joined. No durable selector or enumeration path exists, so an abrupt crash can
leave a v2 item that the next process cannot recover.

RFC-BPR-0003 adds a separately owned epoch worker and composes the concrete
installed-Git cache, SQLite catalog, system entropy/clock ports, epoch host,
and existing Qt epoch projection. The GitHub host calls the narrow handoff
port only on the serialized lane after core snapshot acceptance. The retained
smart authority wraps the accepted account transport, checks the complete
authority and object-format binding on every acquisition, and revokes all
issued transports on identity replacement or shutdown. Epoch and GitHub hosts
must both report quiescence before their workers and provider are drained.

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
ListReactions
AddReaction / RemoveReaction
```

Unsupported capabilities produce explicit unavailable states and safe fallbacks; they do not silently approximate a mutation.

RFC-BPR-0002 composes only the read operations needed for viewer, repository,
PR metadata, files, commits, reviews, threads, comments, checks, search, and
capability probing. The mutation operations above describe the stable future
provider contract but are not callable through the RFC-BPR-0002 composition.

RFC-BPR-0002 exposes narrow read-only provider ports. Recorded fixture methods
pass through one exact parser that accepts only uppercase `GET` and `POST`;
invalid or unknown text is never coerced to a read. REST catalog entries are
admitted only as typed `GET` requests. GraphQL `POST` is admitted only after a
shared bounded full-document scanner finds exactly one explicit `query` plus
bounded fragments. A top-level `mutation` or `subscription` anywhere in the
document, multiple operations, malformed scanned structure or token framing,
or a byte, nesting, or definition limit fails closed. This bounded structural
classifier is not a complete GraphQL grammar parser. Comments, strings, block
strings, fields, and operation names containing words such as `mutation` do
not create false write classifications, and the outbound contract still
requires exact equality with the canonical compiled request body. The
contract suite proves that backstop independently for every legacy/current
GraphQL shape by adding a scanner-approved read selection and requiring the
altered body to fail.

## Failure and diagnostic boundary

Transport and normalization failures are classified into bounded categories
such as authentication, permission, network, API version, unsupported
capability, rate limit, certificate, redirect, cancellation, timeout, process
startup, malformed response, and response limit. Provider-owned details are
redacted before a safe typed event crosses inward. Qt owns user-facing prose.

The application-host contract enumerates all 15
`github_provider_failure` values and requires each to produce one typed core
reason, no accepted snapshot, and no provider detail. The deliberately
coarser mapping sends both process-startup and generic-unavailable failures to
the bounded network reason; it does not expose transport diagnostics.

Raw remote content and credential-bearing diagnostics are never retained as
RFC evidence.

Local GitHub CLI account discovery records its stage, fixed process-failure
category, elapsed milliseconds, observed exit (or unavailable sentinel), and
stdout/stderr byte counts through the existing profile diagnostic log. The
sign-in dialog presents an actionable fixed message and selectable text.
Neither surface receives child output, inventory JSON, account names or
credential environment values. Paths are omitted from both persistent and terminal
summaries. Account eligibility is separate from process success: exit-zero
inventory may still report an inaccessible or signed-out account; only signed-in,
keyring-backed accounts are offered. A nonempty inventory with no eligible account
has its own message rather than a process failure or an empty-inventory message.

### Observed macOS compatibility — 2026-10-17

On one Intel Mac, macOS 15.1.1 (24B91), BPR 0.2.1 and gh 2.102.0,
the unchanged write-denying profile made a password-free Apple `security`
item-metadata lookup fail with exit 44. The user search list and default
Keychain remained correct, and an explicit existing login-Keychain path did
not resolve the lookup. The same profile allowed the metadata lookup on an
M4 running macOS 26.6.2. No sandbox, credential or Keychain-permission change
was made to resolve this observation.

After the Intel update and reboot to macOS 15.8.1 (24H32), with BPR and gh
versions unchanged, the identical password-free lookup passed both normally and
sandboxed. The owner reported that BPR sign-in now worked. A post-update GUI
sign-in course was deferred during unrelated developer-tool updates; SSH account
status is not equivalent to desktop credential access. These observations do not
identify the first fixed macOS version, prove an Apple implementation cause, or
separate the update from reboot-related state. Retain the sandbox and test both
process failure and completed-but-ineligible inventory; do not use this incident
as universal native qualification or as reason to broaden Keychain access.

## Cache isolation

Provider sessions, capabilities, retries, rate limits, staged results, and
accepted normalized snapshots are keyed by profile revision, host,
authenticated account, repository/PR identity, and contract revision as
applicable. Switching profile or authenticated account cancels or suppresses
old work, clears visible shared projections, discards stages, and prevents
cache reuse across the old identity.

Milestone one does not share a physical Git object database across accounts
or profile revisions. The catalog allocates one persistent random 256-bit
`repository_cache_scope_id` for the exact profile/revision, host, account,
canonical repository, and object-format partition, plus a separate random
256-bit `pull_request_authority_scope_id` for each PR in that cache. Their
lowercase 64-hex encodings are opaque, nonreversible, never supplied by a
caller, and collision-checked before use. The PR scope qualifies every
retained ref, epoch ordinal, alias, page query, and selection.

Every catalog/cache access requires the currently accepted authenticated
authority; persisted objects do not grant offline or restart access.
Invalidation revokes visibility. An account cannot enumerate another cache or
PR scope or request an arbitrary cached OID. Catalog/ref scope mismatch,
object-format mismatch, and stale generation fail closed. Provider clone
URLs, locator casing, login text, and raw `HeadRef` values never select a
cache, path, ref, lease, or catalog partition.

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

## Off-hunk publication wire contracts (RFC-0022 milestone B)

The comment request retains reviewer prose separately from its selected wire
representation. Native requests keep the existing body/commit/line/path/side
bytes. File requests compile the validated retained endpoint through the canonical
permalink codec and send only body, commit_id, path and subject_type=file.
Receipts keep the actual remote subject and selected representation; exact path,
commit, public body and actor must agree before a response confirms creation.

The designated live file comment returned `subject_type=file` with synthetic
`line=1`, `original_line=1`, `side=RIGHT`, and null range fields. Receipt parsing
and GET-only recovery admit that bounded metadata shape as well as absent line
metadata. They retain the file subject and derive semantic placement only from
the validated immutable permalink. Other numeric positions, incomplete placeholder
shapes, ranges and replies cannot confirm a file publication. This does not alter
unsupported classification or permit a new POST for a possibly transmitted child.
GraphQL FILE threads may likewise carry `line=1`, `originalLine=1`,
`diffSide=RIGHT`, or both null coordinates. Both read paths discard those thread
positions before producing typed records. FILE comments still require null
coordinates on every page; the immutable body link supplies the source line.

Classifier revision2 accepts the owner-approved recorded contract only for
MikeMarcin/BPR-Test PR2, exact base/head/refs and database/node identities,
MikeMarcin's exact actor identity, and desktop profile2. Every classification
revalidates the sealed endpoint, exact Markdown/text blob and80-line bounds
against the recorded complete RIGHT7–13 provider patch. Other identities,
changed blobs and hunk lines cannot borrow this immutable proof.

The complete bounded422 must have exactly message, documentation_url, string
status and one errors entry, with the exact approved resource/field/custom/
`could not be resolved` tuple. Unknown/duplicate members, other strings/types,
Retry-After, Location, invalid or duplicate metadata and exhausted/invalid core
rate budgets refuse. Native HTTPS must first establish complete transport and
valid framing. The recorded message has an owner-accepted interpretation risk;
it establishes no blanket service capability. The
[mandate r3 decision](../issues/_checkpoints/BPR-0575/m4-r3-amendments.md)
is distinct from live observation. Historical unresolved publications stay
GET-only under revision2. Synthetic GitHub.com/GHES fixtures retain their own
explicit contracts and grant no production authority.

The write evidence cache is separate from the existing read/schema cache. It has
128 entries and a maximum 300-second observed lifetime. Profile/revision, host,
server contract, complete actor, repository database/node identity, operation,
API/classifier revision and freshness generation are exact key components.
Expiry and a backward monotonic observation yield unknown. Only verified native
success, definitive rejection, or an explicitly admitted recorded contract can
supply an evidence entry. The serialized session owns this cache. Keys also bind exact head, path and
line; an observation cannot establish support at a different anchor. The cache
itself is not mutation or recovery authority.

The native HTTPS root-comment exchange now retains a complete bounded HTTP 422
as response data for the exact classifier. This does not mark it a successful
publication. TLS, response limits, framing/completeness, rate metadata,
cancellation and redirect checks still apply; incomplete or network-failed
responses remain failures. Other review operations retain their existing status
contracts. The serialized session selects the recorded contract from its own
validated configuration. Tests also retain explicitly synthetic contracts.
