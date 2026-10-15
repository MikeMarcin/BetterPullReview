---
schema: "implement-rfc.card.v1"
id: "BPR-1519"
title: "Bring File and Change labels closer to their controls"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"The controls for file and change are far away from their label's text. Probably need to shift those labels right","dispatch":"Fresh dedicated implementer queued pending an available agent slot","milestone_plan":{"id":"bpr-1519-navigation-label-spacing","path":"docs/rfcs/plans/bpr-1519-navigation-label-spacing/milestone-plan.json"}}
---

# BPR-1519: Bring File and Change labels closer to their controls

Reduce the excessive visual gap between the File and Change navigation labels and their associated controls. Shift the labels right toward the controls, as suggested by the owner, using the existing layout system.

## Acceptance

- AC1: File and Change labels sit close enough to their corresponding navigation controls to read as clear groups, without the current excessive gap.
- AC2: Both groups retain consistent spacing and alignment at normal and narrow supported window widths and supported display scaling, without overlap or clipped text.
- AC3: Navigation, focus order, accessible label relationships and existing control hit targets remain correct.
- AC4: Verify the actual offscreen rendered navigation area at representative widths/scales and exercise its existing controls.

## Coordination

BPR-1517 owns stalled file navigation; BPR-1518 owns cursor-relative change navigation. Keep this visual spacing correction separate and coordinate shared navigation-layout files. Load frontend-design:frontend-design before UX edits per AGENTS.md. Never drive or capture the owner’s live desktop. Any required human judgement uses a short attended session and exact frozen candidate.
