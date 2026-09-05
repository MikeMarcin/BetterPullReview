---
schema: "implement-rfc.card.v1"
id: "BPR-0030"
title: "Add RFC-BPR-0002 focused tests and hostile fixtures"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0027","BPR-0029"]
completed: "2026-07-30"
---

# Add RFC-BPR-0002 focused tests and hostile fixtures

Add core replay/property tests, recorded GitHub.com and GHES 3.18 contract
tests, fake process/network/vault tests, reordered-completion tests, Qt event
and projection tests, and hostile fixtures for malformed data, oversized
responses, redirects, certificate failures, permissions, rate limits,
unsupported capabilities, and canary-secret leakage.

The protocol sink must reject every non-read REST verb and parse GraphQL
documents to reject `mutation` operations even though GraphQL reads use HTTP
POST.

## Result

- Added one exact recorded-method parser. It admits only uppercase `GET` and
  `POST`; every other verb, case variant, empty value, and padded spelling
  fails before a recorded request can become typed.
- Replaced the duplicated first-token GraphQL checks with one bounded
  full-document structural classifier shared by reads and capability probing.
  It requires exactly one explicit query plus bounded fragments and rejects
  mutation, subscription, multiple operations, malformed scanned framing, and
  byte, nesting, or definition-limit exhaustion.
- Kept exact canonical compiled-body equality as the outbound read backstop.
  All five legacy/current GraphQL shapes directly prove that a
  scanner-approved added read selection still fails canonical equality.
- Added a table-driven application-host contract covering all 15 provider
  failure values through real pull-request completions. Every case produces
  the exact bounded core reason without an accepted snapshot, projection
  payload, or provider-owned detail; retryable failures exhaust the bounded
  retry policy.
- Checked the deterministic conversation-model corpus into one standalone
  property target: 3,000 mixed updates, 5,000 arbitrary 100-row permutations,
  and both 50,000-row rotations. It verifies every public role,
  deletion-aware display/accessibility, exact changed-row counts, minimal
  moves, persistent indexes, and zero resets.
- Added a same-authority query-bearing REST redirect scenario to the current
  native HTTPS verifier. It fails as a typed redirect after exactly one
  CONNECT/TLS/origin/auth request and cannot attempt a second hop.
- Audited the existing core replay, fake process/network/vault,
  reordered-completion, malformed/oversized, certificate, permission,
  rate-limit, unsupported-capability, and canary-leakage coverage rather than
  duplicating it. GitHub Actions has no build or verification role.

## Evidence

- The final macOS `dev` matrix passed 44/44 in 67.24 seconds; the Qt-free
  `core-only` matrix passed 17/17 in 18.62 seconds.
- The current 32-scenario native HTTPS verifier passed on macOS, including the
  query-bearing redirect case. Focused protocol/public-fixture and
  scale/property groups passed after the final assertions.
- The final Qt property corpus passed in 12.59 seconds normally and 38.50
  seconds under AddressSanitizer/UndefinedBehaviorSanitizer. The final
  GraphQL/read contract passed normally and under the same sanitizers.
- In an Ubuntu 26.04 x86_64 full-system QEMU guest, a fresh Release
  tests-enabled GCC 15.2 build completed 83/83 targets without a portability
  change.
- The complete Linux non-native matrix passed 41/41 in 541.09 seconds,
  including the property corpus, all 32 native HTTPS scenarios, both public
  fixtures, dependency/runtime/render checks, hostile architecture checks,
  and the tests-disabled composition seam. The focused BPR-0030 group passed
  17/17.
- The final lowercase/mixed/empty/padded method delta was synchronized
  byte-for-byte, rebuilt, and passed `github_read_contract` 1/1 on Linux.
  Final Linux documentation/architecture/security gates passed 4/4.
- Documentation, immutable source-pack, Python compilation, diff, and tracked
  and on-disk workflow-absence gates passed. A skeptical final review found no
  remaining P0/P1 defect.
- Windows 10/11 x64 target-native verification remains BPR-0036. It is
  nonblocking for later implementation, documentation, security, and
  simplification work and blocks only affected Windows claims and final
  RFC-BPR-0002 Auto-Verified closeout.
