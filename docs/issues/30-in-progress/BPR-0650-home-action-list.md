---
schema: "implement-rfc.card.v1"
id: "BPR-0650"
title: "Home action list"
status: "In Progress"
rfcs: ["RFC-BPR-0028","RFC-BPR-0026"]
depends_on: []
priority: "P2"
---

# Home action list

RFC-BPR-0028 Home, replacing RFC-BPR-0026's desk presentation: a grouped
action list (needs you, waiting on others, following) with pull request,
title, why the row is here, my progress, author, role and updated time; open
by URL or owner/repo#number from the bar. Rows come from the queue and ledger
projections; discovery and observer rules remain RFC-BPR-0026's host work and
are not reimplemented here.

## Acceptance

- RFC-BPR-0028 AC-06 with the mockup's groups and columns.
- Opening a row lands on that pull request's Overview.
- Focused model tests and a composed course over the queue fixture.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.
