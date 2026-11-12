---
schema: "implement-rfc.card.v1"
id: "BPR-2141"
title: "A fresh one-off is one model call: no tools, its context up front"
status: "In Progress"
rfcs: ["RFC-BPR-0101"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["AC13"]}
---

# A fresh one-off is one model call: no tools, its context up front

## Scope and acceptance

Owner direction, 2026-11-12: "One API call per click for fresh one-offs:
no tools, context supplied up front." The correction of the wrapper tests:
the main cost lever is the number of API calls per click, each re-reading a
prefix of about 25k tokens. Fresh one-offs launch tool-free with no MCP
server and their context in the request; one that cannot be filled up front
is named where it is chosen.

Acceptance: AC13 as amended on 2026-11-12.
