---
schema: "implement-rfc.card.v1"
id: "BPR-0959"
title: "Qualify the macOS Intel package"
status: "Backlog"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Qualify the macOS Intel package

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: On a clean native macOS 13+ Intel environment install the exact universal candidate per user, launch, authenticate using an authorized safe route and open a fixture PR to the exact selected-file comparison; two Mach-O slices do not prove native execution.
- AC2: Exercise real rendered high-DPI source/navigation, the shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions with objective native evidence.
- AC3: Uninstall and explicitly purge disposable owned roots and inspect documented credential/file/cache residuals. RFC-0035's Intel updater deferral does not waive this separate RFC-0014 package requirement.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
