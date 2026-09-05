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
- No embedded browser, listener, remote plugin/theme loader, undocumented telemetry, or certificate bypass exists.
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
- SBOM and checksums match delivered packages.

## Documentation

- Product contract, architecture, security, provider, epoch, diff, review semantics, AI, build, package, and operator documentation match runtime behavior.
- Every generated index and documentation validator passes.
- No release note overclaims a stub, disabled path, test-only surface, or unsupported platform.

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
