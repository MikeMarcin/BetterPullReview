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
3. Installed Git process and the opt-in GitHub CLI process boundary
4. Configured AI agent processes
5. Operating-system credential vault and trust store
6. Untrusted repository content, comments, commit messages, diffs, and theme files
7. App-owned bare repositories, private refs, and the review-epoch catalog
8. Delivered package/dependency/signing provenance and separate source assets
9. Profile maintenance, local activation IPC and the qualified macOS updater

The [corporate security packet](../security/corporate-security-overview.md)
consolidates the current network, data and process inventory. Historical component
observations below retain their candidate-specific limits; they do not establish
RFC-BPR-0014 release acceptance.

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
raw seed, enumeration, copy, move, or caller-provided account surface.
BPR-0541 adds an application-owned metadata store for exact device-grant
bindings, while the in-memory catalog remains noncopyable. Profile, hostname,
account, and authentication-transport replacement
exact-delete the prior authority before proceeding; authoritative absence
forgets it; failed cleanup retains an exact tombstone.

The owner-approved device policy retains a settled vault pair across orderly
quit and records only its exact authority, provenance and issuance/expiry facts
in `state/v1/device-bindings.sqlite3`. Startup selects the compiled native
GitHub.com profile and passes its stored authority through the existing strict
profile/revision/hostname and authenticated `/user` account checks. Invalid
schema, duplicate identities, backward or expired lifetime facts, missing
material and mismatches cannot establish a remembered session. No enumeration
or arbitrary account selector is introduced.

Not establishing a session is not erasing the grant ([[BPR-0644]]). Under the
owner's rule of 2026-09-24, a launch whose keychain is locked or refuses,
whose access slot is missing, whose network is down, or whose token or refresh
material the server refuses keeps both slots and the live row, and fails
visibly. Nothing resumes from the kept material until the next launch
validates it again through the same checks. Only Sign out or a replacing
sign-in removes it. Material that signs in as an account the binding never
named is kept too, by the owner's decision of 2026-09-24 on BPR-0644. The
residual is accepted: until the person acts, the slot keeps a credential that
answers for someone else, under a key that names their own account. Nothing
uses it. The session is refused before any read, a renewal's rotated access
token for the other account is never stored, and the Accounts page shows the
mismatch as an error with Sign in and Sign out beside it. The same holds
while the application runs ([[BPR-1116]]). An accepted session whose renewal
gives up ends visibly and keeps its grant. A restored session whose refresh
slot is missing stays signed in on its access token without renewing.

Before replacement or erasure, the stored binding is durably tombstoned. Only
a settled replacement pair can become resumable. Sign out and manual-token
replacement exact-delete access and refresh slots; manual sessions are deleted
on orderly quit. Cleanup failures remain visible, and completion/metadata
effects count through quiescence while their ports and decision lane stay alive.
A crash during an incomplete grant can still leave unresumable vault material;
there is no broad discovery or deletion scan. The composed restart course uses
a runtime-only fake vault and reopened SQLite; it does not claim native vault
continuity across operating-system processes.

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
- No TCP/UDP server or local web service. The existing owner-authenticated,
  owner-private AF_UNIX profile activation listener (on Windows, the equivalent
  owner-private local named pipe, [[BPR-0979]]) is the sole declared listener
  exception under RFC-BPR-0014 mandate r3; it admits only bounded activation
  intents under the exact profile lease. It grants no network-service capability.
  On Windows the sender also passes its foreground-activation right to that
  authenticated owner process and to no other, so the owner can bring its own
  window forward ([[BPR-0980]]). That is a user-interface permission Windows
  withdraws at the person's next input elsewhere; it carries no data or command.
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

### A failing subprocess may explain itself

The Git application port carries one bounded, redacted diagnostic member. This is
a narrowing of a rule that previously forbade the port any text at all, and the
reason for writing it down is that two other documents in this directory argue
the opposite for their own destinations, and the difference between them is not
obvious from either one.

`review-queue.md` says a notification carries no source and no comment body,
because the operating system's notification store outlives the application, is
readable by other software, and may be synchronised to the user's other devices.
`ai-agent-boundary.md` says a supplement carries only explicitly selected notes,
because it goes to a tool the reviewer did not write. Both rules are about
**repository content leaving the product**, and both reject redaction as the
remedy on the same grounds: a redaction step is a place a silent mistake can
live.

A git diagnostic is not that. It does not leave the product — it travels from an
adapter to the application that invoked it, and reaches the operator who caused
it. It is not repository content; it is the subprocess describing why it refused.
And "use bounded stdin/stdout/stderr, timeouts, cancellation, and redaction" is
already the standing instruction directly above, so redaction here is the rule
rather than an exception to one.

What forbidding it cost was concrete: a pack-write failure three layers down
surfaced to the operator as "agent failed to start", and diagnosing it needed an
instrumented build to recover text the process had already printed.

The narrowing is bounded in four ways, and `cmake/VerifyArchitectureBoundaries.cmake`
enforces the first three mechanically:

- exactly one text member on the port, so a second cannot arrive unnoticed;
- collections stay forbidden — an argument or environment list is authority, not
  diagnosis, and text and collections are now separate rules precisely so that
  permitting one cannot permit the other in a single edit;
- the named-authority patterns still apply to that member, so a URL, credential,
  authorization, proxy, or certificate field is rejected by name whatever its
  type;
- remote URLs are redacted in the adapter before the text reaches the port. Refs
  and local paths are not, deliberately: they are the diagnosis.

Product-owner ruling, BPR-0348, 2026-08-19.

## GitHub CLI opt-in risk boundary

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a Connect warning,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; account discovery and API use require `gh` 2.x with minor >= 96
because the account inventory uses `gh auth status --active --json hosts`.
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
change global authentication. Full Git comparison currently requires
`gh` 2.100.0 or 2.101.0 and Apple Git 2.50.1 (Apple Git-155), with a 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold; the
aggregate threshold is not a hard disk quota. Local refs reflect the accepted
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
a separate exact-host/account/PR/operation confirmation for each ordinary write, and
server reconciliation. The configured gh account may still change between
checks; an ambiguous attempted write is not posted twice. Fixture actions
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

BetterPullReview does not extract or copy CLI tokens or change global CLI
authentication. Opt-in trusts the machine's configured executable, configuration,
socket routing, CA and redirect behavior, active account and helper processes.
Pre/post observations can detect some account changes but cannot bind a credential
generation or prevent a concurrent account switch. Executable/configuration/CA
replacement and process escape remain uncontained risks. Read opt-in grants no
publishing authority. RFC-BPR-0013 keeps a 250-record limit for commits returned by the pull-request
endpoint. A larger reported total is accepted only when that endpoint
returns exactly 250 records ending at the accepted head. The UI names the
reported total and returned count separately; this does not establish that
any particular private pull request can render.

### Historical RFC-BPR-0002 zero-launch evidence

The following records the original milestone and its superseded enablement
prohibition. Its findings remain residual risks under the amended opt-in contract.

Native HTTPS was the sole RFC-BPR-0002 milestone-one production authentication transport.
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

The redirect and system-trust containment below applies to native HTTPS.
The explicit CLI profile requests the validated host but delegates actual
redirect, socket and certificate behavior to configured `gh`, with the
uncontained risks described above.

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

The epoch host receives one typed `accepted_pull_request_handoff` only after
the GitHub core emitted the exact `accepted_snapshot_delta` and the GitHub
host committed that snapshot. It is not derived from Qt state or a public
snapshot getter. The handoff carries the complete application session,
profile identity/revision, Git host/authority fingerprint/canonical hostname/
server contract, GitHub identity generation, authenticated account/exact
viewer actor, repository identity/validated locator, pull-request identity/
number/generation, provider snapshot identity/typed revision origin, object
format and algorithm-tagged base/head binary OIDs, opaque base-branch identity,
its validated typed base ref, the one bounded newest-current-viewer review
candidate with typed provider order, and an optional opaque native smart-HTTPS
lease. It contains no source-observation identity, observed time, credential,
URL, provider record, or unvalidated branch/ref string. The epoch host obtains
source identity and time from its operating-system entropy and clock ports
after durable recovery.

The independent pull-request issue timeline read is bounded to ten REST
pages of at most 100 records each. It is admitted anonymously only under the
validated public GitHub.com read authority, carries no viewer token, and
cannot replace the accepted live revision on failure, refusal, cap or
cancellation. The epoch host accepts only typed numeric force-push event ids,
validated after-head OIDs and UTC event times. Portable occurrence evidence
does not authorize arbitrary Git objects: comparison still requires an exact
local retained epoch and the accepted cache authority.

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

Catalog availability is not accepted as physical object proof. The BPR-0045
host directly inspects every candidate commit before admitting a smart-protocol
`have` or resolving a named or explicit comparison. Missing or mismatched
objects are omitted or downgraded to unavailable; a catalog row cannot cause
an arbitrary OID to be read or negotiated.

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
content decoding, and cross-lease credential reuse are disabled. GitHub.com
smart Git uses `github.com`, not the `api.github.com` API authority; GHES uses
the exact configured enterprise hostname. Discovery and `ls-refs` control
replies are read in fixed chunks but retained only within a 64-KiB whole-reply
ceiling before parsing. Fetch response bytes are read in chunks of at most 16
KiB and streamed directly to the incremental pkt-line/sideband consumer.
Wire, packet, progress, pack, temporary-file, repository-quota, cancellation,
and backpressure ceilings are separate from the JSON transport's 4-MiB
response-body rule. The production smart path has a 15-minute total deadline
and a 30-second no-progress idle deadline. Advertised and received object
formats and exact base/head OIDs must match the accepted revision, and
complete connectivity is checked before staging.

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
`incoming/v1/<operation64>.pack.part` spool. BPR-0043's cache/ref contract
validates the completed spool and exposes opaque pack-receipt and pre-import
inventory facts, but does not durably record them. BPR-0044 persists the pack
identity, pre-import pack inventory, and preexisting-pack fact. The BPR-0045
host records those exact facts before authorizing streaming to this fixed
import shape with the validated numeric bound:

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
pack/index paths are validated before staging refs are created. The BPR-0045
host consumes BPR-0044's exact ownership facts in its import, staging, commit,
and recovery sequence; cleanup exact-deletes only matching operation residue.
BPR-0043 alone does not prove that recovery.
The child environment is constructed from nothing, selects
app-owned empty system and global configuration/template roots, and admits
only the exact validated app-owned local bare-repository configuration.
Command overrides and source boundaries exclude credential helpers, askpass,
hooks, filters, LFS, submodules, textconv, external diff, alternates, replace
objects, SSH/file/ext protocols, editors, pagers, prompts, tracing,
maintenance, and repository-supplied executables.

No credential, protocol request, advertisement, URL, proxy, certificate,
trust path, provider payload, remote ref, or ambient account authority reaches
the child. Standard input, output, error, duration, cancellation, descendant
containment, reap, and shutdown are bounded. Platform-specific Windows
process containment remains an explicit target-native verification task and
does not block the other architecture or implementation slices.

BPR-0042 implements the local child boundary and secure completed-spool
consumption. Commit `b65b0a9` hardens that boundary with exact cleanup
identities and process-boundary serialization. BPR-0043 has committed the
closed protocol-v2 acquisition in `d903290` and native smart-HTTPS lease and
wire path in `edd762e`. Commit `2801241` completes its cache writer, spool
producer, pre-import observations, installed-pack reconciliation, and private
retained refs. Its direct contract binds repositories, held locks, spools,
pack/index artifacts, and delete tombstones to exact identities; refuses
link, mount/topology, manifest/configuration, cross-authority, quota, and
substitution ambiguity; and preserves state when cleanup ownership cannot be
proven. Abort/finish and import terminal races are serialized and
stop-aware. The cache contract passed 1/1 in 162.58 seconds, the adjacent
boundary replay passed 7/7 in 92.95 seconds, and the corrected-snapshot
independent review completed cleanly. A tests-disabled Release artifact
contained zero cache fault-injection markers.

The cache port exports no filesystem path. Its versioned, scheme-tagged
fixed-size artifact facts bind the completed spool and installed pack/index
to roles and exact sizes, and its prepared result reports the validated head
relationship to the prior retained head. A canonical bounded pre-import
inventory records whether the expected pack already existed. These facts are
serializable recovery inputs; their accompanying opaque authorization tokens
are only live-session state and are not durable evidence.

BPR-0044 durably stores the exact completed-spool, expected-pack,
pack-inventory, preexisting-pack, and installed head-lineage facts. BPR-0045
commits those facts before local import and enforces the remaining
ref/catalog/recovery order through the dedicated host component. BPR-0047
selects the same native smart-transport, credential-free installed-Git,
cache, catalog, host, core, and Qt path in the public application. Its
two-process GitHub.com and GHES fixtures prove exact retained-cache reuse;
BPR-0048 supplies the broader fault matrix, and BPR-0053 retains target-native
Windows execution.

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

The v1 schema is closed: 18 `STRICT, WITHOUT ROWID` tables and 16 indexes are
matched by type, name, and canonical SQL on every open. The adapter also
verifies the singleton incarnation, `integrity_check(1)`, and
`foreign_key_check`. Operation-detail tables carry a fixed kind and a
composite foreign key to a parent operation of that same kind, preventing a
wrong-kind detail row from acquiring authority. Each epoch also carries the
exact nonzero 256-bit source-observation identity supplied by the host, and the
catalog enforces uniqueness across published epochs and every durable append
operation within one catalog incarnation. Indexed read-before-dispatch and
transactional reservation checks classify source-observation collisions
without scanning epoch history. A retention replacement must not reuse an
identity even when its semantic provider revision and Git anchors correspond
to the cache-deleted
latest epoch; there is no source-identity reuse exception. BPR-0045 implements
operating-system CSPRNG generation for every accepted observation and bounded
retry after a catalog-reported collision. Deterministic catalog/host fixtures
prove persistence, restart behavior, collision rejection, and retry ordering;
native runtime-port evidence proves the production source independently of
those fakes.
Because RFC-BPR-0003 is still open, this definition supersedes earlier interim
development catalogs whose canonical operation-source or install schema
predates these amendments; those transient catalogs fail closed and must be
recreated rather than silently migrated.

The catalog does not contain credentials, Authorization data, source bodies,
patches, comments/review bodies, pack data, raw provider payloads, logins,
hostnames, repository names, clone URLs, generic JSON payloads, or unrestricted
diagnostics. It admits no views, triggers, `ATTACH`, or caller-provided SQL.
OIDs are stored as algorithm plus binary digest.

The catalog opens only its fixed validated app-owned local path through
[`sqlite3_open_v2`](https://sqlite.org/c3ref/open.html), with read/write,
extended-result-code, serialized-mutex, private-cache, and no-follow behavior,
without `SQLITE_OPEN_CREATE` or URI filename interpretation. First creation is
a separate secure bootstrap that pre-creates a private file, initializes and
validates it, checkpoints it, and publishes it without replacement. Loadable
extensions, shared cache, and attachments are disabled. The connection enables
[`SQLITE_DBCONFIG_DEFENSIVE`](https://sqlite.org/c3ref/c_dbconfig_defensive.html),
`trusted_schema=OFF`, foreign keys, a fixed nonzero
[`application_id`](https://sqlite.org/pragma.html#pragma_application_id) of
`0x42504543`, and an exact
[`user_version`](https://sqlite.org/pragma.html#pragma_user_version) of `1`.
A runtime authorizer restricts reads and mutations to the 18 exact tables and
admits only the required aggregate/scalar functions and transaction actions.
Views, triggers, writable schema, memory mapping, SQLite worker threads, and
caller-provided SQL are unavailable. A single serialized writer uses prepared
statements and bounded values. Local [WAL](https://sqlite.org/wal.html) mode with
[`synchronous=FULL`](https://sqlite.org/pragma.html#pragma_synchronous) is
required; a network filesystem is not supported.

BetterPullReview builds verified, download-only SQLite 3.53.4 source as one
static library without a system fallback. After cache validation, only the
verified `sqlite3.c` and `sqlite3.h` are copied into a controlled build-tree
staging directory; unverified cache siblings cannot shadow platform headers or
enter the compiler search path. The exact `SQLITE_SOURCE_ID`, official archive
and source SHA3-256 values, repository intake measurement, and compile contract
are recorded under `third_party/sqlite/`. The runtime verifies the
security-relevant compile options before admitting the catalog.

The main database has an exact 4,096-byte page size and 262,144-page ceiling,
so it cannot exceed 1 GiB. WAL uses a 256-page automatic-checkpoint threshold;
that threshold is not a hard WAL-file-size promise. The connection also sets a
2-second busy timeout, disables attachments and memory mapping, and applies
fixed SQLite limits to values, SQL, columns, expression depth, compound
selects, virtual-machine operations, function arguments, patterns, variables,
triggers, and workers. Product-level requests admit at most 100 history or
recovery rows, 100 exact anchors, 100 prune targets, 10,000 cache-delete PR
scopes, and 100,000 retained epochs per PR. Exhausted page or logical quota
maps to a typed `quota_exceeded` result.

On POSIX, the canonical storage root and `state/v1` directories must be owned
by the effective user at mode `0700`; the database and admitted sidecars must
be owner `0600`, regular, single-link files. macOS requires a local
`MNT_LOCAL` filesystem. Linux admits only the configured ext4, XFS, Btrfs,
tmpfs, F2FS, and overlay set. Bootstrap and orphan cleanup use no-follow,
descriptor-relative, identity-checked operations and delete only the exact
random bootstrap grammar before atomic no-replace publication.

Linux keeps the private root, state, and version directory descriptors alive
for the catalog lifetime. A private per-catalog VFS preserves only the exact
`/proc/self/fd/<retained-version>/<single-name>` spelling through SQLite full-
pathname resolution; main, bootstrap, WAL, SHM, and journal opens therefore
remain rooted in the retained directory and keep `O_NOFOLLOW` on the final
component. Renaming the live version directory and installing a replacement
namespace cannot redirect a catalog operation; the retained catalog continues
and the replacement canary remains unchanged.

Guarded Windows source accepts only a canonical drive path on a fixed NTFS or
ReFS volume with persistent ACL, Unicode, case-preservation, and open-by-ID
support. The storage root, state/version directories, database, sidecars,
lock, and bootstrap artifacts require the exact `TokenUser` owner and a
protected one-ACE owner-only DACL. Reparse, delete-pending, multi-link, and
identity-substituted leaves fail closed. Retained no-delete-share directory
handles protect the live ancestor chain, and a private per-catalog VFS admits
only the exact main, WAL, SHM, and journal names, pre-creates private sidecars,
and compares the underlying SQLite native handle to the guarded file identity.
Bootstrap publication uses a verified source handle and a retained parent
handle with no replacement. These statements describe implemented and
cross-compiled Windows source; BPR-0053 still owns target-native execution.

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
and exact operation/spool/install/ref/object values:

- an exact operation-owned spool may be resumed or deleted;
- an interrupted import may delete only a recorded nonpreexisting pack/index
  and operation-attributable temporary files when no staged, final, or
  catalog ref can reach them;
- `preparing`, `spool_recorded`, `imported`, `staging_ready`, or
  `abort_requested` residue is exact-aborted only through its recorded
  recovery identity;
- `commit_authorized` with the exact staging pair resumes toward promotion;
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

BPR-0044 implements and directly exercises the durable catalog state machine,
including exact operation facts, phase/evidence compare-and-swap, restart
paging, idempotent catalog finalization, destructive tombstones, and repair
provenance. BPR-0045 composes those facts with the cache/ref port through exact
pre-PONR abort and post-PONR continuation, mandatory-success postcondition
inspection, recovery-before-acceptance, serialized publication, and stale
completion rejection. BPR-0047 selects the component through the public
executable and proves durable seed/reuse behavior across two processes;
BPR-0048 supplies the broader cross-component fault matrix.

A bounded host-lifetime registry retains at most 1,000 exact repository-cache
actors. One FIFO and one exact PR-scoped cache session serialize all effects
within an actor; switching PRs closes the old session only after its queued
cleanup, while unrelated repository actors may run in parallel. One context
retains at most 100 exact preparations and one recovery pass admits at most
1,000 operations. Reentrant ingress is deferred, projection exceptions are
contained, and shutdown cannot report quiescence until jobs, pending offers,
decision turns, deferred ingress, and every actor queue are idle.

BPR-0068 makes the retained job vector the global admission bound rather than a
bookkeeping list. It admits 64 jobs by default and at most 1,024 by contract,
and every queued, running, or decision-completion-pending actor closure owns one
slot, including the synchronous diff blob bridge. A slot retires before its
terminal can admit dependent work, and terminal retirement reserves the freed
slot for one core-emitted dependent request so concurrent blob work cannot crowd
out mandatory cleanup. Ordinary reentrant accept and dispatch ingress has its own
default-64, maximum-1,024 FIFO whose overflow is rejected before core acceptance
with exact lease cleanup and one retained diagnostic per turn. Two fixed
invalidation slots and one shutdown flag keep terminal events from being crowded
out by ordinary ingress. A hostile or looping producer therefore meets a bounded
refusal rather than unbounded host residency.

### Quotas, pruning, and deletion

Wire, temporary-storage, repository, object, ref, epoch, catalog-row, text,
page, and transaction counts have hard configured ceilings. Exhaustion fails
the candidate without publishing and without evicting accepted history.
There is no TTL, LRU, automatic epoch pruning, or automatic Git maintenance
that can discard a retained object. Catalog history pages are
snapshot/revision bound and reject stale cursors. BPR-0056 completes the
core-authorized host request/terminal and incremental projection path. The
core requires the exact authority, accepted source, generation, catalog,
cursor, and 1..100 limit; the host rechecks the live catalog binding before
querying; stale or reordered terminals cannot publish rows. The projection
removes the source-observation identity and requests no routine model reset.
Qt triggering and stable-row consumption remain BPR-0046.

Ref pruning releases a retention root and may reclaim the logical epoch quota,
but milestone one claims no immediate physical-byte reclamation without Git
GC. Exact whole-cache deletion is the only guaranteed repository-byte
reclamation path.

Prune and delete are explicit exact-scope commands with a user-visible
consequence summary. A BPR-0044 prune intent persists the exact base/head OIDs,
retained-ref identities, expected catalog revision, and each anchor's expected
starting availability. Only `available`, `unavailable`, and `corrupt` may enter
the operation. Begin, replay, finalization, finalized replay, and restart reject
any revision, identity, or availability drift. BPR-0055 implements the
core-authorized host path that supplies exact paired-ref absence before one
catalog transaction marks both anchors and every linked non-destructive review
association `pruned`, advances the revision, and releases the retained-epoch
quota exactly once. Adapter methods alone are not product authorization;
replay cannot decrement quota or mutate rows a second time.

The selected application exposes these commands only in a secondary local-
history menu. Qt captures the complete exact target before showing a window-
modal warning; Cancel is both the default and Escape action. The warning names
the retained-history or repository-cache consequence, retained tombstones, and
the absence of GitHub or developer-worktree mutation. Confirmation dispatches
the existing typed core intent and grants no direct filesystem, Git, SQLite,
credential, or provider authority to the widget.

The retained anchor/epoch identity remains as a pruned tombstone. Any named
alias whose semantic candidate is that epoch becomes explicitly unavailable
and never remaps to an older resident epoch. Repository-cache deletion
enumerates and durably records every affected PR scope in the one exact
account/profile cache partition and cannot touch another partition. The exact
bounded set is sealed before the point of no cancellation. Handles are closed,
the exact validated directory is
atomically renamed into a transaction-named app-owned quarantine,
cache-deleted tombstones and revisions are finalized, and only that
quarantined path is removed. A failed or interrupted physical delete remains
a retryable unavailable tombstone and is never rediscovered as a cache. The
durable operation ID is the stable authorization token for every replay;
mixed ref state, cache identity ambiguity, or phase drift becomes
`repair_required` rather than broad cleanup.
Whole-cache finalization supersedes an earlier `pruned` availability with
`cache_deleted`; this keeps the durable catalog, incremental projection, and
restart result consistent for every row in the deleted cache scope.
Secure erasure from WAL/journal files or SSD media is not claimed.

Deleting a physical cache does not erase the stable PR authority or rewrite
its history. Reopening the same authenticated PR reparents that stable scope to
a fresh physical cache. Only when its latest live epoch has both anchors
durably marked `cache_deleted` does the catalog expose
`retention_rehydration_required`. That exact fact permits one higher-ordinal,
semantically corresponding epoch with a nonzero host-issued 256-bit source-
observation identity not previously published in the catalog incarnation and
an explicit retention-rehydration cause;
finalization clears the flag atomically and preserves the old tombstone. A PR
with no latest live epoch, a partially deleted pair, or an ordinary unchanged
observation cannot use this exception.

The Git/epoch path keeps credentials and source bodies out of ref names,
catalog metadata, process arguments/environment and its diagnostic records.
This is not a blanket claim about all SQLite or disk content: the private review
ledger intentionally stores notes/drafts, retained Git/workspaces hold source,
and updater recovery can hold review text. Default diagnostic exports omit them.
Complete native crash redaction with resident source/comment/token canaries is
still pending. Security courses use synthetic repositories and canaries; no live
private content is needed for these checks.

## RFC-BPR-0016 device authorization boundary

### What the device host may reach

The device-authorization host reaches exactly four capabilities and nothing
else: the device-authorization transport port, the refresh slot of the
credential vault, the enrollment port, and a clock and timer. It is handed
`github_credential_refresh_vault_port` rather than the whole vault, so
`store`, `retrieve`, and `delete_credential` — the base account slot — are not
reachable from it at all; that is a compile error rather than a convention
([[BPR-0283]], `application_device_authorization_refresh`,
`application_github_credential_vault`). It never sees a profile list, a Git
process, SQLite, an agent, or the workbench.

The browser capability it is given can do one thing: take a single address the
host has just validated and ask the operating system to open it. It receives
no credential, no device code, no client identifier, and no endpoint, and its
`false` answer is an ordinary result rather than an error ([[BPR-0280]],
[[BPR-0516]], `application_device_authorization_browser_handoff`).

### What crosses no boundary

- **No client secret exists.** The registration is a public client; the shipped
  artifact must contain the recorded distributable client identifier and must
  not contain the private owner-only verification identifier used for
  [[BPR-0276]]'s capture ([[BPR-0318]], `scripts/verify_rfc0016_production_identity.py
  --release-binary`, `rfc0016_production_identity_security`).
- **The access token, the refresh token, and the device code never leave the
  host outward.** The token enters the existing enrollment boundary and the
  platform vault and gets no separate storage, lifetime, or erasure rule; the
  device code is bearer material for the length of the poll window only. Core,
  projection, and Qt types cannot name any of them ([[BPR-0277]],
  [[BPR-0280]], [[BPR-0282]], `rfc0016_test_inventory`).
- **The presentation record is closed.** Stage, user code, verification
  address, remaining validity, handoff outcome, and the core's command
  projection. Both text fields are empty at every terminal including the
  successful one, because a user code that outlives its flow is a code a
  reviewer can still be tricked into typing ([[BPR-0280]],
  `qt_device_authorization_scale_100`).
- **Every terminal erases.** Denial, expiry, disablement, refusal,
  cancellation, failure, and success all leave no retained device code and no
  partially enrolled profile ([[BPR-0279]],
  `application_device_authorization_host`).

### The verification address is not the server's decision

An address returned by the service is checked twice against one declared
authority — at the wire by the contract parser, so it never reaches a host,
and again in the host against the authority its composition supplied, before
any browser is invoked. The host's check is an exact host match over HTTPS:
no subdomain, because `github.com.example.invalid` is a subdomain of nothing
the reviewer meant; no userinfo, because `github.com@evil` reads as the
authority to a human and resolves elsewhere; no port, because the compiled
authority names none. A refusal opens no browser ([[BPR-0280]],
[[BPR-0283]], `application_device_authorization_host`,
`github_device_authorization_contract`).

The authority is compiled in rather than configured. An operator who can edit
it can be talked into editing it, and the whole point of the check is that the
address the server sent is not the one that decides.

### Prohibited designs that remain prohibited

No loopback listener, callback server, broker, or daemon; no embedded web
view, browser engine, or in-process credential entry for GitHub itself; no
shipped, derived, or obfuscated client secret; no browser cookie or session
reuse; no operator-editable scope set; and no polling faster than the interval
the service supplies or continuing after denial, expiry, or the total bound.
The device grant was selected over the authorization-code redirect precisely
because the redirect variant needs an inbound socket and this one does not.

### The fixture and production split

A shipped graph binds the real transport and the real system-browser opener. A
tests-enabled graph binds the recorded [[BPR-0276]] grant and a recording
opener instead, and that seam is `BPR_BUILD_TESTING` rather than
fixture-conditional for the browser specifically: a fixture transport reached
by mistake writes a line to a log, and a browser opened by mistake opens a
browser on whoever's machine is running the suite. No test in this repository
launches one ([[BPR-0516]], `application_device_authorization_browser_handoff`).

The registered course therefore exists only in a tests-enabled build. It is
where the headline claim is observed rather than argued: the recorded grant
runs through the real host, the real enrollment boundary, and the real vault,
and the enrolled token is then looked for in every file the application wrote
([[BPR-0282]], `application_device_authorization_registered_course`).

### Automated-test network posture

The device selectors are registered through the default-deny automated-test
boundary. On macOS that is the `sandbox-exec` profile and on Linux a network
namespace, and under both a registered platform's probe reaches `failed`
without sending anything. On Windows the same selectors carry
`WINDOWS_UNCONTAINED`, so they run outside the AppContainer for the reasons
[[BPR-0362]] measured, and the probe reaches `cancelled` by actually reaching
GitHub. That posture was recorded rather than endorsed, and [[BPR-0515]] resolved it on 2026-09-03: a tests-enabled `--network-denied` override routes the dev-tree probe through a closed loopback port, so every platform's dev-tree gate lands on `failed` with zero connections and the shipped Release carries no trace of the flag. The following sentence records what [[BPR-0515]] adjudicated
whether an unattended Windows gate may make outbound contact at all, and it is
open.

## Security evidence

RFC-BPR-0014's [release policy](../security/release-policy.md) binds the final
package, producer, emitted protections, dependency inventory and independent
trust checks. The 2026-09-21 M9 Grype reports fail with 21 macOS / 19 Windows
matches; no vulnerability suppression or applicability approval is recorded.
The exact revised macOS source-distribution approval does not resolve those
findings or qualify changed artifacts. Clean-account/vault, native matrix,
external dependency hardening and full crash-privacy obligations remain open.

Profile maintenance introduces one exact local deletion/export capability,
not a general cleanup service. Its core authorizes a captured selection;
the host drains producers while retaining the lease, validates native identities,
deletes exact vault keys before binding metadata, and journals the irreversible
boundary. Unknown or changed entries refuse; startup recovers before stores open.
Shared registry/updater state, recovered draft text, sibling profiles and exports
are excluded. The [maintenance contract](release-maintenance.md) specifies
request/generation checks, cancellation, durability and hostile tests; the
[operator guide](../development/storage-and-diagnostics.md) states residuals.
Default exports contain only classified metadata; local logs and OS crash reports
are distinct surfaces. Neither a digest nor a passing environment-canary crash
proves all source and secret material absent.

Required release evidence includes threat-model review, SBOM, dependency inventory, secret scan, static analysis, sanitizers, parser fuzzing, hardened build flags, signed package verification, mutation-target tests, and diagnostics redaction tests.

GitHub Actions is not a build, test, package, release, or security-verification
surface for BetterPullReview. Native transport evidence is produced by local
and target-native CMake/CTest runs, loopback fixtures, dependency inspection,
and runtime observation.

The BPR-0044 fresh macOS `dev` matrix passed 58/58, its focused
core/application/storage/behavior matrix passed 20/20 across five repetitions,
the AddressSanitizer/UndefinedBehaviorSanitizer matrix passed 4/4, and the
Qt-free `core-only` matrix passed 25/25. A tests-disabled Release build used the
pinned static SQLite dependency and exposed no catalog fault-injection seam.
Tracked and on-disk GitHub Actions workflow counts were 0/0.

A clean BPR-0044 MinGW cross-build compiled and linked 47/47 targets. Its
inventory listed 25 tests, exactly two labeled `BPR-0053` and
`target-native-windows`, but no Windows executable was run. Those results
establish source/link compatibility only. BPR-0045 adds component-level
Git/ref recovery orchestration plus guarded runtime-port source and current-
host contract evidence; BPR-0047 supplies public composition, and BPR-0053
retains every target-native Windows claim.

## Anonymous public authority

Public preview is an explicit authority with no viewer or credential, not a
fallback after authenticated failure. Its production factories cannot enroll
or retrieve vault material and must not send Authorization. GHES, GraphQL,
viewer identity, and mutation admission fail before request construction. A
404 stays existence-neutral; public cache identity includes the access mode
and cannot select an authenticated private cache namespace.

Native transport canaries, provider refusal counters, SQLite migration/restart,
and selected-file diff tests cover separate boundaries. The successful bounded
Mac live read proves its recorded target and source only; it authorizes no
credential operation or mutation. Linux/Windows public preview remains unavailable.

## Private review-tour text

[Review-tour drafts](review-tours.md) retain authored Markdown and bounded remote
body baselines for editing/recovery in an admitted profile-private store. They
are outside the metadata ledger and diagnostics export. Records bind the exact
profile, host fingerprint, account, repository and PR; save CAS prevents silent
multiwindow replacement. Profile purge owns the exact database and its sidecars.
This retention does not provide encryption, secure erasure, backup removal or
remote-edit authority. Shared tour comments are unaffected by local purge.

## Structured diagnostic framing

BPR-1215 retains the credential-owner sanitizer and the typed diagnostic port.
Its JSON encoder is a framing boundary, not a sanitizer: only fixed operation
facts and the previously classified 2,048-byte summary enter it. Control and
non-ASCII summary bytes refuse before Quill receives them; quote/backslash
escaping prevents field/line injection. The encoded line is capped at 16 KiB
before queue copying. Session labels come from native-backed random-device
entropy, never account/path hashes; they are diagnostic joins only. The current
metadata exporter reconstructs its small allowed vocabulary and counts unknown
or malformed records rather than copying source text. Existing plaintext files
are retained, with their historical privacy limitations.

BPR-1216 bounds native private session files and coordinated retention. BPR-1217
adds persisted independent detail/privacy controls: default request/window fields
become typed opaque labels before any queue or ring copy; Include locally may
retain only the admitted numeric identities. Names, paths, tokens, headers,
source/review bodies and arbitrary producer prose have no new admission path.
Nonempty summaries require the credential-owner's sanitized classification.
Reserved priority ingress preserves room for warnings/errors during normal-detail
overload, within 4,096 events and 8 MiB total (normal traffic stops at 3,584 and
7 MiB); Quill's smaller per-thread bounds can refuse earlier. Every refusal
contributes to the visible loss counter. The transient ring is bounded by three
minutes, 512 events and 2 MiB, with expiry checks before reads and periodic cleanup.
Generation changes invalidate queued records and clear ring/identity state on
Off, detail/privacy transitions, Clear and Sign out. Controls acknowledge only
after storage and writer barriers; failures retain accepted UI state. Sign out
revokes transient access immediately without waiting for file I/O; delayed
intermediate producers carry their captured generation and cannot repopulate it. Historical
files, external exports and OS crash reports retain their original privacy.
Richer scope-aware export and complete native evidence remain later milestones.

## Off-hunk publication and visible anchors

An ambiguous native result followed by a file POST can create two public
comments. RFC-BPR-0022 prevents that transition through one logical parent,
at most two deterministic children and durable authorization before fallback.
Only the exact completed non-creation contract can retire a native witness into
fallback authority; production admits only the owner-approved exact PR2 recorded contract described in the RFC-0022 ledger. Broad server labels and
status codes are insufficient.

An attacker-controlled FILE comment may resemble a code link. Rehydration checks
canonical same-host/repository/path/commit grammar and the exact retained blob
and line bound; it neither follows the link nor grants an effect capability.
External edits and deletion invalidate cached placement by canonical identity.
The SQLite digest detects corruption within the owner-private store; it is not
a defense against a malicious process running as the same user. See the
[owning protocol](review-round-trip.md#semantic-source-line-publication-policy-rfc-bpr-0022)
for migration, capacity, cancellation and restart rules.


### Review-tour outline assistance

Outline results contain only bounded titles, references to supplied context and
private reasons/questions. The application maps source references to its retained
exact-source observations; the model supplies no destination authority, body,
readiness, tool grant or publication instruction. Skeleton insertion is explicit,
local and undoable. Generated ancestry persists through structural edits and both
publication admission paths require a separate current visible author completion.
The restricted process protocol and profile/owner/generation checks remain the
existing writing-assistance boundary. Deterministic process fixtures establish
these application behaviors, not installed provider enforcement or model quality.
