---
schema: "implement-rfc.card.v1"
id: "BPR-1660"
title: "Roll out shared secondary buttons through the app"
status: "Done"
rfcs: ["RFC-BPR-0087"]
depends_on: ["BPR-1659"]
metadata: {"source": "Owner request 2026-10-19; isolated worktree, no merge/push/release", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Inventory","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","detail":"All 46 initial translation units and every mixed factory result are migrated or source-backed intentional variants in docs/ux/shared-action-buttons.md."}},{"criterion":"Migration","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","detail":"Shared QPushButton and QToolButton use one painter, preserving native primary/default, destructive, menu, icon, navigation and specialized controls."}},{"criterion":"Verification","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","detail":"23/23 focused checks, the tour-writing check, 6/6 four-theme style/scale courses and extra review-cycle render course pass; representative composed renders were inspected. See docs/verification/RFC-BPR-0087.md."}}]
---

# BPR-1660: Roll out shared secondary buttons through the app

## Acceptance

- Complete the RFC app-wide inventory with migrated or intentionally retained classifications.
- Migrate remaining comparable text choices and secondary actions using the shared component and preserve primary/destructive/menu/icon variants.
- Focused affected composition checks and representative actual renders pass.
