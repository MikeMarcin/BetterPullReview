---
schema: "implement-rfc.card.v1"
id: "BPR-0592"
title: "Inventory settings authority and plan RFC-BPR-0018"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: []
milestone: "Post-Pack v2"
acceptance: [{"criterion":"Inventory ownership, migration, reset, failure and secrecy; map all criteria and allocate bounded slices","outcome":"pass","revision":"137ba6171c18df425c08a26fbc8422112ca61476 plus planning files in finish","evidence":{"kind":"observation","detail":"docs/development/settings-authority-inventory.md audits existing appearance/workbench/queue/device/agent/notification/storage owners and identifies unimplemented capabilities. Maps AC-01 through AC-14 to reserved BPR-0678..0683 and direct checks; closeout dependencies updated; immutable registration base retained. python3 scripts/verify_documentation.py --root . and git diff --check pass. No product acceptance inferred from inventory."}}]
---

# Inventory settings authority and plan RFC-BPR-0018

## Objective

Inventory every adjustable value, its typed owner, projection, store/migration, reset scope, durability, capability, and secrecy class. Include direct sign-in, server configuration, remembered grants, and the existing default environment. Identify the smallest composed Settings slice and its exact focused verification selectors.

Governing design: [RFC-BPR-0018](../../rfcs/RFC-BPR-0018-robust-settings-center.md).
Closeout: [BPR-0593](../20-ready/BPR-0593-close-rfc-0018.md).

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 1 of 4.
Planned cards: BPR-0592, BPR-0593. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

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

## Activation, 2026-09-07

The owner explicitly requested the broader UX RFC implementation after being
asked to distinguish it from finishing RFC-0028. The canonical index records
the implementation frontier exception. Retain the registration baseline above;
reconcile the already integrated 0028 Settings work before allocating slices.
Routing: standard | Reason: coupled ownership inventory retained by integrator.

## Inventory and bounded plan

[The authority inventory](../../development/settings-authority-inventory.md)
records adjustable values, default scope, stores/schema versions, resets,
failures and secrecy. It maps all fourteen criteria to allocated BPR-0678
through BPR-0683; BPR-0593 now depends on them. Reuse the existing appearance,
workbench and queue hosts. Begin with the shell's missing scrolling and focus.
