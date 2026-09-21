---
schema: "implement-rfc.card.v1"
id: "BPR-0954"
title: "Implement release maintenance host"
status: "Backlog"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0953"]
milestone: "M4 Imperative application host"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Implement release maintenance host

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Compose or complete bounded retention, explicit purge, migration, uninstall-residual and diagnostic-export effects through narrow host-owned ports with exact private-root and authority checks.
- AC2: Exercise cancellation, reordering, failure/restart recovery, canary redaction, file permissions, credential lifecycle and shutdown through fake/manual executors plus disposable native roots where relevant.
- AC3: Keep semantic policy in core, credential/source/comment bodies out of default diagnostics, and GitHub/developer worktrees outside maintenance authority.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
