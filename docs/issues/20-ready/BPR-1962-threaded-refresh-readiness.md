---
schema: "implement-rfc.card.v1"
id: "BPR-1962"
title: "Reaction fixture awaits its accepted refresh before reconciliation"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "frozen43Debug threaded reaction failure", "reservation": "docs/issues/_reservations/BPR-1962--BPR-1962--5fe6af572283.json", "milestone_plan": {"id": "bpr-1962-threaded-refresh-readiness", "path": "docs/rfcs/plans/bpr-1962-threaded-refresh-readiness/milestone-plan.json"}}
---

# BPR-1962: Reaction fixture awaits its accepted refresh before reconciliation

## Acceptance

- AC1. Snapshot-before-refresh and changed accepted snapshot/no PendingPullRequest gate precede the unchanged complete reaction oracle, keeping one refresh/all gestures/provider flow and40s budget.
- AC2. Original Release+Debug threaded course passes and explicit pre-completion negative control proves request-start is insufficient; retain43raw failure, not a Debug timing waiver.
- AC3. Exact test-only two-file content review/reseal1961, format/tidy/docs/board and intentional latest-main integration pass. Protected producers remain unchanged and full final native qualification remains pending.

## Diagnosis

The fixture checks AccountReadCount, a transport request-start counter, plus an empty optimistic overlay. Frozen43 fails while the app says Refreshing the pull request,2453ms into40s, with reads37/overlay0. The fixture must wait for the refreshed accepted snapshot before interpreting the presented reactions. No product reconciliation behavior changes.
