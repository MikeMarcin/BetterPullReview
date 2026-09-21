---
schema: "implement-rfc.card.v1"
id: "BPR-0952"
title: "Allocate package and security ownership"
status: "Ready"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0951"]
milestone: "M2 Architecture allocation"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Allocate package and security ownership

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Inventory the real build/install/runtime graph, existing release tools, retention/migration/diagnostic surfaces and all RFC acceptance gaps at the exact candidate.
- AC2: Record one authority and typed intent/event/observation/request/delta path for each affected feature, including request/session/profile/revision/generation, effect ports, cancellation, shutdown and evidence-backed no-impact regions.
- AC3: Bind concrete package, hardening, provenance, native-fixture and documentation work to the adopted plan; preserve the macOS updater exception, inherited evidence limits and all remaining platform criteria.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.

Owner amendment r2 explicitly requires reuse of the existing project
`publish-release` skill, `tools/release.sh`, `tools/publish-release.sh` and
`macos-release` preset. The [M1 source inventory](../../verification/RFC-BPR-0014.md#owner-amendment-r2-existing-macos-release-workflow)
maps their existing universal build/deploy/sign/notarize/verify/package/publish
mechanisms and exact fingerprints to remaining acceptance. Inventory and extend
demonstrated gaps; do not duplicate those mechanisms or treat their existence
as clean-target, trust or publication evidence.
