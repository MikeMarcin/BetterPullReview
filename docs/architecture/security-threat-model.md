# Security and Threat Model

## Protected assets

- GitHub credentials and authenticated identity
- AWS/Bedrock, Anthropic, and OpenAI agent credentials
- Corporate source code and review comments
- Private review notes and observer rules
- Local Git caches, the review-epoch catalog, and the SQLite review ledger
- Integrity of published review mutations

## Trust boundaries

1. GitHub.com or configured GHES host
2. BetterPullReview native process
3. Installed Git process and the retained, inactive GitHub CLI process boundary
4. Configured AI agent processes
5. Operating-system credential vault and trust store
6. Untrusted repository content, comments, commit messages, diffs, and theme files
7. App-owned bare repositories, private refs, and the review-epoch catalog

## In-process architecture boundary

The functional-core/host/Qt split is not presented as an operating-system security sandbox. It is a capability and authority boundary that reduces accidental credential flow and hidden effects:

- the core receives no credentials, sockets, process handles, filesystem handles, or provider clients;
- the Qt adapter receives projections and command contracts, not credentials or unrestricted service objects;
- credential-bearing transports and child-process execution remain host-owned capabilities;
- all effects are traceable through typed host requests and result events;
- no widget can bypass the mutation coordinator or select a remote target directly.
- no widget, Qt model, GitHub snapshot getter, or provider callback can publish
  an epoch, select a retained ref, or query the epoch catalog directly.

The portable native-vault contract admits at most 2,560 bytes of nonempty
credential material so every supported native vault can represent the valid
domain. The material has a noncopyable owner; movement empties the source,
replacement and destruction overwrite owned bytes, and the type offers no
text, formatting, comparison, hashing, logging, serialization, settings,
environment, or Qt conversion. Vault authority includes exact profile
revision, host identity, canonical hostname, account, and server contract;
async results also retain request, session, and identity generation.
Cancellation that wins suppresses retrieval success and destroys the returned
material before publication.

The BPR-0027 host never accepts an account or login as resume authority.
Credential-authenticated strict `/user` establishes the numeric account, and
a noncopyable host-owned catalog retains the resulting exact
profile/profile-revision/host/hostname/account/server key. The catalog has no
raw seed, enumeration, copy, move, serialization, settings, or SQLite
surface. Profile, hostname, account, and authentication-transport replacement
exact-delete the prior authority before proceeding; authoritative absence
forgets it; failed cleanup retains an exact tombstone.

That selector provenance is process-local. Sharing one catalog across host
instances proves same-process recreation, not restart recovery. The public
composition chooses exact deletion instead of durable rehydration: an exact
delete terminal for the retained authority is part of host quiescence, and
composition keeps the decision lane, vault, catalog, and workers alive until
it returns. A failed delete is reported and retains the exact tombstone.
There is no enumeration or durable selector, so an abrupt process or host
crash can leave a vault item that a later BetterPullReview process cannot
discover or resume.

Qt owns the native-HTTPS credential prompt. It uses password echo, bounds both
characters and UTF-8 bytes, returns a move-only sensitive byte owner, and
clears its mutable temporary on cancellation, rejection, conversion, and
destruction. Composition immediately converts that owner to the application
credential-material type. The prompt cannot access a vault, provider,
filesystem, process, or network capability, and no credential enters a Qt
projection or diagnostic.

The production host also creates its capability cache internally. A caller
can exercise the standalone cache library, but cannot inject populated
capability facts into session establishment or bypass the authenticated live
probe. Capability commit follows the core's exact accepted-session delta.

Provider and vault submissions are nonblocking and, after normal return,
produce exactly one terminal despite advisory cancellation. The decision
executor remains live, accepts every post, and invokes each accepted task once
until the host is quiescent. Duplicate or malformed terminals are still
contained defensively. Projection callbacks are observational: exceptions and
reentrant feedback cannot veto mandatory effects, exact cleanup, or FIFO
progress.

The current macOS adapter targets one explicitly resolved default file-based
Keychain. Every operation matches that exact keychain, a fixed adapter-owned
v2 service, and the 574-character hostname-bound encoding of the complete
portable authority, and disallows authentication UI. It does not search
arbitrary keychains or enable data protection, synchronization, access groups,
or accessibility attributes. There is no fallback to the historical v1
namespace, a second Keychain implementation, or plaintext storage. Raw
Security framework status numbers and messages do not cross the adapter
boundary.

Synchronous Security framework work runs through a host effect executor,
separate from the serialized decision and GUI lanes. The native integration
fixture uses a distinct adapter-owned service and exact synthetic authority,
passes runtime canaries only over standard input, audits bounded child
arguments, environment, output, executable, source, and scratch surfaces, and
verifies exact-item absence after same-binary parent and CTest cleanup.
BPR-0022's closeout evidence exercised the historical v1 namespace;
BPR-0029's current macOS run repeated setup, lifecycle, and cleanup against
the v2 hostname-bound namespace.

The current unbundled integration executable is ad hoc linker-signed. Its
legacy default-Keychain item ACL is therefore tied to an exact code-directory
hash. Same-binary cleanup is proven, but a host crash followed by rebuilding
the test executable can leave an orphan that the new binary cannot remove
noninteractively. This is not cross-rebuild crash-recovery evidence; stable
signed package identity and that lifecycle gate remain RFC-BPR-0014 work.

The Windows adapter selects one generic Credential Manager item under a fixed
v2 target prefix and the complete hostname-bound portable authority. Its
integration fixture uses a distinct v2 prefix and one exact synthetic
authority. Store is a
single create-or-replace call; read and delete use the exact target; valid
returned blobs are overwritten before native release. Credential UI,
enumeration, DPAPI, Credential Locker, registry, settings, SQLite,
environment, file, logging, and alternate-vault paths are absent.

Windows generic credentials are protected by the current user's Credential
Manager boundary, not by an executable-specific ACL. A same-user process that
knows the target can request the item. Local-machine persistence means later
logons of that user on the same computer, not all users, roaming, or hardware
backing. The target prefix limits accidental name collision but is not an
authorization boundary.

BPR-0023's implementation and cross-PE evidence covered the historical v1
target format. BPR-0029 changes the current source to v2, but no target-native
Windows claim is inferred from that edit; BPR-0036 retains the current-format
lifecycle and public-fixture run.

The Windows fixture passes runtime-only canaries over anonymous standard
input, starts the canonical test executable with an exact inherited-handle
list and explicit minimal environment, captures fixed output, bounds process
time, scans its isolated scratch tree, and requires exact parent and fixture
cleanup. These are prepared checks, not completed native evidence: no Windows
x64 SDK/runtime is available on the current macOS host. Cross-compilation and
static boundaries cannot verify operating-system storage, denial,
unavailable-session behavior, or cleanup. BPR-0036 therefore retains those
Windows target-native checks and the affected claim remains open.

The Linux adapter selects only the default Secret Service collection under
the literal canonical `org.freedesktop.secrets` service. It opens with
`SECRET_SERVICE_NONE`, validates the opened proxy name, and only then
establishes a Secret Service session. Dependency-level service redirection
therefore becomes typed unavailability before session or item work even
though production never reads or mutates routing environment. A live hostile
owner observed property inspection only and no `OpenSession`, collection, or
credential-item method. A derived `SecretService` denies prompt vfuncs and
makes at most one bounded, no-auto-start `Dismiss`; it never displays a
prompt, unlocks or creates a collection, or selects another vault.

Production and integration-test schemas are disjoint. Their only explicit
attribute is the non-secret 574-character hostname-bound portable authority;
libsecret adds `xdg:schema`. Credential bytes are stored as
`application/octet-stream`. Retrieval also accepts exact `text/plain` because
the directly tested gnome-keyring 50/libsecret 0.21.7 stack normalized that
metadata while preserving arbitrary opaque bytes. Exact attribute checks,
unique-item enforcement, collection/item lock checks, the 2,560-byte portable
bound, and typed unavailable, access-denied, corrupt, allocation, and
operation failures keep malformed or redirected storage from becoming a
credential result. No raw provider error or credential material crosses the
adapter.

The v2 Keychain service, Credential Manager target, and Secret Service schema
names are disjoint from their historical hostname-free v1 namespaces.
Production neither reads, enumerates, migrates, nor falls back to v1. The v1
production adapters were never publicly composed. Every directly executed v1
integration fixture completed exact cleanup; the Windows v1 fixture remained
preparatory and created no target-native item.

The historical BPR-0024 Linux native fixture used only the v1 integration
schema and a synthetic authority. In an Ubuntu 26.04 x86_64 full-system QEMU
VM with an emulated CPU, a dedicated real user ran it under Xvfb and a desktop
D-Bus Secret Service session. Runtime-only canaries entered the
same-executable child through framed standard input; arguments, the explicit
child environment, fixed output, executable, source, and isolated scratch
were checked. A separate opaque-byte lifecycle included NUL, high-bit, and
invalid UTF-8 bytes. Checked-in cleanup proved the exact fixture authority
absent; a controlled D-Bus audit separately proved integration-schema
absence. A truly locked default collection, no-session-bus unavailability,
and hostile service-name rejection were also checked. Access denial and
failed deletion are fake-backed/status-classification evidence rather than
desktop-policy fixtures. This proves the historical v1 vault surface in the
selected full-system Linux stack.

BPR-0029 separately verified the current v2 Linux fixture and recorded public
composition in an Ubuntu 26.04 x86_64 full-system QEMU guest; this does not
relabel the historical result. The public QAction/render path substitutes its
fixture transport and vault state, so it is structural composition evidence
rather than a native-network or native-vault lifecycle run. The v2 Secret
Service
setup/lifecycle/cleanup cases passed 3/3, independent exact cleanup passed,
and the hostile, unavailable, and locked cases remained fail-closed. Both
public fixtures completed with exact host and rendered state, and the
tests-disabled Release seam, dependency, runtime, and ELF audits passed.

## Baseline posture

- No hosted BetterPullReview service.
- No product-owned listening-socket or local-web-server implementation.
- No embedded browser, HTML renderer, JavaScript runtime, CDN, remote font, analytics, or telemetry.
- No general plugin system or remote theme/highlighter download.
- No repository code execution, hooks, submodules, filters, textconv, external diff, build, or test invocation.
- No credential sharing between BetterPullReview and AI agents.
- No automatic AI invocation or automatic AI publication.

The runtime-surface script supports this listener boundary only as a
point-in-time idle observation: it waits approximately 0.5 seconds, samples the
application's sockets and direct children once, and then terminates the
process. Together with the source boundary scan, that is evidence for the
implemented idle path on platforms where it was run. It is not continuous
monitoring, does not exercise active native-HTTPS mode or indirect descendants,
and does not substitute for the open Windows run in BPR-0036.

## Input handling

All API, Git, diff, syntax, Markdown, agent, and settings inputs are untrusted. Apply explicit size, nesting, time, allocation, and line-length limits. Render a safe Markdown subset with raw HTML disabled. Links require a user gesture and show the destination.

## Process handling

- Resolve executables explicitly.
- Invoke with argument arrays, never a shell.
- Use bounded stdin/stdout/stderr, timeouts, cancellation, and redaction.
- Pass prompts and request bodies through stdin or protocol messages, not command lines.
- Construct explicit child environments from approved names for sensitive
  processes; never inherit everything and then scrub known names.

Native HTTPS is the sole milestone-one production authentication transport.
Visible CLI selections return fixed typed `cli_unavailable` before transport
construction, CLI executable/configuration discovery, or process launch. The
retained CLI broker does not inherit and scrub the parent environment. It
constructs a new child environment from fixed noninteractive settings and
typed platform
credential-session, trust, and proxy facts. Its standard-C++ process request
contains explicit input/output/error/time/reap bounds; the retained
`QProcess` runner implementation uses one owner-managed worker and separate
captured channels but is not public-composed in milestone one. On Unix,
the child is created as a new session and initial
process-group leader; a compiled child directly observes session/group
identity, closed inherited descriptors, disabled core dumps, and reset signal
handlers. Compiled lifecycle cases prove cleanup signals descendants that
remain in that initial process group on clean exit, failure, timeout, and
cancellation while reaping the direct child, and an escape case proves a
descendant can survive after moving groups before explicit fixture cleanup. A
compiled seam proves completion-watchdog quarantine and recovery. Source and
hostile architecture gates additionally cover shutdown and group-resolution
paths and actual failed bounded reap, where the live process must remain on the
quarantined worker for continued reaping; the runtime suite does not force
that live-process path. Fixed-size public reads bound accepted and retained
bytes; Qt's private in-flight pipe allocation is not claimed as hard-bounded.
Allocation-free request-shape validation and pre-cancellation occur before
work-item allocation or request copying. Path, argument, environment, input,
stream, timeout, and reap N+1 requests fail without launching a child.
Executable and working-directory paths are canonical; at validation time the
working directory has neither a `.git` nor bare-repository ancestor. This does
not establish owner-controlled permissions or bind directory identity through
launch. Token, host/repository override, config override, debug,
pager/editor/browser, TTY/color override, update, spinner, prompt, and unknown
parent variables are not read.

Raw macOS child inspection permits only the requested facts plus Darwin's
deterministic `__CF_USER_TEXT_ENCODING` addition. The Linux raw-runner fixture
requests its locale explicitly; production does not add that fixture-only
fact. Windows Job Object descendant containment, private pipe bounds, and
direct target-native CLI verification are deferred to RFC-BPR-0015.
BPR-0036 retains only Windows native-HTTPS, vault, and composed-public-path
verification.

This mechanical boundary is not trust evidence for GitHub CLI networking.
The official `gh` 2.96.0 release dated 2026-07-02 was audited through an exact
owner-private copy of the macOS arm64 binary, SHA-256
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`;
the release ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`.
It produced a fixed bad-certificate alert before HTTP but followed a
cross-authority `302` and did not provide a safe final-authority result
surface. The installed `gh` 2.95.0 binary, SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun with the same material outcomes.

Persisted `http_unix_socket` is a demonstrated ambient routing input, not a
telemetry-only concern: both binaries loaded it and sent one authenticated
`gh api` request through the controlled AF_UNIX sentinel. The production
platform home/application-data facts would leave that setting in scope.
Canonical Linux `SSL_CERT_FILE`/`SSL_CERT_DIR` strings also do not bind trust
material through the CLI's later open. The current `gh api` has no
per-invocation `--user`; `gh auth token --user` exposes the token and
`gh auth switch --user` persistently changes active-account state. Selected
source resolves `ActiveToken` per round trip, can fall back from a keyring
error to a blank-host token, and launches `/usr/bin/security` for the macOS
keyring. None of these facts binds the invocation to BetterPullReview's
correlated identity generation.

The CLI boundary therefore reports eight typed reasons: milestone-one
product-scope deferral, executable-image identity, cross-authority redirect
behavior, ambient configuration, credential/account identity,
working-directory identity, descendant-process containment, and numeric
PID/process-group identity reuse. It returns fixed `cli_unavailable` with zero
process launches. Native HTTPS evidence and fake-child success cannot enable
it. RFC-BPR-0015 owns any future positive CLI contract and must directly close
every blocker without privileged helpers or weakened security.

## Network handling

- Send origin requests only to validated GitHub origins; bounded system-proxy
  endpoints may carry those connections.
- Require HTTPS and operating-system certificate validation.
- Restrict redirects.
- Respect approved system proxy configuration.
- Do not provide certificate-error bypass.
- A metadata-only network activity log is deferred to separately scoped work;
  RFC-BPR-0002 implements and claims no such log.

GitHub profile authority is established before transport execution. The
selected v1 validator accepts only trusted explicit/builtin/managed
provenance, canonical lowercase ASCII FQDNs, and the exact GitHub.com or GHES
3.18 HTTPS roots. Provider content and redirect targets cannot create or
rewrite a profile. GraphQL redirects are rejected; REST redirects require the
same validated authority, a query-free target, and the same API namespace.

The BPR-0026 native HTTPS wire transport confines `QNetworkAccessManager`,
requests, replies, TLS objects, and proxy objects to one dedicated adapter
worker. It revalidates the complete canonical request/correlation before
network use and accepts only one operation per transport at a time.
Bootstrap credentials authorize one exact-context viewer-identity request
and are destroyed once claimed. Account credentials must match the profile,
profile revision, host identity, canonical hostname, account, server contract,
application session, and identity generation. A mismatch cannot be repaired
from provider content and causes no network request.

Production proxy selection calls Qt's system query separately for every hop,
admits no more than 16 candidates, preserves supported direct/HTTP/SOCKS5
order, and falls back only after a retryable proxy or connection failure.
Manager-local proxy selection and cache clearing avoid process-global proxy
mutation. On generic Unix, the selected Qt system policy can use
libproxy/desktop sources or lowercase `http_proxy`, `https_proxy`, and
`no_proxy`; production code itself does not call `getenv`. Proxy
authentication is never filled with the GitHub credential; a proxy challenge
aborts that candidate and may select a later candidate. Origin
authentication, TLS, HTTP, malformed-response, and policy failures do not.
The validated profile controls the origin authority; a Qt-selected HTTP or
SOCKS5 proxy endpoint is a routing intermediary, not a GitHub authority.

TLS begins with an explicit operating-system root set, no local certificate or
private key, `VerifyPeer`, TLS 1.2 or later, HTTP/1.1 ALPN, disabled session
reuse, and no certificate-error bypass. `sslErrors` and pre-shared-key
challenges abort. Accepted terminals require the `QNetworkReply::encrypted`
completed-initial-handshake signal, a non-null peer certificate, no accepted
`sslErrors`, the exact requested URL, no HTTP/2 use, and the configured
verification/minimum-protocol policy. The scheme-associated
`ConnectionEncryptedAttribute` is an additional consistency check, not
standalone negotiated-session evidence. The meaningful evidence is the peer
certificate and completed-handshake signal under enforced
`VerifyPeer`/TLS-1.2-or-later policy, together with the trusted/untrusted
fixture outcomes. The reply surface does not directly record the negotiated
TLS version and must not be described as doing so.

Automatic redirects are off. A fresh credential-bearing request may follow
at most three redirects only for REST `GET` and only to a canonical,
query-free HTTPS path on the exact validated authority inside the API
namespace. GraphQL, cross-authority, looping, malformed, and fourth-hop
redirects fail closed. The loopback fixture verifies that Authorization
appears only inside the post-CONNECT TLS request and never in CONNECT itself;
the same random token is required on an accepted same-authority hop.

The adapter limits request bodies to 256 KiB, accepted response bodies to
4 MiB, response headers to 64 fields/64 KiB aggregate/8 KiB each, proxy
candidates to 16, redirects to three, and the cancellable transfer/event-loop
portion to one absolute 30-second deadline. Fixed-size reads request at most
the remaining accepted bound plus one. Successful responses require HTTP 200,
JSON content type, no content encoding, consistent content length, and
bounded pagination metadata. Every failure crossing inward is a fixed typed
category with no URL, header, provider body, TLS detail, proxy address, or
credential text.

These limits do not make Qt's private HTTP parser, TLS/socket buffers, or
bytes accumulated before `QNetworkReply` metadata exactly bounded. The
reply may normalize or collapse duplicate headers, notably `Location`, before
exposing raw pairs or redirect attributes. The adapter enforces the metadata
Qt exposes, not an exact on-wire Location count. The credential owner and the
transport's explicit mutable Authorization buffer are overwritten, but Qt may
copy the header into request/network/TLS/socket storage for which it exposes
no precise wipe operation. This in-process transport boundary reduces
credential reach; it is not a memory-isolation sandbox.

System/PAC proxy discovery and system-root enumeration are synchronous Qt
calls. The absolute deadline starts when the operation is admitted, is checked
around that preparation, and governs the subsequent request/event loop, but
it cannot interrupt a blocked system discovery call. Only the remaining time
is used after control returns. Cancellation queues an abort and polls the stop
token once an event loop is active; prompt cancellation and shutdown can
therefore wait for synchronous discovery to return.

The 15-case TLS-over-loopback-CONNECT fixture exercises trusted success,
ordered proxy fallback, accepted same-authority redirect, rejected
cross-authority and GraphQL redirects, certificate failure, the response-body
limit, one 8-KiB-plus-one header-field limit, cancellation before and during
I/O, timeout, HTTP 401/403/429, and content-encoding rejection. It has no
separate 65-field-count or aggregate-header-only scenario; those two adapter
checks remain source/contract evidence. It passed on the directly exercised
macOS arm64 host. The runtime fact probe reported Qt Network 6.11.1, the
OpenSSL backend, TLS support, and one direct system-proxy candidate. A separate
test-disabled build omitted the test-only proxy/CA factories. This fixture
verifies the internal wire boundary; BPR-0027 separately verifies the
application-facing provider and host, and BPR-0029 supplies recorded public
composition traces on macOS and Linux using a substituted fixture transport.
Those traces are not real-network evidence. The Linux trace passed both
exact-host rendered fixture states, an exact delete in fixture-vault state,
and tests-disabled Release selection/seam inspection. Separate target-native
vault lifecycle checks establish native item/schema cleanup.

The corresponding Linux verification used an Ubuntu 26.04 x86_64 full-system
QEMU guest with Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. It froze
`native_https_transport.cpp` at SHA-256
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`
and the harness at
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The focused contract/fixture passed 2/2 in 0.12/11.29 seconds and all 15
scenarios passed directly; the architecture matrix passed 2/2 in
5.00/144.35 seconds with all 92 hostile mutations. The uninstrumented
focused suite passed repeatedly. Runtime
reported
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
The ELF directly needs Qt6Network and Qt6Core. A controlled trace of one
trusted-success case under `strace` opened the Qt OpenSSL backend plugin,
`libssl.so.3`, and `libcrypto.so.3`. A full-suite `strace` is explicitly not
evidence because instrumentation perturbs cancellation timing; the trace
establishes only single-success dependency selection, while uninstrumented
runs establish behavior. Tracked and on-disk scans found no GitHub Actions
workflow file, and the source/build configuration selected no GitHub Actions
integration.

Windows target-native execution remains BPR-0036 work. It is nonblocking for
all other implementation, security, documentation, simplification, and
later-RFC work; it blocks only affected Windows claims and final
RFC-BPR-0002 Auto-Verified closeout.

Transport status alone is deliberately not a complete rate-limit
interpreter. HTTP 403 with zero remaining or bounded `Retry-After`, and HTTP
429, receive typed rate-limit outcomes. BPR-0027 adds bounded structured
GraphQL HTTP-200 error and body-defined secondary-limit classification,
resource-scoped accumulation, and exact expected-`HeadOid` binding for
`ListChecks`.

Capability introspection is untrusted input. One allowlisted read-only query
is subject to response-byte, depth, member, collection, string, and aggregate
value bounds. Duplicate names, partial data, GraphQL errors, malformed type
wrappers, permission failures, and transport failures yield explicit
all-Unknown unavailability rather than support or a misleading unsupported
claim. Only complete schema compatibility evidence enters the
profile/host/account/server/probe-contract cache.

## RFC-BPR-0003 Git cache and epoch boundary

### Accepted authority and account isolation

The epoch host receives one typed `accepted_pull_request_revision` only after
the GitHub core emitted the exact `accepted_snapshot_delta` and the GitHub
host committed that snapshot. It is not derived from Qt state or a public
snapshot getter. The handoff carries the complete application session,
profile identity/revision, Git host/canonical hostname/server contract, GitHub
identity generation, authenticated account/exact viewer actor, repository
identity/validated locator, pull-request identity/number/generation, provider
snapshot identity, algorithm-tagged base/head binary OIDs, opaque base-branch
identity, the one bounded newest-current-viewer review candidate with typed
provider order, and an optional opaque native smart-HTTPS lease. It contains
no credential, URL, provider record, or raw branch/ref string.

When any authority component changes, the GitHub host revokes the lease,
sends `pull_request_authority_invalidated`, and only then clears the old
accepted authority and identity-sensitive projection. Epoch terminals retain
their original context and cannot become current by comparing only repository
text, PR number, or OID. Handoff failure cannot roll back an already accepted
GitHub snapshot, while an epoch is never cataloged or projected until its own
core has accepted the exact staged result.

Milestone one does not share a physical object database across accounts or
profile revisions. Every exact profile/revision, host, authenticated account,
canonical repository, and object-format partition receives a
catalog-generated random 256-bit `repository_cache_scope_id`. Its app-owned
bare repository is
`repositories/v1/<64-lowercase-hex-repository-cache-scope-id>.git`.
Each pull request inside that cache separately receives a random 256-bit
`pull_request_authority_scope_id`, whose lowercase 64-hex rendering qualifies
retained refs, epoch ordinals, aliases, paging tokens, and selection and
deletion requests. Both IDs are opaque, nonreversible, unique before use, and
never accepted from a caller.

Physical possession is not authorization. Catalog/cache reads require a
currently accepted matching account authority and expected catalog revision;
there is no offline or restart exposure before reauthentication. A caller
cannot enumerate cache or PR scope IDs or request an arbitrary object by OID.
Invalidation immediately removes visibility. A catalog/ref scope mismatch,
algorithm mismatch, or stale generation is corruption or staleness, never a
cache hit. Provider clone URLs, locator casing, login text, and raw `HeadRef`
values do not select storage or authority.

### Native smart HTTPS and installed Git

Git smart HTTP is a separate revocable native capability, not the existing
JSON `github_wire_transport_port`. It compiles only the accepted repository's
exact discovery
`GET /OWNER/REPOSITORY.git/info/refs?service=git-upload-pack` and upload-pack
`POST /OWNER/REPOSITORY.git/git-upload-pack`. The credential-bearing adapter
owns the request, Authorization, validated origin, system trust/proxy policy,
TLS, streaming, and redacted terminal. Callers have no generic URL, header,
method, credential, or redirect surface.

The discovery query is protocol-mandated on the original URL. Automatic
redirects are disabled, and every redirect fails closed, including a
same-authority or query-bearing target. Cookies, HTTP cache, transparent
content decoding, and cross-lease credential reuse are disabled. Service
advertisements, content types, pkt-lines, sideband data, and packs are
streamed with explicit wire, memory, temporary-file, repository-quota,
deadline, cancellation, and backpressure ceilings rather than the JSON
transport's 4-MiB response-body rule. Advertised and received object formats
and exact base/head OIDs must match the accepted revision, and complete
connectivity is checked before staging.

An app-owned minimal protocol-v2 client emits exactly the validated accepted
base and fixed pull-request-head `ls-refs` prefixes. It requires one matching
advertisement for each, rejects every other ref, and sends only the
deduplicated accepted base/head `want` set. A bounded `have` may name only a
catalog-visible exact object in the same repository cache scope. Its bounded
advertisement parser may recognize known optional GitHub/GHES capabilities
that remain unused. The client never requests, echoes, or exercises thin
packs, URI/bundle retrieval, object filters, shallow history, server options,
session identifiers, or any other alternate retrieval or execution path; an
unrequested feature appearing in a request or response fails closed.
Installed Git never parses the service advertisement or constructs an
upload-pack request.

The installed Git child is only a credential-free local pack/object/ref
engine. Its executable image, matching exec path, and app-owned bare
repository are resolved and validated before launch. The received pack is
first bounded into the exact private request-owned
`incoming/v1/<operation64>.pack.part` spool. The host validates and durably
records its pack identity, pre-import pack inventory, and preexisting-pack
fact before streaming that spool to this fixed import shape with the validated
numeric bound:

```text
git -c core.fsync=all
    -c core.fsyncMethod=fsync
    --git-dir=<validated bare cache>
    index-pack
    --stdin
    --strict
    --check-self-contained-and-connected
    --max-input-size=<validated pack-byte bound>
    --threads=1
    --no-rev-index
```

`fetch-pack`, `--keep`, `--fix-thin`, network subcommands, arbitrary
arguments, and a shell are not admitted. Exact bounded output and on-disk
pack/index paths are validated before staging refs are created, then the spool
is exact-deleted. A crash-recovery path can therefore identify a newly created
pack before deleting it; a preexisting or ambiguous pack is never deleted.
The child environment is constructed from nothing, selects app-owned empty
system and global configuration/template roots, and admits only the exact
validated app-owned local bare-repository configuration. Command overrides
and source boundaries exclude credential helpers, askpass, hooks, filters,
LFS, submodules, textconv, external diff, alternates, replace objects,
SSH/file/ext protocols, editors, pagers, prompts, tracing, maintenance, and
repository-supplied executables.

No credential, protocol request, advertisement, URL, proxy, certificate,
trust path, provider payload, remote ref, or ambient account authority reaches
the child. Standard input, output, error, duration, cancellation, descendant
containment, reap, and shutdown are bounded. Platform-specific Windows
process containment remains an explicit target-native verification task and
does not block the other architecture or implementation slices.

### App-owned bare repository and private refs

Only a versioned app-data directory with validated ownership and permissions
may contain the bare cache and catalog. Every path component is opened without
following symlinks, junctions, or reparse points and is revalidated at use.
Repository manifest, bare status, object format, configuration, alternates,
shallow/graft state, worktrees, hooks, and ownership must match the catalog.
Drift fails closed before installed Git or SQLite use. No repository remote,
wildcard refspec, tag fetch, worktree, provider-controlled path, or
repository-controlled executable is created.

BetterPullReview generates refs from this fixed grammar:

```text
refs/betterpullreview/v1/stage/<operation64>/base
refs/betterpullreview/v1/stage/<operation64>/head
refs/betterpullreview/v1/scope/<pr-scope64>/epoch/<ordinal16>/base
refs/betterpullreview/v1/scope/<pr-scope64>/epoch/<ordinal16>/head
```

`operation64` is a host-issued lowercase 64-hex epoch operation identity,
`pr-scope64` is the catalog-issued lowercase 64-hex PR authority scope, and
`ordinal16` is the fixed-width lowercase 16-hex encoding of a nonzero ordinal
no greater than `INT64_MAX`. Only the terminal literal `base` or `head` is
admitted. Every generated name is round-tripped through Git's ref-format
validation and must match the same catalog authority. Branch names, PR
numbers, `HeadRef`, log text, OIDs, and caller strings never contribute a ref
segment. Base and head refs are prepared and committed with
`git update-ref --stdin -z --no-deref` start/prepare/commit transactions,
exact expected old values, no reflogs, and the one-writer repository lock.
No glob reaches Git. This uses Git's official
[`update-ref` transaction contract](https://git-scm.com/docs/git-update-ref).

### SQLite catalog and ref publication

RFC-BPR-0003 owns a small SQLite epoch catalog separate from the future
RFC-BPR-0007 review ledger:

```text
state/v1/epoch-catalog.sqlite3
```

It has `application_id = 0x42504543` (`BPEC`), `user_version = 1`, and one
nonzero random 256-bit catalog incarnation. Its typed `STRICT` schema contains
schema/incarnation facts, repository cache and PR authority mappings, checked
next ordinals and catalog revisions, epochs, review associations and order,
retained-ref identities, availability/tombstones, and typed append, prune, and
cache-delete operation state. Named aliases are core policy resolved from
these exact facts, not a caller-editable or independently authoritative alias
table. Composite-identity tables use `WITHOUT ROWID` where applicable.

The catalog does not contain credentials, Authorization data, source bodies,
patches, comments/review bodies, pack data, raw provider payloads, logins,
hostnames, repository names, clone URLs, generic JSON payloads, or unrestricted
diagnostics. It admits no views, triggers, `ATTACH`, or caller-provided SQL.
OIDs are stored as algorithm plus binary digest.

The catalog opens only its fixed validated app-owned local path through
[`sqlite3_open_v2`](https://sqlite.org/c3ref/open.html), with read/write,
create, serialized mutex, private-cache, and no-follow behavior and without
URI filename interpretation. Loadable extensions and shared cache are
disabled. The connection enables
[`SQLITE_DBCONFIG_DEFENSIVE`](https://sqlite.org/c3ref/c_dbconfig_defensive.html),
`trusted_schema=OFF`, foreign keys, a fixed nonzero
[`application_id`](https://sqlite.org/pragma.html#pragma_application_id) of
`0x42504543`, and an exact
[`user_version`](https://sqlite.org/pragma.html#pragma_user_version) of `1`.
A single serialized writer uses prepared statements and bounded values.
Local [WAL](https://sqlite.org/wal.html) mode with
[`synchronous=FULL`](https://sqlite.org/pragma.html#pragma_synchronous) is
required; a network filesystem is not supported.

The exact vendored SQLite release, source checksum, compile options, and
platform build proof are deliberately selected and reviewed in the
implementation/dependency card. This architecture does not pin an unreviewed
release or digest. Whatever release is selected must directly support the
open flags and defensive configuration above, and the dependency record must
prove those options in the built artifact.

SQLite is the publication authority; refs alone are not catalog records.
The host performs one recoverable two-resource protocol:

1. Under the exact cache writer lock, `BEGIN IMMEDIATE` verifies authority,
   catalog incarnation/revision, and quota; reserves a never-reused ordinal;
   and records a typed `preparing` operation with exact generated staging and
   final refs, then commits before network work.
2. Spool and validate the bounded pack, then durably add its exact identity,
   bounded pre-import inventory, and preexisting-pack fact to the operation
   before `index-pack`.
3. Import and verify only the accepted objects, create the paired staging refs
   with one expected-value ref transaction and the full-durability policy,
   then exact-delete the spool.
4. Return only bounded lineage, tree, alias, and catalog facts to the
   serialized decision lane.
5. After the epoch core gives exact commit authorization, a catalog
   transaction rechecks every generation/revision and stores the complete
   hidden candidate as `commit_authorized`. Cancellation may exact-abort the
   candidate and staging refs only before the next phase.
6. Durably record `ref_commit_in_progress`, then use one ref transaction to
   verify both staging OIDs and both absent final refs, create the final pair,
   and delete the staging pair. Cancellation can no longer report a false
   cancellation; recovery owns the terminal outcome.
7. In one catalog transaction, check host-supplied exact ref/object
   inspection, insert the visible epoch and review associations, advance the
   catalog revision exactly once, and mark cleanup. An aborted ordinal is
   never reused.
8. Only the matching committed terminal may update core selection or publish
   the epoch projection.

An association-only review update uses one expected-revision SQLite
transaction and no pack, Git, or ref phase.

Recovery takes the same lock and compares typed operation phase, catalog row,
and exact ref/object values:

- an exact operation-owned spool may be resumed or deleted;
- an interrupted import may delete only a recorded nonpreexisting pack/index
  and operation-attributable temporary files when no staged, final, or
  catalog ref can reach them;
- `preparing` or `commit_authorized` with no final refs and the exact staging
  pair is resumed or exact-aborted only as recorded;
- `ref_commit_in_progress` with no final refs and the exact staging pair
  retries the fixed ref transaction and finalizes;
- an exact final pair is object-validated and finalized exactly once;
- a committed row with exact staging residue exact-cleans only that residue;
- mixed, missing, unexpected, or mismatched final/staging/object state becomes
  `repair_required` and makes the affected scope unavailable; and
- a committed epoch with missing or mismatched refs retains its identity as a
  corrupt tombstone and is never silently refetched from mutable history.

An unreferenced, well-formed staging pair may be removed under the lock because
it was never retained history. A preexisting, unrecorded, reachable, or
ambiguous pack is never removed. Recovery never deletes another ref or object,
never guesses, and never exposes a scope before fresh matching authenticated
authority is accepted. Schema drift, SQLite integrity failure, or
repository-manifest drift also fails closed.

### Quotas, pruning, and deletion

Wire, temporary-storage, repository, object, ref, epoch, catalog-row, text,
page, and transaction counts have hard configured ceilings. Exhaustion fails
the candidate without publishing and without evicting accepted history.
There is no TTL, LRU, automatic epoch pruning, or automatic Git maintenance
that can discard a retained object. Paged history is snapshot/revision bound;
a stale cursor is rejected rather than continued against a different catalog
revision.

Ref pruning releases a retention root and may reclaim the logical epoch quota,
but milestone one claims no immediate physical-byte reclamation without Git
GC. Exact whole-cache deletion is the only guaranteed repository-byte
reclamation path.

Prune and delete are explicit exact-scope commands with a user-visible
consequence summary. Pruning an epoch uses the same durable-intent/ref/catalog
protocol, removes only its paired refs, and records the retained anchor/epoch
identity as a pruned tombstone. Any named alias whose semantic candidate is
that epoch becomes explicitly unavailable and never remaps to an older
resident epoch. Repository-cache deletion enumerates and durably records every
affected PR scope in the one exact account/profile cache partition and cannot
touch another partition. Handles are closed, the exact validated directory is
atomically renamed into a transaction-named app-owned quarantine,
cache-deleted tombstones and revisions are finalized, and only that
quarantined path is removed. A failed or interrupted physical delete remains
a retryable unavailable tombstone and is never rediscovered as a cache.
Secure erasure from WAL/journal files or SSD media is not claimed.

Credentials and sensitive source content are absent from ref names, SQLite,
process arguments and environments, diagnostics, logs, crash artifacts,
fixtures, and verification output. Security tests use synthetic repositories
and canaries and inspect both tracked and on-disk surfaces. No live private
repository or GitHub mutation is required.

## Security evidence

Required release evidence includes threat-model review, SBOM, dependency inventory, secret scan, static analysis, sanitizers, parser fuzzing, hardened build flags, signed package verification, mutation-target tests, and diagnostics redaction tests.

GitHub Actions is not a build, test, package, release, or security-verification
surface for BetterPullReview. Native transport evidence is produced by local
and target-native CMake/CTest runs, loopback fixtures, dependency inspection,
and runtime observation.
