---
schema: "implement-rfc.card.v1"
id: "BPR-0948"
title: "Implement updater install transaction and recovery"
status: "Ready"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0947"]
milestone: "RFC-0035 milestone 3"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Implement updater install transaction and recovery

Third authored milestone of [[BPR-0911]]. Add the bundled signed helper,
installation-scoped cooperative ownership, shutdown barrier, durable journal,
recovery and exact healthy-start receipt. Qualify disposable signed installations
before enabling production packages; never replace the developer's running app.

## Acceptance

- AC-01: Exact verified-candidate gesture, refreshed release authority and immediate
  staged revalidation precede replacement; failed flush/quit/quiescence aborts.
- AC-02: All windows and installation processes preserve drafts and indeterminate
  publication reconciliation; timeout never kills processes or retries a write.
- AC-03: Fault injection at every rename/journal/relaunch boundary preserves a
  launchable old copy, prevents concurrent versions and gates success/cleanup on
  transaction/executable/version-bound healthy startup.
- AC-04: Native disposable signed qualification records actual artifacts and
  outcomes; cleanup is restricted to journal-proven ownership and stays truthful.

Record RFC AC-04 through AC-07 evidence and remaining native limits on [[BPR-0912]].

## Verification and execution record

Run `rfc0035-20260920`, segment `m3-implementation`, mandate `r1`; sole
engineering/bookkeeping owner `/root/rfc0035_m3`. Claim `5ca786979432273b56303c1a7d11d66b3b363dc4`;
source candidate `d1e0f46c77b02c07e6ca9ebc4d87f964f94035d3`.

| Criterion | Actual result | Remaining obligation |
|---|---|---|
| AC-01 | Exact gesture/candidate refresh, pre-handoff authority, immediate copied-tree/native revalidation, and typed flush/quit/timeout refusals pass host/public-path fixtures. Revocation after completed handoff does not falsely claim an aborted transfer. | Native signed helper handoff through the packaged user path is not qualified. |
| AC-02 | Two real desktop windows preserve distinct editor text; failed save leaves both usable. Private append-only recovery text is read-only and retains indeterminate markers. An actual peer OS process proves acknowledgment plus process exit is required, including after desktop object destruction. | New-protocol signed process/window shutdown and active-publication/state-retention course are not qualified. The helper's real timeout is not inferred from the typed host fixture. |
| AC-03 | Faults at prepare, both rename sides, launch, receipt, partial cleanup and rollback boundaries reconcile without a second launch. Exact transaction/executable/version receipt gates cleanup; foreign cleanup entries survive retry. Genuine notarized 0.1.3 copies recover their exact original tree after the first rename. | Successful signed replacement startup and missing-receipt recovery in a tests-disabled package remain unverified. Explicit helper recovery exists; a self-service recovery launcher is not qualified. |
| AC-04 | Actual ZIP identity, disposable roots, native trust and rollback outcomes are recorded in the ledger; backup cleanup has a durable per-inode inventory. Tests-disabled application/helper build and ad-hoc helper refusal pass. | The available notarized ZIP predates this protocol. Supply an authorized signed/notarized participating baseline and newer target, then qualify the complete course before enabling production. |

Milestone 3 is **not accepted**. Blocker `signed-helper-upgrade-qualification`
names the unavailable native artifact/course, not a waived criterion. Native
composition intentionally does not select the installer capability; [[BPR-0949]]
remains dependent and must not be promoted. No human decision was substituted for
native evidence, and no attended session or protected review candidate exists.

The [verification ledger](../../verification/RFC-BPR-0035.md) and [[BPR-0912]]
retain exact commands, digests, limitations, ownership and continuation. Prior
milestone evidence and the unrelated worktree/lock-file changes are preserved.
