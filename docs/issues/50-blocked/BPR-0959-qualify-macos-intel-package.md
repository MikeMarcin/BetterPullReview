---
schema: "implement-rfc.card.v1"
id: "BPR-0959"
title: "Qualify the macOS Intel package"
status: "Blocked"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
blocked_by: "native-intel-environment"
---

# Qualify the macOS Intel package

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

## Acceptance

- AC1: On a clean native macOS 13+ Intel environment install the exact universal candidate per user, launch, authenticate using an authorized safe route and open a fixture PR to the exact selected-file comparison; two Mach-O slices do not prove native execution.
- AC2: Exercise real rendered high-DPI source/navigation, the shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions with objective native evidence.
- AC3: Uninstall and explicitly purge disposable owned roots and inspect documented credential/file/cache residuals. RFC-0035's Intel updater deferral does not waive this separate RFC-0014 package requirement.

## Verification and execution record

Imported under mandate r1 at RFC base
`d0a8a6dda005af12b3818234f4878a9f48a44146`; all criteria remain untested.
M8 r3 found an arm64 Mac and the JaegAW Windows/WSL stations, but no native
Intel Mac. The exact universal 0.1.3 candidate at
`6ad716350c7f2e02a1b00370ea49cd5b77ce4c15` has both slices; neither those slices
nor Rosetta is native Intel evidence. A clean macOS 13+ Intel account/operator
is the concrete prerequisite. No baseline or updater-deferral substitution
was made. See the [M8 ledger](../../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance).
