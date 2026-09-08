---
schema: "implement-rfc.card.v1"
id: "BPR-0703"
title: "Reject stale private note edits and retain rejected candidates"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0691"]
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

Bounded read-only ownership investigation identified this slice. No new code
or acceptance check has run. Full same-profile multi-window criteria remain open.
