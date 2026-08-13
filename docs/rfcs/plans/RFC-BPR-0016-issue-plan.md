---
id: RFC-BPR-0016-ISSUE-PLAN
rfc: RFC-BPR-0016
status: Active
rfc_base_sha: b969554fb45d838b2a4ea11811363bd7e158e47e
requested_order: [RFC-BPR-0016, RFC-BPR-0017]
---

# Issue Plan for RFC-BPR-0016

## Goal and authority

Implement and Auto-Verify GitHub device authorization after its external and
dependency gates are explicitly satisfied. The plan itself authorizes no app
registration, endpoint traffic, credential acquisition, fixture capture, or
partial product mode.

RFC-BPR-0016 remains `Deferred`. BPR-0035/BPR-0036 still own the missing
target-native Windows evidence and RFC-BPR-0002 Auto-Verified transition. The
board's general available-target exception permits planning later RFCs, but it
does not silently override RFC-BPR-0016's stricter eligibility text.

## Architecture allocation

| Boundary | Responsibility | Prohibited data/effects |
|---|---|---|
| Core | Bounded acquisition state, legal intents, revision/request identity, terminal policy, sanitized diagnostics | Qt, HTTP, URLs, client identifiers, device/user codes, intervals, access/refresh tokens |
| Application host | Poll schedule, controlled clock, cancellation/shutdown, transient-material erasure, and handoff to the existing `github_application_host::enroll_profile` vault path | Alternate token store, settings/SQLite copies, duplicated semantic policy |
| GitHub adapter | Exact form-encoded device/token wire contracts, sanitized response parsing, RFC-BPR-0002 transport policy reuse | Provider types inward, redirects or certificate bypass, unbounded response material |
| Browser capability | Host-owned, authority-validated system-browser open request with a nonfatal typed result | Embedded browser, cookie/session access, loopback listener |
| Qt adapter | Acquisition chooser, user-code/address/expiry presentation, copy/open/cancel gestures, incremental projection | Token material, client identifier, endpoint, GitHub calls, direct vault access |
| Composition | Registered GitHub.com capability selection, GHES/unregistered zero-request refusal, manual-token coexistence | Fixture-only product reachability, hidden enablement while gates are open |

Device-flow success joins the existing manual enrollment path at the one
credential-material boundary. GitHub App refresh material, if selected by the
recorded application-kind decision, is stored under that same vault authority
and never creates a settings or SQLite path.

## Live issue graph

| Card | Slice | Depends on |
|---|---|---|
| BPR-0275 | Record dependency exception/application-kind/owner/permission authority | none |
| BPR-0276 | Register the public client and capture sanitized real-service wire contracts | BPR-0275, BPR-0035 unless an exact exception removes BPR-0035 |
| BPR-0277 | Implement deterministic core interaction policy | BPR-0276 |
| BPR-0278 | Implement GitHub device/token wire adapter and native-HTTPS contracts | BPR-0276 |
| BPR-0279 | Implement bounded host polling, cancellation, cleanup, refresh, and vault handoff | BPR-0277, BPR-0278 |
| BPR-0280 | Implement Qt acquisition chooser and browser-handoff presentation | BPR-0277, BPR-0279 |
| BPR-0281 | Compose the registered GitHub.com path and zero-request unavailable paths | BPR-0279, BPR-0280 |
| BPR-0282 | Complete security, hostile, release, and public-path evidence | BPR-0281 |
| BPR-0283 | Simplify the complete implementation without changing authority | BPR-0281, BPR-0282 |
| BPR-0284 | Reconcile product, architecture, security, build, and agent documentation | BPR-0281, BPR-0282, BPR-0283 |
| BPR-0285 | Capability-critical final replay and RFC closeout | BPR-0035, BPR-0275 through BPR-0284 |

BPR-0276 initially retains BPR-0035. If the product owner approves a narrow
implementation exception, that same digest-bound decision commit must update
the dependency and RFC eligibility language before code work starts. No
exception can waive RFC-BPR-0002 as a final closeout dependency.

## Required verification families

- Pure core replay/property tests for every intent, stale observation,
  terminal, cancellation, and bounded state transition.
- Application-host tests using a controlled clock and manual executor for
  interval, `slow_down`, total bound, shutdown, erasure, refresh, and exactly
  one existing-vault enrollment.
- GitHub adapter/native-HTTPS tests for exact requests and responses, response
  limits, certificate failures, proxy policy, and redirect refusal on both
  endpoints.
- Qt tests for acquisition choice, accessibility, focus, copy/open/cancel,
  browser failure, manual-token coexistence, and secret-free projection.
- Tests-enabled and tests-disabled application contracts proving production
  selection, GitHub.com capability, GHES/unregistered zero requests, and no
  fixture seam in Release.
- Binary/source/fixture/log/settings/SQLite scans proving that no client secret,
  access token, refresh token, device code, or user code was retained.
- Local or target-native execution only; GitHub Actions is never evidence.
- Every claimed supported platform receives target-native device-flow evidence;
  until then, product capability selection remains unavailable on that platform
  and the exact remaining platform claim stays open.

## Documentation ledger

| Surface | Required update or exact no-impact record |
|---|---|
| Product contract and system architecture | State authority, typed flow, credential boundary, browser capability, composition selection |
| Authentication and review-round-trip architecture | Manual/device coexistence, GitHub.com/GHES capability, one vault enrollment path |
| Foundation security | Public client, exact permissions, transient secrets, redirect/certificate rules, no embedded browser/listener |
| Native HTTPS and cancellation contracts | Endpoint forms, limits, backoff, total bound, cancellation/shutdown cleanup |
| Build and test guide | Registration build selection, local fixtures, negative courses, Release scans, no-Actions rule |
| Operator/authentication guide | Application owner, installation implications, device flow, fallback manual token, diagnostics |
| RFC verification ledger | Immutable base, final-head claim matrix, delegated review, attended/external provenance |
| Agent guidance | Update only if a stable new authority or verification workflow applies to future work |

## Deferred and external work

- Creating or changing the GitHub application, enabling device flow, and
  making real endpoint requests require an attended, digest-bound decision.
- Sanitized fixtures must be captured from the real selected application and
  service; documentation-derived fixtures cannot satisfy BPR-0276.
- GHES remains manual-token-only until an instance-specific administrator
  supplies and records an application registration.
- Direct Windows x64 evidence remains with BPR-0036 and is not replaced by
  macOS, cross-compilation, mocks, or hosted workflows.
