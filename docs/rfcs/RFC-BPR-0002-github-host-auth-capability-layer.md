---
id: "RFC-BPR-0002"
title: "GitHub Host, Authentication, and Capability Layer"
status: "Auto-Verified"
area: "BPR"
milestone: "M1 Native Read-Only Workbench"
depends_on: ["RFC-BPR-0001"]
pack_version: 2
---

# RFC-BPR-0002: GitHub Host, Authentication, and Capability Layer

## Summary

Add GitHub.com and GHES 3.18-compatible host profiles, native HTTPS
authentication, capability probing, and normalized read models without
exposing credentials to unrelated subsystems. GitHub CLI choices remain
visible but fixed at a zero-process `cli_unavailable` result.


## Motivation

BetterPullReview must operate against both GitHub.com and an older corporate GHES installation without baking credentials, cloud assumptions, or one server version into the UI.

## Goals

- Implement explicit host profiles for GitHub.com and GHES.
- Support GHES 3.18 as the minimum contract target.
- Use native HTTPS as the only production authentication transport in M1.
- Keep GitHub CLI choices visible and fail them closed before starting a
  process.
- Normalize REST and GraphQL responses into stable internal models.
- Probe and cache host capabilities rather than assuming feature presence.
- Build a read-only vertical slice that opens PR metadata and conversations.

## Non-goals

- Publishing reviews or comments.
- Managing GitHub Apps, OAuth registration, enterprise administration, or browser cookies.
- Silently bypassing missing capabilities.

## Architectural allocation

- **Core:** owns host/account identity in interaction state, semantic open/refresh intents, capability-dependent command legality, completion acceptance, and structured authentication/capability diagnostics based on bounded observations. It does not know REST, GraphQL, URLs, tokens, `gh`, `QNetworkAccessManager`, or credential-vault APIs.
- **Host:** owns GitHub provider ports, native HTTPS, the fixed
  CLI-unavailable boundary, credentials, proxy/trust behavior, request
  execution, retries, rate-limit state, capability caches, normalized records,
  and identity-sensitive invalidation. Provider completions return as
  revisioned/generation-tagged events.
- **Qt adapter:** owns host/account configuration controls, authentication prompts, capability/error presentation, and action state. It never receives a token or calls a provider directly.
- **Flow:** an `open_pull_request_intent` is decided by the core, becomes a typed fetch/probe request, is performed by the host, and returns a normalized snapshot event that the core accepts before Qt receives projections.

## Provider design

Host profile configuration includes hostname, API roots, product kind,
authentication transport, trust/proxy mode, REST version, and a profile
revision. Authenticated account identity is a later `/user` observation, and
capabilities are separately probed and cached observations. The active host and
accepted account are always visible in the application.

REST supplies the broad baseline. GraphQL is limited to review-thread and viewed-state features, with the narrowly compiled read extensions authorized by RFC-BPR-0068–0070 for check requirements/evaluated results, requested actors/code-owner origin, PR merge/review facts and target-branch requirements. These successor scopes retain capability shaping, bounded pagination and account leases; they authorize no general query executor or mutation. The GHES floor uses the supported date-version header and recorded 3.18 contracts. GitHub.com uses its own fixture set and capability profile.

## Authentication

### GitHub CLI choices

M1 may present GitHub CLI choices so the product's intended authentication
surface is explicit, but selecting any of them must return the stable
`cli_unavailable` diagnostic before any CLI executable or configuration
discovery or CLI process creation. No `gh` process, credential helper, shell,
or other descendant may start.
Positive CLI enablement is neither an RFC-BPR-0002 acceptance criterion nor a
closeout blocker.

Production CLI enablement is deferred to
[RFC-BPR-0015](RFC-BPR-0015-github-cli-production-enablement.md),
which is outside the canonical fourteen-RFC pack and depends on missing
upstream guarantees. Until that RFC is eligible and Auto-Verified, the
zero-launch result is permanent rather than a best-effort fallback. A future
implementation must not extract a token, add a listener or service, require a
privileged helper, or substitute a patched CLI.

### Native HTTPS

The direct transport is the only production authentication transport in M1.
It uses system trust/proxy configuration and native credential vaults. No
certificate-error bypass exists. Redirects to an unknown authority are
rejected before credentials or request bodies can be forwarded, and every
query-bearing redirect fails closed even when it names the configured
authority. A provider interface keeps transport details out of the core and Qt
adapter.

## Build and verification ownership

Build, test, package, release, and verification work is local or
target-native. GitHub Actions has no role in RFC-BPR-0002 and cannot replace
direct application, transport, vault, or process-boundary evidence.

## Acceptance criteria

- A configured GitHub.com profile opens a sanitized fixture PR through the normalized provider surface.
- A configured GHES 3.18 profile opens the equivalent fixture using only supported contracts.
- Capability probing reports review threads, viewed-file mutations, resolve/unresolve, and unsupported features accurately.
- The UI presents clear authentication, permission, network, API-version, and capability failures.
- Canary credentials do not appear in logs, process arguments, diagnostics, SQLite, or UI error text.
- Every visible GitHub CLI choice produces `cli_unavailable` without CLI
  executable or configuration discovery, process creation, or descendants.
- The native provider rejects certificate failures, unknown-authority
  redirects before forwarding credentials or bodies, and all query-bearing
  redirects.
- Switching host or account invalidates all identity-sensitive caches and projections.
- Core tests run with no Qt/network/credential implementation, host tests use fake transports and reordered completions, and Qt tests prove event translation and capability-driven action state without credential access.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| GHES 3.18 path selected | Real provider facade with recorded contract | Inspect request/version/capability trace and normalized result |
| Native HTTPS is the M1 production transport | Tests-disabled public composition and direct native-wire contract | Inspect GitHub.com/GHES factory selection in the real composition, then separately exercise the native request/TLS/proxy path; the recorded QAction fixture may substitute its transport and is not real-network evidence |
| CLI choices are zero-launch | Public self-test, composition contract, host lifecycle, provider counters, and process-port counters | Select every visible CLI choice from clean start, require `CLI_UNAVAILABLE_OK`, and observe one bounded noncredential profile-resolution effect per row; switch from authenticated native HTTPS and require exact deletion of only the predecessor binding; prove the Release executable omits CLI transport/runner symbols and both paths make zero CLI credential-enrollment/retrieval, transport-factory, wire, capability, CLI-specific-effect, and process-port calls |
| Native vault storage | Platform credential adapter | Store/retrieve/delete test secret without settings/SQLite copies |
| Redirects fail closed | Native transport contract | Exercise unknown-authority and query-bearing redirects and prove no redirected credential/body transmission |
| Capability failure is safe | Application error state | Remove required mutation-capability schema fields from the introspection fixture and verify the action is unavailable, not approximated |
| Architecture boundaries hold | Core, fake-host, and Qt adapter tests | Prove core has no transport types, host owns effects, and Qt sees projections only |

## Documentation impact

Host configuration, authentication modes, credential boundaries, corporate CA/proxy behavior, API compatibility, capability probing, errors, and safe test-target policy are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
