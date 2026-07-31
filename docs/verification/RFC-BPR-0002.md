# RFC-BPR-0002 Verification Plan and Ledger

RFC base: `bfa9ab3ad6b08fbb22b697ed6fe6688202d3335a`

RFC-BPR-0002 remains `Proposed`. BPR-0029 closes the public-composition
implementation slice, and BPR-0030 closes the focused hostile-verification
slice, not the RFC. BPR-0031 has closed the available-platform
presentation/accessibility and native macOS appearance gate. BPR-0037 has
closed the milestone-one CLI decision: native HTTPS is the sole production
authentication transport, every visible CLI choice remains fixed at
zero-launch `cli_unavailable`, and the public self-test now reports the exact
visible count (`1` by default and `2` with explicit GHES). Positive CLI
enablement is deferred to RFC-BPR-0015. Windows target-native evidence and the
remaining ordered issue set still block `Auto-Verified`, but do not block
later implementation work.

## Architecture allocation

| Region | Planned responsibility | Direct evidence |
|---|---|---|
| Functional core | Opaque profile/host/account/repository/PR identity, semantic open/refresh/select intents, tri-state capability legality, request identity, explicit snapshot acceptance, invalidation, deltas, diagnostics | Core-only replay/property tests and forbidden-dependency scan |
| Application host | Normalized provider port, staged snapshots, process/network/vault effects, sessions, retries, rate limits, cancellation, caches, lifecycle, serialized completions | Fake-port integration with reordering, identity switching, expiry, failure, cancellation, and commit-after-acceptance assertions |
| GitHub adapter | Profiles, REST/GraphQL contracts, capability probes, fixed CLI-unavailable boundary, dormant broker test surface, native HTTPS, bounded normalization | GitHub.com/GHES recorded-contract and hostile-transport tests |
| Platform credential adapters | Store/retrieve/delete credentials only through macOS Keychain, Windows Credential Manager, or Linux Secret Service | Disposable target-native canary tests with post-test deletion |
| Qt presentation adapter | Semantic event translation, active identity, metadata/conversation projections, capability-driven actions, bounded errors, accessibility | Qt adapter tests and rendered real-application automation |
| Composition root | Explicit concrete profile/transport/vault/provider/host/core/Qt selection | End-to-end trace through the actual executable |

The exact allocation is recorded in [GitHub Host Implementation
Architecture](../architecture/github-host-implementation.md).

## Typed vertical slice

```text
open pull request / refresh / select identity QAction
  -> semantic core intent
  -> bounded active-identity and capability observation
  -> deterministic transition
  -> probe_host or fetch_pull_request request
     (request + authority session + host/account generation)
  -> application provider port
  -> fixed CLI-unavailable result or selected native HTTPS adapter
  -> immutable normalized provider completion
  -> serialized lane and host staging by exact request context
  -> generation-tagged core event
  -> explicit accepted-snapshot or stale-rejection decision
  -> accepted host cache commit
  -> identity/content/command/diagnostic deltas
  -> incremental Qt projection
```

No provider result becomes active merely because a callback completed or a
broad changed-domain bit was set. The host commits and publishes only the
snapshot named by the core's exact acceptance delta.

## Architecture-slice result

The dedicated GitHub adapter target and scoped instructions are present.
Automated boundary contracts prove that core, application, and Qt reject
`QProcess` and every `QNetwork*` type; only the designated GitHub process
runner may use `QProcess`; only the designated native HTTPS source may use
`QNetworkAccessManager`; and only the CLI transport may contain the exact
`GH_TELEMETRY=0` hardening setting.

The focused hostile boundary suite passed 11/11, the Qt-enabled matrix passed
15/15, and the Qt-free matrix passed 10/10. Documentation and source-pack
validation plus `git diff --check` passed. These results verify the allocation
and enforcement mechanism, not any still-unimplemented provider behavior.

## Deterministic-core slice result

The core now exposes a focused GitHub policy surface with strong identity for
profile, host, account, repository, pull request, provider snapshot,
capability revision, application session, request, and identity/request
generation. It owns profile renewal, session/open/refresh legality, all four
tri-state capabilities, complete stale-result comparison, explicit identity
invalidation, safe failure and canceled states, compact projections, and the
exact `accepted_snapshot_delta` required before host cache commit.

Direct tests cover all 81 capability combinations; malformed capability
representations; bounded and hostile repository locators; every independent
session/fetch context mismatch; duplicate, stale, failure, cancellation,
profile-switch, and canonical-target races; old-snapshot refresh preservation;
new-target clearing; and deterministic full-sequence replay. A compile-time
state-size bound and the three-variant read-only request surface prevent full
provider payload or mutation growth.

The hostile core-boundary harness rejects Qt, JSON/provider, database,
filesystem, clock, randomness, concurrency, environment, process, socket,
embedded HTTP endpoint, and native-vault dependencies, including qualified
and unqualified environment/process calls. The Qt-enabled matrix passed 17/17,
the Qt-free matrix passed 12/12, the hostile core boundary passed 4/4, and
documentation/source-pack/diff gates passed. This verifies core policy only;
normalized provider, host, Qt, and public-path claims remain planned.

## Normalized-contract slice result

The application boundary now defines standard C++ records and a narrow
provider port for the ten RFC read operations. Stable opaque node identities,
bounded actor facts, repository-scoped search relationships, nullable provider
facts, line/file review-thread subjects, capability evidence, rate-limit
facts, typed failures, exact request context, cancellation, and aggregate
security limits all fail closed before staging.

The GitHub adapter compiles only canonical read semantics. REST requests are
exact `GET` routes with the required media type, user agent, and REST version;
the sole `POST` is an exact GraphQL `query`. The GraphQL result carries
repository and pull-request node IDs that must match REST. Paired sanitized
GitHub.com and GHES 3.18 manifests drive the real recorded facade and normalize
to one independently constructed result. Hostile tests cover request drift,
mutation operation types, duplicate JSON, structural limits, unknown enums,
identity replay, cross-kind global-node collisions, actor conflicts,
malformed relationships, selected-search aliasing, signed GraphQL integer
bounds, explicit page completeness, result truncation, and legitimate
null/fork/file-level/non-User shapes.

The Qt-enabled matrix passed 20/20, the Qt-free matrix passed 14/14, and the
fixture security gate passed 10/10. This verifies the exact selected v1
contract, not live transport completeness. BPR-0027 now exhausts REST and
GraphQL pagination, validates raw envelopes, projects allowlisted fields, and
stages the complete result through the application host. BPR-0029 now supplies
the public GitHub.com and GHES fixture path.

## Host-profile and capability slice result

The GitHub adapter now validates explicit GitHub.com and GHES 3.18 profiles
before capability-probe request compilation. Only canonical lowercase ASCII
FQDNs, trusted profile provenance, exact HTTPS REST and GraphQL roots, and the
selected `2022-11-28` server contract are admitted. Provider content cannot
create a profile, GraphQL redirects are forbidden, and REST redirects remain
query-free on the validated authority and inside its API namespace.

One static, bounded GraphQL introspection query observes the exact
review-thread, viewed-state, mark/unmark, and resolve/unresolve schema
contracts without executing a mutation. Complete compatible evidence yields
`Supported`; complete missing or incompatible evidence yields `Unsupported`;
and every incomplete, operational, malformed, or over-limit result remains
explicitly unavailable with all four capability states `Unknown`. The
application cache admits only complete recorded or observed facts and keys
them by exact profile revision, host, authenticated account, server contract,
and probe-contract revision with bounded logical freshness and capacity.

The fully supported selected read contract also normalizes per-file viewed
state and per-thread resolve/unresolve target flags. Those facts remain
distinct from host schema support. BPR-0027 implements authenticated `/user`
observation, probe execution, private cache orchestration,
capability-shaped GraphQL selection and unavailable-target normalization,
transport paging, and binding the three independently validated authorities
through the production application host.

The clean Qt-enabled matrix passed 23/23 and the clean Qt-free matrix passed
15/15. Architecture boundaries passed 22/22, the hostile core boundary
passed 4/4, capability/read fixture security passed 12/12, and platform
configuration passed 4/4. Documentation, original source-pack,
`git diff --check`, untracked/tracked workflow-absence gates, and the
conservative completed-slice simplification review passed.

## Portable credential-vault contract slice result

The BPR-0021 application boundary defined one platform-neutral
credential-vault capability. Its historical authority was the exact profile,
profile revision, host, authenticated account, and server-contract tuple.
Every store, retrieve, delete, cancellation, and terminal outcome retained
exact request, application-session, and identity-generation correlation.
BPR-0029 extends the current authority with a canonical hostname as described
below.

Credential material is a non-default-constructible, noncopyable, move-only
owner of nonempty material bounded to 2,560 bytes, the smallest supported
native-vault limit. It exposes only a read-only byte span, empties moved-from
objects, and overwrites owned storage before reset, replacement, or
destruction. The port represents store, retrieval, absence, deletion, prior
absence, bounded failure, and cancellation without raw operating-system error
text or a credential-bearing core or Qt type.

The queued fake-vault suite directly exercises replacement, exact authority
isolation, reordered completions, duplicate work, injected failure,
cancellation races, and late-completion suppression. Source-boundary checks
reserve native Keychain, Credential Manager, and Secret Service APIs for
their designated adapter sources and reject every credential-contract type
from core and Qt. This slice verifies only the portable lifetime, authority,
and result contract. BPR-0022 now supplies the separate macOS lifecycle
evidence below. The Windows implementation and fixture are prepared, but
Windows remains `Planned` until BPR-0036 supplies direct target-native
evidence. BPR-0024 supplies the separate historical Linux v1 lifecycle
evidence below; BPR-0029 owns the current v2 public composition.

The clean Qt-enabled matrix passed 24/24 and the clean Qt-free matrix passed
16/16. The queued fake-vault test passed under address and undefined-behavior
sanitizers, and optimized-code inspection retained the explicit overwrite
loop before deallocation. Architecture boundaries passed 27/27, the hostile
core boundary passed 4/4, capability/read fixture security passed 12/12, and
platform configuration passed 4/4. Documentation, original source-pack,
`git diff --check`, and untracked/tracked workflow-absence gates passed.

## Hostname-bound v2 credential authority and public lifecycle result

BPR-0029 extends profile, provider, transport, host-resume, credential, and
vault authority with a fixed canonical hostname. GitHub.com requires exact
`github.com`; GHES rejects that authority and any `*.github.com` subdomain.
The hostname owner is allocation-free and bounded to 253 canonical lowercase
ASCII bytes.

The current platform-vault key encodes profile, profile revision, host,
account, server discriminator, hostname length, hostname bytes, and fixed
padding as 574 lowercase hexadecimal characters. macOS uses disjoint v2
production/integration services, Windows uses disjoint v2 target prefixes,
and Linux uses disjoint v2 schemas. No adapter reads, enumerates, migrates, or
falls back to the historical hostname-free v1 namespace. The v1 production
vaults were never composed into the public executable. Every directly
executed v1 integration fixture completed exact cleanup; the Windows v1
fixture remained preparatory and created no target-native item.

Public native-HTTPS enrollment begins in the Qt-owned password-echo prompt.
Its move-only sensitive buffer is bounded by character and UTF-8 byte count,
converted immediately to application credential material, and cleared on
every terminal path. The public host selects
`delete_retained_on_shutdown`; exact deletion is part of quiescence, and
composition keeps the decision lane, vault, binding catalog, provider, and
separate provider/vault workers alive until that terminal returns. It then
stops the provider and closes and joins the workers.

There is no durable selector, vault enumeration, or restart recovery. A failed
orderly delete is surfaced and retains the exact catalog tombstone. An abrupt
process or host crash can leave a native-vault item that BetterPullReview
cannot discover or resume. The current macOS v2 contract and native
setup/lifecycle/cleanup passed. The current Linux v2 target-native contract,
native lifecycle, exact cleanup, and public composition also passed in an
Ubuntu 26.04 x86_64 full-system QEMU guest. Windows current-format native
execution remains BPR-0036.

## Historical macOS Keychain v1 slice result

BPR-0022 adds an Apple-only native adapter for one explicit file-based
Keychain v1 contract. It resolves the exact default `SecKeychainRef`, adds one
generic-password item through `kSecUseKeychain`, and scopes retrieve, update,
and delete to a singleton `kSecMatchSearchList`. Production and integration
services are fixed and distinct. The 66-character lowercase hexadecimal
account encodes the complete portable authority with explicit GitHub.com and
GHES contract discriminators independent of C++ enum ordinals.

Every request is noninteractive. Store uses add, duplicate/update, and one
item-not-found add retry without delete-before-replace. Native statuses map to
bounded portable failures without raw status text. The source neither selects
the data-protection Keychain nor sets synchronization, access-group, or
accessibility policy and has no alternate vault, plaintext, settings,
environment, SQLite, process, UI, or logging fallback. Blocking Security
framework calls run through the new host effect-executor interface, not the
serialized decision or GUI lane; callbacks remain typed and must be marshaled
by a future consuming host to its decision lane.

The fake-backed contract suite covers exact encoding, complete selected status
families, authority isolation, malformed and same-/cross-kind duplicate work,
replacement, absence, deletion, injected failures and exceptions for every
operation, queued and running cancellation, reordering, sibling isolation,
reentrancy, atomic concurrent teardown handoff, and observable fake-buffer
wiping. The target-native fixture directly passed setup, add/retrieve,
in-place replacement/retrieve, delete/absence, same-binary parent cleanup, and
independent cleanup against one disposable integration item in the default
Keychain.

Runtime canaries traveled only over framed standard input. The parent inspected
its and the child's arguments, the explicit child environment, fixed captured
output, executable, source, and isolated scratch/XDG tree. Production source
and binary boundaries supply the settings/SQLite/UI/no-fallback evidence; the
scratch scan does not claim to inspect the user's complete home directory.
Final cleanup verified the exact integration item absent and left no matching
audit directory.

The current test executable is arm64, unbundled, and ad hoc linker-signed with
no bound `Info.plist`, team identifier, or provisioning profile. Its legacy
Keychain ACL uses the exact code-directory hash. Cleanup is therefore proven
for the same binary and CTest fixture, not for a crash followed by rebuilding
to a different hash. Stable signed identity, cross-build recovery, and any
future data-protection-Keychain migration remain package lifecycle work under
RFC-BPR-0014.

Binary inspection of the native test showed the selected `SecItemAdd`,
`SecItemCopyMatching`, `SecItemUpdate`, `SecItemDelete`, and
`SecKeychainCopyDefault` imports plus Security and CoreFoundation framework
dependencies. This paragraph records the historical BPR-0022 v1 evidence.
BPR-0029 now selects the same Keychain factory through the public composition
with the disjoint v2 hostname-bound key and reran the macOS native
setup/lifecycle/cleanup.

The Qt-enabled `dev` matrix passed 28/28, the Qt-free `core-only` matrix passed
16/16, the focused macOS contract/setup/lifecycle/cleanup matrix passed 4/4,
and the hostile architecture-boundary suite passed 40/40. Direct architecture,
documentation, source-pack, diff, and tracked/untracked workflow-absence gates
passed. A conservative simplification pass replaced manual CoreFoundation
ownership with scoped RAII, consolidated terminal extraction/failure
normalization, and preserved the explicit Security flow. The concurrent
teardown regression also passed 25 consecutive runs.

## Historical Windows Credential Manager v1 implementation status

BPR-0023 now contains a WIN32-only adapter for the wide-character Credential
Manager contract. Production and integration-test target prefixes are fixed
and disjoint; both append the complete 66-character portable authority
encoding. The adapter stores one `CRED_TYPE_GENERIC`,
`CRED_PERSIST_LOCAL_MACHINE` credential. It uses one `CredWriteW(..., 0)`
create-or-replace call, exact-target `CredReadW` and `CredDeleteW`, immediate
`GetLastError` capture, bounded `SecureZeroMemory`, and `CredFree`.

The adapter names the documented Windows 10/11 `5 * 512`-byte credential blob
limit directly. This preserves the portable 2,560-byte contract when a MinGW
header exposes the obsolete 512-byte macro. A shared structural predicate
prevents malformed native pointers or oversized extents from driving a read
or wipe. Documented errors map conservatively to absence, unavailable,
access-denied, corrupt, allocation, invalid-request, or operation-failed
outcomes. Credential Manager does not document a CRUD status that honestly
means locked storage, so no native status maps to `locked`.

Blocking Credential Manager work runs only through the host effect executor.
Fake-backed tests cover target encoding and bounds, selected status families,
invalid and duplicate work, authority isolation, replacement, absence,
deletion, backend and executor failures, cancellation, reordered completion,
reentrancy, atomic concurrent teardown, malformed results, and wiping.
Hostile boundaries require the selected wide lifecycle and reject Credential
UI, enumeration, Credential Locker, DPAPI, registry, file, settings, SQLite,
environment, process, network, logging, and alternate persistence.

The prepared native fixture uses only the integration-test target and one
exact synthetic authority. A resource-locked setup/lifecycle/cleanup sequence
sends runtime canaries to a same-executable child over standard input,
including replacement with material at the complete 2,560-byte bound,
performs create/read/replace/read/delete/absence, bounds and reaps the child,
performs exact parent cleanup, scans fixed output and an isolated scratch tree,
uses random 32-byte prefix and suffix probes to detect truncated leaks from
either credential, and verifies successful scratch removal. Broader settings,
SQLite, UI, Event Log, and diagnostic absence is not claimed from this runtime
fixture alone; it is paired with the production source boundary.

Both Windows test translation units passed strict host-stub syntax checks.
The adapter, application support, contract test, and native test also
cross-compiled and linked as x64 PE executables with MinGW. Cross-PE
inspection showed the selected `CredWriteW`, `CredReadW`, `CredDeleteW`, and
`CredFree` imports from Advapi32; the native fixture additionally imports
`BCryptGenRandom`, `CreateProcessW`, process-attribute-list operations,
termination, and wait support. This is preparatory evidence only.

The complete fake-backed contract executable also linked the real production
adapter and application sources against temporary Win32 shims whose
`CredWriteW`, `CredReadW`, `CredDeleteW`, and `CredFree` functions aborted if
called. It exited successfully under AddressSanitizer and
UndefinedBehaviorSanitizer with strict project warnings, directly confirming
that every portable contract case stayed on the injected fake backend.
MinGW's GCC static analyzer also reported no production-adapter finding.
Temporary binaries, shims, object files, and debug artifacts were removed.

No Windows SDK/MSVC environment or native Windows runtime is available on the
current macOS host. The checked-in `windows-dev` configure/build/test matrix,
native Credential Manager lifecycle, exact cleanup, and target-native
PE/runtime inspection have therefore not run. BPR-0023 closes only the
completed implementation slice. The claim below remains `Planned`, and
BPR-0036 retains every target-native Windows check required before RFC
closeout.

BPR-0029 changes the current Windows production and integration target
prefixes to disjoint v2 namespaces followed by the 574-character
hostname-bound authority. The historical v1 cross-PE and fake evidence above
is not relabeled as current-format native evidence. BPR-0036 retains the v2
native lifecycle, cleanup, public fixture, HTTPS, process, and dependency
checks.

## Historical Linux Secret Service v1 slice result

BPR-0024 adds a Linux-only adapter using stable libsecret APIs and one fixed
Secret Service v1 schema. Production and integration schemas are fixed and
disjoint. The sole explicit string attribute is the complete 66-character
lowercase hexadecimal portable authority; libsecret's schema marker is the
only additional attribute. Every operation targets the exact default
collection.

The adapter neither creates nor unlocks a collection and has no alternate
vault, settings, SQLite, environment, file, process, network, Qt, diagnostic,
or plaintext fallback. It opens the service without preloading collections,
checks that the resulting proxy has the exact canonical
`org.freedesktop.secrets` name, and only then ensures a session and accesses
the default collection. The derived service denies prompt execution. A
returned prompt receives at most one bounded 1,000 ms `Dismiss` call with
no-auto-start semantics; `Prompt` is never called.

Store uses replace semantics and a `SecretValue` with the binary content type.
Search requires zero or exactly one exact match, loads that exact item, and
checks both collection and item lock state. Direct gnome-keyring 50 and
libsecret 0.21.7 inspection showed that the backend preserves the bytes while
normalizing the stored binary content-type metadata to `text/plain`.
Retrieval therefore accepts only exact `application/octet-stream` or exact
`text/plain`; every other type fails closed. Native failures remain bounded
and typed, including absent, locked, denied, unavailable, corrupt,
allocation, invalid-request, and operation-failed outcomes.

The fake-backed contract and resource-locked native fixture passed
create/read, in-place replacement/read, delete/absence, same-binary cleanup,
and independent cleanup. The runtime-canary lifecycle used 73 bytes and a
replacement at the complete 2,560-byte portable bound. A second native
lifecycle repeated both sizes with opaque values containing NUL, high-bit,
and invalid UTF-8 bytes. Runtime canaries traveled only over framed standard
input. Arguments, environment, fixed output, executable, source, and an
isolated scratch tree were scanned; source and binary boundaries supply the
broader no-settings/SQLite/UI/logging/fallback evidence. Injected effects and
direct native-status classification cover access-denied and failed-delete
terminals; those two are not claimed as desktop-policy fixtures.

The no-session-bus case returned unavailable. A real default collection was
locked through its D-Bus interface, its locked property was observed, and the
adapter returned locked without attempting unlock. A live hostile owner of an
environment-selected service name observed only
`org.freedesktop.DBus.Properties.GetAll`; it observed no `OpenSession`,
`ReadAlias`, `SearchItems`, `CreateItem`, `GetSecrets`, or `Delete`. This
directly proves that exact canonical-name validation precedes session and item
operations even when the dependency redirects service construction.

The direct Linux run used a full-system Ubuntu 26.04 x86_64 QEMU guest with
Linux 7.0.0-28, Qt 6.10.2, libsecret 0.21.7, gnome-keyring 50, GCC 15.2,
CMake 4.2.3, and Ninja. The fixture ran as a dedicated non-root login inside
Xvfb, a D-Bus session, and gnome-keyring. This is direct execution against the
guest operating system and native libraries with an emulated x86_64 CPU; it
is not a bare-metal CPU or physical-desktop claim.

The Linux `dev` matrix passed 28/28, the `core-only` matrix passed 16/16, and
the focused contract/setup/lifecycle/cleanup matrix passed 4/4. The hostile
architecture suite passed 74/74 after the final open/name-check/session
ordering rule, and the direct CMake verifier passed. The fake contract
executable also passed 25 consecutive runs under AddressSanitizer and
UndefinedBehaviorSanitizer. Checked-in setup and cleanup proved the exact
fixture authority absent. A separate controlled D-Bus audit proved the exact
integration schema absent after focused cleanup and after the complete
matrix.

The native test is an x86-64 ELF PIE dynamically linked to libsecret, GIO,
GObject, and GLib. Inspection found the selected service, collection, item,
secret-value, and D-Bus proxy imports. This is historical v1 evidence.
BPR-0029 separately passed the current v2 Linux vault and composed path in an
Ubuntu 26.04 x86_64 full-system QEMU guest; that current-tree result is not
inferred from and does not relabel the historical result. BPR-0026 supplies
the separate Linux native HTTPS TLS/proxy/redirect/runtime evidence below.

## GitHub CLI boundary and dormant broker slice result

The adapter now has a standard-C++ correlated wire transport and injected
process port. Every wire terminal retains the parent session or pull-request
context, profile revision, host, operation, and page. The concrete retained
runner implementation owns `QProcess`; it uses an absolute canonical executable, an
element-wise argument list, a complete requested environment allowlist, a
canonical non-repository working directory, separate captured streams,
bounded standard input and retained output, a total deadline, cancellation,
and bounded call-side kill/reap. Only a clean terminal exposes bounded
stdout. Every failure discards partial output and raw process diagnostics. The
runner is tested scaffolding and is not composed into the milestone-one public
executable.
The runner owns one serialized worker. If bounded reap fails, it returns a
fixed `reap_failure`, quarantines the slot, retains the live `QProcess`, and
continues kill/reap work on that same worker instead of triggering Qt's
destructor wait or creating unbounded reapers. That actual live-process
failed-reap handoff is source- and hostile-architecture-gated; the compiled
runtime suite does not force it. A distinct compiled seam directly proves the
completion watchdog quarantines a stalled worker, rejects another request
without launch, and permits recovery only after the worker resumes.
Allocation-free request-shape validation and pre-cancellation occur before
work-item allocation and request copying; marker-backed N+1 cases make zero
child launches.

On Unix, a compiled child directly observes closed inherited descriptors,
disabled core dumps, reset signal handlers, and that it is the new session and
initial process-group leader. Compiled lifecycle cases prove clean exit,
failure, timeout, and cancellation signal descendants that remain in that
initial process group while the direct child is reaped; an escape case proves
a descendant that moves to another process group survives before explicit
fixture cleanup. Shutdown/group-resolution paths and actual live-process
failed-reap retention remain source- and hostile-architecture-gated. Queued
work is checked against its absolute deadline immediately before launch. The
macOS raw child environment
contains only the requested facts plus
Darwin's deterministic `__CF_USER_TEXT_ENCODING` addition. The Linux
raw-runner fixture requests its locale explicitly; production does not add
that fixture-only fact. Reads use a fixed buffer and ask Qt for at most the
remaining accepted bound plus one byte. Qt exposes no cap for its private
in-flight pipe allocation, so that internal memory is not claimed as
hard-bounded.

The invocation compiler revalidates the canonical wire request and exact
profile/correlation. It emits only the selected `gh api` relative endpoint,
hostname, method, canonical noncredential headers, and GraphQL stdin marker.
GitHub.com and GHES REST prefixing and both GraphQL routes are translated
without ambient repository expansion. The child environment begins empty and
contains fixed noninteractive settings plus typed target platform
credential-session and selected HTTPS proxy/trust facts. Unknown parent
variables, token and authentication surfaces, host/repository/config
overrides, debug/status/cache/pagination/templating flags, shell execution,
and credential-bearing proxy values are absent.

The exact selected macOS executable was
`gh` 2.95.0 with SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`.
The direct fixture copied those exact bytes into an owner-private directory,
executed only that copy, and confirmed the same digest after all probes. A
loopback self-signed certificate produced one fixed bad-certificate TLS alert
and zero HTTP requests. A distinct-host loopback `302` was followed once: its
selected origin received the exact 47-byte body and synthetic authorization,
while the sink received neither. Equivalent `307` and `308` POST requests
reached their selected origins but produced no observed sink request; that is
recorded as unproven rather than redirect rejection. Official 2.95.0 source
inspection also found the default Go redirect client and no stable
final-authority surface in the allowed command output.

Every child ran from the owner-private canonical non-repository fixture
directory. The loopback full URL is necessarily verification-only: `gh`
rejects its ephemeral `host:port` as `--hostname`, so the synthetic
`github.com` startup hostname remains. The observation plus selected-version
source inspection characterizes the exact binary's shared redirect client;
it is not exact production-authority proof and cannot enable CLI mode.

The current direct probe proves that persisted `http_unix_socket` is an
effective transport override, not merely documented configuration. Both exact
`gh` 2.95.0 and 2.96.0 binaries loaded the isolated setting and sent one
authenticated `GET /user` request through an AF_UNIX sentinel. Because the
production child would require selected platform account/configuration paths,
omitting `GH_CONFIG_DIR` cannot prove that rerouting input absent.

BPR-0037 also audited the official macOS arm64 `gh` 2.96.0 release dated
2026-07-02. Its published ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`;
the extracted binary SHA-256 was
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`.
The API/account-critical source is unchanged from 2.95.0. `gh api` rejects
`--user`; `gh auth switch --user` mutates persisted active-account state;
`gh auth token --user` prints the secret; selected-user keyring failure can
fall back to the unkeyed host credential; and macOS keyring access starts
`/usr/bin/security`. Exact 2.96.0 loopback observations matched 2.95.0:
certificate rejection before HTTP, a followed cross-authority `302`, and
unproved `307`/`308` sink behavior.

By product-owner approval, native HTTPS is the only milestone-one production
authentication transport. The public composition creates no CLI
configuration, transport, or process runner. It sends CLI selection to the
provider without credential material, and the provider returns
`cli_unavailable` synchronously before a transport-factory call or scheduled
provider effect. Clean-start public selection performs one bounded
noncredential profile-resolution effect before that provider request. An
interactive switch from authenticated native HTTPS may first exact-delete the
predecessor credential binding; it never enrolls or retrieves a credential for
the CLI profile. The tests-disabled
Release contract rejects dormant CLI transport, configuration,
request-compiler, and runner symbols in the executable.

The dormant transport now exposes eight typed reasons: the milestone-one
scope decision plus executable-image identity, cross-authority redirect,
ambient configuration/trust material, credential/account generation,
working-directory identity, descendant containment, and numeric PID/PGID
reuse. Its direct fake process port remains at zero calls for every operation,
cancellation, and invalid request. These technical gaps are eligibility
inputs to deferred RFC-BPR-0015, not RFC-BPR-0002 closeout blockers. Positive
Windows CLI execution moved there as well; BPR-0036 retains only independent
native HTTPS, Credential Manager, public-composition, runtime, and dependency
evidence.

## Native HTTPS transport slice result

BPR-0026 adds the internal native `github_wire_transport_port` behind a
standard-C++ boundary. One dedicated Qt worker owns one
`QNetworkAccessManager`; complete request/correlation validation,
single-operation admission, absolute-deadline and cancellation checks,
one-shot bootstrap credentials, exact stored-account authority, and typed
redacted terminals all precede or contain network effects.

Production asks Qt for ordered system proxy candidates on every accepted hop,
admits at most 16 supported direct, HTTP, or SOCKS5 entries, and falls back
only after retryable proxy or connection failure. Every attempt uses
manager-local proxy state, explicit system roots, `VerifyPeer`, TLS 1.2 or
later, HTTP/1.1 ALPN, no HTTP/2, no cookie/cache/authentication reuse, and no
certificate-error bypass. A successful terminal requires the completed
`QNetworkReply::encrypted` handshake signal, a peer certificate, exact reply
URL, and consistent TLS policy state. Automatic redirects are disabled; at
most three manually validated REST `GET` redirects may stay on the exact
validated authority, remain query-free, and stay inside the API namespace.
GraphQL, cross-authority, looping, malformed, and over-limit redirects fail
closed.

The adapter admits request bodies through 256 KiB and response bodies through
4 MiB, with 64 response headers, 64 KiB aggregate header text, 8 KiB per
field, fixed 16 KiB reads, and a 30-second absolute deadline. Synchronous Qt
system/PAC proxy discovery and system-root enumeration cannot be interrupted,
so only the remaining deadline applies after they return. The bounds govern
bytes accepted and retained by the adapter, not Qt-private parser, socket, or
TLS buffers. Qt may normalize duplicate response headers, and its internal
Authorization copies cannot be exactly wiped; those limitations are recorded
rather than overclaimed.

The test-only seam preserves the production request, TLS, redirect,
cancellation, response, and credential-binding path while substituting
ordered loopback CONNECT proxies and one disposable fixture CA. Its 15
scenarios cover trusted success, ordered proxy fallback, allowed and rejected
redirects, certificate rejection, the response-body limit, one
8-KiB-plus-one header-field limit, cancellation, timeout, HTTP 401/403/429
classification, content-encoding rejection, and one-shot bootstrap replay
rejection. There is no separate 65-field-count or aggregate-header-only
loopback scenario; those two adapter checks remain source/contract evidence.

On macOS 15.7.7 arm64 with Qt Network 6.11.1, the focused contract and fixture
passed 2/2, all 15 scenarios passed, and the runtime reported
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
A tests-disabled Release audit found no test-only transport factories and
recorded the selected Qt Network, OpenSSL, Security, and CoreFoundation
dependencies plus the current ad hoc linker signature. The focused tests also
passed under AddressSanitizer and UndefinedBehaviorSanitizer.

The final simplified source and harness also passed directly in the Ubuntu
26.04 x86_64 full-system QEMU guest with Linux 7.0.0-28, GCC 15.2, Qt
6.10.2, and OpenSSL 3.5.5. The frozen transport source SHA-256 was
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`;
the harness SHA-256 was
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The focused contract/fixture passed 2/2 in 0.12/11.29 seconds, the direct
harness reported `native-https-transport-fixture:pass`, and the architecture
gates passed 2/2 in 5.00/144.35 seconds with all 92 hostile mutations.
Runtime facts matched macOS. A tests-disabled Release build omitted the
testing macro and factories; ELF inspection recorded Qt6Network and Qt6Core,
and a controlled single-success trace opened Qt's OpenSSL backend,
`libssl.so.3`, and `libcrypto.so.3`. The ordinary suite is the behavioral
evidence because full-suite tracing perturbs cancellation timing.

This verifies the macOS and Linux internal wire transport. BPR-0027 adds
pagination, normalization, and host staging, BPR-0028 adds the safe rendered
Qt projection, and BPR-0029 adds the public composition. Windows 10/11 x64
execution and dependency inspection remain open in BPR-0036. By product-owner
direction, that Windows work is nonblocking for every other implementation
slice and blocks only its affected Windows claims and final RFC-BPR-0002
closeout.

## Application host and provider slice result

BPR-0027 adds the application-facing provider and serialized GitHub host. The
provider resolves validated profiles, performs strict one-shot bootstrap and
account-authenticated `/user` observations, probes capabilities, exhausts
every REST and GraphQL continuation, and returns one bounded normalized
snapshot. It validates exact repository/pull-request authority, Search URL and
pull-request markers, expected changed-file/commit totals, expected checks
head OID, cursor progress, page/record/byte ceilings, and capability-shaped
GraphQL selection before returning success.

Rate-limit state is three independent `core`, `search`, and `graphql`
observations. GitHub.com requires each applicable exact tuple; GHES may omit a
whole tuple but cannot provide a partial, duplicate, malformed, unknown, or
wrong-resource tuple. Provider and host accumulation spans pages and retries.
Typed primary, secondary, and structured GraphQL `RATE_LIMITED` failures
preserve an exact causal record, while bounded `Retry-After` remains separate.

The host owns retries, cancellation, stale suppression, snapshot staging,
capability-cache commit, projections, and shutdown. Its capability cache is
fresh and private; a caller-populated cache cannot bypass the authenticated
probe. Projection exceptions are latched and cannot interrupt effects or
cleanup. Reentrant ingress runs in bounded FIFO generations, older
continuations cannot be overtaken, and shutdown closes ingress with terminal
priority. Conforming provider/vault submissions yield exactly one terminal,
and the decision lane remains live through quiescence.

Native enrollment uses transient submitted material only to observe strict
`/user`, then stores under that observed numeric account. Resume accepts only
a profile; a noncopyable host-owned catalog selects the exact authority
established by a prior accepted observation. Hostile tests cover two retained
bindings while retrieval of A is active and B is selected: A is exact-deleted,
B survives an ordinary profile failure, and later invalidation exact-deletes
B. Replacement, absence, malformed/duplicate terminals, cleanup failure,
shutdown, and late destruction retain exact deletion or tombstone behavior.

The catalog proves same-process recreation only. It is not a durable selector,
and the vault cannot enumerate an account-keyed item after restart. BPR-0029
therefore selects exact shutdown deletion in the public composition and makes
its terminal part of host quiescence. This closes orderly lifecycle without
claiming crash recovery. BPR-0036 owns target-native Windows, BPR-0037 closes
the fixed CLI-unavailable milestone-one scope, and BPR-0038 closes the
redirect-policy decision with query-bearing HTTP redirect targets remaining
rejected.

On macOS arm64, the focused host/provider/pager, recorded-contract,
capability, and native-wire matrix passed 9/9. Fresh `dev` and `core-only`
configure, clean-build, and full-test matrices passed 35/35 and 17/17. The
focused host/provider/pager sanitizer matrix passed 3/3. A tests-disabled
Release build and the binary-dependency, runtime-surface, and public
application self-test gates passed without test factories or a testing
definition. Documentation, immutable source-pack, workflow-absence, and diff
gates passed. The final Ubuntu 26.04 x86_64 full-system QEMU build compiled
the selected production and test targets with GCC 15.2 under
warnings-as-errors, and the focused host/provider/pager, recorded-contract,
capability, and native-wire matrix passed 9/9 in 39.56 seconds under
Qt 6.10.2. That pass also closed GCC aggregate initialization and
test-temporary lifetime defects before commit. No GitHub Actions output is
accepted.

## Qt presentation slice result

BPR-0028 adds one provider-free application presentation contract and the
GitHub Qt window. The host strips effect requests and accepted-completion
internals, maps accepted session and pull-request records inside its exception
boundary, and publishes one atomic update. Qt can dispatch only profile
activation, pull-request open, and refresh intents; it cannot name a provider,
vault, credential, transport, URL, raw request, or completion event.

The window exposes profile selection and read-only active hostname, transport,
and authenticated account; repository and pull-request input; Connect, Open,
Refresh, and Qt-local review-thread focus actions; capability tri-state and
rate-limit state; normalized metadata and read-only conversations; and
presentation-owned status and diagnostic wording. Remote title, body, author,
path, and comment text remain literal plain text. A new target and an identity
change clear prior account, pull request, conversation, and rate observations;
a refresh keeps the last accepted content through loading, failure,
cancellation, or stale completion.

The conversation model uses distinct stable IDs for reviews, threads, nested
review comments, and pull-request comments. It computes insert, remove,
minimal-move, and data-change notifications without reset. Longest-increasing-
subsequence reconciliation makes both 50,000-row rotation directions one
move. BPR-0028 directly ran 3,000 mixed cases, 5,000 arbitrary 100-row
permutations, and both 50,000-row rotations. BPR-0030 now checks that
historical corpus into the standalone
`qt_github_conversation_model_property` target, with fixed seeds, exact
root/child order and every public role, deletion-aware display/accessibility,
deterministic mutable-role coverage, exact changed-row counts, minimal
permutation moves, persistent-index preservation or invalidation, and zero
model resets.

On macOS arm64, a fresh Qt-enabled build passed 38/38 and the focused host,
three-scale Qt, and two architecture cases passed 6/6. A fresh Qt-free build
passed 17/17. The focused host and three Qt cases passed 4/4 under
AddressSanitizer and UndefinedBehaviorSanitizer. A tests-disabled Release
build, public self-test/render, binary-dependency, runtime-surface, and
test-seam absence gates passed. The 1.0x, 1.25x, and 2.0x minimum-top,
minimum-lower, and normal rasters were inspected directly.

The exact selected targets also compiled in the Ubuntu 26.04 x86_64
full-system QEMU guest under GCC 15.2, Qt 6.10.2, and warnings-as-errors.
Architecture checks passed 2/2 and the host/three-scale matrix passed 4/4.
A direct backtrace found Qt's offscreen Fusion cached painter could crash
while painting the combo box at 1.25x. The Linux CTest environment now uses
Qt's basic Windows style; the fractional-scale case then passed 20 consecutive
runs, and its target-generated rasters were inspected. This is deterministic
headless layout evidence, not native-style or public-application UX
certification; BPR-0031 supplies that separate native gate.

Documentation, immutable source-pack, workflow-absence, and diff gates passed.
Independent contract, portability, model, and skeptical reviews found no
remaining P0/P1 defect. Windows 10/11 x64 target-native execution remains
open in BPR-0036 and is nonblocking for later implementation; GitHub Actions
provided no evidence and has no role.

## Public application composition slice result

BPR-0029 replaces the foundation-only executable wiring with one explicit
GitHub composition. Built-in-provenance GitHub.com native-HTTPS and CLI
profiles are always present. Supplying one validated canonical GHES hostname
adds explicit-user-provenance GHES 3.18 native-HTTPS and CLI profiles. The
multi-profile provider owns immutable validated copies, rejects duplicate
identities, and routes each request by exact profile.

Only native HTTPS is usable. The CLI profiles are visibly labeled
unavailable; public activation produces the bounded `cli_unavailable`
presentation and starts zero processes. Native-HTTPS activation uses the
Qt-owned sensitive input, immediately converts its move-only buffer to
application credential material, and selects the target platform vault. The
composition owns separate provider and vault workers, the provider, the Qt
event-loop decision lane, the GitHub window, the binding catalog, and the
serialized host in a destruction-safe order.

The public host chooses exact deletion of retained credentials on orderly
shutdown. It remains non-quiescent until the exact v2 authority's delete
terminal returns; only then does composition stop the provider and close and
join both workers. A failed delete is reported and retains the tombstone.
There is no durable binding selector, vault enumeration, v1 migration or
fallback, or restart recovery, so a crash can leave an item the next process
cannot discover.

The two test-enabled public fixtures drive the real executable through
GitHub.com and GHES 3.18 profiles whose authentication mode is native HTTPS,
while substituting a recorded transport and fixture vault. A real QAction
reaches the provider, strict `/user`, capability probe, complete normalized
pull request, host staging, exact core acceptance, incremental Qt projection,
rendered window, and shutdown delete. Both macOS runs reported
`PUBLIC_PATH_OK`, the matching exact fixture marker, and `RENDER_OK`; fixture
evidence confirmed the recording and capability probe were consumed,
bootstrap and account transports were selected, credential
store/retrieve/delete all completed, and no credential remained in
fixture-vault state. These are structural public-path results, not a
real-network or native-vault lifecycle run; separate native fixtures provide
that evidence.

The production self-test enumerated every visible CLI row with zero launch and
reported `CLI_UNAVAILABLE_COUNT:1` by default and
`CLI_UNAVAILABLE_COUNT:2` with explicit GHES.
At the BPR-0029 snapshot, the macOS `dev` matrix passed 43/43, and focused
prompt, provider, host, worker, credential, public-path, architecture, and v2
Keychain checks passed. The Release composition contract built a fresh
`BPR_BUILD_TESTS=OFF` executable and found no public-header test conditional,
fixture translation unit or macro in compile commands/build graph, and no
fixture argument, status, label, identity, path, or access symbol in raw
binary bytes.

The corresponding pinned BPR-0029 Linux v2 execution passed in an Ubuntu 26.04
x86_64 full-system QEMU guest. A fresh tests-on Release build completed 81/81
targets, and all 40 non-native registered test identities passed on that
snapshot after the repaired dependency and composition gates.
Secret Service setup/lifecycle/cleanup passed 3/3 along with independent exact
cleanup and the hostile, unavailable, and locked cases. Both GitHub.com and
GHES public fixtures passed with exact host and rendered state. A fresh
tests-off Release build completed 36/36 targets, omitted the fixture seam,
reported `PUBLIC_PATH_OK` and `NATIVE_PLATFORM_VAULT_SELECTED:true`, and
passed dependency, runtime, and ELF audits. The tests-off executable SHA-256
is `78a85e15319473e39baaa6b6c8723ff7f9adbade13b48475a3f94a5622719b01`.

Windows target-native execution remains BPR-0036 and is nonblocking for all
other implementation, documentation, security, and simplification work; it
blocks only affected Windows claims and final RFC Auto-Verified closeout. The
deterministic offscreen fixture PNGs are composition/layout evidence only,
not presentation-polish or native system-theme certification; BPR-0031
supplies that separate UX gate.

## Focused hostile-verification slice result

BPR-0030 closes the remaining focused-evidence gaps without widening the
read-only product surface. Recorded HTTP methods now have one exact parser:
only uppercase `GET` and `POST` become typed, while other verbs,
case variants, empty input, and padded spellings fail. REST catalog reads
still require typed `GET`.

GraphQL reads and capability probing share one byte-, depth-, and
definition-bounded structural classifier. Exactly one explicit `query` plus
bounded fragments is required. Mutation or subscription definitions anywhere
at top level, multiple operations, malformed scanned structure or token
framing, and limit exhaustion fail closed. Comments, normal and block
strings, fields, and names do not create false classifications. This scanner
is deliberately not a complete GraphQL grammar parser; exact equality with
the canonical compiled request body remains mandatory. All five
legacy/current request shapes prove that an added `__typename` read remains
scanner-approved but is rejected by canonical equality.

A table-driven application-host contract exhausts all 15 provider failure
values through pull-request completions. Each produces its exact bounded core
reason without an accepted host/core snapshot, a pull-request projection, or
provider-owned diagnostic detail; retryable failures exhaust the bounded
attempt count. The standalone Qt property target checks 3,000 fixed-seed mixed
updates, 5,000 arbitrary 100-row permutations, and both 50,000-row rotations.
It verifies exact tree/order and every public role, deletion-aware display and
accessibility facts, deterministic mutable-role variation, exact changed-row
counts, minimal moves, persistent indexes, and zero resets. The native HTTPS
verifier now includes a same-authority query-bearing REST redirect that must
fail after exactly one CONNECT/TLS/origin/auth request with no second hop.

On macOS arm64, the final `dev` matrix passed 44/44 in 67.24 seconds and
`core-only` passed 17/17 in 18.62 seconds. The final property and read-contract
deltas passed normally and under AddressSanitizer/UndefinedBehaviorSanitizer;
the current native HTTPS verifier passed all 32 scenarios. In the Ubuntu
26.04 x86_64 full-system QEMU guest, a fresh Release tests-enabled GCC 15.2
build completed 83/83 targets without a portability change. The complete
non-native matrix passed 41/41 in 541.09 seconds, the focused BPR-0030 matrix
passed 17/17, and final documentation/architecture/security gates passed 4/4.
All affected implementation, test, agent, and documentation files matched the
macOS tree byte-for-byte.

Documentation, immutable source-pack, Python compilation, diff, and tracked
and on-disk workflow-absence gates passed. The conservative simplification
pass retained the explicit host/redirect tests, consolidated the operation
classifier, and hardened malformed numeric rejection. Final skeptical review
found no remaining P0/P1 defect. GitHub Actions supplied no evidence and has
no role. Windows target-native verification remains open and nonblocking in
BPR-0036.

## Presentation-polish slice result

BPR-0031 replaces the functional-integration layout with a compact,
responsive interim GitHub integration surface without moving policy or
effects into Qt. RFC-BPR-0005 owns the complete native workbench and
virtualized review layout; RFC-BPR-0006 owns syntax presentation and the full
appearance profiles.
Connection, locator, status, pull-request hierarchy, description, and
conversations have distinct visual roles. Capability/rate observations and
exact commit identities remain available through keyboard-operable
disclosures. Locator actions move below their inputs at narrow widths and the
description/conversation splitter changes from horizontal to vertical.

The pull-request stack now exposes exact `disconnected`, `connecting`,
`empty`, `loading`, `error`, `cancelled`, `loaded`, `refreshing`,
`loaded-error`, and `loaded-cancelled` state properties. New-target loading,
failure, and cancellation use a focused empty state. Refresh loading, failure,
and cancellation retain the last accepted pull-request projection, stable
selection, and local expansion. The first accepted projection establishes the
initial conversation hierarchy; subsequent projections do not reopen a thread
the user collapsed.

Conversation rows use a Qt delegate that wraps literal multiline remote text
without elision. The active native style still paints selection, background,
and focus, and the delegate reads the current style option rather than caching
colors or fonts. The window does not set an application or widget palette,
force a style, apply QSS, or replace the application font. Application
palette, font, style, and theme events refresh derived title fonts, native
icons, delegate geometry, and responsive layout. Each published
status/diagnostic update emits at most one combined polite accessible
announcement; identical text is suppressed.

The three deterministic scale cases directly assert every presentation state,
both disclosures, keyboard order, narrow/wide layout, retained and empty
failure/cancellation behavior, literal wrapped rows, nonoverlap, selection,
focus, manually collapsed expansion, zero resets, controlled palette/font
inheritance, and accessibility announcements. At 1.0x, 1.25x, and 2.0x they
save minimum-top, minimum-lower, loaded-success, disconnected, loading,
first-load-error, session-cancelled, first-load-cancelled,
retained-refresh-error, and retained-refresh-cancelled rasters. Those forced
offscreen-style images are inspected layout evidence only.

The final macOS `dev` matrix passed 44/44 in 70.17 seconds and the Qt-free
`core-only` matrix passed 17/17 in 17.29 seconds. The three scale cases plus
the property corpus passed 4/4 in 41.12 seconds under AddressSanitizer and
UndefinedBehaviorSanitizer.

In the Ubuntu 26.04 x86_64 full-system QEMU guest, a fresh tests-enabled
Release build completed all 84 build steps under GCC 15.2 and Qt 6.10.2. The
focused architecture, documentation, three-scale, property, both public-
fixture, and application-render matrix passed 11/11 in 202.34 seconds. GCC
rejected three declaration-inconsistent designated initializers in the new
test fixtures; they were reordered canonically and the same fresh guest build
then completed without a production portability change. The Linux normal,
minimum, loading, and retained-error rasters were inspected directly.

On macOS 15.7.7 arm64 with Qt 6.11.1, the real application loaded
`libqcocoa.dylib` and `libqmacstyle.dylib`, selected QMacStyle, inherited the
system-default Dark appearance at startup, and rendered the complete
GitHub.com fixture. The run reported `PUBLIC_PATH_OK`,
`GITHUB_FIXTURE_OK:github.com`, and `RENDER_OK`; the 2200x1440 raster SHA-256
was `309f70eae18dda39d8920802f1c8cb2840db1761340b17213719a9b4173804d7`.
A safe loaded-process probe changed the application-local color scheme from
Dark to Light while retaining QMacStyle; representative window/base pixels
changed from `rgb(50,50,50)`/`rgb(23,23,23)` to
`rgb(236,236,236)`/`rgb(255,255,255)`, and the complete presentation remained
clean. A settled 13-to-17-point application-font probe also refreshed
geometry and rows without clipping. An ad hoc QMacStyle-to-QFusionStyle probe
is not visual evidence: although it exercised the style event, its inherited
dark control palette produced inadequate input contrast.

The final owner-authorized native gate started the public GitHub.com fixture
under real system Light, waited for independent Qt and global-Light readiness
before either evidence file existed, and changed the operating-system
appearance to Dark while the same process remained loaded. The exact run
reported both `Light->Dark` transitions, Cocoa, QMacStyle, signal/state/pixel
difference/render success, and clean public-path markers. It retained focus,
selection, collapsed conversation state, model identity, and 2200x1440 raster
geometry. Direct inspection found readable native contrast and identical
loaded content/state in the Light and Dark captures; their SHA-256 values were
`43d2e14345d84ef64e276eb5153754c8a2c3c0d8d886e4398f744dccd978a0ce`
and
`49244d85e0b6cf604aee2f2cf9e1a860ccb8dab0b583193afa6e11aa8a05031d`.
The rollback-safe shell recorded original Dark `true` and restored Dark
`true`. Selectable System/Light/Dark/high-contrast profiles remain
RFC-BPR-0006. Windows target-native verification remains separately deferred
and nonblocking in BPR-0036. GitHub Actions supplied no build, test, package,
release, or verification evidence.

## Security and authority verification slice result

BPR-0032 re-audited credential authority, transport trust, read-only request
admission, process boundaries, public composition, and retained evidence
against the current implementation. The audit found and corrected several
claims that had conflated structural composition with direct native execution:
the recorded public fixtures substitute a transport and vault, while the
tests-disabled composition contract proves factory selection and the separate
target-native transport and vault fixtures prove their concrete behavior.
Likewise, the idle runtime audit is one point-in-time socket/direct-child
sample, and validated GitHub origin authority is distinct from bounded
Qt-selected proxy routing.

The public tests-disabled self-test now independently requires the exact
production profile shape: two rows and one CLI choice by default, or four rows
and two CLI choices with explicit GHES. Each clean-start CLI row must publish
the exact selected host, `Not authenticated`, the exact fixed diagnostic, and
exactly one identity plus one diagnostic update before it contributes to
`CLI_UNAVAILABLE_COUNT`. Existing host lifecycle coverage separately proves
that switching from authenticated native HTTPS exact-deletes the predecessor
credential binding before resolving the CLI profile. Neither path enrolls or
retrieves a CLI credential, constructs a transport factory, performs wire or
capability work, schedules a CLI-specific effect, or launches a process. The
tests-disabled Release contract runs both one-row and two-row cases and rejects
all dormant CLI execution symbols.

On macOS, the current Keychain setup/lifecycle/cleanup sequence passed 3/3 and
an independent service-scoped lookup confirmed that no integration item
remained. Native HTTPS contract and 32-scenario fixture checks, normalized
read/provider/host contracts, source boundaries, fixture-security scanning,
dependency/runtime inspection, and sanitizer coverage passed. The evidence
claims only exact fixture-vault state, native integration-item/schema cleanup,
bounded output/scratch surfaces, and source-enforced exclusions; it does not
claim arbitrary memory erasure, continuous listener monitoring, or inspection
of every user or operating-system log.

The BPR-0032 macOS matrix passes 46/46 for `dev`, 17/17 for `core-only`, and
16/16 for the focused AddressSanitizer/UndefinedBehaviorSanitizer security set.
Documentation validation, secret-pattern/private-key/embedded-credential
scans, tracked and on-disk GitHub Actions workflow absence, and
`git diff --check` pass. Pinned Linux evidence remains attached to the exact
BPR-0029/BPR-0030 snapshots rather than being mislabeled current-tree.
Windows target-native execution remains BPR-0036 and blocks only affected
Windows claims and final RFC Auto-Verified closeout.

## Claim matrix

| Claim | Direct surface | Command or interaction | What proves | Result | Evidence |
|---|---|---|---|---|---|
| GitHub.com read path is normalized | Real provider facade and recorded public fixture composition | Open the recorded GitHub.com contract PR through the real application | Recorded metadata and conversations traverse provider, host staging, core acceptance, rendered Qt projection, and orderly exact fixture-vault deletion | Verified | BPR-0029 public GitHub.com fixture reported `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, and `RENDER_OK` with structural provider/host/core/Qt and fixture transport/vault evidence; it is not a real native-wire run |
| GHES 3.18 contract is selected | Real provider facade with recorded 3.18 trace | Open the equivalent recorded GHES fixture and retain its bounded structural trace | Explicit-user profile provenance, exact hostname/API roots, `2022-11-28` version, capability evidence, and equivalent normalized result are observed through the substituted fixture transport | Verified | BPR-0029 public GHES fixture reported `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:ghes-3.18`, and `RENDER_OK`; fixture security pins the exact GHES routes and equivalent non-URL semantics, while separate native-wire evidence covers transport behavior |
| Selected v1 normalization is stable and complete | Strict paired provider contracts | Replay paired viewer/repository/PR/files/commits/reviews/threads/comments/checks/search fixtures | Equivalent semantic facts normalize identically, limits hold, and no provider type crosses inward | Verified | BPR-0019 paired manifests, independent normalized expectation, hostile contract suite, and fixture security gate |
| Validated profile construction is explicit | Profile validator and hostile contracts | Submit non-HTTPS, mismatched-root, content-derived, redirect-derived, moved-from, and unknown-authority profiles | No invalid candidate can produce a usable capability-probe authority | Verified | BPR-0020 validated-profile, value-invariant, request-compilation, and redirect contract suites plus BPR-0027 provider/profile composition |
| Capability state is bounded and honest | Probe cache plus core command policy | Replay supported, unsupported, and insufficient-evidence probes | Review-thread/viewed/resolve/unresolve states remain tri-state and unavailable actions never silently approximate support | Verified | BPR-0018 core policy plus BPR-0020 bounded probe and exact-authority cache suites |
| Active host and account are always visible | GitHub Qt presentation | Render initial, loaded, error, and in-flight identity-switch states | Accessible visible identity always matches the accepted provider session and never exposes a credential | Verified | BPR-0028 atomic safe-projection and three-scale Qt cases plus both BPR-0029 public fixtures cover selected profile/host/transport, authenticated account output, connecting state, and immediate account/PR/rate invalidation |
| Native appearance is inherited and updates live | Real Qt application under the operating-system appearance | Launch under native Light, then change the System Appearance to Dark while the same loaded process remains alive and restore the original setting | QMacStyle and the operating-system palette remain authoritative without a forced palette/style/QSS; focus, selection, collapsed expansion, model identity, and geometry survive the live change; exact persisted rasters visibly differ | Verified | BPR-0031's owner-authorized native probe reported both independent `Light->Dark` transitions, Cocoa/QMacStyle and all signal/state/pixel/render markers; direct inspection confirmed readable Light/Dark output with preserved loaded state, and the trap restored original Dark |
| macOS/Linux dormant CLI broker mechanically preserves the process boundary | Injectable process port plus real child-process integration on each selected Unix target | Launch the same-executable child with exact input/retention/time limits and an unknown parent-environment canary; exercise initial-process-group cleanup and escape on clean success, failure, timeout, and cancellation; compile every selected operation/profile request | Canonical executable, closed argv, stdin body, canonical currently non-repository cwd, requested environment plus only the documented Darwin addition, directly observed Unix hardening/session/group facts, bounded public reads/retention, initial-process-group cleanup, watchdog quarantine, fixed failures, and canary absence are directly inspected | Verified | BPR-0025 macOS and Linux transport/compiler and retained `QProcess` runner suites plus hostile source mutations; the runner is not public-composed, a descendant that changes group can escape, numeric PID/PGID reuse means group signaling is not claimed race-free, actual live-process failed-reap retention remains source-gated, Qt private in-flight pipe allocation is not claimed, and the separate trust row governs whether this boundary may send a provider request |
| Windows CLI selection remains fixed unavailable | Real Windows 10/11 x64 application and Release binary | Run `--self-test`, inspect the process tree, and inspect the executable for dormant CLI transport/runner symbols | The public CLI selection reports `CLI_UNAVAILABLE_OK`, creates no CLI process or descendant, and the Release executable contains no CLI execution path | Planned | BPR-0036 owns the direct Windows application/runtime inspection only. Positive Windows CLI execution and all Job Object/process-boundary work are deferred to RFC-BPR-0015 |
| Every visible CLI mode is unavailable and launches zero processes | Public self-test, Release composition contract, host lifecycle, provider counters, and dormant process-port counters | Enumerate the default and explicit-GHES CLI rows from clean start, switch from authenticated native HTTPS, inspect the tests-disabled executable, exercise the provider with exact call counters, and inject a fake successful process port into the dormant transport | `CLI_UNAVAILABLE_OK` and exact counts `1`/`2` are visible; clean-start rows each perform one bounded profile-resolution effect; a native-to-CLI switch exact-deletes only the predecessor binding; the executable omits CLI transport/runner symbols; CLI credential-enrollment/retrieval, transport-factory, wire, capability, CLI-specific-effect, and process-port counts remain zero | Verified | BPR-0037 plus the BPR-0032 direct GHES row case pass `CLI_UNAVAILABLE_COUNT:1` and `CLI_UNAVAILABLE_COUNT:2`, the Release binary contract, native-to-CLI predecessor cleanup coverage, exact provider zero-call assertions, and all-operation transport zero-call assertions with eight typed reasons. Exact 2.95.0/2.96.0 probes independently prove effective `http_unix_socket` rerouting, followed cross-authority `302`, missing per-invocation account binding, and `broker-availability: unavailable`; positive enablement is deferred to RFC-BPR-0015 |
| macOS/Linux native HTTPS fails closed | Native network adapter on both directly exercised targets | Exercise ordered proxy fallback, system-proxy observation, trusted TLS, unknown-host and GraphQL redirects, certificate failure, cancellation, timeout, response-body/per-field-header bounds, and typed HTTP failures | Only validated origin authorities receive origin requests or credentials; connections may traverse bounded Qt-selected system proxy endpoints that never receive GitHub Authorization; trust, redirect, credential, remaining-deadline, cancellation, and adapter-visible retention policies fail closed inside the internal wire transport | Verified | BPR-0026 15-scenario macOS arm64 and frozen Ubuntu 26.04 x86_64 full-system matrices, runtime facts, tests-disabled symbol/dependency audits, sanitizer run, and hostile architecture gates. The loopback runtime covers response-body and one overlong-field case, not separate 65-field/aggregate-only cases or Qt-private buffers/synchronous discovery. BPR-0029 structurally selects native HTTPS publicly but its QAction fixtures substitute recorded transport; BPR-0030's current macOS verifier passed all 32 scenarios, including fail-closed same-authority query-bearing redirect |
| Windows native HTTPS fails closed | Native network adapter on real Windows 10/11 x64 | Exercise system proxy, trusted TLS, unknown-host and GraphQL redirects, certificate failure, cancellation, response/header bounds, runtime scope, and PE dependencies | The selected Windows Qt TLS/proxy runtime enforces the same fail-closed wire contract | Planned | BPR-0036 owns direct target-native execution and inspection; this open Windows task is nonblocking for all other implementation work |
| Portable credential authority and lifetime are explicit | Application credential-vault contract and queued fake | Exercise construction, movement, replacement, hostname-bound exact-key isolation, failure, cancellation, and reordered completion | Credential bytes remain bounded and move-only; exact authority/correlation survives every terminal path; core and Qt cannot name a vault type | Verified | BPR-0021 lifetime/result contract plus BPR-0029 hostname-bound authority, fake-vault, host-shutdown, prompt, and hostile source-boundary checks |
| macOS credentials stay in Keychain and fail closed | Native macOS adapter plus public vault selection/shutdown policy | Store/retrieve/replace/delete a disposable v2 canary; run composed recorded-fixture enrollment and orderly shutdown; inject lock/deny/unavailable/corrupt/delete failures; inspect Release selection/imports and cleanup | The exact default-Keychain v2 item is the sole native store and is absent after its separate lifecycle; Release selects that factory; the shared public shutdown policy exact-deletes the fixture-vault authority; every failure is typed with no fallback | Verified | Historical BPR-0022 v1 contract/native evidence is preserved; BPR-0029 reran current v2 contract/setup/lifecycle/cleanup, reported `NATIVE_PLATFORM_VAULT_SELECTED:true`, imported the selected `SecItem` operations, and separately observed exact deletion in fixture-vault state |
| Windows credentials stay in Credential Manager and fail closed | Native Windows adapter and HTTPS runtime | Run vault lifecycle/failure cases plus TLS/proxy/redirect/runtime/dependency inspection on native Windows | Credential, network, process, and linked/runtime dependencies match the selected Windows implementation with no fallback | Planned | BPR-0023 adapter, fake contracts, fixture, boundaries, and x64 cross-PE imports plus BPR-0026's portable native-HTTPS implementation are prepared; BPR-0036 retains direct native lifecycle, cleanup, HTTPS, and dependency evidence |
| Historical Linux v1 credentials stayed in Secret Service and failed closed | Native Linux desktop adapter and HTTPS runtime at the recorded BPR-0024/BPR-0026 state | Run vault lifecycle/failure cases plus TLS/proxy/redirect/runtime/dependency inspection on native Linux | The historical credential, network, process, and linked/runtime dependencies matched the selected Linux implementation with no fallback | Verified | BPR-0024 full-system v1 vault lifecycle, locked/unavailable/redirect cases, exact cleanup, and dependency inspection plus BPR-0026 frozen native-HTTPS fixture, runtime facts, and ELF/OpenSSL inspection |
| Current Linux v2 vault and public composition are target-native | Current Linux Secret Service adapter and real executable | Run v2 setup/lifecycle/cleanup, both recorded public fixtures, CLI self-test, Release seam/dependency/runtime inspection, and exact schema cleanup in the controlled Linux desktop session | Separate target-native lifecycle checks prove hostname-bound Secret Service v2 storage and exact item/schema cleanup; tests-disabled composition selects the native vault factory; recorded public fixtures prove the shared shutdown policy exact-deletes fixture-vault state; the Release executable contains no fixture seam | Verified | BPR-0029 Ubuntu 26.04 x86_64 full-system QEMU evidence: fresh Release targets 81/81 tests-on and 36/36 tests-off; all 40 non-native registered test identities passing after repaired gates; Secret Service 3/3 plus independent cleanup and hostile/unavailable/locked cases; exact-host rendered GitHub.com/GHES fixtures through substituted transport/vault; `PUBLIC_PATH_OK`; `NATIVE_PLATFORM_VAULT_SELECTED:true`; seam absence; dependency/runtime/ELF audits; tests-off SHA-256 `78a85e15319473e39baaa6b6c8723ff7f9adbade13b48475a3f94a5622719b01`. Historical v1 evidence is not substituted |
| Host/account switching invalidates identity-sensitive state | Core, host, and Qt projection with reordered completions | Switch profile/account while profile, `/user`, capability, pull-request, store, retrieve, and delete work is in flight | Generation advances; stages, caches, rate/retry state, and projections invalidate; old completions emit no acceptance delta; retained credentials are exact-deleted in predecessor order | Verified | BPR-0018 generation policy, BPR-0027 fake-port lifecycle matrix including the two-binding in-flight retrieve A-to-B regression, and BPR-0028 immediate account/PR/conversation/rate presentation invalidation |
| Staged snapshots commit only after core acceptance | Application host and manual executor | Deliver current, stale-request, stale-profile, stale-account, stale-PR, stale-generation, duplicate, malformed, and projection-throwing completions | Only the exact accepted snapshot ID enters the active host cache or projection; capability cache commit also follows exact core acceptance | Verified | BPR-0027 host acceptance, stale-correlation, cache-stage, projection-failure, and refresh/new-target tests |
| Failures are clear and bounded | Core diagnostics and Qt projection | Exercise authentication, permission, network, API-version, capability, rate-limit, malformed, oversized, vault, TLS, and redirect failures | Typed safe presentation appears without raw provider, process, remote-content, or credential text | Verified | BPR-0028 reason-to-distinguishing-text table covers every core reason, richer bounded retry/rate advisory retention, literal remote text, and hostile source boundaries that exclude provider, wire, URL, auth, and credential material. BPR-0030's table-driven host contract exhausts all 15 `github_provider_failure` values, including process-startup and generic-unavailable mapping to the bounded network reason, with no accepted snapshot or detail-bearing diagnostic |
| Provider inputs fail closed at documented adapter-visible bounds | Strict fixture, pager/provider, host, and transport limits | Exceed directly covered body, page, cursor, nesting, member, array, string, record, retained stdout/stderr, duration, retry, redirect, and allocation limits | Each directly exercised limit fails atomically and leaves accepted authority/projection unchanged | Verified | BPR-0019/BPR-0020 strict normalization and capability bounds; BPR-0025 process bounds; BPR-0026 response-body and per-field-header wire cases plus source/contract checks for header-count and aggregate limits; BPR-0027 pagination, raw-envelope, aggregate, retry, host-staging, and stale-authority limits. This is not a categorical application-exhaustion claim: Qt-private buffers, pre-metadata allocation, synchronous proxy/CA discovery, the two header scenarios without dedicated loopback cases, and Windows behavior remain outside the direct hard-bound evidence |
| Core decides, host performs, Qt presents | Target graph, source scan, host integration, and Qt projection | Run core-only build, boundary checks, host integration, and Qt action/projection tests | Core has no effects/provider/Qt types, host owns execution/staging, and Qt receives projections only | Verified | BPR-0018 core policy, BPR-0027 host staging, BPR-0028 provider-free atomic projection and Qt tests, and the BPR-0029 concrete executable trace |
| Qt conversation reconciliation is incremental and stable | Standalone deterministic conversation-model property target | Run `qt_github_conversation_model_property` once | Mixed insert/remove/move/change updates, arbitrary permutations, and large rotations preserve exact model semantics, all public roles, exact change notifications, accessibility facts, and persistent-index rules without reset | Verified | BPR-0028's direct adversarial run remains historical evidence; BPR-0030 checks in and passed the fixed-seed corpus of 3,000 mixed cases, 5,000 arbitrary 100-row permutations, and both 50,000-row rotations |
| Public application structurally selects real provider implementations | Concrete composition plus separate recorded and native-wire fixtures | Run recorded QAction-to-provider-to-rendered-projection flows for GitHub.com and GHES; inspect tests-disabled native factory selection; run the direct native wire fixture; enumerate CLI separately | The executable structurally selects native HTTPS for both profiles; recorded fixtures reach accepted normalized state through a substituted transport; the separate loopback fixture verifies native wire behavior; every CLI row stays unavailable before CLI discovery or launch | Verified | Both BPR-0029 recorded public fixtures passed with exact markers and structural evidence; the Release contract selects the native factory and excludes fixture/dormant CLI execution symbols; BPR-0026/BPR-0030 provide direct native-wire evidence; the public self-test reports `CLI_UNAVAILABLE_OK` with exact counts `1`/`2` |
| Runtime effects are mode-bound on every claimed target | Live process, network, vault, and binary inspection | Inspect CLI-unavailable mode, direct native transport/vault fixtures, tests-disabled composition, and idle runtime on each applicable native platform | CLI launches nothing; validated origin authority is distinct from bounded proxy routing; native implementations and dependencies match selection; the idle process has no sampled socket or direct child at the single observation point | Planned | macOS/Linux selection, native fixture, zero-launch CLI, dependency, and Linux ELF evidence are recorded. `verify_runtime_surface.py` waits about 0.5 seconds and samples sockets/direct children once, so it is not continuous listener monitoring, active-native-HTTPS evidence, or indirect-descendant proof. Windows target-native inspection remains open |
| RFC-BPR-0002 performs no GitHub mutation | Request inventory, exact recorded-method parser, shared bounded GraphQL scanner, canonical request equality, and composed recordings | Exercise raw `GET`, `HEAD`, `POST`, `PUT`, `PATCH`, `DELETE`, `OPTIONS`, `TRACE`, `CONNECT`, and unknown methods; inject mutation, subscription, query-plus-mutation, multiple-operation, malformed-framing, over-limit, and scanner-approved noncanonical read documents into all five legacy/current GraphQL request shapes; consume both public recordings | Only exact uppercase `GET`/`POST` method text is representable; REST requires typed `GET`; every GraphQL body requires exactly one explicit query plus bounded fragments and exact equality with its canonical compiled body; writes anywhere in the document fail closed | Verified | BPR-0019 verifies the ten-read catalog, BPR-0020 adds the exact introspection query, and BPR-0029 consumes both public recordings. BPR-0030 directly passes the exact raw-method table and rejects mutation/subscription/query-plus-mutation, multiple operations, malformed scanned structure/token framing, and scanner limits across all five GraphQL request shapes without false positives from comments, strings, block strings, fields, or names. Each shape also proves a scanner-approved added `__typename` selection is rejected by canonical body equality. The scanner is a bounded structural classifier, not a complete GraphQL grammar parser |
| Provider dependencies are accurately recorded | Build graph, binary inspector, and packaging records | Inspect selected targets and current/native binaries | Qt Network and platform-vault compile/link/runtime dependencies are recorded; distribution certification remains RFC-BPR-0014 | Planned | BPR-0022 records macOS Security/CoreFoundation linkage and exact native-test imports; BPR-0023 records WIN32 Advapi32/Bcrypt selection and cross-PE imports, with target-native inspection retained by BPR-0036; BPR-0024 records Linux libsecret/GIO/GObject/GLib linkage; BPR-0026 records macOS Qt Network/OpenSSL/framework facts and Linux Qt6Network/Qt6Core/OpenSSL runtime dependencies; BPR-0029 records current Linux public selection plus dependency/runtime/ELF audits; Windows native inspection and package certification remain pending |

## Platform and safe-target plan

| Surface | Plan |
|---|---|
| macOS arm64 | Historical BPR-0022 v1 Keychain evidence is preserved; BPR-0029 directly passed the current v2 native setup/lifecycle/cleanup and separately exercised the exact public shutdown policy through fixture-vault state. BPR-0025 verifies the dormant CLI compiler/runner boundary while public CLI stays visibly unavailable with zero launches; BPR-0026 verifies native HTTPS; BPR-0028 verifies the provider-free Qt projection; BPR-0029 verifies both recorded public composition fixtures, clean Debug 43/43, core-only 17/17, sanitizer 12/12 plus 11/11, tests-off Release selection/seam absence, dependencies, and idle runtime. BPR-0030 adds focused current protocol, host-mapping, property-target, and 32-scenario native-verifier evidence; it is not recorded as a new complete platform matrix |
| Windows x64 | BPR-0023 adapter, fake contracts, fixture, and x64 cross-PE inspection are prepared; BPR-0036 retains direct native configure/build/test, Credential Manager lifecycle, native HTTPS TLS/proxy/redirect/certificate checks, public CLI-unavailable and GitHub fixture automation, runtime network/process inspection, and binary dependency inspection. Positive CLI runner work is deferred to RFC-BPR-0015 |
| Linux x64 desktop | Historical BPR-0024 v1 Secret Service lifecycle/fail-closed/cleanup/dependency evidence is preserved; BPR-0025 verifies the CLI compiler/runner boundary while CLI stays unavailable; BPR-0026 verifies frozen native HTTPS; BPR-0028 verifies the provider-free host/three-scale Qt surface. Separately, BPR-0029 passed the current v2 matrix in an Ubuntu 26.04 x86_64 full-system QEMU guest: fresh Release targets 81/81 tests-on and 36/36 tests-off; all 40 non-native registered identities passing; Secret Service 3/3 plus independent exact cleanup and hostile/unavailable/locked cases; both exact-host rendered public fixtures; seam absence; native-vault selection; and dependency/runtime/ELF audits. BPR-0030 then passed a fresh 83/83-target build, complete non-native 41/41 matrix, focused 17/17 matrix, and final 4/4 documentation/architecture/security gate on the byte-identical current focused-verification tree |
| GitHub.com | Sanitized recorded public fixture passed through the real application; optional authenticated live read remains supplementary only |
| GHES 3.18 | Sanitized recorded public fixture passed through the real application; no corporate credentials or repository content retained |
| GitHub mutations | None; RFC-BPR-0002 exposes and verifies read operations only |
| Build ownership | Local and target-native tools only; GitHub Actions has no role |

Target-native credential evidence is a closeout gate. Configuration,
cross-compilation, mocks, and hosted workflow results cannot substitute for
native store/retrieve/delete behavior. BPR-0024 and BPR-0026 supply historical
Linux v1 vault and current internal HTTPS evidence while explicitly retaining
the emulated-CPU limitation; they are not substituted for BPR-0029's separate
passing current v2 full-system QEMU evidence. BPR-0036 retains the Windows
evidence without blocking other implementation slices; it still blocks the
affected Windows claims and RFC closeout. The Windows closeout still requires
native HTTPS, TLS, proxy, redirect, runtime-surface, binary-dependency, v2
vault, and public-fixture evidence; a macOS or Linux success cannot substitute
for that run.

## Security evidence rules

- Use only disposable canary credentials and delete them after each test.
- Never print a token, call `gh auth token`, use `--show-token`, or retain a
  response that can contain private repository or account data.
- Keep the dormant broker regression surface shell-free, argument-allowlisted,
  stdin-bodied, bounded, redacted, and based on an explicit environment built
  from nothing. Those mechanical tests are not permission to compose or
  launch the broker in production.
- Do not treat canonical path/currently non-repository checks, a preflight
  digest, bounded public Qt reads, or absent canary variables as
  owner/permission, validation-to-launch identity, loaded-image identity, or
  private Qt pipe-allocation proof.
- Retain only redacted, bounded traces and structural assertions.
- Reject certificate failures, unknown-authority redirects before credential
  or body forwarding, and every query-bearing redirect in native HTTPS. The
  milestone-one CLI path stays unavailable before discovery or launch;
  effective ambient configuration, redirect preflight, exact account binding,
  and process containment are eligibility gates for RFC-BPR-0015 rather than
  RFC-BPR-0002 closeout criteria.
- Treat vault locked, unavailable, denied, corrupt, and deletion-failure cases
  as typed failures. Never fall back to plaintext, settings, environment, or
  SQLite storage.
- Expose only narrow read-only provider ports, parse recorded methods without
  fallback, require typed REST `GET`, and pass every GraphQL read and
  capability document through the bounded full-document query-only admission
  scanner.
  Require exactly one explicit query plus bounded fragments; reject mutation
  or subscription definitions anywhere at top level, multiple operations,
  malformed scanned structure or token framing, and scanner-limit exhaustion.
  The scanner is not a complete GraphQL grammar parser, and exact canonical
  compiled request-body equality remains mandatory. An operation name,
  endpoint allowlist, fixture label, comment, string, or field name is
  insufficient evidence.

## Planning-time environment reconnaissance

At planning time, the directly available development host was macOS 15.7.7
arm64 with GitHub CLI 2.95.0, CMake 4.3.4, Ninja 1.13.2, Clang 22.1.8, and
Qt 6.11.1 including Qt Network. The canonical CLI executable resolved beneath
the Homebrew installation, and the active GitHub.com account was authenticated
through the macOS keyring. BPR-0037 later audited the official current
2.96.0 binary separately and found no usable production identity contract.

A planning-time supplementary smoke invoked that canonical executable with
token, host, repository, config-directory, debug, and TTY influences removed;
prompts, telemetry, update notices, spinner, and color disabled; and all output
discarded. Both `gh auth status --active --hostname github.com` and an
authenticated, non-mutating `GET /user` completed successfully. No token or
response body was emitted or retained.

This proves only that the current macOS environment can perform a keyring-backed
GitHub.com read through `gh` under a scrubbed environment. It does not verify PR
normalization, requested scopes, private-repository access, rate-limit
handling, a minimal environment allowlist, unknown-variable non-inheritance,
canonical/currently non-repository working-directory behavior, argv policy,
broker redirect/certificate policy, GHES, native HTTPS, trust/proxy/redirect
policy, Windows/Linux vaults, cache invalidation, or UI behavior.

## Documentation ledger

| Scope | Required documents | Disposition | Evidence |
|---|---|---|---|
| Core state and typed flow | Domain/state model and implementation architecture | Update for opaque identities, capabilities, requests/events/deltas, generation, and invalidation | Core tests and type/dependency scan |
| Host/provider boundary | System architecture, provider contract, implementation architecture | Updated: normalized ports, pagination, authenticated sessions, retry/rate state, private capability cache, hostname-bound credential lifecycle, serialized completion, multi-profile routing, structural public native-HTTPS selection with recorded fixture substitution disclosed, exact recorded-method admission, bounded GraphQL operation classification, canonical-body equality, and exhaustive bounded failure mapping are recorded | BPR-0019/BPR-0020 contracts; BPR-0025/BPR-0026 direct transport evidence; BPR-0027 host/provider evidence; BPR-0029 macOS and Linux recorded public fixtures plus Release factory selection; BPR-0030 macOS/Linux protocol, failure-mapping, and redirect matrices |
| Portable credential contract | Domain/state model, provider contract, implementation architecture, threat model | Updated: bounded move-only material, canonical-hostname authority, exact correlation, typed results, Qt-owned input boundary, and no core/Qt vault capability | BPR-0021 lifetime/result contract plus BPR-0029 fake-vault, hostname, prompt, host-shutdown, and boundary suites |
| Native credentials and network trust | Security baseline, threat model, operator/troubleshooting guidance | Updated: historical v1 evidence remains labeled historical; current v2 namespaces/encoding/no-fallback, separate native item/schema cleanup, and fixture-vault proof of the shared exact-shutdown-delete policy are recorded; macOS and Linux v2 are verified; Windows remains BPR-0036; native HTTPS is the sole M1 production auth transport; CLI stays fixed zero-launch and positive enablement is deferred to RFC-BPR-0015 | Historical BPR-0022/BPR-0023/BPR-0024 evidence; BPR-0025/BPR-0026 direct transport evidence; BPR-0029 macOS Keychain and Linux Secret Service v2 native lifecycle, recorded public-fixture, Release selection/dependency, idle-runtime, and exact-cleanup evidence; BPR-0037/BPR-0032 current official CLI trust audit and every-visible-row zero-call contracts |
| Qt presentation | UI architecture and UX baseline | Updated: provider-free projection, Qt-owned sensitive input, compact responsive hierarchy, explicit retained/empty states, wrapped native delegate, expansion/selection preservation, polite announcements, native inheritance, and the directly observed same-process system Light-to-Dark transition are recorded | BPR-0028/BPR-0029 projection and public-fixture evidence plus BPR-0031 three-scale state/focus/accessibility coverage and inspected owner-authorized Cocoa/QMacStyle Light/Dark rasters; deterministic forced offscreen style remains explicitly layout-only |
| Composition and operation | README, build/test guide, developer overview | Updated: explicit profiles/provenance, multi-profile provider, structural native HTTPS/platform-vault factory selection, separate recorded public fixtures and direct native implementations, separate workers, every-visible-row CLI unavailability with exact counts, fixture-vault exact-delete shutdown, crash limitation, public commands, and Release exclusion of dormant CLI execution symbols | BPR-0029 macOS recorded public fixtures and fresh Debug/core-only/sanitizer matrices; Linux recorded public fixtures, fresh Release 81/81 tests-on and 36/36 tests-off targets, native-vault factory selection, and seam/dependency/idle-runtime/ELF audits; BPR-0026 direct native wire fixtures; BPR-0037/BPR-0032 public and Release CLI composition contracts |
| Tests and fixtures | Test guide and fixture catalog | Updated: historical/current vault versions, Qt sensitive input, worker, composition contract, substituted public fixture transport/vault, exact-host and fixture-delete evidence, direct native wire/vault evidence, current 32-scenario HTTPS verification with header-scenario limits disclosed, exact 2.95.0/2.96.0 Unix-socket/certificate/redirect trust probes, single-sample idle-runtime limits, standalone deterministic conversation property corpus, and UX-evidence limits are recorded | Pinned BPR-0029 macOS/Linux matrices and direct fixture markers; BPR-0030 macOS 44/44, core-only 17/17, sanitizer deltas, Linux 83/83-target build and 41/41 matrix; BPR-0037/BPR-0032 exact CLI trust probes and every-visible-row zero-call tests; offscreen PNGs remain layout-only evidence |
| Packaging and release | Packaging policy and dependency inventory | Updated for public macOS and Linux platform-vault selection and tests-off seam absence; Windows native inspection remains unresolved; RFC-BPR-0014 still owns distribution certification | macOS Release reports Security/CoreFoundation and exact `SecItem` imports; Linux Release reports native-vault selection, seam absence, and passing dependency/runtime/ELF audits; historical Windows evidence remains recorded |
| GitHub Actions | Build/test/package/release documentation | Updated/preserved: absolute prohibition; no workflow files | Documentation validator plus tracked and on-disk workflow-absence scans |

## Performance and projection plan

- Compact core state contains identities, capability summaries, request
  generations, accepted snapshot identity, selected normalized summaries, and
  presentation keys rather than provider payloads or credentials.
- Provider and diagnostic inputs have explicit count and byte limits before
  normalization.
- Capability caches are keyed by host and authenticated account identity and
  are invalidated on identity changes.
- Immutable completions remain staged until an exact core acceptance delta;
  rejected or superseded stages never enter the active cache or projection.
- Qt receives one atomic safe projection update. Its conversation model
  reconciles stable semantic identities with insert, remove, minimal move,
  and data-change notifications without reset, while retained persistent
  indexes continue to name the same row.
- Rate-limit and retry state is host-owned, bounded, cancellable, and reported
  as observations rather than hidden timing policy in core.

## Explicit deferrals

- GitHub review, comment, viewed-file, and resolve/unresolve mutations remain
  RFC-BPR-0008 even though RFC-BPR-0002 probes and presents their capability.
- Local Git cache and immutable pull-request review epochs remain
  RFC-BPR-0003.
- Distribution signing and clean delivered-package certification remain
  RFC-BPR-0014. RFC-BPR-0002 still records and verifies the Qt Network and
  platform-vault dependencies it actually selects.
- By product-owner direction, BPR-0036 defers every Windows x64 target-native
  run until it can be performed on a real Windows 10/11 machine. It does not
  block implementation, tests, documentation, security review, or
  simplification on available targets or later RFC implementation; it blocks
  only the affected Windows claims and final RFC-BPR-0002 Auto-Verified
  closeout.
- BPR-0029 selected exact orderly shutdown deletion rather than durable
  credential rehydration. A crash can still orphan an unenumerable item; no
  restart-recovery claim is made.
- BPR-0037 resolved the milestone-one CLI scope: native HTTPS is the sole
  production authentication transport and the public CLI choices permanently
  return zero-launch `cli_unavailable` for this RFC. Positive enablement is
  deferred to upstream-dependent RFC-BPR-0015 outside pack v2.
- BPR-0038 closes the design decision by keeping query-bearing HTTP redirect
  targets rejected. Pagination queries remain separately validated provider
  requests; any future redirect-policy relaxation requires new direct
  public-contract evidence and separately scoped work.

An Auto-Verified closeout is blocked while any claim remains `Planned`, any
documentation disposition is unresolved, any linked issue is outside `done/`,
target-native vault/HTTPS evidence is absent, the native transport cannot
prove certificate and redirect rejection, the CLI path ceases to be
categorically zero-launch, or the narrow read-only ports and outbound
admission checks do not reject non-GET REST operations, non-query GraphQL
operations, or noncanonical bodies.

## Clean closeout blockers

RFC-BPR-0002 cannot become Auto-Verified until the final committed state passes
all applicable clean checks:

- clean Qt-enabled `dev` configure, build, and complete CTest matrix;
- clean Qt-free `core-only` configure, build, and complete CTest matrix;
- direct core, application, provider, Qt, composition, and architecture
  boundary suites;
- native runtime process/network and binary-dependency audits for every
  platform claim recorded by this RFC;
- target-native vault and HTTPS/TLS/proxy/redirect matrices;
- documentation validator;
- original source-pack validator;
- documentation impact review over the immutable RFC base;
- `git diff --check`;
- issue-board, RFC/index, and verification-ledger agreement.

GitHub Actions output cannot satisfy or replace any of these checks.
