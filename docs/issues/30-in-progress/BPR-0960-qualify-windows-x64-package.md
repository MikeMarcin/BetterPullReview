---
schema: "implement-rfc.card.v1"
id: "BPR-0960"
title: "Qualify the Windows x64 package"
status: "In Progress"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Qualify the Windows x64 package

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: On clean native Windows 10/11 x64 environments install the exact MSVC-built candidate per user without unnecessary administrator rights, launch, authenticate through an authorized safe route and open a fixture PR to the exact selected-file comparison.
- AC2: Exercise real high-DPI source/navigation, one shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions; cross-compilation and hosted output cannot replace native evidence.
- AC3: Uninstall and explicitly purge disposable owned roots and inspect documented Credential Manager/file/cache residuals; bind evidence to package/source/OS/toolchain identities.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
