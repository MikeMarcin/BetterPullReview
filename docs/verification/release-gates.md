# Release Gates

No release candidate is acceptable until all applicable gates pass from a clean checkout and clean package environment.

## Functional

- Every RFC is Auto-Verified and every required issue card is in the terminal done lane.
- GitHub.com and GHES 3.18 contract suites pass.
- Epoch, diff, line identity, navigation, drawer, theme, ledger, conversation, defect, queue, and AI workflows pass through the public application surface.
- Mixed-client, uninstall, database-loss, stale-head, and remote-mutation target tests pass.

## Architecture

- The complete [Architecture Conformance Plan](architecture-conformance.md) passes for the final composition.
- The functional core builds and tests with Qt/provider packages unavailable.
- Every effectful capability is host-owned and every UI workflow reaches it through a semantic core request; no widget/provider bypass exists.
- Async completions return to one serialized application lane with direct stale-generation/cancellation evidence.
- Qt actions/models reflect semantic projections and use incremental updates for bounded changes.
- Structural performance gates prove routine work scales with the active set plus the changed set.
- Core, host, Qt adapter, and end-to-end test layers all exist and cover the claimed responsibilities.

## Security

- Threat model reviewed against final dependency and process graph.
- Secret scanning, dependency vulnerability scan, SBOM generation, static analysis, sanitizers, and fuzzers pass.
- No embedded browser, TCP/UDP server, remote plugin/theme loader, telemetry,
  or certificate bypass exists. The declared listener exceptions are the
  existing bounded owner-private AF_UNIX profile activation channel and the
  RFC-BPR-0045 tour command channel; their peer, frame, identity and lifetime
  checks must pass the release policy.
- Git and AI subprocess policies are verified with hostile fixtures.
- Diagnostics and logs pass canary redaction tests.

## Performance and resilience

- Large review benchmarks pass defined latency, memory, and cancellation budgets.
- Crash/restart, schema migration, offline cache, rate-limit, and partial-failure scenarios pass.
- UI remains responsive while network, Git, highlighting, diff, and AI jobs run or cancel.

## Accessibility and visual

- Keyboard-only review path passes.
- Screen-reader smoke coverage exists for custom diff surfaces.
- Light, dark, system, and high-contrast modes pass screenshot and contrast checks.
- High-DPI and fractional-scaling layouts pass on supported platforms.

## Packaging

- Windows installer is signed and verifies on a clean VM.
- macOS universal app is signed, hardened, notarized, and verifies on Intel and Apple Silicon targets.
- Linux packages verify signatures, dependencies, file permissions, uninstall behavior, and supported baseline.
- License notices and source-offer obligations are complete for shipped dependencies.
  The exact version-bound corresponding-source ZIP is a separate release asset,
  bound by installed notices/offers/provenance and SPDX. Every binary recipient
  has equivalent access; corporate/offline redistribution carries it, and the
  accountable distributor preserves its required immutable retention.
- SBOM and checksums match delivered packages.

## Documentation

- Product contract, architecture, security, provider, epoch, diff, review semantics, AI, build, package, and operator documentation match runtime behavior.
- Every generated index and documentation validator passes.
- No release note overclaims a stub, disabled path, test-only surface, or unsupported platform.

The [corporate packet](../security/corporate-security-overview.md),
[package guide](../development/release-packages.md) and
[storage/removal guide](../development/storage-and-diagnostics.md) supply current
operator-facing facts. RFC-0014 M8/M9 remain blocked: unsigned/ad-hoc candidates,
21 macOS / 19 Windows vulnerability matches, missing native/account/vault/crash
evidence and external dependency hardening are not waived by documentation,
source-delivery approval, a successful audit marker or a prior updater course.
The [release security policy](../security/release-policy.md) defines exact
evidence/disposition requirements. Publication still uses the existing skill
and requires the prepared version's explicit authorization.

## Public-preview selection

A delivered artifact must report its declared platform selection through the
unconditional zero-request probe. macOS is enabled for GitHub.com; Linux and
Windows are unavailable, and GHES is refused. Test-only forced selections,
loopback trust seams, and live-candidate flags must be absent from tests-disabled
Release. An enabled claim also needs the normal provider/normalizer, anonymous
smart HTTP, mode-isolated cache/epoch/diff, and explicit unavailable-action
checks. A passing macOS artifact does not enable another platform. The
[RFC-0017 ledger](RFC-BPR-0017.md) retains exact bounded evidence and remaining
closeout gates; this section is not release approval.

## Source-line compatibility qualification

RFC-BPR-0022 fixture success does not authorize a production unsupported
classifier. Any enabled tuple requires exact observed or explicitly approved
recorded non-creation evidence for the operation/server/API/classifier revision.
Production classifier2 admits only the owner-approved PR2/base/head/actor/profile2
contract with exact sealed-source and patch validation; other scopes stay unknown.
The [exact RFC-0022 candidate](RFC-BPR-0022.md) has observed fallback publication,
GET-only recovery and accepted GitHub-only continuity. The r4 owner amendment
accepts app-closed continuity plus fresh-host reconstruction for AC-10 and waives
separate independent review for source `0f3188093`. Native off-hunk success, native
GHES, Windows qualification and actual installed desktop uninstall remain
unobserved. RFC closeout grants no package trust or release approval.
