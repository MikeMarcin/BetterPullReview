# RFC-BPR-0002 Verification Plan and Ledger

RFC base: `bfa9ab3ad6b08fbb22b697ed6fe6688202d3335a`

## Architecture allocation

| Region | Planned responsibility | Direct evidence |
|---|---|---|
| Functional core | Opaque host/account/repository/PR identity, semantic open/refresh/select intents, capability legality, request identity, completion acceptance, invalidation, deltas, diagnostics | Core-only replay/property tests and forbidden-dependency scan |
| Application host | Normalized provider port, process/network/vault effects, sessions, retries, rate limits, cancellation, caches, lifecycle, serialized completions | Fake-port integration with reordering, identity switching, expiry, failure, and cancellation |
| GitHub adapter | Profiles, REST/GraphQL contracts, capability probes, CLI broker, native HTTPS, bounded normalization | GitHub.com/GHES recorded-contract and hostile-transport tests |
| Platform credential adapters | Store/retrieve/delete credentials only through macOS Keychain, Windows Credential Manager, or Linux Secret Service | Disposable target-native canary tests with post-test deletion |
| Qt presentation adapter | Semantic event translation, active identity, metadata/conversation projections, capability-driven actions, bounded errors, accessibility | Qt adapter tests and rendered real-application automation |
| Composition root | Explicit concrete profile/transport/vault/provider/host/core/Qt selection | End-to-end trace through the actual executable |

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
  -> normalized provider completion
  -> generation-tagged core event
  -> identity/content/command/diagnostic deltas
  -> incremental Qt projection
```

## Claim matrix

| Claim | Direct surface | Planned proof | Result |
|---|---|---|---|
| GitHub.com read path is normalized | Real provider facade and public fixture composition | Recorded PR metadata/conversation contract traverses provider, host, core, and rendered Qt path | Planned |
| GHES 3.18 contract is selected | Real provider facade with recorded 3.18 trace | Inspect hostname, API roots, `2022-11-28` REST version, capability trace, and equivalent normalized result | Planned |
| Normalization is stable and complete | Strict paired provider contracts | Prove viewer, repository, PR, files, commits, reviews, threads, comments, checks, and search facts normalize equivalently without provider types crossing inward | Planned |
| Profile trust is explicit | Profile validator and hostile contracts | Reject non-HTTPS roots, root/hostname mismatch, content-derived hosts, redirect-derived profiles, and unknown authorities | Planned |
| Capability state is observed, bounded, and honest | Probe cache plus core command policy | Prove review-thread, viewed-state, resolve/unresolve, and unsupported states come from contracts and disable unavailable actions | Planned |
| Active host and account are always visible | Real Qt application | Render initial, loaded, error, and identity-switch states; inspect accessible text | Planned |
| CLI broker preserves the credential boundary | Injectable process port and child-process integration | Inspect absolute executable, argv, stdin, scrubbed environment, bounds, and retained output; canary absent everywhere | Planned |
| Native HTTPS fails closed | Native network adapter | Prove system trust/proxy use, approved redirect handling, unknown-host rejection, certificate-failure rejection, cancellation, and bounds | Planned |
| macOS credentials stay in Keychain | Native macOS adapter | Store/retrieve/delete disposable canary and scan settings, SQLite, argv, logs, diagnostics, and UI | Planned |
| Windows credentials stay in Credential Manager | Native Windows adapter on native Windows | Store/retrieve/delete disposable canary and scan settings, SQLite, argv, logs, diagnostics, and UI | Planned |
| Linux credentials stay in Secret Service | Native Linux adapter in desktop session | Store/retrieve/delete disposable canary and scan settings, SQLite, argv, logs, diagnostics, and UI | Planned |
| Host/account switching invalidates identity-sensitive state | Core plus host with reordered completions | Switch identity in flight and prove caches/projections invalidate and old completion cannot apply | Planned |
| Failures are clear and bounded | Core diagnostics and Qt projection | Exercise authentication, permission, network, API-version, capability, rate-limit, malformed, and oversized responses | Planned |
| Provider inputs cannot exhaust the application | Strict fixture and transport limits | Prove body, page, nesting, string, output, timeout, and allocation limits fail without partially updating authority | Planned |
| Core decides, host performs, Qt presents | Target graph, source scan, and full trace | Prove core has no effect/provider/Qt types, host owns execution, Qt receives projections only | Planned |
| Public application selects real provider implementations | Concrete composition | QAction-to-provider-to-completion-to-rendered-projection trace for both profile fixtures | Planned |
| Runtime effects are mode-bound | Live process and network inspection | Foundation mode creates no child/network; CLI mode permits only resolved `gh`; HTTPS mode permits only configured outbound authority; no mode listens | Planned |
| RFC-BPR-0002 performs no GitHub mutation | Provider request inventory and runtime trace | Assert only read operations are exposed/issued by this slice | Planned |

## Platform and safe-target plan

| Surface | Plan |
|---|---|
| macOS arm64 | Direct local configure/build/test, Keychain canary lifecycle, CLI and native HTTPS integration, real application automation, rendered inspection |
| Windows x64 | Direct native configure/build/test and Credential Manager canary lifecycle on an approved Windows environment |
| Linux x64 desktop | Direct native configure/build/test and Secret Service canary lifecycle in an approved Linux desktop session |
| GitHub.com | Sanitized recorded fixtures; optional authenticated read-only smoke through the broker with response body discarded |
| GHES 3.18 | Sanitized recorded contract trace through the real provider facade; no corporate credentials or repository content retained |
| GitHub mutations | None; RFC-BPR-0002 exposes and verifies read operations only |
| Build ownership | Local and target-native tools only; GitHub Actions has no role |

Target-native credential evidence is a closeout gate. Configuration,
cross-compilation, mocks, and hosted workflow results cannot substitute for
native store/retrieve/delete behavior.

## Security evidence rules

- Use only disposable canary credentials and delete them after each test.
- Never print a token, call `gh auth token`, use `--show-token`, or retain a
  response that can contain private repository or account data.
- Invoke a canonical absolute executable without a shell; pass bodies on
  standard input and bound time, standard output, and standard error.
- Scrub token, host, repository, debug, config-directory, pager, color,
  telemetry, update-notifier, and interactive-prompt environment influences
  before broker execution.
- Retain only redacted, bounded traces and structural assertions.
- Reject unknown redirect hosts and certificate errors. There is no bypass.

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
handling, GHES, native HTTPS, trust/proxy/redirect policy, Windows/Linux
vaults, cache invalidation, or UI behavior.

## Documentation ledger

| Scope | Required documents | Planned disposition | Evidence |
|---|---|---|---|
| Core state and typed flow | Domain/state model and implementation architecture | Update for opaque identities, capabilities, requests/events/deltas, generation, and invalidation | Core tests and type/dependency scan |
| Host/provider boundary | System architecture, provider contract, implementation architecture | Update for normalized port, sessions, retry/rate/caches, CLI/native transport ownership | Host and contract suites |
| Native credentials and network trust | Security baseline, threat model, operator/troubleshooting guidance | Update for vault lifecycle, CA/proxy, redirects, certificate failures, bounds, and secret hygiene | Native canary and hostile-transport evidence |
| Qt presentation | UI architecture and UX baseline | Update for active identity, open/read states, capability legality, errors, focus, and accessibility | Qt tests and rendered inspection |
| Composition and operation | README, build/test guide, developer overview | Update for selected profiles/transports, fixture mode, safe read-only smoke, and real public path | End-to-end automation |
| Tests and fixtures | Test guide and fixture catalog | Update for sanitization, GitHub.com/GHES contracts, hostile inputs, and platform-native gates | Documentation validator and fixture audit |
| Packaging and release | Packaging policy | Record no package-format impact; RFC-BPR-0014 still owns distribution certification | Manual path map |
| GitHub Actions | Build/test/package/release documentation | Preserve explicit prohibition; no workflow files | Documentation validator |

## Performance and projection plan

- Compact core state contains identities, capability summaries, request
  generations, selected normalized metadata, and presentation keys rather than
  provider payloads or credentials.
- Provider and diagnostic inputs have explicit count and byte limits before
  normalization.
- Capability caches are keyed by host and authenticated account identity and
  are invalidated on identity changes.
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
  RFC-BPR-0014.
- No RFC-BPR-0002 acceptance criterion, transport, credential adapter, or
  platform-native evidence is deferred.

An Auto-Verified closeout is blocked while any claim remains `Planned`, any
documentation disposition is unresolved, any linked issue is outside `done/`,
or target-native vault evidence is absent.
