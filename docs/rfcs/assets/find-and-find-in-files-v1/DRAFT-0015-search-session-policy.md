---
schema: "implement-rfc.card.v1"
id: "DRAFT-0015"
title: "Search session identity and bounded host lifecycle"
status: "Ready"
rfcs: ["RFC-DRAFT-0015"]
depends_on: []
metadata: {"milestone": "search-session-policy", "provisional": true}
---

# DRAFT-0015 — Search session identity and bounded host lifecycle

## Acceptance

- **AC1:** The core stores only exact scope, query/request generations, compact result counts and cursor facts; no query text, source documents or result collections enter interaction state.
- **AC2:** The public Qt-free search host validates the 4 KiB query boundary, submits no work for empty input, separates independent pane/PR sessions and reports invalid input without retaining apparently current results.
- **AC3:** Reversed query/scope/authority completions and duplicates cannot publish; supersession, clear, close and shutdown cancel old work, preserve exactly-one terminals for admitted effects, and reach observable quiescence.
- **AC4:** Next/Previous traverses accepted counts in constant work, wraps explicitly and never calls the worker; incomplete coverage cannot become unqualified No results.
