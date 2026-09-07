---
schema: "implement-rfc.card.v1"
id: "BPR-0651"
title: "Repositories and files drawers"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Repositories and files drawers

RFC-BPR-0028 Drawers, from the Settings-and-drawers asset sections 4 and 4b.
Replace the current files drawer and queue landing with two drawers: the
repositories tree (saved repositories per account, pull requests of interest
in Home's order with reason and unread dot, open on select) and the files tree
(compacted nesting folders, chevrons and guide lines, read-state dots, counts,
three orders, parent-keeping filter). Each has its own strip toggle, shortcut
and rail; they collapse independently. Rows come from the queue, ledger and
snapshot projections; any missing projection is added in the host.

## Acceptance

- RFC-BPR-0028 AC-09 at 1280x852 and 1680x1000.
- Selecting a pull request row opens it on Overview; selecting a file row
  lands on its first change.
- Focused model and drawer tests; the composed courses that pin the files
  drawer pass with their evidence repinned.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.
