---
schema: "implement-rfc.card.v1"
id: "BPR-1962"
title: "Reaction fixture awaits its accepted refresh before reconciliation"
status: "In Progress"
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

## Focused verification

Original frozen43 Debug phase15 is a real functional negative at2453/40000ms, not a timing waiver. Source52e5e27d2 captures the accepted Snapshot before the single existing Refresh and waits for a different accepted Snapshot with no PendingPullRequest before the identical reaction oracle. Final uninstrumented normal registered Release PASS3.85s and Debug PASS6.57s after exact source restoration. The first diagnostic control exits8/inconclusive (old_ready0/new_ready0 because overlay was nonempty). The one approved diagnostic-only extra Refresh after original reconciliation empirically establishes read_started1/empty_overlay1/read_pending1/snapshot_changed0/old_ready1/new_ready0; that whole instrumented course exits8 afterward because its additional refresh affects fixture completion, and has NO acceptance credit. Both raw results, source hashes and restoration receipts remain external. Final source audit passes with1961binding; normal single-Refresh source remains unchanged by diagnostics.

Evidence: external candidate43/implementer-contract-fixes. All frozen producer source/builds stay untouched. Full common-source native qualification remains release-coordinator work.
