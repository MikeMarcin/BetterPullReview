---
schema: "implement-rfc.card.v1"
id: "BPR-1507"
title: "Make file Accept available in one click"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Accept should be the default action on a file so it's one click, or we redesign that element so it can easily be one click.","milestone_plan":{"id":"bpr-1507-one-click-file-accept","path":"docs/rfcs/plans/bpr-1507-one-click-file-accept/milestone-plan.json"}}
---

# BPR-1507: Make file Accept available in one click

Make Accept the default action on the file control, or revise that control so Accept is readily available with one click. The owner authorizes either approach; choose the smallest coherent solution after inspecting the existing interaction.

## Acceptance

- AC1: An eligible file can be explicitly Accepted with one click from its normal file control, without opening a menu first.
- AC2: The action and its current state are clear; existing alternate file actions remain available.
- AC3: Accept retains existing exact file/epoch authority, persistence and eligibility rules. Opening a file or automatically marking Seen does not implicitly Accept it.
- AC4: Verify the rendered control offscreen and exercise its composed one-click action, including relevant unavailable/stale states.

## Coordination and review

BPR-1506 owns automatic Seen on opening a file. Coordinate any shared file-control or receipt paths before editing; keep automatic Seen distinct from explicit Accept. Load frontend-design:frontend-design before UX work as required by AGENTS.md. Prepare the required short attended review with an exact frozen candidate if human visual judgement remains. Never operate or capture the owner's live desktop.
