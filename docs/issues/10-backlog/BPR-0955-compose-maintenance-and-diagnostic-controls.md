---
schema: "implement-rfc.card.v1"
id: "BPR-0955"
title: "Compose maintenance and diagnostic controls"
status: "Backlog"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0954"]
milestone: "M5 Qt presentation adapter"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Compose maintenance and diagnostic controls

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Expose the required user-initiated diagnostic preview/export and explicit retention/purge controls through typed intents and core command projections, reusing existing controls where justified.
- AC2: Verify keyboard, focus, accessibility, cancellation and safe destructive-confirmation defaults on the real rendered application; Qt performs no filesystem, vault, provider or process effect.
- AC3: Apply bounded changed projections without routine whole-model reset; record exact source/public-path evidence for any presentation no-impact disposition.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
