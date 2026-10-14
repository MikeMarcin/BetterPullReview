---
schema: "implement-rfc.card.v1"
id: "BPR-1516"
title: "Stop vertical diff text shake while dragging the pane divider"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"There's a weird vertical shake of the diff text panes when dragging the divider to resize them","dispatch":"Fresh dedicated implementer queued pending an available agent slot"}
---

# BPR-1516: Stop vertical diff text shake while dragging the pane divider

Dragging the divider to resize the diff text panes causes visible vertical shake. Reproduce the sequence and correct the underlying layout/scroll feedback so horizontal resizing keeps a stable vertical view.

## Acceptance

- AC1: Reproduce and record the vertical displacement during representative continuous divider drags, including reversed drag direction.
- AC2: Resizing the panes no longer introduces repeated vertical oscillation or jumps; legitimate reflow preserves the intended viewport anchor under the existing comparison-space scroll authority.
- AC3: Correspondence, synchronized scrolling, source line identity and selection remain correct after resize, including unequal sides, wrapped lines and large diffs as applicable.
- AC4: Exercise actual divider drag events through the composed diff widget with objective viewport/anchor observations and offscreen rendered evidence. Avoid a fix that only conceals a scroll or layout feedback loop.

## Coordination

BPR-1512 changes the center ribbon width and may touch the splitter; coordinate ownership and integration. Load frontend-design:frontend-design before UX edits per AGENTS.md. Never drive or capture the owner’s live desktop. Required human judgement goes through a short attended session and exact frozen candidate.
