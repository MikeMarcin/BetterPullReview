---
schema: "implement-rfc.card.v1"
id: "BPR-1510"
title: "Remove redundant Cmd+J button covered by Threads tab"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Cmd+j button can go away, threads tab already covers the behavior"}
---

# BPR-1510: Remove redundant Cmd+J button covered by Threads tab

Remove the visible Cmd+J button because the Threads tab already provides its behavior. Preserve the existing keyboard shortcut and Threads tab interaction; the owner requested removal of the button.

## Acceptance

- AC1: The redundant Cmd+J button is absent from the workbench and its former space does not leave an awkward layout gap.
- AC2: The Threads tab still provides the behavior previously reachable through the button.
- AC3: The existing Cmd+J keyboard shortcut continues to work, with no focus or accessibility regression from the removed widget.
- AC4: Verify actual offscreen rendering and exercise the Threads tab and keyboard action through the composed public path.

## Coordination

Load frontend-design:frontend-design before UX edits, as required by AGENTS.md. Coordinate shared tab/layout files with BPR-1509 (Files/Search tab styling), while preserving that card’s scope. Never drive or capture the owner’s live desktop. Any required human visual judgement uses a short attended session and frozen candidate.
