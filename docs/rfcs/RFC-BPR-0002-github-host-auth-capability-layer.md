---
id: RFC-BPR-0002
title: GitHub Host, Authentication, and Capability Layer
status: Proposed
area: BPR
milestone: M1 Native Read-Only Workbench
depends_on: [RFC-BPR-0001]
pack_version: 2
---

# RFC-BPR-0002: GitHub Host, Authentication, and Capability Layer

## Summary

Add GitHub.com and GHES 3.18-compatible host profiles, authentication transports, capability probing, and normalized read models without exposing credentials to unrelated subsystems.


## Motivation

BetterPullReview must operate against both GitHub.com and an older corporate GHES installation without baking credentials, cloud assumptions, or one server version into the UI.

## Goals

- Implement explicit host profiles for GitHub.com and GHES.
- Support GHES 3.18 as the minimum contract target.
- Provide a GitHub CLI broker mode and a native HTTPS mode.
- Normalize REST and GraphQL responses into stable internal models.
- Probe and cache host capabilities rather than assuming feature presence.
- Build a read-only vertical slice that opens PR metadata and conversations.

## Non-goals

- Publishing reviews or comments.
- Managing GitHub Apps, OAuth registration, enterprise administration, or browser cookies.
- Silently bypassing missing capabilities.

## Architectural allocation

- **Core:** owns host/account identity in interaction state, semantic open/refresh intents, capability-dependent command legality, completion acceptance, and structured authentication/capability diagnostics based on bounded observations. It does not know REST, GraphQL, URLs, tokens, `gh`, `QNetworkAccessManager`, or credential-vault APIs.
- **Host:** owns GitHub provider ports, CLI/native transports, credentials, proxy/trust behavior, request execution, retries, rate-limit state, capability caches, normalized records, and identity-sensitive invalidation. Provider completions return as revisioned/generation-tagged events.
- **Qt adapter:** owns host/account configuration controls, authentication prompts, capability/error presentation, and action state. It never receives a token or calls a provider directly.
- **Flow:** an `open_pull_request_intent` is decided by the core, becomes a typed fetch/probe request, is performed by the host, and returns a normalized snapshot event that the core accepts before Qt receives projections.

## Provider design

Host configuration includes hostname, API roots, product kind, authentication transport, account identity, trust/proxy mode, REST version, and probed capabilities. The active host and account are always visible in the application.

REST supplies the broad baseline. GraphQL is limited to review-thread and viewed-state features. The GHES floor uses the supported date-version header and recorded 3.18 contracts. GitHub.com uses its own fixture set and capability profile.

## Authentication

### GitHub CLI broker

BetterPullReview invokes `gh api` or equivalent official CLI surfaces without extracting a token. It uses stdin for bodies, disables prompts, validates executable identity/path, bounds output, and redacts diagnostics.

### Native HTTPS

The direct transport uses system trust/proxy configuration and native credential vaults. No certificate-error bypass exists. A provider interface ensures the rest of the product does not care which transport was selected.

## Acceptance criteria

- A configured GitHub.com profile opens a sanitized fixture PR through the normalized provider surface.
- A configured GHES 3.18 profile opens the equivalent fixture using only supported contracts.
- Capability probing reports review threads, viewed-file mutations, resolve/unresolve, and unsupported features accurately.
- The UI presents clear authentication, permission, network, API-version, and capability failures.
- Canary credentials do not appear in logs, process arguments, diagnostics, SQLite, or UI error text.
- The provider rejects unknown redirect hosts and certificate failures.
- Switching host or account invalidates all identity-sensitive caches and projections.
- Core tests run with no Qt/network/credential implementation, host tests use fake transports and reordered completions, and Qt tests prove event translation and capability-driven action state without credential access.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| GHES 3.18 path selected | Real provider facade with recorded contract | Inspect request/version/capability trace and normalized result |
| `gh` broker owns credential | Child-process integration | Canary token remains absent from parent process and logs |
| Native vault storage | Platform credential adapter | Store/retrieve/delete test secret without settings/SQLite copies |
| Capability failure is safe | Application error state | Disable GraphQL mutation in fixture and verify action is unavailable, not approximated |
| Architecture boundaries hold | Core, fake-host, and Qt adapter tests | Prove core has no transport types, host owns effects, and Qt sees projections only |

## Documentation impact

Host configuration, authentication modes, credential boundaries, corporate CA/proxy behavior, API compatibility, capability probing, errors, and safe test-target policy are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
