---
schema: "implement-rfc.card.v1"
id: "BPR-0687"
title: "Bound Settings queue collections"
status: "In Progress"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0681"]
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
