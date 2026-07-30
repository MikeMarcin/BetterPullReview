# RFC-BPR-0002 Verification Plan and Ledger

RFC base: `bfa9ab3ad6b08fbb22b697ed6fe6688202d3335a`

## Architecture allocation

| Region | Planned responsibility | Direct evidence |
|---|---|---|
| Functional core | Opaque profile/host/account/repository/PR identity, semantic open/refresh/select intents, tri-state capability legality, request identity, explicit snapshot acceptance, invalidation, deltas, diagnostics | Core-only replay/property tests and forbidden-dependency scan |
| Application host | Normalized provider port, staged snapshots, process/network/vault effects, sessions, retries, rate limits, cancellation, caches, lifecycle, serialized completions | Fake-port integration with reordering, identity switching, expiry, failure, cancellation, and commit-after-acceptance assertions |
| GitHub adapter | Profiles, REST/GraphQL contracts, capability probes, CLI broker, native HTTPS, bounded normalization | GitHub.com/GHES recorded-contract and hostile-transport tests |
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
  -> selected CLI or native HTTPS adapter
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
contract, not live transport completeness. BPR-0027 still must exhaust REST
and GraphQL pagination, validate raw envelopes, project allowlisted fields,
and stage the complete result through the application host before the public
read path can be claimed.

## Host-profile and capability slice result

The GitHub adapter now validates explicit GitHub.com and GHES 3.18 profiles
before capability-probe request compilation. Only canonical lowercase ASCII
FQDNs, trusted profile provenance, exact HTTPS REST and GraphQL roots, and the
selected `2022-11-28` server contract are admitted. Provider content cannot
create a profile, GraphQL redirects are forbidden, and REST redirects remain
on the validated authority and inside its API namespace.

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
distinct from host schema support. BPR-0027 still owns authenticated `/user`
observation, probe execution, cache orchestration, capability-shaped GraphQL
selection and unavailable-target normalization, transport paging, and
binding the three independently validated authorities through the production
application host.

The clean Qt-enabled matrix passed 23/23 and the clean Qt-free matrix passed
15/15. Architecture boundaries passed 22/22, the hostile core boundary
passed 4/4, capability/read fixture security passed 12/12, and platform
configuration passed 4/4. Documentation, original source-pack,
`git diff --check`, untracked/tracked workflow-absence gates, and the
conservative completed-slice simplification review passed.

## Portable credential-vault contract slice result

The application boundary now defines one platform-neutral credential-vault
capability. Its authority is the exact profile, profile revision, host,
authenticated account, and server-contract tuple. Every store, retrieve,
delete, cancellation, and terminal outcome retains exact request,
application-session, and identity-generation correlation.

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
evidence below; Windows and Linux remain `Planned` until BPR-0023 and
BPR-0024 supply direct target-native evidence.

The clean Qt-enabled matrix passed 24/24 and the clean Qt-free matrix passed
16/16. The queued fake-vault test passed under address and undefined-behavior
sanitizers, and optimized-code inspection retained the explicit overwrite
loop before deallocation. Architecture boundaries passed 27/27, the hostile
core boundary passed 4/4, capability/read fixture security passed 12/12, and
platform configuration passed 4/4. Documentation, original source-pack,
`git diff --check`, and untracked/tracked workflow-absence gates passed.

## macOS Keychain slice result

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
dependencies. The current public application also links those frameworks
through the static GitHub-adapter target, but imports no `SecItem` or
`SecKeychain` symbol and does not yet compose the vault factory. Public-path
selection correctly remains `Planned`.

The Qt-enabled `dev` matrix passed 28/28, the Qt-free `core-only` matrix passed
16/16, the focused macOS contract/setup/lifecycle/cleanup matrix passed 4/4,
and the hostile architecture-boundary suite passed 40/40. Direct architecture,
documentation, source-pack, diff, and tracked/untracked workflow-absence gates
passed. A conservative simplification pass replaced manual CoreFoundation
ownership with scoped RAII, consolidated terminal extraction/failure
normalization, and preserved the explicit Security flow. The concurrent
teardown regression also passed 25 consecutive runs.

## Claim matrix

| Claim | Direct surface | Command or interaction | What proves | Result | Evidence |
|---|---|---|---|---|---|
| GitHub.com read path is normalized | Real provider facade and public fixture composition | Open the GitHub.com contract PR through the real application | Recorded metadata and conversations traverse provider, host staging, core acceptance, and rendered Qt projection | Planned | BPR-0019 verifies the recorded adapter facade; host/core/Qt composition remains pending |
| GHES 3.18 contract is selected | Real provider facade with recorded 3.18 trace | Open the equivalent GHES fixture and retain a redacted request trace | Exact hostname/API roots, `2022-11-28` version, capability evidence, and equivalent normalized result are observed | Planned | BPR-0019 verifies the relative GHES REST/GraphQL contract and equivalent result; profile/transport selection remains pending |
| Selected v1 normalization is stable and complete | Strict paired provider contracts | Replay paired viewer/repository/PR/files/commits/reviews/threads/comments/checks/search fixtures | Equivalent semantic facts normalize identically, limits hold, and no provider type crosses inward | Verified | BPR-0019 paired manifests, independent normalized expectation, hostile contract suite, and fixture security gate |
| Validated profile construction is explicit | Profile validator and hostile contracts | Submit non-HTTPS, mismatched-root, content-derived, redirect-derived, moved-from, and unknown-authority profiles | No invalid candidate can produce a usable capability-probe authority | Verified | BPR-0020 validated-profile, value-invariant, request-compilation, and redirect contract suites; production read composition remains BPR-0027 |
| Capability state is bounded and honest | Probe cache plus core command policy | Replay supported, unsupported, and insufficient-evidence probes | Review-thread/viewed/resolve/unresolve states remain tri-state and unavailable actions never silently approximate support | Verified | BPR-0018 core policy plus BPR-0020 bounded probe and exact-authority cache suites |
| Active host and account are always visible | Real Qt application | Render initial, loaded, error, and in-flight identity-switch states | Accessible visible identity always matches the accepted provider session and never exposes a credential | Planned | Pending |
| CLI broker preserves the credential and process boundary | Injectable process port plus real child-process integration | Launch the compiled fake `gh` with canary token variables and an unknown environment canary | Canonical executable, argv allowlist, stdin body, neutral cwd, explicit minimal environment allowlist, stream/time/page bounds, and absence of all canaries are directly inspected | Planned | Pending |
| CLI broker fails closed on trust | CLI redirect/certificate contract | Exercise approved authority, cross-authority redirect, and certificate-failure scenarios through the broker | Only an authority/certificate path proven to satisfy the selected profile can normalize; otherwise the broker is unavailable and closeout remains blocked | Planned | Pending |
| Native HTTPS fails closed | Native network adapter on each target platform | Exercise system proxy, trusted TLS, unknown-host redirect, certificate failure, cancellation, and response bounds | Only configured HTTPS authorities succeed; no certificate bypass or partial snapshot commit occurs | Planned | Pending |
| Portable credential authority and lifetime are explicit | Application credential-vault contract and queued fake | Exercise construction, movement, replacement, exact-key isolation, failure, cancellation, and reordered completion | Credential bytes remain bounded and move-only; exact authority/correlation survives every terminal path; core and Qt cannot name a vault type | Verified | BPR-0021 contract, fake-vault suite, and hostile source-boundary checks; no native-vault claim |
| macOS credentials stay in Keychain and fail closed | Native macOS adapter | Store/retrieve/replace/delete a disposable canary; inject lock/deny/unavailable/corrupt/delete failures; inspect source, process inputs/output/scratch, native imports, and cleanup | The exact default-Keychain item is the sole adapter-selected store; captured surfaces are canary-free; final exact item is absent; every failure is typed with no fallback | Verified | BPR-0022 contract and native fixture, source boundary, Security/CoreFoundation import inspection, and exact cleanup; public composition remains separately planned |
| Windows credentials stay in Credential Manager and fail closed | Native Windows adapter and HTTPS runtime | Run vault lifecycle/failure cases plus TLS/proxy/redirect/runtime/dependency inspection on native Windows | Credential, network, process, and linked/runtime dependencies match the selected Windows implementation with no fallback | Planned | Pending |
| Linux credentials stay in Secret Service and fail closed | Native Linux desktop adapter and HTTPS runtime | Run vault lifecycle/failure cases plus TLS/proxy/redirect/runtime/dependency inspection on native Linux | Credential, network, process, and linked/runtime dependencies match the selected Linux implementation with no fallback | Planned | Pending |
| Host/account switching invalidates identity-sensitive state | Core plus host with reordered completions | Switch profile/account while a session probe and PR fetch are in flight | Generation advances; stages, caches, rate/retry state, and projections invalidate; old completions emit no acceptance delta | Planned | Pending |
| Staged snapshots commit only after core acceptance | Application host and manual executor | Deliver current, stale-request, stale-profile, stale-account, stale-PR, and stale-generation completions | Only the exact accepted snapshot ID enters the active cache or Qt projection | Planned | Pending |
| Failures are clear and bounded | Core diagnostics and Qt projection | Exercise authentication, permission, network, API-version, capability, rate-limit, malformed, oversized, vault, TLS, and redirect failures | Typed safe presentation appears without raw provider, process, remote-content, or credential text | Planned | Pending |
| Provider inputs cannot exhaust the application | Strict fixture and transport limits | Exceed body, page, nesting, string, stdout/stderr, duration, and allocation limits independently | Each limit fails atomically and leaves the accepted authority/projection unchanged | Planned | BPR-0019 verifies read JSON/body/page/string/record/allocation bounds; BPR-0020 verifies introspection request/response/depth/member/collection/string/aggregate bounds; process, network, duration, and staged-authority evidence remains pending |
| Core decides, host performs, Qt presents | Target graph, source scan, and full trace | Run core-only build, boundary checks, host integration, and Qt action/projection tests | Core has no effects/provider/Qt types, host owns execution/staging, and Qt receives projections only | Planned | Pending |
| Public application selects real provider implementations | Concrete composition | Run QAction-to-provider-to-completion-to-rendered-projection flows for both profile fixtures and both transport selections | The actual executable selects the claimed transport and reaches an accepted normalized state rather than a helper | Planned | Pending |
| Runtime effects are mode-bound | Live process, network, and binary inspection | Inspect foundation, CLI, and HTTPS modes on applicable native platforms | Foundation has no provider effect; CLI permits only resolved `gh`; HTTPS permits only configured outbound authority; no mode listens; dependencies match selection | Planned | Pending |
| RFC-BPR-0002 performs no GitHub mutation | Request inventory plus rejecting sink | Route every REST and GraphQL operation through a no-mutation sink | Sink validates REST method/route, parses GraphQL operation type, and rejects every `mutation` regardless of operation name | Planned | BPR-0019 verifies the ten-read request catalog and mutation rejection; BPR-0020 adds one exact static introspection `query` and no mutation operation; composed transport inventory remains pending |
| Provider dependencies are accurately recorded | Build graph, binary inspector, and packaging records | Inspect selected targets and current/native binaries | Qt Network and platform-vault compile/link/runtime dependencies are recorded; distribution certification remains RFC-BPR-0014 | Planned | BPR-0022 records macOS Security/CoreFoundation linkage and exact native-test imports; Windows/Linux, native HTTPS, public selection, and package evidence remain pending |

## Platform and safe-target plan

| Surface | Plan |
|---|---|
| macOS arm64 | BPR-0022 directly passed the Keychain success/fail-closed lifecycle and binary inspection; CLI/native HTTPS TLS/proxy/redirect, public application automation, and rendered provider inspection remain later RFC-BPR-0002 slices |
| Windows x64 | Direct native configure/build/test, Credential Manager success and fail-closed lifecycle, native HTTPS TLS/proxy/redirect/certificate checks, runtime network/process inspection, binary dependency inspection, and public fixture automation |
| Linux x64 desktop | Direct native configure/build/test, Secret Service success and fail-closed lifecycle, native HTTPS TLS/proxy/redirect/certificate checks, runtime network/process inspection, binary dependency inspection, and public fixture automation |
| GitHub.com | Sanitized recorded fixtures; optional authenticated read-only smoke through the broker with response body discarded |
| GHES 3.18 | Sanitized recorded contract trace through the real provider facade; no corporate credentials or repository content retained |
| GitHub mutations | None; RFC-BPR-0002 exposes and verifies read operations only |
| Build ownership | Local and target-native tools only; GitHub Actions has no role |

Target-native credential evidence is a closeout gate. Configuration,
cross-compilation, mocks, and hosted workflow results cannot substitute for
native store/retrieve/delete behavior. Windows and Linux closeout also
requires their native HTTPS, TLS, proxy, redirect, runtime-surface, and binary
dependency evidence; a macOS success cannot substitute for either.

## Security evidence rules

- Use only disposable canary credentials and delete them after each test.
- Never print a token, call `gh auth token`, use `--show-token`, or retain a
  response that can contain private repository or account data.
- Invoke a canonical absolute executable without a shell; pass bodies on
  standard input from a neutral non-repository working directory, enforce an
  argument allowlist, and bound time, standard output, and standard error.
- Construct an explicit minimal broker environment allowlist rather than
  inheriting and subtracting known names. Token, host, repository, debug,
  config-directory, pager, editor/browser, color, TTY, telemetry,
  update-notifier, spinner, and interactive-prompt influences are absent.
- Inject a previously unknown canary environment name/value and prove the
  child cannot observe it.
- Retain only redacted, bounded traces and structural assertions.
- Reject unknown redirect hosts and certificate errors in native HTTPS and the
  CLI broker. If the installed broker cannot prove that fail-closed behavior,
  its path is unavailable and RFC closeout remains blocked.
- Treat vault locked, unavailable, denied, corrupt, and deletion-failure cases
  as typed failures. Never fall back to plaintext, settings, environment, or
  SQLite storage.
- Parse GraphQL documents at the no-mutation sink and reject operation type
  `mutation`; an operation name or endpoint allowlist is insufficient.

## Planning-time environment reconnaissance

The directly available development host is macOS 15.7.7 arm64 with
GitHub CLI 2.95.0, CMake 4.3.4, Ninja 1.13.2, Clang 22.1.8, and Qt 6.11.1
including Qt Network. The canonical CLI executable resolves beneath the
Homebrew installation, and the active GitHub.com account is authenticated
through the macOS keyring.

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
neutral working-directory behavior, argv policy, broker redirect/certificate
policy, GHES, native HTTPS, trust/proxy/redirect policy, Windows/Linux vaults,
cache invalidation, or UI behavior.

## Documentation ledger

| Scope | Required documents | Planned disposition | Evidence |
|---|---|---|---|
| Core state and typed flow | Domain/state model and implementation architecture | Update for opaque identities, capabilities, requests/events/deltas, generation, and invalidation | Core tests and type/dependency scan |
| Host/provider boundary | System architecture, provider contract, implementation architecture | Update for normalized port, sessions, retry/rate/caches, CLI/native transport ownership | Host and contract suites |
| Portable credential contract | Domain/state model, provider contract, implementation architecture, threat model | Record bounded move-only material, exact authority/correlation, typed results, and no core/Qt exposure without claiming a native vault | BPR-0021 fake-vault and hostile boundary suites |
| Native credentials and network trust | Security baseline, threat model, operator/troubleshooting guidance | macOS vault lifecycle and limitation recorded; Windows/Linux vaults and CA/proxy/redirect behavior remain to update with their implementing slices | BPR-0022 native canary, source/binary boundaries, and exact cleanup; later native-vault and transport evidence remains pending |
| Qt presentation | UI architecture and UX baseline | Update for active identity, open/read states, capability legality, errors, focus, and accessibility | Qt tests and rendered inspection |
| Composition and operation | README, build/test guide, developer overview | Update for selected profiles/transports, fixture mode, safe read-only smoke, and real public path | End-to-end automation |
| Tests and fixtures | Test guide and fixture catalog | Update for sanitization, GitHub.com/GHES contracts, hostile inputs, and platform-native gates | Documentation validator and fixture audit |
| Packaging and release | Packaging policy and dependency inventory | Record actual Qt Network and platform-vault compile/link/runtime dependencies with no package-format claim; RFC-BPR-0014 still owns distribution certification | Target graph, binary inspection, and manual path map |
| GitHub Actions | Build/test/package/release documentation | Preserve explicit prohibition; no workflow files | Documentation validator |

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
- Qt receives changed identity, content, command, and diagnostic sets and does
  not rebuild the complete application projection for routine completions.
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
- No RFC-BPR-0002 acceptance criterion, transport, credential adapter, or
  platform-native evidence is deferred.

An Auto-Verified closeout is blocked while any claim remains `Planned`, any
documentation disposition is unresolved, any linked issue is outside `done/`,
target-native vault/HTTPS evidence is absent, either transport cannot prove
redirect and certificate rejection, or a no-mutation sink has not rejected
REST writes and parsed GraphQL mutations.

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
