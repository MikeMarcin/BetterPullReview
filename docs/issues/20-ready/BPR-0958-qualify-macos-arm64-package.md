---
schema: "implement-rfc.card.v1"
id: "BPR-0958"
title: "Qualify the macOS Apple Silicon package"
status: "Ready"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M8 UX/UI and clean-target verification"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Qualify the macOS Apple Silicon package

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: On a clean macOS 13+ Apple Silicon environment install the exact universal candidate per user, launch, authenticate using an authorized safe route and open a fixture PR to the exact selected-file comparison.
- AC2: Exercise real rendered high-DPI source/navigation, the shared alignment controller and DiffLinkMap, keyboard/focus/accessibility, diagnostic preview and maintenance interactions with objective native evidence.
- AC3: Uninstall and explicitly purge in disposable owned roots; inspect credential/file/cache residuals against the documented policy and retain exact package/source/environment identities.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
