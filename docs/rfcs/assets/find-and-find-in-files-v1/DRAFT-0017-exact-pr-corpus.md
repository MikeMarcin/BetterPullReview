---
schema: "implement-rfc.card.v1"
id: "DRAFT-0017"
title: "Exact retained-comparison corpus and streamed PR search"
status: "Backlog"
rfcs: ["RFC-DRAFT-0015"]
depends_on: ["DRAFT-0016"]
metadata: {"milestone": "exact-pr-corpus", "provisional": true}
---

# DRAFT-0017 — Exact retained-comparison corpus and streamed PR search

## Acceptance

- **AC1:** Historical membership derives from exact retained trees when no accepted catalogue proves the pair; missing trees/catalogue are unavailable rather than substituted.
- **AC2:** All changed entries, both exact paths and full documents participate independently of Files filters, receipts or opened state; added/removed/rename/copy and unsupported sides stay honest.
- **AC3:** Exact blob acquisition remains on the epoch actor, coalesces immutable reads across generations, yields behind selected navigation and pane Find, and keeps search inside existing cache limits.
- **AC4:** Results publish in defined path/side/offset order in bounded batches with precise partial/limit/retry coverage; core and host need no QApplication.
