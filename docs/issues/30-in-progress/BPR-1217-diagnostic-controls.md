---
schema: "implement-rfc.card.v1"
id: "BPR-1217"
title: "Compose persisted diagnostic detail and privacy controls"
status: "In Progress"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1216"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# Compose persisted diagnostic detail and privacy controls

Signed-out profile Settings own Standard/Verbose, independent identity privacy, Off, recent context, Clear and Open logs; transitions and sign-out clear transient state under acknowledged barriers.

## Acceptance

- AC1: Reach and persist controls safely across restart and invalid preferences through the shipped UI.
- AC2: Enforce detail/privacy combinations, typed identity labels, no forbidden material, bounded priority/loss and ring behavior.
- AC3: Verify mode/Off/Clear/privacy/sign-out barriers and profile/window scope.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
