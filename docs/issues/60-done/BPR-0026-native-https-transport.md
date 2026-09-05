---
schema: "implement-rfc.card.v1"
id: "BPR-0026"
title: "Implement native HTTPS transport and trust policy"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0020","BPR-0021","BPR-0022","BPR-0023","BPR-0024"]
completed: "2026-07-30"
---

# Implement native HTTPS transport and trust policy

Implement the native HTTPS provider behind the same application port using
system trust and proxy behavior, bounded request/response handling, approved
API roots, explicit redirects, cancellation, and typed error classification.

Certificate failures and redirects to unknown hosts must fail closed. No
certificate-error bypass or unbounded response path may exist.

Run controlled target-native trust, proxy, certificate-failure, redirect,
cancellation, and selected-adapter tests on the available macOS and Linux
targets. Inspect each available target's linked/runtime Qt Network, TLS,
proxy, and credential-vault dependencies; configuration or another platform's
backend is not evidence.

By product-owner direction, this card prepares the Windows tests and
boundaries but delegates the Windows 10/11 x64 execution, runtime inspection,
and claim to BPR-0036. That open Windows evidence does not block this
implementation slice.

## Result

- Added one standard-C++ native HTTPS transport boundary. A dedicated Qt
  worker owns its event loop and `QNetworkAccessManager`; Qt network and TLS
  types do not cross the GitHub adapter.
- Reused one strict request/correlation validator across the CLI and native
  transports. Invalid shape, profile, authority, operation, page, context,
  session, identity generation, or credential authority fails before an
  effect.
- Preserved system proxy order for at most 16 supported direct, HTTP, or
  SOCKS5 candidates. Only retryable proxy/connection failures may advance to
  a later candidate, and every attempt uses manager-local proxy/cache state.
- Configured explicit system roots, peer verification, TLS 1.2 or later,
  HTTP/1.1 ALPN, no HTTP/2, no automatic redirects, and no certificate-error
  bypass. A success requires the completed encrypted-handshake signal, peer
  certificate, exact reply URL, and consistent configured TLS policy.
- Allowed at most three manual REST `GET` redirects inside the exact validated
  authority and API namespace. GraphQL, cross-authority, looping, malformed,
  query-bearing, and over-limit redirects fail closed.
- Enforced 256 KiB request, 4 MiB response, 64-header/64 KiB aggregate/8 KiB
  field, fixed 16 KiB read, three-redirect, 16-proxy, and 30-second absolute
  deadline bounds. Cancellation and deadline checks surround every
  synchronous preparation and transfer boundary.
- Bound bootstrap credentials to one exact viewer-identity operation and
  account credentials to the complete stored authority/session/generation.
  The explicit mutable Authorization buffer is overwritten and every
  retained failure is typed and redacted.
- Added a test-only ordered loopback proxy/disposable-CA seam and a 15-scenario
  TLS-over-CONNECT fixture covering success, fallback order, redirects,
  certificate rejection, response/header limits, cancellation, timeout,
  HTTP 401/403/429, content encoding, and bootstrap replay.
- Recorded the honest limits: synchronous Qt proxy/root discovery cannot be
  preempted; bounds do not cover Qt-private parser/socket/TLS buffers; Qt may
  normalize duplicate headers; and Qt-owned Authorization copies cannot be
  exactly wiped.
- Kept pagination, body-aware GraphQL/rate-limit interpretation,
  normalization, host staging, and snapshot acceptance in BPR-0027, with
  executable provider composition separately assigned to BPR-0029.
- Kept all configuration, builds, tests, runtime inspection, and verification
  local or target-native. GitHub Actions has no role.

## Evidence

- Final macOS 15.7.7 arm64 focused contract/fixture: 2/2 passed with all 15
  scenarios. Runtime reported
  `native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`
  under Qt Network 6.11.1.
- Final Ubuntu 26.04 x86_64 full-system QEMU focused contract/fixture: 2/2
  passed in 0.12/11.29 seconds against the exact final transport and harness
  hashes. The direct harness passed all 15 scenarios; architecture gates
  passed 2/2 in 5.00/144.35 seconds with all 92 hostile mutations.
- Linux ELF inspection recorded Qt6Network/Qt6Core 6.10.2. A controlled
  single-success trace directly opened Qt's OpenSSL backend, `libssl.so.3`,
  and `libcrypto.so.3`; the uninstrumented suite supplies behavioral evidence
  because tracing perturbs cancellation timing.
- Fresh macOS Qt-enabled configure/build/test passed 32/32. Fresh Qt-free
  configure/build/test passed 16/16. The final focused tests passed under
  AddressSanitizer and UndefinedBehaviorSanitizer.
- Tests-disabled Release builds passed on macOS and Linux with the testing
  macro and test factories absent. The macOS archive also contained no
  fixture strings; binary inspection recorded Qt Network, OpenSSL, Security,
  and CoreFoundation plus the current ad hoc linker signature.
- Documentation validation, the immutable source-pack validator,
  `git diff --check`, and tracked/on-disk workflow-absence scans passed.
  Source and generated Linux build state contained zero workflow files and
  zero build/configuration references to Actions.
- The completed simplification pass removed redundant access declarations and
  state, centralized error creation, made proxy selection total, clarified
  handshake-signal state, and consolidated fixture buffer wiping without
  weakening explicit TLS, deadline, cleanup, or scenario boundaries.
- The final independent security/correctness review found no actionable
  blocker after directly rechecking cancellation, deadlines, TLS terminals,
  redirects, proxy fallback, credential binding, bootstrap one-shot behavior,
  fixture integrity, hostile mutations, and production seam isolation.
- Windows target-native execution and dependency inspection remain openly
  assigned to BPR-0036. They do not block BPR-0026 or later implementation;
  they block only the affected Windows claims and RFC-BPR-0002 closeout.
