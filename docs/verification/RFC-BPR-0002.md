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

## Claim matrix

| Claim | Direct surface | Command or interaction | What proves | Result | Evidence |
|---|---|---|---|---|---|
| GitHub.com read path is normalized | Real provider facade and public fixture composition | Open the GitHub.com contract PR through the real application | Recorded metadata and conversations traverse provider, host staging, core acceptance, and rendered Qt projection | Planned | Pending |
| GHES 3.18 contract is selected | Real provider facade with recorded 3.18 trace | Open the equivalent GHES fixture and retain a redacted request trace | Exact hostname/API roots, `2022-11-28` version, capability evidence, and equivalent normalized result are observed | Planned | Pending |
| Normalization is stable and complete | Strict paired provider contracts | Replay paired viewer/repository/PR/files/commits/reviews/threads/comments/checks/search fixtures | Equivalent semantic facts normalize identically, limits hold, and no provider type crosses inward | Planned | Pending |
| Profile trust is explicit | Profile validator and hostile contracts | Submit non-HTTPS, mismatched-root, content-derived, redirect-derived, and unknown-authority profiles | Every invalid authority is rejected before a normalized record can be staged | Planned | Pending |
| Capability state is bounded and honest | Probe cache plus core command policy | Replay supported, unsupported, and insufficient-evidence probes | Review-thread/viewed/resolve/unresolve states remain tri-state and unavailable actions never silently approximate support | Planned | Pending |
| Active host and account are always visible | Real Qt application | Render initial, loaded, error, and in-flight identity-switch states | Accessible visible identity always matches the accepted provider session and never exposes a credential | Planned | Pending |
| CLI broker preserves the credential and process boundary | Injectable process port plus real child-process integration | Launch the compiled fake `gh` with canary token variables and an unknown environment canary | Canonical executable, argv allowlist, stdin body, neutral cwd, explicit minimal environment allowlist, stream/time/page bounds, and absence of all canaries are directly inspected | Planned | Pending |
| CLI broker fails closed on trust | CLI redirect/certificate contract | Exercise approved authority, cross-authority redirect, and certificate-failure scenarios through the broker | Only an authority/certificate path proven to satisfy the selected profile can normalize; otherwise the broker is unavailable and closeout remains blocked | Planned | Pending |
| Native HTTPS fails closed | Native network adapter on each target platform | Exercise system proxy, trusted TLS, unknown-host redirect, certificate failure, cancellation, and response bounds | Only configured HTTPS authorities succeed; no certificate bypass or partial snapshot commit occurs | Planned | Pending |
| macOS credentials stay in Keychain and fail closed | Native macOS adapter | Store/retrieve/delete a disposable canary; lock/deny/unavailable/delete-failure fixtures | Success leaves no other copy; every vault failure is typed and has no plaintext fallback | Planned | Pending |
| Windows credentials stay in Credential Manager and fail closed | Native Windows adapter and HTTPS runtime | Run vault lifecycle/failure cases plus TLS/proxy/redirect/runtime/dependency inspection on native Windows | Credential, network, process, and linked/runtime dependencies match the selected Windows implementation with no fallback | Planned | Pending |
| Linux credentials stay in Secret Service and fail closed | Native Linux desktop adapter and HTTPS runtime | Run vault lifecycle/failure cases plus TLS/proxy/redirect/runtime/dependency inspection on native Linux | Credential, network, process, and linked/runtime dependencies match the selected Linux implementation with no fallback | Planned | Pending |
| Host/account switching invalidates identity-sensitive state | Core plus host with reordered completions | Switch profile/account while a session probe and PR fetch are in flight | Generation advances; stages, caches, rate/retry state, and projections invalidate; old completions emit no acceptance delta | Planned | Pending |
| Staged snapshots commit only after core acceptance | Application host and manual executor | Deliver current, stale-request, stale-profile, stale-account, stale-PR, and stale-generation completions | Only the exact accepted snapshot ID enters the active cache or Qt projection | Planned | Pending |
| Failures are clear and bounded | Core diagnostics and Qt projection | Exercise authentication, permission, network, API-version, capability, rate-limit, malformed, oversized, vault, TLS, and redirect failures | Typed safe presentation appears without raw provider, process, remote-content, or credential text | Planned | Pending |
| Provider inputs cannot exhaust the application | Strict fixture and transport limits | Exceed body, page, nesting, string, stdout/stderr, duration, and allocation limits independently | Each limit fails atomically and leaves the accepted authority/projection unchanged | Planned | Pending |
| Core decides, host performs, Qt presents | Target graph, source scan, and full trace | Run core-only build, boundary checks, host integration, and Qt action/projection tests | Core has no effects/provider/Qt types, host owns execution/staging, and Qt receives projections only | Planned | Pending |
| Public application selects real provider implementations | Concrete composition | Run QAction-to-provider-to-completion-to-rendered-projection flows for both profile fixtures and both transport selections | The actual executable selects the claimed transport and reaches an accepted normalized state rather than a helper | Planned | Pending |
| Runtime effects are mode-bound | Live process, network, and binary inspection | Inspect foundation, CLI, and HTTPS modes on applicable native platforms | Foundation has no provider effect; CLI permits only resolved `gh`; HTTPS permits only configured outbound authority; no mode listens; dependencies match selection | Planned | Pending |
| RFC-BPR-0002 performs no GitHub mutation | Request inventory plus rejecting sink | Route every REST and GraphQL operation through a no-mutation sink | Sink validates REST method/route, parses GraphQL operation type, and rejects every `mutation` regardless of operation name | Planned | Pending |
| Provider dependencies are accurately recorded | Build graph, binary inspector, and packaging records | Inspect selected targets and current/native binaries | Qt Network and platform-vault compile/link/runtime dependencies are recorded; distribution certification remains RFC-BPR-0014 | Planned | Pending |

## Platform and safe-target plan

| Surface | Plan |
|---|---|
| macOS arm64 | Direct local configure/build/test, Keychain success and fail-closed lifecycle, CLI and native HTTPS TLS/proxy/redirect integration, runtime/binary inspection, real application automation, rendered inspection |
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
| Native credentials and network trust | Security baseline, threat model, operator/troubleshooting guidance | Update for vault lifecycle, CA/proxy, redirects, certificate failures, bounds, and secret hygiene | Native canary and hostile-transport evidence |
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
