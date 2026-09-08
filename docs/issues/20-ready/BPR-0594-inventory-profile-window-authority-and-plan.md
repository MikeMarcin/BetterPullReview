---
schema: "implement-rfc.card.v1"
id: "BPR-0594"
title: "Inventory profile and window authority and plan RFC-BPR-0024"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0592"]
milestone: "Post-Pack v2"
---

# Inventory profile and window authority and plan RFC-BPR-0024

## Objective

Inventory application-data roots, vault bindings, domain stores, shared review records, singleton runtime assumptions, and window shutdown/restore paths. Specify Default adoption and profile/window identity propagation against the live implementation. Allocate the first bounded profile foundation slice with migration and composed-path selectors.

Governing design: [RFC-BPR-0024](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md).
Closeout: [BPR-0595](../10-backlog/BPR-0595-close-rfc-0024.md).

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 2 of 4.
Planned cards: BPR-0594, BPR-0595. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

The owner activated this work on 2026-09-07. Independent read-only planning may
proceed alongside the remaining Settings integration; product implementation
follows that automated boundary. BPR-0593 and BPR-0242 remain terminal
prerequisites on BPR-0595 under the canonical index's implementation-frontier
exception. This scheduling split does not waive native or human acceptance.

## Acceptance

- Re-audit the live RFC dependencies, accepted scope, governing architecture, and
  current code/verification paths; record concrete ownership and migration facts.
- Map every numbered RFC acceptance criterion to a bounded implementation slice
  and a direct planned check, naming the smallest existing selectors or the
  focused new check required. Include one composed public path for UI behavior.
- Allocate only needed slice cards, link their dependencies and affected RFC,
  and update the closeout's planned cards/checklist without claiming tests ran.

## Non-goals

Product implementation, RFC closeout, live authentication or publication, broad
validation matrices, and redesigning the approved interaction model are not part
of this entry card. Later implementation authority does not waive the RFC's
existing dependency/eligibility rules.

## Verification record

Not run; entry work and product implementation are deferred. When authorized,
run the documentation gate on the planning slice, record exact focused selectors
for its implementation consumers, and commit with explicit paths. This card is
not satisfied merely by the initial RFC registration.
