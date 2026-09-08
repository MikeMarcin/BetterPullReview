---
schema: "implement-rfc.card.v1"
id: "BPR-0679"
title: "Start sign-in directly and retain exact server recovery"
status: "Ready"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
---

# Start sign-in directly and retain exact server recovery

## Scope

One Sign in gesture starts supported device authorization; cancellation, expiry, refusal and manual tokens remain reachable. Reuse existing saved-server authority.

Owned paths: Qt account entry; app device composition; existing device/GitHub hosts only where required; focused tests.

## Acceptance

AC-04/06/12/13/14: recording public path sees one begin; no second Get code; no wrong-authority requests; remembered servers restore and manual credentials stay session-only.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Not run. Created by the authorized BPR-0592 planning pass.
