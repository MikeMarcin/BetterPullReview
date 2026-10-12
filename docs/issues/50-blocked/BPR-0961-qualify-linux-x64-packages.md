---
schema: "implement-rfc.card.v1"
id: "BPR-0961"
title: "Qualify the Linux x64 packages"
status: "Blocked"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["linux"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
blocked_by: "clean-ubuntu-desktop-vault"
---

# Qualify the Linux x64 packages

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Owner amendment r4 — successor qualification

This unchanged-criteria standalone follow-up now belongs to
[RFC-BPR-0038](../../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md).
RFC-0014 closes only its implemented foundation. No pending criterion, native
observation, trust/security gate or human decision is accepted by this transfer.
Paths, blocked state and exact historical evidence remain intact. The
[successor ledger](../../verification/RFC-BPR-0038.md) records the latest Qt 6.11.2
candidates, completed PE mechanics, raw 32/30 Grype failures and unanswered
Windows licensing-v002 decision; earlier records below remain historical.

## Owner amendment 2026-10-11 — selected Ubuntu 26.04 route

The owner selected Ubuntu 26.04 x86_64 for the next release, replacing Ubuntu
24.04 and removing RHEL/RPM from the selected release set. Linux is an optional
convenience distribution. [BPR-1386](../30-in-progress/BPR-1386-prepare-ubuntu-2604-release-route.md)
prepares DEB/per-user mechanics with a glibc 2.43 ceiling under
`release-next-20261011-ubuntu2604-r2`. WSL2 is a valid ordinary binary producer;
WSL/WSLg observations remain narrower than clean Ubuntu desktop/install/vault
qualification. The original acceptance below is retained as authored history.
For current AC1, test the Ubuntu 26.04 DEB and default per-user archive; RPM and
the two obsolete selected baselines are no longer required. AC2/AC3's complete
rendered, authenticated, Secret Service, purge/residual and removal obligations
remain pending, along with independent trust/security acceptance. No package is
built or qualified by the owner's amendment, and this card stays Blocked.

## Original acceptance (before the 2026-10-11 Linux scope amendment)

- AC1: Declare and test the oldest supported native Linux x64 baseline; install exact DEB and RPM candidates, document package-manager privileges and provide the RFC's default per-user path, then launch, authenticate through an authorized safe route and open a fixture PR to the exact selected-file comparison.
- AC2: Exercise real high-DPI source/navigation, one shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions; inspect desktop/hicolor/plugin integration on the delivered package.
- AC3: Uninstall and explicitly purge disposable owned roots and inspect documented Secret Service/file/cache residuals; bind evidence to package/source/distribution/runtime identities.

## Verification and execution record

Imported under mandate r1 at RFC base
`d0a8a6dda005af12b3818234f4878a9f48a44146`; all criteria remain pending.
M8 r3 reconciled the available bpr-linux station: Ubuntu 26.04 LTS x86_64 under
WSL, not either declared baseline. No Ubuntu 24.04 or RHEL 10 environment was
provisioned. Existing unrelated Docker images/containers were not changed.
No DEB/RPM qualification or desktop/vault claim was inferred from WSL or a
container. The station's existing dirty MASK lock remains protected and its
writer lease is free. Supply both declared clean desktop baselines (including
Secret Service and ordinary-user operation) before claiming native acceptance.
See the [M8 ledger](../../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance).

## M2 baseline declaration

The [M2 baseline rationale](../../architecture/release-readiness.md#package-and-security-work-allocation)
selects glibc 2.39 with Ubuntu 24.04 LTS x86_64 for DEB and RHEL 10 x86_64 for
RPM qualification. RHEL 10's x86-64-v3 requirement and the actual package's
C++ runtime/toolchain/CPU floor must be visible in installation instructions.
These are engineering targets, not successful builds or supported-native
observations. Qualify both clean environments and the ordinary-user route;
retain pending status when hardware/operator/trust inputs are unavailable.
