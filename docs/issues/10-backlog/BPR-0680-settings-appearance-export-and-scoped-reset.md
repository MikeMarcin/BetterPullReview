---
schema: "implement-rfc.card.v1"
id: "BPR-0680"
title: "Complete appearance export and scoped defaults"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
---

# Complete appearance export and scoped defaults

## Scope

Extend the existing appearance host and exchange port for export and precise resets; preserve preview/keep/cancel and tuned defaults.

Owned paths: appearance core/host/ports; narrow file adapter; appearance panel; focused host/store/Qt tests.

## Acceptance

AC-03/04/05/06/09/10: export round-trip, exact defaults, refusal restore, cancelled preview, restart and nonregular-file refusal through the selected public path.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Not run. Created by the authorized BPR-0592 planning pass.
