---
schema: "implement-rfc.card.v1"
id: "BPR-0681"
title: "Compose General review and notification preferences"
status: "In Progress"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
---

# Compose General review and notification preferences

## Scope

Expose existing accepted review/queue preferences and bounded notification settings; present unavailable startup restoration honestly until 0024.

Owned paths: workbench/queue/notification core and hosts; relevant stores; Qt Settings pages; composition and focused tests.

## Acceptance

AC-02/04/05/06/09/10: typed acceptance, exact reset, persistence failure isolation, local-only rules and metadata-only notifications; no automatic transitions enabled by default.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Not run. Created by the authorized BPR-0592 planning pass.
