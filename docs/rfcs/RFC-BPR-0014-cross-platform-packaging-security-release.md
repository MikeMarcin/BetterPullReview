---
id: "RFC-BPR-0014"
title: "Cross-Platform Packaging, Signing, Security Hardening, and Release Readiness"
status: "Proposed"
area: "BPR"
milestone: "M6 Production Readiness"
depends_on: ["RFC-BPR-0013"]
pack_version: 2
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# RFC-BPR-0014: Cross-Platform Packaging, Signing, Security Hardening, and Release Readiness

## Summary

Ship auditable Windows, macOS, and Linux packages with signing, notarization, SBOMs, dependency notices, platform hardening, retention controls, and a documented corporate security posture.


## Motivation

A corporate-acceptable tool needs more than source-level security claims. The delivered binaries, installers, dependency inventory, platform hardening, credentials, diagnostics, update policy, and removal behavior must be inspectable and verifiable.

## Goals

- Produce signed Windows, notarized universal macOS, and signed Linux packages.
- Ship only required Qt and first-party runtime components.
- Generate SBOMs, checksums, license notices, and dependency provenance.
- Apply platform compiler/linker/runtime hardening.
- Complete the final threat-model and corporate security documentation.
- Add retention, purge, diagnostics, uninstall, and migration operations.
- Establish a manual/corporate-managed update model for the initial release.

## Architectural allocation

- Packages must preserve the separate core, application-host, provider-adapter, Qt-adapter, and composition-root dependency graph. The core package/target remains buildable without Qt or provider libraries.
- The composition root selects only documented first-party adapters. Qt plugin/library paths are restricted to trusted installation locations.
- Release scans must prove no widget/provider bypass, embedded browser/listener, accidental plugin loader, or undeclared process/network path was introduced.
- The security packet documents the typed effect path, credential-bearing host capabilities, agent/process boundaries, serialized decision lane, lifecycle/shutdown order, and the direct conformance evidence from `verification/architecture-conformance.md`.

## Supported initial matrix

- Windows 10/11 x64 using the selected supported MSVC toolchain.
- macOS 13+ universal x86_64/arm64 application.
- Linux x86_64 packages built against the documented oldest supported baseline; signed DEB and RPM are preferred, with a signed archive optional.

Additional architectures require explicit native build, package, and runtime evidence rather than being implied by Qt support.

## Build ownership

GitHub Actions is not part of the BetterPullReview build, test, package, or
release path. Release evidence is collected from clean release-operator or
corporate-managed Windows, macOS, and Linux environments. The resulting
packages, signatures, checksums, SBOMs, inspection reports, and sanitized test
records—not a hosted workflow name—are authoritative.

## Packaging rules

- Per-user installation is the default and should not require administrator rights.
- Qt dynamic linking and license obligations are handled explicitly.
- Plugin and library search paths are restricted to trusted installation locations.
- The initial release uses manual/corporate-managed updates. The 2026-09-20
  RFC-BPR-0035 implementation mandate amends this only for eligible trusted macOS
  packages with the qualified signed helper/startup protocol: discovery may be
  automatic, while download and installation each require a user gesture.
  Windows/Linux and managed packages retain manual distribution. This exception
  changes neither this RFC's acceptance nor canonical closeout order. Owner
  amendment r3 defers updater-specific Intel validation to BPR-0950 outside
  RFC-BPR-0035 closeout; live private-release evidence remains on BPR-0911/0912.
  This RFC's separate package/security acceptance matrix is unchanged. See the
  [updater security boundary](../security/release-updater.md) and
  [operator/recovery guide](../development/application-updates.md).
- No telemetry or crash upload occurs automatically.
- Diagnostic bundles are user-initiated, previewable, and source/secret-free by default.

Package verification must prove that the selected real application path, not
a test-only shell or isolated widget, composes RFC-BPR-0004's one shared
alignment controller and `DiffLinkMap`. Target-native high-DPI rendering and
runtime/dependency inspection cannot be replaced by hosted workflow output.

## Acceptance criteria

- Clean Windows, macOS Intel/Apple Silicon, and Linux test environments install, launch, authenticate, open a fixture PR, and uninstall successfully.
- Package signatures/notarization verify independently.
- Runtime dependency scan shows no WebEngine, browser engine, Node, local server, arbitrary plugin loader, or undeclared network endpoint.
- SBOM, license notices, source-offer obligations, and checksums match the delivered artifacts.
- Hardened build settings, static analysis, sanitizers, fuzzers, secret scans, and vulnerability scans pass release policy.
- Credential vaults, file permissions, cache locations, retention, purge, and uninstall behavior match documentation.
- Diagnostic and crash artifacts pass canary redaction and contain no source/comment bodies by default.
- Corporate security overview clearly states network destinations, data stored, subprocesses, credentials, AI boundary, logging, and removal.
- Delivered target/dependency graphs and runtime inspection prove the functional core remains Qt/provider-free and all effects are reachable only through the packaged host/composition path.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Package is trusted | Delivered installer/app/package | Verify signature, notarization, checksum, and SBOM |
| Install requires no unnecessary privilege | Clean environment | Install per user and inspect requested privileges |
| Runtime attack surface matches docs | Packaged binary | Dependency, socket, process, plugin-path, and network inspection |
| Removal and purge are complete | Installed application | Uninstall and explicit purge; inspect documented residuals |
| Packaged architecture matches source | Package dependency/runtime scan | Core remains isolated; only composition root owns concrete adapters and effects |

## Documentation impact

Installation, supported matrix, signing, dependencies, licensing, hardening, credentials, storage, network destinations, AI subprocesses, diagnostics, retention, purge, update policy, and security review packet are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.

## Live implementation records

The [adopted twelve-stage plan](plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
preserves the immutable pack-v2 issue-plan stages. The [planning import](assets/rfc-bpr-0014-planning-v1.LIVE_IMPORT.md)
records baseline, provenance and live ID mapping; the [verification ledger](../verification/RFC-BPR-0014.md)
tracks per-criterion evidence and limits. BPR-0951 through BPR-0965 own the
remaining work. This RFC remains Proposed until BPR-0965 satisfies the full
package/security matrix and dependency-ordered closeout.
