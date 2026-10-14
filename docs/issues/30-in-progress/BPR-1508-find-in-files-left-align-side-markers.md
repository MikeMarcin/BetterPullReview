---
schema: "implement-rfc.card.v1"
id: "BPR-1508"
title: "Start Find in Files L/R markers at the far left"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Find in files we can start the LR markers on the far left they don't need to be indented"}
---

# BPR-1508: Start Find in Files L/R markers at the far left

Remove the unnecessary leading indentation before the L/R side markers in Find in Files results, placing those markers at the far left of their result rows.

## Acceptance

- AC1: Find in Files result L/R markers begin at the far left without the current unnecessary indentation.
- AC2: Result text remains legible and aligned, with existing grouping, side identity, selection, keyboard navigation and reveal behavior preserved.
- AC3: Verify representative left and right results in an offscreen rendered application state and exercise the relevant result interaction.

## Implementation

Load frontend-design:frontend-design before UX edits as required by AGENTS.md. Keep scope to Find in Files result alignment. Never operate or capture the owner’s live desktop. Prepare a short attended review with an exact frozen candidate if human judgement remains required.
