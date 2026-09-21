---
schema: "implement-rfc.card.v1"
id: "BPR-0954"
title: "Implement release maintenance host"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0953"]
milestone: "M4 Imperative application host"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8","evidence":{"kind":"observation","detail":"Provider-free one-job host, exact private-root storage/local-output/lifecycle capabilities, POSIX and Windows native mechanisms, immutable snapshot and complete bounded inventory, credential-before-metadata journal/receipt recovery implemented. Existing retention/migration owners reused and actual public retention course passes. Explicit residuals and r3 full-profile-purge rationale in docs/architecture/release-maintenance.md; no full uninstall/native package claim."}},{"criterion":"AC2","outcome":"pass","revision":"19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8","evidence":{"kind":"command","detail":"Exact-candidate 13 host/storage/vault courses and six real logger/profile/retention courses pass; both new host/native direct courses pass instrumented ASan/UBSan; eight Windows source/test TUs pass GCC warnings-as-errors syntax checks. Direct tests cover cancellation, reordering, retirement, typed failure, reentrancy, shutdown, canaries, permissions, exact vault keys, process interruption, torn/foreign receipts and changed-object refusal. Native Windows/MSVC and OS-vault qualification remains M8. Receipts build/rfc0014-run/m4-*-evidence.json."}},{"criterion":"AC3","outcome":"pass","revision":"19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8","evidence":{"kind":"observation","detail":"Core policy unchanged; host converts only typed events and retains actual effect lifetime. Default export omits all unframed prose/source/comment/secret bytes. Exact closed root inventory excludes developer/GitHub/sibling/shared scopes; full candidate archive architecture, source no-impact, canonical-acceptance fidelity, 20-file format, nine-unit tidy, documentation/64 tests, board and plan checks pass. No human/independent/native package verdict inferred."}}]
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

Implemented under mandate r3 by sole child `/root/rfc0014_m4`, selected
coordinator-implementer recipe revision 2, against immutable RFC base
`d0a8a6dda005af12b3818234f4878a9f48a44146`. Entry was
`a9146cac58e584b3c7f225eabe8b31d39eed6bf7`; guarded claim was
`a578f8ec07fb1dd1eb98192e61ab8473152b023b`. Source candidate is
`19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8`.

- AC1 passes: the provider-free one-job host composes exact storage/lifecycle/
  output capabilities; native POSIX/Windows inventory and private local export
  retain genuine root authority. Exact remembered vault slots are deleted before
  metadata, with digest-bound durable journal/receipt recovery. Existing epoch
  retention and per-store migration owners remain. The
  [contract](../../architecture/release-maintenance.md) records explicit residuals
  and r3's whole-profile cache-inventory rationale; no recursive-root deletion
  or universal migration engine is added.
- AC2 passes at the candidate: 13 focused host/storage/vault courses and six
  actual logger/profile/retention courses; the new host/native courses also pass
  with new translation units instrumented by ASan/UBSan. Tests cover exact
  preview/export bytes, hostile metadata, stale/reordered/retired completion,
  cancellation/commit/shutdown, native permissions and containment, credential
  failure, crash/torn receipt, corrupt journal and replaced-object refusal.
  Eight Windows source/test translation units pass MinGW warnings-as-errors
  syntax checks; this is not MSVC/native Windows acceptance.
- AC3 passes: core policy is unchanged and the host only returns typed events;
  the classifier omits unframed prose and source/comment/secret canaries.
  Architecture checks pass the complete candidate archive. Core, Qt/app,
  GitHub/Git/MCP and release-workflow source no-impact checks, formatting,
  nine-unit tidy, documentation/64 verifier tests, board/plan and unchanged
  canonical RFC acceptance checks pass.

The [M4 ledger](../../verification/RFC-BPR-0014.md#milestone-4-imperative-application-host)
records exact commands, earlier corrected failures and evidence limits. Local
receipts are `build/rfc0014-run/m4-*-evidence.json`. Only this card and both M4
exits are accepted. Actual Qt lifecycle composition, rendered/human interaction,
Windows/Linux native execution, OS-vault continuity, native crash artifacts and
tests-disabled package/uninstall/security qualification remain with M5–M9.
No release/notary/publication or remote mutation ran; no human/independent
verdict is inferred. Overlapping BPR-0885/BPR-0891 remain unclaimed.

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
