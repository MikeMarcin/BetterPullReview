# Fixture Catalog

The implementation must maintain sanitized deterministic fixtures covering at least:

## Implemented GitHub read contract v1

`tests/fixtures/github/contracts/v1/` contains paired `github-com` and
`ghes-3.18` manifests for the ten RFC-BPR-0002 read operations. Each manifest
pins the product, synthetic authority, API roots, request method, relative
path, canonical query, non-secret headers, GraphQL document and variables,
and selected response projection. The two response trees contain the same
synthetic semantic facts.

These fixtures are hand-minimized, sanitized provider projections, not
literal complete wire dumps. They deliberately omit authorization, cookies,
transport metadata, remote URLs, patches, source content, email addresses,
home paths, request identifiers, and provider fields that the normalizer does
not consume. The strict recorded-contract harness rejects additions or
omissions from this selected projection. A production transport must validate
the raw envelope and project the allowlisted fields before this strict
normalization boundary; additive raw GitHub response fields are not an excuse
to expose provider records inward.

The paired baseline contains repository, pull-request, viewer, author, review,
thread, comment, commit, check, and search identities. GraphQL repository and
pull-request node IDs are correlated with REST, and actor node IDs plus logins
are correlated across every normalized author. GraphQL file paths map
bijectively to the REST file collection and carry viewed state; threads carry
per-target resolve/unresolve availability. Contract variants directly
exercise null/deleted and non-User actors, fork heads, nullable review commit
IDs, renamed files, every viewed state, queued checks, and both line- and
file-level review threads. Every selected response is explicitly marked
complete; unknown completeness or a transport-declared continuation fails.
The files, threads, and nested comments GraphQL connections each have
terminal `pageInfo`; a true `hasNextPage` result fails rather than being
normalized as a truncated snapshot.

The fixture security gate verifies the exact tree and operations, rejects
duplicate JSON members and encoded secret/path/email/foreign-authority
bypasses, and checks both hosts against one independent normalized
expectation. No live or private repository response was recorded.

## Implemented GitHub capability contract v1

`tests/fixtures/github/capabilities/v1/` contains independent complete
introspection responses derived from the official public GitHub.com and GHES
3.18 schema snapshots with the probe's exact static query. Per-host manifests
pin the official source URL and digest, retrieval metadata, query digest,
generator version, complete response digest, and actual Mutation and
PullRequest field counts. Synthetic missing/incompatible mutation-contract
descriptors derive only from the GitHub.com response. The contract covers the
exact review-thread query fields, viewed-file state, mark/unmark inputs and
payloads, resolve/unresolve inputs and payloads, and their dependency
relationships.

These are public schema-contract observations, not live account-permission
claims or authenticated API recordings. Unsupported results are synthetic
because both selected schemas support all four capabilities. Operational
failures and partial/malformed responses are constructed in the focused probe
test and must remain all-Unknown. The fixture security gate pins the exact
tree, provenance and response digests, rejects duplicate JSON members and
secret-bearing content, proves the host responses are independent, and keeps
every relative descriptor reference inside the capability fixture root.

## macOS Keychain integration fixture

The BPR-0022 target-native fixture uses the fixed
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v2`
service and one exact synthetic authority in the user's default file-based
Keychain. It never selects the production service. A resource-locked CTest
setup/lifecycle/cleanup sequence proves initial absence, add, retrieve,
in-place replacement, retrieve, delete, post-delete absence, and exact final
cleanup.

The parent generates runtime-only printable canaries, sends them to a
same-executable child over framed standard input, uses a neutral audited
working directory and bounded child deadline, and captures only fixed
pass/fail output. It checks arguments, the explicit child environment,
executable, source, and isolated scratch/XDG tree for raw canaries. Combined
source-boundary checks reject settings, SQLite, environment, file, process,
logging, UI, and alternate-Keychain fallbacks from the production adapter.
The scratch scan does not inspect the user's complete home directory, and the
fixture does not claim cross-rebuild cleanup after an ad hoc code-signature
change.

BPR-0022's original closeout run exercised the historical hostname-free v1
service. BPR-0029 changed the current fixture to the disjoint v2 service and
574-character hostname-bound authority, then repeated the macOS setup,
lifecycle, and exact cleanup. Production has no v1 read, enumeration,
migration, or fallback.

## Windows Credential Manager integration fixture

The BPR-0023 fixture uses the fixed
`org.betterpullreview.github-native-credential.windows-credential-manager.integration-test.v2/`
prefix and one exact synthetic authority. It never selects the production
prefix. A resource-locked CTest setup/lifecycle/cleanup sequence is prepared
to prove initial absence, create, retrieve, single-call replacement, retrieve,
delete, post-delete absence, and exact final cleanup on Windows 10/11 x64.

The parent generates two runtime-only printable canaries, including one at the
complete 2,560-byte portable/native bound, and sends framed bytes to a
same-executable child through anonymous standard input. It starts the canonical
executable directly with an exact inherited-handle list, a sorted minimal
Unicode environment, and a unique neutral scratch directory. The child emits
only fixed pass/fail text. The parent bounds execution, requires confirmed
child termination before credential cleanup or scans, checks parent/child
arguments and explicit environment, and searches fixed stdout/stderr,
executable, source, and the isolated scratch tree for random 32-byte prefix
and suffix probes from both canaries. The short probes detect truncated
replacement leaks without placing either complete credential on an audited
surface. Successful scratch removal must leave the exact path absent.

If termination cannot be confirmed, the test fails closed and deliberately
does not delete storage beneath a potentially live child; the independent
CTest cleanup still attempts exact credential deletion. The fixture does not
enumerate Credential Manager. It also does not inspect arbitrary application
settings, SQLite files, Qt UI text, Windows Event Log, ETW, or the user's
complete profile. Settings, SQLite, UI, logging, file, registry, environment,
DPAPI, Credential Locker, and alternate-vault exclusions come from the
production source boundary and must not be attributed to the runtime fixture
alone.

This fixture and its WIN32 registration are checked in but have not run on a
native Windows environment. Host-stub compilation and an x64 MinGW PE
cross-compile do not satisfy that lifecycle claim. BPR-0036 retains the
target-native run and exact cleanup as open work.

BPR-0023's preparatory source and cross-PE evidence exercised the historical
v1 target format. BPR-0029 changed the current source and fixture to the
disjoint v2 target and 574-character hostname-bound authority. The v2
target-native lifecycle and cleanup remain unclaimed until BPR-0036 runs them
on Windows.

## Linux Secret Service integration fixture

The BPR-0024 fixture uses the fixed
`org.betterpullreview.github-native-credential.secret-service.integration-test.v2`
schema and one exact synthetic authority. It never selects the production
schema. Each item has only the 574-character hostname-bound `authority` value
plus the `xdg:schema` attribute supplied by libsecret, and resides only in the
default collection.

The resource-locked CTest setup/lifecycle/cleanup sequence proves initial
absence, a 73-byte store and retrieve, atomic replacement with a 2,560-byte
value and retrieve, delete, repeated absence, and exact final cleanup. Store
labels the value `application/octet-stream`; retrieve accepts that type or
exact `text/plain` because the tested gnome-keyring 50/libsecret 0.21.7 stack
was observed normalizing only the content-type metadata while preserving the
opaque bytes. After the runtime-canary lifecycle, the same native child repeats
the 73-byte and 2,560-byte lifecycle with deterministic opaque values
containing NUL, high-bit, and invalid UTF-8 byte sequences.

The parent generates both runtime-only printable canaries, sends framed bytes
to a same-executable child over standard input, provides an explicit minimal
environment and neutral isolated scratch directory, bounds child execution,
and accepts only fixed output. It checks parent and child arguments,
environment, stdout/stderr, executable, source, and isolated scratch for
canary probes. Parent and independent fixture cleanup both delete only the
exact fixture authority and verify that item absent. A separate controlled
D-Bus query verifies schema-wide integration-test absence after the matrix.

Controlled direct modes separately prove typed unavailable with no desktop
session bus, typed locked with a truly locked default collection, and
rejection of a hostile dependency-level `SECRET_SERVICE_BUS_NAME`. Production
does not read or mutate that variable: the adapter opens with the literal
canonical service argument and `SECRET_SERVICE_NONE`, rejects any opened proxy
whose exact name is not `org.freedesktop.secrets`, and establishes a session
only after that check. The live hostile owner observed only proxy-property
inspection and no `OpenSession`, collection, or credential-item method.
Prompt overrides deny interaction and make one bounded, no-auto-start
`Dismiss` when given a prompt. Source boundaries prove the absence of unlock,
prompt display, collection creation, alternate-vault, plaintext, settings,
SQLite, environment, file, process, network, logging, and UI fallbacks; the
runtime fixture alone does not claim arbitrary user-home or system-log
inspection. Fake-backed operation failures and direct native-status
classification exercise access-denied and failed-delete terminals without
claiming a desktop policy can reliably force those two states.

The historical BPR-0024 v1 fake contract and all three native fixture CTest
cases passed in an Ubuntu 26.04 x86_64 full-system QEMU VM with an emulated
CPU, Linux 7.0, Qt 6.10.2, libsecret 0.21.7, and gnome-keyring 50. The four
focused cases ran under Xvfb, a desktop D-Bus Secret Service session, and a
dedicated real user; the controlled unavailable, locked, hostile-redirection,
and cleanup checks also passed. This directly verifies the historical v1
Linux vault stack in a full-system VM, not bare metal. BPR-0026 separately
verified the Linux internal native HTTPS wire surface in a full-system Ubuntu
26.04 QEMU guest.

BPR-0029 changed the current fixture to the disjoint v2 schema and
hostname-bound authority. Production has no v1 read, enumeration, migration,
or fallback. A separate BPR-0029 run in an Ubuntu 26.04 x86_64 full-system
QEMU guest passed the current v2 setup/lifecycle/cleanup cases 3/3,
independent exact cleanup, and hostile, unavailable, and locked cases. Both
composed public-path fixtures also passed on that pinned BPR-0029 snapshot.
This v2 evidence is not inferred from or substituted for the historical v1
result.

## GitHub CLI process and trust fixtures

The BPR-0025 compiled-child fixture launches its own canonical executable
through the production `QProcess` runner. Helper modes directly inspect the
element-wise argument vector, requested child-environment allowlist, standard
input, and canonical directory currently outside repository ancestry. A
random parent environment name/value unknown to production is absent in the
child. Raw macOS inspection
allows and separately verifies only Darwin's deterministic
`__CF_USER_TEXT_ENCODING` addition; raw Linux inspection requires the
requested fact and explicit locale with no additions. Exact limit and
limit-plus-one cases cover input, retained output, error, duration,
cancellation, nonzero exit, crash, simultaneous stream pressure, and
termination/reaping. Every failing terminal exposes only a fixed category and
discards partial standard output and all standard error.

The Unix fixture directly observes closed inherited descriptors, disabled core
dumps, reset signal disposition, and that the child is the new session and
initial process-group leader. It proves clean leader exit, nonzero failure,
timeout, and cancellation signal descendants that remain in that initial
process group while the direct child is reaped. An escape case proves a
descendant that moves to another group survives until explicit fixture
cleanup. The fixture also proves a request whose absolute deadline expires
behind the runner's sole worker never launches later.

A compiled seam proves completion-watchdog quarantine, zero launch for another
request while quarantined, and recovery after the worker resumes. Source and
hostile architecture gates, not a compiled live-process failed-reap fixture,
require the owner-managed worker to retain a live `QProcess` after actual
bounded reap failure and continue kill/reap work without invoking the Qt
destructor inside the completed call.
Allocation-free request-shape validation and pre-cancellation precede
work-item allocation and request copying. Marker-backed N+1 cases cover path,
argument, environment, input, stream, time, and reap dimensions and require
zero launches.
Reads use a fixed buffer and request at most the remaining retained bound plus
one byte. This does not prove a hard cap on Qt's private in-flight pipe
allocation. Windows Job Object descendant containment, private pipe bounds,
and target-native CLI behavior are deferred to RFC-BPR-0015. BPR-0036 retains
the Windows native-HTTPS, vault, and composed-public-path fixtures.

The invocation fixture compiles every selected GitHub.com and GHES 3.18 read
operation. It checks exact REST-prefix removal, literal GraphQL routing,
hostname/method/header arguments, standard-input bodies, fixed hardening
environment, platform credential-session facts, and the absence of arbitrary
environment or caller argument input. Profile, correlation, server, route,
method, header, body, executable, environment, and working-directory drift
fail before the process port is called. The production trust gate is also
tested with a fake successful child: it remains `cli_unavailable` and makes
zero calls.

`tests/scripts/verify_gh_cli_trust.py` is a separate direct selected-binary
fixture. It resolves one absolute `gh`, makes and executes an owner-private
copy, hashes that copy before use, and verifies its digest again after every
probe. It creates a private temporary config and random synthetic startup
token, generates a disposable loopback certificate, and captures only fixed
TLS/redirect categories, counts, booleans, version, and digest. No GitHub
endpoint is contacted, and every child runs from the canonical
non-repository temporary directory. The certificate fixture requires the
selected fixed bad-certificate alert and zero HTTP requests. The `302`,
`307`, and `308` POST redirect fixtures compare the body digest and
authorization value without retaining either, and record whether a distinct
loopback hostname is contacted.

`gh` rejects the fixture's ephemeral `host:port` as a `--hostname`, so the
verification-only invocation retains the synthetic `github.com` startup
hostname while its full URL selects loopback. Official selected-version
source establishes that relative production endpoints use the same Go
client. This is exact-binary behavior characterization, not exact
production-authority proof.

The current selection is the official macOS arm64 `gh` 2.96.0 release dated
2026-07-02. Its downloaded ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`,
and the extracted binary SHA-256 was
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`.
It produced one bad-certificate alert with zero HTTP requests but followed the
cross-authority `302` once. Its selected origin received the exact 47-byte body
and synthetic authorization; the distinct sink received neither. The `307`
and `308` origins received both, while their sinks were not observed and
therefore remain unproven rather than accepted as redirect rejection. The
installed macOS `gh` 2.95.0 binary, SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun and produced the same material outcomes.

The fixture separately proves that persisted `http_unix_socket` is an active
transport setting. For both selected binaries it verifies that the private
configuration was loaded, then observes one exact authenticated `gh api`
request through an AF_UNIX sentinel with the expected synthetic Authorization
value. This evidence must not be reduced to a telemetry concern. The temporary
executables and configurations are verification-only and removed after the
probe; production never admits `GH_CONFIG_DIR`.

Selected command/source inspection records that `gh api` exposes no
per-invocation `--user`, `gh auth token --user` reveals credential material,
and `gh auth switch --user` mutates persistent active-account state. The
selected source also resolves `ActiveToken` per round trip, falls back from a
keyring error to a blank-host token, and launches `/usr/bin/security` through
the macOS keyring implementation. Production path/name validation cannot bind
an eventually launched image to the exact copied-byte digest across
replacement or update; CA paths cannot bind later-opened contents; working
directory identity is not bound through launch; Unix descendants can escape
the initial process group; and reused numeric PID/process-group identities can
receive later signals.

Milestone one keeps native HTTPS as the sole production authentication
transport. The fixed CLI gate reports eight typed reasons: product-scope
deferral, executable-image identity, cross-authority redirect behavior,
ambient configuration, credential/account identity, working-directory
identity, descendant containment, and numeric PID/process-group identity
reuse. A fake successful child still receives zero calls, and the public path
returns `cli_unavailable` with zero process launches. RFC-BPR-0015 owns any
future positive CLI enablement.

## Native HTTPS loopback transport fixture

`tests/scripts/verify_native_https_transport.py` drives the compiled
`better_pull_review_native_https_transport_tests` child through the real
BPR-0026 request/TLS/redirect/cancellation/response path. It creates a
disposable one-day CA and leaf certificate for the canonical synthetic
`ghe.bpr.invalid` authority, starts loopback-only HTTP CONNECT proxies, and
requires the TLS SNI and origin Host to remain that authority. No GitHub or
other remote endpoint is contacted.

The parent generates a different random bearer token for every scenario and
sends the token and fixture CA to the child through big-endian
length-framed standard input. Tokens are absent from arguments and
environment. The proxy rejects Authorization and Proxy-Authorization on
CONNECT, then compares the exact `Bearer` value only inside the encrypted
origin request. The fixture also checks the exact method, path, body shape,
Host, `Accept-Encoding: identity`, and `Accept-Language: *`. Child output is
bounded and must be one fixed pass line with empty standard error; the parent
scans both captured streams for the live token and overwrites its mutable
token/frame buffers. Python, Qt, TLS, and socket internals can make immutable
or private copies that do not expose precise wiping, so this is not an exact
process-memory erasure claim.

The complete 15-case matrix is:

| Scenario | Direct observation |
|---|---|
| `success` | One CONNECT, trusted TLS origin request, exact authorization, bounded JSON success |
| `proxy-fallback` | A refusing first proxy is tried before the second loopback candidate succeeds |
| `same-redirect` | A REST `GET` follows one query-free path on the exact authority and sends a fresh exact request |
| `cross-redirect` | A distinct authority is rejected without contacting it |
| `graphql-redirect` | GraphQL `POST` redirect is rejected after the initial request |
| `untrusted-certificate` | Omitting the disposable CA produces typed certificate failure with no origin HTTP or authorization |
| `response-limit` | Limit-plus-one response content produces typed response limit |
| `header-limit` | An 8 KiB-plus-one response field produces typed response limit |
| `cancel` | A stop request aborts a deliberately stalled in-flight operation |
| `timeout` | The remaining absolute deadline aborts a deliberately stalled operation |
| `pre-cancel` | Cancellation wins before any proxy, TLS, HTTP, or credential network effect |
| `http-401` | Status maps to typed authentication |
| `http-403` | Status without rate-limit facts maps to typed permission |
| `http-429` | Status plus bounded `Retry-After: 7` maps to typed rate limit with seven seconds |
| `content-encoding` | A compressed-content declaration is rejected as malformed |

Every networked bootstrap scenario also attempts a replay and requires fixed
unavailability, proving that a claimed bootstrap credential is one-shot.
The untrusted-certificate server may observe a completed TLS 1.3 server-side
handshake before the client rejects trust, or it may observe a handshake
failure; in both allowed observations the client sends no origin HTTP bytes or
authorization. The fixture therefore proves peer-verification rejection, not
one particular server-side handshake timing.

The fixture's adapter-private configuration is compiled only when
`BPR_BUILD_TESTS=ON`. It replaces production system-proxy discovery with one
or two ordered loopback proxies and may augment system roots with exactly one
fixture CA. A separate tests-disabled build must contain no
`make_native_https_*_transport_for_test` symbol. The fixture directly proves
candidate fallback and wire policy, while `--runtime` separately reports Qt
TLS support/backend and the first raw system-proxy candidate kind/count. The
runtime count is not proof that every returned system candidate is reachable.

Both the compiled contract and all 15 loopback scenarios passed on the
directly exercised macOS arm64 host. Runtime facts reported Qt Network 6.11.1,
OpenSSL, TLS supported, and one direct candidate. A tests-disabled dependency
audit found no test-only factories, retained the GitHub adapter's Security and
CoreFoundation dependencies, and observed the current ad-hoc linker
signature. This is internal wire and development-runtime evidence, not
composed-provider or package certification. BPR-0029 supplies the separate
public application fixture on the directly exercised platform.

The Linux matrix passed in an Ubuntu 26.04 x86_64 full-system QEMU guest with
Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. Its frozen
`native_https_transport.cpp` SHA-256 was
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`;
the frozen harness SHA-256 was
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The contract and fixture passed 2/2 in 0.12/11.29 seconds, with all 15
scenarios directly passing. Both architecture tests passed 2/2 in
5.00/144.35 seconds with all 92 hostile mutations. The uninstrumented
focused suite passed repeatedly, and
runtime facts were
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
The ELF directly needs Qt6Network and Qt6Core. A controlled dependency trace
of only `success` under `strace` opened
`qt6/plugins/tls/libqopensslbackend.so`, `libssl.so.3`, and `libcrypto.so.3`.
A full-suite `strace` is not evidence: tracing perturbs cancellation timing,
so functional evidence comes from the uninstrumented passes and the
single-success trace proves only dependency selection. Tracked and on-disk
scans found no GitHub Actions workflow file, and the source/build
configuration selected no GitHub Actions integration.

Windows execution remains BPR-0036 work and is nonblocking for all other
slices and later-RFC work; it blocks only affected Windows claims and final
RFC-BPR-0002 Auto-Verified closeout.

BPR-0030 extends the current verifier to 32 scenarios without changing the
frozen 15-case BPR-0026 evidence above. The added `same-query-redirect`
scenario returns a same-authority REST `Location` with a nonempty query and
requires a typed redirect failure after exactly one CONNECT, TLS handshake,
origin request, and exact Authorization observation. Any attempted second hop
violates the fixture counts and request-target contract. The complete current
32-scenario verifier passed on the directly exercised macOS arm64 host.

The transport's 30-second deadline and prompt cancellation cannot preempt
synchronous Qt system/PAC proxy discovery or system-root enumeration.
Response/header limits bound bytes accepted and retained by the adapter, not
Qt's private HTTP parser, network/TLS buffers, or pre-metadata allocation.
`QNetworkReply` may also normalize or collapse duplicate response headers,
including `Location`, before exposing raw header pairs and redirect
attributes; policy enforces the metadata Qt exposes, not an exact count of
on-wire Location fields. Likewise, the explicit Authorization buffer is
overwritten but Qt-held copies cannot be exactly wiped. GraphQL
HTTP-200/body-defined secondary-rate-limit interpretation and `ListChecks`
expected-`HeadOid` binding are exercised by the BPR-0027 provider harnesses
below.

The loopback matrix directly exercises the response-body limit and one
8-KiB-plus-one header-field limit. It has no separate 65-field-count or
aggregate-header-only scenario; those two adapter checks remain
source/contract evidence.

## Application host and provider harnesses

The BPR-0027 pager suite drives synthetic bounded wire pages through the
production pagination and normalization code. REST cases cover canonical
`Link` progress, repeated and malformed continuations, exact authority,
per-page and aggregate count/byte limits, expected totals, and atomic
failure. GraphQL cases cover every selected connection, independent cursors,
missing/repeated/non-progressing cursors, capability-shaped field omission,
repository and pull-request identity, expected head OID, and completeness.
No case contacts GitHub or contains private repository content.

The provider suite uses injected standard-C++ transports and an effect
executor to exercise profile resolution, one-shot bootstrap `/user`,
account-bound `/user`, capability probing, full pull-request assembly,
cancellation, stale completion, shutdown, submission failure, and exact
completion correlation. It validates GitHub.com-required and GHES-optional
resource tuples, independent `core`/`search`/`graphql` accumulation across
pages, bounded `Retry-After`, status/body-aware primary and secondary limits,
and structured GraphQL `RATE_LIMITED` errors.

The application host suite uses fake provider/vault ports and the manual
decision lane. It reorders and duplicates terminals; switches profile and
account during provider, store, retrieve, and delete phases; exhausts retries;
injects projection exceptions and repeated reentrant feedback; and proves
FIFO generations, terminal-priority shutdown, exact staged-snapshot
acceptance, and stale-result suppression. Credential cases establish bindings
only through a real strict bootstrap observation, prove exact A-to-B cleanup
while A retrieval is active, retain cleanup tombstones, reject caller-selected
account authority, and prove an independently populated capability cache
cannot enter the production host.

Catalog sharing across host fixtures proves same-process recreation only.
BPR-0029 adds and directly tests the public host's
`delete_retained_on_shutdown` policy: the exact delete terminal is required
for quiescence, and failure retains the tombstone. There is still no
restart/durable-selector or enumeration fixture; a crash can leave an item the
next process cannot recover. Windows target-native execution remains in
BPR-0036.

## GitHub Qt presentation harness

The BPR-0028 Qt harness feeds synthetic provider-free session and pull-request
projections into the real GitHub window and conversation model. It covers
profile-only identity input, read-only authenticated-account output, semantic
Connect/Open/Refresh intents, Qt-local review-thread focus, command-owned
action state, all status and diagnostic categories, capability tri-state,
rate-limit presentation, identity invalidation, refresh retention, new-target
clearing, selection restoration, plain-text hostile markup, accessibility,
focus stability, and minimum-size scroll behavior.

Reviews, review threads, nested review comments, and pull-request comments use
distinct stable IDs. The model cases require exact insert, remove, minimal
move, and data-change notifications without reset; both reorder directions
must produce the requested full order and preserve persistent indexes.

BPR-0028 recorded a direct adversarial run covering 3,000 mixed cases, 5,000
arbitrary 100-row permutations, and both 50,000-row rotations. That result
remains historical BPR-0028 evidence; it was not a standalone checked-in
property target in that slice. BPR-0030 checks the corpus into
`better_pull_review_qt_github_conversation_model_property_tests`, registered
once as `qt_github_conversation_model_property`. Fixed seeds make the 3,000
mixed projection updates and 5,000 permutations reproducible. The target
requires exact tree/order and every public projection role, deletion-aware
display/accessibility invariants, deterministic variation of every mutable
role on preserved rows, LIS-minimal permutation moves, exact changed-row
counts, insert/remove coverage, correct persistent-index preservation or
invalidation, one move for each large rotation, and zero model resets.

Scale-specific CTest cases render at 1.0x, 1.25x, and 2.0x and save
minimum-top, minimum-lower, and normal-size PNGs beneath the ignored build
tree. Headless Linux selects Qt's basic Windows style after a direct Qt 6.10.2
backtrace identified a fractional-scale offscreen Fusion cached-painter fault;
native-style real-application verification remains a separate surface.

BPR-0031 extends the same harness without adding a provider or effect. It
directly asserts the `disconnected`, `connecting`, `empty`, `loading`,
`error`, `cancelled`, `loaded`, `refreshing`, `loaded-error`, and
`loaded-cancelled` presentation states; collapsed disclosures and keyboard
order; narrow and wide splitter layout; retained content after refresh failure
or cancellation; and empty first-load failure and cancellation. In addition
to the minimum and loaded-success rasters, each scale saves representative
`disconnected`, `loading`, `first-load-error`, `session-cancelled`,
`first-load-cancelled`, `retained-refresh-error`, and
`retained-refresh-cancelled` PNGs. These transient files are inspected for
clipping, overlap, hierarchy, and retained-state behavior.

Conversation rows use a nonuniform wrapped delegate. Tests require literal
multiline remote text, increasing height at narrower widths and larger fonts,
nonoverlapping rows, native palette/style/font inheritance, zero model resets,
and preservation of current selection, keyboard focus, and an explicitly
collapsed thread through accepted refresh and controlled application
palette/font changes. A scoped accessibility handler requires one combined
polite status-plus-diagnostic announcement and suppresses identical repeats.

The harness contains no credential, provider port, transport, URL, request,
or private repository data and performs no GitHub effect. Its forced
offscreen style remains deterministic layout evidence only; it cannot certify
native operating-system appearance integration.

## GitHub public application composition fixture

The BPR-0029 test-enabled executable supports
`--github-fixture github-com` and `--github-fixture ghes-3.18`. Each choice
selects one profile whose authentication mode is native HTTPS and uses the
same concrete profile-selection, QAction, provider, host, core, and Qt
composition as the interactive application. The test-enabled branch
substitutes a recorded transport factory and fixture vault; it therefore
verifies authenticated-viewer observation, capability probing, complete
pull-request assembly, application-host staging, exact core acceptance,
incremental rendering, and orderly shutdown without making a real native
network or native-vault lifecycle claim.

The fixture transport separately records bootstrap and stored-account reads.
Its vault retains the complete credential operation context and accepts
retrieve/delete only when the exact request, application session, identity
generation, profile, profile revision, host identity, canonical hostname,
account, and server contract match. Success requires the recording and
capability fixture to be consumed, both transport selections to be observed,
the credential to be stored and retrieved, the exact shutdown delete to
complete, and no credential to remain in fixture-vault state. Output contains
only fixed markers and bounded structural evidence:

```text
PUBLIC_PATH_OK
GITHUB_FIXTURE_OK:github.com
RENDER_OK
```

or the corresponding `ghes-3.18` marker. PNGs stay under the ignored build
tree. Headless Linux additionally selects Qt's basic Windows style. These
rasters prove deterministic composition and layout, not native light/dark
following or runtime appearance switching. BPR-0031 separately records native
system-Dark startup and controlled loaded-process appearance response; a real
operating-system Light startup and live system toggle remain distinct native
evidence.

The tests-disabled `--self-test` enumerates every visible CLI profile and
requires the bounded `cli_unavailable` presentation for each. The default
profile set reports `CLI_UNAVAILABLE_COUNT:1`; the explicit GHES case
`application_public_path_ghes_cli_unavailable` reports
`CLI_UNAVAILABLE_COUNT:2`. Provider counters independently require zero
transport-factory, wire, capability, or CLI-specific effect calls after the
provider receives each viewer request, and the CLI transport contract proves a
fake successful process port receives zero calls. Public selection itself
performs one bounded noncredential profile-resolution effect per row from
clean start. Host lifecycle coverage separately proves that switching from an
authenticated native profile first exact-deletes the predecessor credential
binding and does not enroll or retrieve a credential for the CLI profile.
Native HTTPS remains the sole milestone-one production authentication
transport.

The fixture implementation, command-line parser branch, vault, recordings,
paths, labels, and evidence strings exist only when `BPR_BUILD_TESTS=ON`.
`application_composition_contract` creates a fresh tests-disabled Release
tree and inspects its public header, compile commands, build graph, and raw
application bytes. It fails if the fixture translation unit, test
definitions, fixture argument/status strings, recorded label/identity, fixture
paths, or fixture-access symbols survive. It also rejects CLI process-runner
factory symbols and requires production to compose only the native wire
transport factory. This proves structural production selection; the separate
native HTTPS loopback fixture above supplies direct wire evidence.

The macOS current-tree public fixture runs passed for both profiles and
reported the fixed success markers above through the substituted fixture
transport/vault. The pinned BPR-0029 Linux v2 run passed both recorded profiles
with the exact host labels and rendered state in an Ubuntu 26.04 x86_64
full-system QEMU guest. That snapshot's fresh tests-off Release build completed
36/36 targets, omitted the fixture seam, reported `PUBLIC_PATH_OK` and
`NATIVE_PLATFORM_VAULT_SELECTED:true`, and passed dependency, runtime, and ELF
audits. Separate target-native HTTPS and Secret Service fixtures supply the
wire and native-item lifecycle evidence. These Linux PNGs remain
layout/composition evidence only, not presentation-polish or system-theme
certification. Windows public-fixture execution remains BPR-0036.

## GitHub hosts

- GitHub.com current contract
- GHES 3.18 REST and GraphQL contract recordings
- Missing GraphQL capability
- Permission denied
- Rate limited and secondary-rate-limited responses
- Proxy and corporate CA configuration
- Stale head during mutation

## Pull request histories

- Single upload, simple edit
- Multiple uploads with unchanged files
- Force-push/rebase with retained old head
- Base branch advance without head movement
- Base branch change
- Pure rename
- Rename with edits
- Added/deleted/binary/submodule files
- Review associated with historical commit

## Diffs

- Replace, insert, delete, and mixed hunks
- Unequal old/new group heights
- Multiple nearby groups
- Long lines and tabs
- Unicode and invalid byte replacement policy
- Multiline comment/string crossing hunk boundary
- Whitespace-only policy changes
- Large generated file and highlighting fallback

## Conversations

- New thread, replies, edits, deletion, resolution, reopening
- Outdated anchor
- File-level comment
- Pending review drafts
- External-client mutation between refreshes

## Local state

- Read and acceptance invalidation
- Account/repository isolation
- Schema migration
- Abrupt termination and WAL recovery
- Deleted local database

## AI

- Codex one-shot JSONL
- Codex App Server session and approval events
- Claude stream-json
- External terminal fallback
- Head changes during analysis
- Invalid finding anchor
- Corporate host mapped to forbidden personal provider

## Architecture and concurrency harnesses

- Core-only build environment with Qt, SQLite, GitHub, Git, AI, and platform provider packages unavailable
- Deterministic event/observation replay corpus for navigation, receipts, defects, queue state, and AI staleness
- Manual executor fixtures that reorder, delay, fail, cancel, and deliver stale completions
- Qt model harness asserting exact insert/remove/move/data-change signals and persistent-index behavior
- QAction projection harness covering enabled, checked, and disabled-reason state across capabilities and selections
- Large-review structural counters proving active-set/changed-set work and absence of routine whole-model resets
- Shutdown fixture with pending GitHub, Git, diff, syntax, store, queue, and AI operations
