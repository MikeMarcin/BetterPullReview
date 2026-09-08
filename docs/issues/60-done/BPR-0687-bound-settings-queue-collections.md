---
schema: "implement-rfc.card.v1"
id: "BPR-0687"
title: "Bound Settings queue collections"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0681"]
acceptance: [{"criterion":"AC-10 bounded queue collections and retained accepted state","outcome":"pass","revision":"29bb2980 plus BPR-0687 finish paths","evidence":{"kind":"observation","detail":"Host and SQLite fixtures passed: application_review_queue_enrollment 8.21s; sqlite_queue_enrollment_storage 0.82s. Exact maximum and limit+1 load/edit coverage for rules, saved repositories, servers and enrollments; atomic rollback and existing identity writes at capacity. Qt workbench scales 100/125/200 passed 12.47s total: max collections, persistent model indexes unchanged on refusal, removal actions enabled, visible status. Canonical tidy passed five worker TUs and two Qt TUs; git diff --check passed. Primary inspected changed host/store/Qt diff."}}]
---

# Bound Settings queue collections

## Scope

Bound durable and new queue rules, saved repositories, saved servers and enrollments before projection or persistence. Limits must be declared and enforced on load and edit; hostile overflow returns a typed refusal without replacing accepted state. Prevent unbounded SQLite materialization as well as Qt loops.

Owned paths: Application queue types/host; SQLite queue load/store; focused queue tests.
Routing: standard | Reason: default implementation (Astra low delegated route).
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-10: limit+1 load/edit fixtures refuse exactly, admitted maximum remains usable, accepted state and unrelated domains are preserved. Focused queue host/storage tests plus Qt bounded projection evidence.

## Evidence

Read-only BPR-0683 acceptance scout identified this concrete missing behavior.
Not yet implemented or checked. This bounded bridge is needed for Settings
acceptance; it does not activate unrelated browser tabs or claim full RFC-0026.
