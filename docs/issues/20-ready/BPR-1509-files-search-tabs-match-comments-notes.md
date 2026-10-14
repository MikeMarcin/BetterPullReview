---
schema: "implement-rfc.card.v1"
id: "BPR-1509"
title: "Style Files and Search tabs like Comments and Notes tabs"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Lets makes files and search tabs more like comments notes tabs style"}
---

# BPR-1509: Style Files and Search tabs like Comments and Notes tabs

Use the existing Comments and Notes tabs as the visual reference for Files and Search tabs. Match the established tab treatment, spacing and selected/hover/focus states while retaining the Files and Search content and behavior.

## Acceptance

- AC1: Files and Search tabs use a coherent visual treatment matching the existing Comments and Notes tabs.
- AC2: Selected, hover, focus and disabled states remain clear and keyboard/accessibility behavior is preserved.
- AC3: Switching tabs preserves existing file selection, search state and drawer behavior.
- AC4: Verify actual offscreen rendered states and the composed tab-switch interaction, and prepare any required owner visual acceptance on an exact frozen candidate.

## Coordination

Load frontend-design:frontend-design before UX work as required by AGENTS.md. BPR-1508 owns L/R result marker indentation inside Search; coordinate shared files without taking over that scope. Never operate or capture the owner’s live desktop.
