---
schema: "implement-rfc.card.v1"
id: "BPR-1662"
title: "Verify and close RFC-BPR-0087"
status: "Backlog"
rfcs: ["RFC-BPR-0087"]
depends_on: ["BPR-1659", "BPR-1660", "BPR-1661"]
metadata: {"source": "Owner request 2026-10-19; isolated worktree, no merge/push/release", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0087"]
---

# BPR-1662: Verify and close RFC-BPR-0087

## Acceptance

- Judge every RFC criterion against direct evidence, with the full migration inventory accounted for.
- Documentation gate, focused integration and relevant formatting checks pass; retain native-platform limitations and obtain separate integration authorization.

RFC base: `74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Overview's selected/unselected Open/All and the migrated Settings choices/actions visibly use one shared treatment in all four themes, without the heavy native bezel or changes to labels and meaning. | Pending; no implementation evidence yet. |
| **AC02:** The app-wide inventory accounts for comparable text buttons on every Qt surface. Each is migrated or has an intentional variant reason; deferred migrations are visible and prevent app-wide closeout. | Pending; no implementation evidence yet. |
| **AC03:** Existing gestures and command outcomes still hold. Keyboard focus, exclusivity, checked state, disabled state, accessibility, icon/menu content and translated sizing survive adoption, including theme/style changes. | Pending; no implementation evidence yet. |
| **AC04:** Representative composed Overview, Settings Review/Comments and another migrated page are rendered and inspected with hover, press, focus and disabled evidence at 1x, 1.25x and 2x. Available style variants are exercised; offscreen and native-platform claims are recorded separately. | Pending; no implementation evidence yet. |
| **AC05:** Focused composition and presentation checks, formatting and the documentation gate pass. The verification record names actual commands, revision, observed results, retained variants and remaining platform work. | Pending; no implementation evidence yet. |
