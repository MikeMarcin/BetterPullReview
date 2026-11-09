---
schema: "implement-rfc.card.v1"
id: "BPR-1948"
title: "Verify and close RFC-BPR-0096"
status: "Ready"
rfcs: ["RFC-BPR-0096"]
closeout_for: ["RFC-BPR-0096"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"go-to-line-r1","assigned_implementer":"/root/go_to_line","reservation":"docs/issues/_reservations/BPR-1948--BPR-1948--71d1d557b9a1.json","milestone_plan":{"id":"go-to-line-v1","path":"docs/rfcs/plans/go-to-line-v1/milestone-plan.json"}}
---

# BPR-1948: Implement and close Go to line

Implement [RFC-BPR-0096](../../rfcs/RFC-BPR-0096-go-to-line.md) as one milestone.
All AC01–AC04 and the behavior they name remain unchanged. No human criterion.

## Acceptance

- AC01: Eligible active main diff menu/hotkey parity, platform help, focus and editing scope; preserve Find keys.
- AC02: Split/unified exact Base/Head navigation including unchanged, changed-group interiors, last and repeated offscreen lines; correct cursor and subsequent draft.
- AC03: Invalid input, unavailable side and cancellation preserve selection; stale file/tab/comparison requests refuse.
- AC04: Composed keyboard/menu flows, accessible focus states and full-window minimum-size scale renders; preserve shared geometry and bounded work.

## Verification and execution record

Pending implementation and exact-candidate checks.
RFC base: `663137a9ed3825c87934c93926d414725f8bee22`.
[Checkpoint](../_checkpoints/go-to-line-v1/handoff.md).

## Closeout checklist

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Menu and hotkey invoke the same dialog for the eligible active diff, and the shortcut help shows the platform-correct binding. Ineligible views and editable fields obey Entry point and scope, and macOS Find keys retain their current behavior. | Composed Go to line course | Pending | Pending |
| **AC02:** In split and unified views, distinguishable Base/Head content demonstrates correct default-side selection and exact navigation to entered source numbers, including unchanged context, a changed-group interior and the last source line. Returning to an already selected offscreen line works; keyboard navigation and a subsequent draft target that line. | Composed Go to line course | Pending | Pending |
| **AC03:** Invalid input, an unavailable side and cancellation follow the dialog behavior without moving or replacing the source selection. A file, tab or comparison invalidated while the dialog is open cannot receive the old request. | Composed Go to line course | Pending | Pending |
| **AC04:** Through the composed production workbench, keyboard-only and menu flows demonstrate the defined focus, selection and accessible dialog states in both presentations. Full-window renders at minimum supported size and supported scaling show usable controls and a visible target; navigation retains the shared scroll geometry and the stated bounded-work behavior. | Composed Go to line course | Pending | Pending |
