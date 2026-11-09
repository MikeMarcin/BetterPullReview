---
schema: "implement-rfc.card.v1"
id: "BPR-1973"
title: "Bound Debug functional application watchdogs"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"tooling","found_by":"build43 Windows Debug short outer watchdog failures","mandate":"Coordinator-approved bounded 120-second Debug-only watchdogs under the permanent owner policy; Release timing and all inner budgets unchanged.","milestone_plan":{"id":"bpr-1973-debug-functional-watchdogs","path":"docs/rfcs/plans/bpr-1973-debug-functional-watchdogs/milestone-plan.json"}}
---

# BPR-1973: Bound Debug functional application watchdogs

- AC1. Add validated optional `DEBUG_TIMEOUT` to the existing run-bound helper
  and opt in exactly the seventeen approved short functional application rows.
  Debug outer timeouts are 120 seconds; optimized outer timeouts remain
  15 seconds and application budgets remain 8000 ms. Invalid or shortening
  values refuse configuration. No domain assertion or timing waiver changes.
- AC2. Capture before/after normal single Release/Debug and multi-config
  Release/Debug/RelWithDebInfo graphs. After backtrace-only canonicalization,
  exactly seventeen Debug `TIMEOUT` values change and every other command,
  environment, label, lock, path, predicate and property stays identical.
- AC3. Complete focused helper controls, documentation/scoped board checks,
  explicit-path commits and intentional main integration after BPR-1971 lands.
  Preserve source/counters and raw old failures. Final Windows semantic checks
  at the supported 120-second bound remain a common-source qualification
  obligation; graph evidence is not native execution credit.

The owner permanently omits dedicated Debug timings and permits a larger bounded
Debug functional watchdog. Actual hangs and semantic failures still require
investigation. The build43 15-second cutoffs and unreached assertions remain
raw diagnostic evidence; no passing result is inferred from a timeout.
