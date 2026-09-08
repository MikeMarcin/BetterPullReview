---
schema: "implement-rfc.card.v1"
id: "BPR-0703"
title: "Reject stale private note edits and retain rejected candidates"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0691"]
acceptance: [{"criterion":"All bounded private-note revision and retained conflict criteria","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Storage0.93s and final host0.29s pass race/CAS/ABA/scope/exhaustion/migration/bounds and first-candidate/hostileCurrent cases. Five workerTUs plus fixes and two primarycallers tidy pass. Rebuilt composedledger40.86s and agentinvocation0.40s pass; strict appmutationinventory0.719s and documentationgate pass. One mediumreview Current validation finding corrected and narrowrechecked. No ordinary noteeditor/durableeditable drafts claimed."}}]
---

# Reject stale private note edits and retain rejected candidates

## Scope

Add mandatory expected revisions to private-note edit/delete admission, with
explicit create-only semantics. SQLite ledger schema 6 transactionally migrates
schema 5 notes and maintains monotonic per-review revisions so delete/recreate
cannot admit stale edits. Return typed conflict with current accepted state;
retain the exact rejected candidate separately in window-owned host projection.
Preserve exact review scope, text/record quotas and publication authority.

This bounded card covers durable private notes and host conflict presentation.
There is no ordinary private-note editor today; do not claim that UI is shipped.
Shared editable draft ownership and its editor conflict UX remain later cards;
this card does not add durable editable drafts or change mutation submission.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, app composition, CMake, bookkeeping and acceptance.
Workers own separate epoch or ledger paths and preserve parallel work.

## Acceptance

Two independent SQLite connections editing revision N yield one winner and one
conflict; reopen retains only the winner. Stale delete, delete/recreate and
foreign review scope cannot bypass admission. Migration interruption/repetition,
unknown/malformed schemas and bounds preserve data/refusal. Host projection
retains exact rejected candidate bytes distinct from current accepted note;
existing ledger/storage contracts and composed ledger course remain passing.

## Evidence

Schema6 note CAS passes an actual synchronized two-connection race, stale
edit/delete, create-only, delete/recreate, exact-scope and allocator exhaustion
checks. Legacy migration rollback/retry and bounded row validation pass. Storage
passes (0.93s); final host tests pass (0.29s), including first-candidate retention
through later failures/successes until explicit dismissal. Medium review found
unvalidated conflict Current observations; exact ID, positive revision and bounded
valid content are now checked before both return and retention, with hostile-port
regressions. No additional migration/CAS blocker was found.

All five worker translation units and final corrections pass tidy; the two
primary promotion/caller translation units pass tidy. Rebuilt application agent
invocation (0.40s) and composed ledger course (40.86s) pass. Promotion returns the
accepted durable revision; create-only cannot silently overwrite a note. Strict
app mutation-source inventory (0.719s) and documentation gate pass. No note-editor
or durable editable-draft UI is claimed by this card.
