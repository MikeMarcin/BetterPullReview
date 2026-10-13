---
schema: "implement-rfc.card.v1"
id: "BPR-1428"
title: "Replace CLI publication popup with strict exact-operation admission"
status: "Ready"
rfcs: ["RFC-BPR-0079"]
depends_on: ["BPR-1427"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
---

# BPR-1428: Replace CLI publication popup with strict exact-operation admission

Imported under `strict-cli-publication-r2`; current acceptance remains open.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Ordinary review/inline/tour gestures with no extra modal.
- **AC02:** Exact actor/target/head/content grant checks.
- **AC03:** Trust/startup/discovery zero writes and strict capability refusal.
