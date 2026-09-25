---
schema: "implement-rfc.card.v1"
id: "BPR-1131"
title: "Document tabs in the core and the host"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
milestone: "Review workbench scaffolding"
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v7.html","criteria":["AC-05","AC-06"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json"}
---

# BPR-1131: Document tabs in the core and the host

## Outcome

The core holds, per window and pull request, an ordered tab collection. Each
tab has an identity, a file, a comparison and a reading position. The window
has its own comparison, which new tabs take. RFC-BPR-0025's operations become
typed transitions: open in the active tab, open in a new tab, duplicate,
close with the neighbour rule, activate, return a tab to the window's
comparison, and make a tab's pair the window's. The restoration store keeps
the collection across restart, as it keeps the single view today. The host
routes every completion to the tab that asked for it, so a late completion
cannot populate another tab. A tab whose exact view becomes unavailable says
so rather than being retargeted.

## Seams

- The window state and restoration of RFC-BPR-0024.
- The epoch and comparison projections the Updates line reads.
- The selected-file and reading-position state today's single view uses.

## Acceptance

- Core transition tests cover every operation, and host tests cover per-tab
  completion routing and stale rejection.
- Restoration tests keep tab order, the active tab, each tab's comparison and
  position, and the window's comparison.
- Two tabs on one file with different pairs stay distinct and exact through
  refresh and restart.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-05, AC-06 as drawn in [round v7](../../design/BPR-1106-scaffolding-v7.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.
