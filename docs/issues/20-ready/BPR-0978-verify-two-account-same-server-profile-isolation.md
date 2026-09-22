---
schema: "implement-rfc.card.v1"
id: "BPR-0978"
title: "Verify concurrent two-account same-server profile isolation"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0693", "BPR-0694", "BPR-0695", "BPR-0696", "BPR-0701", "BPR-0702"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Verify concurrent two-account same-server profile isolation

## Scope

Complete RFC-0024 AC-02/08/09 through the shipped public composition using
two concurrently open BPR profiles with distinct fixture accounts on the same
server. Discriminate credentials, repositories/recents, private roots/caches,
epochs, notes/drafts/receipts, preferences and sign-out/late completion. Repair
any demonstrated product boundary defect; do not add live credentials or
network mutations.

Owned surfaces: public-path fixtures and end-to-end course/contracts, plus
exact demonstrated defects in profile composition, scoped host authority,
credential/storage adapters. Existing single-profile and isolated store
evidence supplements but cannot replace the two-account composed course.

## Acceptance

- Both same-server fixture accounts remain concurrently usable, with distinct
  exact account authority and profile-scoped vault/storage identities.
- Writes/reads to repositories, caches/epochs, private review state and
  preferences discriminate the two profiles, including the same PR identity.
- Sign-out/replacement/late completion in one profile does not retire, expose
  or mutate the other profile; a deliberately broken scoping dependency is
  detected by the public course or focused boundary regression.
- Relevant public, credential and storage tests pass without live writes.

## Evidence

Pending. The existing course proves same-account sibling-window isolation,
not the complete concurrent two-account criterion.
