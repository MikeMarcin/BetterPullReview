---
schema: "implement-rfc.card.v1"
id: "BPR-1610"
title: "Review stats capture and lifetime store"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1607"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"event-counts","outcome":"pass","revision":"00e0eb7dca050f0815c80064286114fd856b85f7","evidence":{"kind":"command","detail":"Typed core deltas and persistent event keys count all kinds once, reject duplicate mutation receipts, count confirmed recovery only, and exclude ledger load/carry. Focused core/SQLite/submission/mutation/ledger courses pass.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/final-capture-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/lifetime-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/core-tests.log"]}},{"criterion":"lifetime","outcome":"pass","revision":"00e0eb7dca050f0815c80064286114fd856b85f7","evidence":{"kind":"command","detail":"Real cache prune and deletion, Clear local diagnostics and restart retain review totals; real profile purge removes insights.sqlite3. Retention and replay tests pass.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/final-capture-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/lifetime-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/core-tests.log"]}},{"criterion":"local-days-and-records","outcome":"pass","revision":"00e0eb7dca050f0815c80064286114fd856b85f7","evidence":{"kind":"command","detail":"Host local date conversion covers fall overlap/25-hour midnight and spring gap. Core ranges/calendar/records and SQLite day/repository buckets pass.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/final-capture-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/lifetime-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/core-tests.log"]}},{"criterion":"no-backfill","outcome":"pass","revision":"00e0eb7dca050f0815c80064286114fd856b85f7","evidence":{"kind":"command","detail":"Fresh activity store starts empty despite existing ledger-like canaries; actual host only records explicit successful commands and creation terminals; freeze replay and reuse add nothing.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/final-capture-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/lifetime-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/core-tests.log"]}}]
---

# Review stats capture and lifetime store

## Scope and acceptance

Implement RFC-BPR-0086 "Reviews" counting (the count table) and AC06.

- Core counting rules and dedup keys. Observers on the submission, mutation,
  resolution, ledger, note, defect and Review Cycle terminals. Local-day and
  per-repository buckets, lifetime totals and records.
- Totals survive cache prune and delete, Clear local diagnostics and restart.
  Profile purge removes them.
- Count from the first launch with Insights. No backfill (Q2).
