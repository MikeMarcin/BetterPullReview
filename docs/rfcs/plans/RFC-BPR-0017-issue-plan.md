---
id: RFC-BPR-0017-ISSUE-PLAN
rfc: RFC-BPR-0017
status: Planning snapshot
rfc_base_sha: a45750f6bd84716240aa27a20f0734f33808ab8e
requested_position: 2
---

# Issue Plan for RFC-BPR-0017

## Goal and stop condition

Determine whether a public, unauthenticated, read-only GitHub.com preview is a
product worth shipping, then implement it only if real evidence and an attended
product decision say yes. RFC-BPR-0016 remains the preferred authentication-
friction answer. Waiting on RFC-0016 external authority is not a reason to build
RFC-0017.

RFC-BPR-0017 stays `Deferred`. Planning and BPR-0297's correction may proceed;
real service requests require BPR-0298 authority, and product implementation
requires BPR-0312's validated positive branch from BPR-0299 plus RFC-BPR-0002
Auto-Verified or an exact recorded implementation-only exception. No exception
can waive RFC-BPR-0002 at final closeout.

If real rate behavior makes refresh impractical, BPR-0299 records `do not
build`, the RFC remains Deferred, and BPR-0312 remains unresolved so the
implementation graph is dependency-blocked; the decision is not converted
into fabricated implementation evidence.

## Live-tree correction

- `github_read_shape` can represent absent thread/viewed capabilities. The
  selected pager already honors an all-false GraphQL shape; the tests-only
  recorded-contract helper still requires ten responses and hardcodes a full
  shape.
- The catalog's only review-comment source is the GraphQL review-thread
  operation. There is no REST `pulls/{number}/comments` read operation today,
  so anonymous thread reconstruction requires one new operation inside the
  existing contract/normalizer, not merely omission of GraphQL.
- The selected production provider uses `perform_paged_github_read`; it already
  skips GraphQL for an all-false shape and marks threads unavailable. The fixed-
  ten-response/full-shape behavior belongs to the recorded-contract helper
  normalizer used by tests, not the selected pager. Both still need the REST
  review-comment operation and exact anonymous admission coverage.
- Native HTTPS unconditionally binds credential material and writes a Bearer
  header. Credential-free mode must be a positive typed authority, never the
  accidental result of a missing token.
- Host/session/cache/projection identities require a nonzero viewer/account.
  Anonymous authority therefore needs a distinct stable identity scope and
  explicit unavailability, not zero/default authenticated identity.

## Architecture allocation

| Boundary | Responsibility | Prohibited behavior |
|---|---|---|
| Core | Anonymous profile mode, legal read-only intents, unavailable viewer/triage/actions, typed rate/private/GHES reasons, discriminated authenticated-account versus anonymous-public revision authority | Qt, HTTP, credential/vault facts, rate clocks, provider types, zero/default/synthetic viewer identity or actionability |
| Application host | Session authority, stable discriminated anonymous repository scope, request orchestration, cache/catalog/epoch/projection invalidation on anonymous/authenticated switch | Vault lookup, synthetic viewer, duplicated normalizer, mutation admission, cross-authority namespace reuse |
| GitHub adapter | Existing read contract plus one REST review-comment operation, credential-free native-HTTPS and public smart-HTTP binding, paging/normalization, 404/rate classifications | Parallel provider/path/fixtures, absent-credential fallback, authorization header |
| Git adapter | Installed-Git execution and authority-scoped public repository/cache materialization | Credential discovery, anonymous/authenticated cache aliasing, push or private/GHES admission |
| SQLite adapter | Authority-scoped epoch persistence and transactional migration of existing authenticated catalog rows to the discriminated schema | Zero-account sentinel, destructive silent recreation, partial migration, unknown authority/version acceptance |
| Qt adapter | Explicit public-preview wording, unavailable triage/actions/viewed/resolution presentation, limit/error UI, switch-to-sign-in gesture | Authenticated wording, provider/vault calls, inferred resolution/actionability |
| Composition | GitHub.com public-preview selection; all GHES and private/mutation paths structurally refused | Fixture-only reachability, credential discovery, public observer subprocess reuse |

The existing macOS designated-live public observer is evidence that one public
GET is possible, not an implementation component. It remains outside the
ordinary provider and is neither generalized nor composed into this RFC.

## Planned issue graph

| Card | Slice | Depends on |
|---|---|---|
| BPR-0297 | Correct RFC findings and eligibility sequencing against the live tree | none |
| BPR-0298 | Authorize and record sanitized real anonymous-service probes | BPR-0297 |
| BPR-0313 | Demonstrate credential-free native HTTPS in a bounded non-product course | BPR-0297 |
| BPR-0299 | Record the attended build/no-build product decision and limit presentation | BPR-0298, BPR-0313 |
| BPR-0312 | Validate the positive build-approved branch; remain unresolved on rejection | BPR-0299 |
| BPR-0300 | Implement deterministic anonymous authority and unavailable semantics | BPR-0035, BPR-0312 |
| BPR-0301 | Implement typed credential-free native HTTPS and provider admission | BPR-0035, BPR-0312 |
| BPR-0302 | Add REST review comments and availability-aware normalization | BPR-0300, BPR-0301 |
| BPR-0303 | Implement viewer-less host session and exact cache invalidation | BPR-0300, BPR-0302 |
| BPR-0317 | Implement anonymous Git/cache/epoch authority and selected-diff delivery | BPR-0301, BPR-0303 |
| BPR-0304 | Implement explicit read-only Qt preview presentation | BPR-0303, BPR-0317 |
| BPR-0305 | Compose unavailable-by-default GitHub.com preview candidate and refusal paths | BPR-0303, BPR-0304, BPR-0317 |
| BPR-0306 | Verify security, public behavior, and target-native macOS | BPR-0305 |
| BPR-0314 | Prove Linux preview stays unavailable with zero effects | BPR-0306 |
| BPR-0315 | Prove Windows preview stays unavailable with zero effects | BPR-0036, BPR-0311, BPR-0306 |
| BPR-0316 | Enable macOS preview after native proof and rerun the selected artifact | BPR-0306 |
| BPR-0307 | Simplify without weakening the complete implementation | BPR-0314, BPR-0315, BPR-0316 |
| BPR-0308 | Reconcile product, architecture, security, build, and agent docs | BPR-0305, BPR-0306, BPR-0307 |
| BPR-0309 | Capability-critical final replay and RFC closeout anchor | BPR-0035, BPR-0285, BPR-0297 through BPR-0308, BPR-0312 through BPR-0317 |

This file is an immutable planning snapshot. Folder placement and card front
matter under `docs/issues/` are the sole live workflow authority; BPR-0309 is
the durable run anchor.

## Real-service evidence

BPR-0298 must resolve anonymous GraphQL, REST review comments and pagination,
observed primary/search budget headers, conditional-request accounting,
private/nonexistent 404 non-disclosure, and current GitHub.com response shapes.
Rate exhaustion itself is a synthetic hostile contract replay; the live probe
neither exhausts nor evades GitHub limits. The RFC's GHES
claim must either be proved against a designated instance or removed as
irrelevant because composition refuses every GHES profile before traffic.

No probe may use credentials, reveal private content, mutate a repository,
exhaust or evade limits, or use an undesignated target. A disposable empty
private-repository comparison, if required, is a separate explicit creation and
deletion authority; it is never inferred from permission to read public data.

## Required verification families

- Core replay/property cases for every public-preview intent, unavailable
  observation/action, stale identity, switch, terminal, and refusal.
- Native-HTTPS loopback cases proving a deliberate credential-free binding,
  exact absence of authorization, unchanged TLS/proxy/redirect/cancellation
  rules, and authenticated-path non-regression.
- Read-contract/provider/normalizer cases for the exact anonymous admission set,
  REST review comments and linkage, missing resolution/viewed/viewer state,
  404 non-disclosure, rate limits, malformed/oversized data, and paging.
- Host tests for zero vault activity, stable anonymous identity, concurrency,
  cancellation, and full identity-sensitive invalidation on mode switches.
- Git/cache/epoch/diff tests for credential-free public smart HTTP,
  discriminated viewer-independent authority, versioned SQLite migration,
  installed-Git/SQLite two-process restart, selected-file identity, rendered
  diff content, cancellation, cache isolation, and authenticated non-regression.
- Qt three-scale/focus/accessibility/cut-wire courses for public-preview wording,
  disabled actions, absent observations, rate/private diagnostics, and sign-in.
- Tests-disabled product selection proving GitHub.com public-preview reachability,
  GHES/private/mutation zero-request refusal, and Release fixture-seam absence.
- Local and target-native evidence only; no GitHub Actions evidence.

Production capability is initially macOS-only after BPR-0306 passes its native
matrix and BPR-0316 performs the same-slice selection flip/replay. Linux and
Windows select typed public-preview unavailability with zero
requests; enabling either platform requires a separately planned target-native
card and dependency update before closeout.

## Documentation ledger

Product contract, system architecture, GitHub host/provider contracts, review
workbench/queue/epoch ownership, security threat model, build/test guidance,
operator authentication guidance, and the RFC verification ledger require an
update or exact no-impact disposition. Agent guidance changes only for a stable
reusable boundary or workflow.
