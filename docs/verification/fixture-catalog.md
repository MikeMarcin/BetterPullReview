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
composed public-path fixtures also passed. This current-tree evidence is not
inferred from or substituted for the historical v1 result.

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
and target-native behavior remain open in BPR-0036.

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

The selected macOS `gh` 2.95.0 binary, SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
produced one bad-certificate alert with zero HTTP requests but followed the
cross-authority `302` once. Its selected origin received the exact 47-byte
body and synthetic authorization; the distinct sink received neither. The
`307` and `308` origins received both, while their sinks were not observed
and therefore remain unproven rather than accepted as redirect rejection.
The `302` is direct evidence that the selected CLI cannot satisfy the
no-cross-authority contract, not a successful broker trust result. The
temporary executable and configuration are verification-only and removed
after the probe; production never admits `GH_CONFIG_DIR`.

The fixed production gate also records three independent source-backed
blockers. Production's canonical path/name validation does not bind the
eventually launched image to the probe's exact copied-byte digest across a
replacement or update race. Persisted CLI configuration can select
`http_unix_socket` through the platform home/application-data path, and
`gh api --hostname` has no per-invocation account selector that binds it to
the request's correlated credential identity generation. An external
`gh auth switch` can change the active account for that host. The fake
successful child still makes zero production calls because
executable-identity, redirect, ambient-configuration, and credential-identity
trust must all be established independently. BPR-0037 owns that
production-enablement work together with owner/permission and
validation-to-launch binding for the canonical/currently non-repository
working directory, Unix descendant escape from the initial process group, and
binding PID/process-group identity across cleanup so a reused numeric group
cannot receive a late signal. The current group signaling is not claimed
race-free. It must also bind selected Linux CA-file/CA-directory identity and
contents through the CLI's later open and directly prove hostile replacement
and content mutation fail closed.

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
single-success trace proves only dependency selection. Source and generated
build contained no workflow files or GitHub Actions references.

Windows execution remains BPR-0036 work and is nonblocking for the other
slices; it blocks only affected Windows claims and final RFC-BPR-0002
closeout.

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
native-style real-application verification remains BPR-0031. The harness
contains no credential, provider port, transport, URL, request, or private
repository data and performs no GitHub effect.

## GitHub public application composition fixture

The BPR-0029 test-enabled executable supports
`--github-fixture github-com` and `--github-fixture ghes-3.18`. Each choice
selects one recorded native-HTTPS profile and uses the same concrete
composition as the interactive application: profile selection, QAction,
provider facade, authenticated viewer observation, capability probe, complete
pull-request assembly, application-host staging, exact core acceptance,
incremental Qt rendering, and orderly shutdown.

The fixture transport separately records bootstrap and stored-account reads.
Its vault retains the complete credential operation context and accepts
retrieve/delete only when the exact request, application session, identity
generation, profile, profile revision, host identity, canonical hostname,
account, and server contract match. Success requires the recording and
capability fixture to be consumed, both transport selections to be observed,
the credential to be stored and retrieved, the exact shutdown delete to
complete, and no secret to remain. Output contains only fixed markers and
bounded structural evidence:

```text
PUBLIC_PATH_OK
GITHUB_FIXTURE_OK:github.com
RENDER_OK
```

or the corresponding `ghes-3.18` marker. PNGs stay under the ignored build
tree. Headless Linux additionally selects Qt's basic Windows style. These
rasters prove deterministic composition and layout, not final visual polish,
native light/dark following, or runtime appearance switching; BPR-0031 owns
those public UX checks.

The production CLI choices are tested separately through `--self-test`.
Selecting the visibly unavailable CLI profile must publish the bounded
`cli_unavailable` presentation and make zero process launches.

The fixture implementation, command-line parser branch, vault, recordings,
paths, labels, and evidence strings exist only when `BPR_BUILD_TESTS=ON`.
`application_composition_contract` creates a fresh tests-disabled Release
tree and inspects its public header, compile commands, build graph, and raw
application bytes. It fails if the fixture translation unit, test
definitions, fixture argument/status strings, recorded label/identity, fixture
paths, or fixture-access symbols survive.

The macOS current-tree public fixture runs passed for both profiles and
reported the fixed success markers above. The current-tree Linux v2 run also
passed both profiles with the exact host labels and rendered state in an
Ubuntu 26.04 x86_64 full-system QEMU guest. Its fresh tests-off Release build
completed 36/36 targets, omitted the fixture seam, reported `PUBLIC_PATH_OK`
and `NATIVE_PLATFORM_VAULT_SELECTED:true`, and passed dependency, runtime, and
ELF audits. These Linux PNGs remain layout/composition evidence only, not
presentation-polish or system-theme certification. Windows public-fixture
execution remains BPR-0036.

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
