---
schema: "implement-rfc.card.v1"
id: "BPR-0683"
title: "Verify Settings integration and native acceptance"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0678", "BPR-0679", "BPR-0680", "BPR-0681", "BPR-0682","BPR-0684"]
---

# Verify Settings integration and native acceptance

## Scope

Reconcile all Settings criteria with focused selected-path evidence, migration failures and native/attended inventory.

Owned paths: Settings integration tests and documentation only, product fixes get bounded follow-ups.

## Acceptance

AC-05/07/08/10/11/14: actual platform facts, four appearances and 100–200% scale, keyboard/accessibility, bounded loads, failure isolation; park unavailable required observations.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Not run. Created by the authorized BPR-0592 planning pass.

BPR-0684 repairs the Settings-close preview/import lifecycle discovered during integration preparation.
