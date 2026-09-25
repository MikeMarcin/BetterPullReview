# Recovery and authority matrix

| Event | Retained fact | Required behavior |
|---|---|---|
| Edit a note after sharing | Frozen item revision and round digest | Keep delivered bytes unchanged; use new round/amendment for new assignment. |
| Copy or save succeeds | Local transport action only | Never claim recipient read, acknowledgment or implementation. |
| Direct send times out | Round, destination, operation and uncertain status | Do not automatically resend to another harness. Reconcile where supported; manual continuation remains available. |
| Source changes while scanning | Collected material and consistency observations | Retry boundedly or refuse/admit explicitly incomplete observation; never call a recursive scan atomic. |
| Crash before revision commit | Uncommitted capture objects | Recover or garbage-collect unreferenced staging; no half-admitted revision. |
| Pinned bytes missing/corrupt | Historical ID plus unavailability | Report unavailable, do not retarget to current source or fabricate old content. |
| Root moved/replaced | Prior bound root identity | Explicit rebind and mapping; same path is not sufficient authority. |
| Imported duplicate report ID | Prior canonical report digest | Exact duplicate is idempotent; different payload under same ID is rejected. |
| Agent says fixed | Attributed recipient claim | Reviewer verification remains separate and material-bound. |
| Agent removed mid-cycle | Notes, rounds, revisions and transport facts | Capture and compare locally; no callback requirement. |
| Automation revoked | Expired read/report authority | Refuse further access; cannot revoke bytes already disclosed. |
| User index/worktree changed externally | Original capture and outgoing candidate | Do not reset user state to make the plan true; re-plan explicit selection. |
| Git object upload response lost | Planned object ID and journal | Read-first identity reconciliation; no duplicate arbitrary commit construction. |
| New-ref or PR response lost | Exact destination/candidate/branch/plan | Reconcile by authoritative matching evidence; ambiguous foreign objects are not adopted. |
| Target base/head drifts | Planned and freshly observed facts | Show new comparison and require current authorization for changed effects. No atomicity claim across separate server operations. |
| Final PR published | Verified PR association, original local history | Keep local R0/R1/R2 and frozen rounds. GitHub governs public PR facts, not private history. |
| Local database lost | At most exported content and public PR | Restore only actual backups/bundles; GitHub cannot reconstruct private cycles, notes or approvals. |
| Exported bundle imported elsewhere | Immutable external round, claimed origin | Read first, map project/source explicitly; no execution, source overwrite or sender impersonation. |
