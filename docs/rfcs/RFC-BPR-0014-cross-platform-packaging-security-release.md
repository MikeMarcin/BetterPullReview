---
id: "RFC-BPR-0014"
title: "Release Packaging and Maintenance Foundation"
status: "Auto-Verified"
area: "BPR"
milestone: "M6 Production Readiness"
depends_on: ["RFC-BPR-0013"]
pack_version: 2
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# RFC-BPR-0014: Release Packaging and Maintenance Foundation

## Summary

Implement auditable cross-platform package definitions, dependency/source
inventory and release-verification mechanics, typed maintenance controls and
truthful operator documentation. Owner amendment r4 separates this accepted
foundation from complete native and release-security qualification, retained
without waiver in [RFC-BPR-0038](RFC-BPR-0038-native-package-release-security-qualification.md).

## Motivation

A corporate-acceptable tool needs more than source-level security claims. The delivered binaries, installers, dependency inventory, platform hardening, credentials, diagnostics, update policy, and removal behavior must be inspectable and verifiable.

## Goals

- Provide Windows, universal macOS and Linux package paths; retain final native,
  signing and notarization qualification under RFC-BPR-0038.
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

## Terminal scope — owner amendment r4

Auto-Verified covers only the foundation criteria below. BPR-0965 records
their exact checks and the unchanged release stop conditions. It is not a
release readiness, native support or distribution approval.

The owner explicitly directed RFC-0014 to land now, with unfinished work filed
as follow-ups and a later RFC where necessary. This changes terminal scope:
the nine foundation criteria below replace the former full-release criteria.
It does **not** mark the former criteria passing. All nine are preserved
verbatim in RFC-BPR-0038, with BPR-0958–0962 still Blocked and BPR-0973 owning
successor closeout. The native matrix and release policy are unchanged there.

The [r4 mandate](plans/rfc-bpr-0014-release-readiness/mandate-owner-closeout-r4.md)
and [closeout ledger](../verification/RFC-BPR-0014.md#owner-amendment-r4-foundation-closeout)
preserve the immutable RFC base and all accepted M1–M7/M10/M11 outcomes.
M8/M9 definitions and exits remain unchanged in the extended shared plan;
M12 accepts only this foundation and M13 owns complete successor qualification.
Original authored imports and historical handoffs are not rewritten.

Qt 6.11.2 remains the owner-selected dependency. Corresponding source remains
a separate version-bound release asset, with installed notices and exact
references, not embedded in every app. Actual JaegAW Windows and WSL/Linux
observations remain evidence only for their recorded operations/environments.
Neither this amendment nor Auto-Verified status authorizes signing, notarization,
publication or claims of complete platform support. Failed vulnerability scans,
native/vault/crash gaps and the unanswered Windows licensing decision stay open.

## Historical implementation basis — owner amendment r3

The 2026-09-20 RFC-BPR-0014 owner mandate r3 permits revision of obsolete
implementation prescriptions using the current repository as evidence. The
nine then-current release criteria, supported platform matrix, security outcomes
and canonical closeout order were unchanged by r3. R4 now transfers those
criteria intact to RFC-BPR-0038. This is separate from RFC-BPR-0035's
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
  RFC-BPR-0038 retains the separate package/security acceptance matrix. See the
  [updater security boundary](../security/release-updater.md) and
  [operator/recovery guide](../development/application-updates.md).
- No telemetry or crash upload occurs automatically.
- Diagnostic bundles are user-initiated, previewable, and source/secret-free by default.

Package verification must prove that the selected real application path, not
a test-only shell or isolated widget, composes RFC-BPR-0004's one shared
alignment controller and `DiffLinkMap`. Target-native high-DPI rendering and
runtime/dependency inspection cannot be replaced by hosted workflow output.

## Acceptance criteria

- Windows, universal macOS and baseline Linux package definitions reuse the existing release workflow and document target prerequisites without claiming complete native qualification.
- The existing publish-release path provides build, checksum, signing, notarization and independent trust-verification mechanisms; available offline package evidence remains distinct from release trust or publication authority.
- Available exact-candidate package and hostile-boundary checks verify declared composition, dependencies, loader restrictions and bounded profile-activation IPC; missing native runtime evidence remains explicitly assigned.
- Qt 6.11.2 package inventories bind SBOMs, notices, checksums and separate version-bound corresponding-source assets; exact licensing decisions and unfulfilled distribution obligations remain distinct.
- Release-policy hardening, analysis, sanitizer, fuzz, secret and vulnerability mechanics have recorded exact-candidate results, with failed scans and unobserved coverage preserved as open qualification work.
- Typed core, host and Qt maintenance paths implement bounded diagnostic cleanup and explicit profile purge through retained-lease retirement and recovery, with direct public-path verification and documented residuals.
- Metadata-only diagnostic preview/export and hostile canary checks preserve source/secret boundaries; full resident-material crash and native privacy qualification remains open.
- Corporate security overview clearly states network destinations, data stored, subprocesses, credentials, AI boundary, logging, and removal.
- Tracked-source architecture checks and available exact-package graph/composition evidence preserve the Qt/provider-free functional core and host-owned effects without claiming whole-matrix certification.

## Verification matrix

The following original release qualification matrix is retained for RFC-BPR-0038,
not asserted complete by this foundation closeout. Mandate r3 permits modernizing
obsolete implementation prescriptions while preserving security outcomes.
M9 reconciles “no local server” in the transferred criteria with
the already-shipped profile activation adapter, not with any new network
service. The [release security policy](../security/release-policy.md) specifies
the exception's negative controls and keeps trust, legal, vulnerability and
native-matrix acceptance independent. The immutable imported RFC/card baseline
is retained as provenance; this amendment does not waive its security outcome.

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

The [extended shared plan](plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
preserves the immutable pack-v2 stages and records r4's M12 amendment plus
RFC-BPR-0038 M13. The [planning import](assets/rfc-bpr-0014-planning-v1.LIVE_IMPORT.md)
records baseline, provenance and live ID mapping; the [verification ledger](../verification/RFC-BPR-0014.md)
tracks per-criterion evidence and limits. BPR-0951–0957/0963–0965 own this
foundation; BPR-0958–0962/0973 own successor qualification. The stage narratives
below are historical checkpoints: their then-pending statements and r3 M12
gates are superseded only by the explicit r4 scope transfer, never by invented
observations. The current disposition is in the r4 ledger.

The [M2 architecture allocation](../architecture/release-readiness.md) maps the
current typed owners, reusable maintenance/release paths, missing controls and
package gaps to exact successor cards and public/native checks. Its Linux
baseline is a declared qualification target, not observed native support.

The [M3 maintenance policy](../architecture/release-maintenance.md) defines
bounded metadata-only preview/export selection and exact profile-purge
confirmation, with direct Qt/provider-free core tests. It reuses existing
retention, migration-value, profile and updater policy. Native effects,
controls, package privacy and the delivered matrix remain unaccepted.

The same contract records M4's bounded application host, exact native inventory,
durable purge recovery, credential-owner cleanup and metadata-only local export.
The existing Quill sink gains private Windows support without a second logger.
The r3 whole-profile-purge recovery adjustment is explicit; ordinary retention
and store migration keep their existing owners. Manual/native disposable-root
tests and Windows source checks do not certify Qt composition, native Windows
execution, real vault continuity, package uninstall or the delivered matrix.
Those obligations and all nine canonical acceptance criteria remain unchanged.

M5 mounts the real profile Settings diagnostic preview/export path and verifies
the typed clear/purge presentation, exact confirmations, cancellation,
accessibility and bounded updates. The
[presentation/composition boundary](../architecture/release-maintenance.md#m5-settings-presentation-and-bounded-composition)
keeps live destructive commands unavailable until M6 supplies complete profile
retirement/store/logger/vault composition under the retained lease. Controlled
widget/host success does not certify production purge. Existing retention and
release paths are reused unchanged; package/native/human/security acceptance
remains with its assigned later owners.

M6 now composes the [retained-lease retirement and recovery barrier](../architecture/release-maintenance.md#m6-retained-lease-maintenance-composition)
before enabling live clear/purge, without restarting producers on cancellation.
The [application-only package definitions](../development/release-packages.md)
reuse the existing macOS release path, add Windows MSI and baseline Linux
DEB/RPM plus a per-user archive, and carry exact dependency/source/notices/SBOM
and checksum mechanics. Their tests-disabled runtime restricts plugin loading
and locates the actual shipped helpers. Unavailable native matrix, attended
acceptance, release trust and distribution-license approval remain open; local
ad-hoc sealing is not signing/notarization acceptance or publication authority.

M7 adds [exact package and hostile-boundary verification](../verification/RFC-BPR-0014.md#milestone-7-focused-tests-and-hostile-fixtures)
without changing product or release behavior. It binds the M6 tests-disabled
artifact to complete file hashes, required packaged capabilities, composition
anchors, byte/symbol/graph seam checks and contained native dependencies. Its
relocated production/MCP and diagnostic canary courses reject broken boundaries.
Actual Windows and WSL probes verify process/socket/library inspector contracts;
they do not qualify a native package or alter the declared platform baselines.
Required install/authentication/removal, independent trust, crash/security and
license/distribution acceptance remain with M8/M9; this RFC stays Proposed.

M8's [native candidate ledger](../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance)
records exact macOS rendered/maintenance evidence and native Windows MSI
build/install/removal mechanics, together with the concrete defects they
exposed and corrected. It reuses the existing release paths; native baselines,
clean-account authentication/vault/purge, required human decisions and full
Windows stripped-PE inspection are not inferred from partial success. Both
original scoped macOS questions were accepted for their exact candidate at
hand-back 454. The accompanying Overview observation reproduced a real
first-load failure-presentation defect, now corrected with a focused regression.
A new exact recovery candidate was accepted at hand-back 460; all three
decisions remain scoped to their original questions and candidate identities.
They do not satisfy the clean-account or vault criteria. The Intel and declared
Linux baselines remain unavailable, and the elevated Windows SSH session is
not an ordinary-user desktop qualification. All four M8 cards and the native
matrix exit remain pending; the human-decision binding exit is satisfied.
Independent M9 may proceed from accepted M7 after coordinator reassignment;
no trust approval, publication or RFC closeout is claimed.

M9's [security ledger](../verification/RFC-BPR-0014.md#milestone-9-security-and-boundary-verification)
records exact macOS/Windows package audits, including the completed mechanical
stripped-PE inspection, but remains blocked on independent trust, unresolved
vulnerabilities and native/privacy/hardening inputs. The owner's source-delivery
decision replaces embedded source archives with a separate version-bound asset
through the existing release paths. Only the revised macOS licensing-v002
criterion was accepted at hand-back 470. That decision grants no security,
native, changed-artifact or publication approval; recipient access and retention
remain distribution obligations.

M10's [documentation reconciliation](../verification/RFC-BPR-0014.md#milestone-10-agent-and-product-documentation)
provides the tracked [corporate security packet](../security/corporate-security-overview.md),
[installation/trust/source guide](../development/release-packages.md) and
[storage/diagnostics/removal guide](../development/storage-and-diagnostics.md).
Architecture, UI and agent guidance now reflect the composed maintenance path,
exact ownership and cleanup residuals. All M8/M9 blockers remain explicit.
Documentation completion permits independent M11 simplification, not M12
closeout or a change to this RFC's Proposed status.

M11's [simplification pass](../verification/RFC-BPR-0014.md#milestone-11-simplification)
separates maintenance effect execution from completion scheduling and shares
the release tool's bounded file/ZIP digest logic. Focused contracts and the
composed maintenance course preserve behavior, authority and separate-source
delivery. Core policy, native cleanup, presentation, selected dependencies and
protected candidates remain unchanged. M11 completion leaves M12 gated by the
full M8/M9 and canonical predecessor acceptance; no release approval is implied.

M9's [post-M11 dependency remediation](../verification/RFC-BPR-0014.md#m9-dependency-remediation-after-m11)
implements the owner-selected Qt 6.11.2 upgrade and rebuilds exact macOS and
native Windows packages with the separate-source model intact. Fresh raw Grype
gates still fail (32 macOS / 30 Windows matches). Source-backed applicability
evidence awaits independent security-owner disposition; changed licensing
inventories have separate owner/legal questions. Historical decisions do not
transfer, broad-suite failures and native/trust/privacy gaps remain explicit,
and neither M9 exit was accepted. At hand-back 478, only the exact macOS
licensing-v003 question was accepted; Windows licensing-v002 remains unanswered.
R4 subsequently changes M12/RFC0014 terminal scope, not those M9 facts.
