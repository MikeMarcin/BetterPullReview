---
schema: "implement-rfc.card.v1"
id: "BPR-0727"
title: "Overview refuses a drawer opened by hand"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"c4817ab5-22dc-4ec9-9278-9c28c08ae457"}
---

# Overview refuses a drawer opened by hand

The owner chose, on the options candidate `BPR-0674-overview-drawer-policy`
in the v006 attended session (decision `c4817ab5`, 18:33:56Z): "Overview
keeps every drawer collapsed; a hand-opened drawer is refused". This is the
"or maybe always" of their 2026-09-07 note that
[BPR-0674](../60-done/BPR-0674-overview-is-starved-by-the-drawers.md)
left to them.

## Scope

- While Overview is the active modality, the three drawers stay
  collapsed: the strip toggles, Alt+Shift+1/2/3 and, with drawer controls
  shown, the rails do not open a drawer, and say so (a disabled toggle is
  not allowed on the strip by RFC-BPR-0028, so the control stays enabled
  and the refusal is explained where the reviewer looks, or the gesture
  switches the modality). Choosing another modality restores that
  modality's preset as today.
- The core decides: the refusal is the workbench interaction's, keyed on
  the modality the window has chosen, not a Qt-side swallow; the window
  still presents what the core publishes.
- RFC-BPR-0028 AC-03 ("never change on their own unless a configured
  transition fires") and the Modalities paragraph are amended to name the
  Overview rule.
- Owned paths: `src/core/src/workbench_interaction.cpp` and its header,
  `src/adapters/qt/src/workbench_window.cpp`, their tests, RFC-BPR-0028.

## Acceptance

- `core_workbench_interaction` pins that a drawer gesture in Overview is
  refused with a named reason and accepted in the other three modalities.
- `qt_workbench_window_scale_100` pins that in Overview the strip toggles
  and shortcuts leave every drawer collapsed and the refusal is visible,
  and that Read, Review and Discuss still open theirs.
- The owner accepts it on the signed bundle in an attended session.
