---
schema: "implement-rfc.card.v1"
id: "BPR-0948"
title: "Implement updater install transaction and recovery"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0947"]
milestone: "RFC-0035 milestone 3"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
acceptance: [{"criterion":"AC-01","outcome":"pass","revision":"b7e2df46cc178bbcff88b49dd2170ecf27a8c764","evidence":{"kind":"observation","detail":"Exact menu/Settings install gesture, refreshed candidate authority, real signed staging/helper handoff and all host refusal fixtures pass. Final 8/8 updater integration and native final-upgrade.json; docs/verification/RFC-BPR-0035.md mandate-r2 ledger."}},{"criterion":"AC-02","outcome":"pass","revision":"b7e2df46cc178bbcff88b49dd2170ecf27a8c764","evidence":{"kind":"observation","detail":"Two real signed native windows drain before old process exit, retain window IDs/drafts and exact possibly-transmitted recovery record. A live peer survives the real helper 30-second timeout; no signal or publication retry. Existing two-editor failed-save/drain fixtures pass."}},{"criterion":"AC-03","outcome":"pass","revision":"b7e2df46cc178bbcff88b49dd2170ecf27a8c764","evidence":{"kind":"observation","detail":"Complete journal-boundary fixtures pass. Final signed 0.0.350 to 0.0.352 receipt matches transaction 85361061-405d-4135-ba11-df5abb00dd5b and exact version/executable/participant inode. Actual missing receipt waits 60 seconds and restores old executable; signed backup helper recovers interrupted first rename. Restored native UI truthfully reports no completed installation."}},{"criterion":"AC-04","outcome":"pass","revision":"b7e2df46cc178bbcff88b49dd2170ecf27a8c764","evidence":{"kind":"artifact","detail":"Production-signed/notarized/stapled unpublished packages and exact digests retained in build/rfc0035-run/packages. Final target ZIP 29ab8fbfc786864ca7f27bdbe95315bcce254fabdb4e5c0695973c2bf1bc3fa7. All guarded native courses pass with live files unchanged; matching receipt authorizes backup cleanup and partial/foreign-entry fixtures pass. Disposable roots recoverably trashed, source commits retained, no task command remains."}}]
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

### Accepted resumed qualification (mandate r2)

Run `rfc0035-20260920`, segment `m3-post-block-signing`, sole owner
`/root/rfc0035_m3_resume`; claim `182733b3105db797fbf345bc4ac099053f18e005`.
User amendment r2 permits production signing/notarization only for disposable
qualification, not publication, tags, pushes or installed/developer-app replacement.
State-root correction: `5cf310c42ac392c704d4722f344afe81886ea89e`.
Final engineering candidate: `b7e2df46cc178bbcff88b49dd2170ecf27a8c764`.

| Criterion | Accepted evidence |
|---|---|
| AC-01 | The real menu/Settings component drives explicit account choice, Download and exact Install gestures through native staging and signed helper handoff. Release metadata refresh occurs again before transfer. Existing revocation/refusal fixtures and the final 8/8 updater suite pass. |
| AC-02 | Signed 0.0.350 drains two native windows and exits normally before replacement. Both logical window IDs reopen at generation 2; exact private draft bodies and a possibly-transmitted publication record/baseline remain unchanged. A real uncooperative peer survives the helper's 30-second timeout; no signal or write retry occurs. Existing two-editor save/refusal fixtures pass. |
| AC-03 | All journal/rename/relaunch fault fixtures pass. Signed 0.0.350→0.0.351 and final 0.0.350→0.0.352 upgrades produce exact transaction/executable/version/inode receipts. Actual missing-receipt startup fails, waits the helper's 60-second bound and restores the exact old executable. The signed backup helper also recovers an interrupted first rename. The restored native UI reports preserved/restored state, not success. |
| AC-04 | Three unpublished universal packages are production-signed, notarized, stapled and assessed through `tools/release.sh`. Exact source/archive/executable identities and guarded roots are in the ledger. Matching receipts remove only the owned backup; foreign-entry/partial-cleanup fixtures pass. Native courses and task processes are quiescent; disposable roots are recoverable in Trash. |

M3's acceptance and exit are satisfied. Production composition selects the
installer only for eligible trusted macOS installations, with managed/untrusted/
unsupported/automated-test fallbacks unchanged. Final package 0.0.352 comes from
`5db1d104a2436a2b0df811ac2e40cff0c58c2319`, differing from the engineering
candidate only in its qualification version; ZIP SHA-256
`29ab8fbfc786864ca7f27bdbe95315bcce254fabdb4e5c0695973c2bf1bc3fa7`.

No human or independent acceptance is claimed or required for this milestone.
[[BPR-0949]] may be promoted to Ready, not started here. Intel execution, live
private-release/vault access and operator/security closeout remain its obligations.
The prior blocked record below is historical and is superseded only by this
explicit native evidence; its original limitations are not rewritten.

### Prior blocked qualification (mandate r1)

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
