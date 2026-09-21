---
schema: "implement-rfc.card.v1"
id: "BPR-0954"
title: "Implement release maintenance host"
status: "Ready"
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

## M2 allocation for the successor

Follow the [typed ownership allocation](../../architecture/release-readiness.md)
for G5/G6: one bounded immutable metadata-only diagnostic snapshot, identical
preview/export identity, explicit cancellation/quiescence, and exact admitted
profile artifact inventory under the existing runtime lease. Omit/refuse
unclassified legacy prose; never raw-copy logs, databases, source/diff/comment/
draft/agent/MCP content into the default bundle. Qualify or implement the missing
Windows sink without inventing a second logging system.

Drain profile producers/windows, close stores/logger, preserve sibling profiles,
shared registry/updater state and unknown objects, and delete only exact
remembered vault bindings through their existing owner before removing metadata.
Retained Git/cache recovery and store migration remain with their existing
ports/transactions. Test partial failure, replay, sidecars, changed root/object
identity, profile replacement and shutdown. This is future work, not M2 acceptance.

## M3 policy handoff

BPR-0953 is Done at `b5a325423e1f11813fcc7cf9a6afd2b899ee350a`; its core
candidate is `7445b09efd0f831ae0838f1eee3c430cdc248ebc`. Adopt the
[implemented core contract](../../architecture/release-maintenance.md) and
`release_maintenance.hpp`, including exact cancellation targets and the
irreversible-boundary/terminal protocol. Snapshot parsing, native inventory,
actual digest/byte fidelity, output capability, root/vault admission and durable
recovery are host obligations; a core descriptor alone proves none of them.
Record unavailable native Windows or credential evidence honestly. The M3
handoff retains all G1–G9/package/native/security and predecessor obligations.
