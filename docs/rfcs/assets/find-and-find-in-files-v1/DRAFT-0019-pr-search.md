---
schema: "implement-rfc.card.v1"
id: "DRAFT-0019"
title: "Composed PR Search drawer, result activation and invalidation"
status: "Backlog"
rfcs: ["RFC-DRAFT-0015"]
depends_on: ["DRAFT-0017", "DRAFT-0018"]
metadata: {"milestone": "pr-search", "provisional": true}
---

# DRAFT-0019 — Composed PR Search drawer, result activation and invalidation

## Acceptance

- **AC1:** AC03–AC07 pass through real Search-tab commands and key paths: historical membership, exact result activation/F4, sibling preservation, filters/deduplication, stale completion/reveal, qualified coverage and accessible native layout.
- **AC2:** Selection/reveal binds PR authority, catalogue, tab, comparison, exact path/side/blob/span and query generation; later reading gestures cancel it.
- **AC3:** Pane and PR query state remain independent and runtime-only; authority changes clear sensitive text/results and all newly admitted effects reach their terminal.
