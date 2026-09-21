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
- Ship only the declared runtime components required by the application and selected adapters, including Qt and other accounted-for third-party dependencies.
- Generate SBOMs, checksums, license notices, and dependency provenance.
- Apply platform compiler/linker/runtime hardening.
- Complete the final threat-model and corporate security documentation.
- Add retention, purge, diagnostics, uninstall, and migration operations.
- Establish a manual/corporate-managed update model for the initial release.

## Architectural allocation

- Packages must preserve the separate core, application-host, provider-adapter, Qt-adapter, and composition-root dependency graph. The core target remains independently buildable and testable without Qt or provider libraries. Application packaging does not require a separately distributed core SDK or general CMake embedding support; any install/export rules actually needed by a delivered package must preserve the same isolation.
- The composition root selects only documented first-party adapters. Qt plugin/library paths are restricted to trusted installation locations.
- Release scans must prove no widget/provider bypass, embedded browser/listener, accidental plugin loader, or undeclared process/network path was introduced.
- The security packet documents the typed effect path, credential-bearing host capabilities, agent/process boundaries, serialized decision lane, lifecycle/shutdown order, and the direct conformance evidence from `verification/architecture-conformance.md`.

## Current implementation basis — owner amendment r3

The 2026-09-20 RFC-BPR-0014 owner mandate r3 permits revision of obsolete
implementation prescriptions using the current repository as evidence. The
nine acceptance criteria below, supported platform matrix, security outcomes
and canonical closeout order are unchanged. This is separate from RFC-BPR-0035's
own amendment numbering.

The project `publish-release` skill, `macos-release` preset and
`tools/release.sh` already provide the macOS build/deploy/sign/notarize/verify/
DMG/ZIP/checksum path. `tools/publish-release.sh` separately guards private
publication. Reuse those mechanisms and complete demonstrated gaps; source
availability or an older successful candidate is not final package evidence.
Publication still requires an explicit request for the prepared version.

The delivered dependency closure can include required third-party libraries
beyond Qt. Each shipped component, including statically linked components,
must appear in dependency provenance, SBOM and licensing/source-offer analysis.
The earlier Qt-and-first-party-only wording must not exclude actual required
dependencies from that accounting. Dependency minimality and trusted plugin/
library paths remain required.

Core isolation is a build/test and delivered dependency-graph obligation, not
a requirement to create an unrelated SDK or make this audited top-level build
embeddable. Complete application installation/package rules where needed.
Likewise, inventory existing profile, credential, maintenance, diagnostic,
helper and updater ownership before adding runtime code. A generic core/host/Qt
planning stage may reuse directly verified behavior or record a source-backed
no-impact result; it does not require a duplicate implementation.

The [amendment and evidence ledger](../verification/RFC-BPR-0014.md#owner-amendment-r3-current-implementation-prescriptions)
records the source basis and remaining native, package, security and human gates.

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

The [M2 architecture allocation](../architecture/release-readiness.md) maps the
current typed owners, reusable maintenance/release paths, missing controls and
package gaps to exact successor cards and public/native checks. Its Linux
baseline is a declared qualification target, not observed native support.
